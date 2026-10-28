# Limitations

Helix is a **teaching simulator**. It is not a real exchange.

- No connection to any live market, broker, or custody system.
- No claim of production readiness, fairness under overload, or
  regulatory compliance.
- The matching engine does **not** persist the book. Restarting
  `helix_engine` empties every order book.
- PostgreSQL stores gateway identity/sessions only.
- Prices are integer ticks; there is no FX, corporate action, or
  halt logic.
- Self-trade prevention is STP-CNI only (cancel incoming).
- The poller is epoll on Linux and WSAPoll on Windows. kqueue is
  not implemented.
- Benchmarks are in-process `execute()` timings. They are not
  comparable to a production matching venue.
