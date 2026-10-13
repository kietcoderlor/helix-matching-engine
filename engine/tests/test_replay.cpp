#include "helix/replay.hpp"
#include "test_harness.hpp"

#include <fstream>
#include <sstream>
#include <string>

using namespace helix;

HELIX_TEST(replay_tape_is_deterministic) {
  constexpr std::string_view tape = R"(
SYMBOL 1 TEST
NEW 1 1 1 SELL LIMIT GTC 100 5
NEW 2 1 1 BUY LIMIT GTC 100 3
CANCEL 1 1
)";
  const auto a = replay_tape(tape);
  const auto b = replay_tape(tape);
  HELIX_CHECK_EQ(a.lines.size(), b.lines.size());
  for (std::size_t i = 0; i < a.lines.size(); ++i) {
    HELIX_CHECK_EQ(a.lines[i], b.lines[i]);
  }
  HELIX_CHECK(a.lines.size() >= 4);
  HELIX_CHECK(a.lines[0].find("ACCEPTED") != std::string::npos);
  bool saw_trade = false;
  bool saw_cancel = false;
  for (const auto& line : a.lines) {
    if (line.find("TRADE") != std::string::npos) {
      saw_trade = true;
    }
    if (line.find("CANCELED") != std::string::npos) {
      saw_cancel = true;
    }
  }
  HELIX_CHECK(saw_trade);
  HELIX_CHECK(saw_cancel);
}

HELIX_TEST(replay_unknown_op_fails) {
  bool threw = false;
  try {
    replay_tape("SYMBOL 1 TEST\nNOPE\n");
  } catch (const std::runtime_error&) {
    threw = true;
  }
  HELIX_CHECK(threw);
}

HELIX_TEST(replay_cross_tape_matches_golden) {
  const std::string dir = HELIX_TAPES_DIR;
  const auto got = replay_tape_file(dir + "/cross.tape");
  std::ifstream in(dir + "/cross.golden");
  HELIX_CHECK(in.good());
  std::string line;
  std::size_t i = 0;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    HELIX_CHECK(i < got.lines.size());
    HELIX_CHECK_EQ(got.lines[i], line);
    ++i;
  }
  HELIX_CHECK_EQ(i, got.lines.size());
}

HELIX_TEST(stringify_event_includes_participant) {
  Event e;
  e.participant = 9;
  e.payload = PublicTradeEvent{1, 1, 10, 2, Side::Buy};
  const auto s = stringify_event(e);
  HELIX_CHECK(s.find("p=9") != std::string::npos);
  HELIX_CHECK(s.find("TRADE") != std::string::npos);
}
