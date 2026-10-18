// MIT License © 2026 Binary Dice Games
/// @file live_view.hpp
/// @brief A figure drawn frame after frame, with persistent ImGui, ImPlot,
///        ImPlot3D and ImUml state: the platform-neutral heart of the
///        interactive viewer (spec/04_web_viewer/DESIGN.md section 4).
#pragma once

#include <imgui.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include "render/texture_store.hpp"

namespace bdg::nymph {

/// @brief The box a view is shown in: CSS pixels and the device pixel
///        ratio. A width or height of 0 means "no constraint".
struct view_size {
  float width = 0.0f;
  float height = 0.0f;
  float pixel_ratio = 1.0f;
};

/// @brief How the source's `image.width` / `height` relate to the box
///        (spec/04_web_viewer/DESIGN.md section 3, "Sizing").
enum class view_fit {
  source, ///< Lay out at the source size; shrink with CSS when the box is narrower.
  width, ///< Lay out at the box width; height scaled to keep the aspect ratio.
  fill, ///< Lay out at the box size.
};

/// @brief Fixed choices of a live view.
struct view_options {
  view_fit fit = view_fit::source;
  /// @brief Answer ImGui's texture requests with a CPU texture_store (for
  ///        the software rasterizer). A GPU presenter that answers them
  ///        itself sets this to false.
  bool cpu_textures = true;
};

/// @brief Where and how big the last frame is.
struct view_geometry {
  float layout_width = 0.0f; ///< Logical pixels the layout runs at (ImGui's DisplaySize).
  float layout_height = 0.0f;
  float css_scale = 1.0f; ///< CSS pixels per logical pixel (below 1 when shrunk).
  float framebuffer_scale = 1.0f; ///< Device pixels per logical pixel.
  int pixel_width = 0; ///< Size of the drawn image in device pixels.
  int pixel_height = 0;
  bool fit_fallback = false; ///< The fit could not be honoured; laid out at the source size.
};

/// @brief Thrown by an update whose figure names fonts without bytes. The
///        previous figure stays; register the fonts (register_font()) and
///        repeat the update.
class fonts_needed : public std::runtime_error {
 public:
  explicit fonts_needed(std::vector<std::string> fonts);
  /// @brief The missing font file names, sorted.
  const std::vector<std::string>& fonts() const {
    return fonts_;
  }

 private:
  std::vector<std::string> fonts_;
};

/// @brief A figure kept on screen: updates, resizes and frames on demand.
///
/// Owns private ImGui, ImPlot, ImPlot3D and ImUml contexts for its
/// lifetime and makes them current only inside its own calls, restoring
/// the previous ones afterwards; several live views may exist on one
/// thread. Not thread-safe: use each from one thread at a time, and never
/// two at once (ImGui's current context is process-global).
class live_view {
 public:
  explicit live_view(view_options options = {});
  ~live_view();
  live_view(live_view&&) noexcept;
  live_view& operator=(live_view&&) noexcept;
  live_view(const live_view&) = delete;
  live_view& operator=(const live_view&) = delete;

  /// @brief Replaces the figure with `source`.
  /// @param reset_view Drop the view state (zoom, hidden series) of the
  ///        previous figure; otherwise elements with an unchanged ID path
  ///        keep it.
  /// @throws nymph::error for a problem in the source, including a layout
  ///         that does not fit its own `image` size; fonts_needed. Either
  ///         way the previous figure stays.
  void set_source(std::string_view source, bool reset_view = false);

  /// @brief Overrides `image.theme`: "light", "dark", "classic", or "" for
  ///        the source's own.
  /// @throws std::invalid_argument for an unknown theme.
  void set_theme(std::string theme);

  /// @brief Sets the box the view is shown in.
  void resize(view_size size);

  /// @brief Runs one frame if one is due (wants_frame()).
  /// @param now_seconds A monotonic clock; the step since the last frame is
  ///        ImGui's DeltaTime (1/60 s for the first frame).
  /// @return The frame's draw data, valid until the next call on this view;
  ///         nullptr when no frame was due or there is no figure.
  const ImDrawData* frame(double now_seconds);

  /// @brief Whether frame() would draw: after an update, a resize or a
  ///        theme change, until the frames that settle it have run.
  bool wants_frame() const;

  /// @brief The theme's window background, opaque: what the image is
  ///        cleared to.
  ImU32 clear_color() const;

  /// @brief Size and scale of the last frame (or of the next, before any).
  view_geometry geometry() const;

  /// @brief Whether a figure was set.
  bool has_figure() const;
  /// @brief The figure's `description`.
  const std::string& description() const;
  /// @brief The source of the figure on screen.
  std::string_view source() const;
  /// @brief The CPU textures (view_options::cpu_textures) the draw data
  ///        refers to.
  const texture_store& textures() const;

 private:
  struct impl;
  std::unique_ptr<impl> impl_;
};

} // namespace bdg::nymph
