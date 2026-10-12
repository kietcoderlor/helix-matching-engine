#include "helix/replay.hpp"

#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "usage: helix_replay <tape> [golden-out]\n";
    return 2;
  }
  try {
    const auto result = helix::replay_tape_file(argv[1]);
    if (argc >= 3) {
      std::ofstream out(argv[2]);
      for (const auto& line : result.lines) {
        out << line << '\n';
      }
    } else {
      for (const auto& line : result.lines) {
        std::cout << line << '\n';
      }
    }
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
}
