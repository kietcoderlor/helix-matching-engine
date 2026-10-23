import pg from "pg";
import { config } from "./config.ts";

export const pool = new pg.Pool({ connectionString: config.databaseUrl });

export async function withClient<T>(fn: (c: pg.PoolClient) => Promise<T>): Promise<T> {
  const c = await pool.connect();
  try {
    return await fn(c);
  } finally {
    c.release();
  }
}
