#include "helix/net/poller.hpp"
#include "helix/net/socket.hpp"
#include "test_harness.hpp"

#include <array>
#include <chrono>
#include <cstring>
#include <string>
#include <utility>

using namespace helix::net;

HELIX_TEST(socket_raii_listen_connect_accept_and_poll_read) {
  ensure_initialized();
  UniqueSocket listener = tcp_listen("127.0.0.1", 0);
  const auto port = local_port(listener);
  HELIX_CHECK(port != 0);

  UniqueSocket client = tcp_connect("127.0.0.1", port);
  UniqueSocket server;
  for (int i = 0; i < 50 && !server.valid(); ++i) {
    server = tcp_accept(listener);
  }
  HELIX_CHECK(server.valid());

  const char msg[] = "ping";
  const auto sent = client.send_some(
      std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(msg), 4));
  HELIX_CHECK_EQ(sent, 4);

  auto poller = create_poller();
  HELIX_CHECK(std::string(poller->name()).size() > 0);
  poller->add(server.get(), Interest::Read);

  std::array<Ready, 8> ready{};
  const int n = poller->wait(ready, std::chrono::milliseconds{500});
  HELIX_CHECK(n >= 1);
  bool saw = false;
  for (int i = 0; i < n; ++i) {
    if (ready[static_cast<std::size_t>(i)].fd == server.get() &&
        ready[static_cast<std::size_t>(i)].readable) {
      saw = true;
    }
  }
  HELIX_CHECK(saw);

  std::array<std::uint8_t, 8> buf{};
  const int recvd = server.recv_some(buf);
  HELIX_CHECK_EQ(recvd, 4);
  HELIX_CHECK(std::memcmp(buf.data(), "ping", 4) == 0);

  poller->del(server.get());
}

HELIX_TEST(unique_socket_move_clears_source) {
  ensure_initialized();
  UniqueSocket listener = tcp_listen("127.0.0.1", 0);
  const NativeHandle raw = listener.get();
  UniqueSocket moved = std::move(listener);
  HELIX_CHECK(!listener.valid());
  HELIX_CHECK_EQ(moved.get(), raw);
}
