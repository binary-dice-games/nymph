// MIT License © 2026 Binary Dice Games
/// @file dataflow_diagram.cpp
/// @brief `DataFlowDiagram`, `ExternalEntity`, `Process`, `DataStore`,
///        `DataFlow` and `TrustBoundary` (spec/03_uml_diagrams/DESIGN.md
///        section 3): data flow diagrams in Yourdon / DeMarco notation.
#include <memory>
#include <string>
#include "core/error.hpp"
#include "elements/uml/uml_common.hpp"

namespace bdg::nymph::uml {

namespace {

/// `ExternalEntity`, `Process` and `DataStore`: one class, three shapes.
class dfd_node_class : public item_class {
 public:
  enum class shape { entity, process, store };

  dfd_node_class(const char* name, shape s, const char* number_doc, const char* description)
      : item_class(
            name,
            kUmlDataFlowItemSlot,
            {},
            concat(
                number_doc != nullptr
                    ? std::vector<field_desc>{text_field("id", "", "Unique id that data flows and notes name."),
                                              text_field("name", "", "Name shown; empty shows the id."),
                                              text_field("number", "", number_doc),
                                              rank_field()}
                    : std::vector<field_desc>{text_field("id", "", "Unique id that data flows and notes name."),
                                              text_field("name", "", "Name shown; empty shows the id."),
                                              rank_field()},
                node_color_fields()),
            description),
        shape_(s) {
    id_ = field("id");
    name_ = field("name");
    number_ = field_index("number");
  }

  void draw(const node& n, draw_context&) const override {
    set_next_colors(n);
    set_next_rank(n);
    const char* id = n.get<std::string>(id_).c_str();
    const char* number = number_ >= 0 ? or_null(n, static_cast<size_t>(number_)) : nullptr;
    switch (shape_) {
      case shape::entity:
        ImUml::ExternalEntity(id, or_null(n, name_));
        break;
      case shape::process:
        ImUml::Process(id, or_null(n, name_), number);
        break;
      case shape::store:
        ImUml::DataStore(id, or_null(n, name_), number);
        break;
    }
  }

 private:
  shape shape_;
  size_t id_, name_;
  int number_;
};

class data_flow_class : public item_class {
 public:
  data_flow_class()
      : item_class(
            "DataFlow",
            kUmlDataFlowItemSlot,
            {},
            concat(
                {
                    text_field("from", "", "Id of the node the data leaves."),
                    text_field("to", "", "Id of the node it reaches."),
                    text_field("label", "", "What flows (\"order details\")."),
                    bool_field("bidirectional", false, "true: arrowheads at both ends."),
                },
                edge_layout_fields()),
            "Data moving between two nodes. One end must be a Process: data does not move between entities and "
            "stores by itself.",
            {"from", "to"}) {
    set_edge(true);
    from_ = field("from");
    to_ = field("to");
    label_ = field("label");
    bidirectional_ = field("bidirectional");
  }

  void draw(const node& n, draw_context&) const override {
    set_next_colors(n);
    set_next_edge_layout(n);
    ImUml::DataFlow(
        n.get<std::string>(from_).c_str(),
        n.get<std::string>(to_).c_str(),
        or_null(n, label_),
        n.get<bool>(bidirectional_) ? ImUmlDataFlowFlags_Bidirectional : ImUmlDataFlowFlags_None);
  }

  size_t from_, to_, label_, bidirectional_;
};

class trust_boundary_class : public item_class {
 public:
  trust_boundary_class()
      : item_class(
            "TrustBoundary",
            kUmlDataFlowItemSlot,
            {kUmlDataFlowItemSlot, kUmlNoteSlot},
            concat(
                {
                    text_field("id", "", "Unique id (boundaries cannot be the end of a data flow)."),
                    text_field("name", "", "Label in the box's corner; empty shows the id."),
                },
                node_color_fields()),
            "A dashed box around the nodes inside a trust boundary (a network, a process, a machine); boundaries "
            "nest.") {
    id_ = field("id");
    name_ = field("name");
  }

  void draw(const node& n, draw_context& dc) const override {
    set_next_colors(n);
    ImUml::BeginGroup(n.get<std::string>(id_).c_str(), or_null(n, name_));
    dc.draw_children(n);
    ImUml::EndGroup();
  }

 private:
  size_t id_, name_;
};

class dataflow_diagram_class : public diagram_class {
 public:
  dataflow_diagram_class()
      : diagram_class(
            "DataFlowDiagram",
            diagram_kind::data_flow,
            kUmlDataFlowItemSlot,
            {},
            "A data flow diagram (DFD): external entities, processes and data stores joined by labelled data flows, "
            "with optional trust boundaries, laid out automatically (left to right). Children: ExternalEntity, "
            "Process, DataStore, DataFlow, TrustBoundary, Note.") {}

 protected:
  void check_rules(const node& n, const id_table& ids) const override {
    walk(n, [&](const node& child, size_t) {
      if (is_a(child, "Note")) {
        check_note(child, false);
        return;
      }
      if (!is_a(child, "DataFlow")) {
        return;
      }
      const auto& fc = static_cast<const data_flow_class&>(*child.cls);
      const node& from = ids.resolve(child, fc.from_, child.get<std::string>(fc.from_));
      const node& to = ids.resolve(child, fc.to_, child.get<std::string>(fc.to_));
      for (const auto& [f, target] : {std::pair<size_t, const node*>{fc.from_, &from}, {fc.to_, &to}}) {
        if (is_a(*target, "TrustBoundary")) {
          child.cls->fail_at(child, f, "names a TrustBoundary; data flows join entities, processes and stores");
        }
      }
      if (!is_a(from, "Process") && !is_a(to, "Process")) {
        child.cls->fail_at(
            child,
            fc.to_,
            "joins a " + std::string(from.cls->name()) + " and a " + std::string(to.cls->name()) +
                "; one end of a data flow must be a Process");
      }
    });
  }
};

} // namespace

void register_dataflow_diagram(registry& reg) {
  reg.add(std::make_unique<dataflow_diagram_class>());
  reg.add(std::make_unique<dfd_node_class>(
      "ExternalEntity",
      dfd_node_class::shape::entity,
      nullptr,
      "A source or destination of data outside the system (a user, another system): a box."));
  reg.add(std::make_unique<dfd_node_class>(
      "Process",
      dfd_node_class::shape::process,
      "Process number shown above the name (\"1\", \"2.3\").",
      "A process that transforms data: a circle."));
  reg.add(std::make_unique<dfd_node_class>(
      "DataStore",
      dfd_node_class::shape::store,
      "Store number shown in its left cell (\"D1\").",
      "Data at rest (a database, a file, a queue): a box open on the right."));
  reg.add(std::make_unique<data_flow_class>());
  reg.add(std::make_unique<trust_boundary_class>());
}

} // namespace bdg::nymph::uml
