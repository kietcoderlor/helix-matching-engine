#include "helix/codec.hpp"
#include "helix/endian.hpp"
#include "test_harness.hpp"

#include <array>
#include <cstring>
#include <vector>

using namespace helix;

static std::vector<std::uint8_t> frame_of(wire::Type type, Sequence seq,
                                          const CommandPayload& p) {
  const auto payload = encode_command_payload(p);
  return encode_frame(type, seq, payload);
}

HELIX_TEST(endian_round_trip_integers) {
  std::vector<std::uint8_t> buf;
  write_u16le(buf, 0xBEEF);
  write_u32le(buf, 0xA1B2C3D4u);
  write_i64le(buf, -42);
  ByteReader rd(buf);
  HELIX_CHECK_EQ(rd.u16le(), 0xBEEF);
  HELIX_CHECK_EQ(rd.u32le(), 0xA1B2C3D4u);
  HELIX_CHECK_EQ(rd.i64le(), static_cast<std::int64_t>(-42));
  HELIX_CHECK_EQ(rd.remaining(), 0u);
}

HELIX_TEST(frame_need_more_on_partial_header_and_body) {
  const auto full = encode_frame(wire::Type::Heartbeat, 7, {});
  HELIX_CHECK_EQ(full.size(), wire::kHeaderSize);

  auto r = try_decode_frame(std::span<const std::uint8_t>(full.data(), 4));
  HELIX_CHECK_EQ(r.status, DecodeStatus::NeedMore);
  HELIX_CHECK_EQ(r.consumed, 0u);

  NewOrderCommand n;
  n.client_order_id = 99;
  n.symbol = 1;
  n.quantity = 10;
  n.price = 100;
  const auto order_frame = frame_of(wire::Type::NewOrder, 1, n);
  HELIX_CHECK(order_frame.size() > wire::kHeaderSize);
  r = try_decode_frame(
      std::span<const std::uint8_t>(order_frame.data(), wire::kHeaderSize + 3));
  HELIX_CHECK_EQ(r.status, DecodeStatus::NeedMore);
}

HELIX_TEST(frame_rejects_bad_magic_version_and_oversize) {
  auto bad_magic = encode_frame(wire::Type::Heartbeat, 1, {});
  bad_magic[0] = 0x00;
  auto r = try_decode_frame(bad_magic);
  HELIX_CHECK_EQ(r.status, DecodeStatus::InvalidMagic);

  auto bad_ver = encode_frame(wire::Type::Heartbeat, 1, {});
  bad_ver[4] = 99;  // version low byte
  r = try_decode_frame(bad_ver);
  HELIX_CHECK_EQ(r.status, DecodeStatus::UnsupportedVersion);

  std::vector<std::uint8_t> oversized;
  write_u32le(oversized, wire::kMagicLE);
  write_u16le(oversized, wire::kVersion);
  write_u16le(oversized, static_cast<std::uint16_t>(wire::Type::Heartbeat));
  write_u32le(oversized, wire::kMaxPayload + 1);
  write_u32le(oversized, 1);
  r = try_decode_frame(oversized);
  HELIX_CHECK_EQ(r.status, DecodeStatus::PayloadTooLarge);
}

HELIX_TEST(new_order_command_round_trip) {
  NewOrderCommand n;
  n.client_order_id = 0x1122334455667788ull;
  n.symbol = 42;
  n.side = Side::Sell;
  n.type = OrderType::Limit;
  n.tif = TimeInForce::Ioc;
  n.price = 12345;
  n.quantity = 7;
  const auto bytes = frame_of(wire::Type::NewOrder, 99, n);
  const auto decoded = try_decode_frame(bytes);
  HELIX_CHECK_EQ(decoded.status, DecodeStatus::Ok);
  HELIX_CHECK_EQ(decoded.consumed, bytes.size());
  HELIX_CHECK_EQ(decoded.frame->header.seq, 99u);
  CommandPayload payload;
  HELIX_CHECK_EQ(decode_command_payload(decoded.frame->header.type, decoded.frame->payload, payload),
                 DecodeStatus::Ok);
  const auto* got = std::get_if<NewOrderCommand>(&payload);
  HELIX_CHECK(got != nullptr);
  HELIX_CHECK_EQ(got->client_order_id, n.client_order_id);
  HELIX_CHECK_EQ(got->symbol, n.symbol);
  HELIX_CHECK_EQ(got->side, n.side);
  HELIX_CHECK_EQ(got->tif, n.tif);
  HELIX_CHECK_EQ(got->price, n.price);
  HELIX_CHECK_EQ(got->quantity, n.quantity);
}

HELIX_TEST(event_private_fill_round_trip) {
  PrivateFillEvent fill;
  fill.trade_id = 5;
  fill.exchange_order_id = 9;
  fill.client_order_id = 3;
  fill.symbol = 1;
  fill.side = Side::Buy;
  fill.price = 100;
  fill.quantity = 2;
  fill.remaining = 8;
  fill.liquidity = Liquidity::Maker;
  EventPayload ep = fill;
  const auto payload = encode_event_payload(ep);
  const auto bytes = encode_frame(wire::Type::PrivateFill, 2, payload);
  const auto decoded = try_decode_frame(bytes);
  HELIX_CHECK_EQ(decoded.status, DecodeStatus::Ok);
  EventPayload out;
  HELIX_CHECK_EQ(decode_event_payload(wire::Type::PrivateFill, decoded.frame->payload, out),
                 DecodeStatus::Ok);
  const auto* got = std::get_if<PrivateFillEvent>(&out);
  HELIX_CHECK(got != nullptr);
  HELIX_CHECK_EQ(got->trade_id, fill.trade_id);
  HELIX_CHECK_EQ(got->remaining, fill.remaining);
  HELIX_CHECK_EQ(got->liquidity, Liquidity::Maker);
}

HELIX_TEST(truncated_new_order_payload_is_bad) {
  std::array<std::uint8_t, 4> tiny{1, 2, 3, 4};
  CommandPayload payload;
  HELIX_CHECK_EQ(decode_command_payload(wire::Type::NewOrder, tiny, payload),
                 DecodeStatus::BadPayload);
}

HELIX_TEST(book_snapshot_round_trip) {
  BookSnapshotEvent snap;
  snap.symbol = 7;
  snap.bids.push_back(BookLevel{100, 10, 2});
  snap.asks.push_back(BookLevel{101, 4, 1});
  EventPayload ep = snap;
  const auto payload = encode_event_payload(ep);
  EventPayload out;
  HELIX_CHECK_EQ(decode_event_payload(wire::Type::BookSnapshot, payload, out), DecodeStatus::Ok);
  const auto* got = std::get_if<BookSnapshotEvent>(&out);
  HELIX_CHECK(got != nullptr);
  HELIX_CHECK_EQ(got->bids.size(), 1u);
  HELIX_CHECK_EQ(got->asks[0].price, 101);
  HELIX_CHECK_EQ(got->asks[0].order_count, 1u);
}

HELIX_TEST(magic_bytes_are_HXCH) {
  const auto bytes = encode_frame(wire::Type::Heartbeat, 0, {});
  HELIX_CHECK_EQ(bytes[0], static_cast<std::uint8_t>('H'));
  HELIX_CHECK_EQ(bytes[1], static_cast<std::uint8_t>('X'));
  HELIX_CHECK_EQ(bytes[2], static_cast<std::uint8_t>('C'));
  HELIX_CHECK_EQ(bytes[3], static_cast<std::uint8_t>('H'));
}
