# Helix binary protocol (version 1)

Length-prefixed, little-endian frames. One TCP connection is one trading
session after a successful `Login`. The matching engine is authoritative;
this document describes the *wire*, not JSON used by the browser.

## Frame

```
offset  size  field
0       4     magic     ASCII H X C H  (0x48 0x58 0x43 0x48)
4       2     version   1
6       2     type      see table
8       4     payload_len
12      4     seq       per-connection sequence
16      N     payload   N = payload_len, 0 <= N <= 65536
```

Total size = 16 + N.

### Framing rules

- **Partial read:** wait. Do not interpret a short buffer as a message.
- **Partial write:** keep the unsent tail in a per-connection buffer.
- **Invalid magic / unsupported version / payload_len > 65536:** close.
  Do not skip bytes hoping to resynchronize; the stream is corrupt.
- **Unknown type with a legal length:** send `Error` (`UnknownType` is
  not a reject of an order; connection-level `Error` with the echoed seq)
  or close. Helix closes.
- Sequence numbers are assigned by the sender on that connection.
  They are not a global matching sequence.

## Inbound types (client → engine)

| type   | name         | payload |
|--------|--------------|---------|
| 0x0001 | Heartbeat    | empty |
| 0x0002 | Login        | u64 participant_id |
| 0x0003 | Logout       | empty |
| 0x0004 | Subscribe    | u32 symbol, u8 book, u8 trades, u8 pad, u8 pad |
| 0x0010 | NewOrder     | u64 client_order_id, u32 symbol, u8 side, u8 type, u8 tif, u8 pad, i64 price, i64 qty |
| 0x0011 | CancelOrder  | u64 exchange_order_id, u64 client_order_id |
| 0x0012 | ModifyOrder  | u64 exchange_order_id, u64 client_order_id, i64 new_price, i64 new_qty |

`CancelOrder` uses `exchange_order_id` when non-zero, otherwise the
client order id (scoped to the logged-in participant).

## Outbound types (engine → client)

| type   | name           | notes |
|--------|----------------|-------|
| 0x8001 | HeartbeatAck   | empty |
| 0x8002 | LoginAck       | u64 participant_id |
| 0x8003 | SessionEnd     | u16 reason, u16 pad |
| 0x8010 | OrderAccepted  | ids, symbol, side/type/tif, price, qty |
| 0x8011 | OrderRejected  | client id, exchange id, u16 reason |
| 0x8012 | OrderCanceled  | ids, i64 canceled qty, u8 reason |
| 0x8013 | OrderReplaced  | ids, price, qty, u8 priority_lost |
| 0x8014 | PrivateFill    | trade id, order ids, symbol, side, liquidity, price, qty, remaining |
| 0x8020 | PublicTrade    | trade id, symbol, aggressor, price, qty |
| 0x8021 | BookSnapshot   | symbol, nbid, nask, then levels (i64 price, i64 qty, u32 count) |
| 0x8022 | BookDelta      | symbol, side, op, price, qty, count |
| 0x80FF | Error          | u16 reason, u16 pad, u32 ref_seq |

Private events (0x8010–0x8014) go only to the owning session.
Public events (0x8020–0x8022) go to subscribers of that symbol.

## Enumerations

- Side: 0 buy, 1 sell
- OrderType: 0 limit, 1 market
- TimeInForce: 0 GTC, 1 IOC, 2 FOK
- Liquidity: 0 maker, 1 taker

Reject and cancel reason codes live in `engine/include/helix/types.hpp`.

## What this protocol is not

It is not FIX, SBE, or a real-exchange API. It exists so the engine,
gateway, and tests share one framing contract.
