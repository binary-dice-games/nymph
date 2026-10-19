// MIT License © 2026 Binary Dice Games
/// @file image_fields.hpp
/// @brief The `image:` options of a source (`width`, `height`, `scale`,
///        `theme`, `padding`, `font`, `font_size`, `text_color`) described as
///        fields, so the editor shows them like an element's
///        (spec/08_view_editor/DESIGN.md, "The image options").
#pragma once

#include <string_view>
#include <vector>
#include "render/image_options.hpp"
#include "schema/field_desc.hpp"
#include "schema/value.hpp"

namespace bdg::nymph {

/// @brief The path the editor gives the image options: selected like an
///        element, though it is not one.
inline constexpr std::string_view kImagePath = "image";

/// @brief The image options as fields, in the order the docs list them.
const std::vector<field_desc>& image_fields();

/// @brief The value of image field `index` in `options`.
value image_field_value(const image_options& options, size_t index);

} // namespace bdg::nymph
