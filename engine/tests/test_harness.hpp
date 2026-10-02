#pragma once

// Minimal C++20 test harness. Tests register themselves via static
// constructors so each translation unit can add cases without a central
// list. Keep assertions in the test files; this header only runs them.

#include <cstdio>
#include <exception>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace helix::test {

struct TestCase {
  const char* name;
  void (*fn)();
};

inline std::vector<TestCase>& registry() {
  static std::vector<TestCase> cases;
  return cases;
}

struct Register {
  Register(const char* name, void (*fn)()) {
    registry().push_back(TestCase{name, fn});
  }
};

struct CheckFailure : std::exception {
  explicit CheckFailure(std::string m) : message(std::move(m)) {}
  const char* what() const noexcept override { return message.c_str(); }
  std::string message;
};

inline int run_all() {
  int failed = 0;
  const auto& cases = registry();
  std::fprintf(stdout, "[helix-test] running %zu case(s)\n", cases.size());
  for (const auto& tc : cases) {
    try {
      tc.fn();
      std::fprintf(stdout, "  PASS  %s\n", tc.name);
    } catch (const CheckFailure& ex) {
      std::fprintf(stderr, "  FAIL  %s\n         %s\n", tc.name, ex.what());
      ++failed;
    } catch (const std::exception& ex) {
      std::fprintf(stderr, "  FAIL  %s\n         exception: %s\n", tc.name, ex.what());
      ++failed;
    } catch (...) {
      std::fprintf(stderr, "  FAIL  %s\n         unknown exception\n", tc.name);
      ++failed;
    }
  }
  std::fprintf(stdout, "[helix-test] %zu passed, %d failed\n",
               cases.size() - static_cast<std::size_t>(failed), failed);
  return failed == 0 ? 0 : 1;
}

}  // namespace helix::test

#define HELIX_TEST(name)                                                       \
  static void name();                                                          \
  static const helix::test::Register HELIX_REG_##name{#name, &name};           \
  static void name()

#define HELIX_CHECK(cond)                                                      \
  do {                                                                         \
    if (!(cond)) {                                                             \
      throw helix::test::CheckFailure(                                         \
          std::string(__FILE__) + ":" + std::to_string(__LINE__) +             \
          " check failed: " #cond);                                            \
    }                                                                          \
  } while (0)

#define HELIX_CHECK_EQ(a, b)                                                   \
  do {                                                                         \
    const auto helix_lhs = (a);                                                \
    const auto helix_rhs = (b);                                                \
    if (!(helix_lhs == helix_rhs)) {                                           \
      throw helix::test::CheckFailure(                                         \
          std::string(__FILE__) + ":" + std::to_string(__LINE__) +             \
          " check failed: " #a " == " #b);                                     \
    }                                                                          \
  } while (0)
