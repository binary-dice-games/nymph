// MIT License © 2026 Binary Dice Games
/// @file gantt.hpp
/// @brief `GanttChart`, `GanttSection` and `GanttTask`: a schedule drawn as
///        bars on a time axis with ImPlot (spec/01_core_features/DESIGN.md
///        section 3, "Gantt charts").
#pragma once

#include "schema/registry.hpp"

namespace bdg::nymph {

/// @brief Registers `GanttChart`, `GanttSection` and `GanttTask`.
void register_gantt(registry& reg);

} // namespace bdg::nymph
