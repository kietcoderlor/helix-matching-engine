#pragma once

#include "helix/net/connection.hpp"
#include "helix/net/poller.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace helix::net {

// Multi-client TCP listener. Does not match orders; it only frames and
// dispatches complete messages through on_frame.
class TcpServer {
 public:
  using FrameFn = std::function<void(Connection& conn, const DecodedFrame& frame)>;

  TcpServer(const char* host, std::uint16_t port, FrameFn on_frame);

  [[nodiscard]] std::uint16_t port() const noexcept { return port_; }
  [[nodiscard]] std::size_t connection_count() const noexcept { return conns_.size(); }

  // One poll cycle. Safe to call from a dedicated I/O thread.
  void poll_once(std::chrono::milliseconds timeout);

  Connection* connection(ConnectionId id);
  void close(ConnectionId id, CloseReason reason);

 private:
  void accept_ready();
  void read_ready(ConnectionId id);
  void write_ready(ConnectionId id);
  void drop(ConnectionId id);

  UniqueSocket listener_;
  std::unique_ptr<Poller> poller_;
  FrameFn on_frame_;
  std::uint16_t port_{0};
  ConnectionId next_id_{1};
  std::unordered_map<ConnectionId, Connection> conns_;
  std::unordered_map<NativeHandle, ConnectionId> by_fd_;
};

}  // namespace helix::net
