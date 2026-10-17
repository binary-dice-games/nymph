// MIT License © 2026 Binary Dice Games
/// @file class_diagram.cpp
/// @brief `ClassDiagram`, `Class` and `ClassRelation`
///        (spec/03_uml_diagrams/DESIGN.md section 3, "Class diagram").
#include <memory>
#include <string>
#include <vector>
#include "elements/uml/uml_common.hpp"

namespace bdg::nymph::uml {

namespace {

const name_table& class_kind_names() {
  static const name_table t{
      {"Class", ImUmlClassKind_Class},
      {"Abstract", ImUmlClassKind_Abstract},
      {"Interface", ImUmlClassKind_Interface},
      {"Enum", ImUmlClassKind_Enum},
  };
  return t;
}

const name_table& relation_kind_names() {
  static const name_table t{
      {"Association", ImUmlRelationKind_Association},
      {"DirectedAssociation", ImUmlRelationKind_DirectedAssociation},
      {"Inheritance", ImUmlRelationKind_Inheritance},
      {"Realization", ImUmlRelationKind_Realization},
      {"Dependency", ImUmlRelationKind_Dependency},
      {"Aggregation", ImUmlRelationKind_Aggregation},
      {"Composition", ImUmlRelationKind_Composition},
  };
  return t;
}

class class_class : public item_class {
 public:
  class_class()
      : item_class(
            "Class",
            kUmlClassItemSlot,
            {},
            concat(
                {
                    text_field("id", "", "Unique id that relations and notes name."),
                    text_field("name", "", "Name shown; empty shows the id."),
                    enum_field(
                        "kind",
                        ImUmlClassKind_Class,
                        class_kind_names(),
                        "Abstract, Interface and Enum show their stereotype; an abstract name is italic."),
                    text_field("stereotype", "", "Stereotype shown as «text» above the name; replaces the kind's."),
                    text_field("generic", "", "Type parameters (mermaid's Animal~T~), shown on the box's corner."),
                    texts_field(
                        "attributes",
                        "Attribute lines, drawn as written (\"+name: string\"). A trailing $ underlines (static), a "
                        "trailing * italicises (abstract)."),
                    texts_field("methods", "Method lines, drawn as written (\"+speak() string\"); $ and * as above."),
                    rank_field(),
                },
                node_color_fields()),
            "A class box with name, attribute and method compartments.") {
    id_ = field("id");
    name_ = field("name");
    kind_ = field("kind");
    stereotype_ = field("stereotype");
    generic_ = field("generic");
    attributes_ = field("attributes");
    methods_ = field("methods");
  }

  void draw(const node& n, draw_context&) const override {
    set_next_colors(n);
    set_next_rank(n);
    ImUml::BeginClass(
        n.get<std::string>(id_).c_str(),
        or_null(n, name_),
        static_cast<ImUmlClassKind>(n.get<int64_t>(kind_)),
        or_null(n, stereotype_),
        or_null(n, generic_));
    for (const std::string& a : n.get<std::vector<std::string>>(attributes_)) {
      ImUml::ClassAttribute(a.c_str());
    }
    for (const std::string& m : n.get<std::vector<std::string>>(methods_)) {
      ImUml::ClassMethod(m.c_str());
    }
    ImUml::EndClass();
  }

 private:
  size_t id_, name_, kind_, stereotype_, generic_, attributes_, methods_;
};

class relation_class : public item_class {
 public:
  relation_class()
      : item_class(
            "ClassRelation",
            kUmlClassItemSlot,
            {},
            concat(
                {
                    text_field("from", "", "Id of the class the relation starts at (the child, the whole, the user)."),
                    text_field("to", "", "Id of the class it points to (the parent, the part, the used)."),
                    enum_field(
                        "kind",
                        ImUmlRelationKind_Association,
                        relation_kind_names(),
                        "Inheritance and Realization draw a hollow triangle at `to` and rank `to` above; "
                        "Aggregation / Composition a hollow / filled diamond at `from`; DirectedAssociation and "
                        "Dependency (dashed) an open arrow at `to`."),
                    text_field("label", "", "Text drawn beside the line."),
                    text_field("from_label", "", "Text near the `from` end (a multiplicity such as \"1\")."),
                    text_field("to_label", "", "Text near the `to` end (a multiplicity such as \"*\")."),
                },
                edge_layout_fields()),
            "A relation between two classes.",
            {"from", "to"}) {
    set_edge(true);
    from_ = field("from");
    to_ = field("to");
    kind_ = field("kind");
    label_ = field("label");
    from_label_ = field("from_label");
    to_label_ = field("to_label");
  }

  void draw(const node& n, draw_context&) const override {
    set_next_colors(n);
    set_next_edge_layout(n);
    ImUml::Relation(
        n.get<std::string>(from_).c_str(),
        n.get<std::string>(to_).c_str(),
        static_cast<ImUmlRelationKind>(n.get<int64_t>(kind_)),
        or_null(n, label_),
        or_null(n, from_label_),
        or_null(n, to_label_));
  }

  size_t from_, to_, kind_, label_, from_label_, to_label_;
};

class class_diagram_class : public diagram_class {
 public:
  class_diagram_class()
      : diagram_class(
            "ClassDiagram",
            diagram_kind::class_diagram,
            kUmlClassItemSlot,
            {},
            "A UML class diagram, laid out automatically (Graphviz dot's layered layout). Children: Class, "
            "ClassRelation, Note.") {}

 protected:
  void check_rules(const node& n, const id_table& ids) const override {
    walk(n, [&](const node& child, size_t) {
      if (is_a(child, "Note")) {
        check_note(child, false);
      } else if (is_a(child, "ClassRelation")) {
        const auto& rel = static_cast<const relation_class&>(*child.cls);
        for (const size_t f : {rel.from_, rel.to_}) {
          const node& target = ids.resolve(child, f, child.get<std::string>(f));
          if (!is_a(target, "Class")) {
            child.cls->fail_at(child, f, "must name a Class, not a " + std::string(target.cls->name()));
          }
        }
      }
    });
  }
};

} // namespace

void register_class_diagram(registry& reg) {
  reg.add(std::make_unique<class_diagram_class>());
  reg.add(std::make_unique<class_class>());
  reg.add(std::make_unique<relation_class>());
}

} // namespace bdg::nymph::uml
