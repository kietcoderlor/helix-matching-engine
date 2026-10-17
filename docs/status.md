# Implementation status

This file is the source of truth for what is actually implemented.
Do not claim a feature in the README until this file and the tests agree.

| Area | Status | Evidence |
|------|--------|----------|
| Repository layout, CMake, test harness | done | M1 tests pass |
| Core types / protocol spec | done | protocol unit tests |
| Order book | done | test_order_book.cpp |
| Matching, TIF, cancel/modify | done | test_matching.cpp, test_cancel_modify.cpp, test_tif_stp.cpp |
| Deterministic replay | done | test_replay.cpp, tapes/cross.golden |
| Networking (poller, framing, server) | done | test_net.cpp, test_server.cpp |
| Pipeline / backpressure | not started | |
| Gateway / PostgreSQL | not started | |
| React terminal | not started | |
| Docker / benchmarks | not started | |

This is a **simulated** exchange for learning and interviews. It is not
connected to any real market and is not production-ready.
