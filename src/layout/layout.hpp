// MIT License © 2026 Binary Dice Games
/// @file layout.hpp
/// @brief The layout engine: shares space among `figure` elements before
///        anything is drawn (spec/01_core_features/DESIGN.md section 3 "Layout",
///        section 4 "Layout engine").
#pragma once

#include <span>
#include <vector>
#include "render/draw_context.hpp"
#include "schema/node.hpp"

namespace bdg::nymph {

class element_class;

/// @brief One child along a layout's axis: its hint (positive fixed,
///        0 natural, negative stretch weight) and its natural size, used
///        when the hint is 0.
struct axis_item {
  float hint = 0.0f;
  float natural = 0.0f;
};

/// @brief How distribute() shared an axis.
struct axis_plan {
  std::vector<float> sizes; ///< One per item.
  float needed = 0.0f; ///< Fixed + natural sizes + spacing: what must fit.
};

/// @brief Shares `available` pixels among `items` with `spacing` between
///        them: fixed and natural items get their size, stretch items share
///        what is left in proportion to their weights (none if it is
///        negative). Pure: no ImGui.
/// @return The sizes, and how much the non-stretch part needs. The caller
///         reports an overflow when `needed > available`.
axis_plan distribute(std::span<const axis_item> items, float available, float spacing);

/// @brief Options of a linear (vertical or horizontal) layout.
struct linear_layout {
  bool vertical = true;
  float spacing = 0.0f;
  bool align_right = false; ///< Horizontal only: push the row right when it has room left.
};

/// @brief Arranges the `figure` children of `n` inside `r` along one axis,
///        sets their rectangles and arranges them in turn.
/// @throws nymph::error at `n` when the children's fixed and natural sizes
///         and spacing need more than `r` along the axis.
void arrange_linear(const node& n, draw_context& dc, const rect& r, const linear_layout& how);

/// @brief Natural size of a linear layout's children (without padding):
///        the sum along the axis (stretch children count 0) plus spacing,
///        the largest across.
ImVec2 measure_linear(const node& n, draw_context& dc, float width, const linear_layout& how);

/// @brief Places `root` in the content rectangle (a positive width or
///        height hint is its size, anything else fills) and arranges it.
/// @throws nymph::error when a fixed root size or a layout does not fit.
void layout_root(const node& root, draw_context& dc);

} // namespace bdg::nymph
