#include "helix/engine_service.hpp"
#include "helix/version.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

namespace {

std::uint16_t env_u16(const char* name, std::uint16_t fallback) {
  const char* v = std::getenv(name);
  if (v == nullptr || *v == '\0') {
    return fallback;
  }
  return static_cast<std::uint16_t>(std::stoi(v));
}

const char* env_s(const char* name, const char* fallback) {
  const char* v = std::getenv(name);
  return (v == nullptr || *v == '\0') ? fallback : v;
}

}  // namespace

int main() {
  std::cout << helix::kEngineName << ' ' << helix::version_string() << '\n';
  std::cout << "simulated exchange — not a real market, not production-ready\n";

  helix::EngineService::Config cfg;
  cfg.host = env_s("HELIX_BIND", "127.0.0.1");
  cfg.port = env_u16("HELIX_PORT", 9001);

  helix::EngineService svc(cfg);
  svc.add_symbol(1, "AAPL");
  svc.add_symbol(2, "MSFT");
  svc.add_symbol(3, "GOOG");
  svc.start();
  std::cout << "listening on " << cfg.host << ':' << svc.port() << '\n';
  std::cout << "symbols: 1=AAPL 2=MSFT 3=GOOG\n";

  while (true) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}
