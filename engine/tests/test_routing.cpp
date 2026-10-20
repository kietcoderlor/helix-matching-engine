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

struct RecvBuf {
  UniqueSocket* sock{};
  std::vector<std::uint8_t> acc;
  std::vector<wire::Type> types;
};

static void drain(RecvBuf& r, int spins) {
  for (int i = 0; i < spins; ++i) {
    std::uint8_t tmp[512];
    const int n = r.sock->recv_some(tmp);
    if (n > 0) {
      r.acc.insert(r.acc.end(), tmp, tmp + n);
      while (true) {
        const auto d = try_decode_frame(r.acc);
        if (d.status == DecodeStatus::NeedMore) {
          break;
        }
        if (d.status != DecodeStatus::Ok) {
          return;
        }
        r.types.push_back(d.frame->header.type);
        r.acc.erase(r.acc.begin(), r.acc.begin() + static_cast<std::ptrdiff_t>(d.consumed));
      }
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds{2});
    }
  }
}

static bool has_type(const RecvBuf& r, wire::Type t) {
  for (auto x : r.types) {
    if (x == t) {
      return true;
    }
  }
  return false;
}

static bool wait_type(RecvBuf& r, wire::Type want, int spins) {
  if (has_type(r, want)) {
    return true;
  }
  drain(r, spins);
  return has_type(r, want);
}

static void login(UniqueSocket& sock, ParticipantId p, Sequence seq) {
  LoginCommand c;
  c.participant = p;
  send_all(sock, encode_frame(wire::Type::Login, seq, encode_command_payload(c)));
}

HELIX_TEST(two_symbols_do_not_share_books) {
  EngineService::Config cfg;
  EngineService svc(cfg);
  svc.add_symbol(1, "AAPL");
  svc.add_symbol(2, "MSFT");
  svc.start();
  UniqueSocket c = tcp_connect("127.0.0.1", svc.port());
  RecvBuf rc{&c, {}, {}};
  login(c, 1, 1);
  HELIX_CHECK(wait_type(rc, wire::Type::LoginAck, 100));

  NewOrderCommand n;
  n.client_order_id = 1;
  n.symbol = 1;
  n.side = Side::Buy;
  n.type = OrderType::Limit;
  n.tif = TimeInForce::Gtc;
  n.price = 10;
  n.quantity = 1;
  send_all(c, encode_frame(wire::Type::NewOrder, 2, encode_command_payload(n)));
  HELIX_CHECK(wait_type(rc, wire::Type::OrderAccepted, 100));

  n.client_order_id = 2;
  n.symbol = 2;
  n.side = Side::Sell;
  n.price = 50;
  send_all(c, encode_frame(wire::Type::NewOrder, 3, encode_command_payload(n)));
  HELIX_CHECK(wait_type(rc, wire::Type::OrderAccepted, 100));
  svc.stop();
}

HELIX_TEST(public_trades_only_go_to_subscribers) {
  EngineService::Config cfg;
  EngineService svc(cfg);
  svc.add_symbol(1, "AAPL");
  svc.start();

  UniqueSocket maker = tcp_connect("127.0.0.1", svc.port());
  UniqueSocket taker = tcp_connect("127.0.0.1", svc.port());
  UniqueSocket watcher = tcp_connect("127.0.0.1", svc.port());
  RecvBuf rm{&maker, {}, {}};
  RecvBuf rt{&taker, {}, {}};
  RecvBuf rw{&watcher, {}, {}};
  login(maker, 1, 1);
  login(taker, 2, 1);
  login(watcher, 3, 1);
  HELIX_CHECK(wait_type(rm, wire::Type::LoginAck, 100));
  HELIX_CHECK(wait_type(rt, wire::Type::LoginAck, 100));
  HELIX_CHECK(wait_type(rw, wire::Type::LoginAck, 100));

  SubscribeCommand sub;
  sub.symbol = 1;
  sub.book = true;
  sub.trades = true;
  send_all(taker, encode_frame(wire::Type::Subscribe, 2, encode_command_payload(sub)));
  send_all(maker, encode_frame(wire::Type::Subscribe, 2, encode_command_payload(sub)));

  NewOrderCommand sell;
  sell.client_order_id = 1;
  sell.symbol = 1;
  sell.side = Side::Sell;
  sell.type = OrderType::Limit;
  sell.tif = TimeInForce::Gtc;
  sell.price = 25;
  sell.quantity = 2;
  send_all(maker, encode_frame(wire::Type::NewOrder, 3, encode_command_payload(sell)));
  HELIX_CHECK(wait_type(rm, wire::Type::OrderAccepted, 150));

  NewOrderCommand buy = sell;
  buy.client_order_id = 1;
  buy.side = Side::Buy;
  send_all(taker, encode_frame(wire::Type::NewOrder, 3, encode_command_payload(buy)));
  HELIX_CHECK(wait_type(rt, wire::Type::PrivateFill, 150));
  HELIX_CHECK(wait_type(rt, wire::Type::PublicTrade, 50));
  HELIX_CHECK(wait_type(rm, wire::Type::PrivateFill, 150));

  drain(rw, 20);
  HELIX_CHECK(!has_type(rw, wire::Type::PublicTrade));
  svc.stop();
}
