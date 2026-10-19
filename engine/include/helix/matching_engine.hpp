#pragma once

#include "helix/command.hpp"
#include "helix/event.hpp"
#include "helix/order_book.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace helix {

// Authoritative matcher. Not thread-safe: one thread owns every OrderBook
// in `books_`. Callers (the pipeline) sequence commands; this class never
// performs I/O.
class MatchingEngine {
 public:
  struct Config {
    bool check_invariants{true};
    std::size_t snapshot_depth{10};
  };

  MatchingEngine();
  explicit MatchingEngine(Config config);

  void add_symbol(SymbolId id, std::string name);

  [[nodiscard]] bool has_symbol(SymbolId id) const;
  [[nodiscard]] const OrderBook* book(SymbolId id) const;
  [[nodiscard]] OrderId peek_next_order_id() const noexcept { return next_order_id_; }
  [[nodiscard]] TradeId peek_next_trade_id() const noexcept { return next_trade_id_; }

  std::vector<Event> execute(const Command& command);

 private:
  std::vector<Event> on_new(const Command& cmd, const NewOrderCommand& n);
  std::vector<Event> on_cancel(const Command& cmd, const CancelOrderCommand& c);
  std::vector<Event> on_modify(const Command& cmd, const ModifyOrderCommand& m);
  std::vector<Event> on_subscribe(const Command& cmd, const SubscribeCommand& s);

  void match(const Command& cmd, OrderBook& book, RestingOrder& aggressor,
             std::vector<Event>& out);
  void rest_or_cancel_remainder(const Command& cmd, OrderBook& book, RestingOrder& aggressor,
                                std::vector<Event>& out);
  void emit_fill(const Command& cmd, const RestingOrder& maker, const RestingOrder& taker,
                 Quantity qty, std::vector<Event>& out);
  void emit_book_level(const OrderBook& book, Side side, PriceTicks price,
                       std::vector<Event>& out);
  Event base_event(const Command& cmd, EventScope scope, SymbolId symbol) const;
  Event reject(const Command& cmd, ClientOrderId cid, OrderId eid, RejectReason reason) const;
  void maybe_check(const OrderBook& book) const;

  TimestampNs stamp(const Command& cmd);

  Config config_{};
  TimestampNs clock_{0};
  TimestampNs last_ts_{0};
  OrderId next_order_id_{1};
  TradeId next_trade_id_{1};
  std::unordered_map<SymbolId, OrderBook> books_;
  std::unordered_map<SymbolId, std::string> names_;
};

}  // namespace helix
