// MIT License © 2026 Binary Dice Games
/// @file mcp_session.cpp
/// @brief The MCP lifecycle and method dispatch.
#include "mcp/mcp_session.hpp"
#include <algorithm>
#include <array>
#include <exception>
#include <utility>

namespace bdg::nymph::mcp {

namespace {

constexpr std::array<std::string_view, 3> kProtocolVersions = {"2025-11-25", "2025-06-18", "2025-03-26"};

/// The methods answered after `initialize`, besides `ping`.
constexpr std::array<std::string_view, 2> kSessionMethods = {"tools/list", "resources/list"};

bool is_session_method(const std::string& method) {
  return std::find(kSessionMethods.begin(), kSessionMethods.end(), method) != kSessionMethods.end();
}

} // namespace

std::span<const std::string_view> supported_protocol_versions() {
  return kProtocolVersions;
}

session::session(server_options options) : options_(options) {}

std::optional<json> session::handle(const rpc_message& message) {
  if (message.type != rpc_message::kind::request) {
    // notifications/initialized, notifications/cancelled (renders take
    // milliseconds) and replies need nothing.
    return std::nullopt;
  }
  try {
    return make_result(message.id, dispatch(message.method, message.params));
  } catch (const rpc_error& e) {
    return make_error(message.id, e.code(), e.what());
  } catch (const std::exception& e) {
    return make_error(message.id, rpc_code::internal_error, std::string("internal error: ") + e.what());
  }
}

json session::dispatch(const std::string& method, const json& params) {
  if (method == "ping") {
    return json::object();
  }
  if (method == "initialize") {
    return initialize(params);
  }
  if (!is_session_method(method)) {
    throw rpc_error(rpc_code::method_not_found, "method not found: " + method);
  }
  if (!initialized()) {
    throw rpc_error(rpc_code::invalid_request, "the session is not initialized: send initialize first");
  }
  if (method == "tools/list") {
    return json{{"tools", json::array()}};
  }
  return json{{"resources", json::array()}};
}

json session::initialize(const json& params) {
  if (initialized()) {
    throw rpc_error(rpc_code::invalid_request, "the session is already initialized");
  }
  if (!params.is_object() || !params.contains("protocolVersion") || !params["protocolVersion"].is_string()) {
    throw rpc_error(rpc_code::invalid_params, "initialize needs \"protocolVersion\" (a string)");
  }
  // The client's revision if supported, otherwise the newest: the client
  // then decides whether it can continue.
  const std::string asked = params["protocolVersion"].get<std::string>();
  const auto match = std::find(kProtocolVersions.begin(), kProtocolVersions.end(), asked);
  protocol_version_ = std::string(match != kProtocolVersions.end() ? *match : kProtocolVersions.front());
  return json{
      {"protocolVersion", protocol_version_},
      {"capabilities",
       {
           {"tools", {{"listChanged", false}}},
           {"resources", {{"subscribe", false}, {"listChanged", false}}},
       }},
      {"serverInfo", {{"name", "nymph"}, {"title", "nymph"}, {"version", std::string(version())}}},
  };
}

} // namespace bdg::nymph::mcp
