// MIT License © 2026 Binary Dice Games
/// @file figure_renderer.hpp
/// @brief Offscreen frame driver: runs ImGui / ImPlot / ImPlot3D frames with
///        no window or GPU and rasterizes the last one.
#pragma once

#include <functional>
#include "render/draw_context.hpp"
#include "render/image.hpp"
#include "render/image_options.hpp"

namespace bdg::nymph {

/// @brief Draws into the open figure window; called once per frame.
using draw_fn = std::function<void(draw_context&)>;

/// @brief Number of frames run per image; the last is kept. ImPlot fits
///        axes one frame late and ImGui sizes new windows and text after
///        their first frame.
inline constexpr int kFigureFrames = 3;

/// @brief Font size of the figure text in logical pixels (wish's default).
inline constexpr float kFigureFontSize = 16.0f;

/// @brief Renders `draw` into an image of `width*scale` x `height*scale`.
///
/// Creates a private ImGui context (with its own font atlas), ImPlot context
/// and ImPlot3D context for the call, runs kFigureFrames frames with fixed
/// inputs (no clock, no mouse), and rasterizes the last frame with
/// soft_rasterizer. The contexts that were current before the call are
/// current again afterwards, also when it throws, so it may be called while
/// another ImGui frame is open on the same thread. Not thread-safe: ImGui's
/// current context is process-global.
///
/// @param options Size, scale, theme and padding; validated here.
/// @param draw    Called once per frame inside a borderless window covering
///                the display, with no window padding.
/// @return The opaque RGBA image.
/// @throws std::invalid_argument if an option is out of range or the theme
///         is unknown; std::runtime_error from the rasterizer; anything
///         `draw` throws.
image render_frames(const image_options& options, const draw_fn& draw);

/// @brief Whether `name` is a theme render_frames() accepts.
bool is_known_theme(const std::string& name);

} // namespace bdg::nymph
