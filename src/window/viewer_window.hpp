// MIT License © 2026 Binary Dice Games
/// @file viewer_window.hpp
/// @brief The live viewer in a native window: a Win32 window and Dear ImGui's OpenGL3
///        backend around a live_view (spec/04_web_viewer/DESIGN.md
///        section 10).
#pragma once

#include <stdexcept>
#include "window/view_command.hpp"

namespace bdg::nymph {

/// @brief The window or its OpenGL context could not be created.
class window_error : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

/// @brief Opens a window on `request` and runs it until it is closed
///        (Esc or Q, or the window's close button).
/// @throws window_error when there is no display or no OpenGL 3 context.
void show_window(const view_request& request);

} // namespace bdg::nymph
