// MIT License © 2026 Binary Dice Games
/// @file test_pick.cpp
/// @brief spec/04_web_viewer plan step 5: hovering a data point of a 2D
///        series reports it and shows a tooltip, in a live view only.
#include <gtest/gtest.h>
#include <imgui.h>
#include <map>
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

// ── Bars, cells, slices (plan step 6) ─────────────────────────────────────────

/// A plot from 0 to `xmax` and 0 to `ymax` holding `children`.
std::string area_plot(const std::string& children, double xmax, double ymax, const std::string& extra = "") {
  return "image: {width: 400, height: 300}\n"
         "root:\n"
         "  type: Plot\n"
         "  x_min: -0.5\n"
         "  x_max: " +
      std::to_string(xmax) + "\n  y_min: 0\n  y_max: " + std::to_string(ymax) + "\n" + extra + "  children:\n" +
      children;
}

TEST(pick_shapes, ABarIsPickedInsideItAndNotInTheGap) {
  driven_view d(area_plot("    - {type: PlotBars, label: b, ys: [3, 1, 2]}\n", 2.5, 4));
  const ImVec2 in = driven_view::at(d.plot(), 1.1, 0.5);
  d.move(in.x, in.y);
  ASSERT_TRUE(d.v.hovered().has_value());
  EXPECT_EQ(d.v.hovered()->type, "PlotBars");
  EXPECT_EQ(d.v.hovered()->index, 1);
  EXPECT_EQ(value_of(*d.v.hovered(), "y"), "1");
  const ImVec2 gap = driven_view::at(d.plot(), 1.5, 0.5);
  d.move(gap.x, gap.y);
  EXPECT_FALSE(d.v.hovered().has_value());
  const ImVec2 above = driven_view::at(d.plot(), 1, 1.5); // above the short bar
  d.move(above.x, above.y);
  EXPECT_FALSE(d.v.hovered().has_value());
}

TEST(pick_shapes, AHorizontalBarReportsItsLengthAsX) {
  // Bars at y = 1 and 2 with lengths 3 and 1.
  driven_view d(area_plot("    - {type: PlotBarsH, label: h, xs: [3, 1], ys: [1, 2], bar_size: 0.5}\n", 4, 3));
  const ImVec2 p = driven_view::at(d.plot(), 2.5, 1.1);
  d.move(p.x, p.y);
  ASSERT_TRUE(d.v.hovered().has_value());
  EXPECT_EQ(d.v.hovered()->index, 0);
  EXPECT_EQ(value_of(*d.v.hovered(), "x"), "3");
  EXPECT_EQ(value_of(*d.v.hovered(), "y"), "1");
}

TEST(pick_shapes, AHistogramBinReportsItsRangeAndCount) {
  driven_view d(area_plot("    - {type: PlotHistogram, label: h, values: [0, 1, 1, 2, 2, 2, 3], bins: 3}\n", 3.5, 4));
  // Bins of width 1 over [0, 3]: the last one, [2, 3], holds 2, 2, 2 and 3.
  const ImVec2 p = driven_view::at(d.plot(), 2.5, 1);
  d.move(p.x, p.y);
  ASSERT_TRUE(d.v.hovered().has_value());
  EXPECT_EQ(d.v.hovered()->index, 2);
  EXPECT_EQ(value_of(*d.v.hovered(), "range"), "2 to 3");
  EXPECT_EQ(value_of(*d.v.hovered(), "count"), "4");
}

TEST(pick_shapes, AHeatmapCellReportsItsRowColumnAndValue) {
  driven_view d(area_plot(
      "    - {type: PlotHeatmap, label: h, values: [1, 2, 3, 4, 5, 6], rows: 2, cols: 3, x_min: 0, x_max: 3, y_min: 0, "
      "y_max: 2}\n",
      3,
      2));
  // Row 0 is the top row: (2.5, 1.5) is row 0, column 2.
  const ImVec2 p = driven_view::at(d.plot(), 2.5, 1.5);
  d.move(p.x, p.y);
  ASSERT_TRUE(d.v.hovered().has_value());
  EXPECT_EQ(value_of(*d.v.hovered(), "row"), "0");
  EXPECT_EQ(value_of(*d.v.hovered(), "column"), "2");
  EXPECT_EQ(value_of(*d.v.hovered(), "value"), "3");
  const ImVec2 q = driven_view::at(d.plot(), 0.5, 0.5);
  d.move(q.x, q.y);
  ASSERT_TRUE(d.v.hovered().has_value());
  EXPECT_EQ(value_of(*d.v.hovered(), "value"), "4");
}

TEST(pick_shapes, A2DHistogramCellReportsItsCount) {
  // x and y over [0, 2], 2 x 2 bins: (0.5, 0.5) twice, (1.5, 1.5) once.
  driven_view d(area_plot(
      "    - {type: PlotHistogram2D, label: h, xs: [0, 0.5, 2], ys: [0, 0.5, 2], x_bins: 2, y_bins: 2}\n", 2, 2));
  const ImVec2 p = driven_view::at(d.plot(), 0.5, 0.5);
  d.move(p.x, p.y);
  ASSERT_TRUE(d.v.hovered().has_value());
  EXPECT_EQ(value_of(*d.v.hovered(), "count"), "2");
  EXPECT_EQ(value_of(*d.v.hovered(), "x"), "0 to 1");
}

TEST(pick_shapes, APieSliceReportsItsValueAndShare) {
  // Two halves from angle 0, counter-clockwise: a on top, b below.
  driven_view d(
      "image: {width: 400, height: 400}\n"
      "root:\n"
      "  type: Plot\n"
      "  flags: Equal\n"
      "  x_min: 0\n  x_max: 1\n  y_min: 0\n  y_max: 1\n"
      "  children:\n"
      "    - {type: PlotPieChart, labels: \"a\\nb\", values: [3, 1], x: 0.5, y: 0.5, radius: 0.4, angle0: 0}\n",
      400,
      400);
  const ImVec2 top = driven_view::at(d.plot(), 0.5, 0.8);
  d.move(top.x, top.y);
  ASSERT_TRUE(d.v.hovered().has_value());
  EXPECT_EQ(d.v.hovered()->label, "a");
  EXPECT_EQ(value_of(*d.v.hovered(), "value"), "3");
  EXPECT_EQ(value_of(*d.v.hovered(), "share"), "75.0%");
  // b spans the last quarter: angles 270 to 360 degrees, lower right.
  const ImVec2 b = driven_view::at(d.plot(), 0.7, 0.3);
  d.move(b.x, b.y);
  ASSERT_TRUE(d.v.hovered().has_value());
  EXPECT_EQ(d.v.hovered()->label, "b");
  const ImVec2 out = driven_view::at(d.plot(), 0.95, 0.95);
  d.move(out.x, out.y);
  EXPECT_FALSE(d.v.hovered().has_value());
}

TEST(pick_shapes, ABarGroupReportsItsItemGroupAndValue) {
  // Groups 0 and 1, items a and b side by side (0.4 wide each).
  driven_view d(area_plot(
      "    - {type: PlotBarGroups, labels: [a, b], values: [1, 2, 3, 4], group_size: 0.8}\n",
      1.5,
      5,
      "  x_ticks: [0, 1]\n  x_tick_labels: [first, second]\n"));
  const ImVec2 p = driven_view::at(d.plot(), 1.2, 1); // group 1, item b
  d.move(p.x, p.y);
  ASSERT_TRUE(d.v.hovered().has_value());
  EXPECT_EQ(d.v.hovered()->label, "b");
  EXPECT_EQ(value_of(*d.v.hovered(), "group"), "second");
  EXPECT_EQ(value_of(*d.v.hovered(), "value"), "4");
}

TEST(pick_shapes, StackedBarGroupsArePickedAtTheirStackedHeight) {
  driven_view d(area_plot(
      "    - {type: PlotBarGroups, labels: [a, b], values: [1, 2, 3, 4], group_size: 0.8, stacked: true}\n", 1.5, 8));
  const ImVec2 p = driven_view::at(d.plot(), 1, 5); // group 1: a in [0, 3], b in [3, 7]
  d.move(p.x, p.y);
  ASSERT_TRUE(d.v.hovered().has_value());
  EXPECT_EQ(d.v.hovered()->label, "b");
  EXPECT_EQ(value_of(*d.v.hovered(), "value"), "4");
}

// ── Gantt and Sankey ─────────────────────────────────────────────────────────

TEST(pick_shapes, AGanttTaskReportsItsDatesAndSection) {
  driven_view d(
      "image: {width: 600, height: 240}\n"
      "root:\n"
      "  type: GanttChart\n"
      "  children:\n"
      "    - type: GanttSection\n"
      "      name: Build\n"
      "      children:\n"
      "        - {type: GanttTask, id: a, name: Write, start: 2026-03-02, duration: 4}\n"
      "        - {type: GanttTask, id: b, name: Test, after: [a], duration: 6}\n",
      600,
      240);
  const plot_view v = d.plot();
  // Task b: row 1 (the Y axis is inverted: row 0 on top), 2026-03-06 to 03-12.
  const double mid = 1772755200 + 3 * 86400.0; // 2026-03-09
  const float x = v.x + static_cast<float>((mid - v.x_min) / (v.x_max - v.x_min)) * v.width;
  const float y = v.y + static_cast<float>((1 - v.y_min) / (v.y_max - v.y_min)) * v.height;
  d.move(x, y);
  ASSERT_TRUE(d.v.hovered().has_value());
  EXPECT_EQ(d.v.hovered()->type, "GanttTask");
  EXPECT_EQ(d.v.hovered()->label, "Test");
  EXPECT_EQ(d.v.hovered()->element, "root/children/0/children/1");
  EXPECT_EQ(value_of(*d.v.hovered(), "start"), "2026-03-06");
  EXPECT_EQ(value_of(*d.v.hovered(), "end"), "2026-03-12");
  EXPECT_EQ(value_of(*d.v.hovered(), "section"), "Build");
}

TEST(pick_shapes, ASankeyDiagramReportsNodesAndFlows) {
  driven_view d(
      "image: {width: 400, height: 200}\n"
      "root:\n"
      "  type: SankeyDiagram\n"
      "  from: [A, A]\n"
      "  to: [B, C]\n"
      "  values: [30, 10]\n",
      400,
      200);
  // Sweep the diagram; every node and flow must be found somewhere.
  std::map<std::string, std::string> seen; // label -> value
  for (float y = 4; y < 200; y += 6) {
    for (float x = 4; x < 400; x += 12) {
      d.move(x, y);
      if (const std::optional<view_pick>& h = d.v.hovered()) {
        seen[h->type + " " + h->label] = value_of(*h, "value");
      }
    }
  }
  EXPECT_EQ(seen["SankeyNode A"], "40");
  EXPECT_EQ(seen["SankeyNode B"], "30");
  EXPECT_EQ(seen["SankeyNode C"], "10");
  EXPECT_EQ(seen["SankeyFlow A -> B"], "30");
  EXPECT_EQ(seen["SankeyFlow A -> C"], "10");
}

// ── Diagrams (plan step 7) ───────────────────────────────────────────────────

TEST(pick_diagrams, AClassAndARelationAreReportedAsTheirElements) {
  driven_view d(
      "image: {width: 400, height: 300}\n"
      "root:\n"
      "  type: ClassDiagram\n"
      "  children:\n"
      "    - {type: Class, id: Animal}\n"
      "    - {type: Class, id: Duck}\n"
      "    - {type: ClassRelation, from: Duck, to: Animal, kind: Inheritance}\n");
  std::map<std::string, view_pick> seen; // label -> pick
  for (float y = 4; y < 300; y += 5) {
    for (float x = 4; x < 400; x += 10) {
      d.move(x, y);
      if (const std::optional<view_pick>& h = d.v.hovered()) {
        seen.emplace(h->label, *h);
      }
    }
  }
  ASSERT_TRUE(seen.count("Animal"));
  EXPECT_EQ(seen["Animal"].type, "Class");
  EXPECT_EQ(seen["Animal"].element, "root/children/0");
  EXPECT_EQ(value_of(seen["Animal"], "id"), "Animal");
  ASSERT_TRUE(seen.count("Duck -> Animal"));
  EXPECT_EQ(seen["Duck -> Animal"].type, "ClassRelation");
  EXPECT_EQ(seen["Duck -> Animal"].element, "root/children/2");
}

} // namespace
} // namespace bdg::nymph
