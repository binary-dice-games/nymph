// MIT License © 2026 Binary Dice Games
/// @file image_options.hpp
/// @brief The `image:` section of a nymph source: output size and look.
#pragma once

#include <string>

namespace bdg::nymph {

/// @brief Output image options (DESIGN.md section 3, `image`). Defaults are
///        wish nymph's.
struct image_options {
  int width = 800; ///< Logical width in pixels, 16-4096.
  int height = 500; ///< Logical height in pixels, 16-4096.
  int scale = 1; ///< Supersampling factor 1-4; PNG is width*scale x height*scale.
  std::string theme = "light"; ///< "light", "dark" or "classic".
  int padding = 8; ///< Logical pixels between the image edge and the content.
};

} // namespace bdg::nymph
