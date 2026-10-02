#include "helix/version.hpp"
#include "test_harness.hpp"

#include <cstring>
#include <string_view>

HELIX_TEST(version_string_matches_header_constants) {
  HELIX_CHECK_EQ(helix::kEngineVersion.major, 0);
  HELIX_CHECK_EQ(helix::kEngineVersion.minor, 1);
  HELIX_CHECK_EQ(helix::kEngineVersion.patch, 0);
  HELIX_CHECK_EQ(std::string_view{helix::version_string()}, std::string_view{"0.1.0"});
  HELIX_CHECK_EQ(helix::kEngineName, std::string_view{"helix-engine"});
}
