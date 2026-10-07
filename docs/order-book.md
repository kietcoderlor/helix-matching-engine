# Order book

One `OrderBook` instance per symbol. The matching thread is the only
writer. There is no lock on this object.

## Layout

```
bids: std::map<price, PriceLevel, greater>   // best (highest) at begin()
asks: std::map<price, PriceLevel, less>      // best (lowest) at begin()

PriceLevel:
  std::list<RestingOrder>   // FIFO time priority
  Quantity total            // sum of remaining

indexes:
  exchange_order_id -> {side, price, list iterator}
  (participant, client_order_id) -> exchange_order_id
```

`std::list` iterators stay valid when other nodes are inserted or erased,
which is why cancel is O(1) after the hash lookup.

## Complexity

| operation | time |
|-----------|------|
| rest (add) | O(log P) map + O(1) list push |
| cancel / reduce | O(1) hash + O(1) list erase; O(log P) if the level is removed |
| best | O(1) |
| depth of L levels | O(L) |
| FOK available() | O(orders on the walk) |

P = distinct prices on that side.

## What the book does not do

It does not match, assign ids, or talk to the network. A crossed book is a
matching-engine bug (`check_uncrossed()`), not something the book repairs.
