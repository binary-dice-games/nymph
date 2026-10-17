// MIT License © 2026 Binary Dice Games
/// @file usecase_diagram.cpp
/// @brief `UseCaseDiagram`, `Actor`, `UseCase` and `UseCaseRelation`
///        (spec/03_uml_diagrams/DESIGN.md section 3).
#include <memory>
#include <string>
#include "elements/uml/uml_common.hpp"

namespace bdg::nymph::uml {

namespace {

const name_table& side_names() {
  static const name_table t{{"Left", ImUmlSide_Left}, {"Right", ImUmlSide_Right}};
  return t;
}

const name_table& link_kind_names() {
  static const name_table t{
      {"Association", ImUmlUseCaseLinkKind_Association},
      {"Include", ImUmlUseCaseLinkKind_Include},
      {"Extend", ImUmlUseCaseLinkKind_Extend},
      {"Generalization", ImUmlUseCaseLinkKind_Generalization},
  };
  return t;
}

class actor_class : public item_class {
 public:
  actor_class()
      : item_class(
            "Actor",
            kUmlUseCaseItemSlot,
            {},
            concat(
                {
                    text_field("id", "", "Unique id that relations and notes name."),
                    text_field("name", "", "Name shown under the figure; empty shows the id."),
                    enum_field(
                        "side",
                        ImUmlSide_Left,
                        side_names(),
                        "Left actors are placed before the system, Right actors after it."),
                },
                node_color_fields()),
            "An actor: a stick figure outside the system boundary.") {
    id_ = field("id");
    name_ = field("name");
    side_ = field("side");
  }

  void draw(const node& n, draw_context&) const override {
    set_next_colors(n);
    ImUml::Actor(n.get<std::string>(id_).c_str(), or_null(n, name_), static_cast<ImUmlSide>(n.get<int64_t>(side_)));
  }

  size_t id_, name_, side_;
};

class usecase_class : public item_class {
 public:
  usecase_class()
      : item_class(
            "UseCase",
            kUmlUseCaseItemSlot,
            {},
            concat(
                {
                    text_field("id", "", "Unique id that relations and notes name."),
                    text_field("name", "", "Name shown in the ellipse; empty shows the id."),
                    rank_field(),
                },
                node_color_fields()),
            "A use case: an ellipse inside the system boundary.") {
    id_ = field("id");
    name_ = field("name");
  }

  void draw(const node& n, draw_context&) const override {
    set_next_colors(n);
    set_next_rank(n);
    ImUml::UseCase(n.get<std::string>(id_).c_str(), or_null(n, name_));
  }

  size_t id_, name_;
};

class link_class : public item_class {
 public:
  link_class()
      : item_class(
            "UseCaseRelation",
            kUmlUseCaseItemSlot,
            {},
            concat(
                {
                    text_field("from", "", "Id of the actor or use case it starts at."),
                    text_field("to", "", "Id of the actor or use case it points to."),
                    enum_field(
                        "kind",
                        ImUmlUseCaseLinkKind_Association,
                        link_kind_names(),
                        "Association: a line between an actor and a use case. Include / Extend: a dashed «include» / "
                        "«extend» arrow between use cases. Generalization: a hollow triangle at `to`, between two "
                        "actors or two use cases."),
                },
                edge_layout_fields()),
            "A relation between actors and use cases.",
            {"from", "to"}) {
    set_edge(true);
    from_ = field("from");
    to_ = field("to");
    kind_ = field("kind");
  }

  void draw(const node& n, draw_context&) const override {
    set_next_colors(n);
    set_next_edge_layout(n);
    ImUml::UseCaseLink(
        n.get<std::string>(from_).c_str(),
        n.get<std::string>(to_).c_str(),
        static_cast<ImUmlUseCaseLinkKind>(n.get<int64_t>(kind_)));
  }

  size_t from_, to_, kind_;
};

class usecase_diagram_class : public diagram_class {
 public:
  usecase_diagram_class()
      : diagram_class(
            "UseCaseDiagram",
            diagram_kind::use_case,
            kUmlUseCaseItemSlot,
            {text_field("system", "", "Title of the system boundary drawn around the use cases; empty for none.")},
            "A UML use case diagram: actors outside a system boundary, use cases inside, laid out automatically "
            "(Graphviz dot's layered layout, left to right). Children: Actor, UseCase, UseCaseRelation, Note.") {
    system_ = field("system");
  }

 protected:
  void setup(const node& n) const override {
    const std::string& system = n.get<std::string>(system_);
    if (!system.empty()) {
      ImUml::SetupSystemBoundary(system.c_str());
    }
  }

  void check_rules(const node& n, const id_table& ids) const override {
    walk(n, [&](const node& child, size_t) {
      if (is_a(child, "Note")) {
        check_note(child, false);
        return;
      }
      if (!is_a(child, "UseCaseRelation")) {
        return;
      }
      const auto& lc = static_cast<const link_class&>(*child.cls);
      const node& from = ids.resolve(child, lc.from_, child.get<std::string>(lc.from_));
      const node& to = ids.resolve(child, lc.to_, child.get<std::string>(lc.to_));
      const int64_t kind = child.get<int64_t>(lc.kind_);
      const bool from_actor = is_a(from, "Actor"), to_actor = is_a(to, "Actor");
      switch (kind) {
        case ImUmlUseCaseLinkKind_Include:
        case ImUmlUseCaseLinkKind_Extend:
          if (from_actor || to_actor) {
            child.cls->fail_at(
                child,
                from_actor ? lc.from_ : lc.to_,
                "names an Actor; " + link_kind_names().format_enum(kind) + " relates two use cases");
          }
          break;
        case ImUmlUseCaseLinkKind_Generalization:
          if (from_actor != to_actor) {
            child.cls->fail_at(child, lc.to_, "Generalization relates two actors or two use cases, not one of each");
          }
          break;
        default:
          if (from_actor == to_actor) {
            child.cls->fail_at(
                child,
                lc.to_,
                std::string("Association joins an actor and a use case, not two ") +
                    (from_actor ? "actors (use Generalization)" : "use cases (use Include or Extend)"));
          }
          break;
      }
    });
  }

 private:
  size_t system_;
};

} // namespace

void register_usecase_diagram(registry& reg) {
  reg.add(std::make_unique<usecase_diagram_class>());
  reg.add(std::make_unique<actor_class>());
  reg.add(std::make_unique<usecase_class>());
  reg.add(std::make_unique<link_class>());
}

} // namespace bdg::nymph::uml
