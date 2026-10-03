#pragma once

#include <cstdint>
#include <string_view>

namespace helix::wire {

// On-the-wire magic is ASCII 'H','X','C','H' (0x48 0x58 0x43 0x48).
// A little-endian uint32 load of that sequence equals 0x48435848.
inline constexpr std::uint32_t kMagicLE = 0x48435848u;
inline constexpr std::uint16_t kVersion = 1;
inline constexpr std::uint32_t kHeaderSize = 16;
inline constexpr std::uint32_t kMaxPayload = 65536;

enum class Type : std::uint16_t {
  Heartbeat = 0x0001,
  Login = 0x0002,
  Logout = 0x0003,
  Subscribe = 0x0004,
  NewOrder = 0x0010,
  CancelOrder = 0x0011,
  ModifyOrder = 0x0012,

  HeartbeatAck = 0x8001,
  LoginAck = 0x8002,
  SessionEnd = 0x8003,
  OrderAccepted = 0x8010,
  OrderRejected = 0x8011,
  OrderCanceled = 0x8012,
  OrderReplaced = 0x8013,
  PrivateFill = 0x8014,
  PublicTrade = 0x8020,
  BookSnapshot = 0x8021,
  BookDelta = 0x8022,
  Error = 0x80FF,
};

struct Header {
  std::uint32_t magic{kMagicLE};
  std::uint16_t version{kVersion};
  Type type{Type::Heartbeat};
  std::uint32_t payload_size{0};
  std::uint32_t seq{0};
};

inline constexpr bool is_inbound(Type t) noexcept {
  return static_cast<std::uint16_t>(t) < 0x8000;
}

inline constexpr std::string_view magic_bytes() noexcept { return "HXCH"; }

}  // namespace helix::wire
