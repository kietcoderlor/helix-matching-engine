# Helix Simulated Exchange — implementation plan

Independent design for a Trading System Engineer portfolio project.
The public ExchangeMatchingEngine repository was used only as a reminder
of the *shape* of a teaching stack (C++ engine, TypeScript gateway, web
terminal). No source, README text, assets, or history from that project
are copied here.

## What already existed

The workspace `d:\C++ project` was empty: no source, no CMake tree, no
git history. Git author identity is configured (`user.name` / `user.email`)
so commits are allowed. Tooling on this machine:

- CMake 4.3, Visual Studio 18 2026 (MSVC) as the native Windows compiler
- Node.js 24, npm / pnpm / yarn
- Docker 29
- WSL2 Ubuntu (kernel present; g++/cmake not installed in the distro)

Linux `epoll` is the production polling backend. The poller is behind an
interface so Windows can use `WSAPoll` during local development. Engine
correctness tests do not require a network backend.

## Design goals (interview talking points)

1. **One owner of mutable book state.** A single matching thread is the
   only writer of every order book. That gives deterministic match order,
   no book locks, and a trivial mental model: "the matcher is a state
   machine that consumes a sequenced command stream."
2. **I/O stays off the match loop.** `recv`/`send`/`epoll_wait` are
   unbounded in time. They run on ingress/egress threads. Matching never
   calls into the socket layer.
3. **SPSC queues only where the contract is 1:1.** Ingress thread →
   matcher is SPSC. Matcher → egress thread is SPSC. Multi-producer
   queues are not used because they are not needed and they hide
   ordering.
4. **Never silently drop.** Queue-full means backpressure (pause reads)
   or an explicit reject/disconnect with a reason code. Market-data
   slow consumers may be disconnected; orders are not discarded.
5. **Integer prices.** `PriceTicks` and `Quantity` are `int64_t`. No
   floating-point comparison on the hot path.
6. **Gateway is not authoritative.** It authenticates users and
   translates JSON ↔ binary. The engine assigns exchange order IDs,
   owns fills, and enforces participant ownership.

## Component map

```
  React / Vite terminal
           |  HTTP + WebSocket (JSON)
           v
  TypeScript gateway (Express)
           |  session auth, input validation
           |  one TCP connection per trading session
           v
  C++20 engine
     Ingress (poll + frame) --SPSC cmd--> Matching thread --SPSC evt--> Egress
           |
           +-- per-symbol OrderBook (price-time FIFO)
  PostgreSQL  (gateway identity / sessions only; not the book of record)
```

## Matching semantics (to implement)

| Order | Resting? | Unfilled remainder |
|-------|----------|--------------------|
| Limit + GTC | yes, at limit | rests |
| Limit + IOC | no | canceled |
| Limit + FOK | no | entire order rejected if it cannot fill fully |
| Market      | no | treated as IOC against any opposing price |

- **Price-time priority:** best price first; FIFO within a price level.
  Time priority is the list position, not a timestamp compare.
- **Trade price:** the resting order's price (maker price).
- **Self-trade prevention (STP-CNI):** if the next maker is the same
  participant as the aggressor, cancel the *incoming* remainder and
  leave the resting order untouched.
- **Modify:** size-down at the same price keeps priority; price change
  or size-up cancels and re-inserts (new time priority).
- **Ownership:** cancel/modify rejected unless `participant_id` matches
  the resting order.

Complexity of the book (std::map of levels, std::list of orders, hash
index by exchange order id):

- Add: O(log P) to find/create the level + O(1) list insert
- Cancel: O(1) via iterator index + O(log P) if the level becomes empty
- Match k fills: O(k) plus O(log P) for each emptied level

P = distinct price levels on that side.

## Protocol

Versioned, little-endian, length-prefixed frames. Magic `HXCH`.
Maximum payload 64 KiB. Incomplete reads stay in a per-connection
buffer. Invalid magic, unknown version, or oversized length closes the
connection. Full layout lives in `docs/protocol.md` once the encoder
exists.

## Backpressure policy

1. Command SPSC full → ingress stops `EPOLLIN` (TCP window applies).
2. Event SPSC full → matcher retries until space; it does not drop
   events and does not read the next command until the current event
   batch is queued.
3. Per-connection write buffer over limit → that client is disconnected
   with `SlowConsumer`; other clients are unaffected.

## 15 milestones, 6 commits each (target 90)

Commits are real, cohesive, and leave the tree buildable. Counts may
flex if a change is naturally larger or smaller; history will not be
padded or backdated.

### M1 Repository foundation and build
1. License, gitignore, status doc
2. Implementation plan (this file)
3. Top-level and engine CMake
4. Version header + `helix_engine` hello executable
5. Test harness and a trivial passing test
6. Documented Windows/Linux build commands

### M2 Core types and protocol specification
1. Integer IDs, side, TIF, reject codes
2. Command and event algebraic types
3. Protocol header + message type enum
4. Little-endian encode/decode helpers
5. Protocol documentation
6. Encoder round-trip tests

### M3 Order-book data structures
1. Order and price-level types
2. Bid/ask maps with FIFO lists
3. Exchange-id index
4. Best bid/offer and depth snapshot
5. Book invariant checker
6. Book unit tests (insert/erase/best)

### M4 Matching and price-time priority
1. MatchingEngine as sole book owner
2. Limit GTC add + cross
3. Maker-price trades and partial fills
4. Market orders
5. Event emission (accepted/trade/book)
6. Priority tests (better price, earlier time)

### M5 Cancel, modify, and TIF
1. Cancel by exchange id + ownership
2. Modify size-down / size-up / price
3. IOC remainder cancel
4. FOK all-or-nothing
5. Client-order-id duplicate rejection
6. TIF + cancel/modify tests

### M6 Correctness tests and replay
1. STP tests
2. Multi-fill walks the book
3. Replay log format
4. Replay tool over a scripted tape
5. Golden-file event comparison
6. Wire the tests into CTest

### M7 Socket RAII and polling
1. Native handle + UniqueSocket
2. Poller interface
3. Linux epoll backend
4. Windows WSAPoll backend
5. Factory + RAII WSA startup
6. Loopback poll tests

### M8 Binary framing and multi-client server
1. Frame decoder (partial / invalid / oversized)
2. Frame encoder + write-buffering
3. TCP listen/accept
4. Connection table
5. Heartbeat
6. Multi-client echo/framing tests

### M9 Pipeline and backpressure
1. Bounded SPSC ring
2. Ingress parse → command queue
3. Matcher thread
4. Egress event delivery
5. Queue-full behavior
6. Pipeline tests (no silent drop)

### M10 Multi-symbol books and ownership
1. Symbol registry
2. Engine routes by symbol
3. Login maps connection → participant
4. Subscribe to public book/trades
5. Private vs public event fan-out
6. Isolation tests (two symbols, two users)

### M11 Gateway protocol client and WebSocket
1. Gateway package skeleton
2. Binary protocol client
3. Express HTTP health
4. WebSocket session glue
5. JSON order/cancel/modify mapping
6. Reconnect + client-scoped subscriptions

### M12 PostgreSQL identity
1. docker-compose Postgres
2. Migration runner
3. Accounts table
4. Sessions / tokens
5. Gateway auth middleware
6. Auth integration tests

### M13 React terminal
1. Vite + React app
2. Symbol selector + connection badge
3. Order entry with validation errors
4. Open orders + cancel/modify
5. Order book + recent trades
6. Error banner for engine rejects

### M14 E2E, Docker, observability
1. Compose stack (engine, gateway, db, terminal)
2. Structured engine logs
3. Gateway request logs
4. Multi-client integration test
5. One-command local start script
6. Troubleshooting notes

### M15 Benchmarks, docs, cleanup
1. Replay/benchmark executable
2. Measurement harness (hardware, compiler, flags)
3. Architecture README diagram
4. Limitations (no real money, no persistence of book)
5. Interview FAQ
6. Status file brought in line with the tree

## Commit-count note

The 15×6 = 90 figure was a planning target, not a quota. Early matching
milestones sometimes needed fewer commits because cancel/modify/TIF share
one `execute()` loop (splitting that loop across cosmetic commits would
have been dishonest). Later gateway/terminal/docs commits follow the
same rule: one cohesive change per commit, tree left buildable, no
empty or backdated history.

## Non-goals

- No real exchange connectivity, market data feeds, or custody
- No claim of production readiness, fairness under overload, or
  nanosecond latency
- No copying of third-party source or commit history
- No silent benchmark numbers; any figure is measured on a named
  machine with command, compiler, and build type recorded
