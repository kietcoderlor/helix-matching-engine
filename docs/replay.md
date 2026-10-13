# Deterministic replay

`helix_replay` feeds a text tape into `MatchingEngine::execute` in order.
The same tape must emit the same `stringify_event` lines every time.
That is the regression oracle for matching — not wall-clock time.

## Tape syntax

```
# comment
SYMBOL <id> <name>
TS <nanoseconds>          # optional; otherwise commands increment a counter
NEW <participant> <cid> <symbol> BUY|SELL LIMIT|MARKET GTC|IOC|FOK <px> <qty>
CANCEL <participant> <exchange_id> [client_id]
MODIFY <participant> <exchange_id> <px> <qty>
```

## Golden files

`engine/tapes/cross.tape` is checked in with `engine/tapes/cross.golden`.
Regenerate the golden only when a deliberate semantic change happens:

```
helix_replay engine/tapes/cross.tape engine/tapes/cross.golden
```
