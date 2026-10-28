# Interview notes

Use this as a talking outline, not a script.

1. **Why one matching thread?** Deterministic FIFO across the whole
   command stream, no book lock, simple mental model. Parallelism is
   at I/O, not inside `execute()`.
2. **Why SPSC?** Ingress→match and match→egress each have exactly one
   producer and one consumer. MPSC would add contention we do not need.
3. **Why not drop on queue full?** An exchange that loses an order is
   wrong. We stall the producer (TCP window / matcher retry) or close a
   slow market-data consumer with an explicit reason.
4. **Price-time:** best price first; `std::list` order is time. Trade
   price is the maker's price.
5. **Integer ticks:** never compare `double` on the hot path.
6. **I/O off the match loop:** `recv`/`epoll_wait` have unbounded tail
   latency relative to a map lookup.
7. **Gateway vs engine:** the gateway authenticates and translates JSON.
   Exchange order ids, fills, and ownership checks live in C++.
8. **STP-CNI:** if the next maker is the same participant, cancel the
   aggressor remainder; do not count self as FOK liquidity.
9. **Modify priority:** size-down same price keeps queue position;
   price change or size-up loses it.
10. **What we would do next in production:** persistence/replay log,
    per-symbol sharding, instrumentation, and a real risk layer — none
    of which this repo claims to have.
