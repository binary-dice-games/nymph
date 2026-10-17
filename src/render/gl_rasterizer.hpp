// MIT License © 2026 Binary Dice Games
/// @file gl_rasterizer.hpp
/// @brief The OpenGL rasterizer: ImGui draw data to RGBA8 pixels through an
///        OpenGL 3.3 framebuffer object
///        (spec/02_opengl_rasterizer/DESIGN.md section 3, `gl_backend`).
#pragma once

#include "render/gl/gl_device.hpp"
#include "render/raster_backend.hpp"

namespace bdg::nymph {

/// @brief raster_backend over a gl_device. Matches the software rasterizer
///        within a small tolerance (same geometry, blend equation, sampling
///        rule and clear), not byte for byte.
///
/// Every texture a command uses is uploaded per call and every GL object
/// but the shader program is deleted before rasterize() returns, also when
/// it throws. The device's context is current only during the call; the
/// thread's previous context is current again afterwards.
class gl_backend final : public raster_backend {
 public:
  /// @param device Must outlive the backend.
  explicit gl_backend(gl_device& device) : device_(device) {}
  ~gl_backend() override;
  gl_backend(const gl_backend&) = delete;
  gl_backend& operator=(const gl_backend&) = delete;

  const char* name() const noexcept override {
    return "nymph_opengl3";
  }

  /// @throws raster_too_large for an image larger than the device allows;
  ///         raster_unavailable for a shader that does not compile, an incomplete framebuffer or a
  ///         GL error; std::runtime_error for an unknown texture or draw
  ///         callback, as the software rasterizer does.
  image rasterize(const ImDrawData& draw_data, const texture_store& textures, ImU32 clear_color) override;

  /// @brief Test hook: the next rasterize() throws raster_unavailable as a
  ///        driver failure would.
  void fail_next_rasterize() noexcept {
    fail_next_ = true;
  }

  /// @brief Test hook: treat images with a side above `pixels` as too
  ///        large, as on a device with that limit (0: the device's own).
  void limit_size(int pixels) noexcept {
    size_limit_ = pixels;
  }

 private:
  void build_program();

  gl_device& device_;
  GLuint program_ = 0;
  GLint projection_location_ = -1;
  GLint texture_location_ = -1;
  bool fail_next_ = false;
  int size_limit_ = 0;
};

} // namespace bdg::nymph
