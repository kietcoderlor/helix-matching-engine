import crypto from "node:crypto";
import { pool } from "./db.ts";
import { config } from "./config.ts";

export interface Account {
  id: number;
  username: string;
  participantId: string;
}

function hashPassword(password: string, salt: string): string {
  return crypto.scryptSync(password, salt, 32).toString("hex");
}

export async function register(username: string, password: string): Promise<Account> {
  if (!username || username.length < 3 || username.length > 32) {
    throw Object.assign(new Error("username must be 3-32 characters"), { status: 400 });
  }
  if (!password || password.length < 8) {
    throw Object.assign(new Error("password must be at least 8 characters"), { status: 400 });
  }
  const salt = crypto.randomBytes(16).toString("hex");
  const hash = hashPassword(password, salt);
  const client = await pool.connect();
  try {
    await client.query("BEGIN");
    const acc = await client.query(
      `INSERT INTO accounts (username, password_salt, password_hash, participant_id)
       VALUES ($1, $2, $3, 0) RETURNING id, username`,
      [username, salt, hash],
    );
    const id = acc.rows[0].id as number;
    await client.query(`UPDATE accounts SET participant_id = $1 WHERE id = $1`, [id]);
    await client.query("COMMIT");
    return { id, username, participantId: String(id) };
  } catch (err: unknown) {
    await client.query("ROLLBACK");
    const code = (err as { code?: string }).code;
    if (code === "23505") {
      throw Object.assign(new Error("username already exists"), { status: 409 });
    }
    throw err;
  } finally {
    client.release();
  }
}

export async function login(username: string, password: string): Promise<{ account: Account; token: string }> {
  const res = await pool.query(
    `SELECT id, username, password_salt, password_hash, participant_id FROM accounts WHERE username = $1`,
    [username],
  );
  if (res.rowCount === 0) {
    throw Object.assign(new Error("invalid credentials"), { status: 401 });
  }
  const row = res.rows[0];
  const hash = hashPassword(password, row.password_salt);
  if (!crypto.timingSafeEqual(Buffer.from(hash, "hex"), Buffer.from(row.password_hash, "hex"))) {
    throw Object.assign(new Error("invalid credentials"), { status: 401 });
  }
  const token = crypto.randomBytes(32).toString("hex");
  const expires = new Date(Date.now() + config.sessionTtlHours * 3600 * 1000);
  await pool.query(`INSERT INTO sessions (token, account_id, expires_at) VALUES ($1, $2, $3)`, [
    token,
    row.id,
    expires,
  ]);
  return {
    account: { id: row.id, username: row.username, participantId: String(row.participant_id) },
    token,
  };
}

export async function accountForToken(token: string): Promise<Account | null> {
  if (!token) {
    return null;
  }
  const res = await pool.query(
    `SELECT a.id, a.username, a.participant_id
     FROM sessions s JOIN accounts a ON a.id = s.account_id
     WHERE s.token = $1 AND s.expires_at > NOW()`,
    [token],
  );
  if (res.rowCount === 0) {
    return null;
  }
  const row = res.rows[0];
  return { id: row.id, username: row.username, participantId: String(row.participant_id) };
}
