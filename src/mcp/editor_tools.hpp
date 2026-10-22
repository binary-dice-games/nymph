// MIT License © 2026 Binary Dice Games
/// @file editor_tools.hpp
/// @brief The MCP tools of a running editor (`nymph view --edit`): read its
///        state (element tree, selection, tables, the view on screen) and
///        change it as the user would (spec/14_editor_workspace/DESIGN.md
///        section 6). The editor serves them on its socket; `nymph mcp` lists
///        the same definitions and forwards calls (editor_bridge.hpp).
#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "edit/editor_model.hpp"
#include "mcp/json_rpc.hpp"
#include "mcp/tools.hpp"
#include "view/live_view.hpp"

namespace bdg::nymph::mcp {

/// @brief What the editor tools act on: the running editor, used only on its
///        UI thread (the tools reach it through an editor_runner).
class editor_host {
 public:
  virtual ~editor_host() = default;
  /// @brief The model, or nullptr while the editor panel is hidden.
  virtual editor_model* model() = 0;
  /// @brief The file being edited.
  virtual std::string file() const = 0;
  /// @brief The data table shown instead of the figure, or nullopt for the chart.
  virtual std::optional<std::string> data_tab() const = 0;
  /// @brief Shows a table's tab, or the chart for nullopt.
  virtual void show_tab(std::optional<std::string> table) = 0;
  /// @brief The elements in drawing order (live_view::element_list()).
  virtual std::vector<edit_element> tree() const = 0;
  /// @brief The source with the view on screen in its `view` key.
  virtual std::string view_source() const = 0;
  /// @brief The error the panel shows; "" for none.
  virtual std::string panel_error() const = 0;
  /// @brief Shows an accepted change: the figure, the panel, a status line
  ///        naming `tool`.
  virtual void changed(const commit_result& r, const std::string& tool) = 0;
  /// @brief Selects in the panel and the figure.
  virtual void select(const edit_target_id& target) = 0;
  /// @brief A PNG of the figure as the user sees it, scaled down to at most
  ///        `max_width` pixels (0: as it is); its size in `width`, `height`.
  virtual std::vector<uint8_t> view_image(int max_width, int& width, int& height) = 0;
};

/// @brief A piece of work for the editor's UI thread.
using editor_job = std::function<json(editor_host&)>;

/// @brief Runs a job on the editor's UI thread and returns its result.
///        Throws tool_error when there is no editor to run it.
using editor_runner = std::function<json(const editor_job&)>;

/// @brief The editor tools' `tools/list` entries, in order.
json editor_tool_definitions();

/// @brief Adds the editor tools to `registry`, each running through `run`.
void add_editor_tools(tool_registry& registry, editor_runner run);

/// @brief The `instructions` of the editor's own MCP session.
std::string editor_instructions();

} // namespace bdg::nymph::mcp
