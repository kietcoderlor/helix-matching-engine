#include "helix/command.hpp"
#include "helix/event.hpp"
#include "helix/types.hpp"
#include "test_harness.hpp"

HELIX_TEST(side_opposite_and_labels) {
  HELIX_CHECK_EQ(helix::opposite(helix::Side::Buy), helix::Side::Sell);
  HELIX_CHECK_EQ(helix::opposite(helix::Side::Sell), helix::Side::Buy);
  HELIX_CHECK(helix::is_buy(helix::Side::Buy));
  HELIX_CHECK(!helix::is_buy(helix::Side::Sell));
  HELIX_CHECK_EQ(helix::to_string(helix::Side::Buy), std::string_view{"BUY"});
  HELIX_CHECK_EQ(helix::to_string(helix::TimeInForce::Fok), std::string_view{"FOK"});
}

HELIX_TEST(quantity_and_price_validation) {
  HELIX_CHECK(!helix::valid_quantity(0));
  HELIX_CHECK(!helix::valid_quantity(-1));
  HELIX_CHECK(helix::valid_quantity(1));
  HELIX_CHECK(!helix::valid_limit_price(0));
  HELIX_CHECK(!helix::valid_limit_price(-5));
  HELIX_CHECK(helix::valid_limit_price(100));
}

HELIX_TEST(command_and_event_defaults_are_safe) {
  helix::NewOrderCommand n;
  HELIX_CHECK_EQ(n.symbol, helix::kInvalidSymbol);
  HELIX_CHECK_EQ(n.quantity, 0);
  helix::Event e;
  HELIX_CHECK_EQ(e.participant, helix::kInvalidParticipant);
}
