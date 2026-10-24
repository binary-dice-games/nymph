// MIT License © 2026 Binary Dice Games
/// @file editor_bridge.hpp
/// @brief The editor tools in `nymph mcp`: each call goes to a running
///        editor's own MCP server over its local socket, so an agent that has
///        nymph reaches the chart the user has open with no new setup
///        (spec/14_editor_workspace/DESIGN.md sections 3 and 9).
#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include "ipc/editor_registry.hpp"
#include "ipc/local_socket.hpp"
#include "mcp/json_rpc.hpp"
#include "mcp/tools.hpp"
#include "mcp/web_bridge.hpp"

namespace bdg::nymph::mcp {

/// @brief Connections from `nymph mcp` to running editors. Not thread-safe
///        (one session uses it).
class editor_bridge {
 public:
  /// @param dir The editors' folder; empty for ipc::editors_dir().
  /// @param web The paired web app pages, which count as editors too
  ///        (spec/17_agent_connectors/DESIGN.md section 7.2); may be null.
  explicit editor_bridge(std::filesystem::path dir = {}, std::shared_ptr<web_bridge> web = {})
      : dir_(std::move(dir)), web_(std::move(web)) {}

  /// @brief Calls `tool` on the editor that `arguments.editor` names (a file,
  ///        a process id, or `web:<name>` for a paired web app tab; the one
  ///        focused last when absent), without that argument.
  /// @return The editor's CallToolResult.
  /// @throws tool_error when no editor is open, it went away, or it refused
  ///         the request itself.
  json call(const std::string& tool, json arguments);

 private:
  struct link {
    ipc::editor_entry entry;
    ipc::local_stream stream;
    int next_id = 1;
  };
  link& connect(const std::string& want);
  std::filesystem::path dir() const;
  double latest_native_focus() const;
  json request(link& l, const std::string& method, json params);

  std::filesystem::path dir_;
  std::shared_ptr<web_bridge> web_;
  std::map<int64_t, std::unique_ptr<link>> links_; ///< By process id.
};

/// @brief Adds the editor tools to `nymph mcp`'s registry, each with an
///        optional `editor` argument, forwarding through one bridge.
void add_editor_bridge(tool_registry& registry, std::shared_ptr<editor_bridge> bridge);

} // namespace bdg::nymph::mcp
