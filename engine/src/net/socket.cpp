#include "helix/net/socket.hpp"

#include <stdexcept>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#endif

namespace helix::net {
namespace {

#ifdef _WIN32
int last_error() { return WSAGetLastError(); }
bool would_block(int e) { return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS; }
void close_handle(NativeHandle h) { closesocket(static_cast<SOCKET>(h)); }
using SockLen = int;
#else
int last_error() { return errno; }
bool would_block(int e) { return e == EAGAIN || e == EWOULDBLOCK || e == EINPROGRESS; }
void close_handle(NativeHandle h) { ::close(h); }
using SockLen = socklen_t;
#endif

sockaddr_in make_addr(const char* host, std::uint16_t port) {
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  if (host == nullptr || std::string(host) == "0.0.0.0") {
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
  } else if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
    throw SocketError(std::string("inet_pton failed for ") + host);
  }
  return addr;
}

NativeHandle native_socket() {
#ifdef _WIN32
  const SOCKET s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (s == INVALID_SOCKET) {
    throw SocketError("socket() failed");
  }
  return static_cast<NativeHandle>(s);
#else
  const int s = ::socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0) {
    throw SocketError("socket() failed");
  }
  return s;
#endif
}

}  // namespace

SocketError::SocketError(const std::string& m) : std::runtime_error(m) {}

void ensure_initialized() {
#ifdef _WIN32
  struct Wsa {
    Wsa() {
      WSADATA data{};
      if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        throw SocketError("WSAStartup failed");
      }
    }
    ~Wsa() { WSACleanup(); }
  };
  static Wsa wsa;
#endif
}

UniqueSocket::UniqueSocket(NativeHandle handle) noexcept : handle_(handle) {}

UniqueSocket::~UniqueSocket() { close(); }

UniqueSocket::UniqueSocket(UniqueSocket&& other) noexcept : handle_(other.handle_) {
  other.handle_ = kInvalidHandle;
}

UniqueSocket& UniqueSocket::operator=(UniqueSocket&& other) noexcept {
  if (this != &other) {
    close();
    handle_ = other.handle_;
    other.handle_ = kInvalidHandle;
  }
  return *this;
}

NativeHandle UniqueSocket::release() noexcept {
  const NativeHandle h = handle_;
  handle_ = kInvalidHandle;
  return h;
}

void UniqueSocket::close() noexcept {
  if (handle_ != kInvalidHandle) {
    close_handle(handle_);
    handle_ = kInvalidHandle;
  }
}

void UniqueSocket::set_nonblocking() {
#ifdef _WIN32
  u_long mode = 1;
  if (ioctlsocket(static_cast<SOCKET>(handle_), FIONBIO, &mode) != 0) {
    throw SocketError("ioctlsocket FIONBIO failed");
  }
#else
  const int flags = fcntl(handle_, F_GETFL, 0);
  if (flags < 0 || fcntl(handle_, F_SETFL, flags | O_NONBLOCK) < 0) {
    throw SocketError("fcntl O_NONBLOCK failed");
  }
#endif
}

void UniqueSocket::set_reuseaddr() {
  int yes = 1;
#ifdef _WIN32
  if (setsockopt(static_cast<SOCKET>(handle_), SOL_SOCKET, SO_REUSEADDR,
                 reinterpret_cast<const char*>(&yes), sizeof(yes)) != 0) {
    throw SocketError("setsockopt SO_REUSEADDR failed");
  }
#else
  if (setsockopt(handle_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) != 0) {
    throw SocketError("setsockopt SO_REUSEADDR failed");
  }
#endif
}

int UniqueSocket::send_some(std::span<const std::uint8_t> bytes) {
  if (bytes.empty()) {
    return 0;
  }
#ifdef _WIN32
  const int n = ::send(static_cast<SOCKET>(handle_),
                       reinterpret_cast<const char*>(bytes.data()),
                       static_cast<int>(bytes.size()), 0);
  if (n == SOCKET_ERROR) {
    const int e = last_error();
    if (would_block(e)) {
      return -1;
    }
    throw SocketError("send failed");
  }
  return n;
#else
  const ssize_t n = ::send(handle_, bytes.data(), bytes.size(), 0);
  if (n < 0) {
    if (would_block(last_error())) {
      return -1;
    }
    throw SocketError("send failed");
  }
  return static_cast<int>(n);
#endif
}

int UniqueSocket::recv_some(std::span<std::uint8_t> bytes) {
  if (bytes.empty()) {
    return 0;
  }
#ifdef _WIN32
  const int n = ::recv(static_cast<SOCKET>(handle_), reinterpret_cast<char*>(bytes.data()),
                       static_cast<int>(bytes.size()), 0);
  if (n == SOCKET_ERROR) {
    if (would_block(last_error())) {
      return -1;
    }
    throw SocketError("recv failed");
  }
  return n;
#else
  const ssize_t n = ::recv(handle_, bytes.data(), bytes.size(), 0);
  if (n < 0) {
    if (would_block(last_error())) {
      return -1;
    }
    throw SocketError("recv failed");
  }
  return static_cast<int>(n);
#endif
}

UniqueSocket tcp_listen(const char* host, std::uint16_t port) {
  ensure_initialized();
  UniqueSocket sock(native_socket());
  sock.set_reuseaddr();
  auto addr = make_addr(host, port);
#ifdef _WIN32
  if (bind(static_cast<SOCKET>(sock.get()), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    throw SocketError("bind failed");
  }
  if (listen(static_cast<SOCKET>(sock.get()), SOMAXCONN) != 0) {
    throw SocketError("listen failed");
  }
#else
  if (bind(sock.get(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    throw SocketError("bind failed");
  }
  if (listen(sock.get(), SOMAXCONN) != 0) {
    throw SocketError("listen failed");
  }
#endif
  sock.set_nonblocking();
  return sock;
}

std::uint16_t local_port(const UniqueSocket& socket) {
  sockaddr_in addr{};
  SockLen len = sizeof(addr);
#ifdef _WIN32
  if (getsockname(static_cast<SOCKET>(socket.get()), reinterpret_cast<sockaddr*>(&addr), &len) != 0) {
    throw SocketError("getsockname failed");
  }
#else
  if (getsockname(socket.get(), reinterpret_cast<sockaddr*>(&addr), &len) != 0) {
    throw SocketError("getsockname failed");
  }
#endif
  return ntohs(addr.sin_port);
}

UniqueSocket tcp_connect(const char* host, std::uint16_t port) {
  ensure_initialized();
  UniqueSocket sock(native_socket());
  auto addr = make_addr(host, port);
#ifdef _WIN32
  if (connect(static_cast<SOCKET>(sock.get()), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    throw SocketError("connect failed");
  }
#else
  if (connect(sock.get(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    throw SocketError("connect failed");
  }
#endif
  sock.set_nonblocking();
  return sock;
}

UniqueSocket tcp_accept(UniqueSocket& listener) {
#ifdef _WIN32
  const SOCKET s = accept(static_cast<SOCKET>(listener.get()), nullptr, nullptr);
  if (s == INVALID_SOCKET) {
    if (would_block(last_error())) {
      return UniqueSocket{};
    }
    throw SocketError("accept failed");
  }
  UniqueSocket sock(static_cast<NativeHandle>(s));
#else
  const int s = accept(listener.get(), nullptr, nullptr);
  if (s < 0) {
    if (would_block(last_error())) {
      return UniqueSocket{};
    }
    throw SocketError("accept failed");
  }
  UniqueSocket sock(s);
#endif
  sock.set_nonblocking();
  return sock;
}

}  // namespace helix::net
