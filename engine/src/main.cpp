#include "helix/version.hpp"

#include <iostream>

int main() {
  std::cout << helix::kEngineName << ' ' << helix::version_string() << '\n';
  std::cout << "simulated exchange — not a real market, not production-ready\n";
  return 0;
}
