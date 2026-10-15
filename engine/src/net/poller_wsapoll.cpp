#include "helix/net/poller.hpp"

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace helix::net {
namespace {

class WsaPoller final : public Poller {
 public:
  void add(NativeHandle fd, Interest interest) override {
    for (const auto& f : fds_) {
      if (f.fd == static_cast<SOCKET>(fd)) {
        throw SocketError("poller add: duplicate fd");
      }
    }
    WSAPOLLFD p{};
    p.fd = static_cast<SOCKET>(fd);
    p.events = to_events(interest);
    fds_.push_back(p);
  }

  void mod(NativeHandle fd, Interest interest) override {
    auto* p = find(fd);
    if (p == nullptr) {
      throw SocketError("poller mod: unknown fd");
    }
    p->events = to_events(interest);
  }

  void del(NativeHandle fd) override {
    const auto it = std::remove_if(fds_.begin(), fds_.end(), [&](const WSAPOLLFD& p) {
      return p.fd == static_cast<SOCKET>(fd);
    });
    if (it == fds_.end()) {
      throw SocketError("poller del: unknown fd");
    }
    fds_.erase(it, fds_.end());
  }

  int wait(std::span<Ready> out, std::chrono::milliseconds timeout) override {
    if (fds_.empty() || out.empty()) {
      return 0;
    }
    const int n = WSAPoll(fds_.data(), static_cast<ULONG>(fds_.size()),
                          static_cast<INT>(timeout.count()));
    if (n == SOCKET_ERROR) {
      throw SocketError("WSAPoll failed");
    }
    int filled = 0;
    for (const auto& p : fds_) {
      if (p.revents == 0) {
        continue;
      }
      if (filled >= static_cast<int>(out.size())) {
        break;
      }
      Ready r;
      r.fd = static_cast<NativeHandle>(p.fd);
      r.readable = (p.revents & POLLRDNORM) != 0 || (p.revents & POLLIN) != 0;
      r.writable = (p.revents & POLLWRNORM) != 0 || (p.revents & POLLOUT) != 0;
      r.hangup = (p.revents & POLLHUP) != 0;
      r.error = (p.revents & POLLERR) != 0;
      out[static_cast<std::size_t>(filled++)] = r;
    }
    return filled;
  }

  const char* name() const noexcept override { return "WSAPoll"; }

 private:
  static short to_events(Interest interest) {
    short ev = 0;
    if (has(interest, Interest::Read)) {
      ev = static_cast<short>(ev | POLLRDNORM);
    }
    if (has(interest, Interest::Write)) {
      ev = static_cast<short>(ev | POLLWRNORM);
    }
    return ev;
  }

  WSAPOLLFD* find(NativeHandle fd) {
    for (auto& p : fds_) {
      if (p.fd == static_cast<SOCKET>(fd)) {
        return &p;
      }
    }
    return nullptr;
  }

  std::vector<WSAPOLLFD> fds_;
};

}  // namespace

std::unique_ptr<Poller> create_poller() {
  ensure_initialized();
  return std::make_unique<WsaPoller>();
}

}  // namespace helix::net

#endif
