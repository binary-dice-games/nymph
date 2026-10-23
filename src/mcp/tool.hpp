// MIT License © 2026 Binary Dice Games
/// @file tool.hpp
/// @brief One MCP tool (its `tools/list` entry and its handler), the error a
///        tool reports to the agent, and the text result: what the editor
///        tools need without the rest of `nymph mcp`
///        (spec/15_web_app/DESIGN.md section 6, `nymph_editor_tools`).
#pragma once

#include <functional>
#include <stdexcept>
#include <string>
#include "mcp/json_rpc.hpp"

namespace bdg::nymph::mcp {

/// @brief A failure the agent should read and fix: becomes a tool result
///        with `isError` and this text.
class tool_error : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

/// @brief One tool: its `tools/list` entry and its handler.
struct tool {
  json definition; ///< name, title, description, inputSchema, ...
  /// Runs the tool on validated-as-object arguments and returns a
  /// CallToolResult. Throws tool_error for a problem to report.
  std::function<json(const json& arguments)> handler;
};

/// @brief A result with one text block, and `isError` when `error`.
json text_result(const std::string& text, bool error = false);

} // namespace bdg::nymph::mcp
