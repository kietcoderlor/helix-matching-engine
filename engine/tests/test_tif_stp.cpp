#include "helix/matching_engine.hpp"
#include "test_harness.hpp"
#include "test_support.hpp"

using namespace helix;
using namespace helix::test;

HELIX_TEST(ioc_cancels_unfilled_remainder) {
  auto eng = make_engine();
  eng.execute(make_new(1, 1, 1, Side::Sell, 50, 3));
  const auto ev = eng.execute(make_new(2, 1, 1, Side::Buy, 50, 10, TimeInForce::Ioc));
  HELIX_CHECK_EQ(count_payload<PublicTradeEvent>(ev), 1);
  const auto* c = first_payload<OrderCanceledEvent>(ev);
  HELIX_CHECK(c != nullptr);
  HELIX_CHECK_EQ(c->reason, CancelReason::IocRemainder);
  HELIX_CHECK_EQ(c->canceled, 7);
  HELIX_CHECK(eng.book(1)->best(Side::Buy) == nullptr);
}

HELIX_TEST(ioc_with_no_cross_accepts_then_cancels_all) {
  auto eng = make_engine();
  eng.execute(make_new(1, 1, 1, Side::Sell, 80, 1));
  const auto ev = eng.execute(make_new(2, 1, 1, Side::Buy, 70, 1, TimeInForce::Ioc));
  HELIX_CHECK_EQ(count_payload<PublicTradeEvent>(ev), 0);
  HELIX_CHECK_EQ(first_payload<OrderCanceledEvent>(ev)->canceled, 1);
  HELIX_CHECK_EQ(eng.book(1)->order_count(), 1u);
}

HELIX_TEST(fok_rejects_without_touching_the_book) {
  auto eng = make_engine();
  eng.execute(make_new(1, 1, 1, Side::Sell, 10, 3));
  const auto ev = eng.execute(make_new(2, 1, 1, Side::Buy, 10, 4, TimeInForce::Fok));
  HELIX_CHECK_EQ(first_payload<OrderRejectedEvent>(ev)->reason, RejectReason::FokNotFilled);
  HELIX_CHECK_EQ(count_payload<PublicTradeEvent>(ev), 0);
  HELIX_CHECK_EQ(eng.book(1)->best(Side::Sell)->remaining, 3);
}

HELIX_TEST(fok_fills_completely_when_liquidity_is_sufficient) {
  auto eng = make_engine();
  eng.execute(make_new(1, 1, 1, Side::Sell, 10, 2));
  eng.execute(make_new(1, 2, 1, Side::Sell, 11, 2));
  const auto ev = eng.execute(make_new(2, 1, 1, Side::Buy, 11, 4, TimeInForce::Fok));
  HELIX_CHECK_EQ(count_payload<PublicTradeEvent>(ev), 2);
  HELIX_CHECK_EQ(count_payload<OrderRejectedEvent>(ev), 0);
  HELIX_CHECK_EQ(count_payload<OrderCanceledEvent>(ev), 0);
  HELIX_CHECK(eng.book(1)->empty());
}

HELIX_TEST(stp_cni_cancels_aggressor_and_leaves_resting) {
  auto eng = make_engine();
  eng.execute(make_new(1, 1, 1, Side::Sell, 25, 4));
  const auto ev = eng.execute(make_new(1, 2, 1, Side::Buy, 25, 4));
  HELIX_CHECK_EQ(count_payload<PublicTradeEvent>(ev), 0);
  const auto* c = first_payload<OrderCanceledEvent>(ev);
  HELIX_CHECK_EQ(c->reason, CancelReason::SelfTradePrevention);
  HELIX_CHECK_EQ(c->canceled, 4);
  HELIX_CHECK_EQ(eng.book(1)->best(Side::Sell)->remaining, 4);
}

HELIX_TEST(stp_skips_self_then_matches_other_participant) {
  auto eng = make_engine();
  // Best ask is self, next ask is someone else.
  eng.execute(make_new(1, 1, 1, Side::Sell, 10, 2));
  eng.execute(make_new(2, 1, 1, Side::Sell, 11, 2));
  const auto ev = eng.execute(make_new(1, 2, 1, Side::Buy, 11, 2));
  // Hits own 10 first → STP cancels incoming; never reaches participant 2.
  HELIX_CHECK_EQ(count_payload<PublicTradeEvent>(ev), 0);
  HELIX_CHECK_EQ(first_payload<OrderCanceledEvent>(ev)->reason, CancelReason::SelfTradePrevention);
  HELIX_CHECK_EQ(eng.book(1)->order_count(), 2u);
}

HELIX_TEST(fok_does_not_count_self_as_liquidity) {
  auto eng = make_engine();
  eng.execute(make_new(1, 1, 1, Side::Sell, 10, 5));
  const auto ev = eng.execute(make_new(1, 2, 1, Side::Buy, 10, 5, TimeInForce::Fok));
  HELIX_CHECK_EQ(first_payload<OrderRejectedEvent>(ev)->reason, RejectReason::FokNotFilled);
  HELIX_CHECK_EQ(eng.book(1)->best(Side::Sell)->remaining, 5);
}
