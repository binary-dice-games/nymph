// MIT License © 2026 Binary Dice Games
/// @file sankey_layout.hpp
/// @brief Places the nodes and links of a Sankey diagram (the algorithm of
///        d3-sankey: columns by longest path, sinks justified to the last
///        column, heights proportional to value, then relaxation towards
///        the linked nodes). Pure: no ImGui, deterministic.
#pragma once

#include <cstddef>
#include <vector>

namespace bdg::nymph {

/// @brief One flow: `value` (> 0) from node `from` to node `to`.
struct sankey_link_in {
  size_t from = 0;
  size_t to = 0;
  double value = 0.0;
};

/// @brief What to lay out, in a `width` x `height` area (origin 0, 0).
struct sankey_input {
  size_t node_count = 0;
  std::vector<sankey_link_in> links; ///< Must form no cycle; from != to.
  float width = 600.0f;
  float height = 400.0f;
  float node_width = 16.0f;
  float node_spacing = 12.0f; ///< Least vertical gap between nodes of a column.
  int iterations = 32; ///< Relaxation passes.
};

/// @brief A node's rectangle.
struct sankey_box {
  float x0 = 0.0f, y0 = 0.0f, x1 = 0.0f, y1 = 0.0f;
};

/// @brief A link's band: its vertical centre where it leaves the source
///        (at x0) and enters the target (at x1), and its thickness.
struct sankey_link_out {
  float x0 = 0.0f, y0 = 0.0f;
  float x1 = 0.0f, y1 = 0.0f;
  float width = 0.0f;
};

struct sankey_output {
  std::vector<sankey_box> nodes;
  std::vector<int> column; ///< Per node, 0 on the left.
  std::vector<double> value; ///< Per node: the larger of what enters and what leaves.
  std::vector<sankey_link_out> links; ///< Per input link, in order.
  int columns = 0;
};

/// @brief The columns of the nodes (longest path from a source; nodes with
///        nothing leaving them in the last column), or an empty vector when
///        the links form a cycle.
std::vector<int> sankey_columns(size_t node_count, const std::vector<sankey_link_in>& links);

/// @brief The whole layout. Nodes with no links get a column of 0 and no
///        height. The input must have no cycle (check with sankey_columns()).
sankey_output layout_sankey(const sankey_input& in);

} // namespace bdg::nymph
