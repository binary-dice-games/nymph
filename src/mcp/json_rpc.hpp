// MIT License © 2026 Binary Dice Games
/// @file json_rpc.hpp
/// @brief JSON-RPC 2.0 messages for `nymph mcp`: one message per line of
///        standard input and output
///        (spec/06_claude_integration/DESIGN.md section 4.1).
#pragma once

#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <string_view>

namespace bdg::nymph::mcp {

using json = nlohmann::json;

/// @brief The JSON-RPC 2.0 error codes the server returns.
enum class rpc_code : int {
  parse_error = -32700, ///< the line is not JSON
  invalid_request = -32600, ///< JSON, but not a valid request (or a batch)
  method_not_found = -32601, ///< no such method
  invalid_params = -32602, ///< the method's params are wrong
  internal_error = -32603, ///< the server failed
  resource_not_found = -32002, ///< MCP: resources/read of an unknown URI
};

/// @brief A request that gets an error response instead of a result.
class rpc_error : public std::runtime_error {
 public:
  rpc_error(rpc_code code, const std::string& message) : std::runtime_error(message), code_(code) {}
  /// @brief The JSON-RPC error code.
  rpc_code code() const {
    return code_;
  }

 private:
  rpc_code code_;
};

/// @brief One incoming message.
struct rpc_message {
  enum class kind {
    request, ///< has a method and an id: needs a response
    notification, ///< has a method and no id: no response
    response, ///< has an id and a result or error (a reply to the server)
  };
  kind type = kind::request;
  json id; ///< string or integer; null for notifications
  std::string method; ///< empty for responses
  json params; ///< object, array, or null when absent
};

/// @brief Parses one line into a message.
/// @param line One line of input, without its line ending.
/// @param[out] id The request's id as soon as it is known, so an error
///        response can carry it; null otherwise.
/// @throws rpc_error parse_error for text that is not JSON (or not UTF-8),
///         invalid_request for JSON that is not a JSON-RPC 2.0 message,
///         including a batch array (MCP has no batches).
rpc_message parse_message(std::string_view line, json& id);

/// @brief A success response: `{"jsonrpc":"2.0","id":id,"result":result}`.
json make_result(const json& id, json result);

/// @brief An error response; `id` is null when the request's id is unknown.
json make_error(const json& id, rpc_code code, const std::string& message);

/// @brief A message as one line of text, without the line ending. Strings
///        with invalid UTF-8 are written with U+FFFD in its place.
std::string to_line(const json& message);

} // namespace bdg::nymph::mcp
