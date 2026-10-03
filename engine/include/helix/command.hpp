#pragma once

#include "helix/types.hpp"

#include <variant>

namespace helix {

struct HeartbeatCommand {};

struct LoginCommand {
  ParticipantId participant{kInvalidParticipant};
};

struct LogoutCommand {};

struct SubscribeCommand {
  SymbolId symbol{kInvalidSymbol};
  bool book{true};
  bool trades{true};
};

struct NewOrderCommand {
  ClientOrderId client_order_id{0};
  SymbolId symbol{kInvalidSymbol};
  Side side{Side::Buy};
  OrderType type{OrderType::Limit};
  TimeInForce tif{TimeInForce::Gtc};
  PriceTicks price{0};  // ignored for market orders
  Quantity quantity{0};
};

struct CancelOrderCommand {
  OrderId exchange_order_id{kInvalidOrderId};
  ClientOrderId client_order_id{0};  // used if exchange_order_id == 0
};

struct ModifyOrderCommand {
  OrderId exchange_order_id{kInvalidOrderId};
  ClientOrderId client_order_id{0};
  PriceTicks new_price{0};
  Quantity new_quantity{0};
};

using CommandPayload = std::variant<HeartbeatCommand,
                                    LoginCommand,
                                    LogoutCommand,
                                    SubscribeCommand,
                                    NewOrderCommand,
                                    CancelOrderCommand,
                                    ModifyOrderCommand>;

struct Command {
  ConnectionId connection{0};
  ParticipantId participant{kInvalidParticipant};  // filled after login
  Sequence seq{0};
  TimestampNs ts_ns{0};
  CommandPayload payload{};
};

}  // namespace helix
