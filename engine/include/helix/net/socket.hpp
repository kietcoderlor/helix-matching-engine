#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>

namespace helix::net {

#ifdef _WIN32
using NativeHandle = std::uintptr_t;
inline constexpr NativeHandle kInvalidHandle = static_cast<NativeHandle>(-1);
#else
using NativeHandle = int;
inline constexpr NativeHandle kInvalidHandle = -1;
#endif

void ensure_initialized();

class UniqueSocket {
 public:
  UniqueSocket() noexcept = default;
  explicit UniqueSocket(NativeHandle handle) noexcept;
  ~UniqueSocket();

  UniqueSocket(UniqueSocket&& other) noexcept;
  UniqueSocket& operator=(UniqueSocket&& other) noexcept;
  UniqueSocket(const UniqueSocket&) = delete;
  UniqueSocket& operator=(const UniqueSocket&) = delete;

  [[nodiscard]] NativeHandle get() const noexcept { return handle_; }
  [[nodiscard]] bool valid() const noexcept { return handle_ != kInvalidHandle; }
  NativeHandle release() noexcept;

  void set_nonblocking();
  void set_reuseaddr();

  // Returns bytes transferred, 0 on orderly shutdown, -1 if would-block,
  // throws on fatal errors.
  int send_some(std::span<const std::uint8_t> bytes);
  int recv_some(std::span<std::uint8_t> bytes);

 private:
  void close() noexcept;
  NativeHandle handle_{kInvalidHandle};
};

UniqueSocket tcp_listen(const char* host, std::uint16_t port);
UniqueSocket tcp_connect(const char* host, std::uint16_t port);
UniqueSocket tcp_accept(UniqueSocket& listener);
std::uint16_t local_port(const UniqueSocket& socket);

class SocketError : public std::runtime_error {
 public:
  explicit SocketError(const std::string& m);
};

}  // namespace helix::net
