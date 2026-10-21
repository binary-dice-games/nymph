// MIT License © 2026 Binary Dice Games
/// @file component_diagram.cpp
/// @brief `ComponentDiagram`, `Component` and `ComponentLink`: components
///        with provided and required interfaces; the ball-and-socket
///        connectors are derived from the interface names
///        (spec/13_more_diagrams_and_shapes/DESIGN.md section 5).
#include <map>
#include <memory>
#include <set>
#include <string>
#include "elements/uml/uml_common.hpp"

namespace bdg::nymph::uml {

namespace {

constexpr int64_t kDependency = 0, kAssociation = 1, kDelegation = 2, kRealization = 3;

const name_table& component_link_kinds() {
  static const name_table t{
      {"Dependency", kDependency},
      {"Association", kAssociation},
      {"Delegation", kDelegation},
      {"Realization", kRealization},
  };
  return t;
}

class component_class : public item_class {
 public:
  component_class()
      : item_class(
            "Component",
            kUmlComponentItemSlot,
            {kUmlComponentItemSlot, kUmlNoteSlot},
            concat(
                {
                    text_field("id", "", "Unique id that links and notes name."),
                    text_field("name", "", "Name shown; empty shows the id."),
                    text_field(
                        "stereotype",
                        "component",
                        "Shown as «text» over the name; empty hides it (default component)."),
                    texts_field(
                        "provides",
                        "Interfaces this component provides. A ball-and-socket connector is drawn to every component "
                        "that requires the same name; an interface nobody requires is a ball on a stub."),
                    texts_field(
                        "requires",
                        "Interfaces this component requires: connected to the one component that provides the name, "
                        "else a socket on a stub (an external dependency)."),
                    shape_field("component", "The component's drawing."),
                    rank_field(),
                },
                node_color_fields()),
            "A component (a replaceable part of a system) with provided and required interfaces. With children "
            "(components) it is a titled frame around them. Children: Component, Note.") {
    id_ = field("id");
    name_ = field("name");
    stereotype_ = field("stereotype");
    provides_ = field("provides");
    requires_ = field("requires");
    shape_ = field("shape");
    also_fits(kUmlDeploymentItemSlot); // a component deployed on a node
  }

  void draw(const node& n, draw_context& dc) const override {
    set_next_colors(n);
    set_next_rank(n);
    const std::string& stereotype = n.get<std::string>(stereotype_);
    draw_shape_item(
        n, dc, n.get<std::string>(id_), or_null(n, name_), shape_of(n, shape_),
        stereotype.empty() ? nullptr : stereotype.c_str());
  }

  size_t id_, name_, stereotype_, provides_, requires_, shape_;
};

class component_link_class : public item_class {
 public:
  component_link_class()
      : item_class(
            "ComponentLink",
            kUmlComponentItemSlot,
            {},
            concat(
                {
                    text_field("from", "", "Id of the component the link leaves."),
                    text_field("to", "", "Id of the component it reaches."),
                    enum_field(
                        "kind",
                        kDependency,
                        component_link_kinds(),
                        "Dependency (dashed arrow, default), Association (a plain line), Delegation (a solid arrow) or "
                        "Realization (dashed, hollow triangle at to)."),
                    text_field("label", "", "Text in the middle of the link."),
                },
                edge_layout_fields()),
            "A link between two components, for what interfaces do not say (a dependency, a delegation).",
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
    ImUmlShapeLinkFlags flags = 0;
    switch (n.get<int64_t>(kind_)) {
      case kDependency:
        flags = ImUmlShapeLinkFlags_Dashed | ImUmlShapeLinkFlags_ArrowOpen;
        break;
      case kDelegation:
        flags = ImUmlShapeLinkFlags_ArrowOpen;
        break;
      case kRealization:
        flags = ImUmlShapeLinkFlags_Dashed | ImUmlShapeLinkFlags_ArrowHollow | ImUmlShapeLinkFlags_RankToFirst;
        break;
      default:
        break;
    }
    ImUml::ShapeLink(n.get<std::string>(from_).c_str(), n.get<std::string>(to_).c_str(), or_null(n, label_), flags);
  }

  size_t from_, to_, kind_, label_;
};

class component_diagram_class : public diagram_class {
 public:
  component_diagram_class()
      : diagram_class(
            "ComponentDiagram",
            diagram_kind::component,
            kUmlComponentItemSlot,
            {},
            "A UML component diagram: components with provided and required interfaces, wired automatically (an "
            "interface name a component provides and another requires becomes a ball-and-socket connector), laid "
            "out automatically. Children: Component, ComponentLink, Note.") {}

 protected:
  void check_rules(const node& n, const id_table& ids) const override {
    std::map<std::string, const node*> provider;
    walk(n, [&](const node& child, size_t) {
      if (is_a(child, "Note")) {
        check_note(child, false);
        return;
      }
      if (is_a(child, "Component")) {
        const auto& cc = static_cast<const component_class&>(*child.cls);
        for (const char* which : {"provides", "requires"}) {
          const size_t field = which[0] == 'p' ? cc.provides_ : cc.requires_;
          for (const std::string& i : child.get<std::vector<std::string>>(field)) {
            if (i.empty()) {
              child.cls->fail_at(child, field, "has an empty interface name");
            }
          }
        }
        for (const std::string& i : child.get<std::vector<std::string>>(cc.provides_)) {
          const auto [it, fresh] = provider.emplace(i, &child);
          if (!fresh && it->second != &child) {
            child.cls->fail_at(
                child,
                cc.provides_,
                ": interface '" + i + "' is already provided by '" + id_of(*it->second) +
                    "'; give the interfaces different names, or join the components with a ComponentLink");
          }
        }
        return;
      }
      if (is_a(child, "ComponentLink")) {
        const auto& lc = static_cast<const component_link_class&>(*child.cls);
        for (const size_t f : {lc.from_, lc.to_}) {
          const node& end = ids.resolve(child, f, child.get<std::string>(f));
          if (!is_a(end, "Component")) {
            child.cls->fail_at(child, f, "must name a Component, not a " + std::string(end.cls->name()));
          }
        }
      }
    });
  }

  /// The derived wiring: an assembly connector per matched interface, a stub
  /// for each interface with no partner.
  void setup(const node& n) const override {
    std::vector<const node*> comps;
    walk(n, [&](const node& c, size_t) {
      if (is_a(c, "Component")) {
        comps.push_back(&c);
      }
    });
    std::map<std::string, std::string> provider;
    std::set<std::string> required;
    for (const node* c : comps) {
      const auto& cc = static_cast<const component_class&>(*c->cls);
      for (const std::string& i : c->get<std::vector<std::string>>(cc.provides_)) {
        provider.emplace(i, id_of(*c));
      }
      for (const std::string& i : c->get<std::vector<std::string>>(cc.requires_)) {
        required.insert(i);
      }
    }
    std::set<std::string> done;
    for (const node* c : comps) {
      const auto& cc = static_cast<const component_class&>(*c->cls);
      const std::string& id = id_of(*c);
      for (const std::string& i : c->get<std::vector<std::string>>(cc.requires_)) {
        const auto it = provider.find(i);
        if (it != provider.end()) {
          if (it->second != id && done.insert(it->second + '\x1f' + id + '\x1f' + i).second) {
            ImUml::ShapeLink(it->second.c_str(), id.c_str(), i.c_str(), ImUmlShapeLinkFlags_Assembly);
          }
        } else if (done.insert('s' + id + '\x1f' + i).second) {
          const std::string stub = "\x1f" "socket" + id + '\x1f' + i;
          ImUml::ShapeNode(stub.c_str(), i.c_str(), "interface_socket");
          ImUml::ShapeLink(id.c_str(), stub.c_str(), nullptr, 0);
        }
      }
      for (const std::string& i : c->get<std::vector<std::string>>(cc.provides_)) {
        if (required.count(i) == 0 && done.insert('b' + id + '\x1f' + i).second) {
          const std::string stub = "\x1f" "ball" + id + '\x1f' + i;
          ImUml::ShapeNode(stub.c_str(), i.c_str(), "interface_ball");
          ImUml::ShapeLink(id.c_str(), stub.c_str(), nullptr, 0);
        }
      }
    }
  }
};

} // namespace

void register_component_diagram(registry& reg) {
  reg.add(std::make_unique<component_diagram_class>());
  reg.add(std::make_unique<component_class>());
  reg.add(std::make_unique<component_link_class>());
}

} // namespace bdg::nymph::uml
