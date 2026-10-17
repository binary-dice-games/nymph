// MIT License © 2026 Binary Dice Games
/// @file state_diagram.cpp
/// @brief `StateDiagram`, `State` (composite when it has children) and
///        `Transition` (spec/03_uml_diagrams/DESIGN.md section 3).
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "core/error.hpp"
#include "elements/uml/uml_common.hpp"

namespace bdg::nymph::uml {

namespace {

const name_table& state_kind_names() {
  static const name_table t{
      {"Simple", ImUmlStateKind_Simple},
      {"Initial", ImUmlStateKind_Initial},
      {"Final", ImUmlStateKind_Final},
      {"Choice", ImUmlStateKind_Choice},
      {"Fork", ImUmlStateKind_Fork},
      {"Join", ImUmlStateKind_Join},
      {"History", ImUmlStateKind_History},
      {"DeepHistory", ImUmlStateKind_DeepHistory},
  };
  return t;
}

class state_class : public item_class {
 public:
  state_class()
      : item_class(
            "State",
            kUmlStateItemSlot,
            {kUmlStateItemSlot, kUmlNoteSlot},
            concat(
                {
                    text_field("id", "", "Unique id that transitions and notes name."),
                    text_field("name", "", "Name shown; empty shows the id."),
                    enum_field(
                        "kind",
                        ImUmlStateKind_Simple,
                        state_kind_names(),
                        "Simple is a rounded box; the others are pseudo-states (Initial dot, Final bullseye, "
                        "Choice diamond, Fork / Join bar, History H / H*)."),
                    texts_field(
                        "activities", "Lines of the activity compartment, such as \"entry / start timer\"."),
                    rank_field(),
                },
                node_color_fields()),
            "A state. A Simple state with children (states, transitions, notes) is a composite state: its "
            "children are laid out inside it.") {
    id_ = field("id");
    name_ = field("name");
    kind_ = field("kind");
    activities_ = field("activities");
  }

  void draw(const node& n, draw_context& dc) const override {
    set_next_colors(n);
    set_next_rank(n);
    const std::string& id = n.get<std::string>(id_);
    // A pseudo-state's name is not shown; its id stays the default.
    const bool pseudo = n.get<int64_t>(kind_) != ImUmlStateKind_Simple;
    ImUml::BeginState(id.c_str(), pseudo ? nullptr : or_null(n, name_), static_cast<ImUmlStateKind>(n.get<int64_t>(kind_)));
    for (const std::string& a : n.get<std::vector<std::string>>(activities_)) {
      ImUml::StateActivity(a.c_str());
    }
    dc.draw_children(n);
    ImUml::EndState();
  }

  size_t id_, name_, kind_, activities_;
};

class transition_class : public item_class {
 public:
  transition_class()
      : item_class(
            "Transition",
            kUmlStateItemSlot,
            {},
            concat(
                {
                    text_field("from", "", "Id of the source state."),
                    text_field("to", "", "Id of the target state."),
                    text_field("label", "", "Text beside the arrow: mermaid's \"event [guard] / action\"."),
                },
                edge_layout_fields()),
            "A transition between two states.",
            {"from", "to"}) {
    set_edge(true);
    from_ = field("from");
    to_ = field("to");
    label_ = field("label");
  }

  void draw(const node& n, draw_context&) const override {
    set_next_colors(n);
    set_next_edge_layout(n);
    ImUml::Transition(n.get<std::string>(from_).c_str(), n.get<std::string>(to_).c_str(), or_null(n, label_));
  }

  size_t from_, to_, label_;
};

/// Whether `inner` is inside the composite `outer` (at any depth).
bool contains(const node& outer, const node& inner) {
  for (const node& c : outer.children) {
    if (&c == &inner || contains(c, inner)) {
      return true;
    }
  }
  return false;
}

class state_diagram_class : public diagram_class {
 public:
  state_diagram_class()
      : diagram_class(
            "StateDiagram",
            diagram_kind::state,
            kUmlStateItemSlot,
            {},
            "A UML state diagram, laid out automatically (Graphviz dot's layered layout). Children: State, "
            "Transition, Note.") {}

 protected:
  void check_rules(const node& n, const id_table& ids) const override {
    std::map<const node*, const node*> first_out_of_initial;
    walk(n, [&](const node& child, size_t) {
      if (is_a(child, "Note")) {
        check_note(child, false);
        return;
      }
      if (is_a(child, "State")) {
        const auto& sc = static_cast<const state_class&>(*child.cls);
        const int64_t kind = child.get<int64_t>(sc.kind_);
        if (kind != ImUmlStateKind_Simple && !child.children.empty()) {
          throw error(
              child.children[0].line,
              child.children[0].column,
              "State '" + child.get<std::string>(sc.id_) + "' is a pseudo-state (" +
                  state_kind_names().format_enum(kind) + ") and cannot contain children; only a Simple state "
                  "can be composite");
        }
        if (kind != ImUmlStateKind_Simple && !child.get<std::vector<std::string>>(sc.activities_).empty()) {
          child.cls->fail_at(child, sc.activities_, "is for Simple states only");
        }
        return;
      }
      if (!is_a(child, "Transition")) {
        return;
      }
      const auto& tc = static_cast<const transition_class&>(*child.cls);
      const node& from = ids.resolve(child, tc.from_, child.get<std::string>(tc.from_));
      const node& to = ids.resolve(child, tc.to_, child.get<std::string>(tc.to_));
      for (const auto& [f, target] : {std::pair<size_t, const node*>{tc.from_, &from}, {tc.to_, &to}}) {
        if (!is_a(*target, "State")) {
          child.cls->fail_at(child, f, "must name a State, not a " + std::string(target->cls->name()));
        }
      }
      const auto& sc = static_cast<const state_class&>(*from.cls);
      if (to.get<int64_t>(sc.kind_) == ImUmlStateKind_Initial) {
        child.cls->fail_at(child, tc.to_, "names an Initial state; nothing may transition into it");
      }
      if (from.get<int64_t>(sc.kind_) == ImUmlStateKind_Final) {
        child.cls->fail_at(child, tc.from_, "names a Final state; nothing may leave it");
      }
      if (from.get<int64_t>(sc.kind_) == ImUmlStateKind_Initial) {
        const auto [it, first] = first_out_of_initial.emplace(&from, &child);
        if (!first) {
          child.cls->fail_at(
              child,
              tc.from_,
              "is a second transition out of the Initial state '" + from.get<std::string>(sc.id_) +
                  "' (the first is at line " + std::to_string(it->second->line) + "); an initial state has one");
        }
      }
      if (contains(from, to) || contains(to, from)) {
        child.cls->fail_at(
            child, tc.to_, "connects a composite state with a state inside it; name the inner states instead");
      }
    });
  }
};

} // namespace

void register_state_diagram(registry& reg) {
  reg.add(std::make_unique<state_diagram_class>());
  reg.add(std::make_unique<state_class>());
  reg.add(std::make_unique<transition_class>());
}

} // namespace bdg::nymph::uml
