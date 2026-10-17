// MIT License © 2026 Binary Dice Games
/// @file test_uml_er.cpp
/// @brief UML plan step 14: `ERDiagram`, `Entity` and `Relationship`, and
///        ImUml's crow's foot markers.
#include <gtest/gtest.h>
#include <string>
#include "uml_test_util.hpp"

namespace bdg::nymph {
namespace {

using namespace uml_test;

const char* const kExample =
    "image: {width: 640, height: 480}\n" // 1
    "root:\n" // 2
    "  type: ERDiagram\n" // 3
    "  children:\n" // 4
    "    - {type: Entity, id: customer, attributes: [\"int id PK\", \"string email UK \\\"login\\\"\"]}\n" // 5
    "    - {type: Entity, id: order, attributes: [\"int id PK\", \"int customer_id FK\"]}\n" // 6
    "    - {type: Entity, id: tag}\n" // 7
    "    - {type: Relationship, from: customer, to: order, to_cardinality: ZeroOrMore, label: places}\n" // 8
    "    - {type: Relationship, from: order, to: tag, from_cardinality: ZeroOrMore, identifying: false}\n"; // 9

/// Pixels that are mostly red (anti-aliased line pixels included).
size_t reddish(const image& img, int x0 = 0, int y0 = 0, int x1 = -1, int y1 = -1) {
  x1 = x1 < 0 ? img.width : x1;
  y1 = y1 < 0 ? img.height : y1;
  size_t n = 0;
  for (int y = y0; y < y1; ++y) {
    for (int x = x0; x < x1; ++x) {
      const uint8_t* p = img.at(x, y);
      n += p[0] > 120 && p[0] > p[1] + 60 && p[0] > p[2] + 60 ? 1 : 0;
    }
  }
  return n;
}

std::string with(const std::string& from, const std::string& to) {
  std::string s = kExample;
  const size_t at = s.find(from);
  EXPECT_NE(at, std::string::npos) << from;
  return s.replace(at, from.size(), to);
}

TEST(uml_er, ExampleRendersRoundTripsAndIsDeterministic) {
  EXPECT_GT(non_background(render(kExample)), 2000u);
  expect_round_trip(kExample);
}

TEST(uml_er, EntitiesAreTablesLaidOutByRelationship) {
  const drawn_diagram d = draw_and_inspect(kExample, {"customer", "order", "tag"});
  ASSERT_EQ(d.nodes.size(), 3u);
  EXPECT_LT(d.nodes.at("customer").Max.y, d.nodes.at("order").Min.y);
  // A row per attribute: two-attribute tables are taller than the empty one.
  EXPECT_GT(d.nodes.at("order").GetHeight(), d.nodes.at("tag").GetHeight() + 20.0f);
}

TEST(uml_er, AttributeSyntaxIsChecked) {
  expect_error(with("\"int customer_id FK\"", "\"customer_id\""), 6, 0, "needs a type and a name");
  expect_error(with("\"int customer_id FK\"", "\"int customer_id FX\""), 6, 0,
               "'FX' after the name; only the keys PK, FK and UK");
  expect_error(with("\"int customer_id FK\"", "\"int customer_id \\\"open\""), 6, 0, "unterminated comment");
  EXPECT_NO_THROW(check_source(with("\"int customer_id FK\"", "\"int customer_id PK,FK\"")));
}

TEST(uml_er, RelationshipRules) {
  expect_error(with("from: order, to: tag,", "from: order, to: tags,"), 9, 0, "unknown id 'tags' (did you mean 'tag'?)");
  expect_error(with("to_cardinality: ZeroOrMore", "to_cardinality: Many"), 8, 0, "unknown value 'Many'");
  expect_error(
      "root:\n  type: ERDiagram\n  children:\n    - {type: Class, id: A}\n", 4, 0, "cannot be placed in ERDiagram");
}

TEST(uml_er, NonIdentifyingRelationshipsAreDashed) {
  // Coloured lines: the solid one has more line pixels than the dashed one.
  const std::string src =
      "image: {width: 300, height: 400}\nroot:\n  type: ERDiagram\n  children:\n"
      "    - {type: Entity, id: a}\n    - {type: Entity, id: b}\n"
      "    - {type: Relationship, from: a, to: b, min_length: 3, color: \"#FF0000\"}\n";
  std::string dashed = src;
  dashed.replace(dashed.find("color:"), 6, "identifying: false, color:");
  EXPECT_GT(reddish(render(src)), reddish(render(dashed)) + 10u);
}

// ── Crow's foot markers ──────────────────────────────────────────────────────

ImU32 pixel(const image& img, int x, int y) {
  const uint8_t* p = img.at(x, y);
  return IM_COL32(p[0], p[1], p[2], p[3]);
}

TEST(imuml_render, CardinalityMarkers) {
  const ImU32 line = IM_COL32(200, 0, 0, 255), bg = IM_COL32(0, 0, 200, 255);
  image_options o;
  o.width = 300;
  o.height = 120;
  o.padding = 0;
  // Tips at y = 100, lines arriving from above (dir points down), size 20.
  const image img = render_frames(o, [&](draw_context&) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImUmlCardinality kinds[4] = {
        ImUmlCardinality_ExactlyOne, ImUmlCardinality_ZeroOrOne, ImUmlCardinality_ZeroOrMore,
        ImUmlCardinality_OneOrMore};
    for (int i = 0; i < 4; ++i) {
      ImUml::RenderCardinality(dl, kinds[i], ImVec2(40.0f + 70.0f * i, 100.0f), ImVec2(0, 1), 20.0f, line, bg, 1.0f);
    }
  });
  // "Zero" ends have a circle 29 px back, filled with the background.
  EXPECT_EQ(pixel(img, 110, 71), bg);
  EXPECT_EQ(pixel(img, 180, 71), bg);
  EXPECT_NE(pixel(img, 40, 71), bg);
  // "Many" ends spread to the tip: line pixels near the tip, off the axis.
  EXPECT_GE(reddish(img, 240, 96, 247, 100), 1u);
  EXPECT_GE(reddish(img, 254, 96, 261, 100), 1u);
  EXPECT_EQ(reddish(img, 30, 96, 37, 100), 0u); // "one" ends do not spread
}

} // namespace
} // namespace bdg::nymph
