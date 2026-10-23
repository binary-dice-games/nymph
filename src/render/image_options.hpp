// MIT License © 2026 Binary Dice Games
/// @file image_options.hpp
/// @brief The `image:` section of a nymph source: output size and look.
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace bdg::nymph {

/// @brief Output image options (spec/01_core_features/DESIGN.md section 3,
///        `image`). Defaults are wish nymph's.
struct image_options {
  int width = 800; ///< Logical width in pixels, 16-4096.
  int height = 500; ///< Logical height in pixels, 16-4096.
  int scale = 1; ///< Supersampling factor 1-4; PNG is width*scale x height*scale.
  /// The chart theme (themes/theme_catalog.hpp) the fields left out took
  /// their values from: "light" (default), "dark", "ocean", ...
  std::string theme = "light";
  /// The theme's ImGui / ImPlot / ImUml colours: "light", "dark" or "classic".
  std::string base = "light";
  int padding = 8; ///< Logical pixels between the image edge and the content.
  std::string font; ///< Embedded font file name; "" or "default" is ImGui's built-in font.
  double font_size = 16.0; ///< Text size in logical pixels.
  std::string text_color; ///< "#RRGGBB[AA]", or "" for the theme's.
  std::string background; ///< "#RRGGBB[AA]", or "" for the base colours'.
  /// Series colours ("#RRGGBB[AA]"): the plots' default colormap; empty for
  /// ImPlot's (Deep).
  std::vector<std::string> palette;
};

/// @brief Whether `name` is a built-in chart theme (defined in
///        themes/theme_catalog.cpp).
bool is_known_theme(std::string_view name);

} // namespace bdg::nymph
