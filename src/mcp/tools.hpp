// MIT License © 2026 Binary Dice Games
/// @file tools.hpp
/// @brief The tools of `nymph mcp`: render, check and extract charts, the
///        element schema and the font list
///        (spec/06_claude_integration/DESIGN.md section 5.1).
#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "mcp/json_rpc.hpp"
#include "mcp/server_options.hpp"
#include "nymph.hpp"

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

/// @brief The server's tools, and the renderers they share.
///
/// Renderers are created on first use, one per rasterizer, and kept for
/// the session (an OpenGL context is costly to create). Not thread-safe.
class tool_registry {
 public:
  explicit tool_registry(const server_options& options);
  ~tool_registry();
  tool_registry(const tool_registry&) = delete;
  tool_registry& operator=(const tool_registry&) = delete;

  /// @brief Adds a tool, replacing one of the same name.
  void add(tool t);

  /// @brief The tool called `name`, or nullptr.
  tool* find(const std::string& name);

  /// @brief The `tools/list` result: every tool, in the order added.
  json list() const;

  /// @brief Runs `tools/call`.
  /// @return A CallToolResult. Problems with the arguments and failures of
  ///         the tool are results with `isError` (the agent can fix them).
  /// @throws rpc_error invalid_params for params without a string `name`,
  ///         `arguments` that is not an object, or an unknown tool.
  json call(const json& params);

  /// @brief The renderer for `r`, created on first use.
  renderer& renderer_for(rasterizer r);

  const server_options& options() const {
    return options_;
  }

 private:
  server_options options_;
  std::vector<tool> tools_;
  std::map<rasterizer, std::unique_ptr<renderer>> renderers_;
};

/// @brief Adds the built-in tools (render_chart, check_chart,
///        extract_chart, chart_schema, example_chart, list_fonts, list_shapes) to `registry`, and when
///        its options carry the App page, render_chart's `_meta.ui` and
///        the app-only viewer_font (DESIGN.md section 4.2).
void add_builtin_tools(tool_registry& registry);

/// @brief A result with one text block, and `isError` when `error`.
json text_result(const std::string& text, bool error = false);

} // namespace bdg::nymph::mcp
