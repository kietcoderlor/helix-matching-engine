#pragma once

#include <cstdint>
#include <string_view>

namespace helix {

struct Version {
  std::uint16_t major;
  std::uint16_t minor;
  std::uint16_t patch;
};

constexpr Version kEngineVersion{0, 1, 0};

constexpr std::string_view kEngineName = "helix-engine";

// Human-readable "major.minor.patch".
const char* version_string() noexcept;

}  // namespace helix
