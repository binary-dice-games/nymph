// MIT License © 2026 Binary Dice Games
/// @file sankey_layout.cpp
/// @brief layout_sankey(): d3-sankey's layout (Mike Bostock, ISC license),
///        reimplemented: columns, value scale, relaxation, collisions and
///        link stacking.
#include "layout/sankey_layout.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace bdg::nymph {

namespace {

/// Node indices of each column, in submission order.
std::vector<std::vector<size_t>> by_column(const std::vector<int>& column, int columns) {
  std::vector<std::vector<size_t>> out(static_cast<size_t>(std::max(columns, 0)));
  for (size_t i = 0; i < column.size(); ++i) {
    out[static_cast<size_t>(column[i])].push_back(i);
  }
  return out;
}

/// Sorts `nodes` top to bottom (ties by index), then moves them apart so
/// no two are closer than `spacing` and all lie in [0, height].
void resolve_collisions(std::vector<size_t>& nodes, std::vector<sankey_box>& box, float spacing, float height) {
  std::stable_sort(nodes.begin(), nodes.end(), [&](size_t a, size_t b) { return box[a].y0 < box[b].y0; });
  float y = 0.0f;
  for (size_t i : nodes) {
    const float dy = y - box[i].y0;
    if (dy > 0.0f) {
      box[i].y0 += dy;
      box[i].y1 += dy;
    }
    y = box[i].y1 + spacing;
  }
  // Pushed past the bottom: move back up from the bottom.
  y = height;
  for (auto it = nodes.rbegin(); it != nodes.rend(); ++it) {
    const size_t i = *it;
    const float dy = box[i].y1 - y;
    if (dy > 0.0f) {
      box[i].y0 -= dy;
      box[i].y1 -= dy;
    }
    y = box[i].y0 - spacing;
  }
}

float centre(const sankey_box& b) {
  return (b.y0 + b.y1) * 0.5f;
}

} // namespace

std::vector<int> sankey_columns(size_t node_count, const std::vector<sankey_link_in>& links) {
  // Longest path from a source, by Kahn's topological order.
  std::vector<size_t> indegree(node_count, 0);
  std::vector<std::vector<size_t>> out(node_count);
  for (const sankey_link_in& l : links) {
    out[l.from].push_back(l.to);
    ++indegree[l.to];
  }
  std::vector<int> depth(node_count, 0);
  std::vector<size_t> ready;
  for (size_t i = 0; i < node_count; ++i) {
    if (indegree[i] == 0) {
      ready.push_back(i);
    }
  }
  size_t done = 0;
  for (size_t k = 0; k < ready.size(); ++k) {
    const size_t i = ready[k];
    ++done;
    for (size_t t : out[i]) {
      depth[t] = std::max(depth[t], depth[i] + 1);
      if (--indegree[t] == 0) {
        ready.push_back(t);
      }
    }
  }
  if (done != node_count) {
    return {};
  }
  const int last = node_count == 0 ? 0 : *std::max_element(depth.begin(), depth.end());
  for (size_t i = 0; i < node_count; ++i) {
    const bool has_in = std::any_of(links.begin(), links.end(), [&](const sankey_link_in& l) { return l.to == i; });
    if (out[i].empty() && has_in) {
      depth[i] = last; // a sink: justified to the right
    }
  }
  return depth;
}

sankey_output layout_sankey(const sankey_input& in) {
  sankey_output o;
  const size_t n = in.node_count;
  o.column = sankey_columns(n, in.links);
  if (o.column.size() != n) {
    o.column.assign(n, 0);
  }
  o.columns = n == 0 ? 0 : *std::max_element(o.column.begin(), o.column.end()) + 1;
  // Node values.
  std::vector<double> in_sum(n, 0.0), out_sum(n, 0.0);
  for (const sankey_link_in& l : in.links) {
    out_sum[l.from] += l.value;
    in_sum[l.to] += l.value;
  }
  o.value.resize(n);
  for (size_t i = 0; i < n; ++i) {
    o.value[i] = std::max(in_sum[i], out_sum[i]);
  }
  std::vector<std::vector<size_t>> columns = by_column(o.column, o.columns);
  // Scale: the fullest column fills the height.
  double ky = 1e300;
  for (const std::vector<size_t>& col : columns) {
    double total = 0.0;
    for (size_t i : col) {
      total += o.value[i];
    }
    if (total > 0.0) {
      const double room = std::max(1.0, static_cast<double>(in.height) - (col.size() - 1.0) * in.node_spacing);
      ky = std::min(ky, room / total);
    }
  }
  if (ky == 1e300) {
    ky = 1.0;
  }
  // Initial positions: stacked in order.
  o.nodes.resize(n);
  const float step = o.columns > 1 ? (in.width - in.node_width) / static_cast<float>(o.columns - 1) : 0.0f;
  for (const std::vector<size_t>& col : columns) {
    float y = 0.0f;
    for (size_t i : col) {
      sankey_box& b = o.nodes[i];
      b.x0 = std::round(static_cast<float>(o.column[i]) * step);
      b.x1 = b.x0 + in.node_width;
      b.y0 = y;
      b.y1 = y + static_cast<float>(o.value[i] * ky);
      y = b.y1 + in.node_spacing;
    }
  }
  // Relaxation: each node moves towards the value-weighted centre of the
  // nodes it is linked to, alternately from the right and from the left.
  for (int iter = 0; iter < in.iterations; ++iter) {
    const float alpha = std::pow(0.99f, static_cast<float>(iter));
    for (int pass = 0; pass < 2; ++pass) {
      const bool from_left = pass == 1;
      for (size_t c = 0; c < columns.size(); ++c) {
        std::vector<size_t>& col = columns[from_left ? c : columns.size() - 1 - c];
        for (size_t i : col) {
          double weighted = 0.0, weight = 0.0;
          for (const sankey_link_in& l : in.links) {
            const size_t other = from_left ? (l.to == i ? l.from : n) : (l.from == i ? l.to : n);
            if (other < n) {
              weighted += centre(o.nodes[other]) * l.value;
              weight += l.value;
            }
          }
          if (weight > 0.0) {
            const float dy = static_cast<float>((weighted / weight - centre(o.nodes[i])) * alpha);
            o.nodes[i].y0 += dy;
            o.nodes[i].y1 += dy;
          }
        }
        resolve_collisions(col, o.nodes, in.node_spacing, in.height);
      }
    }
  }
  // Links: stacked inside each node, ordered by the other end's position.
  std::vector<size_t> order(in.links.size());
  std::iota(order.begin(), order.end(), 0);
  o.links.resize(in.links.size());
  std::vector<float> out_y(n), in_y(n);
  for (size_t i = 0; i < n; ++i) {
    out_y[i] = in_y[i] = o.nodes[i].y0;
  }
  std::vector<size_t> by_target = order, by_source = order;
  std::stable_sort(by_target.begin(), by_target.end(), [&](size_t a, size_t b) {
    return o.nodes[in.links[a].to].y0 < o.nodes[in.links[b].to].y0;
  });
  std::stable_sort(by_source.begin(), by_source.end(), [&](size_t a, size_t b) {
    return o.nodes[in.links[a].from].y0 < o.nodes[in.links[b].from].y0;
  });
  for (size_t k : by_target) {
    const sankey_link_in& l = in.links[k];
    const float w = static_cast<float>(l.value * ky);
    o.links[k].width = w;
    o.links[k].x0 = o.nodes[l.from].x1;
    o.links[k].y0 = out_y[l.from] + w * 0.5f;
    out_y[l.from] += w;
  }
  for (size_t k : by_source) {
    const sankey_link_in& l = in.links[k];
    const float w = o.links[k].width;
    o.links[k].x1 = o.nodes[l.to].x0;
    o.links[k].y1 = in_y[l.to] + w * 0.5f;
    in_y[l.to] += w;
  }
  return o;
}

} // namespace bdg::nymph
