# One-command local stack for the simulated exchange.
$ErrorActionPreference = "Stop"
Set-Location (Split-Path $PSScriptRoot -Parent)

if (-not (Test-Path .env)) {
  Copy-Item .env.example .env
}

Write-Host "Starting PostgreSQL via Docker Compose..."
docker compose up -d postgres

Write-Host "Building C++ engine (Release)..."
if (-not (Test-Path build)) {
  cmake -B build -S . -A x64
}
cmake --build build --config Release

Write-Host "Installing JS dependencies..."
Push-Location gateway
npm install
npm run migrate
Pop-Location
Push-Location terminal
npm install
Pop-Location

Write-Host @"
Start these three processes (separate terminals):

  1) .\build\engine\Release\helix_engine.exe
  2) cd gateway; npm run dev
  3) cd terminal; npm run dev

Then open http://127.0.0.1:5173
This is a simulator. Not a real exchange.
"@
