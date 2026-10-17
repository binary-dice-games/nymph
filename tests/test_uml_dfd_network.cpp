// MIT License © 2026 Binary Dice Games
/// @file test_uml_dfd_network.cpp
/// @brief UML plan step 15: `DataFlowDiagram` (with trust boundaries) and
///        `NetworkDiagram` (devices, links, zones).
#include <gtest/gtest.h>
#include <cstdint>
#include <set>
#include <string>
#include "uml_test_util.hpp"

namespace bdg::nymph {
namespace {

using namespace uml_test;

size_t reddish(const image& img) {
  size_t n = 0;
  for (int y = 0; y < img.height; ++y) {
    for (int x = 0; x < img.width; ++x) {
      const uint8_t* p = img.at(x, y);
      n += p[0] > 120 && p[0] > p[1] + 60 && p[0] > p[2] + 60 ? 1 : 0;
    }
  }
  return n;
}

std::string with(const std::string& src, const std::string& from, const std::string& to) {
  std::string s = src;
  const size_t at = s.find(from);
  EXPECT_NE(at, std::string::npos) << from;
  return s.replace(at, from.size(), to);
}

// ── Data flow ────────────────────────────────────────────────────────────────

const char* const kDfd =
    "image: {width: 760, height: 420}\n" // 1
    "root:\n" // 2
    "  type: DataFlowDiagram\n" // 3
    "  children:\n" // 4
    "    - {type: ExternalEntity, id: user, name: User}\n" // 5
    "    - type: TrustBoundary\n" // 6
    "      id: app\n" // 7
    "      children:\n" // 8
    "        - {type: Process, id: login, name: Log in, number: \"1\"}\n" // 9
    "        - {type: DataStore, id: users, name: Users, number: D1}\n" // 10
    "    - {type: DataFlow, from: user, to: login, label: credentials}\n" // 11
    "    - {type: DataFlow, from: login, to: users, label: lookup, bidirectional: true}\n"; // 12

TEST(uml_dfd, ExampleRendersRoundTripsAndIsDeterministic) {
  EXPECT_GT(non_background(render(kDfd)), 1500u);
  expect_round_trip(kDfd);
}

TEST(uml_dfd, BoundaryHoldsItsNodesAndProcessesAreCircles) {
  const drawn_diagram d = draw_and_inspect(kDfd, {"app", "user", "login", "users"});
  ASSERT_EQ(d.nodes.size(), 4u);
  EXPECT_TRUE(d.nodes.at("app").Contains(d.nodes.at("login")));
  EXPECT_TRUE(d.nodes.at("app").Contains(d.nodes.at("users")));
  EXPECT_FALSE(d.nodes.at("app").Overlaps(d.nodes.at("user")));
  EXPECT_EQ(d.nodes.at("login").GetWidth(), d.nodes.at("login").GetHeight());
  // Left to right by default.
  EXPECT_LT(d.nodes.at("user").Max.x, d.nodes.at("login").Min.x);
}

TEST(uml_dfd, DataFlowRules) {
  expect_error(with(kDfd, "from: user, to: login", "from: user, to: users"), 11, 0,
               "joins a ExternalEntity and a DataStore; one end of a data flow must be a Process");
  expect_error(with(kDfd, "from: user, to: login", "from: user, to: app"), 11, 0, "names a TrustBoundary");
  expect_error(with(kDfd, "id: users,", "id: login,"), 10, 0, "id 'login' is already used");
}

// ── Network ──────────────────────────────────────────────────────────────────

const char* const kNet =
    "image: {width: 760, height: 560}\n" // 1
    "root:\n" // 2
    "  type: NetworkDiagram\n" // 3
    "  children:\n" // 4
    "    - {type: Device, id: fw, kind: Firewall, address: 10.0.0.1}\n" // 5
    "    - type: Zone\n" // 6
    "      id: dmz\n" // 7
    "      children:\n" // 8
    "        - {type: Device, id: web, kind: Server}\n" // 9
    "        - {type: Device, id: db, kind: Database}\n" // 10
    "    - type: Zone\n" // 11
    "      id: lan\n" // 12
    "      children:\n" // 13
    "        - {type: Device, id: sw, kind: Switch}\n" // 14
    "        - {type: Device, id: pc, kind: Workstation}\n" // 15
    "    - {type: Link, from: fw, to: web, from_label: eth1}\n" // 16
    "    - {type: Link, from: web, to: db}\n" // 17
    "    - {type: Link, from: fw, to: sw}\n" // 18
    "    - {type: Link, from: sw, to: pc}\n" // 19
    "    - {type: Link, from: pc, to: db, kind: Vpn, constraint: false}\n"; // 20

TEST(uml_network, ExampleRendersRoundTripsAndIsDeterministic) {
  EXPECT_GT(non_background(render(kNet)), 1500u);
  expect_round_trip(kNet);
}

TEST(uml_network, ZonesHoldTheirDevicesAndNeverOverlap) {
  const drawn_diagram d = draw_and_inspect(kNet, {"dmz", "lan", "web", "db", "sw", "pc", "fw"});
  ASSERT_EQ(d.nodes.size(), 7u);
  EXPECT_TRUE(d.nodes.at("dmz").Contains(d.nodes.at("web")));
  EXPECT_TRUE(d.nodes.at("dmz").Contains(d.nodes.at("db")));
  EXPECT_TRUE(d.nodes.at("lan").Contains(d.nodes.at("sw")));
  EXPECT_TRUE(d.nodes.at("lan").Contains(d.nodes.at("pc")));
  EXPECT_FALSE(d.nodes.at("dmz").Overlaps(d.nodes.at("lan")));
  EXPECT_FALSE(d.nodes.at("dmz").Overlaps(d.nodes.at("fw")));
}

TEST(uml_network, LinkRules) {
  expect_error(with(kNet, "from: fw, to: sw}", "from: fw, to: lan}"), 18, 0, "must name a Device, not a Zone");
  expect_error(with(kNet, "kind: Firewall", "kind: Firewal"), 5, 0, "unknown value 'Firewal'");
}

TEST(uml_network, EveryDeviceIconDraws) {
  const char* kinds[] = {"Server", "Database", "Storage", "Router", "Switch", "Firewall", "LoadBalancer",
                         "AccessPoint", "Cloud", "Internet", "Workstation", "Laptop", "Mobile", "User"};
  std::set<std::string> seen;
  for (const char* kind : kinds) {
    const std::string src = std::string("image: {width: 160, height: 120}\nroot:\n  type: NetworkDiagram\n") +
                            "  children:\n    - {type: Device, id: d, name: \"\", kind: " + kind + "}\n";
    const image img = render(src);
    const size_t drawn = non_background(img);
    EXPECT_GT(drawn, 60u) << kind;
    EXPECT_TRUE(seen.insert(std::string(img.rgba.begin(), img.rgba.end())).second) << kind << " draws like another kind";
  }
}

TEST(uml_network, WirelessAndVpnLinksAreDashed) {
  const std::string base =
      "image: {width: 200, height: 400}\nroot:\n  type: NetworkDiagram\n  children:\n"
      "    - {type: Device, id: a, kind: Switch}\n    - {type: Device, id: b, kind: Switch}\n"
      "    - {type: Link, from: a, to: b, min_length: 3, color: \"#FF0000\"}\n";
  const std::string wireless = with(base, "color:", "kind: Wireless, color:");
  const std::string vpn = with(base, "color:", "kind: Vpn, color:");
  const size_t solid = reddish(render(base));
  EXPECT_GT(solid, reddish(render(wireless)) + 10u);
  EXPECT_GT(solid, reddish(render(vpn)) + 10u);
}

} // namespace
} // namespace bdg::nymph
