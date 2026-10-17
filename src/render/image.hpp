// MIT License © 2026 Binary Dice Games
/// @file image.hpp
/// @brief An RGBA8 pixel buffer, the output of the rasterizer.
#pragma once

#include <cstdint>
#include <vector>

namespace bdg::nymph {

/// @brief Row-major RGBA8 pixels, top row first, 4 bytes per pixel in R, G,
///        B, A order (straight, not premultiplied, alpha).
struct image {
  int width = 0;
  int height = 0;
  std::vector<uint8_t> rgba; ///< width * height * 4 bytes.

  /// @brief Pointer to the 4 bytes of pixel (x, y). No bounds check.
  uint8_t* at(int x, int y) {
    return rgba.data() + (static_cast<size_t>(y) * width + x) * 4;
  }
  const uint8_t* at(int x, int y) const {
    return rgba.data() + (static_cast<size_t>(y) * width + x) * 4;
  }
};

} // namespace bdg::nymph
