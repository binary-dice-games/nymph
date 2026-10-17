// MIT License © 2026 Binary Dice Games
/// @file sankey.hpp
/// @brief `SankeyDiagram`, `SankeyNode` and `SankeyFlow`: quantities flowing
///        between nodes as bands of proportional width
///        (spec/01_core_features/DESIGN.md section 3, "Sankey diagrams").
#pragma once

#include "schema/registry.hpp"

namespace bdg::nymph {

/// @brief Registers `SankeyDiagram`, `SankeyNode` and `SankeyFlow`.
void register_sankey(registry& reg);

} // namespace bdg::nymph
