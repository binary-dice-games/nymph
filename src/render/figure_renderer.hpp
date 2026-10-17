// MIT License © 2026 Binary Dice Games
/// @file figure_renderer.hpp
/// @brief Offscreen frame driver: runs ImGui / ImPlot / ImPlot3D frames with
///        no window or GPU and rasterizes the last one.
#pragma once

#include <functional>
#include <unordered_map>
#include "document/bind.hpp"
#include "render/draw_context.hpp"
#include "render/image.hpp"
#include "render/image_options.hpp"
#include "render/raster_backend.hpp"

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
/// @param backend Draws the last frame; its name() is ImGui's
///                `BackendRendererName` while the frames run.
/// @return The opaque RGBA image.
/// @throws std::invalid_argument if an option is out of range or the theme
///         is unknown; std::runtime_error (raster_unavailable included,
///         for the caller to fall back) from the backend; anything `draw`
///         throws.
image render_frames(const image_options& options, const draw_fn& draw, raster_backend& backend);

/// @brief render_frames() with the software rasterizer.
image render_frames(const image_options& options, const draw_fn& draw);

/// @brief Renders a bound figure: render_frames() with `f.image`, drawing
///        `f.root` in the content rectangle (the image minus padding).
/// @throws as render_frames().
image render_figure(const figure& f, raster_backend& backend);

/// @brief render_figure() with the software rasterizer.
image render_figure(const figure& f);

/// @brief Runs one frame of `f` with only the layout pass: every `figure`
///        node's rectangle, in logical pixels. Used by check_source() (a
///        layout overflow is only found here) and by tests.
/// @throws nymph::error for a layout that does not fit; as render_frames().
std::unordered_map<const node*, rect> layout_figure(const figure& f);

} // namespace bdg::nymph
