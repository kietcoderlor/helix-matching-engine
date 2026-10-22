# Gateway

The TypeScript process is **not** the matching engine. It:

- registers/logs in users against PostgreSQL
- opens **one TCP connection per WebSocket session** to `helix_engine`
- translates JSON ↔ the version-1 binary protocol
- reconnects the engine socket if it drops, then re-logins and
  re-subscribes

Authorization (who may talk) is enforced here. Order state (ids, fills,
ownership, STP) is enforced in C++.

```
Browser  --JSON/WS-->  gateway  --HXCH frames-->  helix_engine
                |
           PostgreSQL (accounts, sessions)
```

Run `npm --prefix gateway test` for encode/decode checks. Matching
correctness stays in `helix_tests`.
