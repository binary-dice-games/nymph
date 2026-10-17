// MIT License © 2026 Binary Dice Games
/// @file plot3d.hpp
/// @brief The 3D chart elements: `Plot3D` and its series.
#pragma once

#include "schema/registry.hpp"

namespace bdg::nymph {

/// @brief Slot of the 3D series (`Plot3DLine`, ...): children of a `Plot3D`.
inline constexpr const char* kPlot3dItemSlot = "plot3d.item";

/// @brief Registers `Plot3D`, `Plot3DLine`, `Plot3DScatter`,
///        `Plot3DSurface`, `Plot3DTriangle`, `Plot3DQuad`, `Plot3DMesh` and
///        `Plot3DText`, with wish's fields, defaults and descriptions.
void register_plot3d(registry& reg);

} // namespace bdg::nymph
