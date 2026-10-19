// MIT License © 2026 Binary Dice Games
/// @file test_source_edit.cpp
/// @brief spec/08_view_editor plan step 1: set_field() / remove_field()
///        patch one value of the YAML source and nothing else.
#include <gtest/gtest.h>
#include <string>
#include "core/error.hpp"
#include "document/yaml_doc.hpp"
#include "edit/source_edit.hpp"

namespace bdg::nymph {
namespace {

// ── Replacing a value ────────────────────────────────────────────────────────

TEST(source_edit, ReplacesAValueInAFlowElement) {
  const std::string src =
      "# a chart\n"
      "root: {type: Plot, title: Old, x_label: t} # keep\n"
      "data: {}\n";
  EXPECT_EQ(
      set_field(src, "root", "title", "New title"),
      "# a chart\n"
      "root: {type: Plot, title: New title, x_label: t} # keep\n"
      "data: {}\n");
}

TEST(source_edit, ReplacesAValueInABlockElementKeepingComments) {
  const std::string src =
      "root:\n"
      "  type: Plot # the plot\n"
      "  # the title\n"
      "  title: \"Old: title\"\n"
      "  x_label: time\n";
  EXPECT_EQ(
      set_field(src, "root", "title", "'x'"),
      "root:\n"
      "  type: Plot # the plot\n"
      "  # the title\n"
      "  title: 'x'\n"
      "  x_label: time\n");
}

TEST(source_edit, ReplacesAnEmptyValue) {
  EXPECT_EQ(set_field("root:\n  type: Plot\n  title:\n", "root", "title", "T"), "root:\n  type: Plot\n  title: T\n");
  EXPECT_EQ(set_field("root: {type: Plot, title: }\n", "root", "title", "T"), "root: {type: Plot, title: T }\n");
}

TEST(source_edit, ReplacesAFlowListAndAMultiLineValue) {
  const std::string src =
      "root:\n"
      "  type: Plot\n"
      "  children:\n"
      "    - {type: PlotLine, ys: [1, 2,\n"
      "        3]}\n"
      "    - {type: PlotLine, label: b}\n";
  EXPECT_EQ(
      set_field(src, "root/children/0", "ys", "[4, 5]"),
      "root:\n"
      "  type: Plot\n"
      "  children:\n"
      "    - {type: PlotLine, ys: [4, 5]}\n"
      "    - {type: PlotLine, label: b}\n");
}

TEST(source_edit, ReplacesABlockScalar) {
  const std::string src =
      "root:\n"
      "  type: Label\n"
      "  text: |\n"
      "    one\n"
      "    two\n"
      "\n"
      "data: {}\n";
  EXPECT_EQ(
      set_field(src, "root", "text", "\"x\""),
      "root:\n"
      "  type: Label\n"
      "  text: \"x\"\n"
      "\n"
      "data: {}\n");
}

// ── Adding a key ─────────────────────────────────────────────────────────────

TEST(source_edit, AddsAKeyToAFlowElement) {
  EXPECT_EQ(set_field("root: {type: Plot}\n", "root", "title", "T"), "root: {type: Plot, title: T}\n");
  EXPECT_EQ(
      set_field("root: {type: Plot, x_label: a}\n", "root", "title", "T"),
      "root: {type: Plot, x_label: a, title: T}\n");
}

TEST(source_edit, AddsAKeyToABlockElementAfterItsLastEntry) {
  const std::string src =
      "root:\n"
      "  type: Plot\n"
      "  children:\n"
      "    - {type: PlotLine, ys: [1]}\n"
      "data: {}\n";
  EXPECT_EQ(
      set_field(src, "root", "title", "T"),
      "root:\n"
      "  type: Plot\n"
      "  children:\n"
      "    - {type: PlotLine, ys: [1]}\n"
      "  title: T\n"
      "data: {}\n");
}

TEST(source_edit, AddsAKeyToAListItemAlignedWithItsKeys) {
  const std::string src =
      "root:\n"
      "  type: VBox\n"
      "  children:\n"
      "    - type: Label\n"
      "      text: a\n"
      "    - type: Label\n"
      "      text: b";
  EXPECT_EQ(
      set_field(src, "root/children/1", "text_color", "\"#ff0000\""),
      "root:\n"
      "  type: VBox\n"
      "  children:\n"
      "    - type: Label\n"
      "      text: a\n"
      "    - type: Label\n"
      "      text: b\n"
      "      text_color: \"#ff0000\"");
}

TEST(source_edit, KeepsWindowsLineBreaks) {
  EXPECT_EQ(
      set_field("root:\r\n  type: Plot\r\n", "root", "title", "T"),
      "root:\r\n  type: Plot\r\n  title: T\r\n");
}

TEST(source_edit, ColumnsCountCharactersNotBytes) {
  EXPECT_EQ(
      set_field("root: {type: Plot, title: \"é€\", x_label: a}\n", "root", "x_label", "b"),
      "root: {type: Plot, title: \"é€\", x_label: b}\n");
}

// ── Removing ─────────────────────────────────────────────────────────────────

TEST(source_edit, RemovesFieldsInFlowAndBlockStyle) {
  EXPECT_EQ(
      remove_field("root: {type: Plot, title: T, x_label: a}\n", "root", "title"),
      "root: {type: Plot, x_label: a}\n");
  EXPECT_EQ(
      remove_field("root: {type: Plot, title: T, x_label: a}\n", "root", "x_label"),
      "root: {type: Plot, title: T}\n");
  EXPECT_EQ(
      remove_field("root:\n  type: Plot\n  title: T\n  x_label: a\n", "root", "title"),
      "root:\n  type: Plot\n  x_label: a\n");
  EXPECT_EQ(remove_field("root:\n  type: Plot\n  title: T", "root", "title"), "root:\n  type: Plot");
  EXPECT_EQ(remove_field("root: {type: Plot}\n", "root", "title"), "root: {type: Plot}\n");
}

TEST(source_edit, TheFirstKeyOfAListItemCannotBeRemoved) {
  const std::string src = "root:\n  type: VBox\n  children:\n    - type: Label\n      text: a\n";
  EXPECT_THROW(remove_field(src, "root/children/0", "type"), error);
  EXPECT_EQ(remove_field(src, "root/children/0", "text"), "root:\n  type: VBox\n  children:\n    - type: Label\n");
}

// ── Paths and errors ─────────────────────────────────────────────────────────

TEST(source_edit, ResolvesNestedPaths) {
  const std::string src =
      "root:\n"
      "  type: VBox\n"
      "  children:\n"
      "    - type: HBox\n"
      "      children:\n"
      "        - {type: Label, text: a}\n"
      "        - {type: Label, text: b}\n";
  const yaml_node doc = parse_yaml(src);
  ASSERT_NE(find_element(doc, "root/children/0/children/1"), nullptr);
  EXPECT_EQ(find_element(doc, "root/children/0/children/1")->find("text")->text, "b");
  EXPECT_EQ(find_element(doc, "root/children/0/children/2"), nullptr);
  EXPECT_EQ(find_element(doc, "root/kids/0"), nullptr);
  EXPECT_EQ(find_element(doc, "data"), nullptr);
  EXPECT_NE(set_field(src, "root/children/0/children/1", "text", "c").find("text: c}"), std::string::npos);
}

TEST(source_edit, BadInputThrowsNymphErrors) {
  EXPECT_THROW(set_field("root: {type: Plot}\n", "root/children/0", "title", "T"), error);
  EXPECT_THROW(set_field("root: {type: Plot}\n", "nothing", "title", "T"), error);
  EXPECT_THROW(set_field("root: {type: Plot}\n", "root", "title", "a\nb"), error);
  EXPECT_THROW(set_field("root: [1\n", "root", "title", "T"), error);
  EXPECT_THROW(set_field("root: 3\n", "root", "title", "T"), error);
  EXPECT_THROW(set_field("- a\n", "root", "title", "T"), error);
}

TEST(source_edit, TheEditedSourceParsesToTheIntendedValue) {
  const std::string src = "root:\n  type: Plot\n  title: a\n";
  const std::string out = set_field(src, "root", "title", "\"x: #y\"");
  EXPECT_EQ(parse_yaml(out).find("root")->find("title")->text, "x: #y");
}

} // namespace
} // namespace bdg::nymph
