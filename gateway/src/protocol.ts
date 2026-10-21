const MAGIC = Buffer.from("HXCH");
const VERSION = 1;
export const HEADER_SIZE = 16;
export const MAX_PAYLOAD = 65536;

export const Type = {
  Heartbeat: 0x0001,
  Login: 0x0002,
  Logout: 0x0003,
  Subscribe: 0x0004,
  NewOrder: 0x0010,
  CancelOrder: 0x0011,
  ModifyOrder: 0x0012,
  HeartbeatAck: 0x8001,
  LoginAck: 0x8002,
  SessionEnd: 0x8003,
  OrderAccepted: 0x8010,
  OrderRejected: 0x8011,
  OrderCanceled: 0x8012,
  OrderReplaced: 0x8013,
  PrivateFill: 0x8014,
  PublicTrade: 0x8020,
  BookSnapshot: 0x8021,
  BookDelta: 0x8022,
  Error: 0x80ff,
} as const;

export type WireType = (typeof Type)[keyof typeof Type];

export interface Frame {
  type: number;
  seq: number;
  payload: Buffer;
}

export function encodeFrame(type: number, seq: number, payload: Buffer): Buffer {
  if (payload.length > MAX_PAYLOAD) {
    throw new Error("payload too large");
  }
  const buf = Buffer.alloc(HEADER_SIZE + payload.length);
  MAGIC.copy(buf, 0);
  buf.writeUInt16LE(VERSION, 4);
  buf.writeUInt16LE(type, 6);
  buf.writeUInt32LE(payload.length, 8);
  buf.writeUInt32LE(seq >>> 0, 12);
  payload.copy(buf, HEADER_SIZE);
  return buf;
}

export type DecodeStatus = "ok" | "need_more" | "invalid_magic" | "bad_version" | "too_large";

export function tryDecodeFrame(buf: Buffer): { status: DecodeStatus; frame?: Frame; consumed: number } {
  if (buf.length < HEADER_SIZE) {
    return { status: "need_more", consumed: 0 };
  }
  if (buf.subarray(0, 4).toString("ascii") !== "HXCH") {
    return { status: "invalid_magic", consumed: 0 };
  }
  const version = buf.readUInt16LE(4);
  if (version !== VERSION) {
    return { status: "bad_version", consumed: 0 };
  }
  const type = buf.readUInt16LE(6);
  const plen = buf.readUInt32LE(8);
  const seq = buf.readUInt32LE(12);
  if (plen > MAX_PAYLOAD) {
    return { status: "too_large", consumed: 0 };
  }
  if (buf.length < HEADER_SIZE + plen) {
    return { status: "need_more", consumed: 0 };
  }
  return {
    status: "ok",
    consumed: HEADER_SIZE + plen,
    frame: { type, seq, payload: buf.subarray(HEADER_SIZE, HEADER_SIZE + plen) },
  };
}

export function encodeLogin(participant: bigint): Buffer {
  const b = Buffer.alloc(8);
  b.writeBigUInt64LE(participant, 0);
  return b;
}

export function encodeSubscribe(symbol: number, book: boolean, trades: boolean): Buffer {
  const b = Buffer.alloc(8);
  b.writeUInt32LE(symbol, 0);
  b.writeUInt8(book ? 1 : 0, 4);
  b.writeUInt8(trades ? 1 : 0, 5);
  return b;
}

export function encodeNewOrder(o: {
  clientOrderId: bigint;
  symbol: number;
  side: number;
  type: number;
  tif: number;
  price: bigint;
  quantity: bigint;
}): Buffer {
  const b = Buffer.alloc(32);
  b.writeBigUInt64LE(o.clientOrderId, 0);
  b.writeUInt32LE(o.symbol, 8);
  b.writeUInt8(o.side, 12);
  b.writeUInt8(o.type, 13);
  b.writeUInt8(o.tif, 14);
  b.writeBigInt64LE(o.price, 16);
  b.writeBigInt64LE(o.quantity, 24);
  return b;
}

export function encodeCancel(exchangeId: bigint, clientId: bigint): Buffer {
  const b = Buffer.alloc(16);
  b.writeBigUInt64LE(exchangeId, 0);
  b.writeBigUInt64LE(clientId, 8);
  return b;
}

export function encodeModify(exchangeId: bigint, clientId: bigint, price: bigint, qty: bigint): Buffer {
  const b = Buffer.alloc(32);
  b.writeBigUInt64LE(exchangeId, 0);
  b.writeBigUInt64LE(clientId, 8);
  b.writeBigInt64LE(price, 16);
  b.writeBigInt64LE(qty, 24);
  return b;
}

function u64(buf: Buffer, off: number): string {
  return buf.readBigUInt64LE(off).toString();
}
function i64(buf: Buffer, off: number): string {
  return buf.readBigInt64LE(off).toString();
}

export function frameToJson(frame: Frame): Record<string, unknown> {
  const p = frame.payload;
  switch (frame.type) {
    case Type.HeartbeatAck:
      return { type: "heartbeat_ack", seq: frame.seq };
    case Type.LoginAck:
      return { type: "login_ack", participantId: u64(p, 0) };
    case Type.OrderAccepted:
      return {
        type: "order_accepted",
        exchangeOrderId: u64(p, 0),
        clientOrderId: u64(p, 8),
        symbol: p.readUInt32LE(16),
        side: p.readUInt8(20),
        orderType: p.readUInt8(21),
        tif: p.readUInt8(22),
        price: i64(p, 24),
        quantity: i64(p, 32),
      };
    case Type.OrderRejected:
      return {
        type: "order_rejected",
        clientOrderId: u64(p, 0),
        exchangeOrderId: u64(p, 8),
        reason: p.readUInt16LE(16),
      };
    case Type.OrderCanceled:
      return {
        type: "order_canceled",
        exchangeOrderId: u64(p, 0),
        clientOrderId: u64(p, 8),
        canceled: i64(p, 16),
        reason: p.readUInt8(24),
      };
    case Type.OrderReplaced:
      return {
        type: "order_replaced",
        exchangeOrderId: u64(p, 0),
        clientOrderId: u64(p, 8),
        price: i64(p, 16),
        quantity: i64(p, 24),
        priorityLost: p.readUInt8(32) !== 0,
      };
    case Type.PrivateFill:
      return {
        type: "private_fill",
        tradeId: u64(p, 0),
        exchangeOrderId: u64(p, 8),
        clientOrderId: u64(p, 16),
        symbol: p.readUInt32LE(24),
        side: p.readUInt8(28),
        liquidity: p.readUInt8(29),
        price: i64(p, 32),
        quantity: i64(p, 40),
        remaining: i64(p, 48),
      };
    case Type.PublicTrade:
      return {
        type: "public_trade",
        tradeId: u64(p, 0),
        symbol: p.readUInt32LE(8),
        aggressor: p.readUInt8(12),
        price: i64(p, 16),
        quantity: i64(p, 24),
      };
    case Type.BookSnapshot: {
      const symbol = p.readUInt32LE(0);
      const nbid = p.readUInt16LE(4);
      const nask = p.readUInt16LE(6);
      let off = 8;
      const readLevels = (n: number) => {
        const levels = [];
        for (let i = 0; i < n; i++) {
          levels.push({
            price: i64(p, off),
            quantity: i64(p, off + 8),
            orderCount: p.readUInt32LE(off + 16),
          });
          off += 20;
        }
        return levels;
      };
      return { type: "book_snapshot", symbol, bids: readLevels(nbid), asks: readLevels(nask) };
    }
    case Type.BookDelta:
      return {
        type: "book_delta",
        symbol: p.readUInt32LE(0),
        side: p.readUInt8(4),
        price: i64(p, 8),
        quantity: i64(p, 16),
        orderCount: p.readUInt32LE(24),
      };
    case Type.Error:
      return { type: "error", reason: p.readUInt16LE(0), refSeq: p.readUInt32LE(4) };
    case Type.SessionEnd:
      return { type: "session_end", reason: p.readUInt16LE(0) };
    default:
      return { type: "unknown", wireType: frame.type, seq: frame.seq };
  }
}
