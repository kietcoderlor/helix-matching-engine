#pragma once

#include "helix/matching_engine.hpp"
#include "helix/net/server.hpp"
#include "helix/spsc_queue.hpp"

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>

namespace helix {

struct Session {
  ConnectionId connection{0};
  ParticipantId participant{kInvalidParticipant};
  std::unordered_set<SymbolId> book_subs;
  std::unordered_set<SymbolId> trade_subs;
};

// Owns the I/O poller thread and the matching thread. Socket syscalls never
// run in execute(). Commands that would overflow the SPSC queue are retried
// (TCP backpressure) rather than dropped.
class EngineService {
 public:
  struct Config {
    std::string host{"127.0.0.1"};
    std::uint16_t port{0};
    std::size_t command_queue{4096};
    std::size_t event_queue{16384};
  };

  explicit EngineService(Config config);
  ~EngineService();

  EngineService(const EngineService&) = delete;
  EngineService& operator=(const EngineService&) = delete;

  void add_symbol(SymbolId id, std::string name);
  void start();
  void stop();

  [[nodiscard]] std::uint16_t port() const;
  [[nodiscard]] std::size_t connection_count() const;

 private:
  void on_frame(net::Connection& conn, const DecodedFrame& frame);
  void match_loop();
  void io_loop();
  void drain_events();
  void route_event(const Event& event);
  bool push_command(Command cmd);

  Config config_;
  MatchingEngine engine_;
  SpscQueue<Command> commands_;
  SpscQueue<Event> events_;
  net::TcpServer server_;
  std::atomic<bool> running_{false};
  std::thread match_thread_;
  std::thread io_thread_;
  std::unordered_map<ConnectionId, Session> sessions_;
  std::unordered_map<ParticipantId, ConnectionId> by_participant_;
};

}  // namespace helix
