// MIT License © 2026 Binary Dice Games
/// @file loopback.hpp
/// @brief A TCP listener and connections on 127.0.0.1 only: how the web app
///        in a browser tab reaches `nymph mcp`
///        (spec/17_agent_connectors/DESIGN.md section 7.2). The streams are
///        `local_stream`s, so reads, writes and shutdown work as for the
///        editors' file sockets.
#pragma once

#include <atomic>
#include <cstdint>
#include <optional>
#include "ipc/local_socket.hpp"

namespace bdg::nymph::ipc {

/// @brief Connects to 127.0.0.1:`port`.
/// @throws ipc_error when nothing listens there.
local_stream loopback_connect(uint16_t port);

/// @brief A listening TCP socket bound to 127.0.0.1 (never another
///        address). accept() blocks; stop() from another thread makes it
///        return nullopt.
class loopback_listener {
 public:
  /// @brief Listens on 127.0.0.1:`port`; 0 picks a free port (port()).
  /// @throws ipc_error when the socket cannot be made or bound.
  explicit loopback_listener(uint16_t port = 0);
  /// Stops and closes the socket.
  ~loopback_listener();
  loopback_listener(const loopback_listener&) = delete;
  loopback_listener& operator=(const loopback_listener&) = delete;

  /// @brief The next connection; nullopt once stop() was called.
  std::optional<local_stream> accept();

  /// @brief Makes accept() return nullopt, now and from then on (any thread).
  void stop();

  /// @brief Closes the socket, so connections are refused from now on. Call
  ///        it on the thread that called accept(), after accept() returned
  ///        nullopt (or when no thread accepts).
  void close();

  /// @brief The port it listens on.
  uint16_t port() const {
    return port_;
  }

 private:
  intptr_t handle_ = local_stream::kInvalid;
  uint16_t port_ = 0;
  std::atomic<bool> stopped_{false};
};

} // namespace bdg::nymph::ipc
