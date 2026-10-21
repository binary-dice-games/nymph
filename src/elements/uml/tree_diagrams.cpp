// MIT License © 2026 Binary Dice Games
/// @file tree_diagrams.cpp
/// @brief `TreeDiagram`, `MindMap` and `OrgChart` with their nodes and
///        cross links: hierarchies written by nesting, laid out by the tree
///        layout (spec/13_more_diagrams_and_shapes/DESIGN.md section 5).
#include <memory>
#include <string>
#include "core/error.hpp"
#include "elements/uml/uml_common.hpp"
#include "placeholders/placeholders.hpp"

namespace bdg::nymph::uml {

namespace {

/// The number of nodes below `n` (of the node classes, not links).
int count_below(const node& n) {
  int count = 0;
  for (const node& c : n.children) {
    if (dynamic_cast<const item_class*>(c.cls) != nullptr && !dynamic_cast<const item_class*>(c.cls)->is_edge()) {
      count += 1 + count_below(c);
    }
  }
  return count;
}

// ── Cross links ──────────────────────────────────────────────────────────────

class tree_link_class : public item_class {
 public:
  tree_link_class(const char* name, const char* slot, const char* what)
      : item_class(
            name,
            slot,
            {},
            {
                text_field("from", "", "Id of the node the link leaves."),
                text_field("to", "", "Id of the node it reaches."),
                text_field("label", "", "Text on the link."),
                color_field("color", "Line colour; empty for the theme's."),
            },
            std::string("A dashed curve between two nodes of a ") + what +
                " (both need an id); it does not move anything.",
            {"from", "to"}) {
    set_edge(true);
    from_ = field("from");
    to_ = field("to");
    label_ = field("label");
  }

  void draw(const node& n, draw_context&) const override {
    set_next_colors(n);
    ImUml::TreeLink(n.get<std::string>(from_).c_str(), n.get<std::string>(to_).c_str(), or_null(n, label_));
  }

  size_t from_, to_, label_;
};

// ── The diagrams ─────────────────────────────────────────────────────────────

class tree_diagram_base : public diagram_class {
 public:
  using diagram_class::diagram_class;

 protected:
  /// Names of the node and link classes of the diagram.
  virtual const char* node_class() const = 0;

  void check_rules(const node& n, const id_table& ids) const override {
    (void)ids;
    size_t roots = 0;
    for (const node& c : n.children) {
      if (is_a(c, node_class())) {
        if (++roots > 1) {
          throw error(
              c.line, c.column, std::string(name()) + " has one root " + node_class() + "; this is a second one");
        }
      }
    }
    if (roots == 0) {
      throw error(n.line, n.column, std::string(name()) + " needs a root " + node_class() + " in its children");
    }
  }
};

// TreeDiagram (nodes are `TreeItem`: `TreeNode` is the ImGui widget of mockups) ----------------------------------------------------------------

class tree_node_class : public item_class {
 public:
  tree_node_class()
      : item_class(
            "TreeItem",
            kUmlTreeItemSlot,
            {kUmlTreeItemSlot},
            concat(
                {
                    text_field("id", "", "Optional id; unique when given."),
                    text_field("text", "", "The label (a line break starts a new line); empty shows the id."),
                    shape_field("rounded_rectangle", "The node's drawing."),
                    bool_field("collapsed", false, "The children are not drawn; a +N badge counts what is hidden."),
                },
                node_color_fields()),
            "A node of a tree. Its children, written inside it, are the nodes below it.") {
    set_optional_id(true);
    id_ = field("id");
    text_ = field("text");
    shape_ = field("shape");
    collapsed_ = field("collapsed");
  }

  void draw(const node& n, draw_context& dc) const override {
    set_next_colors(n);
    const std::string& id = n.get<std::string>(id_);
    const bool collapsed = n.get<bool>(collapsed_);
    const std::string shape = shape_of(n, shape_);
    ImUmlTreeNodeDesc desc;
    desc.Id = id.empty() ? nullptr : id.c_str();
    desc.Text = or_null(n, text_);
    desc.Shape = shape.c_str();
    desc.Hidden = collapsed ? count_below(n) : 0;
    ImUml::BeginTreeNode(desc);
    if (!collapsed) {
      dc.draw_children(n);
    }
    ImUml::EndTreeNode();
  }

  size_t id_, text_, shape_, collapsed_;
};

class tree_diagram_class : public tree_diagram_base {
 public:
  tree_diagram_class()
      : tree_diagram_base(
            "TreeDiagram",
            diagram_kind::tree,
            kUmlTreeItemSlot,
            {},
            "A tree (a hierarchy, a file layout, a breakdown): one root TreeItem whose children are nested inside "
            "it, laid out automatically with each parent centred over its children. Child: TreeItem.") {}

 protected:
  const char* node_class() const override {
    return "TreeItem";
  }

  void setup(const node&) const override {
    ImUml::SetupTree(ImUmlTreeMode_Tidy);
  }
};

// MindMap ----------------------------------------------------------------------

constexpr int64_t kSideAuto = -1;

const name_table& side_names() {
  static const name_table t{{"Auto", kSideAuto}, {"Left", ImUmlSide_Left}, {"Right", ImUmlSide_Right}};
  return t;
}

class mind_node_class : public item_class {
 public:
  mind_node_class()
      : item_class(
            "MindNode",
            kUmlMindItemSlot,
            {kUmlMindItemSlot},
            concat(
                {
                    text_field("id", "", "Optional id; needed to be named by a MindLink."),
                    text_field("text", "", "The label; empty shows the id."),
                    enum_field(
                        "side",
                        kSideAuto,
                        side_names(),
                        "Children of the root only: Left or Right pins the branch to that side; Auto (default) "
                        "balances the two sides."),
                    bool_field("collapsed", false, "The children are not drawn; a +N badge counts what is hidden."),
                },
                node_color_fields()),
            "A node of a mind map. Its children, written inside it, are its branches; `color` sets the branch's "
            "colour (inherited down it).") {
    set_optional_id(true);
    id_ = field("id");
    text_ = field("text");
    side_ = field("side");
    collapsed_ = field("collapsed");
  }

  void draw(const node& n, draw_context& dc) const override {
    set_next_colors(n);
    const std::string& id = n.get<std::string>(id_);
    const bool collapsed = n.get<bool>(collapsed_);
    ImUmlTreeNodeDesc desc;
    desc.Id = id.empty() ? nullptr : id.c_str();
    desc.Text = or_null(n, text_);
    desc.Side = static_cast<int>(n.get<int64_t>(side_));
    desc.Hidden = collapsed ? count_below(n) : 0;
    ImUml::BeginTreeNode(desc);
    if (!collapsed) {
      dc.draw_children(n);
    }
    ImUml::EndTreeNode();
  }

  size_t id_, text_, side_, collapsed_;
};

class mind_map_class : public tree_diagram_base {
 public:
  mind_map_class()
      : tree_diagram_base(
            "MindMap",
            diagram_kind::mind_map,
            kUmlMindItemSlot,
            {},
            "A mind map: a root idea in the middle with branches to both sides, each branch in its own colour, "
            "laid out automatically. Children: one root MindNode (branches nested inside it), MindLink.",
            {kUmlMindLinkSlot}) {
    style_ = field("edge_style");
  }

 protected:
  const char* node_class() const override {
    return "MindNode";
  }

  void check_rules(const node& n, const id_table& ids) const override {
    tree_diagram_base::check_rules(n, ids);
    for (const node& root : n.children) {
      if (!is_a(root, "MindNode")) {
        continue;
      }
      walk(root, [&](const node& c, size_t depth) {
        if (!is_a(c, "MindNode")) {
          return;
        }
        const auto& mc = static_cast<const mind_node_class&>(*c.cls);
        if (depth > 1 && c.get<int64_t>(mc.side_) != kSideAuto) {
          c.cls->fail_at(c, mc.side_, "is for the children of the root only; a branch stays on its side");
        }
      });
    }
  }

  void setup(const node& n) const override {
    ImUml::SetupTree(ImUmlTreeMode_MindMap);
    ImUml::SetupLayout(ImUmlDirection_LeftRight, static_cast<ImUmlEdgeStyle>(n.get<int64_t>(style_)));
  }

  size_t style_;
};

// OrgChart ---------------------------------------------------------------------

constexpr int64_t kHorizontal = 0, kHanging = 1;

const name_table& children_layout_names() {
  static const name_table t{{"Horizontal", kHorizontal}, {"Hanging", kHanging}};
  return t;
}

/// "none" and every placeholder image, as an enumeration.
const name_table& portrait_names() {
  static const name_table t = [] {
    std::vector<name_entry> entries{{"none", 0}};
    int64_t i = 1;
    for (const std::string& name : placeholder_names()) {
      entries.push_back({name, i++});
    }
    return name_table(std::move(entries));
  }();
  return t;
}

class person_class : public item_class {
 public:
  person_class()
      : item_class(
            "Person",
            kUmlOrgItemSlot,
            {kUmlOrgItemSlot},
            concat(
                {
                    text_field("id", "", "Optional id; needed to be named by an OrgLink."),
                    text_field("name", "", "The person's name; empty shows the id."),
                    text_field("title", "", "Job title, under the name."),
                    text_field("department", "", "A small third line (department, team, location)."),
                    enum_field(
                        "image",
                        0,
                        portrait_names(),
                        "A portrait at the card's left: none (default) or a placeholder image (person, people, "
                        "logo, ...). A vacancy has none."),
                    shape_field("rounded_rectangle", "The card's drawing."),
                    enum_field(
                        "children_layout",
                        kHorizontal,
                        children_layout_names(),
                        "Horizontal (default): the reports in a row under the card. Hanging: they hang below it in an "
                        "indented list, for teams with many leaves."),
                    bool_field("staff", false, "An assistant: the card sits beside its manager's, not among the reports."),
                    bool_field("vacant", false, "An open position: a dashed card with no portrait."),
                    bool_field("collapsed", false, "The reports are not drawn; a +N badge counts what is hidden."),
                },
                node_color_fields()),
            "A person (a card with a name, a title and optionally a portrait) in an org chart. The reports are "
            "written inside it.") {
    set_optional_id(true);
    id_ = field("id");
    name_ = field("name");
    title_ = field("title");
    department_ = field("department");
    image_ = field("image");
    shape_ = field("shape");
    layout_ = field("children_layout");
    staff_ = field("staff");
    vacant_ = field("vacant");
    collapsed_ = field("collapsed");
  }

  void draw(const node& n, draw_context& dc) const override {
    set_next_colors(n);
    const std::string& id = n.get<std::string>(id_);
    const bool collapsed = n.get<bool>(collapsed_);
    const std::string shape = shape_of(n, shape_);
    const std::string image = portrait_names().format_enum(n.get<int64_t>(image_));
    const std::string portrait = image == "none" ? std::string() : "image:" + image;
    ImUmlTreeNodeDesc desc;
    desc.Id = id.empty() ? nullptr : id.c_str();
    desc.Text = or_null(n, name_);
    desc.Subtext = or_null(n, title_);
    desc.Detail = or_null(n, department_);
    desc.Shape = shape.c_str();
    desc.Image = portrait.empty() ? nullptr : portrait.c_str();
    desc.Hanging = n.get<int64_t>(layout_) == kHanging;
    desc.Staff = n.get<bool>(staff_);
    desc.Vacant = n.get<bool>(vacant_);
    desc.Hidden = collapsed ? count_below(n) : 0;
    ImUml::BeginTreeNode(desc);
    if (!collapsed) {
      dc.draw_children(n);
    }
    ImUml::EndTreeNode();
  }

  size_t id_, name_, title_, department_, image_, shape_, layout_, staff_, vacant_, collapsed_;
};

class org_chart_class : public tree_diagram_base {
 public:
  org_chart_class()
      : tree_diagram_base(
            "OrgChart",
            diagram_kind::org_chart,
            kUmlOrgItemSlot,
            {},
            "An organisation chart: one root Person whose reports are nested inside it, drawn as cards (portrait, "
            "name, title), with hanging teams, assistants beside their manager and open positions, laid out "
            "automatically. Children: one root Person, OrgLink.",
            {kUmlOrgLinkSlot}) {}

 protected:
  const char* node_class() const override {
    return "Person";
  }

  void check_rules(const node& n, const id_table& ids) const override {
    tree_diagram_base::check_rules(n, ids);
    for (const node& root : n.children) {
      if (is_a(root, "Person") && root.get<bool>(static_cast<const person_class&>(*root.cls).staff_)) {
        root.cls->fail_at(root, static_cast<const person_class&>(*root.cls).staff_, "cannot be set on the root");
      }
    }
  }

  void setup(const node&) const override {
    ImUml::SetupTree(ImUmlTreeMode_OrgChart);
  }
};

} // namespace

void register_tree_diagrams(registry& reg) {
  reg.add(std::make_unique<tree_diagram_class>());
  reg.add(std::make_unique<tree_node_class>());
  reg.add(std::make_unique<mind_map_class>());
  reg.add(std::make_unique<mind_node_class>());
  reg.add(std::make_unique<tree_link_class>("MindLink", kUmlMindLinkSlot, "mind map"));
  reg.add(std::make_unique<org_chart_class>());
  reg.add(std::make_unique<person_class>());
  reg.add(std::make_unique<tree_link_class>("OrgLink", kUmlOrgLinkSlot, "org chart"));
}

} // namespace bdg::nymph::uml
