# Matching semantics

The `MatchingEngine` is the only writer of every `OrderBook`. One
`execute(command)` call is atomic from the caller's point of view: it
returns the full event list for that command.

## Priority

1. **Price:** buy higher, sell lower.
2. **Time:** FIFO within a price (`std::list` order). Time is position,
   not a timestamp comparison.

Trade price is always the **resting** order's price (maker price).

## Order handling

| incoming | after matching |
|----------|----------------|
| Limit GTC | rest remainder |
| Limit IOC | cancel remainder (`IocRemainder`) |
| Limit FOK | if `available() < qty` (skipping self), reject `FokNotFilled` with no book change; else fill fully |
| Market    | never rests; empty book → `MarketNoLiquidity`; leftover → `MarketRemainder` |

Market + GTC is treated as IOC.

## Self-trade prevention (STP-CNI)

If the next maker has the same `participant_id` as the aggressor, the
incoming remainder is canceled (`SelfTradePrevention`) and the resting
order is left unchanged. Self is not counted in FOK `available()`.

## Cancel / modify

- Cancel and modify require `participant_id` to match the owner.
- Size-down at the same price keeps time priority.
- Price change or size-up **loses** time priority (pull + rematch + rest).
- The exchange order id is kept on modify.

## Event order for a new order

1. `OrderAccepted` or `OrderRejected`
2. For each fill: maker `PrivateFill`, taker `PrivateFill`, `PublicTrade`, maker `BookDelta`
3. Remainder: `BookDelta` (rest) or `OrderCanceled`

## Complexity

- Matching k fills is O(k) plus O(log P) for each emptied or created price level. See [order-book.md](order-book.md).
- Order cancellation and modification lookups are O(1) within a target `OrderBook` via hash index. At the `MatchingEngine` level, because inbound `CancelOrderCommand` and `ModifyOrderCommand` do not currently supply `symbol_id`, the engine scans across configured symbols (O(S) where S is symbol count). Passing `symbol_id` or maintaining a global order-to-symbol index would make engine dispatch strictly O(1).
