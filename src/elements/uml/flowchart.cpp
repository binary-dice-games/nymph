// MIT License © 2026 Binary Dice Games
/// @file flowchart.cpp
/// @brief `Flowchart`, `FlowNode`, `FlowGroup` (mermaid's subgraph) and
///        `FlowLink`: general node-and-link diagrams on the UML diagrams'
///        layered layout (spec/03_uml_diagrams/DESIGN.md section 3,
///        "Flowchart").
#include <memory>
#include <string>
#include "elements/uml/uml_common.hpp"

namespace bdg::nymph::uml {

namespace {

const name_table& flow_shape_names() {
  static const name_table t{
      {"Box", ImUmlFlowShape_Box},
      {"Rounded", ImUmlFlowShape_Rounded},
      {"Stadium", ImUmlFlowShape_Stadium},
      {"Diamond", ImUmlFlowShape_Diamond},
      {"Circle", ImUmlFlowShape_Circle},
      {"Parallelogram", ImUmlFlowShape_Parallelogram},
      {"Hexagon", ImUmlFlowShape_Hexagon},
      {"Cylinder", ImUmlFlowShape_Cylinder},
      {"Subroutine", ImUmlFlowShape_Subroutine},
  };
  return t;
}

/// `FlowLink.line` values.
constexpr int64_t kLineSolid = 0, kLineDotted = 1, kLineThick = 2;

const name_table& link_line_names() {
  static const name_table t{{"Solid", kLineSolid}, {"Dotted", kLineDotted}, {"Thick", kLineThick}};
  return t;
}

/// `FlowLink.arrow` values.
constexpr int64_t kArrowEnd = 0, kArrowNone = 1, kArrowBoth = 2;

const name_table& link_arrow_names() {
  static const name_table t{{"End", kArrowEnd}, {"None", kArrowNone}, {"Both", kArrowBoth}};
  return t;
}

class flow_node_class : public item_class {
 public:
  flow_node_class()
      : item_class(
            "FlowNode",
            kUmlFlowItemSlot,
            {},
            concat(
                {
                    text_field("id", "", "Unique id that links and notes name."),
                    text_field("text", "", "Text in the node (a line break starts a new line); empty shows the id."),
                    enum_field(
                        "shape",
                        ImUmlFlowShape_Box,
                        flow_shape_names(),
                        "Box (a step, default), Rounded, Stadium (start / end), Diamond (decision), Circle, "
                        "Parallelogram (input / output), Hexagon (preparation), Cylinder (database) or "
                        "Subroutine (a predefined process)."),
                    symbol_field(),
                    rank_field(),
                },
                node_color_fields()),
            "A node of a flowchart.") {
    id_ = field("id");
    text_ = field("text");
    shape_ = field("shape");
    symbol_ = field("symbol");
  }

  void draw(const node& n, draw_context&) const override {
    set_next_colors(n);
    set_next_rank(n);
    if (n.get<int64_t>(symbol_) != 0) {
      const std::string symbol = symbol_table().format_enum(n.get<int64_t>(symbol_));
      ImUml::ShapeNode(n.get<std::string>(id_).c_str(), or_null(n, text_), symbol.c_str());
      return;
    }
    ImUml::FlowNode(
        n.get<std::string>(id_).c_str(), or_null(n, text_), static_cast<ImUmlFlowShape>(n.get<int64_t>(shape_)));
  }

  /// `symbol`: "none" (use `shape`) or any shape of the library.
  static const name_table& symbol_table() {
    static const name_table t = [] {
      std::vector<name_entry> entries{{"none", 0}};
      int64_t i = 1;
      for (const name_entry& e : shape_table().entries()) {
        entries.push_back({e.name, i++});
      }
      return name_table(std::move(entries));
    }();
    return t;
  }

  static field_desc symbol_field() {
    return enum_field(
        "symbol",
        0,
        symbol_table(),
        "none (default: the `shape` above is used) or any shape of the library (`nymph shapes` lists them, "
        "docs/shapes.md shows them); it replaces `shape` and sizes the node around the text.");
  }

  size_t id_, text_, shape_, symbol_;
};

class flow_group_class : public item_class {
 public:
  flow_group_class()
      : item_class(
            "FlowGroup",
            kUmlFlowItemSlot,
            {kUmlFlowItemSlot, kUmlNoteSlot},
            concat(
                {
                    text_field("id", "", "Unique id; links may name a group to end at its box."),
                    text_field("title", "", "Title at the top of the box; empty shows the id."),
                },
                node_color_fields()),
            "A group of nodes in a titled box (mermaid's subgraph), such as a service, a team or a phase. "
            "Children: FlowNode, FlowGroup, FlowLink, Note; links inside may also join nodes outside.") {
    id_ = field("id");
    title_ = field("title");
  }

  void draw(const node& n, draw_context& dc) const override {
    set_next_colors(n);
    ImUml::BeginFlowGroup(n.get<std::string>(id_).c_str(), or_null(n, title_));
    dc.draw_children(n);
    ImUml::EndFlowGroup();
  }

  size_t id_, title_;
};

class flow_link_class : public item_class {
 public:
  flow_link_class()
      : item_class(
            "FlowLink",
            kUmlFlowItemSlot,
            {},
            concat(
                {
                    text_field("from", "", "Id of the node or group the link leaves."),
                    text_field("to", "", "Id of the node or group it reaches."),
                    text_field("label", "", "Text in the middle of the link, such as a decision's answer."),
                    enum_field(
                        "line",
                        kLineSolid,
                        link_line_names(),
                        "Solid (default), Dotted (dashed: optional or asynchronous) or Thick (the main path)."),
                    enum_field(
                        "arrow",
                        kArrowEnd,
                        link_arrow_names(),
                        "Arrowheads: End (at to, default), None (a plain line) or Both."),
                },
                edge_layout_fields()),
            "A link between two nodes or groups of a flowchart.",
            {"from", "to"}) {
    set_edge(true);
    from_ = field("from");
    to_ = field("to");
    label_ = field("label");
    line_ = field("line");
    arrow_ = field("arrow");
  }

  void draw(const node& n, draw_context&) const override {
    set_next_colors(n);
    set_next_edge_layout(n);
    const int64_t line = n.get<int64_t>(line_);
    const int64_t arrow = n.get<int64_t>(arrow_);
    ImUmlFlowLinkFlags flags = ImUmlFlowLinkFlags_None;
    flags |= line == kLineDotted ? ImUmlFlowLinkFlags_Dotted : 0;
    flags |= line == kLineThick ? ImUmlFlowLinkFlags_Thick : 0;
    flags |= arrow == kArrowNone ? ImUmlFlowLinkFlags_NoArrow : 0;
    flags |= arrow == kArrowBoth ? ImUmlFlowLinkFlags_BothArrows : 0;
    ImUml::FlowLink(n.get<std::string>(from_).c_str(), n.get<std::string>(to_).c_str(), or_null(n, label_), flags);
  }

  size_t from_, to_, label_, line_, arrow_;
};

/// Whether `inner` is inside the group `outer` (at any depth).
bool contains(const node& outer, const node& inner) {
  for (const node& c : outer.children) {
    if (&c == &inner || contains(c, inner)) {
      return true;
    }
  }
  return false;
}

class flowchart_class : public diagram_class {
 public:
  flowchart_class()
      : diagram_class(
            "Flowchart",
            diagram_kind::flowchart,
            kUmlFlowItemSlot,
            {},
            "A flowchart or any node-and-link diagram (process, decision tree, dependency or architecture graph, "
            "mermaid's flowchart), laid out automatically (Graphviz dot's layered layout). Children: FlowNode, "
            "FlowGroup, FlowLink, Note.") {}

 protected:
  void check_rules(const node& n, const id_table& ids) const override {
    walk(n, [&](const node& child, size_t) {
      if (is_a(child, "Note")) {
        check_note(child, false);
        return;
      }
      if (!is_a(child, "FlowLink")) {
        return;
      }
      const auto& lc = static_cast<const flow_link_class&>(*child.cls);
      const node& from = ids.resolve(child, lc.from_, child.get<std::string>(lc.from_));
      const node& to = ids.resolve(child, lc.to_, child.get<std::string>(lc.to_));
      if (contains(from, to) || contains(to, from)) {
        child.cls->fail_at(
            child, lc.to_, "joins a FlowGroup with a node inside it; link the inner nodes, or nodes outside the group");
      }
    });
  }
};

} // namespace

void register_flowchart(registry& reg) {
  reg.add(std::make_unique<flowchart_class>());
  reg.add(std::make_unique<flow_node_class>());
  reg.add(std::make_unique<flow_group_class>());
  reg.add(std::make_unique<flow_link_class>());
}

} // namespace bdg::nymph::uml
