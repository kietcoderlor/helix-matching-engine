#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [ ! -f .env ]; then
  cp .env.example .env
fi
echo "Starting PostgreSQL via Docker Compose..."
docker compose up -d postgres
echo "Building C++ engine..."
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
echo "Installing JS dependencies..."
(cd gateway && npm install && npm run migrate)
(cd terminal && npm install)
cat <<'EOF'
Start these three processes:

  1) ./build/engine/helix_engine
  2) (cd gateway && npm run dev)
  3) (cd terminal && npm run dev)

Then open http://127.0.0.1:5173
This is a simulator. Not a real exchange.
EOF
