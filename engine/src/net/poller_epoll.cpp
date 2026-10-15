#include "helix/net/poller.hpp"

#ifndef _WIN32

#include <sys/epoll.h>
#include <unistd.h>

#include <stdexcept>
#include <vector>

namespace helix::net {
namespace {

int to_epoll(Interest interest) {
  int ev = 0;
  if (has(interest, Interest::Read)) {
    ev |= EPOLLIN;
  }
  if (has(interest, Interest::Write)) {
    ev |= EPOLLOUT;
  }
  return ev;
}

class EpollPoller final : public Poller {
 public:
  EpollPoller() {
    fd_ = epoll_create1(EPOLL_CLOEXEC);
    if (fd_ < 0) {
      throw SocketError("epoll_create1 failed");
    }
  }

  ~EpollPoller() override {
    if (fd_ >= 0) {
      ::close(fd_);
    }
  }

  void add(NativeHandle fd, Interest interest) override { ctl(EPOLL_CTL_ADD, fd, interest); }
  void mod(NativeHandle fd, Interest interest) override { ctl(EPOLL_CTL_MOD, fd, interest); }
  void del(NativeHandle fd) override { ctl(EPOLL_CTL_DEL, fd, Interest::None); }

  int wait(std::span<Ready> out, std::chrono::milliseconds timeout) override {
    if (out.empty()) {
      return 0;
    }
    std::vector<epoll_event> ev(out.size());
    const int n = epoll_wait(fd_, ev.data(), static_cast<int>(ev.size()),
                             static_cast<int>(timeout.count()));
    if (n < 0) {
      throw SocketError("epoll_wait failed");
    }
    for (int i = 0; i < n; ++i) {
      Ready r;
      r.fd = ev[static_cast<std::size_t>(i)].data.fd;
      const auto mask = ev[static_cast<std::size_t>(i)].events;
      r.readable = (mask & EPOLLIN) != 0;
      r.writable = (mask & EPOLLOUT) != 0;
      r.hangup = (mask & EPOLLHUP) != 0;
      r.error = (mask & EPOLLERR) != 0;
      out[static_cast<std::size_t>(i)] = r;
    }
    return n;
  }

  const char* name() const noexcept override { return "epoll"; }

 private:
  void ctl(int op, NativeHandle fd, Interest interest) {
    epoll_event ev{};
    ev.events = static_cast<std::uint32_t>(to_epoll(interest));
    ev.data.fd = fd;
    if (epoll_ctl(fd_, op, fd, op == EPOLL_CTL_DEL ? nullptr : &ev) < 0) {
      throw SocketError("epoll_ctl failed");
    }
  }

  int fd_{-1};
};

}  // namespace

std::unique_ptr<Poller> create_poller() {
  ensure_initialized();
  return std::make_unique<EpollPoller>();
}

}  // namespace helix::net

#endif
