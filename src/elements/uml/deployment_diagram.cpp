// MIT License © 2026 Binary Dice Games
/// @file deployment_diagram.cpp
/// @brief `DeploymentDiagram`, `DeploymentNode`, `Artifact` and
///        `DeploymentLink` (spec/13_more_diagrams_and_shapes/DESIGN.md
///        section 5).
#include <memory>
#include <string>
#include "elements/uml/uml_common.hpp"

namespace bdg::nymph::uml {

namespace {

constexpr int64_t kNodeKind = 0, kDeviceKind = 1, kEnvironmentKind = 2;

const name_table& node_kinds() {
  static const name_table t{
      {"Node", kNodeKind}, {"Device", kDeviceKind}, {"ExecutionEnvironment", kEnvironmentKind}};
  return t;
}

constexpr int64_t kCommunication = 0, kDeploy = 1, kManifest = 2;

const name_table& link_kinds() {
  static const name_table t{{"Communication", kCommunication}, {"Deploy", kDeploy}, {"Manifest", kManifest}};
  return t;
}

class deployment_node_class : public item_class {
 public:
  deployment_node_class()
      : item_class(
            "DeploymentNode",
            kUmlDeploymentItemSlot,
            {kUmlDeploymentItemSlot, kUmlNoteSlot},
            concat(
                {
                    text_field("id", "", "Unique id that links and notes name."),
                    text_field("name", "", "Name shown; empty shows the id."),
                    enum_field(
                        "kind",
                        kNodeKind,
                        node_kinds(),
                        "Node (default), Device (shown as «device») or ExecutionEnvironment (shown as «execution "
                        "environment»)."),
                    shape_field("node_3d", "The node's drawing."),
                    rank_field(),
                },
                node_color_fields()),
            "A deployment target: a device or an execution environment. Nodes, artifacts and components written "
            "inside it are deployed on it. Children: DeploymentNode, Artifact, Component, Note.") {
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
      case kDeviceKind:
        stereotype = "device";
        break;
      case kEnvironmentKind:
        stereotype = "execution environment";
        break;
      default:
        break;
    }
    draw_shape_item(n, dc, n.get<std::string>(id_), or_null(n, name_), shape_of(n, shape_), stereotype);
  }

  size_t id_, name_, kind_, shape_;
};

class artifact_class : public item_class {
 public:
  artifact_class()
      : item_class(
            "Artifact",
            kUmlDeploymentItemSlot,
            {},
            concat(
                {
                    text_field("id", "", "Unique id that links and notes name."),
                    text_field("name", "", "File or package name shown; empty shows the id."),
                    shape_field("artifact", "The artifact's drawing."),
                    rank_field(),
                },
                node_color_fields()),
            "An artifact: a file, a package, a library or an executable that is deployed (shown as «artifact»).") {
    id_ = field("id");
    name_ = field("name");
    shape_ = field("shape");
  }

  void draw(const node& n, draw_context& dc) const override {
    set_next_colors(n);
    set_next_rank(n);
    draw_shape_item(n, dc, n.get<std::string>(id_), or_null(n, name_), shape_of(n, shape_), "artifact");
  }

  size_t id_, name_, shape_;
};

class deployment_link_class : public item_class {
 public:
  deployment_link_class()
      : item_class(
            "DeploymentLink",
            kUmlDeploymentItemSlot,
            {},
            concat(
                {
                    text_field("from", "", "Id of the node, artifact or component the link leaves."),
                    text_field("to", "", "Id it reaches."),
                    enum_field(
                        "kind",
                        kCommunication,
                        link_kinds(),
                        "Communication (a line between two nodes; the label is the protocol), Deploy (an artifact or "
                        "component deployed on a node, shown across the diagram) or Manifest (an artifact "
                        "manifesting a component)."),
                    text_field("label", "", "Text in the middle of the link, such as a protocol."),
                    text_field("from_label", "", "Text next to the from end."),
                    text_field("to_label", "", "Text next to the to end."),
                },
                edge_layout_fields()),
            "A communication path between nodes, or a deploy / manifest relation.",
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
    ImUmlShapeLinkFlags flags = 0;
    std::string label = n.get<std::string>(label_);
    const int64_t kind = n.get<int64_t>(kind_);
    if (kind != kCommunication) {
      flags = ImUmlShapeLinkFlags_Dashed | ImUmlShapeLinkFlags_ArrowOpen;
      const std::string stereotype = std::string("\xC2\xAB") + (kind == kDeploy ? "deploy" : "manifest") + "\xC2\xBB";
      label = label.empty() ? stereotype : stereotype + " " + label;
    }
    ImUml::ShapeLink(
        n.get<std::string>(from_).c_str(), n.get<std::string>(to_).c_str(), label.empty() ? nullptr : label.c_str(),
        flags, or_null(n, from_label_), or_null(n, to_label_));
  }

  size_t from_, to_, kind_, label_, from_label_, to_label_;
};

class deployment_diagram_class : public diagram_class {
 public:
  deployment_diagram_class()
      : diagram_class(
            "DeploymentDiagram",
            diagram_kind::deployment,
            kUmlDeploymentItemSlot,
            {},
            "A UML deployment diagram: nodes (devices, execution environments) with the artifacts and components "
            "deployed on them, and the communication paths between nodes, laid out automatically. Children: "
            "DeploymentNode, Artifact, Component, DeploymentLink, Note.") {}

 protected:
  void check_rules(const node& n, const id_table& ids) const override {
    walk(n, [&](const node& child, size_t) {
      if (is_a(child, "Note")) {
        check_note(child, false);
        return;
      }
      if (!is_a(child, "DeploymentLink")) {
        return;
      }
      const auto& lc = static_cast<const deployment_link_class&>(*child.cls);
      const node& from = ids.resolve(child, lc.from_, child.get<std::string>(lc.from_));
      const node& to = ids.resolve(child, lc.to_, child.get<std::string>(lc.to_));
      const int64_t kind = child.get<int64_t>(lc.kind_);
      const auto name = [](const node& x) { return std::string(x.cls->name()); };
      if (kind == kCommunication) {
        if (!is_a(from, "DeploymentNode")) {
          child.cls->fail_at(child, lc.from_, "of a Communication link must be a DeploymentNode, not a " + name(from));
        }
        if (!is_a(to, "DeploymentNode")) {
          child.cls->fail_at(child, lc.to_, "of a Communication link must be a DeploymentNode, not a " + name(to));
        }
      } else if (kind == kDeploy) {
        if (!is_a(from, "Artifact") && !is_a(from, "Component")) {
          child.cls->fail_at(child, lc.from_, "of a Deploy link must be an Artifact or a Component, not a " + name(from));
        }
        if (!is_a(to, "DeploymentNode")) {
          child.cls->fail_at(child, lc.to_, "of a Deploy link must be a DeploymentNode, not a " + name(to));
        }
      } else {
        if (!is_a(from, "Artifact")) {
          child.cls->fail_at(child, lc.from_, "of a Manifest link must be an Artifact, not a " + name(from));
        }
        if (!is_a(to, "Component")) {
          child.cls->fail_at(child, lc.to_, "of a Manifest link must be a Component, not a " + name(to));
        }
      }
      if (contains_node(from, to) || contains_node(to, from)) {
        child.cls->fail_at(child, lc.to_, "joins a node with something deployed inside it; link nodes outside it");
      }
    });
  }
};

} // namespace

void register_deployment_diagram(registry& reg) {
  reg.add(std::make_unique<deployment_diagram_class>());
  reg.add(std::make_unique<deployment_node_class>());
  reg.add(std::make_unique<artifact_class>());
  reg.add(std::make_unique<deployment_link_class>());
}

} // namespace bdg::nymph::uml
