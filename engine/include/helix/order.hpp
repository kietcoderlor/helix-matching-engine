#pragma once

#include "helix/types.hpp"

namespace helix {

// A resting (or not-yet-matched) order as stored in a book. Matching never
// shares this object across threads; the matching thread owns every field.
struct RestingOrder {
  OrderId id{kInvalidOrderId};
  ClientOrderId client_id{0};
  ParticipantId owner{kInvalidParticipant};
  SymbolId symbol{kInvalidSymbol};
  Side side{Side::Buy};
  OrderType type{OrderType::Limit};
  TimeInForce tif{TimeInForce::Gtc};
  PriceTicks price{0};
  Quantity remaining{0};
  Quantity original{0};
};

}  // namespace helix
