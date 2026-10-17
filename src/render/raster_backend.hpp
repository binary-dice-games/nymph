// MIT License © 2026 Binary Dice Games
/// @file raster_backend.hpp
/// @brief The swappable "draw data to pixels" step of the frame driver
///        (spec/02_opengl_rasterizer/DESIGN.md section 3).
#pragma once

#include <imgui.h>
#include <stdexcept>
#include "render/image.hpp"
#include "render/soft_rasterizer.hpp"
#include "render/texture_store.hpp"

namespace bdg::nymph {

/// @brief Thrown by a backend that cannot draw this image (no device, a
///        size over its limits, a driver error). The caller renders the
///        image with the software rasterizer instead. Bad draw data is not
///        this: it is a plain std::runtime_error and is never hidden.
class raster_unavailable : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

/// @brief raster_unavailable for an image larger than the backend allows:
///        only that image falls back; the backend stays usable.
class raster_too_large : public raster_unavailable {
 public:
  using raster_unavailable::raster_unavailable;
};

/// @brief Turns the last frame's draw data into pixels.
class raster_backend {
 public:
  virtual ~raster_backend() = default;

  /// @brief ImGui's `BackendRendererName` while frames run.
  virtual const char* name() const noexcept = 0;

  /// @brief Same contract as rasterize() in soft_rasterizer.hpp: an image
  ///        of `round(DisplaySize * FramebufferScale)`, cleared to
  ///        `clear_color` (`IM_COL32`) before drawing.
  /// @throws raster_unavailable when this backend cannot draw the image;
  ///         std::runtime_error for an unknown texture or draw callback.
  virtual image rasterize(const ImDrawData& draw_data, const texture_store& textures, ImU32 clear_color) = 0;
};

/// @brief The CPU rasterizer of soft_rasterizer.hpp as a backend.
class software_backend final : public raster_backend {
 public:
  const char* name() const noexcept override {
    return "nymph_soft_rasterizer";
  }
  image rasterize(const ImDrawData& draw_data, const texture_store& textures, ImU32 clear_color) override {
    return bdg::nymph::rasterize(draw_data, textures, clear_color);
  }
};

} // namespace bdg::nymph
