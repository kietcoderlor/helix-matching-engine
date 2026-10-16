#pragma once

#include "helix/codec.hpp"
#include "helix/net/socket.hpp"
#include "helix/types.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace helix::net {

inline constexpr std::size_t kMaxWriteBuffer = 1u << 20;  // 1 MiB
inline constexpr std::size_t kMaxReadBuffer = wire::kHeaderSize + wire::kMaxPayload;

enum class CloseReason {
  None = 0,
  Peer = 1,
  InvalidFrame = 2,
  SlowConsumer = 3,
  Error = 4,
};

class Connection {
 public:
  Connection(UniqueSocket socket, ConnectionId id);

  [[nodiscard]] ConnectionId id() const noexcept { return id_; }
  [[nodiscard]] NativeHandle fd() const noexcept { return socket_.get(); }
  [[nodiscard]] UniqueSocket& socket() noexcept { return socket_; }
  [[nodiscard]] bool write_pending() const noexcept { return !outbuf_.empty(); }
  [[nodiscard]] CloseReason close_reason() const noexcept { return close_; }
  [[nodiscard]] ParticipantId participant() const noexcept { return participant_; }
  void set_participant(ParticipantId p) noexcept { participant_ = p; }

  struct ReadOutcome {
    std::vector<DecodedFrame> frames;
    CloseReason close{CloseReason::None};
  };

  ReadOutcome on_readable();
  // Flush outbound. Returns false if the socket should be dropped.
  bool on_writable();

  // Enqueue a complete frame. Returns false if the write buffer would
  // exceed kMaxWriteBuffer (caller must disconnect SlowConsumer).
  bool enqueue(wire::Type type, Sequence seq, std::span<const std::uint8_t> payload);

  bool enqueue_event(const Event& event);

 private:
  UniqueSocket socket_;
  ConnectionId id_{0};
  ParticipantId participant_{kInvalidParticipant};
  std::vector<std::uint8_t> inbuf_;
  std::vector<std::uint8_t> outbuf_;
  CloseReason close_{CloseReason::None};
};

}  // namespace helix::net
