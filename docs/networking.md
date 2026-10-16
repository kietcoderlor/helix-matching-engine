# Networking

Socket I/O never runs inside `MatchingEngine::execute`.

## Poller

`helix::net::Poller` is the only OS-specific wait API.

| platform | backend | file |
|----------|---------|------|
| Linux | `epoll` | `engine/src/net/poller_epoll.cpp` |
| Windows | `WSAPoll` | `engine/src/net/poller_wsapoll.cpp` |

`create_poller()` picks the backend at compile time. Adding kqueue later
means a third `.cpp` and a CMake `elseif`, not a rewrite of the server.

## RAII

`UniqueSocket` closes on destruction and is move-only. `ensure_initialized()`
owns `WSAStartup`/`WSACleanup` on Windows via a function-local static.

Sockets used by the server are non-blocking. `send_some`/`recv_some`
return `-1` on would-block, `0` on orderly shutdown, and throw on fatal
errors.
