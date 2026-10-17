// MIT License © 2026 Binary Dice Games
/// @file network_diagram.cpp
/// @brief `NetworkDiagram`, `Device`, `Link` and `Zone`
///        (spec/03_uml_diagrams/DESIGN.md section 3): network topology with
///        device icons and zones.
#include <memory>
#include <string>
#include "elements/uml/uml_common.hpp"

namespace bdg::nymph::uml {

namespace {

const name_table& device_kind_names() {
  static const name_table t{
      {"Server", ImUmlDeviceKind_Server},
      {"Database", ImUmlDeviceKind_Database},
      {"Storage", ImUmlDeviceKind_Storage},
      {"Router", ImUmlDeviceKind_Router},
      {"Switch", ImUmlDeviceKind_Switch},
      {"Firewall", ImUmlDeviceKind_Firewall},
      {"LoadBalancer", ImUmlDeviceKind_LoadBalancer},
      {"AccessPoint", ImUmlDeviceKind_AccessPoint},
      {"Cloud", ImUmlDeviceKind_Cloud},
      {"Internet", ImUmlDeviceKind_Internet},
      {"Workstation", ImUmlDeviceKind_Workstation},
      {"Laptop", ImUmlDeviceKind_Laptop},
      {"Mobile", ImUmlDeviceKind_Mobile},
      {"User", ImUmlDeviceKind_User},
  };
  return t;
}

const name_table& link_kind_names() {
  static const name_table t{
      {"Wired", ImUmlLinkKind_Wired},
      {"Wireless", ImUmlLinkKind_Wireless},
      {"Vpn", ImUmlLinkKind_Vpn},
  };
  return t;
}

class device_class : public item_class {
 public:
  device_class()
      : item_class(
            "Device",
            kUmlNetworkItemSlot,
            {},
            concat(
                {
                    text_field("id", "", "Unique id that links and notes name."),
                    text_field("name", "", "Name shown under the icon; empty shows the id."),
                    enum_field("kind", ImUmlDeviceKind_Server, device_kind_names(), "The icon."),
                    text_field("address", "", "Shown under the name, dimmer: an IP address, a host name, a CIDR."),
                    rank_field(),
                },
                node_color_fields()),
            "A network device drawn as an icon: server, database, storage, router, switch, firewall, load balancer, "
            "access point, cloud, internet, workstation, laptop, mobile or user.") {
    id_ = field("id");
    name_ = field("name");
    kind_ = field("kind");
    address_ = field("address");
  }

  void draw(const node& n, draw_context&) const override {
    set_next_colors(n);
    set_next_rank(n);
    ImUml::Device(
        n.get<std::string>(id_).c_str(),
        or_null(n, name_),
        static_cast<ImUmlDeviceKind>(n.get<int64_t>(kind_)),
        or_null(n, address_));
  }

 private:
  size_t id_, name_, kind_, address_;
};

class link_class : public item_class {
 public:
  link_class()
      : item_class(
            "Link",
            kUmlNetworkItemSlot,
            {},
            concat(
                {
                    text_field("from", "", "Id of one device."),
                    text_field("to", "", "Id of the other device."),
                    enum_field(
                        "kind",
                        ImUmlLinkKind_Wired,
                        link_kind_names(),
                        "Wired: solid line. Wireless: short dashes. Vpn: long dashes, a thicker line."),
                    text_field("label", "", "Text beside the line (\"1 Gbit/s\", \"VLAN 20\")."),
                    text_field("from_label", "", "Text near the `from` end (a port, an interface: \"eth0\")."),
                    text_field("to_label", "", "Text near the `to` end."),
                    bool_field("directed", false, "true: an arrowhead at the `to` end (traffic in one direction)."),
                },
                edge_layout_fields()),
            "A connection between two devices.",
            {"from", "to"}) {
    set_edge(true);
    from_ = field("from");
    to_ = field("to");
    kind_ = field("kind");
    label_ = field("label");
    from_label_ = field("from_label");
    to_label_ = field("to_label");
    directed_ = field("directed");
  }

  void draw(const node& n, draw_context&) const override {
    set_next_colors(n);
    set_next_edge_layout(n);
    ImUml::Link(
        n.get<std::string>(from_).c_str(),
        n.get<std::string>(to_).c_str(),
        static_cast<ImUmlLinkKind>(n.get<int64_t>(kind_)),
        or_null(n, label_),
        or_null(n, from_label_),
        or_null(n, to_label_),
        n.get<bool>(directed_) ? ImUmlLinkFlags_Directed : ImUmlLinkFlags_None);
  }

  size_t from_, to_, kind_, label_, from_label_, to_label_, directed_;
};

class zone_class : public item_class {
 public:
  zone_class()
      : item_class(
            "Zone",
            kUmlNetworkItemSlot,
            {kUmlNetworkItemSlot, kUmlNoteSlot},
            concat(
                {
                    text_field("id", "", "Unique id (zones cannot be the end of a link)."),
                    text_field("name", "", "Label in the box's corner (\"DMZ 10.0.1.0/24\"); empty shows the id."),
                },
                node_color_fields()),
            "A dashed box around devices: a subnet, a VLAN, a site, a cloud region; zones nest.") {
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

class network_diagram_class : public diagram_class {
 public:
  network_diagram_class()
      : diagram_class(
            "NetworkDiagram",
            diagram_kind::network,
            kUmlNetworkItemSlot,
            {},
            "A network diagram: devices drawn as icons, joined by links, grouped in zones, laid out automatically. "
            "Children: Device, Link, Zone, Note.") {}

 protected:
  void check_rules(const node& n, const id_table& ids) const override {
    walk(n, [&](const node& child, size_t) {
      if (is_a(child, "Note")) {
        check_note(child, false);
      } else if (is_a(child, "Link")) {
        const auto& lc = static_cast<const link_class&>(*child.cls);
        for (const size_t f : {lc.from_, lc.to_}) {
          const node& target = ids.resolve(child, f, child.get<std::string>(f));
          if (!is_a(target, "Device")) {
            child.cls->fail_at(child, f, "must name a Device, not a " + std::string(target.cls->name()));
          }
        }
      }
    });
  }
};

} // namespace

void register_network_diagram(registry& reg) {
  reg.add(std::make_unique<network_diagram_class>());
  reg.add(std::make_unique<device_class>());
  reg.add(std::make_unique<link_class>());
  reg.add(std::make_unique<zone_class>());
}

} // namespace bdg::nymph::uml
