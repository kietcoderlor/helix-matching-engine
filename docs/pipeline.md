# Pipeline and backpressure

```
I/O thread                         Matching thread
-----------                        ----------------
poll / accept / recv
frame decode  (ingress)
try_push Command  ---SPSC--->      try_pop Command
                                   MatchingEngine::execute
try_pop Event     <---SPSC---      try_push Event
route + send      (egress)
```

The matching thread is the only caller of `execute()`. Heartbeat and
login are session-layer and stay on the I/O thread.

## Queue-full behavior

- **Command SPSC full:** the I/O thread retries `try_push` (yield). It
  does not read the next frame until the current command is queued.
  TCP applies window backpressure. The command is **not** dropped.
- **Event SPSC full:** the matching thread retries `try_push` and does
  not take the next command. Events are **not** dropped.
- **Per-connection write buffer > 1 MiB:** that client is disconnected
  as `SlowConsumer`. Other clients are unaffected.

`try_push` moves the item only on success so a failed push cannot lose
the value.
