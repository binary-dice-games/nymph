// MIT License © 2026 Binary Dice Games
/// @file sankey.cpp
/// @brief `SankeyDiagram`, `SankeyNode` and `SankeyFlow`.
///
/// Flows come from the diagram's `from` / `to` / `values` lists (so a table
/// maps onto them with $columns) and from `SankeyFlow` children. Nodes are
/// named by the flows, as in mermaid's sankey; a `SankeyNode` only sets a
/// node's label, colour or order. graph_of() turns a node into the layout's
/// input and is the one place source errors come from. The layout is
/// layout_sankey(); bands are drawn as filled polygons on the window's draw
/// list, nodes as bars, labels beside them.
#include "elements/sankey.hpp"
#include <imgui.h>
#include <implot.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "core/error.hpp"
#include "core/limits.hpp"
#include "elements/plot_pick.hpp"
#include "elements/plot_style.hpp"
#include "layout/sankey_layout.hpp"
#include "render/draw_context.hpp"
#include "render/text_style.hpp"
#include "schema/element_class.hpp"

namespace bdg::nymph {

namespace {

constexpr const char* kSankeyItemSlot = "sankey.item";

// ── Children ─────────────────────────────────────────────────────────────────

namespace node_field {
constexpr size_t id = 0, name = 1, color = 2;
}

class sankey_node_class : public element_class {
 public:
  sankey_node_class()
      : element_class(
            "SankeyNode",
            kSankeyItemSlot,
            {},
            {
                text_field("id", "", "The name flows use for the node."),
                text_field("name", "", "Label shown; empty shows the id."),
                color_field("color", "Colour of the node (and of the bands leaving it); empty takes the colormap's."),
            },
            "Optional: sets the label or colour of a node that flows name. Nodes are listed in this order "
            "within their column.") {}

  void check(const node& n) const override {
    if (n.get<std::string>(node_field::id).empty()) {
      fail_at(n, node_field::id, "is empty; give the name the flows use for this node");
    }
  }

  void draw(const node&, draw_context&) const override {} // drawn by SankeyDiagram
};

namespace flow_field {
constexpr size_t from = 0, to = 1, value = 2, color = 3;
}

class sankey_flow_class : public element_class {
 public:
  sankey_flow_class()
      : element_class(
            "SankeyFlow",
            kSankeyItemSlot,
            {},
            {
                text_field("from", "", "Node the quantity leaves."),
                text_field("to", "", "Node it reaches."),
                number_field("value", 0.0, "The quantity; the band's width is proportional to it. Above 0."),
                color_field("color", "Colour of the band; empty follows the diagram's link_color."),
            },
            "A flow of `value` from one node to another. The same flows can be given as the diagram's from / "
            "to / values lists.") {}

  void draw(const node&, draw_context&) const override {} // drawn by SankeyDiagram
};

// ── Graph ────────────────────────────────────────────────────────────────────

namespace diagram_field {
constexpr size_t title = 0, width = 1, height = 2, from = 3, to = 4, values = 5, colormap = 6, link_color = 7,
                 node_width = 8, node_spacing = 9, show_values = 10, value_format = 11;
}

/// `link_color` values.
constexpr int64_t kLinkSource = 0, kLinkTarget = 1, kLinkGray = 2;

const name_table& link_color_names() {
  static const name_table t{{"Source", kLinkSource}, {"Target", kLinkTarget}, {"Gray", kLinkGray}};
  return t;
}

struct graph {
  std::vector<std::string> ids; ///< Per node, in order.
  std::vector<std::string> labels;
  std::vector<std::string> colors; ///< Per node, "" for the colormap's.
  std::vector<sankey_link_in> links;
  std::vector<std::string> link_colors;
};

/// The nodes and links of a SankeyDiagram node; throws nymph::error at the
/// field for any problem.
graph graph_of(const node& n) {
  using namespace diagram_field;
  const element_class& cls = *n.cls;
  graph g;
  std::map<std::string, size_t, std::less<>> index;
  std::vector<const node*> declared; // SankeyNode per node, or null
  auto node_of = [&](const std::string& id) {
    const auto [it, added] = index.emplace(id, g.ids.size());
    if (added) {
      g.ids.push_back(id);
      g.labels.push_back(id);
      g.colors.emplace_back();
      declared.push_back(nullptr);
    }
    return it->second;
  };
  for (const node& child : n.children) {
    if (child.cls->name() != "SankeyNode") {
      continue;
    }
    const std::string& id = child.get<std::string>(node_field::id);
    if (index.count(id) != 0) {
      child.cls->fail_at(
          child,
          node_field::id,
          ": node '" + id + "' is already declared at line " + std::to_string(declared[index[id]]->line));
    }
    const size_t i = node_of(id);
    declared[i] = &child;
    if (const std::string& name = child.get<std::string>(node_field::name); !name.empty()) {
      g.labels[i] = name;
    }
    g.colors[i] = child.get<std::string>(node_field::color);
  }
  // Flows from the lists.
  const auto& from_list = n.get<std::vector<std::string>>(from);
  const auto& to_list = n.get<std::vector<std::string>>(to);
  const auto& value_list = n.get<std::vector<double>>(values);
  if (to_list.size() != from_list.size()) {
    cls.fail_at(
        n, to, "has " + std::to_string(to_list.size()) + " names but from has " + std::to_string(from_list.size()));
  }
  if (value_list.size() != from_list.size()) {
    cls.fail_at(
        n,
        values,
        "has " + std::to_string(value_list.size()) + " values but from has " + std::to_string(from_list.size()));
  }
  auto add = [&](const std::string& a,
                 const std::string& b,
                 double v,
                 const std::string& color,
                 const node& where,
                 size_t from_f,
                 size_t to_f,
                 size_t value_f,
                 const std::string& which) {
    if (a.empty()) {
      where.cls->fail_at(where, from_f, which + "has an empty node name");
    }
    if (b.empty()) {
      where.cls->fail_at(where, to_f, which + "has an empty node name");
    }
    if (a == b) {
      where.cls->fail_at(where, to_f, which + "goes from '" + a + "' to itself");
    }
    if (!(v > 0.0) || !std::isfinite(v)) {
      char text[64];
      std::snprintf(text, sizeof(text), "%g", v);
      where.cls->fail_at(where, value_f, which + "has value " + text + "; a flow's value must be above 0");
    }
    g.links.push_back({node_of(a), node_of(b), v});
    g.link_colors.push_back(color);
  };
  for (size_t k = 0; k < from_list.size(); ++k) {
    add(from_list[k],
        to_list[k],
        value_list[k],
        "",
        n,
        from,
        to,
        values,
        "(flow " + std::to_string(k + 1) + " of the lists) ");
  }
  for (const node& child : n.children) {
    if (child.cls->name() != "SankeyFlow") {
      continue;
    }
    add(child.get<std::string>(flow_field::from),
        child.get<std::string>(flow_field::to),
        child.get<double>(flow_field::value),
        child.get<std::string>(flow_field::color),
        child,
        flow_field::from,
        flow_field::to,
        flow_field::value,
        "");
  }
  if (g.links.empty()) {
    throw error(n.line, n.column, "SankeyDiagram has no flows; give from / to / values, or SankeyFlow children");
  }
  for (size_t i = 0; i < g.ids.size(); ++i) {
    const bool used =
        std::any_of(g.links.begin(), g.links.end(), [&](const sankey_link_in& l) { return l.from == i || l.to == i; });
    if (!used) {
      declared[i]->cls->fail_at(*declared[i], node_field::id, ": node '" + g.ids[i] + "' is in no flow");
    }
  }
  if (g.ids.size() > kMaxSankeyNodes || g.links.size() > kMaxSankeyFlows) {
    throw error(
        n.line,
        n.column,
        "SankeyDiagram has " + std::to_string(g.ids.size()) + " nodes and " + std::to_string(g.links.size()) +
            " flows; at most " + std::to_string(kMaxSankeyNodes) + " and " + std::to_string(kMaxSankeyFlows) +
            " are allowed");
  }
  return g;
}

/// Whether node `goal` can be reached from node `start` along the links.
bool reaches(const graph& g, size_t start, size_t goal) {
  std::vector<bool> seen(g.ids.size(), false);
  std::vector<size_t> stack{start};
  while (!stack.empty()) {
    const size_t i = stack.back();
    stack.pop_back();
    if (i == goal) {
      return true;
    }
    if (seen[i]) {
      continue;
    }
    seen[i] = true;
    for (const sankey_link_in& l : g.links) {
      if (l.from == i) {
        stack.push_back(l.to);
      }
    }
  }
  return false;
}

// ── SankeyDiagram ────────────────────────────────────────────────────────────

class sankey_diagram_class : public element_class {
 public:
  sankey_diagram_class()
      : element_class(
            "SankeyDiagram",
            std::string(kFigureSlot),
            {kSankeyItemSlot},
            with_text_style({
                text_field("title", "", "Title above the diagram; empty for none."),
                number_field(
                    "width",
                    0.0,
                    "Width in pixels; 0 (default) is 600 in a HorizontalLayout and fills elsewhere; a negative "
                    "value is a stretch weight in a HorizontalLayout.",
                    value_range{-1000, 8192}),
                number_field(
                    "height",
                    0.0,
                    "Height in pixels; 0 (default) is the natural height (at least 300, more for many nodes) in a "
                    "VerticalLayout and fills elsewhere; a negative value is a stretch weight in a VerticalLayout.",
                    value_range{-1000, 8192}),
                texts_field("from", "Node each flow leaves, such as $source; with to and values."),
                texts_field("to", "Node each flow reaches, such as $target."),
                numbers_field("values", "Quantity of each flow, such as $amount; each above 0."),
                colormap_field(),
                enum_field(
                    "link_color",
                    kLinkSource,
                    link_color_names(),
                    "Colour of the bands: Source (the node they leave, default), Target (the node they reach) "
                    "or Gray."),
                number_field("node_width", 16.0, "Width of the node bars in pixels.", value_range{2, 200}),
                number_field(
                    "node_spacing",
                    12.0,
                    "Least vertical gap between nodes of a column, in pixels.",
                    value_range{0, 200}),
                bool_field("show_values", true, "When true, each label ends with the node's total."),
                double_format_field("value_format", "%g", "printf format of the totals in the labels (\"%.0f GWh\")."),
            }),
            "A Sankey diagram: quantities flowing from node to node as bands whose width is proportional to the "
            "amount (energy, budgets, traffic, conversions). Nodes are placed in columns automatically. Flows "
            "come from the from / to / values lists or SankeyFlow children; SankeyNode sets a node's label and "
            "colour. Cycles are not allowed.") {}

  void check(const node& n) const override {
    const graph g = graph_of(n);
    if (sankey_columns(g.ids.size(), g.links).size() == g.ids.size()) {
      return;
    }
    // A cycle: name the first flow on it.
    size_t k = 0;
    const size_t list_flows = n.get<std::vector<std::string>>(diagram_field::from).size();
    for (k = 0; k < g.links.size(); ++k) {
      if (reaches(g, g.links[k].to, g.links[k].from)) {
        break;
      }
    }
    const std::string message = "makes a cycle: '" + g.ids[g.links[k].to] + "' leads back to '" +
        g.ids[g.links[k].from] + "'; a Sankey diagram flows one way";
    if (k < list_flows) {
      fail_at(n, diagram_field::to, "(flow " + std::to_string(k + 1) + " of the lists) " + message);
    }
    size_t flow = list_flows;
    for (const node& child : n.children) {
      if (child.cls->name() == "SankeyFlow" && flow++ == k) {
        child.cls->fail_at(child, flow_field::to, message);
      }
    }
  }

  /// Natural size: 600 wide; 300 high, or 40 pixels per node of the
  /// fullest column, plus the title.
  ImVec2 measure(const node& n, draw_context&, float) const override {
    const graph g = graph_of(n);
    const std::vector<int> columns = sankey_columns(g.ids.size(), g.links);
    std::map<int, int> count;
    int most = 0;
    for (int c : columns) {
      most = std::max(most, ++count[c]);
    }
    const float title = n.get<std::string>(diagram_field::title).empty() ? 0.0f : ImGui::GetTextLineHeight() + 6.0f;
    return ImVec2(600.0f, std::max(300.0f, 40.0f * static_cast<float>(most)) + title);
  }

  void draw(const node& n, draw_context& dc) const override {
    using namespace diagram_field;
    const graph g = graph_of(n);
    const rect r = dc.rect_of(n);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRect(r.min, r.max, true);
    const float line = ImGui::GetTextLineHeight();
    const ImU32 text_col = ImGui::GetColorU32(ImGuiCol_Text);
    float top = r.min.y;
    if (const std::string& t = n.get<std::string>(title); !t.empty()) {
      const ImVec2 ts = ImGui::CalcTextSize(t.c_str());
      dl->AddText(ImVec2(std::floor((r.min.x + r.max.x - ts.x) * 0.5f), r.min.y + 2.0f), text_col, t.c_str());
      top += line + 6.0f;
    }
    // Half a line of room above and below, so labels of the outer nodes fit.
    const ImVec2 origin(r.min.x + 2.0f, top + line * 0.5f);
    sankey_input in;
    in.node_count = g.ids.size();
    in.links = g.links;
    in.width = std::max(10.0f, r.width() - 4.0f);
    in.height = std::max(10.0f, r.max.y - origin.y - line * 0.5f);
    in.node_width = static_cast<float>(n.get<double>(node_width));
    in.node_spacing = static_cast<float>(n.get<double>(node_spacing));
    const sankey_output out = layout_sankey(in);

    // Node colours.
    const int64_t cmap_field = n.get<int64_t>(colormap);
    const ImPlotColormap cmap =
        cmap_field == kColormapDefault ? ImPlotColormap_Deep : static_cast<ImPlotColormap>(cmap_field);
    const int cmap_size = ImPlot::GetColormapSize(cmap);
    // Nodes without a colour take the colormap's in turn.
    std::vector<ImVec4> color(g.ids.size());
    int next = 0;
    for (size_t i = 0; i < g.ids.size(); ++i) {
      color[i] =
          g.colors[i].empty() ? ImPlot::GetColormapColor(next++ % cmap_size, cmap) : parse_hex_color(g.colors[i]);
    }
    // Bands, under the nodes.
    const int64_t mode = n.get<int64_t>(link_color);
    for (size_t k = 0; k < g.links.size(); ++k) {
      const sankey_link_in& l = g.links[k];
      ImVec4 c = !g.link_colors[k].empty() ? parse_hex_color(g.link_colors[k])
          : mode == kLinkTarget            ? color[l.to]
          : mode == kLinkGray              ? ImGui::GetStyleColorVec4(ImGuiCol_Text)
                                           : color[l.from];
      c.w *= g.link_colors[k].empty() ? (mode == kLinkGray ? 0.25f : 0.45f) : 1.0f;
      draw_band(dl, out.links[k], origin, ImGui::ColorConvertFloat4ToU32(c));
    }
    // Nodes and labels: beside the node, towards the middle of the diagram.
    const std::string& fmt = n.get<std::string>(value_format);
    const bool values_shown = n.get<bool>(show_values);
    for (size_t i = 0; i < g.ids.size(); ++i) {
      const sankey_box& b = out.nodes[i];
      const ImVec2 a(std::floor(origin.x + b.x0), std::floor(origin.y + b.y0));
      const ImVec2 z(std::floor(origin.x + b.x1), std::max(a.y + 1.0f, std::floor(origin.y + b.y1 + 0.5f)));
      dl->AddRectFilled(a, z, ImGui::ColorConvertFloat4ToU32(color[i]));
      std::string label = g.labels[i];
      if (values_shown) {
        char text[64];
        std::snprintf(text, sizeof(text), fmt.c_str(), out.value[i]);
        label += std::string("  ") + text;
      }
      const ImVec2 ts = ImGui::CalcTextSize(label.c_str());
      const bool right = b.x0 < in.width * 0.5f; // as d3: left half labelled on the right
      const float x = right ? z.x + 6.0f : a.x - 6.0f - ts.x;
      const float y = std::floor((a.y + z.y - ts.y) * 0.5f);
      dl->AddText(ImVec2(std::floor(x), y), text_col, label.c_str());
    }
    offer_hover(n, dc, g, out, origin, r);
    finish_picks(dc, dl);
    dl->PopClipRect();
  }

 private:
  /// In a live view, offers the node under the pointer, else the flow
  /// (nodes are drawn over the bands).
  static void offer_hover(
      const node& n,
      draw_context& dc,
      const graph& g,
      const sankey_output& out,
      const ImVec2& origin,
      const rect& r) {
    if (!dc.interactive() || !ImGui::IsWindowHovered()) {
      return;
    }
    const ImVec2 p = ImGui::GetIO().MousePos;
    if (p.x < r.min.x || p.x > r.max.x || p.y < r.min.y || p.y > r.max.y) {
      return;
    }
    const auto value_text = [&](double v) {
      char buf[64];
      std::snprintf(buf, sizeof(buf), n.get<std::string>(diagram_field::value_format).c_str(), v);
      return std::string(buf);
    };
    const ImU32 col = ImGui::GetColorU32(ImGuiCol_Text);
    for (size_t i = 0; i < g.ids.size(); ++i) {
      const sankey_box& b = out.nodes[i];
      const ImVec2 a(std::floor(origin.x + b.x0), std::floor(origin.y + b.y0));
      const ImVec2 z(std::floor(origin.x + b.x1), std::max(a.y + 1.0f, std::floor(origin.y + b.y1 + 0.5f)));
      if (p.x >= a.x && p.x <= z.x && p.y >= a.y && p.y <= z.y) {
        pick hit;
        hit.element = &n;
        hit.type = "SankeyNode";
        hit.label = g.labels[i];
        hit.index = static_cast<int>(i);
        hit.values = {{"id", g.ids[i]}, {"value", value_text(out.value[i])}};
        offer_rect_px(dc, p, std::move(hit), a, z, col);
        return;
      }
    }
    // Flows: the band's centre line at the pointer's x, as draw_band draws it.
    for (size_t k = g.links.size(); k-- > 0;) {
      const sankey_link_out& l = out.links[k];
      const float x0 = origin.x + l.x0, x1 = origin.x + l.x1;
      if (p.x < x0 || p.x > x1 || x1 <= x0) {
        continue;
      }
      // x(t) is monotonic: bisect for the t at the pointer.
      float lo = 0.0f, hi = 1.0f;
      const float xm = (x0 + x1) * 0.5f;
      for (int it = 0; it < 30; ++it) {
        const float t = (lo + hi) * 0.5f, u = 1.0f - t;
        const float x = u * u * u * x0 + 3 * u * u * t * xm + 3 * u * t * t * xm + t * t * t * x1;
        (x < p.x ? lo : hi) = t;
      }
      const float t = (lo + hi) * 0.5f, u = 1.0f - t;
      const float y0 = origin.y + l.y0, y1 = origin.y + l.y1;
      const float y = u * u * u * y0 + 3 * u * u * t * y0 + 3 * u * t * t * y1 + t * t * t * y1;
      if (std::abs(p.y - y) <= std::max(0.5f, l.width * 0.5f)) {
        const sankey_link_in& in = g.links[k];
        pick_candidate c;
        c.anchor = ImVec2(p.x, y);
        c.color = col;
        c.hit.element = &n;
        c.hit.type = "SankeyFlow";
        c.hit.label = g.labels[in.from] + " -> " + g.labels[in.to];
        c.hit.index = static_cast<int>(k);
        c.hit.values = {{"from", g.ids[in.from]}, {"to", g.ids[in.to]}, {"value", value_text(in.value)}};
        dc.offer_pick(std::move(c));
        return;
      }
    }
  }

  /// A band of constant vertical thickness along a horizontal S curve.
  static void draw_band(ImDrawList* dl, const sankey_link_out& l, const ImVec2& origin, ImU32 col) {
    constexpr int kSegments = 32;
    const float x0 = origin.x + l.x0, x1 = origin.x + l.x1;
    const float y0 = origin.y + l.y0, y1 = origin.y + l.y1;
    const float half = std::max(0.5f, l.width * 0.5f);
    ImVec2 pts[2 * (kSegments + 1)];
    for (int s = 0; s <= kSegments; ++s) {
      // d3's link shape: a cubic with both control points at mid-x, at the
      // start and end heights.
      const float t = static_cast<float>(s) / kSegments;
      const float u = 1.0f - t;
      const float xm = (x0 + x1) * 0.5f;
      const float x = u * u * u * x0 + 3 * u * u * t * xm + 3 * u * t * t * xm + t * t * t * x1;
      const float y = u * u * u * y0 + 3 * u * u * t * y0 + 3 * u * t * t * y1 + t * t * t * y1;
      pts[s] = ImVec2(x, y - half);
      pts[2 * (kSegments + 1) - 1 - s] = ImVec2(x, y + half);
    }
    dl->AddConcavePolyFilled(pts, 2 * (kSegments + 1), col);
  }
};

} // namespace

void register_sankey(registry& reg) {
  reg.add(std::make_unique<sankey_diagram_class>());
  reg.add(std::make_unique<sankey_node_class>());
  reg.add(std::make_unique<sankey_flow_class>());
}

} // namespace bdg::nymph
