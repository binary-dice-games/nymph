// MIT License © 2026 Binary Dice Games
/// @file websocket.hpp
/// @brief The server side of RFC 6455 that the web bridge needs and no more
///        (spec/17_agent_connectors/DESIGN.md section 7.2): the upgrade,
///        masked client frames, text messages up to 16 MB (fragments
///        joined), ping / pong and close. No extensions, no binary messages,
///        no TLS (loopback only).
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include "ipc/local_socket.hpp"

namespace bdg::nymph::mcp {

/// @brief The HTTP request that asks for the upgrade.
struct ws_request {
  std::string method;
  std::string path;
  std::string host; ///< `Host`, as sent
  std::string origin; ///< `Origin`; empty when absent (not a browser)
  std::string key; ///< `Sec-WebSocket-Key`
  std::string upgrade; ///< `Upgrade`, lower case
  std::string connection; ///< `Connection`, lower case
  std::string version; ///< `Sec-WebSocket-Version`
};

/// @brief The `Sec-WebSocket-Accept` value for a client's key.
std::string ws_accept_value(std::string_view key);

/// @brief An open WebSocket connection, server side.
///
/// read_message() runs on one thread (the reader); send_text(), close() and
/// shutdown() may be called from any thread.
class websocket {
 public:
  /// The largest message accepted (and sent).
  static constexpr size_t kMaxMessage = 16u * 1024u * 1024u;

  /// @brief Takes a connection that has not been upgraded yet.
  explicit websocket(ipc::local_stream stream);
  websocket(const websocket&) = delete;
  websocket& operator=(const websocket&) = delete;

  /// @brief Reads the HTTP upgrade request and, when it is a valid
  ///        WebSocket upgrade to `path` that `allow` accepts, answers 101.
  ///        Otherwise answers 400 (malformed) or 403 (refused by `allow`).
  /// @param allow Checks `Host` and `Origin` (DESIGN.md section 7.2).
  /// @return Whether the connection is now a WebSocket.
  bool accept_upgrade(std::string_view path, const std::function<bool(const ws_request&)>& allow);

  /// @brief Reads the next text message, answering pings and a close.
  /// @return false once the connection is closed, broken or refused (a
  ///         binary, unmasked or oversized message closes it).
  bool read_message(std::string& text);

  /// @brief Sends one text message. @return false when it cannot be sent.
  bool send_text(std::string_view text);

  /// @brief Sends a close frame (once).
  void close(uint16_t code = 1000);

  /// @brief Ends the connection now: a read blocked on another thread
  ///        returns false.
  void shutdown();

 private:
  bool fill(size_t n);
  bool send_frame(uint8_t opcode, std::string_view payload);

  ipc::local_stream stream_;
  std::string in_; ///< Bytes read, not yet framed.
  std::mutex send_mtx_;
  bool close_sent_ = false;
};

} // namespace bdg::nymph::mcp
