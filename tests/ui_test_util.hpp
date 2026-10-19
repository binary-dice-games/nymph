// MIT License © 2026 Binary Dice Games
/// @file ui_test_util.hpp
/// @brief Rendering helpers for the user-interface element tests
///        (spec/09_ui_mockups): the image plus the UI state and edit
///        targets of the last frame, with the unique-ID check on.
#pragma once

#include <array>
#include <string>
#include <unordered_map>
#include <vector>
#include "document/bind.hpp"
#include "document/view.hpp"
#include "elements/register_builtin.hpp"
#include "layout/layout.hpp"
#include "render/figure_renderer.hpp"

namespace bdg::nymph::test {

using rgba = std::array<uint8_t, 4>;

inline rgba pixel(const image& img, int x, int y) {
  const uint8_t* p = img.at(x, y);
  return {p[0], p[1], p[2], p[3]};
}

/// What render_ui() returns.
struct ui_render {
  image img;
  std::unordered_map<std::string, ui_entry> state; ///< By element path, after the last frame.
  std::vector<edit_record> targets; ///< Of the last frame (edit mode on).
  std::unordered_map<const node*, std::string> paths;
  figure fig;
};

/// Renders `source` as render_figure() does, with the unique-ID check on
/// (throws std::logic_error on a repeated ImGui ID) and edit targets
/// recorded, and returns the image, the last frame's targets and the UI
/// state after it.
inline ui_render render_ui(const std::string& source, bool edit_mode = false) {
  ui_render out;
  out.fig = bind_source(source, builtin_registry());
  for (const auto& [path, n] : element_paths(out.fig.root)) {
    out.paths[n] = path;
  }
  out.img = render_frames(out.fig.image, [&](draw_context& dc) {
    dc.begin_frame(); // targets of this frame only, as a live view keeps them
    dc.set_views(&out.fig.views, &out.paths);
    dc.ui().set_check_ids(true);
    dc.set_edit_mode(edit_mode);
    layout_root(out.fig.root, dc);
    dc.draw_node(out.fig.root);
    out.state = dc.ui().entries();
    out.targets = dc.targets();
  });
  return out;
}

} // namespace bdg::nymph::test
