// MIT License © 2026 Binary Dice Games
/// @file limits.hpp
/// @brief Input size limits, checked before the allocations they bound
///        (spec/01_core_features/DESIGN.md section 8).
#pragma once

#include <cstddef>

namespace bdg::nymph {

/// @brief Largest accepted source text, and largest source extracted from a
///        PNG: 16 MiB.
inline constexpr size_t kMaxSourceBytes = 16u * 1024u * 1024u;

/// @brief Largest number of data rows in a source's `data`.
inline constexpr size_t kMaxDataRows = 1000000u;

/// @brief Largest number of nodes (elements with an `id`) in one UML diagram
///        (spec/03_uml_diagrams/DESIGN.md section 8).
inline constexpr size_t kMaxUmlNodes = 500u;

/// @brief Largest number of edges (relations, transitions, flows) in one UML
///        graph diagram.
inline constexpr size_t kMaxUmlEdges = 2000u;

/// @brief Largest number of rows (messages, notes, fragment parts) in one
///        sequence diagram.
inline constexpr size_t kMaxUmlRows = 2000u;

/// @brief Deepest nesting of composite states or sequence fragments.
inline constexpr size_t kMaxUmlNesting = 8u;

/// @brief Deepest nesting of a tree, mind map or org chart
///        (spec/13_more_diagrams_and_shapes/DESIGN.md section 6).
inline constexpr size_t kMaxTreeDepth = 32u;

/// @brief Largest number of tasks in one GanttChart.
inline constexpr size_t kMaxGanttTasks = 500u;

/// @brief Largest number of nodes and of flows in one SankeyDiagram.
inline constexpr size_t kMaxSankeyNodes = 500u;
inline constexpr size_t kMaxSankeyFlows = 2000u;

} // namespace bdg::nymph
