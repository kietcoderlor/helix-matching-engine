#include "helix/matching_engine.hpp"

#include <type_traits>
#include <utility>
#include <variant>

namespace helix {
namespace {

bool crosses(Side aggressor, PriceTicks limit, bool market, PriceTicks maker_px) {
  if (market || limit == 0) {
    return true;
  }
  if (aggressor == Side::Buy) {
    return maker_px <= limit;
  }
  return maker_px >= limit;
}

}  // namespace

MatchingEngine::MatchingEngine() : MatchingEngine(Config{}) {}

MatchingEngine::MatchingEngine(Config config) : config_(config) {}

void MatchingEngine::add_symbol(SymbolId id, std::string name) {
  if (id == kInvalidSymbol) {
    throw InvariantError("symbol id 0 is reserved");
  }
  if (books_.contains(id)) {
    throw InvariantError("duplicate symbol");
  }
  names_.emplace(id, std::move(name));
  books_.emplace(id, OrderBook{id});
}

bool MatchingEngine::has_symbol(SymbolId id) const { return books_.contains(id); }

const OrderBook* MatchingEngine::book(SymbolId id) const {
  const auto it = books_.find(id);
  return it == books_.end() ? nullptr : &it->second;
}

TimestampNs MatchingEngine::stamp(const Command& cmd) {
  if (cmd.ts_ns != 0) {
    last_ts_ = cmd.ts_ns;
    if (cmd.ts_ns > clock_) {
      clock_ = cmd.ts_ns;
    }
    return cmd.ts_ns;
  }
  last_ts_ = ++clock_;
  return last_ts_;
}

Event MatchingEngine::base_event(const Command& cmd, EventScope scope, SymbolId symbol) const {
  Event e;
  e.scope = scope;
  e.connection = cmd.connection;
  e.participant = cmd.participant;
  e.symbol = symbol;
  e.seq = cmd.seq;
  e.ts_ns = last_ts_;
  return e;
}

Event MatchingEngine::reject(const Command& cmd, ClientOrderId cid, OrderId eid,
                             RejectReason reason) const {
  Event e = base_event(cmd, EventScope::Private, kInvalidSymbol);
  e.payload = OrderRejectedEvent{cid, eid, reason};
  return e;
}

void MatchingEngine::maybe_check(const OrderBook& book) const {
  if (!config_.check_invariants) {
    return;
  }
  book.check_invariants();
  book.check_uncrossed();
}

std::vector<Event> MatchingEngine::execute(const Command& command) {
  stamp(command);
  return std::visit(
      [&](const auto& payload) -> std::vector<Event> {
        using T = std::decay_t<decltype(payload)>;
        if constexpr (std::is_same_v<T, NewOrderCommand>) {
          return on_new(command, payload);
        } else if constexpr (std::is_same_v<T, CancelOrderCommand>) {
          return on_cancel(command, payload);
        } else if constexpr (std::is_same_v<T, ModifyOrderCommand>) {
          return on_modify(command, payload);
        } else {
          Event e = base_event(command, EventScope::Connection, kInvalidSymbol);
          e.payload = ErrorEvent{RejectReason::Unauthorized, command.seq};
          return {e};
        }
      },
      command.payload);
}

std::vector<Event> MatchingEngine::on_new(const Command& cmd, const NewOrderCommand& n) {
  if (cmd.participant == kInvalidParticipant) {
    return {reject(cmd, n.client_order_id, kInvalidOrderId, RejectReason::Unauthorized)};
  }
  if (!valid_quantity(n.quantity)) {
    return {reject(cmd, n.client_order_id, kInvalidOrderId, RejectReason::InvalidQuantity)};
  }
  if (n.type != OrderType::Limit && n.type != OrderType::Market) {
    return {reject(cmd, n.client_order_id, kInvalidOrderId, RejectReason::InvalidOrderType)};
  }
  if (n.tif != TimeInForce::Gtc && n.tif != TimeInForce::Ioc && n.tif != TimeInForce::Fok) {
    return {reject(cmd, n.client_order_id, kInvalidOrderId, RejectReason::InvalidTimeInForce)};
  }
  if (n.type == OrderType::Limit && !valid_limit_price(n.price)) {
    return {reject(cmd, n.client_order_id, kInvalidOrderId, RejectReason::InvalidPrice)};
  }
  if (n.type == OrderType::Market && n.tif == TimeInForce::Gtc) {
    // Market orders never rest; treat GTC market as IOC.
  }
  auto bit = books_.find(n.symbol);
  if (bit == books_.end()) {
    return {reject(cmd, n.client_order_id, kInvalidOrderId, RejectReason::InvalidSymbol)};
  }
  OrderBook& book = bit->second;
  if (n.client_order_id != 0 && book.find_by_client(cmd.participant, n.client_order_id) != nullptr) {
    return {reject(cmd, n.client_order_id, kInvalidOrderId, RejectReason::DuplicateClientOrderId)};
  }

  const bool market = n.type == OrderType::Market;
  const PriceTicks limit = market ? 0 : n.price;

  if (n.tif == TimeInForce::Fok) {
    const Quantity vis = book.available(n.side, limit, cmd.participant);
    if (vis < n.quantity) {
      return {reject(cmd, n.client_order_id, kInvalidOrderId, RejectReason::FokNotFilled)};
    }
  }
  if (market) {
    const Quantity vis = book.available(n.side, 0, cmd.participant);
    if (vis == 0) {
      return {reject(cmd, n.client_order_id, kInvalidOrderId, RejectReason::MarketNoLiquidity)};
    }
  }

  RestingOrder aggressor;
  aggressor.id = next_order_id_++;
  aggressor.client_id = n.client_order_id;
  aggressor.owner = cmd.participant;
  aggressor.symbol = n.symbol;
  aggressor.side = n.side;
  aggressor.type = n.type;
  aggressor.tif = n.tif;
  aggressor.price = n.price;
  aggressor.remaining = n.quantity;
  aggressor.original = n.quantity;

  std::vector<Event> out;
  Event acc = base_event(cmd, EventScope::Private, n.symbol);
  acc.payload = OrderAcceptedEvent{aggressor.id, aggressor.client_id, aggressor.symbol,
                                   aggressor.side, aggressor.type, aggressor.tif, aggressor.price,
                                   aggressor.original};
  out.push_back(std::move(acc));

  match(cmd, book, aggressor, out);
  rest_or_cancel_remainder(cmd, book, aggressor, out);
  maybe_check(book);
  return out;
}

void MatchingEngine::match(const Command& cmd, OrderBook& book, RestingOrder& aggressor,
                           std::vector<Event>& out) {
  const bool market = aggressor.type == OrderType::Market;
  while (aggressor.remaining > 0) {
    RestingOrder* maker = book.best(opposite(aggressor.side));
    if (maker == nullptr) {
      break;
    }
    if (!crosses(aggressor.side, aggressor.price, market, maker->price)) {
      break;
    }
    if (maker->owner == aggressor.owner) {
      Event c = base_event(cmd, EventScope::Private, book.symbol());
      c.payload = OrderCanceledEvent{aggressor.id, aggressor.client_id, aggressor.remaining,
                                     CancelReason::SelfTradePrevention};
      out.push_back(std::move(c));
      aggressor.remaining = 0;
      break;
    }
    const Quantity qty =
        aggressor.remaining < maker->remaining ? aggressor.remaining : maker->remaining;
    const RestingOrder maker_copy = *maker;
    emit_fill(cmd, maker_copy, aggressor, qty, out);
    book.reduce(maker->id, qty);
    aggressor.remaining -= qty;
    emit_book_level(book, maker_copy.side, maker_copy.price, out);
  }
}

void MatchingEngine::emit_fill(const Command& cmd, const RestingOrder& maker,
                               const RestingOrder& taker, Quantity qty, std::vector<Event>& out) {
  const TradeId tid = next_trade_id_++;
  const PriceTicks px = maker.price;
  const Quantity maker_rem = maker.remaining - qty;
  const Quantity taker_rem = taker.remaining - qty;

  PrivateFillEvent maker_fill;
  maker_fill.trade_id = tid;
  maker_fill.exchange_order_id = maker.id;
  maker_fill.client_order_id = maker.client_id;
  maker_fill.symbol = maker.symbol;
  maker_fill.side = maker.side;
  maker_fill.price = px;
  maker_fill.quantity = qty;
  maker_fill.remaining = maker_rem;
  maker_fill.liquidity = Liquidity::Maker;

  PrivateFillEvent taker_fill = maker_fill;
  taker_fill.exchange_order_id = taker.id;
  taker_fill.client_order_id = taker.client_id;
  taker_fill.side = taker.side;
  taker_fill.remaining = taker_rem;
  taker_fill.liquidity = Liquidity::Taker;

  Event em = base_event(cmd, EventScope::Private, maker.symbol);
  em.participant = maker.owner;
  em.connection = 0;  // maker may live on another connection; router uses participant
  em.payload = maker_fill;

  Event et = base_event(cmd, EventScope::Private, taker.symbol);
  et.participant = taker.owner;
  et.payload = taker_fill;

  Event pub = base_event(cmd, EventScope::Public, taker.symbol);
  pub.participant = kInvalidParticipant;
  pub.payload = PublicTradeEvent{tid, taker.symbol, px, qty, taker.side};

  out.push_back(std::move(em));
  out.push_back(std::move(et));
  out.push_back(std::move(pub));
}

void MatchingEngine::emit_book_level(const OrderBook& book, Side side, PriceTicks price,
                                     std::vector<Event>& out) {
  Event e;
  e.scope = EventScope::Public;
  e.symbol = book.symbol();
  e.ts_ns = last_ts_;
  e.payload = BookDeltaEvent{book.symbol(), side, BookDeltaOp::Set, price,
                             book.level_total(side, price), book.level_count(side, price)};
  out.push_back(std::move(e));
}

void MatchingEngine::rest_or_cancel_remainder(const Command& cmd, OrderBook& book,
                                              RestingOrder& aggressor, std::vector<Event>& out) {
  if (aggressor.remaining <= 0) {
    return;
  }
  const bool market = aggressor.type == OrderType::Market;
  const bool may_rest = !market && aggressor.tif == TimeInForce::Gtc;
  if (may_rest) {
    const Side side = aggressor.side;
    const PriceTicks px = aggressor.price;
    book.rest(aggressor);
    emit_book_level(book, side, px, out);
    return;
  }
  Event c = base_event(cmd, EventScope::Private, aggressor.symbol);
  const auto reason = market ? CancelReason::MarketRemainder : CancelReason::IocRemainder;
  c.payload = OrderCanceledEvent{aggressor.id, aggressor.client_id, aggressor.remaining, reason};
  out.push_back(std::move(c));
  aggressor.remaining = 0;
}

std::vector<Event> MatchingEngine::on_cancel(const Command& cmd, const CancelOrderCommand& c) {
  if (cmd.participant == kInvalidParticipant) {
    return {reject(cmd, c.client_order_id, c.exchange_order_id, RejectReason::Unauthorized)};
  }
  // Scan all books: exchange ids are globally unique.
  OrderBook* found_book = nullptr;
  const RestingOrder* found = nullptr;
  if (c.exchange_order_id != kInvalidOrderId) {
    for (auto& [sid, book] : books_) {
      if (const RestingOrder* o = book.find(c.exchange_order_id)) {
        found_book = &book;
        found = o;
        break;
      }
    }
  } else {
    for (auto& [sid, book] : books_) {
      if (const RestingOrder* o = book.find_by_client(cmd.participant, c.client_order_id)) {
        found_book = &book;
        found = o;
        break;
      }
    }
  }
  if (found == nullptr || found_book == nullptr) {
    return {reject(cmd, c.client_order_id, c.exchange_order_id, RejectReason::UnknownOrder)};
  }
  if (found->owner != cmd.participant) {
    return {reject(cmd, found->client_id, found->id, RejectReason::NotOwner)};
  }
  const Side side = found->side;
  const PriceTicks px = found->price;
  const auto canceled = found_book->cancel(found->id);
  Event e = base_event(cmd, EventScope::Private, found_book->symbol());
  e.payload = OrderCanceledEvent{canceled->id, canceled->client_id, canceled->remaining,
                                 CancelReason::UserRequested};
  std::vector<Event> out;
  out.push_back(std::move(e));
  emit_book_level(*found_book, side, px, out);
  maybe_check(*found_book);
  return out;
}

std::vector<Event> MatchingEngine::on_modify(const Command& cmd, const ModifyOrderCommand& m) {
  if (cmd.participant == kInvalidParticipant) {
    return {reject(cmd, m.client_order_id, m.exchange_order_id, RejectReason::Unauthorized)};
  }
  if (!valid_quantity(m.new_quantity) || !valid_limit_price(m.new_price)) {
    return {reject(cmd, m.client_order_id, m.exchange_order_id, RejectReason::InvalidModify)};
  }
  OrderBook* found_book = nullptr;
  RestingOrder* found = nullptr;
  if (m.exchange_order_id != kInvalidOrderId) {
    for (auto& [sid, book] : books_) {
      if (RestingOrder* o = book.find(m.exchange_order_id)) {
        found_book = &book;
        found = o;
        break;
      }
    }
  } else {
    for (auto& [sid, book] : books_) {
      if (RestingOrder* o = book.find_by_client(cmd.participant, m.client_order_id)) {
        found_book = &book;
        found = o;
        break;
      }
    }
  }
  if (found == nullptr || found_book == nullptr) {
    return {reject(cmd, m.client_order_id, m.exchange_order_id, RejectReason::UnknownOrder)};
  }
  if (found->owner != cmd.participant) {
    return {reject(cmd, found->client_id, found->id, RejectReason::NotOwner)};
  }
  if (found->type != OrderType::Limit) {
    return {reject(cmd, found->client_id, found->id, RejectReason::InvalidModify)};
  }

  const bool same_price = found->price == m.new_price;
  const bool size_down = m.new_quantity < found->remaining;
  if (same_price && size_down) {
    const Quantity cut = found->remaining - m.new_quantity;
    const Side side = found->side;
    const PriceTicks px = found->price;
    const auto after = found_book->reduce(found->id, cut);
    Event e = base_event(cmd, EventScope::Private, found_book->symbol());
    e.payload = OrderReplacedEvent{after->id, after->client_id, after->price, after->remaining,
                                   false};
    std::vector<Event> out;
    out.push_back(std::move(e));
    emit_book_level(*found_book, side, px, out);
    maybe_check(*found_book);
    return out;
  }

  // Price change or size-up: pull, then rematch. Same exchange id, new time priority.
  RestingOrder pulled = *found;
  const Side old_side = pulled.side;
  const PriceTicks old_px = pulled.price;
  found_book->cancel(pulled.id);
  std::vector<Event> out;
  emit_book_level(*found_book, old_side, old_px, out);

  pulled.price = m.new_price;
  pulled.remaining = m.new_quantity;
  pulled.original = m.new_quantity;
  Event repl = base_event(cmd, EventScope::Private, found_book->symbol());
  repl.payload =
      OrderReplacedEvent{pulled.id, pulled.client_id, pulled.price, pulled.remaining, true};
  out.push_back(std::move(repl));

  match(cmd, *found_book, pulled, out);
  if (pulled.remaining > 0) {
    const Side side = pulled.side;
    const PriceTicks px = pulled.price;
    found_book->rest(pulled);
    emit_book_level(*found_book, side, px, out);
  }
  maybe_check(*found_book);
  return out;
}

}  // namespace helix
