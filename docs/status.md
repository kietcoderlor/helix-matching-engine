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
| Pipeline / backpressure | done | test_spsc.cpp, test_pipeline.cpp |
| Multi-symbol + event routing | done | test_routing.cpp |
| Gateway / PostgreSQL | done | `npm --prefix gateway test`; migrations/001_init.sql |
| React terminal | done | `npm --prefix terminal run build` |
| Docker / local start | done | docker-compose.yml, scripts/start-local.ps1 |
| Benchmarks | done | helix_bench; docs/performance.md (measured, not invented) |

This is a **simulated** exchange for learning and interviews. It is not
connected to any real market and is not production-ready.
