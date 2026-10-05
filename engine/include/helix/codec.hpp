#pragma once

#include "helix/command.hpp"
#include "helix/event.hpp"
#include "helix/types.hpp"
#include "helix/wire.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace helix {

enum class DecodeStatus {
  Ok = 0,
  NeedMore = 1,
  InvalidMagic = 2,
  UnsupportedVersion = 3,
  PayloadTooLarge = 4,
  UnknownType = 5,
  BadPayload = 6,
};

struct DecodedFrame {
  wire::Header header{};
  std::vector<std::uint8_t> payload;
};

struct DecodeResult {
  DecodeStatus status{DecodeStatus::NeedMore};
  std::optional<DecodedFrame> frame;
  std::size_t consumed{0};
};

// Returns NeedMore if `bytes` is shorter than a full frame.
// On Invalid* / PayloadTooLarge, `consumed` is 0; the caller should close.
DecodeResult try_decode_frame(std::span<const std::uint8_t> bytes);

std::vector<std::uint8_t> encode_frame(wire::Type type, Sequence seq,
                                       std::span<const std::uint8_t> payload);

std::vector<std::uint8_t> encode_command_payload(const CommandPayload& payload);
std::vector<std::uint8_t> encode_event_payload(const EventPayload& payload);

DecodeStatus decode_command_payload(wire::Type type,
                                    std::span<const std::uint8_t> payload,
                                    CommandPayload& out);

DecodeStatus decode_event_payload(wire::Type type,
                                  std::span<const std::uint8_t> payload,
                                  EventPayload& out);

wire::Type command_wire_type(const CommandPayload& payload);
wire::Type event_wire_type(const EventPayload& payload);

}  // namespace helix
