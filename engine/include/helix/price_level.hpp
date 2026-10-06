#pragma once

#include "helix/order.hpp"

#include <cstddef>
#include <list>

namespace helix {

// FIFO queue of resting orders at one price. Quantity total is cached so
// book snapshots and FOK probes are O(1) per level, not O(orders).
class PriceLevel {
 public:
  using List = std::list<RestingOrder>;
  using Iterator = List::iterator;
  using ConstIterator = List::const_iterator;

  Iterator push_back(RestingOrder order) {
    total_ += order.remaining;
    orders_.push_back(std::move(order));
    return std::prev(orders_.end());
  }

  void erase(Iterator it) {
    total_ -= it->remaining;
    orders_.erase(it);
  }

  void reduce(Iterator it, Quantity by) {
    it->remaining -= by;
    total_ -= by;
  }

  [[nodiscard]] bool empty() const noexcept { return orders_.empty(); }
  [[nodiscard]] std::size_t count() const noexcept { return orders_.size(); }
  [[nodiscard]] Quantity total() const noexcept { return total_; }

  [[nodiscard]] RestingOrder& front() { return orders_.front(); }
  [[nodiscard]] const RestingOrder& front() const { return orders_.front(); }

  List& orders() noexcept { return orders_; }
  const List& orders() const noexcept { return orders_; }

 private:
  List orders_;
  Quantity total_{0};
};

}  // namespace helix
