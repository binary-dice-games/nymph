// MIT License © 2026 Binary Dice Games
/// @file plot2d.hpp
/// @brief The 2D chart elements: `Plot` and its series.
#pragma once

#include "schema/registry.hpp"

namespace bdg::nymph {

/// @brief Slot of the 2D series (`PlotLine`, ...): children of a `Plot`.
inline constexpr const char* kPlot2dItemSlot = "plot2d.item";

/// @brief Registers `Plot`, `PlotLine`, `PlotScatter`, `PlotStairs`,
///        `PlotStems`, `PlotShaded`, `PlotBars`, `PlotBarsH`,
///        `PlotHistogram`, `PlotHistogram2D`, `PlotHeatmap`, `PlotPieChart`,
///        `PlotDigital`, `PlotText` and `PlotInfLines`, with wish's fields,
///        defaults and descriptions.
void register_plot2d(registry& reg);

} // namespace bdg::nymph
