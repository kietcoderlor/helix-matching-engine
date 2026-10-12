#include "helix/replay.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace helix {
namespace {

std::string side_s(Side s) { return std::string(to_string(s)); }

std::string stringify_payload(const EventPayload& payload) {
  return std::visit(
      [](const auto& p) -> std::string {
        using T = std::decay_t<decltype(p)>;
        std::ostringstream os;
        if constexpr (std::is_same_v<T, OrderAcceptedEvent>) {
          os << "ACCEPTED eid=" << p.exchange_order_id << " cid=" << p.client_order_id
             << " " << side_s(p.side) << " px=" << p.price << " qty=" << p.quantity;
        } else if constexpr (std::is_same_v<T, OrderRejectedEvent>) {
          os << "REJECTED cid=" << p.client_order_id << " reason=" << static_cast<int>(p.reason);
        } else if constexpr (std::is_same_v<T, OrderCanceledEvent>) {
          os << "CANCELED eid=" << p.exchange_order_id << " qty=" << p.canceled
             << " reason=" << static_cast<int>(p.reason);
        } else if constexpr (std::is_same_v<T, OrderReplacedEvent>) {
          os << "REPLACED eid=" << p.exchange_order_id << " px=" << p.price << " qty=" << p.quantity
             << " lost=" << (p.priority_lost ? 1 : 0);
        } else if constexpr (std::is_same_v<T, PrivateFillEvent>) {
          os << "FILL eid=" << p.exchange_order_id << " " << side_s(p.side)
             << " px=" << p.price << " qty=" << p.quantity << " rem=" << p.remaining
             << " liq=" << (p.liquidity == Liquidity::Maker ? "M" : "T");
        } else if constexpr (std::is_same_v<T, PublicTradeEvent>) {
          os << "TRADE tid=" << p.trade_id << " px=" << p.price << " qty=" << p.quantity
             << " ag=" << side_s(p.aggressor);
        } else if constexpr (std::is_same_v<T, BookDeltaEvent>) {
          os << "DELTA " << side_s(p.side) << " px=" << p.price << " qty=" << p.quantity
             << " n=" << p.order_count;
        } else if constexpr (std::is_same_v<T, ErrorEvent>) {
          os << "ERROR reason=" << static_cast<int>(p.reason);
        } else {
          os << "OTHER";
        }
        return os.str();
      },
      payload);
}

struct Tokens {
  std::vector<std::string> w;
  explicit Tokens(const std::string& line) {
    std::istringstream is(line);
    std::string t;
    while (is >> t) {
      w.push_back(std::move(t));
    }
  }
  const std::string& at(std::size_t i) const {
    if (i >= w.size()) {
      throw std::runtime_error("tape line missing token");
    }
    return w[i];
  }
  std::size_t size() const { return w.size(); }
};

Side parse_side(const std::string& s) {
  if (s == "BUY") return Side::Buy;
  if (s == "SELL") return Side::Sell;
  throw std::runtime_error("bad side: " + s);
}

OrderType parse_type(const std::string& s) {
  if (s == "LIMIT") return OrderType::Limit;
  if (s == "MARKET") return OrderType::Market;
  throw std::runtime_error("bad type: " + s);
}

TimeInForce parse_tif(const std::string& s) {
  if (s == "GTC") return TimeInForce::Gtc;
  if (s == "IOC") return TimeInForce::Ioc;
  if (s == "FOK") return TimeInForce::Fok;
  throw std::runtime_error("bad tif: " + s);
}

long long parse_ll(const std::string& s) { return std::stoll(s); }

}  // namespace

std::string stringify_event(const Event& event) {
  std::ostringstream os;
  os << "p=" << event.participant << " " << stringify_payload(event.payload);
  return os.str();
}

ReplayResult replay_tape(std::string_view text) {
  MatchingEngine engine;
  ReplayResult result;
  TimestampNs ts = 0;
  std::istringstream in{std::string(text)};
  std::string line;
  int lineno = 0;
  while (std::getline(in, line)) {
    ++lineno;
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    auto first = line.find_first_not_of(" \t");
    if (first == std::string::npos || line[first] == '#') {
      continue;
    }
    Tokens tok(line);
    const std::string& op = tok.at(0);
    try {
      if (op == "SYMBOL") {
        engine.add_symbol(static_cast<SymbolId>(parse_ll(tok.at(1))), tok.at(2));
      } else if (op == "TS") {
        ts = static_cast<TimestampNs>(parse_ll(tok.at(1)));
      } else if (op == "NEW") {
        Command c;
        c.participant = static_cast<ParticipantId>(parse_ll(tok.at(1)));
        c.connection = c.participant;
        c.ts_ns = ++ts;
        NewOrderCommand n;
        n.client_order_id = static_cast<ClientOrderId>(parse_ll(tok.at(2)));
        n.symbol = static_cast<SymbolId>(parse_ll(tok.at(3)));
        n.side = parse_side(tok.at(4));
        n.type = parse_type(tok.at(5));
        n.tif = parse_tif(tok.at(6));
        n.price = static_cast<PriceTicks>(parse_ll(tok.at(7)));
        n.quantity = static_cast<Quantity>(parse_ll(tok.at(8)));
        c.payload = n;
        auto evs = engine.execute(c);
        for (auto& e : evs) {
          result.lines.push_back(stringify_event(e));
          result.events.push_back(std::move(e));
        }
      } else if (op == "CANCEL") {
        Command c;
        c.participant = static_cast<ParticipantId>(parse_ll(tok.at(1)));
        c.connection = c.participant;
        c.ts_ns = ++ts;
        CancelOrderCommand n;
        n.exchange_order_id = static_cast<OrderId>(parse_ll(tok.at(2)));
        n.client_order_id = tok.size() > 3 ? static_cast<ClientOrderId>(parse_ll(tok.at(3))) : 0;
        c.payload = n;
        auto evs = engine.execute(c);
        for (auto& e : evs) {
          result.lines.push_back(stringify_event(e));
          result.events.push_back(std::move(e));
        }
      } else if (op == "MODIFY") {
        Command c;
        c.participant = static_cast<ParticipantId>(parse_ll(tok.at(1)));
        c.connection = c.participant;
        c.ts_ns = ++ts;
        ModifyOrderCommand n;
        n.exchange_order_id = static_cast<OrderId>(parse_ll(tok.at(2)));
        n.new_price = static_cast<PriceTicks>(parse_ll(tok.at(3)));
        n.new_quantity = static_cast<Quantity>(parse_ll(tok.at(4)));
        c.payload = n;
        auto evs = engine.execute(c);
        for (auto& e : evs) {
          result.lines.push_back(stringify_event(e));
          result.events.push_back(std::move(e));
        }
      } else {
        throw std::runtime_error("unknown op " + op);
      }
    } catch (const std::exception& ex) {
      throw std::runtime_error("tape line " + std::to_string(lineno) + ": " + ex.what());
    }
  }
  return result;
}

ReplayResult replay_tape_file(const std::string& path) {
  std::ifstream in(path);
  if (!in) {
    throw std::runtime_error("cannot open tape " + path);
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  return replay_tape(ss.str());
}

}  // namespace helix
