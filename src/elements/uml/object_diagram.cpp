// MIT License © 2026 Binary Dice Games
/// @file object_diagram.cpp
/// @brief `ObjectDiagram`, `Object` and `ObjectLink`: instances with slot
///        values, on the class diagram's layout
///        (spec/13_more_diagrams_and_shapes/DESIGN.md section 5).
#include <memory>
#include <string>
#include "elements/uml/uml_common.hpp"

namespace bdg::nymph::uml {

namespace {

const name_table& link_kinds() {
  static const name_table t{
      {"Association", ImUmlRelationKind_Association},
      {"DirectedAssociation", ImUmlRelationKind_DirectedAssociation},
      {"Aggregation", ImUmlRelationKind_Aggregation},
      {"Composition", ImUmlRelationKind_Composition},
      {"Dependency", ImUmlRelationKind_Dependency},
  };
  return t;
}

class object_class : public item_class {
 public:
  object_class()
      : item_class(
            "Object",
            kUmlObjectItemSlot,
            {},
            concat(
                {
                    text_field("id", "", "Unique id that links and notes name."),
                    text_field("name", "", "The instance name (order1); empty draws an anonymous \": Order\"."),
                    text_field("class", "", "The class the object is an instance of, shown after the name."),
                    texts_field("slots", "Slot lines, drawn as written (\"total = 42.50\")."),
                    rank_field(),
                },
                node_color_fields()),
            "An object: an instance of a class with values for its attributes (slots). The header \"name : class\" "
            "is underlined.") {
    id_ = field("id");
    name_ = field("name");
    class_ = field("class");
    slots_ = field("slots");
  }

  void draw(const node& n, draw_context&) const override {
    set_next_colors(n);
    set_next_rank(n);
    const std::string& name = n.get<std::string>(name_);
    const std::string& cls = n.get<std::string>(class_);
    std::string header = name.empty() && cls.empty() ? n.get<std::string>(id_) : name;
    if (!cls.empty()) {
      header += (header.empty() ? ": " : " : ") + cls;
    }
    ImUml::BeginClass(n.get<std::string>(id_).c_str(), header.c_str(), ImUmlClassKind_Object);
    for (const std::string& slot : n.get<std::vector<std::string>>(slots_)) {
      ImUml::ClassAttribute(slot.c_str());
    }
    ImUml::EndClass();
  }

  size_t id_, name_, class_, slots_;
};

class object_link_class : public item_class {
 public:
  object_link_class()
      : item_class(
            "ObjectLink",
            kUmlObjectItemSlot,
            {},
            concat(
                {
                    text_field("from", "", "Id of the object the link leaves."),
                    text_field("to", "", "Id of the object it reaches."),
                    enum_field(
                        "kind",
                        ImUmlRelationKind_Association,
                        link_kinds(),
                        "Association (a plain line, default), DirectedAssociation, Aggregation, Composition or "
                        "Dependency."),
                    text_field("label", "", "Text in the middle of the link, such as a role name."),
                    text_field("from_label", "", "Text next to the from end."),
                    text_field("to_label", "", "Text next to the to end."),
                },
                edge_layout_fields()),
            "A link between two objects.",
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

class object_diagram_class : public diagram_class {
 public:
  object_diagram_class()
      : diagram_class(
            "ObjectDiagram",
            diagram_kind::object,
            kUmlObjectItemSlot,
            {},
            "A UML object diagram: instances with their slot values and the links between them, laid out "
            "automatically. Children: Object, ObjectLink, Note.") {}

 protected:
  void check_rules(const node& n, const id_table& ids) const override {
    (void)ids;
    walk(n, [&](const node& child, size_t) {
      if (is_a(child, "Note")) {
        check_note(child, false);
      }
    });
  }
};

} // namespace

void register_object_diagram(registry& reg) {
  reg.add(std::make_unique<object_diagram_class>());
  reg.add(std::make_unique<object_class>());
  reg.add(std::make_unique<object_link_class>());
}

} // namespace bdg::nymph::uml
