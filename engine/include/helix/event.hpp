#pragma once

#include "helix/types.hpp"

#include <cstdint>
#include <variant>
#include <vector>

namespace helix {

// Private lifecycle events are delivered only to the owning session.
// Public trade / book events are delivered to subscribers of the symbol.

struct HeartbeatAckEvent {};

struct LoginAckEvent {
  ParticipantId participant{kInvalidParticipant};
};

struct SessionEndEvent {
  RejectReason reason{RejectReason::None};
};

struct OrderAcceptedEvent {
  OrderId exchange_order_id{kInvalidOrderId};
  ClientOrderId client_order_id{0};
  SymbolId symbol{kInvalidSymbol};
  Side side{Side::Buy};
  OrderType type{OrderType::Limit};
  TimeInForce tif{TimeInForce::Gtc};
  PriceTicks price{0};
  Quantity quantity{0};
};

struct OrderRejectedEvent {
  ClientOrderId client_order_id{0};
  OrderId exchange_order_id{kInvalidOrderId};
  RejectReason reason{RejectReason::None};
};

struct OrderCanceledEvent {
  OrderId exchange_order_id{kInvalidOrderId};
  ClientOrderId client_order_id{0};
  Quantity canceled{0};
  CancelReason reason{CancelReason::UserRequested};
};

struct OrderReplacedEvent {
  OrderId exchange_order_id{kInvalidOrderId};
  ClientOrderId client_order_id{0};
  PriceTicks price{0};
  Quantity quantity{0};
  bool priority_lost{false};
};

struct PrivateFillEvent {
  TradeId trade_id{0};
  OrderId exchange_order_id{kInvalidOrderId};
  ClientOrderId client_order_id{0};
  SymbolId symbol{kInvalidSymbol};
  Side side{Side::Buy};
  PriceTicks price{0};
  Quantity quantity{0};
  Quantity remaining{0};
  Liquidity liquidity{Liquidity::Taker};
};

struct PublicTradeEvent {
  TradeId trade_id{0};
  SymbolId symbol{kInvalidSymbol};
  PriceTicks price{0};
  Quantity quantity{0};
  Side aggressor{Side::Buy};
};

struct BookLevel {
  PriceTicks price{0};
  Quantity quantity{0};
  std::uint32_t order_count{0};
};

struct BookSnapshotEvent {
  SymbolId symbol{kInvalidSymbol};
  std::vector<BookLevel> bids;
  std::vector<BookLevel> asks;
};

enum class BookDeltaOp : std::uint8_t {
  Set = 0,     // set level qty/count (0 qty means delete)
};

struct BookDeltaEvent {
  SymbolId symbol{kInvalidSymbol};
  Side side{Side::Buy};
  BookDeltaOp op{BookDeltaOp::Set};
  PriceTicks price{0};
  Quantity quantity{0};
  std::uint32_t order_count{0};
};

struct ErrorEvent {
  RejectReason reason{RejectReason::None};
  Sequence ref_seq{0};
};

using EventPayload = std::variant<HeartbeatAckEvent,
                                  LoginAckEvent,
                                  SessionEndEvent,
                                  OrderAcceptedEvent,
                                  OrderRejectedEvent,
                                  OrderCanceledEvent,
                                  OrderReplacedEvent,
                                  PrivateFillEvent,
                                  PublicTradeEvent,
                                  BookSnapshotEvent,
                                  BookDeltaEvent,
                                  ErrorEvent>;

enum class EventScope : std::uint8_t {
  Connection = 0,  // heartbeat, login, errors
  Private = 1,     // owning participant only
  Public = 2,      // symbol subscribers
};

struct Event {
  EventScope scope{EventScope::Private};
  ConnectionId connection{0};          // set for connection-scoped and private
  ParticipantId participant{kInvalidParticipant};
  SymbolId symbol{kInvalidSymbol};     // set for public book/trade
  Sequence seq{0};
  TimestampNs ts_ns{0};
  EventPayload payload{};
};

}  // namespace helix
