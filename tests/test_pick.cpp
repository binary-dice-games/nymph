// MIT License © 2026 Binary Dice Games
/// @file test_pick.cpp
/// @brief spec/04_web_viewer plan step 5: hovering a data point of a 2D
///        series reports it and shows a tooltip, in a live view only.
#include <gtest/gtest.h>
#include <imgui.h>
#include <optional>
#include <string>
#include "live_view_driver.hpp"
#include "render/soft_rasterizer.hpp"
#include "view/live_view.hpp"

namespace bdg::nymph {
namespace {

std::string plot_source(const std::string& children, const std::string& plot_fields = "") {
  return "image: {width: 400, height: 300}\n"
         "root:\n"
         "  type: Plot\n"
         "  title: P\n"
         "  x_min: 0\n"
         "  x_max: 4\n"
         "  y_min: 0\n"
         "  y_max: 10\n" +
      plot_fields + "  children:\n" + children;
}

const std::string kLine =
    plot_source("    - {type: PlotLine, label: temp, xs: [0, 1, 2, 3, 4], ys: [1, 4, 7, 2, 5]}\n");

std::string value_of(const view_pick& p, const std::string& name) {
  for (const auto& [k, v] : p.values) {
    if (k == name) {
      return v;
    }
  }
  return "<none>";
}

// ── XY series ────────────────────────────────────────────────────────────────

TEST(pick, ThePointerOnAPointPicksIt) {
  driven_view d(kLine);
  const ImVec2 p = driven_view::at(d.plot(), 2, 7);
  d.move(p.x + 3, p.y - 2); // within the pick radius
  const std::optional<view_pick>& h = d.v.hovered();
  ASSERT_TRUE(h.has_value());
  EXPECT_EQ(h->element, "root/children/0");
  EXPECT_EQ(h->type, "PlotLine");
  EXPECT_EQ(h->label, "temp");
  EXPECT_EQ(h->index, 2);
  EXPECT_EQ(value_of(*h, "x"), "2");
  EXPECT_EQ(value_of(*h, "y"), "7");
}

TEST(pick, NothingIsPickedAwayFromThePoints) {
  driven_view d(kLine);
  const ImVec2 p = driven_view::at(d.plot(), 2, 7);
  d.move(p.x, p.y + 20);
  EXPECT_FALSE(d.v.hovered().has_value());
  d.move(p.x, p.y);
  EXPECT_TRUE(d.v.hovered().has_value());
  d.send({pointer_event::kind::leave});
  EXPECT_FALSE(d.v.hovered().has_value());
}

TEST(pick, TheCloserOfTwoSeriesWins) {
  driven_view d(
      plot_source("    - {type: PlotScatter, label: a, xs: [2], ys: [5]}\n"
                  "    - {type: PlotScatter, label: b, xs: [2], ys: [5.4]}\n"));
  const ImVec2 a = driven_view::at(d.plot(), 2, 5);
  const ImVec2 b = driven_view::at(d.plot(), 2, 5.4);
  d.move(a.x, a.y + 1);
  ASSERT_TRUE(d.v.hovered().has_value());
  EXPECT_EQ(d.v.hovered()->label, "a");
  d.move(b.x, b.y - 1);
  ASSERT_TRUE(d.v.hovered().has_value());
  EXPECT_EQ(d.v.hovered()->label, "b");
}

TEST(pick, ATimeAxisValueIsAnIsoDate) {
  // 2026-03-02 and 2026-03-03, 00:00 UTC.
  driven_view d(
      "image: {width: 400, height: 300}\n"
      "root:\n"
      "  type: Plot\n"
      "  x_scale: Time\n"
      "  x_min: 1772323200\n"
      "  x_max: 1772582400\n"
      "  y_min: 0\n"
      "  y_max: 10\n"
      "  children:\n"
      "    - {type: PlotScatter, label: s, xs: [1772409600, 1772452800], ys: [5, 6]}\n");
  const ImVec2 p = driven_view::at(d.plot(), 1772409600, 5);
  d.move(p.x, p.y);
  ASSERT_TRUE(d.v.hovered().has_value());
  EXPECT_EQ(value_of(*d.v.hovered(), "x"), "2026-03-02");
  const ImVec2 q = driven_view::at(d.plot(), 1772452800, 6);
  d.move(q.x, q.y);
  ASSERT_TRUE(d.v.hovered().has_value());
  EXPECT_EQ(value_of(*d.v.hovered(), "x"), "2026-03-02 12:00:00");
}

TEST(pick, AnAxisFormatAndCategoryTicksFormatTheValues) {
  driven_view d(plot_source(
      "    - {type: PlotScatter, label: s, xs: [1, 2], ys: [2.5, 7.25]}\n",
      "  y_format: \"%.2f kg\"\n"
      "  x_ticks: [1, 2]\n"
      "  x_tick_labels: [north, south]\n"));
  const ImVec2 p = driven_view::at(d.plot(), 2, 7.25);
  d.move(p.x, p.y);
  ASSERT_TRUE(d.v.hovered().has_value());
  EXPECT_EQ(value_of(*d.v.hovered(), "x"), "south");
  EXPECT_EQ(value_of(*d.v.hovered(), "y"), "7.25 kg");
}

TEST(pick, ErrorBarsAddTheirErrorAndWinOverTheirLine) {
  driven_view d(
      plot_source("    - {type: PlotLine, label: m, xs: [1, 2, 3], ys: [2, 5, 3]}\n"
                  "    - {type: PlotErrorBars, label: m, xs: [1, 2, 3], ys: [2, 5, 3], err: [0.5, 1, 0.25]}\n"));
  const ImVec2 p = driven_view::at(d.plot(), 2, 5);
  d.move(p.x, p.y);
  ASSERT_TRUE(d.v.hovered().has_value());
  EXPECT_EQ(d.v.hovered()->type, "PlotErrorBars");
  EXPECT_EQ(value_of(*d.v.hovered(), "error"), "+/-1");
}

TEST(pick, ASecondYAxisSeriesIsPickedOnItsOwnAxis) {
  driven_view d(plot_source(
      "    - {type: PlotScatter, label: right, xs: [2], ys: [500], y_axis: Y2}\n", "  y2_min: 0\n  y2_max: 1000\n"));
  const plot_view v = d.plot();
  ASSERT_TRUE(v.has_y2);
  const float py = v.y + static_cast<float>((v.y2_max - 500) / (v.y2_max - v.y2_min)) * v.height;
  d.move(driven_view::at(v, 2, 0).x, py);
  ASSERT_TRUE(d.v.hovered().has_value());
  EXPECT_EQ(value_of(*d.v.hovered(), "y"), "500");
}

// ── Clicks and tooltips ──────────────────────────────────────────────────────

TEST(pick, AClickIsReportedOnce) {
  driven_view d(kLine);
  EXPECT_FALSE(d.v.take_click().has_value());
  const ImVec2 p = driven_view::at(d.plot(), 1, 4);
  d.click(p.x, p.y);
  const std::optional<view_pick> c = d.v.take_click();
  ASSERT_TRUE(c.has_value());
  EXPECT_EQ(c->index, 1);
  EXPECT_FALSE(d.v.take_click().has_value());
  // A drag that ends on a point is not a click.
  d.drag(p.x - 60, p.y, p.x, p.y);
  EXPECT_FALSE(d.v.take_click().has_value());
}

TEST(pick, TheTooltipIsAWindowOfItsOwnAndCanBeTurnedOff) {
  const auto lists_when_hovering = [](bool tooltips) {
    view_options o;
    o.tooltips = tooltips;
    driven_view d(kLine, 400, 300, o);
    const int idle = d.last->CmdListsCount;
    const ImVec2 p = driven_view::at(d.plot(), 2, 7);
    d.move(p.x, p.y);
    EXPECT_TRUE(d.v.hovered().has_value()); // reported either way
    return d.last->CmdListsCount - idle;
  };
  EXPECT_EQ(lists_when_hovering(true), 1);
  EXPECT_EQ(lists_when_hovering(false), 0);
}

TEST(pick, AHiddenSeriesIsNotPicked) {
  driven_view d(plot_source("    - {type: PlotScatter, label: hide me, xs: [2], ys: [5]}\n"));
  const plot_view v = d.plot();
  const ImVec2 p = driven_view::at(v, 2, 5);
  d.move(p.x, p.y);
  ASSERT_TRUE(d.v.hovered().has_value());
  // ImPlot's default legend, in the north-west corner of the plot area
  // (10 px padding, 5 px inner padding): click its only entry.
  d.click(v.x + 25, v.y + 23);
  d.move(p.x, p.y);
  EXPECT_FALSE(d.v.hovered().has_value());
}

TEST(pick, NothingIsLeftOnScreenAfterThePointerLeaves) {
  driven_view d(kLine);
  const image before = rasterize(*d.last, d.v.textures(), d.v.clear_color());
  const ImVec2 p = driven_view::at(d.plot(), 2, 7);
  d.move(p.x, p.y);
  const image hovering = rasterize(*d.last, d.v.textures(), d.v.clear_color());
  EXPECT_FALSE(hovering.rgba == before.rgba); // ring and tooltip
  d.send({pointer_event::kind::leave});
  EXPECT_TRUE(rasterize(*d.last, d.v.textures(), d.v.clear_color()).rgba == before.rgba);
}

} // namespace
} // namespace bdg::nymph
