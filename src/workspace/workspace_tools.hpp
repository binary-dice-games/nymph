// MIT License © 2026 Binary Dice Games
/// @file workspace_tools.hpp
/// @brief The editor tools' view of an editor_workspace
///        (mcp::editor_host): the native editor's MCP server and the web
///        editor's `tool()` both run the tools through it
///        (spec/15_web_app/DESIGN.md sections 5.2 and 11).
#pragma once

#include <functional>
#include <string>
#include "mcp/editor_tools.hpp"
#include "workspace/editor_workspace.hpp"

namespace bdg::nymph {

/// @brief An mcp::editor_host over a workspace; UI thread only.
class workspace_tools : public mcp::editor_host {
 public:
  /// @param file What editor_state reports as the file (a path or a name).
  workspace_tools(editor_workspace& workspace, std::function<std::string()> file)
      : ws_(workspace), file_(std::move(file)) {}

  editor_model* model() override;
  std::string file() const override;
  std::optional<std::string> data_tab() const override;
  void show_tab(std::optional<std::string> table) override;
  std::vector<edit_element> tree() const override;
  std::string view_source() const override;
  std::string panel_error() const override;
  /// The change is shown like a panel edit; the status line names the tool
  /// and, for an agent, who called it.
  void changed(const commit_result& r, const std::string& tool) override;
  void select(const edit_target_id& target) override;
  std::string locked_reason() const override;
  std::vector<uint8_t> view_image(int max_width, int& width, int& height) override;

 private:
  editor_workspace& ws_;
  std::function<std::string()> file_;
};

} // namespace bdg::nymph
