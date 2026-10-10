#include "helix/matching_engine.hpp"
#include "test_harness.hpp"
#include "test_support.hpp"

using namespace helix;
using namespace helix::test;

HELIX_TEST(cancel_by_exchange_id_removes_resting_order) {
  auto eng = make_engine();
  const auto accepted = eng.execute(make_new(1, 1, 1, Side::Buy, 10, 5));
  const auto* acc = first_payload<OrderAcceptedEvent>(accepted);
  HELIX_CHECK(acc != nullptr);
  const auto ev = eng.execute(make_cancel(1, acc->exchange_order_id));
  const auto* c = first_payload<OrderCanceledEvent>(ev);
  HELIX_CHECK(c != nullptr);
  HELIX_CHECK_EQ(c->reason, CancelReason::UserRequested);
  HELIX_CHECK_EQ(c->canceled, 5);
  HELIX_CHECK(eng.book(1)->empty());
}

HELIX_TEST(cancel_by_client_id_when_exchange_id_is_zero) {
  auto eng = make_engine();
  eng.execute(make_new(1, 42, 1, Side::Sell, 10, 2));
  const auto ev = eng.execute(make_cancel(1, 0, 42));
  HELIX_CHECK(first_payload<OrderCanceledEvent>(ev) != nullptr);
  HELIX_CHECK(eng.book(1)->empty());
}

HELIX_TEST(cancel_rejects_unknown_and_not_owner) {
  auto eng = make_engine();
  const auto accepted = eng.execute(make_new(1, 1, 1, Side::Buy, 10, 1));
  const auto* acc = first_payload<OrderAcceptedEvent>(accepted);
  const auto unknown_ev = eng.execute(make_cancel(1, 999));
  HELIX_CHECK_EQ(first_payload<OrderRejectedEvent>(unknown_ev)->reason, RejectReason::UnknownOrder);
  const auto stolen_ev = eng.execute(make_cancel(2, acc->exchange_order_id));
  HELIX_CHECK_EQ(first_payload<OrderRejectedEvent>(stolen_ev)->reason, RejectReason::NotOwner);
  HELIX_CHECK_EQ(eng.book(1)->order_count(), 1u);
}

HELIX_TEST(modify_size_down_keeps_time_priority) {
  auto eng = make_engine();
  const auto accepted = eng.execute(make_new(1, 1, 1, Side::Buy, 10, 5));
  const auto* a = first_payload<OrderAcceptedEvent>(accepted);
  eng.execute(make_new(2, 1, 1, Side::Buy, 10, 5));
  const auto ev = eng.execute(make_modify(1, a->exchange_order_id, 10, 1));
  const auto* r = first_payload<OrderReplacedEvent>(ev);
  HELIX_CHECK(r != nullptr);
  HELIX_CHECK(!r->priority_lost);
  HELIX_CHECK_EQ(r->quantity, 1);
  HELIX_CHECK_EQ(eng.book(1)->best(Side::Buy)->id, a->exchange_order_id);
  HELIX_CHECK_EQ(eng.book(1)->best(Side::Buy)->remaining, 1);
}

HELIX_TEST(modify_size_up_loses_time_priority) {
  auto eng = make_engine();
  const auto accepted = eng.execute(make_new(1, 1, 1, Side::Buy, 10, 2));
  const auto* a = first_payload<OrderAcceptedEvent>(accepted);
  eng.execute(make_new(2, 1, 1, Side::Buy, 10, 2));
  const auto ev = eng.execute(make_modify(1, a->exchange_order_id, 10, 9));
  const auto* r = first_payload<OrderReplacedEvent>(ev);
  HELIX_CHECK(r->priority_lost);
  HELIX_CHECK_EQ(eng.book(1)->best(Side::Buy)->owner, static_cast<ParticipantId>(2));
  HELIX_CHECK_EQ(eng.book(1)->find(a->exchange_order_id)->remaining, 9);
}

HELIX_TEST(modify_price_can_cross_and_keeps_exchange_id) {
  auto eng = make_engine();
  const auto accepted = eng.execute(make_new(1, 1, 1, Side::Buy, 10, 4));
  const auto* a = first_payload<OrderAcceptedEvent>(accepted);
  eng.execute(make_new(2, 1, 1, Side::Sell, 12, 4));
  const auto ev = eng.execute(make_modify(1, a->exchange_order_id, 12, 4));
  HELIX_CHECK(first_payload<OrderReplacedEvent>(ev)->priority_lost);
  HELIX_CHECK_EQ(count_payload<PublicTradeEvent>(ev), 1);
  HELIX_CHECK_EQ(first_payload<PublicTradeEvent>(ev)->price, 12);
  HELIX_CHECK(eng.book(1)->empty());
}

HELIX_TEST(modify_rejects_not_owner_and_bad_qty) {
  auto eng = make_engine();
  const auto accepted = eng.execute(make_new(1, 1, 1, Side::Buy, 10, 2));
  const auto* a = first_payload<OrderAcceptedEvent>(accepted);
  const auto stolen = eng.execute(make_modify(2, a->exchange_order_id, 10, 1));
  HELIX_CHECK_EQ(first_payload<OrderRejectedEvent>(stolen)->reason, RejectReason::NotOwner);
  const auto bad = eng.execute(make_modify(1, a->exchange_order_id, 10, 0));
  HELIX_CHECK_EQ(first_payload<OrderRejectedEvent>(bad)->reason, RejectReason::InvalidModify);
}
