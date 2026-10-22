// MIT License © 2026 Binary Dice Games
/// @file test_editor_layout.cpp
/// @brief The regions of the editor window: strip width clamping, the
///        divider's grab zone and the figure box
///        (spec/14_editor_workspace/PLAN.md steps 1 and 7).
#include <gtest/gtest.h>
#include "edit/workspace_layout.hpp"

using namespace bdg::nymph;

// ── Strip width ──────────────────────────────────────────────────────────────

TEST(editor_layout, StripKeepsTheWidthAskedForWhenItFits) {
  EXPECT_FLOAT_EQ(clamp_strip_width(1000.0f, 320.0f), 320.0f);
  EXPECT_FLOAT_EQ(clamp_strip_width(1000.0f, 500.0f), 500.0f);
}

TEST(editor_layout, StripIsNeverNarrowerThanItsMinimum) {
  EXPECT_FLOAT_EQ(clamp_strip_width(1000.0f, 10.0f), kMinStripWidth);
  EXPECT_FLOAT_EQ(clamp_strip_width(1000.0f, -50.0f), kMinStripWidth);
}

TEST(editor_layout, StripLeavesTheFigureItsMinimum) {
  EXPECT_FLOAT_EQ(clamp_strip_width(1000.0f, 900.0f), 1000.0f - kMinFigureWidth);
}

TEST(editor_layout, NarrowWindowKeepsTheFigureMinimumAndNeverGoesNegative) {
  // Too narrow for both minimums: the figure keeps 240, the strip what is left.
  EXPECT_FLOAT_EQ(clamp_strip_width(400.0f, 320.0f), 160.0f);
  EXPECT_FLOAT_EQ(clamp_strip_width(200.0f, 320.0f), 0.0f);
  EXPECT_FLOAT_EQ(clamp_strip_width(0.0f, 320.0f), 0.0f);
}

// ── Divider ──────────────────────────────────────────────────────────────────

TEST(editor_layout, DividerGrabZoneIsCentredOnTheStripEdge) {
  EXPECT_TRUE(divider_hit(600.0f, 600.0f));
  EXPECT_TRUE(divider_hit(600.0f - kDividerGrab, 600.0f));
  EXPECT_TRUE(divider_hit(600.0f + kDividerGrab, 600.0f));
  EXPECT_FALSE(divider_hit(600.0f - kDividerGrab - 0.5f, 600.0f));
  EXPECT_FALSE(divider_hit(600.0f + kDividerGrab + 0.5f, 600.0f));
}

// ── Regions ──────────────────────────────────────────────────────────────────

TEST(editor_layout, FigureShrinksAndGrowsWithTheStrip) {
  const workspace_layout narrow = layout_workspace(1000.0f, 700.0f, 300.0f, 0.0f);
  const workspace_layout wide = layout_workspace(1000.0f, 700.0f, 500.0f, 0.0f);
  EXPECT_FLOAT_EQ(narrow.left.width, 700.0f);
  EXPECT_FLOAT_EQ(wide.left.width, 500.0f);
  EXPECT_FLOAT_EQ(wide.strip.x, 500.0f);
  EXPECT_FLOAT_EQ(wide.strip.width, 500.0f);
  EXPECT_FLOAT_EQ(wide.left.height, 700.0f);
  EXPECT_TRUE(wide.tabs.empty());
}

TEST(editor_layout, TabBarSitsOverTheLeftSide) {
  const workspace_layout l = layout_workspace(1000.0f, 700.0f, 320.0f, 30.0f);
  EXPECT_FLOAT_EQ(l.tabs.y, 0.0f);
  EXPECT_FLOAT_EQ(l.tabs.width, 680.0f);
  EXPECT_FLOAT_EQ(l.left.y, 30.0f);
  EXPECT_FLOAT_EQ(l.left.height, 670.0f);
  EXPECT_TRUE(l.left.contains(10.0f, 40.0f));
  EXPECT_FALSE(l.left.contains(10.0f, 20.0f)); // on the tab bar
  EXPECT_FALSE(l.left.contains(690.0f, 40.0f)); // in the strip
}
