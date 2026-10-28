// MIT License © 2026 Binary Dice Games
/// @file uml_common.cpp
/// @brief The UML diagram base class, the child base class, `Note`, and
///        register_uml().
#include "elements/uml/uml_common.hpp"
#include <imgui.h>
#include <imgui_internal.h> // ImHashStr: ImUml hashes item ids with it
#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include "core/error.hpp"
#include "core/limits.hpp"
#include "core/suggest.hpp"
#include "elements/plot_pick.hpp"
#include "elements/plot_style.hpp"
#include "fonts/embedded_fonts.hpp"
#include "render/text_style.hpp"
#include "shapes/shape_catalog.hpp"

namespace bdg::nymph::uml {

namespace {

diagram_kind g_current_kind = diagram_kind::class_diagram;

/// Sets current_kind() while a diagram's ImUml scope is open.
class kind_scope {
 public:
  explicit kind_scope(diagram_kind k) : previous_(g_current_kind) {
    g_current_kind = k;
  }
  ~kind_scope() {
    g_current_kind = previous_;
  }
  kind_scope(const kind_scope&) = delete;
  kind_scope& operator=(const kind_scope&) = delete;

 private:
  diagram_kind previous_;
};

const name_table& direction_names() {
  static const name_table t{
      {"TopDown", ImUmlDirection_TopDown},
      {"BottomUp", ImUmlDirection_BottomUp},
      {"LeftRight", ImUmlDirection_LeftRight},
      {"RightLeft", ImUmlDirection_RightLeft},
  };
  return t;
}

const name_table& edge_style_names() {
  static const name_table t{{"Spline", ImUmlEdgeStyle_Spline}, {"Polyline", ImUmlEdgeStyle_Polyline}};
  return t;
}

const name_table& tree_edge_style_names() {
  static const name_table t{
      {"Elbow", ImUmlEdgeStyle_Elbow}, {"Curve", ImUmlEdgeStyle_Curve}, {"Straight", ImUmlEdgeStyle_Straight}};
  return t;
}

const name_table& note_side_names() {
  static const name_table t{{"Left", ImUmlSide_Left}, {"Right", ImUmlSide_Right}};
  return t;
}

bool is_graph(diagram_kind k) {
  return k != diagram_kind::sequence;
}

using begin_fn = bool (*)(const char*, const ImVec2&, ImUmlDiagramFlags);
using end_fn = void (*)();

begin_fn begin_of(diagram_kind k) {
  switch (k) {
    case diagram_kind::sequence:
      return &ImUml::BeginSequenceDiagram;
    case diagram_kind::state:
      return &ImUml::BeginStateDiagram;
    case diagram_kind::activity:
      return &ImUml::BeginActivityDiagram;
    case diagram_kind::use_case:
      return &ImUml::BeginUseCaseDiagram;
    case diagram_kind::flowchart:
      return &ImUml::BeginFlowchart;
    case diagram_kind::er:
      return &ImUml::BeginERDiagram;
    case diagram_kind::data_flow:
      return &ImUml::BeginDataFlowDiagram;
    case diagram_kind::network:
      return &ImUml::BeginNetworkDiagram;
    case diagram_kind::component:
    case diagram_kind::deployment:
    case diagram_kind::package:
      return &ImUml::BeginShapeDiagram;
    case diagram_kind::tree:
    case diagram_kind::mind_map:
    case diagram_kind::org_chart:
      return &ImUml::BeginTreeDiagram;
    default:
      return &ImUml::BeginClassDiagram;
  }
}

end_fn end_of(diagram_kind k) {
  switch (k) {
    case diagram_kind::sequence:
      return &ImUml::EndSequenceDiagram;
    case diagram_kind::state:
      return &ImUml::EndStateDiagram;
    case diagram_kind::activity:
      return &ImUml::EndActivityDiagram;
    case diagram_kind::use_case:
      return &ImUml::EndUseCaseDiagram;
    case diagram_kind::flowchart:
      return &ImUml::EndFlowchart;
    case diagram_kind::er:
      return &ImUml::EndERDiagram;
    case diagram_kind::data_flow:
      return &ImUml::EndDataFlowDiagram;
    case diagram_kind::network:
      return &ImUml::EndNetworkDiagram;
    case diagram_kind::component:
    case diagram_kind::deployment:
    case diagram_kind::package:
      return &ImUml::EndShapeDiagram;
    case diagram_kind::tree:
    case diagram_kind::mind_map:
    case diagram_kind::org_chart:
      return &ImUml::EndTreeDiagram;
    default:
      return &ImUml::EndClassDiagram;
  }
}

std::vector<field_desc> diagram_fields(diagram_kind kind, std::vector<field_desc> extra) {
  std::vector<field_desc> f{
      text_field("title", "", "Title drawn above the diagram, at the full text size; empty for none."),
      number_field(
          "width",
          0.0,
          "Width in pixels. 0 (default) is the natural width in a layout; as root it fills the image. In a "
          "HorizontalLayout a negative value is a stretch weight.",
          value_range{-1000, 8192}),
      number_field(
          "height",
          0.0,
          "Height in pixels. 0 (default) is the natural height in a layout; as root it fills the image. In a "
          "VerticalLayout a negative value is a stretch weight.",
          value_range{-1000, 8192}),
  };
  if (is_tree_kind(kind)) {
    if (kind != diagram_kind::mind_map) {
      f.push_back(enum_field(
          "direction",
          ImUmlDirection_TopDown,
          direction_names(),
          "Direction the levels grow: TopDown (default), BottomUp, LeftRight or RightLeft."));
    }
    f.push_back(enum_field(
        "edge_style",
        kind == diagram_kind::mind_map ? ImUmlEdgeStyle_Curve : ImUmlEdgeStyle_Elbow,
        tree_edge_style_names(),
        "Shape of the connectors: Elbow (right angles), Curve (smooth) or Straight."));
    f.push_back(number_field(
        "node_spacing", 0.0, "Pixels between sibling subtrees; 0 uses the default (20).", value_range{0, 500}));
    f.push_back(number_field(
        "rank_spacing", 0.0, "Pixels between levels; 0 uses the default (40).", value_range{0, 500}));
  } else if (is_graph(kind)) {
    f.push_back(enum_field(
        "direction",
        kind == diagram_kind::use_case || kind == diagram_kind::data_flow ? ImUmlDirection_LeftRight
                                                                            : ImUmlDirection_TopDown,
        direction_names(),
        "Direction of the ranks (Graphviz dot's rankdir; mermaid's TB, BT, LR, RL)."));
    f.push_back(enum_field(
        "edge_style",
        ImUmlEdgeStyle_Spline,
        edge_style_names(),
        "Shape of the edges: smooth curves (Spline) or straight segments (Polyline)."));
    f.push_back(number_field(
        "node_spacing",
        0.0,
        "Pixels between nodes in one rank (dot's nodesep); 0 uses the default (30).",
        value_range{0, 500}));
    f.push_back(number_field(
        "rank_spacing",
        0.0,
        "Pixels between ranks (dot's ranksep); 0 uses the default (46).",
        value_range{0, 500}));
  } else {
    f.push_back(number_field(
        "node_spacing",
        0.0,
        "Least pixels between participant boxes; 0 uses the default (40). Columns widen to fit message text.",
        value_range{0, 500}));
  }
  for (field_desc& e : extra) {
    f.push_back(std::move(e));
  }
  f.push_back(number_field(
      "min_zoom",
      0.5,
      "Smallest scale the diagram may be drawn at to fit its space. Needing less is an error that gives the "
      "size the diagram needs.",
      value_range{0.05, 1}));
  f.push_back(number_field(
      "max_zoom",
      1.0,
      "Largest scale: 1 (default) never enlarges a small diagram; 2 lets it grow to fill a large space.",
      value_range{0.1, 8}));
  f.push_back(color_field("line_color", "Default colour of edges and node outlines; empty for the theme's."));
  f.push_back(color_field("fill_color", "Default fill of nodes; empty for the theme's."));
  return with_text_style(std::move(f));
}

std::string px(float v) {
  return std::to_string(static_cast<long>(std::lround(v)));
}

/// The `-Italic` sibling of the current font, if it is loaded.
ImFont* italic_sibling(const draw_context& dc) {
  const std::string sibling = italic_sibling_name(dc.font_name(ImGui::GetFont()));
  return sibling.empty() ? nullptr : dc.font(sibling);
}

} // namespace

diagram_kind current_kind() {
  return g_current_kind;
}

// ── item_class ───────────────────────────────────────────────────────────────

item_class::item_class(
    std::string name,
    std::string slot,
    std::vector<std::string> accepts,
    std::vector<field_desc> fields,
    std::string description,
    std::vector<std::string> ref_fields)
    : element_class(
          std::move(name),
          std::move(slot),
          std::move(accepts),
          with_text_style(std::move(fields)),
          std::move(description)) {
  id_field_ = field_index("id");
  for (const std::string& r : ref_fields) {
    ref_fields_.push_back(field(r));
  }
}

size_t item_class::field(std::string_view name) const {
  const int i = field_index(name);
  if (i < 0) {
    throw std::logic_error(std::string(this->name()) + " has no field " + std::string(name));
  }
  return static_cast<size_t>(i);
}

const std::string& id_of(const node& n) {
  static const std::string empty;
  const auto* item = dynamic_cast<const item_class*>(n.cls);
  if (item == nullptr || item->id_field() < 0) {
    return empty;
  }
  return n.get<std::string>(static_cast<size_t>(item->id_field()));
}

const node* id_table::find(std::string_view id) const {
  const auto it = by_id.find(id);
  return it == by_id.end() ? nullptr : it->second;
}

const node& id_table::resolve(const node& n, size_t field, const std::string& id) const {
  if (id.empty()) {
    n.cls->fail_at(n, field, "is empty; it must be the id of a node of this diagram");
  }
  const node* target = find(id);
  if (target == nullptr) {
    std::string list;
    for (size_t i = 0; i < ids.size() && i < 12; ++i) {
      list += (i ? ", " : "") + ids[i];
    }
    if (ids.size() > 12) {
      list += ", ...";
    }
    n.cls->fail_at(
        n,
        field,
        ": unknown id '" + id + "'" + did_you_mean(id, ids) + (ids.empty() ? "" : " (ids: " + list + ")"));
  }
  return *target;
}

void walk(const node& n, const std::function<void(const node&, size_t depth)>& f) {
  std::function<void(const node&, size_t)> visit = [&](const node& parent, size_t depth) {
    for (const node& child : parent.children) {
      f(child, depth);
      visit(child, depth + 1);
    }
  };
  visit(n, 1);
}

// ── diagram_class ────────────────────────────────────────────────────────────

diagram_class::diagram_class(
    std::string name,
    diagram_kind kind,
    const char* item_slot,
    std::vector<field_desc> extra_fields,
    std::string description,
    std::vector<std::string> extra_slots)
    : element_class(
          std::move(name),
          std::string(kFigureSlot),
          [&] {
            std::vector<std::string> slots{item_slot};
            if (!is_tree_kind(kind)) {
              slots.push_back(kUmlNoteSlot);
            }
            for (std::string& s : extra_slots) {
              slots.push_back(std::move(s));
            }
            return slots;
          }(),
          diagram_fields(kind, std::move(extra_fields)),
          std::move(description)),
      kind_(kind) {
  title_ = field("title");
  min_zoom_ = field("min_zoom");
  max_zoom_ = field("max_zoom");
  line_color_ = field("line_color");
  fill_color_ = field("fill_color");
  node_spacing_ = field("node_spacing");
  direction_ = field_index("direction");
  edge_style_ = field_index("edge_style");
  rank_spacing_ = field_index("rank_spacing");
}

size_t diagram_class::field(std::string_view name) const {
  const int i = field_index(name);
  if (i < 0) {
    throw std::logic_error(std::string(this->name()) + " has no field " + std::string(name));
  }
  return static_cast<size_t>(i);
}

void diagram_class::check(const node& n) const {
  id_table ids;
  size_t nodes = 0, edges = 0, rows = 0;
  walk(n, [&](const node& child, size_t depth) {
    const size_t max_depth = is_tree_kind(kind_) ? kMaxTreeDepth : kMaxUmlNesting;
    if (depth > max_depth) {
      throw error(
          child.line,
          child.column,
          std::string(child.cls->name()) + " is nested " + std::to_string(depth) + " deep; at most " +
              std::to_string(max_depth) + " levels are allowed");
    }
    const auto* item = dynamic_cast<const item_class*>(child.cls);
    if (item == nullptr) {
      return;
    }
    ++rows;
    edges += item->is_edge() ? 1 : 0;
    if (item->id_field() < 0) {
      return;
    }
    const size_t idf = static_cast<size_t>(item->id_field());
    const std::string& id = child.get<std::string>(idf);
    if (id.empty() && item->optional_id()) {
      ++nodes;
      return;
    }
    if (id.empty()) {
      child.cls->fail_at(child, idf, "is empty; every node needs an id that edges can name");
    }
    if (const node* other = ids.find(id)) {
      child.cls->fail_at(
          child,
          idf,
          ": id '" + id + "' is already used by the " + std::string(other->cls->name()) + " at line " +
              std::to_string(other->line) + "; ids must be unique in a diagram");
    }
    ids.by_id.emplace(id, &child);
    ids.ids.push_back(id);
    ++nodes;
  });
  if (nodes > kMaxUmlNodes) {
    throw error(
        n.line,
        n.column,
        std::string(name()) + " has " + std::to_string(nodes) + " nodes; at most " + std::to_string(kMaxUmlNodes) +
            " are allowed");
  }
  if (edges > kMaxUmlEdges) {
    throw error(
        n.line,
        n.column,
        std::string(name()) + " has " + std::to_string(edges) + " edges; at most " + std::to_string(kMaxUmlEdges) +
            " are allowed");
  }
  if (kind_ == diagram_kind::sequence && rows > kMaxUmlRows) {
    throw error(
        n.line,
        n.column,
        std::string(name()) + " has " + std::to_string(rows) + " rows; at most " + std::to_string(kMaxUmlRows) +
            " are allowed");
  }
  walk(n, [&](const node& child, size_t) {
    const auto* item = dynamic_cast<const item_class*>(child.cls);
    if (item == nullptr) {
      return;
    }
    for (const size_t f : item->ref_fields()) {
      if (child.cls->fields()[f].type == field_type::text_array) {
        for (const std::string& id : child.get<std::vector<std::string>>(f)) {
          ids.resolve(child, f, id);
        }
      } else {
        ids.resolve(child, f, child.get<std::string>(f));
      }
    }
  });
  check_rules(n, ids);
}

ImUmlDiagramInfo diagram_class::submit(const node& n, draw_context& dc, const rect* r) const {
  const kind_scope scope(kind_);
  ImUmlStyle& style = ImUml::GetStyle();
  ImFont* const previous_italic = style.ItalicFont;
  style.ItalicFont = italic_sibling(dc);
  int colors = 0;
  const ImU32 line = color_u32(n.get<std::string>(line_color_));
  const ImU32 fill = color_u32(n.get<std::string>(fill_color_));
  if (line != 0) {
    ImUml::PushStyleColor(ImUmlCol_Line, line);
    ImUml::PushStyleColor(ImUmlCol_NodeBorder, line);
    colors += 2;
  }
  if (fill != 0) {
    ImUml::PushStyleColor(ImUmlCol_NodeFill, fill);
    ++colors;
  }
  ImUml::PushStyleVar(ImUmlStyleVar_MaxZoom, static_cast<float>(n.get<double>(max_zoom_)));
  const std::string id = n.get<std::string>(title_) + "##uml";
  ImVec2 size(0.0f, 0.0f);
  ImUmlDiagramFlags flags = ImUmlDiagramFlags_MeasureOnly;
  if (r != nullptr) {
    size = ImVec2(r->width(), r->height());
    // A live view pans and zooms the diagram (spec/04_web_viewer).
    flags = dc.interactive() ? ImUmlDiagramFlags_Interactive : ImUmlDiagramFlags_None;
    if (dc.edit_mode()) {
      flags |= ImUmlDiagramFlags_CaptureText | ImUmlDiagramFlags_CaptureItems;
      dc.clear_item_ranges(); // what the measuring passes recorded no longer applies
    }
    if (dc.show_limits()) {
      flags |= ImUmlDiagramFlags_ShowOutside;
    }
    // The saved view (the `view` key).
    if (const element_view* v = dc.view_of(n); v != nullptr && dc.apply_view() && (v->zoom || v->center)) {
      const float nan = std::numeric_limits<float>::quiet_NaN();
      ImUml::SetNextDiagramView(
          v->zoom ? static_cast<float>(*v->zoom) : 1.0f,
          v->center ? ImVec2(static_cast<float>((*v->center)[0]), static_cast<float>((*v->center)[1]))
                    : ImVec2(nan, nan));
    }
    if (const std::optional<pinch_gesture> p = dc.take_pinch(*r)) {
      ImUml::SetNextDiagramPinch(p->from, p->to, p->scale);
    }
    ImGui::SetCursorScreenPos(r->min);
  }
  if (begin_of(kind_)(id.c_str(), size, flags)) {
    if (direction_ >= 0) {
      ImUml::SetupLayout(
          static_cast<ImUmlDirection>(n.get<int64_t>(static_cast<size_t>(direction_))),
          static_cast<ImUmlEdgeStyle>(n.get<int64_t>(static_cast<size_t>(edge_style_))));
    }
    ImUml::SetupSpacing(
        static_cast<float>(n.get<double>(node_spacing_)),
        rank_spacing_ >= 0 ? static_cast<float>(n.get<double>(static_cast<size_t>(rank_spacing_))) : 0.0f);
    setup(n);
    dc.draw_children(n);
    end_of(kind_)();
  }
  const ImUmlDiagramInfo info = ImUml::GetLastDiagramInfo();
  ImUml::PopStyleVar();
  ImUml::PopStyleColor(colors);
  style.ItalicFont = previous_italic;
  return info;
}

ImVec2 diagram_class::measure(const node& n, draw_context& dc, float width) const {
  (void)width;
  return submit(n, dc, nullptr).NaturalSize;
}

void diagram_class::arrange(const node& n, draw_context& dc, const rect& r) const {
  const ImUmlDiagramInfo info = submit(n, dc, nullptr);
  const ImVec2 c = info.ContentSize;
  if (c.x <= 0.0f || c.y <= 0.0f) {
    return;
  }
  const float min_zoom = static_cast<float>(n.get<double>(min_zoom_));
  const float available_h = r.height() - info.TitleHeight;
  const float zoom = std::min(r.width() / c.x, available_h / c.y);
  if (zoom + 1e-4f < min_zoom) {
    throw error(
        n.line,
        n.column,
        std::string(name()) + " needs " + px(c.x) + " x " + px(c.y + info.TitleHeight) + " px (at min_zoom " +
            px(min_zoom * 100.0f) + "%: " + px(c.x * min_zoom) + " x " + px(c.y * min_zoom + info.TitleHeight) +
            "), has " + px(r.width()) + " x " + px(r.height()) +
            "; enlarge the image or the diagram's space, lower min_zoom, or split the diagram");
  }
}

std::vector<std::string> diagram_class::implied_fonts(const node& n, const std::string& font) const {
  (void)n;
  std::string sibling = italic_sibling_name(font);
  if (sibling.empty()) {
    return {};
  }
  return {std::move(sibling)};
}

void diagram_class::draw(const node& n, draw_context& dc) const {
  const rect r = dc.rect_of(n);
  dc.add_input_area(r);
  const ImUmlDiagramInfo info = submit(n, dc, &r);
  if (dc.edit_mode()) {
    record_texts(n, dc, info);
  }
  if (dc.interactive()) {
    offer_hovered(n, dc, info);
    finish_picks(dc, ImGui::GetWindowDrawList());
    plot_view_record view;
    view.element = &n;
    view.kind = view_kind::diagram;
    view.area = r;
    view.zoom = info.ViewZoom;
    view.center = {info.ViewCenter.x, info.ViewCenter.y};
    dc.record_view(std::move(view));
  }
}

namespace {

/// The text field of `item` that shows `shown`, or "". Fields that name
/// other nodes or hold no display text are not considered; `id` is what a
/// node shows when its own text is empty, which then belongs to the first of
/// text / label / name / title that is empty.
std::string field_showing(const node& item, const std::string& shown) {
  static constexpr std::string_view kNotText[] = {"id", "from", "to", "font", "rank", "over", "color", "fill_color"};
  const std::span<const field_desc> fields = item.cls->fields();
  for (size_t i = 0; i < fields.size(); ++i) {
    if (fields[i].type != field_type::text || !std::holds_alternative<std::string>(item.values[i])) {
      continue;
    }
    if (std::find(std::begin(kNotText), std::end(kNotText), fields[i].name) != std::end(kNotText)) {
      continue;
    }
    if (item.get<std::string>(i) == shown) {
      return fields[i].name;
    }
  }
  if (!id_of(item).empty() && id_of(item) == shown) {
    for (const char* name : {"text", "label", "name", "title"}) {
      const int i = item.cls->field_index(name);
      if (i >= 0 && item.get<std::string>(static_cast<size_t>(i)).empty()) {
        return name;
      }
    }
  }
  return {};
}

/// The element that made item `item` of ImUmlTextRunKind_ `kind`, the
/// narrowest if groups nest; null for none.
const node* element_of_item(const draw_context& dc, int kind, int item) {
  static constexpr int kind_slot[] = {-1, 0, 1, 2, -1}; // ImUmlTextRunKind_ -> node / edge / event range
  if (kind <= ImUmlTextRunKind_None || kind > ImUmlTextRunKind_Event) {
    return nullptr;
  }
  const int slot = kind_slot[kind];
  const item_range* best = nullptr;
  for (const item_range& r : dc.item_ranges()) {
    if (r.first[slot] <= item && item < r.last[slot] &&
        (best == nullptr || r.last[slot] - r.first[slot] < best->last[slot] - best->first[slot])) {
      best = &r;
    }
  }
  return best != nullptr ? best->element : nullptr;
}

} // namespace

void diagram_class::record_texts(const node& n, draw_context& dc, const ImUmlDiagramInfo& info) const {
  // The shapes first: a text over its own shape is recorded later, so it is on top.
  for (int i = 0; i < info.ItemAreaCount; ++i) {
    const ImUmlItemArea& a = info.ItemAreas[i];
    if (const node* element = element_of_item(dc, a.Kind, a.Item)) {
      dc.record_target(*element, {}, rect{a.Min, a.Max});
    }
  }
  for (int i = 0; i < info.TextRunCount; ++i) {
    const ImUmlTextRun& run = info.TextRuns[i];
    const rect area{run.Min, run.Max};
    const std::string shown = run.Text;
    if (run.Kind == ImUmlTextRunKind_Title) {
      dc.record_target(n, "title", area, shown);
      continue;
    }
    const node* element = element_of_item(dc, run.Kind, run.Item);
    if (element == nullptr) {
      continue;
    }
    std::string field = field_showing(*element, shown);
    std::string text = shown;
    if (field.empty() && shown.size() > 3 && std::isdigit(static_cast<unsigned char>(shown[0]))) {
      // An autonumbered message: "3. text".
      const size_t dot = shown.find(". ");
      if (dot != std::string::npos && dot <= 4) {
        text = shown.substr(dot + 2);
        field = field_showing(*element, text);
      }
    }
    if (!field.empty()) {
      dc.record_target(*element, field, area, text);
    }
  }
}

void diagram_class::offer_hovered(const node& n, draw_context& dc, const ImUmlDiagramInfo& info) const {
  if (info.HoveredNode < 0 && info.HoveredEdge < 0) {
    return;
  }
  const auto text = [](const node& c, const char* field) -> std::string {
    const int i = c.cls->field_index(field);
    return i < 0 ? std::string() : c.get<std::string>(static_cast<size_t>(i));
  };
  const node* hit = nullptr;
  walk(n, [&](const node& c, size_t) {
    if (hit != nullptr) {
      return;
    }
    if (info.HoveredNode >= 0) {
      const std::string& id = id_of(c);
      if (!id.empty() && ImHashStr(id.c_str()) == info.HoveredNodeId) {
        hit = &c;
      }
    } else {
      const std::string from = text(c, "from"), to = text(c, "to");
      if (!from.empty() && ImHashStr(from.c_str()) == info.HoveredEdgeFrom &&
          ImHashStr(to.c_str()) == info.HoveredEdgeTo) {
        hit = &c;
      }
    }
  });
  if (hit == nullptr) {
    return;
  }
  pick_candidate c;
  c.color = ImGui::GetColorU32(ImGuiCol_Text);
  c.hit.element = hit;
  c.hit.type = std::string(hit->cls->name());
  if (info.HoveredNode >= 0) {
    c.hit.index = info.HoveredNode;
    c.hit.label = id_of(*hit);
    c.hit.values = {{"id", id_of(*hit)}};
    for (const char* field : {"name", "text"}) {
      if (const std::string v = text(*hit, field); !v.empty()) {
        c.hit.values.push_back({field, v});
      }
    }
    c.outline = true;
    c.outline_min = info.HoveredMin;
    c.outline_max = info.HoveredMax;
  } else {
    c.hit.index = info.HoveredEdge;
    c.hit.label = text(*hit, "from") + " -> " + text(*hit, "to");
    c.hit.values = {{"from", text(*hit, "from")}, {"to", text(*hit, "to")}};
    if (const std::string v = text(*hit, "label"); !v.empty()) {
      c.hit.values.push_back({"label", v});
    }
  }
  c.anchor = ImVec2((info.HoveredMin.x + info.HoveredMax.x) * 0.5f, (info.HoveredMin.y + info.HoveredMax.y) * 0.5f);
  dc.offer_pick(std::move(c));
}

// ── Field helpers ────────────────────────────────────────────────────────────

const name_table& shape_table() {
  static const name_table t = [] {
    std::vector<name_entry> entries;
    int64_t i = 0;
    for (const std::string& name : shape_names()) {
      entries.push_back({name, i++});
    }
    return name_table(std::move(entries));
  }();
  return t;
}

field_desc shape_field(const char* def, const std::string& description) {
  return enum_field(
      "shape",
      shape_table().find(def).value_or(0),
      shape_table(),
      description + " Any shape of the library (`nymph shapes` lists them, docs/shapes.md shows them); default " + def +
          ".");
}

std::string shape_of(const node& n, size_t field) {
  return shape_table().format_enum(n.get<int64_t>(field));
}

void draw_shape_item(
    const node& n, draw_context& dc, const std::string& id, const char* text, const std::string& shape,
    const char* stereotype) {
  if (n.children.empty()) {
    ImUml::ShapeNode(id.c_str(), text, shape.c_str(), stereotype);
    return;
  }
  ImUml::BeginShapeGroup(id.c_str(), text, shape.c_str(), stereotype);
  dc.draw_children(n);
  ImUml::EndShapeGroup();
}

bool contains_node(const node& outer, const node& inner) {
  if (&outer == &inner) {
    return true;
  }
  for (const node& c : outer.children) {
    if (contains_node(c, inner)) {
      return true;
    }
  }
  return false;
}

ImU32 color_u32(const std::string& hex) {
  if (hex.empty()) {
    return 0;
  }
  const ImU32 c = ImGui::ColorConvertFloat4ToU32(parse_hex_color(hex));
  return c == 0 ? 1 : c; // fully transparent black would read as "the diagram's"
}

const char* or_null(const node& n, size_t field) {
  const std::string& s = n.get<std::string>(field);
  return s.empty() ? nullptr : s.c_str();
}

void set_next_colors(const node& n) {
  const int color = n.cls->field_index("color");
  const int fill = n.cls->field_index("fill_color");
  ImUml::SetNextItemColors(
      color >= 0 ? color_u32(n.get<std::string>(static_cast<size_t>(color))) : 0,
      fill >= 0 ? color_u32(n.get<std::string>(static_cast<size_t>(fill))) : 0);
}

void set_next_rank(const node& n) {
  const int rank = n.cls->field_index("rank");
  if (rank >= 0) {
    const std::string& group = n.get<std::string>(static_cast<size_t>(rank));
    if (!group.empty()) {
      ImUml::SetNextNodeRank(group.c_str());
    }
  }
}

void set_next_edge_layout(const node& n) {
  ImUml::SetNextEdgeLayout(
      static_cast<float>(n.get<double>(static_cast<size_t>(n.cls->field_index("weight")))),
      static_cast<int>(n.get<int64_t>(static_cast<size_t>(n.cls->field_index("min_length")))),
      n.get<bool>(static_cast<size_t>(n.cls->field_index("constraint"))));
}

std::vector<field_desc> node_color_fields() {
  return {
      color_field("color", "Outline colour; empty for the diagram's."),
      color_field("fill_color", "Fill colour; empty for the diagram's."),
  };
}

field_desc rank_field() {
  return text_field(
      "rank",
      "",
      "Nodes with the same rank text are placed in one rank (Graphviz dot's rank=same); empty for none.");
}

std::vector<field_desc> edge_layout_fields() {
  return {
      number_field(
          "weight",
          1.0,
          "How strongly the layout keeps this edge short and straight (Graphviz dot's weight).",
          value_range{0, 100}),
      int_field(
          "min_length",
          1,
          "Least number of ranks between the two ends (dot's minlen); 0 allows one rank.",
          value_range{0, 10}),
      bool_field("constraint", true, "false: the edge is drawn but does not influence the ranks (dot's constraint)."),
      color_field("color", "Line colour; empty for the diagram's."),
  };
}

std::vector<field_desc> concat(std::vector<field_desc> a, const std::vector<field_desc>& b) {
  for (const field_desc& f : b) {
    a.push_back(f);
  }
  return a;
}

// ── Note ─────────────────────────────────────────────────────────────────────

namespace {

class note_class : public item_class {
 public:
  note_class()
      : item_class(
            "Note",
            kUmlNoteSlot,
            {},
            concat(
                {
                    text_field("text", "", "The note's text; \\n starts a new line."),
                    texts_field(
                        "attach",
                        "Ids the note is attached to. Graph diagrams: drawn next to the first, joined to each by a "
                        "dashed line; none is a free note. Sequence diagrams: one participant, with side."),
                    texts_field(
                        "over",
                        "Sequence diagrams only: the note spans these one or two participants' lifelines."),
                    enum_field(
                        "side",
                        ImUmlSide_Right,
                        note_side_names(),
                        "Sequence diagrams only, with attach: which side of the lifeline."),
                },
                node_color_fields()),
            "A note. In a graph diagram it sits beside the nodes it is attached to; in a sequence diagram it "
            "takes a row, over participants or beside one.",
            {"attach", "over"}),
        text_(field("text")),
        attach_(field("attach")),
        over_(field("over")),
        side_(field("side")) {}

  void draw(const node& n, draw_context& dc) const override {
    (void)dc;
    const std::string& text = n.get<std::string>(text_);
    const auto& attach = n.get<std::vector<std::string>>(attach_);
    const auto& over = n.get<std::vector<std::string>>(over_);
    set_next_colors(n);
    if (current_kind() == diagram_kind::sequence) {
      if (!over.empty()) {
        ImUml::NoteOver(text.c_str(), over[0].c_str(), over.size() > 1 ? over[1].c_str() : nullptr);
      } else if (!attach.empty()) {
        ImUml::NoteSide(text.c_str(), attach[0].c_str(), static_cast<ImUmlSide>(n.get<int64_t>(side_)));
      }
      return;
    }
    std::vector<const char*> ids;
    for (const std::string& id : attach) {
      ids.push_back(id.c_str());
    }
    ImUml::Note(text.c_str(), ids.data(), static_cast<int>(ids.size()));
  }

  /// Rules that depend on the diagram kind; called by the diagrams.
  void check_in(const node& n, bool sequence) const {
    const auto& attach = n.get<std::vector<std::string>>(attach_);
    const auto& over = n.get<std::vector<std::string>>(over_);
    if (n.get<std::string>(text_).empty()) {
      fail_at(n, text_, "is empty; a note needs text");
    }
    if (!sequence) {
      if (n.field_pos[over_].line > 0) {
        fail_at(n, over_, "is for sequence diagrams; use attach to attach a note to nodes");
      }
      if (n.field_pos[side_].line > 0) {
        fail_at(n, side_, "is for sequence diagrams only");
      }
      return;
    }
    if (over.empty() == attach.empty()) {
      fail_at(
          n,
          over.empty() ? attach_ : over_,
          "a sequence diagram note needs either over (one or two participants) or attach (one participant, with "
          "side)");
    }
    if (over.size() > 2) {
      fail_at(n, over_, "takes one or two participants, not " + std::to_string(over.size()));
    }
    if (attach.size() > 1) {
      fail_at(n, attach_, "takes one participant in a sequence diagram; use over to span two");
    }
  }

 private:
  size_t text_, attach_, over_, side_;
};

} // namespace

/// Called by the diagrams' check_rules() for every Note in their subtree.
void check_note(const node& n, bool sequence) {
  static_cast<const note_class*>(n.cls)->check_in(n, sequence);
}

void register_note(registry& reg) {
  reg.add(std::make_unique<note_class>());
}

} // namespace bdg::nymph::uml

namespace bdg::nymph {

void register_uml(registry& reg) {
  uml::register_note(reg);
  uml::register_class_diagram(reg);
  uml::register_sequence_diagram(reg);
  uml::register_state_diagram(reg);
  uml::register_activity_diagram(reg);
  uml::register_usecase_diagram(reg);
  uml::register_flowchart(reg);
  uml::register_er_diagram(reg);
  uml::register_dataflow_diagram(reg);
  uml::register_network_diagram(reg);
  uml::register_component_diagram(reg);
  uml::register_deployment_diagram(reg);
  uml::register_package_diagram(reg);
  uml::register_object_diagram(reg);
  uml::register_tree_diagrams(reg);
}

} // namespace bdg::nymph
