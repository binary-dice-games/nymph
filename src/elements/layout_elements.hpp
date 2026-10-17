// MIT License © 2026 Binary Dice Games
/// @file layout_elements.hpp
/// @brief Layout and text elements: combine several plots and captions in
///        one image.
#pragma once

#include "schema/registry.hpp"

namespace bdg::nymph {

/// @brief Registers `VerticalLayout`, `HorizontalLayout`, `Spring`, `Label`,
///        `Separator` and `SeparatorText` (all `figure` elements; the two
///        layouts accept `figure` children).
void register_layout(registry& reg);

} // namespace bdg::nymph
