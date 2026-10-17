#include "helix/net/server.hpp"

#include "helix/endian.hpp"

#include <array>

namespace helix::net {

TcpServer::TcpServer(const char* host, std::uint16_t port, FrameFn on_frame)
    : listener_(tcp_listen(host, port)),
      poller_(create_poller()),
      on_frame_(std::move(on_frame)),
      port_(local_port(listener_)) {
  poller_->add(listener_.get(), Interest::Read);
}

Connection* TcpServer::connection(ConnectionId id) {
  const auto it = conns_.find(id);
  return it == conns_.end() ? nullptr : &it->second;
}

void TcpServer::close(ConnectionId id, CloseReason reason) {
  const auto it = conns_.find(id);
  if (it == conns_.end()) {
    return;
  }
  (void)reason;
  drop(id);
}

void TcpServer::drop(ConnectionId id) {
  const auto it = conns_.find(id);
  if (it == conns_.end()) {
    return;
  }
  const NativeHandle fd = it->second.fd();
  try {
    poller_->del(fd);
  } catch (...) {
  }
  by_fd_.erase(fd);
  conns_.erase(it);
}

void TcpServer::accept_ready() {
  for (;;) {
    UniqueSocket sock = tcp_accept(listener_);
    if (!sock.valid()) {
      break;
    }
    const ConnectionId id = next_id_++;
    const NativeHandle fd = sock.get();
    Connection conn(std::move(sock), id);
    poller_->add(fd, Interest::Read);
    by_fd_.emplace(fd, id);
    conns_.emplace(id, std::move(conn));
  }
}

void TcpServer::read_ready(ConnectionId id) {
  Connection* conn = connection(id);
  if (conn == nullptr) {
    return;
  }
  auto outcome = conn->on_readable();
  for (const auto& frame : outcome.frames) {
    if (on_frame_) {
      on_frame_(*conn, frame);
    }
  }
  if (outcome.close != CloseReason::None || conn->close_reason() != CloseReason::None) {
    drop(id);
    return;
  }
  if (conn->write_pending()) {
    poller_->mod(conn->fd(), Interest::Read | Interest::Write);
  }
}

void TcpServer::write_ready(ConnectionId id) {
  Connection* conn = connection(id);
  if (conn == nullptr) {
    return;
  }
  if (!conn->on_writable()) {
    drop(id);
    return;
  }
  if (!conn->write_pending()) {
    poller_->mod(conn->fd(), Interest::Read);
  }
}

void TcpServer::poll_once(std::chrono::milliseconds timeout) {
  std::array<Ready, 64> ready{};
  const int n = poller_->wait(ready, timeout);
  for (int i = 0; i < n; ++i) {
    const Ready& r = ready[static_cast<std::size_t>(i)];
    if (r.fd == listener_.get()) {
      if (r.readable) {
        accept_ready();
      }
      continue;
    }
    const auto it = by_fd_.find(r.fd);
    if (it == by_fd_.end()) {
      continue;
    }
    const ConnectionId id = it->second;
    if (r.error || r.hangup) {
      if (r.readable) {
        read_ready(id);
      } else {
        drop(id);
      }
      continue;
    }
    if (r.readable) {
      read_ready(id);
    }
    if (r.writable && connection(id) != nullptr) {
      write_ready(id);
    }
  }
}

}  // namespace helix::net
