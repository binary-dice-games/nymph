// MIT License © 2026 Binary Dice Games
/// @file workspace_tools.cpp
/// @brief workspace_tools (see workspace_tools.hpp).
#include "workspace/workspace_tools.hpp"

namespace bdg::nymph {

editor_model* workspace_tools::model() {
  return ws_.panel() != nullptr ? &ws_.panel()->model() : nullptr;
}

std::string workspace_tools::file() const {
  return file_ ? file_() : std::string();
}

std::optional<std::string> workspace_tools::data_tab() const {
  return ws_.panel() != nullptr ? ws_.panel()->data_tab() : std::nullopt;
}

void workspace_tools::show_tab(std::optional<std::string> table) {
  if (ws_.panel() != nullptr) {
    ws_.panel()->show_tab(std::move(table));
  }
}

std::vector<edit_element> workspace_tools::tree() const {
  return ws_.view().element_list();
}

std::string workspace_tools::view_source() const {
  try {
    return ws_.view().source_with_view();
  } catch (const std::exception&) {
    return std::string(ws_.view().source());
  }
}

std::string workspace_tools::panel_error() const {
  if (ws_.panel() == nullptr) {
    return {};
  }
  return ws_.panel()->advanced() ? ws_.panel()->source().error() : ws_.panel()->panel().error();
}

std::string workspace_tools::changes_refused() const {
  if (!ws_.advanced()) {
    return {};
  }
  return "the user is editing the YAML source by hand (the editor's advanced mode): the chart cannot be changed "
         "by tools until they go back to the basic editor; the read-only tools still work";
}

void workspace_tools::changed(const commit_result& r, const std::string& tool) {
  const std::string& origin = r.origin;
  std::string by = "the agent";
  if (origin.rfind("agent:", 0) == 0 && origin.size() > 6 && origin != "agent:mcp") {
    by = origin.substr(6);
  } else if (origin == "user") {
    by.clear();
  }
  ws_.apply_change(r, by.empty() ? std::string() : "changed by " + by + ": " + tool);
}

void workspace_tools::select(const edit_target_id& target) {
  if (ws_.panel() == nullptr) {
    return;
  }
  ws_.panel()->model().select(target);
  ws_.view().set_selection(ws_.panel()->model().selection());
  ws_.panel()->refresh();
}

std::vector<uint8_t> workspace_tools::view_image(int max_width, int& width, int& height) {
  return ws_.capture_figure(max_width, width, height, workspace_now());
}

} // namespace bdg::nymph
