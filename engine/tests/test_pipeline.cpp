#include "helix/codec.hpp"
#include "helix/endian.hpp"
#include "helix/engine_service.hpp"
#include "helix/net/socket.hpp"
#include "test_harness.hpp"

#include <chrono>
#include <thread>
#include <vector>

using namespace helix;
using namespace helix::net;

static void send_all(UniqueSocket& sock, const std::vector<std::uint8_t>& bytes) {
  std::size_t off = 0;
  while (off < bytes.size()) {
    const int n = sock.send_some(std::span<const std::uint8_t>(bytes.data() + off, bytes.size() - off));
    if (n < 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds{1});
      continue;
    }
    HELIX_CHECK(n > 0);
    off += static_cast<std::size_t>(n);
  }
}

static std::vector<std::uint8_t> recv_frame(UniqueSocket& sock, int spins) {
  std::vector<std::uint8_t> buf;
  buf.resize(16);
  std::size_t have = 0;
  auto wait_more = [&](std::size_t need) {
    if (buf.size() < need) {
      buf.resize(need);
    }
    for (int i = 0; i < spins && have < need; ++i) {
      const int n = sock.recv_some(std::span<std::uint8_t>(buf.data() + have, need - have));
      if (n > 0) {
        have += static_cast<std::size_t>(n);
      } else {
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
      }
    }
  };
  wait_more(16);
  if (have < 16) {
    buf.resize(have);
    return buf;
  }
  ByteReader rd(std::span<const std::uint8_t>(buf.data(), 16));
  (void)rd.u32le();
  (void)rd.u16le();
  (void)rd.u16le();
  const auto plen = rd.u32le();
  wait_more(16 + plen);
  buf.resize(have);
  return buf;
}

HELIX_TEST(pipeline_login_and_new_order_round_trip) {
  EngineService::Config cfg;
  cfg.host = "127.0.0.1";
  cfg.port = 0;
  EngineService svc(cfg);
  svc.add_symbol(1, "AAPL");
  svc.start();

  UniqueSocket c = tcp_connect("127.0.0.1", svc.port());
  LoginCommand login;
  login.participant = 42;
  send_all(c, encode_frame(wire::Type::Login, 1, encode_command_payload(login)));
  const auto login_bytes = recv_frame(c, 200);
  const auto login_dec = try_decode_frame(login_bytes);
  HELIX_CHECK_EQ(login_dec.status, DecodeStatus::Ok);
  HELIX_CHECK_EQ(login_dec.frame->header.type, wire::Type::LoginAck);

  NewOrderCommand n;
  n.client_order_id = 7;
  n.symbol = 1;
  n.side = Side::Buy;
  n.type = OrderType::Limit;
  n.tif = TimeInForce::Gtc;
  n.price = 100;
  n.quantity = 5;
  send_all(c, encode_frame(wire::Type::NewOrder, 2, encode_command_payload(n)));
  const auto acc_bytes = recv_frame(c, 200);
  const auto acc_dec = try_decode_frame(acc_bytes);
  HELIX_CHECK_EQ(acc_dec.status, DecodeStatus::Ok);
  HELIX_CHECK_EQ(acc_dec.frame->header.type, wire::Type::OrderAccepted);

  svc.stop();
}
