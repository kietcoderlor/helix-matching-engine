#include "helix/codec.hpp"
#include "helix/endian.hpp"

#include <stdexcept>

namespace helix {
namespace {

void write_header(std::vector<std::uint8_t>& out, wire::Type type, Sequence seq,
                  std::uint32_t payload_size) {
  write_u32le(out, wire::kMagicLE);
  write_u16le(out, wire::kVersion);
  write_u16le(out, static_cast<std::uint16_t>(type));
  write_u32le(out, payload_size);
  write_u32le(out, seq);
}

}  // namespace

DecodeResult try_decode_frame(std::span<const std::uint8_t> bytes) {
  DecodeResult r;
  if (bytes.size() < wire::kHeaderSize) {
    r.status = DecodeStatus::NeedMore;
    return r;
  }
  ByteReader rd(bytes);
  const auto magic = rd.u32le();
  if (magic != wire::kMagicLE) {
    r.status = DecodeStatus::InvalidMagic;
    return r;
  }
  const auto version = rd.u16le();
  if (version != wire::kVersion) {
    r.status = DecodeStatus::UnsupportedVersion;
    return r;
  }
  const auto type = static_cast<wire::Type>(rd.u16le());
  const auto payload_size = rd.u32le();
  const auto seq = rd.u32le();
  if (payload_size > wire::kMaxPayload) {
    r.status = DecodeStatus::PayloadTooLarge;
    return r;
  }
  const std::size_t total = wire::kHeaderSize + payload_size;
  if (bytes.size() < total) {
    r.status = DecodeStatus::NeedMore;
    return r;
  }
  DecodedFrame frame;
  frame.header.magic = magic;
  frame.header.version = version;
  frame.header.type = type;
  frame.header.payload_size = payload_size;
  frame.header.seq = seq;
  frame.payload.assign(bytes.begin() + wire::kHeaderSize, bytes.begin() + static_cast<std::ptrdiff_t>(total));
  r.status = DecodeStatus::Ok;
  r.frame = std::move(frame);
  r.consumed = total;
  return r;
}

std::vector<std::uint8_t> encode_frame(wire::Type type, Sequence seq,
                                       std::span<const std::uint8_t> payload) {
  if (payload.size() > wire::kMaxPayload) {
    throw std::runtime_error("payload exceeds kMaxPayload");
  }
  std::vector<std::uint8_t> out;
  out.reserve(wire::kHeaderSize + payload.size());
  write_header(out, type, seq, static_cast<std::uint32_t>(payload.size()));
  out.insert(out.end(), payload.begin(), payload.end());
  return out;
}

template <class... Ts>
struct Overloaded : Ts... {
  using Ts::operator()...;
};
template <class... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

wire::Type command_wire_type(const CommandPayload& payload) {
  return std::visit(Overloaded{
                        [](const HeartbeatCommand&) { return wire::Type::Heartbeat; },
                        [](const LoginCommand&) { return wire::Type::Login; },
                        [](const LogoutCommand&) { return wire::Type::Logout; },
                        [](const SubscribeCommand&) { return wire::Type::Subscribe; },
                        [](const NewOrderCommand&) { return wire::Type::NewOrder; },
                        [](const CancelOrderCommand&) { return wire::Type::CancelOrder; },
                        [](const ModifyOrderCommand&) { return wire::Type::ModifyOrder; },
                    },
                    payload);
}

wire::Type event_wire_type(const EventPayload& payload) {
  return std::visit(Overloaded{
                        [](const HeartbeatAckEvent&) { return wire::Type::HeartbeatAck; },
                        [](const LoginAckEvent&) { return wire::Type::LoginAck; },
                        [](const SessionEndEvent&) { return wire::Type::SessionEnd; },
                        [](const OrderAcceptedEvent&) { return wire::Type::OrderAccepted; },
                        [](const OrderRejectedEvent&) { return wire::Type::OrderRejected; },
                        [](const OrderCanceledEvent&) { return wire::Type::OrderCanceled; },
                        [](const OrderReplacedEvent&) { return wire::Type::OrderReplaced; },
                        [](const PrivateFillEvent&) { return wire::Type::PrivateFill; },
                        [](const PublicTradeEvent&) { return wire::Type::PublicTrade; },
                        [](const BookSnapshotEvent&) { return wire::Type::BookSnapshot; },
                        [](const BookDeltaEvent&) { return wire::Type::BookDelta; },
                        [](const ErrorEvent&) { return wire::Type::Error; },
                    },
                    payload);
}

std::vector<std::uint8_t> encode_command_payload(const CommandPayload& payload) {
  std::vector<std::uint8_t> out;
  std::visit(
      [&](const auto& p) {
        using T = std::decay_t<decltype(p)>;
        if constexpr (std::is_same_v<T, HeartbeatCommand> || std::is_same_v<T, LogoutCommand>) {
          return;
        } else if constexpr (std::is_same_v<T, LoginCommand>) {
          write_u64le(out, p.participant);
        } else if constexpr (std::is_same_v<T, SubscribeCommand>) {
          write_u32le(out, p.symbol);
          write_u8(out, p.book ? 1 : 0);
          write_u8(out, p.trades ? 1 : 0);
          write_u8(out, 0);
          write_u8(out, 0);
        } else if constexpr (std::is_same_v<T, NewOrderCommand>) {
          write_u64le(out, p.client_order_id);
          write_u32le(out, p.symbol);
          write_u8(out, static_cast<std::uint8_t>(p.side));
          write_u8(out, static_cast<std::uint8_t>(p.type));
          write_u8(out, static_cast<std::uint8_t>(p.tif));
          write_u8(out, 0);
          write_i64le(out, p.price);
          write_i64le(out, p.quantity);
        } else if constexpr (std::is_same_v<T, CancelOrderCommand>) {
          write_u64le(out, p.exchange_order_id);
          write_u64le(out, p.client_order_id);
        } else if constexpr (std::is_same_v<T, ModifyOrderCommand>) {
          write_u64le(out, p.exchange_order_id);
          write_u64le(out, p.client_order_id);
          write_i64le(out, p.new_price);
          write_i64le(out, p.new_quantity);
        }
      },
      payload);
  return out;
}

std::vector<std::uint8_t> encode_event_payload(const EventPayload& payload) {
  std::vector<std::uint8_t> out;
  std::visit(
      [&](const auto& p) {
        using T = std::decay_t<decltype(p)>;
        if constexpr (std::is_same_v<T, HeartbeatAckEvent>) {
          return;
        } else if constexpr (std::is_same_v<T, LoginAckEvent>) {
          write_u64le(out, p.participant);
        } else if constexpr (std::is_same_v<T, SessionEndEvent>) {
          write_u16le(out, static_cast<std::uint16_t>(p.reason));
          write_u16le(out, 0);
        } else if constexpr (std::is_same_v<T, OrderAcceptedEvent>) {
          write_u64le(out, p.exchange_order_id);
          write_u64le(out, p.client_order_id);
          write_u32le(out, p.symbol);
          write_u8(out, static_cast<std::uint8_t>(p.side));
          write_u8(out, static_cast<std::uint8_t>(p.type));
          write_u8(out, static_cast<std::uint8_t>(p.tif));
          write_u8(out, 0);
          write_i64le(out, p.price);
          write_i64le(out, p.quantity);
        } else if constexpr (std::is_same_v<T, OrderRejectedEvent>) {
          write_u64le(out, p.client_order_id);
          write_u64le(out, p.exchange_order_id);
          write_u16le(out, static_cast<std::uint16_t>(p.reason));
          write_u16le(out, 0);
        } else if constexpr (std::is_same_v<T, OrderCanceledEvent>) {
          write_u64le(out, p.exchange_order_id);
          write_u64le(out, p.client_order_id);
          write_i64le(out, p.canceled);
          write_u8(out, static_cast<std::uint8_t>(p.reason));
          write_u8(out, 0);
          write_u8(out, 0);
          write_u8(out, 0);
        } else if constexpr (std::is_same_v<T, OrderReplacedEvent>) {
          write_u64le(out, p.exchange_order_id);
          write_u64le(out, p.client_order_id);
          write_i64le(out, p.price);
          write_i64le(out, p.quantity);
          write_u8(out, p.priority_lost ? 1 : 0);
          write_u8(out, 0);
          write_u8(out, 0);
          write_u8(out, 0);
        } else if constexpr (std::is_same_v<T, PrivateFillEvent>) {
          write_u64le(out, p.trade_id);
          write_u64le(out, p.exchange_order_id);
          write_u64le(out, p.client_order_id);
          write_u32le(out, p.symbol);
          write_u8(out, static_cast<std::uint8_t>(p.side));
          write_u8(out, static_cast<std::uint8_t>(p.liquidity));
          write_u8(out, 0);
          write_u8(out, 0);
          write_i64le(out, p.price);
          write_i64le(out, p.quantity);
          write_i64le(out, p.remaining);
        } else if constexpr (std::is_same_v<T, PublicTradeEvent>) {
          write_u64le(out, p.trade_id);
          write_u32le(out, p.symbol);
          write_u8(out, static_cast<std::uint8_t>(p.aggressor));
          write_u8(out, 0);
          write_u8(out, 0);
          write_u8(out, 0);
          write_i64le(out, p.price);
          write_i64le(out, p.quantity);
        } else if constexpr (std::is_same_v<T, BookSnapshotEvent>) {
          write_u32le(out, p.symbol);
          write_u16le(out, static_cast<std::uint16_t>(p.bids.size()));
          write_u16le(out, static_cast<std::uint16_t>(p.asks.size()));
          auto write_levels = [&](const std::vector<BookLevel>& levels) {
            for (const auto& lv : levels) {
              write_i64le(out, lv.price);
              write_i64le(out, lv.quantity);
              write_u32le(out, lv.order_count);
            }
          };
          write_levels(p.bids);
          write_levels(p.asks);
        } else if constexpr (std::is_same_v<T, BookDeltaEvent>) {
          write_u32le(out, p.symbol);
          write_u8(out, static_cast<std::uint8_t>(p.side));
          write_u8(out, static_cast<std::uint8_t>(p.op));
          write_u8(out, 0);
          write_u8(out, 0);
          write_i64le(out, p.price);
          write_i64le(out, p.quantity);
          write_u32le(out, p.order_count);
        } else if constexpr (std::is_same_v<T, ErrorEvent>) {
          write_u16le(out, static_cast<std::uint16_t>(p.reason));
          write_u16le(out, 0);
          write_u32le(out, p.ref_seq);
        }
      },
      payload);
  return out;
}

DecodeStatus decode_command_payload(wire::Type type, std::span<const std::uint8_t> payload,
                                    CommandPayload& out) {
  try {
    ByteReader rd(payload);
    switch (type) {
      case wire::Type::Heartbeat:
        rd.expect_end();
        out = HeartbeatCommand{};
        return DecodeStatus::Ok;
      case wire::Type::Logout:
        rd.expect_end();
        out = LogoutCommand{};
        return DecodeStatus::Ok;
      case wire::Type::Login: {
        LoginCommand c;
        c.participant = rd.u64le();
        rd.expect_end();
        out = c;
        return DecodeStatus::Ok;
      }
      case wire::Type::Subscribe: {
        SubscribeCommand c;
        c.symbol = rd.u32le();
        c.book = rd.u8() != 0;
        c.trades = rd.u8() != 0;
        (void)rd.u8();
        (void)rd.u8();
        rd.expect_end();
        out = c;
        return DecodeStatus::Ok;
      }
      case wire::Type::NewOrder: {
        NewOrderCommand c;
        c.client_order_id = rd.u64le();
        c.symbol = rd.u32le();
        c.side = static_cast<Side>(rd.u8());
        c.type = static_cast<OrderType>(rd.u8());
        c.tif = static_cast<TimeInForce>(rd.u8());
        (void)rd.u8();
        c.price = rd.i64le();
        c.quantity = rd.i64le();
        rd.expect_end();
        out = c;
        return DecodeStatus::Ok;
      }
      case wire::Type::CancelOrder: {
        CancelOrderCommand c;
        c.exchange_order_id = rd.u64le();
        c.client_order_id = rd.u64le();
        rd.expect_end();
        out = c;
        return DecodeStatus::Ok;
      }
      case wire::Type::ModifyOrder: {
        ModifyOrderCommand c;
        c.exchange_order_id = rd.u64le();
        c.client_order_id = rd.u64le();
        c.new_price = rd.i64le();
        c.new_quantity = rd.i64le();
        rd.expect_end();
        out = c;
        return DecodeStatus::Ok;
      }
      default:
        return DecodeStatus::UnknownType;
    }
  } catch (const std::runtime_error&) {
    return DecodeStatus::BadPayload;
  }
}

DecodeStatus decode_event_payload(wire::Type type, std::span<const std::uint8_t> payload,
                                  EventPayload& out) {
  try {
    ByteReader rd(payload);
    auto read_levels = [&](std::uint16_t n) {
      std::vector<BookLevel> levels;
      levels.reserve(n);
      for (std::uint16_t i = 0; i < n; ++i) {
        BookLevel lv;
        lv.price = rd.i64le();
        lv.quantity = rd.i64le();
        lv.order_count = rd.u32le();
        levels.push_back(lv);
      }
      return levels;
    };
    switch (type) {
      case wire::Type::HeartbeatAck:
        rd.expect_end();
        out = HeartbeatAckEvent{};
        return DecodeStatus::Ok;
      case wire::Type::LoginAck: {
        LoginAckEvent e;
        e.participant = rd.u64le();
        rd.expect_end();
        out = e;
        return DecodeStatus::Ok;
      }
      case wire::Type::SessionEnd: {
        SessionEndEvent e;
        e.reason = static_cast<RejectReason>(rd.u16le());
        (void)rd.u16le();
        rd.expect_end();
        out = e;
        return DecodeStatus::Ok;
      }
      case wire::Type::OrderAccepted: {
        OrderAcceptedEvent e;
        e.exchange_order_id = rd.u64le();
        e.client_order_id = rd.u64le();
        e.symbol = rd.u32le();
        e.side = static_cast<Side>(rd.u8());
        e.type = static_cast<OrderType>(rd.u8());
        e.tif = static_cast<TimeInForce>(rd.u8());
        (void)rd.u8();
        e.price = rd.i64le();
        e.quantity = rd.i64le();
        rd.expect_end();
        out = e;
        return DecodeStatus::Ok;
      }
      case wire::Type::OrderRejected: {
        OrderRejectedEvent e;
        e.client_order_id = rd.u64le();
        e.exchange_order_id = rd.u64le();
        e.reason = static_cast<RejectReason>(rd.u16le());
        (void)rd.u16le();
        rd.expect_end();
        out = e;
        return DecodeStatus::Ok;
      }
      case wire::Type::OrderCanceled: {
        OrderCanceledEvent e;
        e.exchange_order_id = rd.u64le();
        e.client_order_id = rd.u64le();
        e.canceled = rd.i64le();
        e.reason = static_cast<CancelReason>(rd.u8());
        (void)rd.u8();
        (void)rd.u8();
        (void)rd.u8();
        rd.expect_end();
        out = e;
        return DecodeStatus::Ok;
      }
      case wire::Type::OrderReplaced: {
        OrderReplacedEvent e;
        e.exchange_order_id = rd.u64le();
        e.client_order_id = rd.u64le();
        e.price = rd.i64le();
        e.quantity = rd.i64le();
        e.priority_lost = rd.u8() != 0;
        (void)rd.u8();
        (void)rd.u8();
        (void)rd.u8();
        rd.expect_end();
        out = e;
        return DecodeStatus::Ok;
      }
      case wire::Type::PrivateFill: {
        PrivateFillEvent e;
        e.trade_id = rd.u64le();
        e.exchange_order_id = rd.u64le();
        e.client_order_id = rd.u64le();
        e.symbol = rd.u32le();
        e.side = static_cast<Side>(rd.u8());
        e.liquidity = static_cast<Liquidity>(rd.u8());
        (void)rd.u8();
        (void)rd.u8();
        e.price = rd.i64le();
        e.quantity = rd.i64le();
        e.remaining = rd.i64le();
        rd.expect_end();
        out = e;
        return DecodeStatus::Ok;
      }
      case wire::Type::PublicTrade: {
        PublicTradeEvent e;
        e.trade_id = rd.u64le();
        e.symbol = rd.u32le();
        e.aggressor = static_cast<Side>(rd.u8());
        (void)rd.u8();
        (void)rd.u8();
        (void)rd.u8();
        e.price = rd.i64le();
        e.quantity = rd.i64le();
        rd.expect_end();
        out = e;
        return DecodeStatus::Ok;
      }
      case wire::Type::BookSnapshot: {
        BookSnapshotEvent e;
        e.symbol = rd.u32le();
        const auto nbid = rd.u16le();
        const auto nask = rd.u16le();
        e.bids = read_levels(nbid);
        e.asks = read_levels(nask);
        rd.expect_end();
        out = e;
        return DecodeStatus::Ok;
      }
      case wire::Type::BookDelta: {
        BookDeltaEvent e;
        e.symbol = rd.u32le();
        e.side = static_cast<Side>(rd.u8());
        e.op = static_cast<BookDeltaOp>(rd.u8());
        (void)rd.u8();
        (void)rd.u8();
        e.price = rd.i64le();
        e.quantity = rd.i64le();
        e.order_count = rd.u32le();
        rd.expect_end();
        out = e;
        return DecodeStatus::Ok;
      }
      case wire::Type::Error: {
        ErrorEvent e;
        e.reason = static_cast<RejectReason>(rd.u16le());
        (void)rd.u16le();
        e.ref_seq = rd.u32le();
        rd.expect_end();
        out = e;
        return DecodeStatus::Ok;
      }
      default:
        return DecodeStatus::UnknownType;
    }
  } catch (const std::runtime_error&) {
    return DecodeStatus::BadPayload;
  }
}

}  // namespace helix
