#pragma once

#include "helix/net/socket.hpp"

#include <chrono>
#include <cstdint>
#include <memory>
#include <span>

namespace helix::net {

enum class Interest : std::uint32_t {
  None = 0,
  Read = 1u << 0,
  Write = 1u << 1,
};

inline constexpr Interest operator|(Interest a, Interest b) noexcept {
  return static_cast<Interest>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}

inline constexpr Interest operator&(Interest a, Interest b) noexcept {
  return static_cast<Interest>(static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b));
}

inline constexpr bool has(Interest mask, Interest bit) noexcept {
  return (static_cast<std::uint32_t>(mask) & static_cast<std::uint32_t>(bit)) != 0;
}

struct Ready {
  NativeHandle fd{kInvalidHandle};
  bool readable{false};
  bool writable{false};
  bool hangup{false};
  bool error{false};
};

// OS-specific wait backend. Linux uses epoll; Windows uses WSAPoll.
// The TCP server talks only to this interface.
class Poller {
 public:
  virtual ~Poller() = default;
  virtual void add(NativeHandle fd, Interest interest) = 0;
  virtual void mod(NativeHandle fd, Interest interest) = 0;
  virtual void del(NativeHandle fd) = 0;
  // Returns the number of filled events. 0 means timeout.
  virtual int wait(std::span<Ready> out, std::chrono::milliseconds timeout) = 0;
  [[nodiscard]] virtual const char* name() const noexcept = 0;
};

std::unique_ptr<Poller> create_poller();

}  // namespace helix::net
