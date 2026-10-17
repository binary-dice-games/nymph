// MIT License © 2026 Binary Dice Games
/// @file image_options.hpp
/// @brief The `image:` section of a nymph source: output size and look.
#pragma once

#include <array>
#include <string>
#include <string_view>

namespace bdg::nymph {

/// @brief Output image options (DESIGN.md section 3, `image`). Defaults are
///        wish nymph's.
struct image_options {
  int width = 800; ///< Logical width in pixels, 16-4096.
  int height = 500; ///< Logical height in pixels, 16-4096.
  int scale = 1; ///< Supersampling factor 1-4; PNG is width*scale x height*scale.
  std::string theme = "light"; ///< "light", "dark" or "classic".
  int padding = 8; ///< Logical pixels between the image edge and the content.
  std::string font; ///< Embedded font file name; "" or "default" is ImGui's built-in font.
  double font_size = 16.0; ///< Text size in logical pixels.
  std::string text_color; ///< "#RRGGBB[AA]", or "" for the theme's.
};

/// @brief The themes `image.theme` accepts.
inline constexpr std::array<std::string_view, 3> kThemeNames{"light", "dark", "classic"};

/// @brief Whether `name` is one of kThemeNames.
inline bool is_known_theme(std::string_view name) {
  for (std::string_view t : kThemeNames) {
    if (t == name) {
      return true;
    }
  }
  return false;
}

} // namespace bdg::nymph
