#pragma once

#include "helix/event.hpp"
#include "helix/matching_engine.hpp"

#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

namespace helix {

std::string stringify_event(const Event& event);

struct ReplayResult {
  std::vector<Event> events;
  std::vector<std::string> lines;
};

// Scripted tape → engine. Throws std::runtime_error on parse errors.
ReplayResult replay_tape(std::string_view text);

ReplayResult replay_tape_file(const std::string& path);

}  // namespace helix
