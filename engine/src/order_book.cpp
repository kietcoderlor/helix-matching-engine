#include "helix/order_book.hpp"

#include <sstream>

namespace helix {

OrderBook::OrderBook(SymbolId symbol) : symbol_(symbol) {}

void OrderBook::rest(RestingOrder order) {
  if (order.id == kInvalidOrderId) {
    throw InvariantError("rest() requires an exchange order id");
  }
  if (order.symbol != symbol_) {
    throw InvariantError("rest() symbol mismatch");
  }
  if (order.remaining <= 0) {
    throw InvariantError("rest() requires positive remaining qty");
  }
  if (by_id_.contains(order.id)) {
    throw InvariantError("rest() duplicate exchange order id");
  }
  const ClientKey ck{order.owner, order.client_id};
  if (order.client_id != 0 && by_client_.contains(ck)) {
    throw InvariantError("rest() duplicate client order id for participant");
  }

  const Side side = order.side;
  const PriceTicks price = order.price;
  PriceLevel::Iterator it;
  if (side == Side::Buy) {
    auto [lv, _] = bids_.try_emplace(price);
    it = lv->second.push_back(std::move(order));
  } else {
    auto [lv, _] = asks_.try_emplace(price);
    it = lv->second.push_back(std::move(order));
  }
  by_id_.emplace(it->id, IndexEntry{side, price, it});
  if (it->client_id != 0) {
    by_client_.emplace(ClientKey{it->owner, it->client_id}, it->id);
  }
}

std::optional<RestingOrder> OrderBook::cancel(OrderId id) {
  const auto found = by_id_.find(id);
  if (found == by_id_.end()) {
    return std::nullopt;
  }
  const IndexEntry idx = found->second;
  PriceLevel* lv = level(idx.side, idx.price);
  if (lv == nullptr) {
    throw InvariantError("cancel() missing price level");
  }
  RestingOrder copy = *idx.it;
  lv->erase(idx.it);
  erase_index(copy);
  drop_level_if_empty(idx.side, idx.price);
  return copy;
}

std::optional<RestingOrder> OrderBook::cancel_by_client(ParticipantId owner,
                                                        ClientOrderId client_id) {
  const auto it = by_client_.find(ClientKey{owner, client_id});
  if (it == by_client_.end()) {
    return std::nullopt;
  }
  return cancel(it->second);
}

std::optional<RestingOrder> OrderBook::reduce(OrderId id, Quantity by) {
  const auto found = by_id_.find(id);
  if (found == by_id_.end()) {
    return std::nullopt;
  }
  if (by <= 0) {
    throw InvariantError("reduce() requires positive qty");
  }
  IndexEntry idx = found->second;
  PriceLevel* lv = level(idx.side, idx.price);
  if (lv == nullptr) {
    throw InvariantError("reduce() missing price level");
  }
  if (by > idx.it->remaining) {
    throw InvariantError("reduce() exceeds remaining");
  }
  lv->reduce(idx.it, by);
  RestingOrder copy = *idx.it;
  if (idx.it->remaining == 0) {
    lv->erase(idx.it);
    erase_index(copy);
    drop_level_if_empty(idx.side, idx.price);
    copy.remaining = 0;
  }
  return copy;
}

RestingOrder* OrderBook::best(Side side) {
  if (side == Side::Buy) {
    return bids_.empty() ? nullptr : &bids_.begin()->second.front();
  }
  return asks_.empty() ? nullptr : &asks_.begin()->second.front();
}

const RestingOrder* OrderBook::best(Side side) const {
  if (side == Side::Buy) {
    return bids_.empty() ? nullptr : &bids_.begin()->second.front();
  }
  return asks_.empty() ? nullptr : &asks_.begin()->second.front();
}

PriceTicks OrderBook::best_price(Side side) const {
  if (side == Side::Buy) {
    return bids_.empty() ? 0 : bids_.begin()->first;
  }
  return asks_.empty() ? 0 : asks_.begin()->first;
}

const RestingOrder* OrderBook::find(OrderId id) const {
  const auto it = by_id_.find(id);
  return it == by_id_.end() ? nullptr : &(*it->second.it);
}

RestingOrder* OrderBook::find(OrderId id) {
  const auto it = by_id_.find(id);
  return it == by_id_.end() ? nullptr : &(*it->second.it);
}

const RestingOrder* OrderBook::find_by_client(ParticipantId owner,
                                              ClientOrderId client_id) const {
  const auto it = by_client_.find(ClientKey{owner, client_id});
  if (it == by_client_.end()) {
    return nullptr;
  }
  return find(it->second);
}

RestingOrder* OrderBook::find_by_client(ParticipantId owner, ClientOrderId client_id) {
  return const_cast<RestingOrder*>(
      static_cast<const OrderBook*>(this)->find_by_client(owner, client_id));
}

std::vector<BookLevel> OrderBook::depth(Side side, std::size_t levels) const {
  std::vector<BookLevel> out;
  auto fill = [&](const auto& m) {
    for (const auto& [px, lv] : m) {
      if (out.size() >= levels) {
        break;
      }
      out.push_back(BookLevel{px, lv.total(), static_cast<std::uint32_t>(lv.count())});
    }
  };
  if (side == Side::Buy) {
    fill(bids_);
  } else {
    fill(asks_);
  }
  return out;
}

Quantity OrderBook::available(Side aggressor, PriceTicks limit, ParticipantId stp_skip) const {
  Quantity sum = 0;
  const bool is_market = limit == 0;
  auto walk = [&](const auto& m, bool bid_side) {
    for (const auto& [px, lv] : m) {
      if (!is_market) {
        if (aggressor == Side::Buy && px > limit) {
          break;  // asks are ascending; stop past the buyer's limit
        }
        if (aggressor == Side::Sell && px < limit) {
          break;  // bids are descending; stop past the seller's limit
        }
      }
      (void)bid_side;
      for (const auto& o : lv.orders()) {
        if (stp_skip != kInvalidParticipant && o.owner == stp_skip) {
          continue;
        }
        sum += o.remaining;
      }
    }
  };
  if (aggressor == Side::Buy) {
    walk(asks_, false);
  } else {
    walk(bids_, true);
  }
  return sum;
}

Quantity OrderBook::level_total(Side side, PriceTicks price) const {
  const PriceLevel* lv = level(side, price);
  return lv == nullptr ? 0 : lv->total();
}

std::uint32_t OrderBook::level_count(Side side, PriceTicks price) const {
  const PriceLevel* lv = level(side, price);
  return lv == nullptr ? 0 : static_cast<std::uint32_t>(lv->count());
}

PriceLevel* OrderBook::level(Side side, PriceTicks price) {
  if (side == Side::Buy) {
    const auto it = bids_.find(price);
    return it == bids_.end() ? nullptr : &it->second;
  }
  const auto it = asks_.find(price);
  return it == asks_.end() ? nullptr : &it->second;
}

const PriceLevel* OrderBook::level(Side side, PriceTicks price) const {
  return const_cast<OrderBook*>(this)->level(side, price);
}

void OrderBook::erase_index(const RestingOrder& order) {
  by_id_.erase(order.id);
  if (order.client_id != 0) {
    by_client_.erase(ClientKey{order.owner, order.client_id});
  }
}

void OrderBook::drop_level_if_empty(Side side, PriceTicks price) {
  if (side == Side::Buy) {
    const auto it = bids_.find(price);
    if (it != bids_.end() && it->second.empty()) {
      bids_.erase(it);
    }
  } else {
    const auto it = asks_.find(price);
    if (it != asks_.end() && it->second.empty()) {
      asks_.erase(it);
    }
  }
}

void OrderBook::check_invariants() const {
  auto check_side = [&](const auto& m, Side side) {
    std::size_t counted = 0;
    PriceTicks prev = 0;
    bool have_prev = false;
    for (const auto& [px, lv] : m) {
      if (lv.empty()) {
        throw InvariantError("empty price level retained");
      }
      Quantity sum = 0;
      for (const auto& o : lv.orders()) {
        if (o.side != side) {
          throw InvariantError("order on wrong side");
        }
        if (o.price != px) {
          throw InvariantError("order price mismatches level");
        }
        if (o.remaining <= 0) {
          throw InvariantError("resting order with non-positive remaining");
        }
        if (o.symbol != symbol_) {
          throw InvariantError("order symbol mismatch");
        }
        const auto idx = by_id_.find(o.id);
        if (idx == by_id_.end()) {
          throw InvariantError("resting order missing from id index");
        }
        if (idx->second.price != px || idx->second.side != side) {
          throw InvariantError("id index stale");
        }
        if (idx->second.it->id != o.id) {
          throw InvariantError("id index iterator mismatch");
        }
        if (o.client_id != 0) {
          const auto ck = by_client_.find(ClientKey{o.owner, o.client_id});
          if (ck == by_client_.end() || ck->second != o.id) {
            throw InvariantError("client index stale");
          }
        }
        sum += o.remaining;
        ++counted;
      }
      if (sum != lv.total()) {
        throw InvariantError("level total disagrees with sum of remaining");
      }
      if (have_prev) {
        if (side == Side::Buy && !(px < prev)) {
          throw InvariantError("bids are not strictly descending");
        }
        if (side == Side::Sell && !(px > prev)) {
          throw InvariantError("asks are not strictly ascending");
        }
      }
      prev = px;
      have_prev = true;
    }
    return counted;
  };

  const std::size_t n = check_side(bids_, Side::Buy) + check_side(asks_, Side::Sell);
  if (n != by_id_.size()) {
    throw InvariantError("id index size disagrees with resting orders");
  }
  if (by_client_.size() > by_id_.size()) {
    throw InvariantError("client index larger than id index");
  }
}

void OrderBook::check_uncrossed() const {
  const auto bb = best_price(Side::Buy);
  const auto ba = best_price(Side::Sell);
  if (bb != 0 && ba != 0 && bb >= ba) {
    throw InvariantError("crossed or locked book");
  }
}

}  // namespace helix
