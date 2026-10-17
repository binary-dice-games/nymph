// MIT License © 2026 Binary Dice Games
/// @file renderer_options.hpp
/// @brief Internal options of bdg::nymph::renderer, for tests: simulate a
///        machine without OpenGL, allow a software OpenGL, inject failures.
#pragma once

#include <string>
#include <vector>
#include "nymph.hpp"

namespace bdg::nymph {

/// @brief What make_renderer() changes from a plain renderer.
struct renderer_test_options {
  /// Accept a software OpenGL (llvmpipe), so the OpenGL path runs on
  /// machines without a GPU.
  bool allow_software_gl = false;
  /// Library names to try instead of the platform's.
  std::vector<std::string> library_names;
  /// The first render after creation fails as a driver error would.
  bool fail_first_render = false;
  /// Treat images with a side above this as too large (0: the device's).
  int max_size = 0;
};

/// @brief A renderer created with test options.
renderer make_renderer(rasterizer requested, const renderer_test_options& options);

} // namespace bdg::nymph
