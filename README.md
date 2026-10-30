# Helix Simulated Exchange

A **simulated** limit-order exchange for learning matching-engine,
networking, and gateway design. Aimed at Trading System Engineer
interviews. **Not** a real exchange, **not** connected to any live
market, **not** production-ready.

## Architecture

```
  React / Vite terminal (:5173)
           |  JSON HTTP + WebSocket
           v
  TypeScript gateway (:3001)
           |  sessions in PostgreSQL
           |  one TCP session per logged-in user
           v
  C++20 helix_engine (:9001)
     I/O thread  --SPSC commands-->  matching thread  --SPSC events-->  I/O thread
                      |
                      + per-symbol price-time books
```

The matching thread is the **only** writer of order-book state.
See [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md),
[docs/matching.md](docs/matching.md), [docs/protocol.md](docs/protocol.md),
[docs/pipeline.md](docs/pipeline.md), [docs/limitations.md](docs/limitations.md).

## One-command local bootstrap

```powershell
.\scripts\start-local.ps1
```

Then run engine, gateway, and terminal as the script prints.

Linux:

```bash
./scripts/start-local.sh
```

## Manual start

```powershell
copy .env.example .env
docker compose up -d postgres
cmake -B build -S . -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure

.\build\engine\Release\helix_engine.exe

cd gateway
npm install
npm run migrate
npm run test
npm run dev

cd ..\terminal
npm install
npm run dev
```

Open http://127.0.0.1:5173 — register a user (password ≥ 8 chars).

Environment variables are listed in [.env.example](.env.example).

## Tests

C++ matching, protocol, networking, and pipeline tests:

```powershell
.\build\engine\Release\helix_tests.exe
```

Gateway framing:

```powershell
cd gateway
npm run test
```

## Benchmarks

```powershell
.\build\engine\Release\helix_bench.exe --orders 100000
```

Results are machine-specific. See [docs/performance.md](docs/performance.md)
for a recorded run. Do not copy numbers you did not measure.

## License

MIT. See [LICENSE](LICENSE).
