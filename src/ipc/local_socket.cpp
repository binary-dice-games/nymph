// MIT License © 2026 Binary Dice Games
/// @file local_socket.cpp
/// @brief Stream sockets on file paths (see local_socket.hpp). One file for
///        both systems: Winsock's AF_UNIX takes the same calls as POSIX
///        sockets but for a few names, kept in the guard below.
#include "ipc/local_socket.hpp"
#include "ipc/loopback.hpp"
#include <algorithm>
#include <cstring>
#include <system_error>
#include <utility>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <afunix.h>
#else
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cerrno>
#endif

namespace bdg::nymph::ipc {

namespace {

#if defined(_WIN32)
using native = SOCKET;
const native kNone = INVALID_SOCKET;
constexpr int kShutBoth = SD_BOTH;
constexpr int kShutRead = SD_RECEIVE;
constexpr int kSendFlags = 0;

/// Winsock is started once for the process.
void start_sockets() {
  static const bool started = [] {
    WSADATA data;
    return WSAStartup(MAKEWORD(2, 2), &data) == 0;
  }();
  if (!started) {
    throw ipc_error("cannot use sockets (WSAStartup failed)");
  }
}
void close_native(native s) {
  closesocket(s);
}
std::string last_error() {
  return "Windows socket error " + std::to_string(WSAGetLastError());
}
#else
using native = int;
constexpr native kNone = -1;
constexpr int kShutBoth = SHUT_RDWR;
constexpr int kShutRead = SHUT_RD;
#if defined(MSG_NOSIGNAL)
constexpr int kSendFlags = MSG_NOSIGNAL; // a closed peer is an error, not SIGPIPE
#else
constexpr int kSendFlags = 0;
#endif
void start_sockets() {}
void close_native(native s) {
  ::close(s);
}
std::string last_error() {
  return std::generic_category().message(errno);
}
#endif

native as_native(intptr_t h) {
  return h == local_stream::kInvalid ? kNone : static_cast<native>(h);
}
intptr_t as_handle(native s) {
  return s == kNone ? local_stream::kInvalid : static_cast<intptr_t>(s);
}

sockaddr_un address_of(const std::filesystem::path& path) {
  const std::string text = path.string();
  if (text.size() > kMaxSocketPath) {
    throw ipc_error("the socket path is too long (" + std::to_string(text.size()) + " bytes): " + text);
  }
  sockaddr_un a{};
  a.sun_family = AF_UNIX;
  std::memcpy(a.sun_path, text.c_str(), text.size() + 1);
  return a;
}

native new_socket() {
  start_sockets();
  const native s = ::socket(AF_UNIX, SOCK_STREAM, 0);
  if (s == kNone) {
    throw ipc_error("cannot make a socket: " + last_error());
  }
#if defined(SO_NOSIGPIPE)
  const int on = 1;
  ::setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on)); // macOS: no SIGPIPE
#endif
  return s;
}

/// Whether something accepts connections at `path`.
bool answers(const std::filesystem::path& path) {
  try {
    local_stream probe = local_stream::connect(path);
    return probe.valid();
  } catch (const ipc_error&) {
    return false;
  }
}

} // namespace

// ── local_stream ─────────────────────────────────────────────────────────────

local_stream::~local_stream() {
  close();
}

local_stream::local_stream(local_stream&& other) noexcept
    : handle_(std::exchange(other.handle_, kInvalid)), buffer_(std::move(other.buffer_)) {}

local_stream& local_stream::operator=(local_stream&& other) noexcept {
  if (this != &other) {
    close();
    handle_ = std::exchange(other.handle_, kInvalid);
    buffer_ = std::move(other.buffer_);
  }
  return *this;
}

void local_stream::close() {
  if (handle_ != kInvalid) {
    close_native(as_native(handle_));
    handle_ = kInvalid;
  }
}

local_stream local_stream::connect(const std::filesystem::path& path) {
  const sockaddr_un a = address_of(path);
  const native s = new_socket();
  if (::connect(s, reinterpret_cast<const sockaddr*>(&a), sizeof(a)) != 0) {
    const std::string why = last_error();
    close_native(s);
    throw ipc_error("cannot connect to " + path.string() + ": " + why);
  }
  return local_stream(as_handle(s));
}

bool local_stream::read_line(std::string& line) {
  while (true) {
    const size_t nl = buffer_.find('\n');
    if (nl != std::string::npos) {
      line = buffer_.substr(0, nl);
      buffer_.erase(0, nl + 1);
      if (!line.empty() && line.back() == '\r') {
        line.pop_back();
      }
      return true;
    }
    if (handle_ == kInvalid) {
      return false;
    }
    char chunk[4096];
    const auto n = ::recv(as_native(handle_), chunk, sizeof(chunk), 0);
    if (n <= 0) {
      return false; // the end, or the stream was shut down
    }
    buffer_.append(chunk, static_cast<size_t>(n));
  }
}

bool local_stream::read_some(std::string& out) {
  if (!buffer_.empty()) {
    out += buffer_;
    buffer_.clear();
    return true;
  }
  if (handle_ == kInvalid) {
    return false;
  }
  char chunk[16384];
  const auto n = ::recv(as_native(handle_), chunk, sizeof(chunk), 0);
  if (n <= 0) {
    return false;
  }
  out.append(chunk, static_cast<size_t>(n));
  return true;
}

bool local_stream::write(std::string_view data) {
  while (!data.empty()) {
    if (handle_ == kInvalid) {
      return false;
    }
    const int part = static_cast<int>(std::min<size_t>(data.size(), 1u << 20));
    const auto n = ::send(as_native(handle_), data.data(), part, kSendFlags);
    if (n <= 0) {
      return false;
    }
    data.remove_prefix(static_cast<size_t>(n));
  }
  return true;
}

void local_stream::shutdown() {
  if (handle_ != kInvalid) {
    ::shutdown(as_native(handle_), kShutBoth);
  }
}

void local_stream::shutdown_reads() {
  if (handle_ != kInvalid) {
    ::shutdown(as_native(handle_), kShutRead);
  }
}

// ── local_listener ───────────────────────────────────────────────────────────

local_listener::local_listener(std::filesystem::path path) : path_(std::move(path)) {
  const sockaddr_un a = address_of(path_);
  std::error_code ec;
  if (std::filesystem::exists(path_, ec)) {
    if (answers(path_)) {
      throw ipc_error("another process listens at " + path_.string());
    }
    std::filesystem::remove(path_, ec); // left by a process that is gone
  }
  const native s = new_socket();
  if (::bind(s, reinterpret_cast<const sockaddr*>(&a), sizeof(a)) != 0 || ::listen(s, 8) != 0) {
    const std::string why = last_error();
    close_native(s);
    throw ipc_error("cannot listen at " + path_.string() + ": " + why);
  }
  handle_ = as_handle(s);
}

local_listener::~local_listener() {
  stop();
  if (handle_ != local_stream::kInvalid) {
    close_native(as_native(handle_));
  }
  std::error_code ec;
  std::filesystem::remove(path_, ec);
}

std::optional<local_stream> local_listener::accept() {
  while (!stopped_.load()) {
    const native s = ::accept(as_native(handle_), nullptr, nullptr);
    if (stopped_.load()) {
      if (s != kNone) {
        close_native(s); // stop()'s own connection
      }
      return std::nullopt;
    }
    if (s != kNone) {
      return local_stream(as_handle(s));
    }
#if !defined(_WIN32)
    if (errno == EINTR) {
      continue;
    }
#endif
    return std::nullopt;
  }
  return std::nullopt;
}

void local_listener::stop() {
  if (stopped_.exchange(true)) {
    return;
  }
  // A blocked accept() returns for a connection on every system (shutting a
  // listening socket down does not wake it on macOS or Windows).
  try {
    local_stream poke = local_stream::connect(path_);
  } catch (const ipc_error&) {
  }
}

// ── Loopback TCP ─────────────────────────────────────────────────────────────

namespace {

sockaddr_in loopback_address(uint16_t port) {
  sockaddr_in a{};
  a.sin_family = AF_INET;
  a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  a.sin_port = htons(port);
  return a;
}

native new_tcp_socket() {
  start_sockets();
  const native s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (s == kNone) {
    throw ipc_error("cannot make a socket: " + last_error());
  }
#if defined(SO_NOSIGPIPE)
  const int on = 1;
  ::setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
#endif
  const int nodelay = 1; // small JSON messages, answered at once
  ::setsockopt(s, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&nodelay), sizeof(nodelay));
  return s;
}

} // namespace

local_stream loopback_connect(uint16_t port) {
  const sockaddr_in a = loopback_address(port);
  const native s = new_tcp_socket();
  if (::connect(s, reinterpret_cast<const sockaddr*>(&a), sizeof(a)) != 0) {
    const std::string why = last_error();
    close_native(s);
    throw ipc_error("cannot connect to 127.0.0.1:" + std::to_string(port) + ": " + why);
  }
  return local_stream(as_handle(s));
}

loopback_listener::loopback_listener(uint16_t port) {
  const sockaddr_in a = loopback_address(port);
  const native s = new_tcp_socket();
#if defined(_WIN32)
  // Another process may not take the port over while it is ours.
  const int exclusive = 1;
  ::setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&exclusive), sizeof(exclusive));
#endif
  if (::bind(s, reinterpret_cast<const sockaddr*>(&a), sizeof(a)) != 0 || ::listen(s, 8) != 0) {
    const std::string why = last_error();
    close_native(s);
    throw ipc_error("cannot listen on 127.0.0.1: " + why);
  }
  sockaddr_in bound{};
  socklen_t size = sizeof(bound);
  if (::getsockname(s, reinterpret_cast<sockaddr*>(&bound), &size) != 0) {
    const std::string why = last_error();
    close_native(s);
    throw ipc_error("cannot read the listening port: " + why);
  }
  port_ = ntohs(bound.sin_port);
  handle_ = as_handle(s);
}

loopback_listener::~loopback_listener() {
  stop();
  close();
}

void loopback_listener::close() {
  if (handle_ != local_stream::kInvalid) {
    close_native(as_native(handle_));
    handle_ = local_stream::kInvalid;
  }
}

std::optional<local_stream> loopback_listener::accept() {
  while (!stopped_.load() && handle_ != local_stream::kInvalid) {
    const native s = ::accept(as_native(handle_), nullptr, nullptr);
    if (stopped_.load()) {
      if (s != kNone) {
        close_native(s);
      }
      return std::nullopt;
    }
    if (s != kNone) {
      return local_stream(as_handle(s));
    }
#if !defined(_WIN32)
    if (errno == EINTR) {
      continue;
    }
#endif
    return std::nullopt;
  }
  return std::nullopt;
}

void loopback_listener::stop() {
  if (stopped_.exchange(true)) {
    return;
  }
  try {
    local_stream poke = loopback_connect(port_); // wakes a blocked accept()
  } catch (const ipc_error&) {
  }
}

} // namespace bdg::nymph::ipc
