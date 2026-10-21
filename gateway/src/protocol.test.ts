import { encodeFrame, encodeNewOrder, tryDecodeFrame, Type } from "./protocol.ts";

function assert(cond: unknown, msg: string): void {
  if (!cond) {
    throw new Error(msg);
  }
}

const payload = encodeNewOrder({
  clientOrderId: 99n,
  symbol: 1,
  side: 1,
  type: 0,
  tif: 1,
  price: 12345n,
  quantity: 7n,
});
const frame = encodeFrame(Type.NewOrder, 42, payload);
const partial = tryDecodeFrame(frame.subarray(0, 10));
assert(partial.status === "need_more", "partial header");
const bad = Buffer.from(frame);
bad[0] = 0;
assert(tryDecodeFrame(bad).status === "invalid_magic", "magic");
const ok = tryDecodeFrame(frame);
assert(ok.status === "ok" && ok.frame?.seq === 42, "round trip");
assert(ok.frame?.payload.readBigUInt64LE(0) === 99n, "cid");
console.log("gateway protocol tests passed");
