// MIT License © 2026 Binary Dice Games
/// @file test_flowchart.cpp
/// @brief `Flowchart`, `FlowNode`, `FlowGroup` and `FlowLink`
///        (spec/03_uml_diagrams/DESIGN.md section 3, "Flowchart").
#include <gtest/gtest.h>
#include <string>
#include "uml_test_util.hpp"

namespace bdg::nymph {
namespace {

using namespace uml_test;

std::string with(const std::string& src, const std::string& from, const std::string& to) {
  std::string s = src;
  const size_t at = s.find(from);
  EXPECT_NE(at, std::string::npos) << from;
  return s.replace(at, from.size(), to);
}

const char* const kFlow =
    "image: {width: 700, height: 640}\n" // 1
    "root:\n" // 2
    "  type: Flowchart\n" // 3
    "  children:\n" // 4
    "    - {type: FlowNode, id: start, text: Start, shape: Stadium}\n" // 5
    "    - {type: FlowNode, id: check, text: Valid?, shape: Diamond}\n" // 6
    "    - type: FlowGroup\n" // 7
    "      id: svc\n" // 8
    "      title: Service\n" // 9
    "      children:\n" // 10
    "        - {type: FlowNode, id: save, text: Save, shape: Subroutine}\n" // 11
    "        - {type: FlowNode, id: db, text: Database, shape: Cylinder}\n" // 12
    "        - {type: FlowLink, from: save, to: db, line: Dotted}\n" // 13
    "    - {type: FlowNode, id: err, text: \"Show\\nerror\", shape: Parallelogram}\n" // 14
    "    - {type: FlowNode, id: end, text: End, shape: Circle}\n" // 15
    "    - {type: FlowLink, from: start, to: check, line: Thick}\n" // 16
    "    - {type: FlowLink, from: check, to: svc, label: \"yes\"}\n" // 17
    "    - {type: FlowLink, from: check, to: err, label: \"no\"}\n" // 18
    "    - {type: FlowLink, from: svc, to: end, arrow: Both}\n" // 19
    "    - {type: FlowLink, from: err, to: end, arrow: None}\n" // 20
    "    - {type: Note, text: retried twice, attach: [save]}\n"; // 21

TEST(flowchart, ExampleRendersRoundTripsAndIsDeterministic) {
  EXPECT_GT(non_background(render(kFlow)), 3000u);
  expect_round_trip(kFlow);
}

TEST(flowchart, NodesFollowTheLinksAndGroupsHoldTheirNodes) {
  const drawn_diagram d = draw_and_inspect(kFlow, {"start", "check", "svc", "save", "db", "err", "end"});
  ASSERT_EQ(d.nodes.size(), 7u);
  EXPECT_EQ(d.info.SkippedItems, 0);
  // Top down: start above the decision, the decision above what follows it.
  EXPECT_LT(d.nodes.at("start").Max.y, d.nodes.at("check").Min.y);
  EXPECT_LT(d.nodes.at("check").Max.y, d.nodes.at("svc").Min.y);
  EXPECT_LT(d.nodes.at("check").Max.y, d.nodes.at("err").Min.y);
  // The group's box holds its nodes.
  EXPECT_TRUE(d.nodes.at("svc").Contains(d.nodes.at("save")));
  EXPECT_TRUE(d.nodes.at("svc").Contains(d.nodes.at("db")));
  EXPECT_FALSE(d.nodes.at("svc").Overlaps(d.nodes.at("err")));
  // A two-line text makes a taller node.
  EXPECT_GT(d.nodes.at("err").GetHeight(), d.nodes.at("start").GetHeight() * 1.5f);
  // Circles are round; a decision diamond is twice as wide as tall.
  EXPECT_EQ(d.nodes.at("end").GetWidth(), d.nodes.at("end").GetHeight());
  EXPECT_NEAR(d.nodes.at("check").GetWidth(), d.nodes.at("check").GetHeight() * 2.0f, 1.0f);
}

TEST(flowchart, DirectionTurnsTheChart) {
  const drawn_diagram d = draw_and_inspect(
      with(kFlow, "type: Flowchart\n", "type: Flowchart\n  direction: LeftRight\n"), {"start", "check"});
  EXPECT_LT(d.nodes.at("start").Max.x, d.nodes.at("check").Min.x);
}

TEST(flowchart, EveryShapeAndLinkStyleDraws) {
  const std::string base =
      "image: {width: 300, height: 260}\nroot:\n  type: Flowchart\n  children:\n"
      "    - {type: FlowNode, id: a, text: Alpha, shape: SHAPE}\n"
      "    - {type: FlowNode, id: b, text: Beta}\n"
      "    - {type: FlowLink, from: a, to: b, LINK}\n";
  const image box = render(with(with(base, "SHAPE", "Box"), "LINK", "line: Solid"));
  for (const char* shape :
       {"Rounded", "Stadium", "Diamond", "Circle", "Parallelogram", "Hexagon", "Cylinder", "Subroutine"}) {
    SCOPED_TRACE(shape);
    EXPECT_FALSE(render(with(with(base, "SHAPE", shape), "LINK", "line: Solid")).rgba == box.rgba);
  }
  for (const char* link : {"line: Dotted", "line: Thick", "arrow: None", "arrow: Both", "label: go"}) {
    SCOPED_TRACE(link);
    EXPECT_FALSE(render(with(with(base, "SHAPE", "Box"), "LINK", link)).rgba == box.rgba);
  }
}

TEST(flowchart, LinkRules) {
  expect_error(
      with(kFlow, "from: check, to: err", "from: check, to: eror"), 18, 0, "unknown id 'eror' (did you mean 'err'?)");
  expect_error(
      with(kFlow, "from: svc, to: end", "from: svc, to: db"), 19, 0, "joins a FlowGroup with a node inside it");
  expect_error(with(kFlow, "id: err,", "id: start,"), 14, 0, "id 'start' is already used");
  // Links between nodes of different groups, and self-links, are fine.
  EXPECT_NO_THROW(check_source(with(kFlow, "from: err, to: end", "from: err, to: db")));
  EXPECT_NO_THROW(check_source(with(kFlow, "from: err, to: end", "from: err, to: err")));
}

TEST(flowchart, OnlyFlowchartItemsGoInside) {
  expect_error(with(kFlow, "type: FlowNode, id: end", "type: Action, id: end"), 15, 0, "cannot be placed in");
}

} // namespace
} // namespace bdg::nymph
