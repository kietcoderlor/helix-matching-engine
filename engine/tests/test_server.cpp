#include "helix/codec.hpp"
#include "helix/net/server.hpp"
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
    HELIX_CHECK(n != 0);
    if (n < 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds{1});
      continue;
    }
    off += static_cast<std::size_t>(n);
  }
}

static std::vector<std::uint8_t> recv_some_wait(UniqueSocket& sock, std::size_t want, int spins) {
  std::vector<std::uint8_t> buf(want);
  std::size_t off = 0;
  for (int i = 0; i < spins && off < want; ++i) {
    const int n = sock.recv_some(std::span<std::uint8_t>(buf.data() + off, want - off));
    if (n > 0) {
      off += static_cast<std::size_t>(n);
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
  }
  buf.resize(off);
  return buf;
}

HELIX_TEST(server_heartbeats_two_clients) {
  TcpServer server("127.0.0.1", 0, [](Connection& conn, const DecodedFrame& frame) {
    if (frame.header.type == wire::Type::Heartbeat) {
      conn.enqueue(wire::Type::HeartbeatAck, frame.header.seq, {});
    }
  });

  UniqueSocket a = tcp_connect("127.0.0.1", server.port());
  UniqueSocket b = tcp_connect("127.0.0.1", server.port());
  for (int i = 0; i < 20 && server.connection_count() < 2; ++i) {
    server.poll_once(std::chrono::milliseconds{10});
  }
  HELIX_CHECK_EQ(server.connection_count(), 2u);

  send_all(a, encode_frame(wire::Type::Heartbeat, 1, {}));
  send_all(b, encode_frame(wire::Type::Heartbeat, 2, {}));
  for (int i = 0; i < 40; ++i) {
    server.poll_once(std::chrono::milliseconds{10});
  }

  const auto ra = recv_some_wait(a, wire::kHeaderSize, 50);
  const auto rb = recv_some_wait(b, wire::kHeaderSize, 50);
  const auto da = try_decode_frame(ra);
  const auto db = try_decode_frame(rb);
  HELIX_CHECK_EQ(da.status, DecodeStatus::Ok);
  HELIX_CHECK_EQ(db.status, DecodeStatus::Ok);
  HELIX_CHECK_EQ(da.frame->header.type, wire::Type::HeartbeatAck);
  HELIX_CHECK_EQ(db.frame->header.type, wire::Type::HeartbeatAck);
  HELIX_CHECK_EQ(da.frame->header.seq, 1u);
  HELIX_CHECK_EQ(db.frame->header.seq, 2u);
}

HELIX_TEST(server_closes_invalid_magic) {
  TcpServer server("127.0.0.1", 0, [](Connection&, const DecodedFrame&) {});
  UniqueSocket c = tcp_connect("127.0.0.1", server.port());
  for (int i = 0; i < 20 && server.connection_count() < 1; ++i) {
    server.poll_once(std::chrono::milliseconds{10});
  }
  HELIX_CHECK_EQ(server.connection_count(), 1u);
  std::vector<std::uint8_t> junk(16, 0xFF);
  send_all(c, junk);
  for (int i = 0; i < 20; ++i) {
    server.poll_once(std::chrono::milliseconds{10});
  }
  HELIX_CHECK_EQ(server.connection_count(), 0u);
}
