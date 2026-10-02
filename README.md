# Helix Simulated Exchange

A **simulated** limit-order exchange for learning matching-engine,
networking, and gateway design. It is aimed at Trading System Engineer
interviews. It is **not** a real exchange, does not connect to any live
market, and is **not** production-ready.

## Current status

Foundation only: repository layout, license, and the implementation
plan. See [docs/status.md](docs/status.md) and
[IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md).

## Intended architecture

```
Browser terminal  --JSON/WS-->  TypeScript gateway  --binary TCP-->  C++ engine
                                      |
                                 PostgreSQL
                                      (identity only)
```

The C++ matching thread is the only writer of order-book state.
Networking, JSON, and the UI never mutate the book directly.

## Build (once the engine CMake lands)

```bash
cmake -B build -S . -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Linux (epoll backend):

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## License

MIT. See [LICENSE](LICENSE).
