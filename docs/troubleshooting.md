# Troubleshooting

## Engine will not bind

Another process owns `HELIX_PORT` (default 9001). Set a free port in
`.env` and match `HELIX_ENGINE_PORT` in the gateway.

## Gateway `engine disconnected`

Start `helix_engine` before the gateway. The gateway reconnects every
second; you should see `engine_status: connected` on the websocket.

## `npm run migrate` fails

PostgreSQL is not up. From the repo root:

```
docker compose up -d postgres
```

Default URL: `postgres://helix:helix@127.0.0.1:5432/helix`

## Terminal cannot log in

Register first (password ≥ 8 characters). Check the gateway is on
port 3001. Vite proxies `/api` and `/ws` there.

## Tests fail on the golden tape

`engine/tapes/cross.golden` must match `helix_replay`. Only regenerate
it when matching semantics change on purpose.

## Slow-consumer disconnects

A client that does not read events fills a 1 MiB write buffer and is
dropped. This is intentional and is not a silent drop of *orders*.
