// MIT License © 2026 Binary Dice Games
/// @file json_rpc.cpp
/// @brief Parsing and writing JSON-RPC 2.0 messages.
#include "mcp/json_rpc.hpp"

namespace bdg::nymph::mcp {

namespace {

bool valid_id(const json& id) {
  // JSON-RPC allows null and fractional numbers; MCP does not.
  return id.is_string() || id.is_number_integer();
}

} // namespace

rpc_message parse_message(std::string_view line, json& id) {
  id = nullptr;
  json value = json::parse(line, nullptr, false);
  if (value.is_discarded()) {
    throw rpc_error(rpc_code::parse_error, "parse error: the line is not valid JSON");
  }
  if (value.is_array()) {
    throw rpc_error(rpc_code::invalid_request, "batches are not supported");
  }
  if (!value.is_object()) {
    throw rpc_error(rpc_code::invalid_request, "a message must be a JSON object");
  }
  const auto id_it = value.find("id");
  if (id_it != value.end() && valid_id(*id_it)) {
    id = *id_it;
  }
  const auto version = value.find("jsonrpc");
  if (version == value.end() || *version != "2.0") {
    throw rpc_error(rpc_code::invalid_request, "\"jsonrpc\" must be \"2.0\"");
  }
  if (id_it != value.end() && !valid_id(*id_it)) {
    throw rpc_error(rpc_code::invalid_request, "\"id\" must be a string or an integer");
  }

  rpc_message message;
  const auto method = value.find("method");
  if (method == value.end()) {
    if (id_it == value.end() || (!value.contains("result") && !value.contains("error"))) {
      throw rpc_error(rpc_code::invalid_request, "a message needs \"method\", or \"id\" with \"result\" or \"error\"");
    }
    message.type = rpc_message::kind::response;
    message.id = *id_it;
    return message;
  }
  if (!method->is_string()) {
    throw rpc_error(rpc_code::invalid_request, "\"method\" must be a string");
  }
  message.method = method->get<std::string>();
  const auto params = value.find("params");
  if (params != value.end()) {
    if (!params->is_object() && !params->is_array()) {
      throw rpc_error(rpc_code::invalid_request, "\"params\" must be an object or an array");
    }
    message.params = *params;
  }
  if (id_it == value.end()) {
    message.type = rpc_message::kind::notification;
  } else {
    message.type = rpc_message::kind::request;
    message.id = *id_it;
  }
  return message;
}

json make_result(const json& id, json result) {
  return json{{"jsonrpc", "2.0"}, {"id", id}, {"result", std::move(result)}};
}

json make_error(const json& id, rpc_code code, const std::string& message) {
  return json{
      {"jsonrpc", "2.0"},
      {"id", id},
      {"error", {{"code", static_cast<int>(code)}, {"message", message}}},
  };
}

std::string to_line(const json& message) {
  // dump() escapes control characters, so the text never holds a newline.
  return message.dump(-1, ' ', false, json::error_handler_t::replace);
}

} // namespace bdg::nymph::mcp
