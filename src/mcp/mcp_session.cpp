// MIT License © 2026 Binary Dice Games
/// @file mcp_session.cpp
/// @brief The MCP lifecycle and method dispatch.
#include "mcp/mcp_session.hpp"
#include "mcp/editor_bridge.hpp"
#include "mcp/web_bridge.hpp"
#include <algorithm>
#include <array>
#include <exception>
#include <utility>
#include "mcp/resources.hpp"

namespace bdg::nymph::mcp {

namespace {

constexpr std::array<std::string_view, 3> kProtocolVersions = {"2025-11-25", "2025-06-18", "2025-03-26"};

/// The methods answered after `initialize`, besides `ping`.
constexpr std::array<std::string_view, 4> kSessionMethods = {
    "tools/list",
    "tools/call",
    "resources/list",
    "resources/read"};

bool is_session_method(const std::string& method) {
  return std::find(kSessionMethods.begin(), kSessionMethods.end(), method) != kSessionMethods.end();
}

} // namespace

std::span<const std::string_view> supported_protocol_versions() {
  return kProtocolVersions;
}

session::session(server_options options) : tools_(std::make_unique<tool_registry>(options)) {
  if (options.builtin_tools) {
    add_builtin_tools(*tools_);
    // The chart open in the web app (spec/17_agent_connectors): nothing is
    // opened until web_app_pair.
    web_bridge_options web;
    web.app_url = options.web_app_url;
    web.origins = options.web_app_origins;
    web.call_timeout = options.web_call_timeout;
    web_ = std::make_shared<web_bridge>(std::move(web), [notify = options.notify] {
      if (notify) {
        notify(json{{"jsonrpc", "2.0"}, {"method", "notifications/tools/list_changed"}});
      }
    });
    // The chart open in a running editor (spec/14_editor_workspace), or in
    // a paired web app tab.
    add_editor_bridge(*tools_, std::make_shared<editor_bridge>(options.editors_dir, web_));
    add_web_app_tools(*tools_, web_);
  }
  if (options.extra_tools) {
    options.extra_tools(*tools_);
  }
}

session::~session() = default;

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
    return tools_->list();
  }
  if (method == "tools/call") {
    return tools_->call(params);
  }
  if (method == "resources/read") {
    return read_resource(params, tools_->options().viewer_html);
  }
  return list_resources(tools_->options().viewer_html);
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
  if (web_ && params.contains("clientInfo") && params["clientInfo"].is_object()) {
    web_->set_client(params["clientInfo"].value("name", std::string()));
  }
  return json{
      {"protocolVersion", protocol_version_},
      {"capabilities",
       {
           {"tools", {{"listChanged", web_ != nullptr}}},
           {"resources", {{"subscribe", false}, {"listChanged", false}}},
       }},
      {"serverInfo",
       {{"name", tools_->options().server_name},
        {"title", tools_->options().server_name},
        {"version", std::string(version())}}},
      {"instructions", tools_->options().instructions.empty() ? server_instructions() : tools_->options().instructions},
  };
}

} // namespace bdg::nymph::mcp
