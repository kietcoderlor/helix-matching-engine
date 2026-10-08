#pragma once

#include "helix/command.hpp"
#include "helix/event.hpp"
#include "helix/matching_engine.hpp"

#include <variant>
#include <vector>

namespace helix::test {

inline Command make_new(ParticipantId p, ClientOrderId cid, SymbolId sym, Side side,
                        PriceTicks px, Quantity qty,
                        TimeInForce tif = TimeInForce::Gtc,
                        OrderType type = OrderType::Limit) {
  Command c;
  c.participant = p;
  c.connection = p;
  c.payload = NewOrderCommand{cid, sym, side, type, tif, px, qty};
  return c;
}

inline Command make_cancel(ParticipantId p, OrderId eid, ClientOrderId cid = 0) {
  Command c;
  c.participant = p;
  c.connection = p;
  c.payload = CancelOrderCommand{eid, cid};
  return c;
}

inline Command make_modify(ParticipantId p, OrderId eid, PriceTicks px, Quantity qty,
                           ClientOrderId cid = 0) {
  Command c;
  c.participant = p;
  c.connection = p;
  c.payload = ModifyOrderCommand{eid, cid, px, qty};
  return c;
}

inline MatchingEngine make_engine(SymbolId sym = 1) {
  MatchingEngine engine;
  engine.add_symbol(sym, "TEST");
  return engine;
}

template <class T>
inline int count_payload(const std::vector<Event>& evs) {
  int n = 0;
  for (const auto& e : evs) {
    if (std::holds_alternative<T>(e.payload)) {
      ++n;
    }
  }
  return n;
}

template <class T>
inline const T* first_payload(const std::vector<Event>& evs) {
  for (const auto& e : evs) {
    if (const T* p = std::get_if<T>(&e.payload)) {
      return p;
    }
  }
  return nullptr;
}

template <class T>
inline const T* last_payload(const std::vector<Event>& evs) {
  const T* found = nullptr;
  for (const auto& e : evs) {
    if (const T* p = std::get_if<T>(&e.payload)) {
      found = p;
    }
  }
  return found;
}

}  // namespace helix::test
