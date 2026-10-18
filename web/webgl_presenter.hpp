// MIT License © 2026 Binary Dice Games
/// @file webgl_presenter.hpp
/// @brief Draws live views with WebGL2 through Dear ImGui's OpenGL3
///        backend, on one WebGL context shared by every viewer of the page
///        (spec/04_web_viewer/DESIGN.md section 4, "Web module").
#pragma once

#include <imgui.h>
#include "view/live_view.hpp"

namespace bdg::nymph::web {

/// @brief Creates the shared WebGL2 context on the canvas Emscripten finds
///        as `target` (the JavaScript side registers a hidden canvas in
///        `specialHTMLTargets`).
/// @return false when WebGL2 is unavailable; viewers then draw on the CPU.
bool gl_init(const char* target);

/// @brief Whether gl_init() succeeded.
bool gl_ready();

/// @brief Makes `view` (created with view_options::cpu_textures false)
///        draw through the shared context: installs ImGui's OpenGL3
///        backend in its contexts, now and whenever they are recreated.
void gl_attach(live_view& view);

/// @brief Draws a frame of `view` into the bottom-left `width` x `height`
///        pixels of the shared canvas (growing it when needed), cleared to
///        the view's background. The caller copies it out before yielding
///        to the browser.
void gl_draw(live_view& view, ImDrawData& draw_data, int* width, int* height);

} // namespace bdg::nymph::web
