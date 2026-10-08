#pragma once

#include "helix/event.hpp"
#include "helix/order.hpp"
#include "helix/price_level.hpp"

#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace helix {

struct ClientKey {
  ParticipantId participant{kInvalidParticipant};
  ClientOrderId client_id{0};

  bool operator==(const ClientKey& o) const noexcept {
    return participant == o.participant && client_id == o.client_id;
  }
};

struct ClientKeyHash {
  std::size_t operator()(const ClientKey& k) const noexcept {
    std::size_t h = std::hash<ParticipantId>{}(k.participant);
    const std::size_t h2 = std::hash<ClientOrderId>{}(k.client_id);
    h ^= h2 + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    return h;
  }
};

class InvariantError : public std::runtime_error {
 public:
  explicit InvariantError(const std::string& m) : std::runtime_error(m) {}
};

// Per-symbol resting book. Not thread-safe. The matching thread is the
// sole writer. Complexity: add/cancel O(log P) with O(1) FIFO ops at the
// level; P is the number of distinct prices on that side.
class OrderBook {
 public:
  explicit OrderBook(SymbolId symbol);

  [[nodiscard]] SymbolId symbol() const noexcept { return symbol_; }
  [[nodiscard]] std::size_t order_count() const noexcept { return by_id_.size(); }
  [[nodiscard]] bool empty() const noexcept { return by_id_.empty(); }

  // Rest an already-accepted limit remainder. Does not match.
  void rest(RestingOrder order);

  std::optional<RestingOrder> cancel(OrderId id);
  std::optional<RestingOrder> cancel_by_client(ParticipantId owner, ClientOrderId client_id);

  // Subtract `by` from remaining. Removes the order if remaining hits 0.
  // Returns the order snapshot after the reduce (remaining may be 0).
  std::optional<RestingOrder> reduce(OrderId id, Quantity by);

  [[nodiscard]] RestingOrder* best(Side side);
  [[nodiscard]] const RestingOrder* best(Side side) const;
  [[nodiscard]] PriceTicks best_price(Side side) const;  // 0 if none

  [[nodiscard]] const RestingOrder* find(OrderId id) const;
  [[nodiscard]] RestingOrder* find(OrderId id);
  [[nodiscard]] const RestingOrder* find_by_client(ParticipantId owner,
                                                   ClientOrderId client_id) const;
  [[nodiscard]] RestingOrder* find_by_client(ParticipantId owner, ClientOrderId client_id);

  [[nodiscard]] std::vector<BookLevel> depth(Side side, std::size_t levels) const;

  // Visible opposing quantity at prices that would trade with `aggressor`
  // at `limit` (limit 0 = market). Same-participant orders are skipped
  // when `stp_skip` is non-zero (STP-CNI does not consume self as liquidity).
  [[nodiscard]] Quantity available(Side aggressor, PriceTicks limit,
                                   ParticipantId stp_skip) const;

  [[nodiscard]] Quantity level_total(Side side, PriceTicks price) const;
  [[nodiscard]] std::uint32_t level_count(Side side, PriceTicks price) const;

  // Index, FIFO, and aggregate consistency. Does not require an uncrossed book;
  // matching is responsible for never resting a crossing remainder.
  void check_invariants() const;
  void check_uncrossed() const;

 private:
  struct IndexEntry {
    Side side{Side::Buy};
    PriceTicks price{0};
    PriceLevel::Iterator it{};
  };

  PriceLevel* level(Side side, PriceTicks price);
  const PriceLevel* level(Side side, PriceTicks price) const;
  void erase_index(const RestingOrder& order);
  void drop_level_if_empty(Side side, PriceTicks price);

  SymbolId symbol_;
  std::map<PriceTicks, PriceLevel, std::greater<PriceTicks>> bids_;
  std::map<PriceTicks, PriceLevel, std::less<PriceTicks>> asks_;
  std::unordered_map<OrderId, IndexEntry> by_id_;
  std::unordered_map<ClientKey, OrderId, ClientKeyHash> by_client_;
};

}  // namespace helix
