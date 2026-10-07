#include "helix/order_book.hpp"
#include "test_harness.hpp"

using namespace helix;

static RestingOrder make_order(OrderId id, ParticipantId owner, Side side, PriceTicks px,
                               Quantity qty, ClientOrderId cid = 0) {
  RestingOrder o;
  o.id = id;
  o.client_id = cid == 0 ? id : cid;
  o.owner = owner;
  o.symbol = 1;
  o.side = side;
  o.type = OrderType::Limit;
  o.tif = TimeInForce::Gtc;
  o.price = px;
  o.remaining = qty;
  o.original = qty;
  return o;
}

HELIX_TEST(book_best_price_is_highest_bid_lowest_ask) {
  OrderBook book(1);
  book.rest(make_order(1, 10, Side::Buy, 100, 5));
  book.rest(make_order(2, 10, Side::Buy, 101, 3));
  book.rest(make_order(3, 11, Side::Sell, 105, 4));
  book.rest(make_order(4, 11, Side::Sell, 104, 2));
  book.check_invariants();
  book.check_uncrossed();
  HELIX_CHECK_EQ(book.best_price(Side::Buy), 101);
  HELIX_CHECK_EQ(book.best_price(Side::Sell), 104);
  HELIX_CHECK_EQ(book.best(Side::Buy)->id, static_cast<OrderId>(2));
  HELIX_CHECK_EQ(book.best(Side::Sell)->id, static_cast<OrderId>(4));
}

HELIX_TEST(book_same_price_is_fifo) {
  OrderBook book(1);
  book.rest(make_order(1, 10, Side::Buy, 100, 5, 1));
  book.rest(make_order(2, 11, Side::Buy, 100, 7, 2));
  book.rest(make_order(3, 12, Side::Buy, 100, 1, 3));
  HELIX_CHECK_EQ(book.best(Side::Buy)->id, static_cast<OrderId>(1));
  HELIX_CHECK_EQ(book.level_total(Side::Buy, 100), 13);
  HELIX_CHECK_EQ(book.level_count(Side::Buy, 100), 3u);
  book.reduce(1, 5);  // fully fills first
  book.check_invariants();
  HELIX_CHECK_EQ(book.best(Side::Buy)->id, static_cast<OrderId>(2));
  HELIX_CHECK_EQ(book.level_count(Side::Buy, 100), 2u);
}

HELIX_TEST(book_cancel_middle_keeps_fifo_of_neighbors) {
  OrderBook book(1);
  book.rest(make_order(1, 10, Side::Sell, 50, 1, 1));
  book.rest(make_order(2, 11, Side::Sell, 50, 1, 2));
  book.rest(make_order(3, 12, Side::Sell, 50, 1, 3));
  auto gone = book.cancel(2);
  HELIX_CHECK(gone.has_value());
  HELIX_CHECK_EQ(gone->id, static_cast<OrderId>(2));
  book.check_invariants();
  HELIX_CHECK_EQ(book.best(Side::Sell)->id, static_cast<OrderId>(1));
  book.reduce(1, 1);
  HELIX_CHECK_EQ(book.best(Side::Sell)->id, static_cast<OrderId>(3));
}

HELIX_TEST(book_cancel_last_order_drops_level) {
  OrderBook book(1);
  book.rest(make_order(1, 10, Side::Buy, 90, 2, 1));
  book.rest(make_order(2, 10, Side::Buy, 91, 2, 2));
  book.cancel(2);
  book.check_invariants();
  HELIX_CHECK_EQ(book.best_price(Side::Buy), 90);
  book.cancel(1);
  HELIX_CHECK(book.empty());
  HELIX_CHECK_EQ(book.best_price(Side::Buy), 0);
  HELIX_CHECK(book.best(Side::Buy) == nullptr);
}

HELIX_TEST(book_client_index_and_duplicate_client_id) {
  OrderBook book(1);
  book.rest(make_order(1, 10, Side::Buy, 100, 1, 77));
  HELIX_CHECK(book.find_by_client(10, 77) != nullptr);
  HELIX_CHECK(book.find_by_client(11, 77) == nullptr);
  bool threw = false;
  try {
    book.rest(make_order(2, 10, Side::Buy, 99, 1, 77));
  } catch (const InvariantError&) {
    threw = true;
  }
  HELIX_CHECK(threw);
  auto c = book.cancel_by_client(10, 77);
  HELIX_CHECK(c.has_value());
  HELIX_CHECK(book.find_by_client(10, 77) == nullptr);
}

HELIX_TEST(book_depth_snapshot_aggregates_levels) {
  OrderBook book(1);
  book.rest(make_order(1, 1, Side::Buy, 10, 2, 1));
  book.rest(make_order(2, 2, Side::Buy, 10, 3, 2));
  book.rest(make_order(3, 3, Side::Buy, 9, 8, 3));
  const auto d = book.depth(Side::Buy, 8);
  HELIX_CHECK_EQ(d.size(), 2u);
  HELIX_CHECK_EQ(d[0].price, 10);
  HELIX_CHECK_EQ(d[0].quantity, 5);
  HELIX_CHECK_EQ(d[0].order_count, 2u);
  HELIX_CHECK_EQ(d[1].price, 9);
}

HELIX_TEST(book_available_respects_limit_and_stp_skip) {
  OrderBook book(1);
  book.rest(make_order(1, 10, Side::Sell, 100, 5, 1));
  book.rest(make_order(2, 10, Side::Sell, 101, 5, 2));
  book.rest(make_order(3, 11, Side::Sell, 102, 5, 3));
  HELIX_CHECK_EQ(book.available(Side::Buy, 101, kInvalidParticipant), 10);
  HELIX_CHECK_EQ(book.available(Side::Buy, 0, kInvalidParticipant), 15);
  HELIX_CHECK_EQ(book.available(Side::Buy, 0, 10), 5);  // skip participant 10
}

HELIX_TEST(book_cancel_unknown_is_empty) {
  OrderBook book(1);
  HELIX_CHECK(!book.cancel(999).has_value());
  HELIX_CHECK(!book.reduce(999, 1).has_value());
}
