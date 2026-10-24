import http from "node:http";
import express from "express";
import { WebSocketServer, WebSocket } from "ws";
import { config } from "./config.ts";
import { accountForToken, login, register, type Account } from "./auth.ts";
import { EngineClient } from "./engineClient.ts";
import {
  encodeCancel,
  encodeModify,
  encodeNewOrder,
  frameToJson,
  Type,
} from "./protocol.ts";

const SYMBOLS: Record<string, number> = { AAPL: 1, MSFT: 2, GOOG: 3 };
const SIDE: Record<string, number> = { BUY: 0, SELL: 1 };
const OTYPE: Record<string, number> = { LIMIT: 0, MARKET: 1 };
const TIF: Record<string, number> = { GTC: 0, IOC: 1, FOK: 2 };

const app = express();
app.use(express.json());
app.use((req, _res, next) => {
  if (req.path !== "/api/health") {
    console.log(`${req.method} ${req.path}`);
  }
  next();
});
app.use((_req, res, next) => {
  res.setHeader("Access-Control-Allow-Origin", "*");
  res.setHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
  res.setHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  if (_req.method === "OPTIONS") {
    res.status(204).end();
    return;
  }
  next();
});

app.get("/api/health", (_req, res) => {
  res.json({ ok: true, service: "helix-gateway", simulated: true });
});

app.get("/api/symbols", (_req, res) => {
  res.json(Object.entries(SYMBOLS).map(([name, id]) => ({ name, id })));
});

app.post("/api/register", async (req, res) => {
  try {
    const account = await register(String(req.body.username ?? ""), String(req.body.password ?? ""));
    const sess = await login(account.username, String(req.body.password));
    res.json({ token: sess.token, account: sess.account });
  } catch (err) {
    const e = err as Error & { status?: number };
    res.status(e.status ?? 500).json({ error: e.message });
  }
});

app.post("/api/login", async (req, res) => {
  try {
    const sess = await login(String(req.body.username ?? ""), String(req.body.password ?? ""));
    res.json({ token: sess.token, account: sess.account });
  } catch (err) {
    const e = err as Error & { status?: number };
    res.status(e.status ?? 500).json({ error: e.message });
  }
});

app.get("/api/me", async (req, res) => {
  const token = bearer(req.headers.authorization);
  const account = await accountForToken(token);
  if (!account) {
    res.status(401).json({ error: "unauthorized" });
    return;
  }
  res.json({ account });
});

const server = http.createServer(app);
const wss = new WebSocketServer({ server, path: "/ws" });

wss.on("connection", async (ws, req) => {
  const url = new URL(req.url ?? "", "http://localhost");
  const token = url.searchParams.get("token") ?? "";
  const account = await accountForToken(token);
  if (!account) {
    ws.close(4401, "unauthorized");
    return;
  }
  const engine = new EngineClient();
  attachSession(ws, account, engine);
});

function attachSession(ws: WebSocket, account: Account, engine: EngineClient): void {
  const participant = BigInt(account.participantId);
  engine.on("frame", (frame) => {
    if (ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify(frameToJson(frame)));
    }
  });
  engine.on("status", (status) => {
    if (ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify({ type: "engine_status", status }));
    }
  });
  engine.start(participant, Object.values(SYMBOLS));

  ws.on("message", (raw) => {
    try {
      const msg = JSON.parse(String(raw)) as Record<string, unknown>;
      handleClient(engine, msg);
    } catch (err) {
      ws.send(JSON.stringify({ type: "error", error: (err as Error).message }));
    }
  });
  ws.on("close", () => engine.stop());
}

function handleClient(engine: EngineClient, msg: Record<string, unknown>): void {
  const t = String(msg.type ?? "");
  if (t === "new_order") {
    const symbolName = String(msg.symbol ?? "AAPL").toUpperCase();
    const symbol = SYMBOLS[symbolName];
    if (!symbol) {
      throw Object.assign(new Error("unknown symbol"), { status: 400 });
    }
    const side = SIDE[String(msg.side ?? "").toUpperCase()];
    const otype = OTYPE[String(msg.orderType ?? "LIMIT").toUpperCase()];
    const tif = TIF[String(msg.tif ?? "GTC").toUpperCase()];
    if (side === undefined || otype === undefined || tif === undefined) {
      throw new Error("invalid side, orderType, or tif");
    }
    const qty = BigInt(String(msg.quantity ?? "0"));
    if (qty <= 0n) {
      throw new Error("quantity must be positive");
    }
    engine.send(
      Type.NewOrder,
      encodeNewOrder({
        clientOrderId: BigInt(String(msg.clientOrderId ?? "0")),
        symbol,
        side,
        type: otype,
        tif,
        price: BigInt(String(msg.price ?? "0")),
        quantity: qty,
      }),
    );
    return;
  }
  if (t === "cancel") {
    engine.send(
      Type.CancelOrder,
      encodeCancel(BigInt(String(msg.exchangeOrderId ?? "0")), BigInt(String(msg.clientOrderId ?? "0"))),
    );
    return;
  }
  if (t === "modify") {
    engine.send(
      Type.ModifyOrder,
      encodeModify(
        BigInt(String(msg.exchangeOrderId ?? "0")),
        BigInt(String(msg.clientOrderId ?? "0")),
        BigInt(String(msg.price ?? "0")),
        BigInt(String(msg.quantity ?? "0")),
      ),
    );
    return;
  }
  throw new Error("unknown message type");
}

function bearer(h: string | undefined): string {
  if (!h) {
    return "";
  }
  return h.startsWith("Bearer ") ? h.slice(7) : h;
}

server.listen(config.gatewayPort, () => {
  console.log(`helix-gateway http://127.0.0.1:${config.gatewayPort} (simulated, not production)`);
});
