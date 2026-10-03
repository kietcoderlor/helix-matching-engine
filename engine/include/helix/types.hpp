#pragma once

#include <cstdint>
#include <string_view>

namespace helix {

// All money and size fields are integers. Prices are integer ticks
// (for example 1 tick = 0.01 USD). Quantities are integer lots.
// Floating-point is never used on the matching path.

using ParticipantId = std::uint64_t;
using OrderId = std::uint64_t;          // exchange-assigned, monotonic
using ClientOrderId = std::uint64_t;    // client-assigned, unique per participant
using SymbolId = std::uint32_t;
using TradeId = std::uint64_t;
using PriceTicks = std::int64_t;
using Quantity = std::int64_t;
using TimestampNs = std::uint64_t;      // engine monotonic clock
using ConnectionId = std::uint64_t;
using Sequence = std::uint32_t;

inline constexpr ParticipantId kInvalidParticipant = 0;
inline constexpr OrderId kInvalidOrderId = 0;
inline constexpr SymbolId kInvalidSymbol = 0;
inline constexpr Quantity kMaxQuantity = 1'000'000'000'000;
inline constexpr PriceTicks kMaxPriceTicks = 1'000'000'000'000;

enum class Side : std::uint8_t {
  Buy = 0,
  Sell = 1,
};

enum class OrderType : std::uint8_t {
  Limit = 0,
  Market = 1,
};

enum class TimeInForce : std::uint8_t {
  Gtc = 0,  // rest remainder
  Ioc = 1,  // cancel remainder after matching
  Fok = 2,  // reject entirely if it cannot fill in full
};

enum class RejectReason : std::uint16_t {
  None = 0,
  InvalidSymbol = 1,
  InvalidQuantity = 2,
  InvalidPrice = 3,
  DuplicateClientOrderId = 4,
  UnknownOrder = 5,
  NotOwner = 6,
  FokNotFilled = 7,
  MarketNoLiquidity = 8,
  Unauthorized = 9,
  InvalidFrame = 10,
  QueueFull = 11,
  SessionNotLoggedIn = 12,
  InvalidModify = 13,
  UnsupportedVersion = 14,
  InvalidSide = 15,
  InvalidOrderType = 16,
  InvalidTimeInForce = 17,
  PayloadTruncated = 18,
  PayloadTooLarge = 19,
};

enum class CancelReason : std::uint8_t {
  UserRequested = 0,
  IocRemainder = 1,
  SelfTradePrevention = 2,
  MarketRemainder = 3,
  Disconnect = 4,
  Replaced = 5,
};

enum class Liquidity : std::uint8_t {
  Maker = 0,
  Taker = 1,
};

inline constexpr std::string_view to_string(Side s) noexcept {
  return s == Side::Buy ? "BUY" : "SELL";
}

inline constexpr std::string_view to_string(OrderType t) noexcept {
  return t == OrderType::Limit ? "LIMIT" : "MARKET";
}

inline constexpr std::string_view to_string(TimeInForce t) noexcept {
  switch (t) {
    case TimeInForce::Gtc:
      return "GTC";
    case TimeInForce::Ioc:
      return "IOC";
    case TimeInForce::Fok:
      return "FOK";
  }
  return "?";
}

inline constexpr Side opposite(Side s) noexcept {
  return s == Side::Buy ? Side::Sell : Side::Buy;
}

inline constexpr bool is_buy(Side s) noexcept { return s == Side::Buy; }

inline constexpr bool valid_quantity(Quantity q) noexcept {
  return q > 0 && q <= kMaxQuantity;
}

inline constexpr bool valid_limit_price(PriceTicks p) noexcept {
  return p > 0 && p <= kMaxPriceTicks;
}

}  // namespace helix
