// MIT License © 2026 Binary Dice Games
/// @file mcp_session.hpp
/// @brief The MCP session of `nymph mcp`: lifecycle, protocol version
///        negotiation and method dispatch
///        (spec/06_claude_integration/DESIGN.md section 4.1).
#pragma once

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include "mcp/json_rpc.hpp"
#include "mcp/server_options.hpp"
#include "mcp/tools.hpp"

namespace bdg::nymph::mcp {

/// @brief The MCP protocol revisions the server speaks, newest first.
std::span<const std::string_view> supported_protocol_versions();

/// @brief One client connection: answers requests in arrival order.
///
/// Before `initialize` only `initialize` and `ping` are answered; other
/// known methods get invalid_request. Unknown methods always get
/// method_not_found, so a client probing a newer revision first (Claude
/// Code sends `server/discover`) falls back to `initialize`. Not
/// thread-safe: one message at a time.
class session {
 public:
  explicit session(server_options options = {});
  ~session();
  session(const session&) = delete;
  session& operator=(const session&) = delete;

  /// @brief Handles one message.
  /// @return The response to a request (a result or an error); nullopt for
  ///         notifications and for responses from the client. Never throws:
  ///         a failure becomes an error response.
  std::optional<json> handle(const rpc_message& message);

  /// @brief Whether `initialize` succeeded.
  bool initialized() const {
    return !protocol_version_.empty();
  }
  /// @brief The negotiated protocol revision; empty before `initialize`.
  const std::string& protocol_version() const {
    return protocol_version_;
  }
  /// @brief The session's tools (tests add their own).
  tool_registry& tools() {
    return *tools_;
  }

 private:
  json dispatch(const std::string& method, const json& params);
  json initialize(const json& params);

  std::unique_ptr<tool_registry> tools_;
  std::shared_ptr<class web_bridge> web_; ///< nymph mcp only
  std::string protocol_version_;
};

} // namespace bdg::nymph::mcp
