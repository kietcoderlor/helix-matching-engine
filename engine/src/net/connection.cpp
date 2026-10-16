#include "helix/net/connection.hpp"

#include <algorithm>

namespace helix::net {

Connection::Connection(UniqueSocket socket, ConnectionId id)
    : socket_(std::move(socket)), id_(id) {
  inbuf_.reserve(4096);
  outbuf_.reserve(4096);
}

Connection::ReadOutcome Connection::on_readable() {
  ReadOutcome out;
  std::uint8_t tmp[4096];
  for (;;) {
    const int n = socket_.recv_some(tmp);
    if (n < 0) {
      break;  // would-block
    }
    if (n == 0) {
      close_ = CloseReason::Peer;
      out.close = close_;
      return out;
    }
    if (inbuf_.size() + static_cast<std::size_t>(n) > kMaxReadBuffer * 2) {
      close_ = CloseReason::InvalidFrame;
      out.close = close_;
      return out;
    }
    inbuf_.insert(inbuf_.end(), tmp, tmp + n);
  }

  while (true) {
    const auto decoded = try_decode_frame(inbuf_);
    if (decoded.status == DecodeStatus::NeedMore) {
      break;
    }
    if (decoded.status != DecodeStatus::Ok) {
      close_ = CloseReason::InvalidFrame;
      out.close = close_;
      return out;
    }
    inbuf_.erase(inbuf_.begin(),
                 inbuf_.begin() + static_cast<std::ptrdiff_t>(decoded.consumed));
    out.frames.push_back(*decoded.frame);
  }
  return out;
}

bool Connection::on_writable() {
  while (!outbuf_.empty()) {
    const int n = socket_.send_some(outbuf_);
    if (n < 0) {
      return true;  // still pending
    }
    if (n == 0) {
      close_ = CloseReason::Peer;
      return false;
    }
    outbuf_.erase(outbuf_.begin(), outbuf_.begin() + n);
  }
  return true;
}

bool Connection::enqueue(wire::Type type, Sequence seq, std::span<const std::uint8_t> payload) {
  if (close_ != CloseReason::None) {
    return false;
  }
  const auto bytes = encode_frame(type, seq, payload);
  if (outbuf_.size() + bytes.size() > kMaxWriteBuffer) {
    close_ = CloseReason::SlowConsumer;
    return false;
  }
  outbuf_.insert(outbuf_.end(), bytes.begin(), bytes.end());
  return true;
}

bool Connection::enqueue_event(const Event& event) {
  const auto type = event_wire_type(event.payload);
  const auto payload = encode_event_payload(event.payload);
  return enqueue(type, event.seq, payload);
}

}  // namespace helix::net
