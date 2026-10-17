// MIT License © 2026 Binary Dice Games
/// @file test_uml_class.cpp
/// @brief UML plan step 6: `ClassDiagram`, `Class`, `ClassRelation` and `Note`
///        through the whole pipeline, and the shared diagram behaviour
///        (ids, references, fit, layout hints, limits).
#include <gtest/gtest.h>
#include <string>
#include "uml_test_util.hpp"

namespace bdg::nymph {
namespace {

using namespace uml_test;

const char* const kExample =
    "image: {width: 720, height: 560}\n" // 1
    "root:\n" // 2
    "  type: ClassDiagram\n" // 3
    "  title: Animals\n" // 4
    "  children:\n" // 5
    "    - type: Class\n" // 6
    "      id: Animal\n" // 7
    "      kind: Abstract\n" // 8
    "      attributes: [\"+name: string\", \"+age: int\"]\n" // 9
    "      methods: [\"+makeSound()*\", \"+isMammal() bool\"]\n" // 10
    "    - {type: Class, id: Duck, attributes: [\"+beakColor: string\"], methods: [\"+swim()\"]}\n" // 11
    "    - {type: Class, id: Pond}\n" // 12
    "    - {type: ClassRelation, from: Duck, to: Animal, kind: Inheritance}\n" // 13
    "    - {type: ClassRelation, from: Pond, to: Duck, kind: Aggregation, from_label: \"1\", to_label: \"*\", "
    "label: has}\n" // 14
    "    - {type: Note, text: \"Ducks can fly\", attach: [Duck]}\n"; // 15

std::string with(const std::string& from, const std::string& to) {
  std::string s = kExample;
  const size_t at = s.find(from);
  EXPECT_NE(at, std::string::npos) << from;
  return s.replace(at, from.size(), to);
}

TEST(uml_class, ExampleRendersRoundTripsAndIsDeterministic) {
  const image img = render(kExample);
  EXPECT_GT(non_background(img), 2000u);
  expect_round_trip(kExample);
}

TEST(uml_class, InheritancePlacesTheParentAbove) {
  const drawn_diagram d = draw_and_inspect(kExample, {"Animal", "Duck", "Pond"});
  ASSERT_EQ(d.nodes.size(), 3u);
  EXPECT_LT(d.nodes.at("Animal").Max.y, d.nodes.at("Duck").Min.y);
  EXPECT_LT(d.nodes.at("Pond").Max.y, d.nodes.at("Duck").Min.y); // the whole above its part
  EXPECT_EQ(d.info.SkippedItems, 0);
}

TEST(uml_class, IdAndReferenceErrors) {
  expect_error(with("id: Duck,", "id: Animal,"), 11, 0, "id 'Animal' is already used by the Class at line 6");
  expect_error(with("from: Duck, to: Animal", "from: Duck, to: Aminal"), 13, 0, "unknown id 'Aminal' (did you mean 'Animal'?)");
  expect_error(with("from: Duck, to: Animal", "from: Duck, to: \"\""), 13, 0, "is empty");
  expect_error(with("attach: [Duck]", "attach: [Dock]"), 15, 0, "unknown id 'Dock'");
  expect_error(with("{type: Class, id: Pond}", "{type: Class, name: Pond}"), 12, 0, "Class.id is empty");
}

TEST(uml_class, PlacementAndKindErrors) {
  expect_error(
      "root:\n  type: VerticalLayout\n  children:\n    - {type: Class, id: A}\n", 4, 14, "it goes inside: ClassDiagram");
  expect_error(
      "root:\n  type: ClassDiagram\n  children:\n    - {type: Class, id: A}\n    - {type: Note, text: n, id: x}\n", 5, 0,
      "Note has no field 'id'");
  expect_error(
      "root:\n  type: ClassDiagram\n  children:\n    - {type: Class, id: A}\n    - {type: Note, text: n, over: [A]}\n", 5,
      0, "over is for sequence diagrams");
  expect_error(with("kind: Abstract", "kind: Abstrakt"), 8, 13, "unknown value 'Abstrakt'");
}

TEST(uml_class, TooBigForItsSpaceAtMinZoomIsALayoutError) {
  std::string big = "image: {width: 200, height: 120}\nroot:\n  type: ClassDiagram\n  children:\n";
  for (int i = 0; i < 12; ++i) {
    big += "    - {type: Class, id: C" + std::to_string(i) + ", attributes: [\"+a_long_attribute_name: string\"]}\n";
  }
  const error e = source_error(big);
  EXPECT_EQ(e.line(), 3);
  EXPECT_TRUE(contains(e, "ClassDiagram needs")) << e.what();
  EXPECT_TRUE(contains(e, "lower min_zoom")) << e.what();
  // Allowed to shrink further, it renders.
  const std::string small_ok = big.replace(big.find("type: ClassDiagram"), 18, "type: ClassDiagram\n  min_zoom: 0.05");
  EXPECT_NO_THROW(check_source(small_ok));
}

TEST(uml_class, NaturalSizeInALayout) {
  const std::string src =
      "image: {width: 600, height: 500}\nroot:\n  type: VerticalLayout\n  children:\n"
      "    - {type: Label, text: caption}\n"
      "    - type: ClassDiagram\n      children:\n        - {type: Class, id: A}\n        - {type: Class, id: B}\n"
      "        - {type: ClassRelation, from: A, to: B}\n";
  const figure f = bind_source(src, builtin_registry());
  const auto rects = layout_figure(f);
  const node& diagram = f.root.children[1];
  const rect r = rects.at(&diagram);
  EXPECT_GT(r.height(), 60.0f);
  EXPECT_LT(r.height(), 300.0f); // natural, not the remaining space
}

TEST(uml_class, FillColourChangesOnlyItsClass) {
  const std::string plain = "image: {width: 400, height: 200}\nroot:\n  type: ClassDiagram\n  children:\n"
                            "    - {type: Class, id: A}\n    - {type: Class, id: B}\n";
  std::string red = plain;
  red.replace(red.find("id: A}"), 6, "id: A, fill_color: \"#FF0000\"}");
  const image a = render(plain);
  const image b = render(red);
  EXPECT_EQ(count_color(a, {255, 0, 0, 255}), 0u);
  EXPECT_GT(count_color(b, {255, 0, 0, 255}), 200u);
}

TEST(uml_class, LayoutHintsReachTheLayout) {
  const std::string base =
      "root:\n  type: ClassDiagram\n  children:\n    - {type: Class, id: A}\n    - {type: Class, id: B}\n"
      "    - {type: Class, id: C}\n    - {type: ClassRelation, from: A, to: B}\n";
  std::string same_rank = base;
  same_rank.replace(same_rank.find("id: C}"), 6, "id: C, rank: r}");
  same_rank.replace(same_rank.find("id: B}"), 6, "id: B, rank: r}");
  same_rank.replace(same_rank.find("id: A}"), 6, "id: A, rank: r}");
  drawn_diagram d = draw_and_inspect(same_rank, {"A", "B", "C"});
  EXPECT_EQ(d.nodes.at("A").Min.y, d.nodes.at("B").Min.y);

  std::string longer = base;
  longer.replace(longer.find("to: B}"), 6, "to: B, min_length: 2}");
  const drawn_diagram d1 = draw_and_inspect(base, {"A", "B"});
  const drawn_diagram d2 = draw_and_inspect(longer, {"A", "B"});
  EXPECT_GT(d2.nodes.at("B").Min.y - d2.nodes.at("A").Max.y, 1.5f * (d1.nodes.at("B").Min.y - d1.nodes.at("A").Max.y));

  std::string bad = base;
  bad.replace(bad.find("to: B}"), 6, "to: B, weight: 101}");
  expect_error(bad, 7, 0, "ClassRelation.weight");
}

TEST(uml_class, NodeLimit) {
  std::string src = "root:\n  type: ClassDiagram\n  children:\n";
  for (int i = 0; i <= 500; ++i) {
    src += "    - {type: Class, id: C" + std::to_string(i) + "}\n";
  }
  expect_error(src, 2, 0, "has 501 nodes; at most 500");
}

} // namespace
} // namespace bdg::nymph
