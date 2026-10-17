// MIT License © 2026 Binary Dice Games
/// @file activity_diagram.cpp
/// @brief `ActivityDiagram`, `Action`, `ActivityControl` and `Flow`
///        (spec/03_uml_diagrams/DESIGN.md section 3).
#include <memory>
#include <string>
#include "elements/uml/uml_common.hpp"

namespace bdg::nymph::uml {

namespace {

const name_table& action_kind_names() {
  static const name_table t{
      {"Action", ImUmlActionKind_Action},
      {"Send", ImUmlActionKind_Send},
      {"Receive", ImUmlActionKind_Receive},
  };
  return t;
}

const name_table& control_kind_names() {
  static const name_table t{
      {"Initial", ImUmlControlKind_Initial},
      {"Final", ImUmlControlKind_Final},
      {"FlowFinal", ImUmlControlKind_FlowFinal},
      {"Decision", ImUmlControlKind_Decision},
      {"Merge", ImUmlControlKind_Merge},
      {"Fork", ImUmlControlKind_Fork},
      {"Join", ImUmlControlKind_Join},
  };
  return t;
}

class action_class : public item_class {
 public:
  action_class()
      : item_class(
            "Action",
            kUmlActivityItemSlot,
            {},
            concat(
                {
                    text_field("id", "", "Unique id that flows and notes name."),
                    text_field("text", "", "What the action does; empty shows the id."),
                    enum_field(
                        "kind",
                        ImUmlActionKind_Action,
                        action_kind_names(),
                        "Action is a rounded box; Send and Receive are the signal shapes."),
                    rank_field(),
                },
                node_color_fields()),
            "An action (a step of the activity).") {
    id_ = field("id");
    text_ = field("text");
    kind_ = field("kind");
  }

  void draw(const node& n, draw_context&) const override {
    set_next_colors(n);
    set_next_rank(n);
    const std::string& id = n.get<std::string>(id_);
    const std::string& text = n.get<std::string>(text_);
    ImUml::Action(id.c_str(), text.empty() ? id.c_str() : text.c_str(), static_cast<ImUmlActionKind>(n.get<int64_t>(kind_)));
  }

  size_t id_, text_, kind_;
};

class control_class : public item_class {
 public:
  control_class()
      : item_class(
            "ActivityControl",
            kUmlActivityItemSlot,
            {},
            concat(
                {
                    text_field("id", "", "Unique id that flows and notes name."),
                    enum_field(
                        "kind",
                        ImUmlControlKind_Initial,
                        control_kind_names(),
                        "Initial dot, Final bullseye, FlowFinal crossed circle, Decision / Merge diamond, Fork / "
                        "Join bar."),
                    text_field("label", "", "Text beside the node (a decision's question)."),
                    rank_field(),
                },
                node_color_fields()),
            "A control node of an activity: start, end, decision, merge, fork or join.") {
    id_ = field("id");
    kind_ = field("kind");
    label_ = field("label");
  }

  void draw(const node& n, draw_context&) const override {
    set_next_colors(n);
    set_next_rank(n);
    ImUml::ControlNode(
        n.get<std::string>(id_).c_str(), static_cast<ImUmlControlKind>(n.get<int64_t>(kind_)), or_null(n, label_));
  }

  size_t id_, kind_, label_;
};

class flow_class : public item_class {
 public:
  flow_class()
      : item_class(
            "Flow",
            kUmlActivityItemSlot,
            {},
            concat(
                {
                    text_field("from", "", "Id of the node the flow leaves."),
                    text_field("to", "", "Id of the node it enters."),
                    text_field("guard", "", "Condition, drawn as [guard] beside the arrow."),
                },
                edge_layout_fields()),
            "A control flow between two activity nodes.",
            {"from", "to"}) {
    set_edge(true);
    from_ = field("from");
    to_ = field("to");
    guard_ = field("guard");
  }

  void draw(const node& n, draw_context&) const override {
    set_next_colors(n);
    set_next_edge_layout(n);
    ImUml::Flow(n.get<std::string>(from_).c_str(), n.get<std::string>(to_).c_str(), or_null(n, guard_));
  }

  size_t from_, to_, guard_;
};

class activity_diagram_class : public diagram_class {
 public:
  activity_diagram_class()
      : diagram_class(
            "ActivityDiagram",
            diagram_kind::activity,
            kUmlActivityItemSlot,
            {},
            "A UML activity diagram, laid out automatically (Graphviz dot's layered layout). Children: Action, "
            "ActivityControl, Flow, Note.") {}

 protected:
  void check_rules(const node& n, const id_table& ids) const override {
    walk(n, [&](const node& child, size_t) {
      if (is_a(child, "Note")) {
        check_note(child, false);
        return;
      }
      if (!is_a(child, "Flow")) {
        return;
      }
      const auto& fc = static_cast<const flow_class&>(*child.cls);
      const node& from = ids.resolve(child, fc.from_, child.get<std::string>(fc.from_));
      const node& to = ids.resolve(child, fc.to_, child.get<std::string>(fc.to_));
      if (is_a(to, "ActivityControl")) {
        const auto& cc = static_cast<const control_class&>(*to.cls);
        if (to.get<int64_t>(cc.kind_) == ImUmlControlKind_Initial) {
          child.cls->fail_at(child, fc.to_, "names an Initial node; nothing may flow into it");
        }
      }
      if (is_a(from, "ActivityControl")) {
        const auto& cc = static_cast<const control_class&>(*from.cls);
        const int64_t k = from.get<int64_t>(cc.kind_);
        if (k == ImUmlControlKind_Final || k == ImUmlControlKind_FlowFinal) {
          child.cls->fail_at(
              child, fc.from_, "names a " + control_kind_names().format_enum(k) + " node; nothing may flow out of it");
        }
      }
    });
  }
};

} // namespace

void register_activity_diagram(registry& reg) {
  reg.add(std::make_unique<activity_diagram_class>());
  reg.add(std::make_unique<action_class>());
  reg.add(std::make_unique<control_class>());
  reg.add(std::make_unique<flow_class>());
}

} // namespace bdg::nymph::uml
