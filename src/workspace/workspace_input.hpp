// MIT License © 2026 Binary Dice Games
/// @file workspace_input.hpp
/// @brief The keys, modifiers and cursor shapes the editor workspace takes
///        from its host, whatever the host is: a native window
///        (window_host.hpp) or a web page (spec/15_web_app/DESIGN.md
///        section 5.1).
#pragma once

#include <imgui.h>

namespace bdg::nymph {

/// @brief The keys the viewer and the editor react to: the native viewer's
///        own shortcuts, and what a text field of the editor panel needs.
enum class window_key {
  escape, q, r, s, y, e,
  enter, tab, backspace, del, left, right, up, down, home, end, page_up, page_down,
  a, c, d, v, x, z, f, h, u,
  other
};

/// @brief The pointer's look over the workspace.
enum class cursor_shape {
  arrow,
  resize_horizontal, ///< Over a divider (figure and strip, source pane and figure).
  text, ///< Over a text field.
};

/// @brief ImGui's key for `key` (ImGuiKey_None for keys ImGui does not
///        need: the viewer's own letters).
ImGuiKey imgui_key(window_key key);

/// @brief The modifier keys held with a key.
struct key_mods {
  bool ctrl = false;
  bool shift = false;
  bool alt = false;
};

/// @brief Who gets a key.
enum class key_route {
  panel, ///< The editor panel's text field or dialog.
  figure, ///< A text field of the figure (UI mockups, spec/09_ui_mockups).
  viewer, ///< The shortcuts (the native viewer's Esc, Q, R, S, Y, E).
};

/// @brief The panel first while it has the keyboard, then the figure while
///        one of its text fields does, else the shortcuts: typing "q" in a
///        field never closes the window.
inline key_route route_key(bool panel_has_keyboard, bool figure_has_keyboard) {
  if (panel_has_keyboard) {
    return key_route::panel;
  }
  return figure_has_keyboard ? key_route::figure : key_route::viewer;
}

} // namespace bdg::nymph
