#include "helix/matching_engine.hpp"
#include "helix/version.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#define HELIX_STR_(x) #x
#define HELIX_STR(x) HELIX_STR_(x)
#ifdef _MSC_VER
#define HELIX_COMPILER "MSVC " HELIX_STR(_MSC_VER)
#elif defined(__clang__)
#define HELIX_COMPILER "clang"
#elif defined(__GNUC__)
#define HELIX_COMPILER "gcc"
#else
#define HELIX_COMPILER "unknown"
#endif

#ifndef NDEBUG
#define HELIX_BUILD "Debug"
#else
#define HELIX_BUILD "Release (NDEBUG)"
#endif

using SteadyClock = std::chrono::steady_clock;

int main(int argc, char** argv) {
  int n = 100000;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--orders" && i + 1 < argc) {
      n = std::stoi(argv[++i]);
    }
  }

  helix::MatchingEngine::Config cfg;
  cfg.check_invariants = false;
  helix::MatchingEngine engine(cfg);
  engine.add_symbol(1, "AAPL");

  // Seed a deep opposite book so later marketable orders actually match.
  for (int i = 0; i < 1000; ++i) {
    helix::Command c;
    c.participant = 1;
    c.payload = helix::NewOrderCommand{static_cast<helix::ClientOrderId>(i + 1),
                                       1,
                                       helix::Side::Sell,
                                       helix::OrderType::Limit,
                                       helix::TimeInForce::Gtc,
                                       100 + (i % 10),
                                       10};
    engine.execute(c);
  }

  std::vector<std::uint64_t> ns;
  ns.reserve(static_cast<std::size_t>(n));
  const auto wall0 = SteadyClock::now();
  for (int i = 0; i < n; ++i) {
    helix::Command c;
    c.participant = 2;
    c.ts_ns = static_cast<helix::TimestampNs>(i + 1);
    const bool marketable = (i % 4) == 0;
    c.payload = helix::NewOrderCommand{
        static_cast<helix::ClientOrderId>(i + 1),
        1,
        helix::Side::Buy,
        helix::OrderType::Limit,
        marketable ? helix::TimeInForce::Ioc : helix::TimeInForce::Gtc,
        marketable ? 109 : 90,
        1};
    const auto t0 = SteadyClock::now();
    engine.execute(c);
    const auto t1 = SteadyClock::now();
    ns.push_back(static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count()));
  }
  const auto wall1 = SteadyClock::now();
  const double secs = std::chrono::duration<double>(wall1 - wall0).count();

  std::sort(ns.begin(), ns.end());
  const auto pct = [&](double p) {
    const std::size_t i = static_cast<std::size_t>(p * static_cast<double>(ns.size() - 1));
    return ns[i];
  };

  const char* cpu = std::getenv("PROCESSOR_IDENTIFIER");
  const char* host = std::getenv("COMPUTERNAME");
  std::cout << "Helix matching microbenchmark (in-process execute(), no sockets)\n";
  std::cout << "engine: " << helix::kEngineName << ' ' << helix::version_string() << '\n';
  std::cout << "build: " << HELIX_BUILD << '\n';
  std::cout << "compiler: " << HELIX_COMPILER << '\n';
  std::cout << "host: " << (host ? host : "unknown") << '\n';
  std::cout << "cpu: " << (cpu ? cpu : "unknown") << '\n';
  std::cout << "command: helix_bench --orders " << n << '\n';
  std::cout << "orders: " << n << '\n';
  std::cout << "wall_seconds: " << secs << '\n';
  std::cout << "throughput_per_s: " << (secs > 0 ? static_cast<double>(n) / secs : 0) << '\n';
  std::cout << "latency_ns_p50: " << pct(0.50) << '\n';
  std::cout << "latency_ns_p99: " << pct(0.99) << '\n';
  std::cout << "latency_ns_max: " << ns.back() << '\n';
  std::cout << "note: figures are for this run only; do not copy them as claims.\n";
  return 0;
}
