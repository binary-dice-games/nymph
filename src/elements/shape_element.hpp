// MIT License © 2026 Binary Dice Games
/// @file shape_element.hpp
/// @brief The `Shape` element (spec/13_more_diagrams_and_shapes/DESIGN.md).
#pragma once

#include "schema/registry.hpp"

namespace bdg::nymph {

/// @brief Registers `Shape`, a `figure` element drawing one shape of the
///        built-in library.
void register_shape_elements(registry& reg);

} // namespace bdg::nymph
