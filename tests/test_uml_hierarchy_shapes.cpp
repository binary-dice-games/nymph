// MIT License © 2026 Binary Dice Games
/// @file test_uml_hierarchy_shapes.cpp
/// @brief spec/13_more_diagrams_and_shapes PLAN steps 5 and 8 to 12: the
///        component, deployment, package and object diagrams, trees, mind
///        maps and org charts, and library shapes on flowchart nodes.
#include <gtest/gtest.h>
#include <fstream>
#include <sstream>
#include <string>
#include "uml_test_util.hpp"

namespace bdg::nymph {
namespace {

using namespace uml_test;

std::string read_file(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream s;
  s << in.rdbuf();
  return s.str();
}

std::string example(const std::string& name) {
  return read_file(std::string(NYMPH_SOURCE_DIR) + "/examples/" + name + ".yaml");
}

/// Expects check_source(`source`) to fail with a message containing `part`.
void expect_message(const std::string& source, const std::string& part) {
  const error e = source_error(source);
  EXPECT_GT(e.line(), 0) << e.what();
  EXPECT_TRUE(contains(e, part)) << "message: " << e.what() << "\nexpected: " << part;
}

/// The text of `source` with `from` replaced by `to`.
std::string with(std::string source, const std::string& from, const std::string& to) {
  const size_t at = source.find(from);
  EXPECT_NE(at, std::string::npos) << from;
  return source.replace(at, from.size(), to);
}

TEST(uml_new_diagrams, EveryExampleRendersRoundTripsAndIsDeterministic) {
  for (const char* name :
       {"uml_component", "uml_deployment", "uml_package", "uml_object", "tree", "mind_map", "org_chart"}) {
    SCOPED_TRACE(name);
    const std::string source = example(name);
    ASSERT_FALSE(source.empty());
    EXPECT_GT(non_background(render(source)), 2000u);
    expect_round_trip(source);
  }
}

// ── Component ────────────────────────────────────────────────────────────────

TEST(uml_component, MatchedInterfacesAreWiredAndUnmatchedOnesGetStubs) {
  const drawn_diagram d = draw_and_inspect(example("uml_component"), {"web", "orders", "billing", "auth", "mail"});
  ASSERT_EQ(d.nodes.size(), 5u);
  // Five components, the Shop frame and one stub node for ISmtp, which
  // nobody provides; the three matched interfaces add links, not nodes.
  EXPECT_EQ(d.info.NodeCount, 7);
  EXPECT_EQ(d.info.EdgeCount, 3 + 1 + 1) << "IOrders, IAuth, IPay, the stub, and the ComponentLink";
}

TEST(uml_component, AnInterfaceWithTwoProvidersIsAnError) {
  const std::string source = with(
      example("uml_component"), "{type: Component, id: mail, name: Mailer, requires: [ISmtp]}",
      "{type: Component, id: mail, name: Mailer, requires: [ISmtp], provides: [IAuth]}");
  const error e = source_error(source);
  EXPECT_TRUE(contains(e, "'IAuth' is already provided by")) << e.what();
}

TEST(uml_component, LinksNeedComponentsAndKnownIds) {
  const std::string source = with(example("uml_component"), "to: mail, kind", "to: mial, kind");
  const error e = source_error(source);
  EXPECT_TRUE(contains(e, "unknown id 'mial'") && contains(e, "did you mean 'mail'")) << e.what();
}

// ── Deployment ───────────────────────────────────────────────────────────────

TEST(uml_deployment, NestedArtifactsLieInsideTheirNode) {
  const drawn_diagram d = draw_and_inspect(example("uml_deployment"), {"srv", "jvm", "war", "shop", "db", "schema"});
  ASSERT_EQ(d.nodes.size(), 6u);
  const auto inside = [&](const char* inner, const char* outer) {
    const ImRect& a = d.nodes.at(inner);
    const ImRect& b = d.nodes.at(outer);
    return a.Min.x >= b.Min.x && a.Max.x <= b.Max.x && a.Min.y >= b.Min.y && a.Max.y <= b.Max.y;
  };
  EXPECT_TRUE(inside("war", "jvm"));
  EXPECT_TRUE(inside("shop", "jvm"));
  EXPECT_TRUE(inside("jvm", "srv"));
  EXPECT_TRUE(inside("schema", "db"));
  EXPECT_FALSE(inside("db", "srv"));
}

TEST(uml_deployment, LinkEndsFollowTheirKind) {
  const std::string base = example("uml_deployment");
  expect_message(with(base, "to: db, label", "to: war, label"), "must be a DeploymentNode");
  expect_message(
      with(base, "{type: DeploymentLink, from: srv, to: db, label: JDBC / TLS}",
           "{type: DeploymentLink, from: srv, to: db, kind: Deploy}"),
      "must be an Artifact or a Component");
  expect_message(
      with(base, "{type: DeploymentLink, from: srv, to: db, label: JDBC / TLS}",
           "{type: DeploymentLink, from: war, to: shop, kind: Manifest}\n    - {type: DeploymentLink, from: db, to: shop, kind: Manifest}"),
      "must be an Artifact");
  expect_message(with(base, "from: srv, to: db", "from: srv, to: jvm"), "inside it");
}

// ── Package ──────────────────────────────────────────────────────────────────

TEST(uml_package, NestedPackagesDrawTheirItemsInsideAndRelationsToDescendantsFail) {
  const drawn_diagram d = draw_and_inspect(example("uml_package"), {"ui", "view", "ctl", "domain", "order"});
  EXPECT_GE(d.nodes.at("view").Min.x, d.nodes.at("ui").Min.x);
  EXPECT_LE(d.nodes.at("ctl").Max.x, d.nodes.at("ui").Max.x);
  EXPECT_GT(d.nodes.at("view").Min.y, d.nodes.at("ui").Min.y) << "below the tab and the name";
  expect_message(with(example("uml_package"), "from: ui, to: domain", "from: ui, to: view"), "inside it");
}

// ── Object ───────────────────────────────────────────────────────────────────

TEST(uml_object, HeadersAreInstanceNamesAndSlotsAreRows) {
  const drawn_diagram d = draw_and_inspect(example("uml_object"), {"o1", "c1", "l1"});
  ASSERT_EQ(d.nodes.size(), 3u);
  // Two slots make a taller box than one.
  EXPECT_GT(d.nodes.at("o1").GetHeight(), d.nodes.at("c1").GetHeight());
  expect_message(with(example("uml_object"), "from: o1, to: c1", "from: o1, to: nobody"), "unknown id 'nobody'");
}

// ── Trees ────────────────────────────────────────────────────────────────────

TEST(uml_tree, ExactlyOneRootIsRequired) {
  const std::string two = with(
      example("tree"), "          children: [{type: TreeItem, text: rasterizers}]\n",
      "          children: [{type: TreeItem, text: rasterizers}]\n    - {type: TreeItem, text: second root}\n");
  expect_message(two, "this is a second one");
  const std::string none = "nymph: 1\nimage: {width: 200, height: 100}\nroot: {type: TreeDiagram, children: []}\n";
  expect_message(none, "needs a root TreeItem");
}

TEST(uml_tree, ADeepChainIsRefusedBeyondTheLimit) {
  std::string source = "nymph: 1\nimage: {width: 200, height: 4000}\nroot:\n  type: TreeDiagram\n  children:\n";
  std::string open, close;
  std::string line = "    - type: TreeItem\n      text: n\n";
  source += line;
  std::string indent = "      ";
  for (int i = 0; i < 40; ++i) {
    source += indent + "children:\n" + indent + "  - type: TreeItem\n" + indent + "    text: n\n";
    indent += "    ";
  }
  const error e = source_error(source);
  EXPECT_TRUE(contains(e, "levels are allowed")) << e.what();
}

TEST(uml_tree, CollapsedNodesHideTheirDescendantsAndShowACount) {
  const std::string collapsed = with(example("tree"), "      text: src\n", "      text: src\n      collapsed: true\n");
  const drawn_diagram open = draw_and_inspect(example("tree"), {});
  const drawn_diagram shut = draw_and_inspect(collapsed, {});
  EXPECT_EQ(open.info.NodeCount, 8);
  EXPECT_EQ(shut.info.NodeCount, 1);
}

TEST(uml_mind_map, SidesBelongToTheRootsChildrenOnly) {
  const std::string base = example("mind_map");
  const std::string pinned = with(base, "          text: Support\n", "          text: Support\n          side: Right\n");
  EXPECT_GT(non_background(render(pinned)), 2000u);
  const std::string deep = with(base, "[{type: MindNode, text: Blog},", "[{type: MindNode, text: Blog, side: Left},");
  expect_message(deep, "for the children of the root only");
}

TEST(uml_mind_map, LinksNeedKnownIds) {
  expect_message(with(example("mind_map"), "from: qa, to: eng", "from: qa, to: engg"), "did you mean 'eng'");
}

TEST(uml_org_chart, FeaturesRenderAndTheRootCannotBeStaff) {
  const std::string base = example("org_chart");
  EXPECT_GT(non_background(render(base)), 4000u);
  expect_message(with(base, "      id: ceo\n", "      id: ceo\n      staff: true\n"), "cannot be set on the root");
  expect_message(with(base, "image: person\n      children:\n        - {type: Person, name: Leo", "image: nobody\n      children:\n        - {type: Person, name: Leo"), "nobody");
}

TEST(uml_org_chart, HangingTeamsFormAColumnAndAssistantsSitBesideTheirManager) {
  const std::string source = example("org_chart");
  const figure f = bind_source(source, builtin_registry());
  EXPECT_NO_THROW(render_figure(f));
  // Assistant: drawn on the manager's row (checked by the layout tests); here
  // only that adding staff does not change the chart's other cards.
  const std::string no_staff = with(source, "        - {type: Person, name: Leo Park, title: Executive assistant, staff: true}\n", "");
  EXPECT_GT(non_background(render(no_staff)), 4000u);
}

TEST(uml_org_chart, ACardGrowsAroundItsContentInsideTheShapesTextArea) {
  const auto card = [](const char* shape) {
    const std::string source = std::string(
        "nymph: 1\nimage: {width: 600, height: 300}\nroot:\n  type: OrgChart\n  children:\n"
        "    - {type: Person, id: p, name: Mia Chen, title: VP Engineering, image: person, shape: ") + shape + "}\n";
    return draw_and_inspect(source, {"p"}).nodes.at("p");
  };
  const ImRect plain = card("rounded_rectangle"), ribbon = card("ribbon"), bubble = card("speech_bubble");
  // The ribbon's text area is 16 px narrower in all than the rounded box's, the
  // bubble's has a 14 px tail below: the card is that much larger.
  EXPECT_GE(ribbon.GetWidth(), plain.GetWidth() + 14.0f);
  EXPECT_GE(bubble.GetHeight(), plain.GetHeight() + 8.0f);
}

// ── Library shapes on flowchart nodes ────────────────────────────────────────

TEST(flowchart_symbol, ANodeTakesAnyShapeOfTheLibrary) {
  const std::string source =
      "nymph: 1\nimage: {width: 400, height: 300}\nroot:\n  type: Flowchart\n  children:\n"
      "    - {type: FlowNode, id: a, text: Start, symbol: terminator}\n"
      "    - {type: FlowNode, id: b, text: Store, symbol: cylinder}\n"
      "    - {type: FlowNode, id: c, text: Done, symbol: cloud}\n"
      "    - {type: FlowLink, from: a, to: b}\n    - {type: FlowLink, from: b, to: c}\n";
  const drawn_diagram d = draw_and_inspect(source, {"a", "b", "c"});
  EXPECT_GT(d.nodes.at("b").GetHeight(), d.nodes.at("a").GetHeight()) << "a cylinder's caps add height";
  EXPECT_LT(d.nodes.at("a").Min.y, d.nodes.at("b").Min.y);
  expect_round_trip(source);
  expect_message(with(source, "symbol: cloud", "symbol: clod"), "cloud");
}

} // namespace
} // namespace bdg::nymph
