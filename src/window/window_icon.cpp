// MIT License © 2026 Binary Dice Games
/// @file window_icon.cpp
/// @brief window_icon() / scale_icon(): the window's icon from the
///        embedded `resources/icon.png`.
#include "window/window_icon.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include "png/png_meta.hpp"

namespace bdg::nymph {

namespace embedded {
extern const unsigned char window_icon_png[];
extern const std::size_t window_icon_png_size;
} // namespace embedded

const image& window_icon() {
  static const image icon = [] {
    const std::span<const uint8_t> bytes(embedded::window_icon_png, embedded::window_icon_png_size);
    return decode_png(bytes).value_or(image{});
  }();
  return icon;
}

image scale_icon(const image& icon, int size) {
  image out;
  if (icon.width <= 0 || icon.height <= 0 || size <= 0) {
    return out;
  }
  out.width = size;
  out.height = size;
  out.rgba.resize(static_cast<size_t>(size) * size * 4);
  for (int y = 0; y < size; ++y) {
    // The source rows and columns that cover this pixel (at least one).
    const int y0 = y * icon.height / size;
    const int y1 = std::max((y + 1) * icon.height / size, y0 + 1);
    for (int x = 0; x < size; ++x) {
      const int x0 = x * icon.width / size;
      const int x1 = std::max((x + 1) * icon.width / size, x0 + 1);
      uint64_t r = 0, g = 0, b = 0, a = 0;
      for (int sy = y0; sy < y1; ++sy) {
        for (int sx = x0; sx < x1; ++sx) {
          const uint8_t* p = icon.at(sx, sy);
          r += uint64_t{p[0]} * p[3];
          g += uint64_t{p[1]} * p[3];
          b += uint64_t{p[2]} * p[3];
          a += p[3];
        }
      }
      const uint64_t count = static_cast<uint64_t>(y1 - y0) * (x1 - x0);
      uint8_t* q = out.at(x, y);
      if (a > 0) {
        q[0] = static_cast<uint8_t>((r + a / 2) / a);
        q[1] = static_cast<uint8_t>((g + a / 2) / a);
        q[2] = static_cast<uint8_t>((b + a / 2) / a);
      }
      q[3] = static_cast<uint8_t>((a + count / 2) / count);
    }
  }
  return out;
}

} // namespace bdg::nymph
