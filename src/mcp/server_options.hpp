// MIT License © 2026 Binary Dice Games
/// @file server_options.hpp
/// @brief What `nymph mcp` was started with.
#pragma once

#include <cstddef>
#include "nymph.hpp"
#include "render/renderer_options.hpp"

namespace bdg::nymph::mcp {

/// @brief What `nymph mcp` was started with.
struct server_options {
  rasterizer raster = rasterizer::opengl; ///< --rasterizer (default opengl, falling back silently)
  bool raster_explicit = false; ///< --rasterizer was given: warn on an OpenGL fallback
  /// Results whose image would exceed this much base64 leave the image out
  /// (DESIGN.md section 5.1). Tests lower it.
  size_t max_image_base64 = 8u * 1024u * 1024u;
  renderer_test_options gl_options; ///< tests: simulate machines without OpenGL
};

} // namespace bdg::nymph::mcp
