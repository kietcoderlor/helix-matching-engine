# Performance notes

These figures were produced by `helix_bench` on this machine. They are
**in-process** `MatchingEngine::execute()` timings: no sockets, no
gateway, no JSON. They are not a production SLA.

## How to measure

```
cmake -B build -S . -A x64
cmake --build build --config Release
.\build\engine\Release\helix_bench.exe --orders 100000
```

## Recorded run

Captured 2026-10-30 on the development host. Output copied verbatim:

```
Helix matching microbenchmark (in-process execute(), no sockets)
engine: helix-engine 0.1.0
build: Release (NDEBUG)
compiler: MSVC 1950
host: DESKTOP-NC78VN5
cpu: AMD64 Family 25 Model 117 Stepping 2, AuthenticAMD
command: helix_bench --orders 100000
orders: 100000
wall_seconds: 0.0883666
throughput_per_s: 1.13165e+06
latency_ns_p50: 500
latency_ns_p99: 1700
latency_ns_max: 10867700
note: figures are for this run only; do not copy them as claims.
```

Additional context not printed by the binary:

- OS: Microsoft Windows NT 10.0.22631.0
- CPU name: AMD Ryzen 7 8845H w/ Radeon 780M Graphics
- CMake: 4.3.0
- Generator: Visual Studio 18 2026, `-A x64`, `--config Release`
- `p50: 500 ns` is likely near the `steady_clock` resolution on this
  host; treat it as a lower bound, not a physical 500 ns match.

Do not reuse these numbers on another machine or build type.
