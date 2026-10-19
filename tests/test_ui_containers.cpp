// MIT License © 2026 Binary Dice Games
/// @file test_ui_containers.cpp
/// @brief spec/09_ui_mockups PLAN step 6: containers nest widgets, figure
///        elements draw in a window's flow, repeated labels stay apart.
#include <gtest/gtest.h>
#include <string>
#include "nymph.hpp"
#include "ui_test_util.hpp"

namespace bdg::nymph {
namespace {

using test::render_ui;
using test::ui_render;

std::string in_window(const std::string& children, int width = 600, int height = 440) {
  return "image: {width: 640, height: 480, padding: 0}\n"
         "data: {x: [1, 2, 3], y: [3, 1, 2]}\n"
         "root:\n"
         "  type: Screen\n"
         "  children:\n"
         "    - type: Window\n"
         "      title: W\n"
         "      x: 10\n"
         "      y: 10\n"
         "      width: " +
      std::to_string(width) + "\n      height: " + std::to_string(height) +
      "\n"
      "      children:\n" +
      children;
}

/// The element target of the element at `path` (edit mode), if drawn.
std::optional<rect> target(const ui_render& r, const std::string& path) {
  for (const edit_record& t : r.targets) {
    if (t.field.empty() && r.paths.at(t.element) == path) {
      return t.area;
    }
  }
  return std::nullopt;
}

TEST(ui_containers, AClosedTreeNodeDrawsNoChildren) {
  const std::string source = in_window(
      "        - type: TreeNode\n"
      "          label: Node\n"
      "          open: OPEN\n"
      "          children:\n"
      "            - {type: Checkbox, label: Inside}\n",
      0,
      0);
  std::string open = source, closed = source;
  open.replace(open.find("OPEN"), 4, "true");
  closed.replace(closed.find("OPEN"), 4, "false");
  const ui_render a = render_ui(open);
  const ui_render b = render_ui(closed);
  EXPECT_EQ(a.state.count("root/children/0/children/0/children/0"), 1u);
  EXPECT_EQ(b.state.count("root/children/0/children/0/children/0"), 0u);
  EXPECT_GT(a.state.at("root/children/0").size.y, b.state.at("root/children/0").size.y);
}

TEST(ui_containers, SameLabelledTreeNodesOpenIndependently) {
  const ui_render r = render_ui(in_window(
      "        - type: TreeNode\n"
      "          label: Node\n"
      "          open: true\n"
      "          children: [{type: Button, label: A}]\n"
      "        - type: TreeNode\n"
      "          label: Node\n"
      "          children: [{type: Button, label: B}]\n"));
  EXPECT_TRUE(r.state.at("root/children/0/children/0").flag);
  EXPECT_FALSE(r.state.at("root/children/0/children/1").flag);
}

TEST(ui_containers, TheSelectedTabShowsItsChildren) {
  // What the last frame drew: the targets (the state keeps entries of
  // earlier frames).
  const ui_render r = render_ui(
      in_window(
          "        - type: TabBar\n"
          "          children:\n"
          "            - {type: TabItem, label: One, children: [{type: Checkbox, label: a}]}\n"
          "            - {type: TabItem, label: Two, selected: true, children: [{type: Checkbox, label: b}]}\n"),
      true);
  EXPECT_FALSE(target(r, "root/children/0/children/0/children/0/children/0"));
  EXPECT_TRUE(target(r, "root/children/0/children/0/children/1/children/0"));
  // Without a selected tab, the first one shows.
  const ui_render first = render_ui(
      in_window(
          "        - type: TabBar\n"
          "          children:\n"
          "            - {type: TabItem, label: One, children: [{type: Checkbox, label: a}]}\n"
          "            - {type: TabItem, label: Two, children: [{type: Checkbox, label: b}]}\n"),
      true);
  EXPECT_TRUE(target(first, "root/children/0/children/0/children/0/children/0"));
  EXPECT_FALSE(target(first, "root/children/0/children/0/children/1/children/0"));
}

TEST(ui_containers, TabsGoOnlyInATabBar) {
  try {
    check_source(in_window("        - {type: TabItem, label: x}\n"));
    FAIL() << "no error";
  } catch (const error& e) {
    EXPECT_NE(std::string(e.what()).find("'TabItem' cannot be placed in Window; it goes inside: TabBar"), std::string::npos)
        << e.what();
  }
}

TEST(ui_containers, SameLinePutsWidgetsOnOneRow) {
  const ui_render r = render_ui(
      in_window(
          "        - {type: Button, label: OK}\n"
          "        - {type: SameLine}\n"
          "        - {type: Button, label: Cancel}\n"
          "        - {type: Button, label: Below}\n"),
      true);
  const rect ok = *target(r, "root/children/0/children/0");
  const rect cancel = *target(r, "root/children/0/children/2");
  const rect below = *target(r, "root/children/0/children/3");
  EXPECT_EQ(ok.min.y, cancel.min.y);
  EXPECT_GT(cancel.min.x, ok.max.x);
  EXPECT_GT(below.min.y, ok.max.y);
}

TEST(ui_containers, APlotFillsTheRestOfTheWindow) {
  const ui_render r = render_ui(
      in_window(
          "        - {type: Text, text: Above}\n"
          "        - type: Plot\n"
          "          height: -1\n"
          "          children: [{type: PlotLine, label: y, xs: $x, ys: $y}]\n"),
      true);
  const rect plot = *target(r, "root/children/0/children/1");
  // The window is 600 x 440 at (10, 10), padding 8.
  EXPECT_NEAR(plot.max.y, 10 + 440 - 8, 1);
  EXPECT_NEAR(plot.min.x, 18, 1);
  EXPECT_NEAR(plot.max.x, 10 + 600 - 8, 1);
}

TEST(ui_containers, ALayoutInATabLaysOutAsALayout) {
  const ui_render r = render_ui(
      in_window(
          "        - type: TabBar\n"
          "          children:\n"
          "            - type: TabItem\n"
          "              label: T\n"
          "              children:\n"
          "                - type: VerticalLayout\n"
          "                  spacing: 6\n"
          "                  children:\n"
          "                    - {type: Label, text: first}\n"
          "                    - {type: Label, text: second}\n"),
      true);
  const std::string layout = "root/children/0/children/0/children/0/children/0";
  const rect a = *target(r, layout + "/children/0");
  const rect b = *target(r, layout + "/children/1");
  EXPECT_EQ(b.min.y, a.max.y + 6);
  EXPECT_EQ(a.min.x, b.min.x);
}

TEST(ui_containers, DisabledGreysItsChildren) {
  const std::string on = in_window("        - type: Disabled\n          children: [{type: Button, label: B}]\n");
  const std::string off = in_window("        - type: Group\n          children: [{type: Button, label: B}]\n");
  EXPECT_NE(render_ui(on).img.rgba, render_ui(off).img.rgba);
}

TEST(ui_containers, RepeatedLabelsInEveryContainerKeepTheirIds) {
  // render_ui() runs the unique-ID check.
  const std::string leaf = "[{type: Button, label: Same}, {type: Checkbox, label: Same}]";
  EXPECT_NO_THROW(render_ui(in_window(
      "        - {type: TreeNode, label: Same, open: true, children: " + leaf +
      "}\n"
      "        - {type: TreeNode, label: Same, open: true, children: " +
      leaf +
      "}\n"
      "        - {type: CollapsingHeader, label: Same, open: true, children: " +
      leaf +
      "}\n"
      "        - {type: Group, children: " +
      leaf +
      "}\n"
      "        - {type: Group, children: " +
      leaf +
      "}\n"
      "        - {type: Child, label: Same, size: [0, 60], children: " +
      leaf +
      "}\n"
      "        - {type: Child, label: Same, size: [0, 60], children: " +
      leaf +
      "}\n"
      "        - type: TabBar\n"
      "          children:\n"
      "            - {type: TabItem, label: Same, selected: true, children: " +
      leaf +
      "}\n"
      "            - {type: TabItem, label: Same, children: " +
      leaf + "}\n")));
}

} // namespace
} // namespace bdg::nymph
