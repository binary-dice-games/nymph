// MIT License © 2026 Binary Dice Games
/// @file register_builtin.hpp
/// @brief The element types nymph ships with.
#pragma once

#include "schema/registry.hpp"

namespace bdg::nymph {

/// @brief Registers every built-in element family into `reg`.
void register_builtin_elements(registry& reg);

/// @brief A registry holding the built-in elements, built on first use.
const registry& builtin_registry();

} // namespace bdg::nymph
