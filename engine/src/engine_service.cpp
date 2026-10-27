#include "helix/engine_service.hpp"

#include "helix/codec.hpp"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <utility>
#include <variant>

namespace helix {
namespace {

void yield_pause() { std::this_thread::yield(); }

}  // namespace

EngineService::EngineService(Config config)
    : config_(std::move(config)),
      commands_(config_.command_queue),
      events_(config_.event_queue),
      server_(config_.host.c_str(), config_.port,
              [this](net::Connection& c, const DecodedFrame& f) { on_frame(c, f); }) {}

EngineService::~EngineService() { stop(); }

void EngineService::add_symbol(SymbolId id, std::string name) {
  engine_.add_symbol(id, std::move(name));
}

void EngineService::start() {
  if (running_.exchange(true)) {
    return;
  }
  match_thread_ = std::thread([this] { match_loop(); });
  io_thread_ = std::thread([this] { io_loop(); });
}

void EngineService::stop() {
  if (!running_.exchange(false)) {
    if (match_thread_.joinable()) {
      match_thread_.join();
    }
    if (io_thread_.joinable()) {
      io_thread_.join();
    }
    return;
  }
  if (match_thread_.joinable()) {
    match_thread_.join();
  }
  if (io_thread_.joinable()) {
    io_thread_.join();
  }
}

std::uint16_t EngineService::port() const { return server_.port(); }

std::size_t EngineService::connection_count() const { return server_.connection_count(); }

void EngineService::io_loop() {
  while (running_.load(std::memory_order_relaxed)) {
    server_.poll_once(std::chrono::milliseconds{5});
    drain_events();
  }
  drain_events();
}

void EngineService::match_loop() {
  Command cmd;
  while (running_.load(std::memory_order_relaxed)) {
    if (!commands_.try_pop(cmd)) {
      yield_pause();
      continue;
    }
    auto evs = engine_.execute(cmd);
    for (auto& e : evs) {
      while (running_.load(std::memory_order_relaxed) && !events_.try_push(std::move(e))) {
        yield_pause();
      }
    }
  }
  // Drain remaining commands so stop() does not drop in-flight work silently.
  while (commands_.try_pop(cmd)) {
    auto evs = engine_.execute(cmd);
    for (auto& e : evs) {
      while (!events_.try_push(std::move(e))) {
        yield_pause();
      }
    }
  }
}

bool EngineService::push_command(Command cmd) {
  while (!commands_.try_push(std::move(cmd))) {
    if (!running_.load(std::memory_order_relaxed)) {
      return false;
    }
    yield_pause();
  }
  return true;
}

void EngineService::on_frame(net::Connection& conn, const DecodedFrame& frame) {
  sessions_.try_emplace(conn.id(), Session{conn.id(), conn.participant(), {}, {}});
  Session& session = sessions_[conn.id()];

  if (frame.header.type == wire::Type::Heartbeat) {
    conn.enqueue(wire::Type::HeartbeatAck, frame.header.seq, {});
    return;
  }
  if (frame.header.type == wire::Type::Login) {
    CommandPayload payload;
    if (decode_command_payload(frame.header.type, frame.payload, payload) != DecodeStatus::Ok) {
      conn.enqueue(wire::Type::Error, frame.header.seq, {});
      return;
    }
    const auto login = std::get<LoginCommand>(payload);
    session.participant = login.participant;
    conn.set_participant(login.participant);
    by_participant_[login.participant] = conn.id();
    std::clog << "login conn=" << conn.id() << " participant=" << login.participant << '\n';
    Event ack;
    ack.scope = EventScope::Connection;
    ack.connection = conn.id();
    ack.participant = login.participant;
    ack.seq = frame.header.seq;
    ack.payload = LoginAckEvent{login.participant};
    conn.enqueue_event(ack);
    return;
  }
  if (frame.header.type == wire::Type::Subscribe) {
    CommandPayload payload;
    if (decode_command_payload(frame.header.type, frame.payload, payload) != DecodeStatus::Ok) {
      return;
    }
    const auto sub = std::get<SubscribeCommand>(payload);
    if (sub.book) {
      session.book_subs.insert(sub.symbol);
    }
    if (sub.trades) {
      session.trade_subs.insert(sub.symbol);
    }
    Command cmd;
    cmd.connection = conn.id();
    cmd.participant = session.participant;
    cmd.seq = frame.header.seq;
    cmd.payload = sub;
    push_command(std::move(cmd));
    return;
  }

  if (session.participant == kInvalidParticipant) {
    Event err;
    err.scope = EventScope::Connection;
    err.connection = conn.id();
    err.seq = frame.header.seq;
    err.payload = ErrorEvent{RejectReason::SessionNotLoggedIn, frame.header.seq};
    conn.enqueue_event(err);
    return;
  }

  Command cmd;
  cmd.connection = conn.id();
  cmd.participant = session.participant;
  cmd.seq = frame.header.seq;
  if (decode_command_payload(frame.header.type, frame.payload, cmd.payload) != DecodeStatus::Ok) {
    Event err;
    err.scope = EventScope::Connection;
    err.connection = conn.id();
    err.seq = frame.header.seq;
    err.payload = ErrorEvent{RejectReason::InvalidFrame, frame.header.seq};
    conn.enqueue_event(err);
    return;
  }
  push_command(std::move(cmd));
}

void EngineService::drain_events() {
  Event e;
  while (events_.try_pop(e)) {
    route_event(e);
  }
}

void EngineService::route_event(const Event& event) {
  auto send_to = [&](ConnectionId id) {
    net::Connection* conn = server_.connection(id);
    if (conn == nullptr) {
      return;
    }
    if (!conn->enqueue_event(event)) {
      server_.close(id, net::CloseReason::SlowConsumer);
      sessions_.erase(id);
      return;
    }
    server_.try_flush(id);
  };

  if (event.scope == EventScope::Connection || event.scope == EventScope::Private) {
    if (event.connection != 0) {
      send_to(event.connection);
      return;
    }
    const auto it = by_participant_.find(event.participant);
    if (it != by_participant_.end()) {
      send_to(it->second);
    }
    return;
  }

  for (auto& [id, session] : sessions_) {
    const bool book = std::holds_alternative<BookDeltaEvent>(event.payload) ||
                      std::holds_alternative<BookSnapshotEvent>(event.payload);
    const bool trade = std::holds_alternative<PublicTradeEvent>(event.payload);
    if (book && session.book_subs.contains(event.symbol)) {
      send_to(id);
    } else if (trade && session.trade_subs.contains(event.symbol)) {
      send_to(id);
    }
  }
}

}  // namespace helix
