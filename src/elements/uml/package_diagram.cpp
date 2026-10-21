// MIT License © 2026 Binary Dice Games
/// @file package_diagram.cpp
/// @brief `PackageDiagram`, `Package`, `PackageItem` and `PackageRelation`
///        (spec/13_more_diagrams_and_shapes/DESIGN.md section 5).
#include <memory>
#include <string>
#include "elements/uml/uml_common.hpp"

namespace bdg::nymph::uml {

namespace {

constexpr int64_t kItemClass = 0, kItemInterface = 1, kItemEnum = 2, kItemComponent = 3, kItemOther = 4;

const name_table& item_kinds() {
  static const name_table t{
      {"Class", kItemClass},
      {"Interface", kItemInterface},
      {"Enum", kItemEnum},
      {"Component", kItemComponent},
      {"Other", kItemOther},
  };
  return t;
}

constexpr int64_t kDependency = 0, kImport = 1, kAccess = 2, kMerge = 3;

const name_table& relation_kinds() {
  static const name_table t{{"Dependency", kDependency}, {"Import", kImport}, {"Access", kAccess}, {"Merge", kMerge}};
  return t;
}

class package_class : public item_class {
 public:
  package_class()
      : item_class(
            "Package",
            kUmlPackageItemSlot,
            {kUmlPackageItemSlot, kUmlNoteSlot},
            concat(
                {
                    text_field("id", "", "Unique id that relations and notes name."),
                    text_field("name", "", "Name shown on the tab; empty shows the id."),
                    shape_field("package", "The package's drawing."),
                    rank_field(),
                },
                node_color_fields()),
            "A package (a namespace or module). With children (packages, items) it is a folder drawn around them. "
            "Children: Package, PackageItem, Note.") {
    id_ = field("id");
    name_ = field("name");
    shape_ = field("shape");
  }

  void draw(const node& n, draw_context& dc) const override {
    set_next_colors(n);
    set_next_rank(n);
    draw_shape_item(n, dc, n.get<std::string>(id_), or_null(n, name_), shape_of(n, shape_), nullptr);
  }

  size_t id_, name_, shape_;
};

class package_item_class : public item_class {
 public:
  package_item_class()
      : item_class(
            "PackageItem",
            kUmlPackageItemSlot,
            {},
            concat(
                {
                    text_field("id", "", "Unique id that relations and notes name."),
                    text_field("name", "", "Name shown; empty shows the id."),
                    enum_field(
                        "kind",
                        kItemClass,
                        item_kinds(),
                        "Class (default), Interface, Enum, Component or Other; all but Class and Other show a "
                        "stereotype."),
                    shape_field("rectangle", "The item's drawing."),
                    rank_field(),
                },
                node_color_fields()),
            "A named element inside a package: a class, an interface, a component.") {
    id_ = field("id");
    name_ = field("name");
    kind_ = field("kind");
    shape_ = field("shape");
  }

  void draw(const node& n, draw_context& dc) const override {
    set_next_colors(n);
    set_next_rank(n);
    const char* stereotype = nullptr;
    switch (n.get<int64_t>(kind_)) {
      case kItemInterface:
        stereotype = "interface";
        break;
      case kItemEnum:
        stereotype = "enumeration";
        break;
      case kItemComponent:
        stereotype = "component";
        break;
      default:
        break;
    }
    draw_shape_item(n, dc, n.get<std::string>(id_), or_null(n, name_), shape_of(n, shape_), stereotype);
  }

  size_t id_, name_, kind_, shape_;
};

class package_relation_class : public item_class {
 public:
  package_relation_class()
      : item_class(
            "PackageRelation",
            kUmlPackageItemSlot,
            {},
            concat(
                {
                    text_field("from", "", "Id of the package or item that depends on the other."),
                    text_field("to", "", "Id of the one it depends on."),
                    enum_field(
                        "kind",
                        kDependency,
                        relation_kinds(),
                        "Dependency (a dashed arrow, default), Import («import»), Access («access») or Merge "
                        "(«merge»)."),
                    text_field("label", "", "Text in the middle, after the stereotype."),
                },
                edge_layout_fields()),
            "A dependency between packages or items.",
            {"from", "to"}) {
    set_edge(true);
    from_ = field("from");
    to_ = field("to");
    kind_ = field("kind");
    label_ = field("label");
  }

  void draw(const node& n, draw_context&) const override {
    set_next_colors(n);
    set_next_edge_layout(n);
    const char* keyword = nullptr;
    switch (n.get<int64_t>(kind_)) {
      case kImport:
        keyword = "import";
        break;
      case kAccess:
        keyword = "access";
        break;
      case kMerge:
        keyword = "merge";
        break;
      default:
        break;
    }
    std::string label = n.get<std::string>(label_);
    if (keyword != nullptr) {
      const std::string stereotype = std::string("\xC2\xAB") + keyword + "\xC2\xBB";
      label = label.empty() ? stereotype : stereotype + " " + label;
    }
    ImUml::ShapeLink(
        n.get<std::string>(from_).c_str(),
        n.get<std::string>(to_).c_str(),
        label.empty() ? nullptr : label.c_str(),
        ImUmlShapeLinkFlags_Dashed | ImUmlShapeLinkFlags_ArrowOpen);
  }

  size_t from_, to_, kind_, label_;
};

class package_diagram_class : public diagram_class {
 public:
  package_diagram_class()
      : diagram_class(
            "PackageDiagram",
            diagram_kind::package,
            kUmlPackageItemSlot,
            {},
            "A UML package diagram: packages (nested as written) and the dependencies between them, laid out "
            "automatically. Children: Package, PackageItem, PackageRelation, Note.") {}

 protected:
  void check_rules(const node& n, const id_table& ids) const override {
    walk(n, [&](const node& child, size_t) {
      if (is_a(child, "Note")) {
        check_note(child, false);
        return;
      }
      if (!is_a(child, "PackageRelation")) {
        return;
      }
      const auto& rc = static_cast<const package_relation_class&>(*child.cls);
      const node& from = ids.resolve(child, rc.from_, child.get<std::string>(rc.from_));
      const node& to = ids.resolve(child, rc.to_, child.get<std::string>(rc.to_));
      if (contains_node(from, to) || contains_node(to, from)) {
        child.cls->fail_at(
            child, rc.to_, "joins a package with something inside it; relate packages or items outside it");
      }
    });
  }
};

} // namespace

void register_package_diagram(registry& reg) {
  reg.add(std::make_unique<package_diagram_class>());
  reg.add(std::make_unique<package_class>());
  reg.add(std::make_unique<package_item_class>());
  reg.add(std::make_unique<package_relation_class>());
}

} // namespace bdg::nymph::uml
