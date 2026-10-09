#include "helix/matching_engine.hpp"
#include "test_harness.hpp"
#include "test_support.hpp"

using namespace helix;
using namespace helix::test;

HELIX_TEST(limit_gtc_rests_when_it_does_not_cross) {
  auto eng = make_engine();
  const auto ev = eng.execute(make_new(1, 1, 1, Side::Buy, 100, 10));
  HELIX_CHECK_EQ(count_payload<OrderAcceptedEvent>(ev), 1);
  HELIX_CHECK_EQ(count_payload<PublicTradeEvent>(ev), 0);
  HELIX_CHECK_EQ(eng.book(1)->order_count(), 1u);
  HELIX_CHECK_EQ(eng.book(1)->best_price(Side::Buy), 100);
}

HELIX_TEST(limit_crosses_at_maker_price_and_fifo) {
  auto eng = make_engine();
  eng.execute(make_new(1, 1, 1, Side::Sell, 100, 3));
  eng.execute(make_new(2, 1, 1, Side::Sell, 100, 3));
  const auto ev = eng.execute(make_new(3, 1, 1, Side::Buy, 105, 4));
  HELIX_CHECK_EQ(count_payload<PublicTradeEvent>(ev), 2);
  const auto* t0 = first_payload<PublicTradeEvent>(ev);
  HELIX_CHECK(t0 != nullptr);
  HELIX_CHECK_EQ(t0->price, 100);
  HELIX_CHECK_EQ(t0->quantity, 3);
  // Remainder 1 fills against the second sell (FIFO at 100).
  HELIX_CHECK_EQ(eng.book(1)->best(Side::Sell)->id, static_cast<OrderId>(2));
  HELIX_CHECK_EQ(eng.book(1)->best(Side::Sell)->remaining, 2);
  HELIX_CHECK(eng.book(1)->best(Side::Buy) == nullptr);
}

HELIX_TEST(better_price_trades_before_worse_price) {
  auto eng = make_engine();
  eng.execute(make_new(1, 1, 1, Side::Sell, 102, 5));
  eng.execute(make_new(2, 1, 1, Side::Sell, 100, 5));
  const auto ev = eng.execute(make_new(3, 1, 1, Side::Buy, 102, 5));
  const auto* t = first_payload<PublicTradeEvent>(ev);
  HELIX_CHECK(t != nullptr);
  HELIX_CHECK_EQ(t->price, 100);
  HELIX_CHECK_EQ(eng.book(1)->best_price(Side::Sell), 102);
}

HELIX_TEST(partial_fill_rests_gtc_remainder) {
  auto eng = make_engine();
  eng.execute(make_new(1, 1, 1, Side::Sell, 50, 4));
  const auto ev = eng.execute(make_new(2, 1, 1, Side::Buy, 50, 10));
  HELIX_CHECK_EQ(count_payload<PublicTradeEvent>(ev), 1);
  HELIX_CHECK_EQ(eng.book(1)->best_price(Side::Buy), 50);
  HELIX_CHECK_EQ(eng.book(1)->best(Side::Buy)->remaining, 6);
  HELIX_CHECK(eng.book(1)->best(Side::Sell) == nullptr);
}

HELIX_TEST(market_walks_book_and_cancels_remainder) {
  auto eng = make_engine();
  eng.execute(make_new(1, 1, 1, Side::Sell, 10, 2));
  eng.execute(make_new(1, 2, 1, Side::Sell, 11, 2));
  const auto ev = eng.execute(make_new(2, 1, 1, Side::Buy, 0, 10, TimeInForce::Ioc, OrderType::Market));
  HELIX_CHECK_EQ(count_payload<PublicTradeEvent>(ev), 2);
  HELIX_CHECK_EQ(count_payload<OrderCanceledEvent>(ev), 1);
  const auto* c = first_payload<OrderCanceledEvent>(ev);
  HELIX_CHECK_EQ(c->reason, CancelReason::MarketRemainder);
  HELIX_CHECK_EQ(c->canceled, 6);
  HELIX_CHECK(eng.book(1)->empty());
}

HELIX_TEST(market_rejects_when_no_liquidity) {
  auto eng = make_engine();
  const auto ev = eng.execute(make_new(1, 1, 1, Side::Buy, 0, 1, TimeInForce::Ioc, OrderType::Market));
  const auto* r = first_payload<OrderRejectedEvent>(ev);
  HELIX_CHECK(r != nullptr);
  HELIX_CHECK_EQ(r->reason, RejectReason::MarketNoLiquidity);
}

HELIX_TEST(invalid_symbol_and_qty_and_duplicate_cid) {
  auto eng = make_engine();
  HELIX_CHECK_EQ(first_payload<OrderRejectedEvent>(eng.execute(make_new(1, 1, 9, Side::Buy, 1, 1)))->reason,
                 RejectReason::InvalidSymbol);
  HELIX_CHECK_EQ(first_payload<OrderRejectedEvent>(eng.execute(make_new(1, 1, 1, Side::Buy, 1, 0)))->reason,
                 RejectReason::InvalidQuantity);
  eng.execute(make_new(1, 7, 1, Side::Buy, 1, 1));
  HELIX_CHECK_EQ(first_payload<OrderRejectedEvent>(eng.execute(make_new(1, 7, 1, Side::Buy, 1, 1)))->reason,
                 RejectReason::DuplicateClientOrderId);
}

HELIX_TEST(private_fills_mark_maker_and_taker) {
  auto eng = make_engine();
  eng.execute(make_new(1, 1, 1, Side::Sell, 20, 2));
  const auto ev = eng.execute(make_new(2, 1, 1, Side::Buy, 20, 2));
  int makers = 0;
  int takers = 0;
  for (const auto& e : ev) {
    if (const auto* f = std::get_if<PrivateFillEvent>(&e.payload)) {
      if (f->liquidity == Liquidity::Maker) {
        ++makers;
        HELIX_CHECK_EQ(e.participant, static_cast<ParticipantId>(1));
      }
      if (f->liquidity == Liquidity::Taker) {
        ++takers;
        HELIX_CHECK_EQ(e.participant, static_cast<ParticipantId>(2));
      }
    }
  }
  HELIX_CHECK_EQ(makers, 1);
  HELIX_CHECK_EQ(takers, 1);
}
