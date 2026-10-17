// MIT License © 2026 Binary Dice Games
/// @file test_plot2d.cpp
/// @brief Plan step 9: `Plot` and the 2D series, with wish's schema, drawn
///        through the whole pipeline (bind, render, PNG).
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include "core/error.hpp"
#include "document/bind.hpp"
#include "elements/register_builtin.hpp"
#include "nymph.hpp"
#include "render/figure_renderer.hpp"
#include "schema/element_class.hpp"

namespace bdg::nymph {
namespace {

namespace fs = std::filesystem;

const fs::path kDataDir = fs::path(NYMPH_SOURCE_DIR) / "tests" / "data" / "plot2d";

std::string read_file(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

image render(const std::string& source) {
  return render_figure(bind_source(source, builtin_registry()));
}

/// The bind error for `source`.
error source_error(const std::string& source) {
  try {
    check_source(source);
  } catch (const error& e) {
    return e;
  }
  ADD_FAILURE() << "no error for:\n" << source;
  return error(0, 0, "");
}

using rgba = std::array<uint8_t, 4>;

size_t count_color(const image& img, rgba c, int x0 = 0, int y0 = 0, int x1 = -1, int y1 = -1) {
  x1 = x1 < 0 ? img.width : x1;
  y1 = y1 < 0 ? img.height : y1;
  size_t n = 0;
  for (int y = y0; y < y1; ++y) {
    for (int x = x0; x < x1; ++x) {
      const uint8_t* p = img.at(x, y);
      n += (rgba{p[0], p[1], p[2], p[3]} == c) ? 1 : 0;
    }
  }
  return n;
}

size_t distinct_colors(const image& img, int x0, int y0, int x1, int y1) {
  std::set<uint32_t> colors;
  for (int y = y0; y < y1; ++y) {
    for (int x = x0; x < x1; ++x) {
      const uint8_t* p = img.at(x, y);
      colors.insert(static_cast<uint32_t>(p[0]) | p[1] << 8 | p[2] << 16);
    }
  }
  return colors.size();
}

// ── Pinned schema ──────────────────────────────────────────────────

/// "name=default" for every field of a class, sorted.
std::vector<std::string> schema_of(const std::string& type) {
  const element_class* cls = builtin_registry().find(type);
  EXPECT_NE(cls, nullptr) << type;
  std::vector<std::string> out;
  if (cls == nullptr) {
    return out;
  }
  for (const field_desc& f : cls->fields()) {
    std::string def;
    if (const auto* b = std::get_if<bool>(&f.default_value)) {
      def = *b ? "true" : "false";
    } else if (const auto* i = std::get_if<int64_t>(&f.default_value)) {
      def = std::to_string(*i);
    } else if (const auto* d = std::get_if<double>(&f.default_value)) {
      std::ostringstream os;
      os << *d;
      def = os.str();
    } else if (const auto* s = std::get_if<std::string>(&f.default_value)) {
      def = "'" + *s + "'";
    } else {
      def = "[]";
    }
    out.push_back(f.name + "=" + def);
  }
  std::sort(out.begin(), out.end());
  return out;
}

std::vector<std::string> sorted(std::vector<std::string> v) {
  std::sort(v.begin(), v.end());
  return v;
}

/// Fields of wish's PlotItem base, inherited by every series.
std::vector<std::string> with_item_fields(std::vector<std::string> own) {
  own.insert(
      own.end(),
      {"label=''", "color=''", "fill_color=''", "line_weight=1", "fill_alpha=-1", "marker=-3", "marker_size=0"});
  return sorted(own);
}

TEST(plot2d, SchemaIsPinned) {
  // Started from wish's src/ui/plot_elements/*.cpp registrations, plus
  // nymph's text style fields.
  const std::map<std::string, std::vector<std::string>> expected{
      {"Plot",
       sorted(
           {"font=''",
            "font_size=0",
            "text_color=''",
            "title='##plot'",
            "x_label=''",
            "y_label=''",
            "width=-1",
            "height=300",
            "flags=0",
            "x_flags=0",
            "y_flags=0",
            "x_min=0",
            "x_max=0",
            "y_min=0",
            "y_max=0",
            "legend_location=-1",
            "legend_flags=0",
            "colormap=-1"})},
      {"PlotLine", with_item_fields({"xs=[]", "ys=[]"})},
      {"PlotScatter", with_item_fields({"xs=[]", "ys=[]"})},
      {"PlotStairs", with_item_fields({"xs=[]", "ys=[]"})},
      {"PlotDigital", with_item_fields({"xs=[]", "ys=[]"})},
      {"PlotStems", with_item_fields({"xs=[]", "ys=[]", "ref=0"})},
      {"PlotShaded", with_item_fields({"xs=[]", "ys=[]", "ys2=[]", "ref=0"})},
      {"PlotBars", with_item_fields({"xs=[]", "ys=[]", "bar_size=0.67"})},
      {"PlotBarsH", with_item_fields({"xs=[]", "ys=[]", "bar_size=0.67"})},
      {"PlotHistogram",
       with_item_fields({"values=[]", "bins=-1", "cumulative=false", "density=false", "range_min=0", "range_max=0"})},
      {"PlotHistogram2D", with_item_fields({"xs=[]", "ys=[]", "x_bins=-1", "y_bins=-1"})},
      {"PlotHeatmap",
       with_item_fields(
           {"values=[]",
            "rows=1",
            "cols=1",
            "scale_min=0",
            "scale_max=1",
            "format='%.1f'",
            "x_min=0",
            "x_max=1",
            "y_min=0",
            "y_max=1"})},
      {"PlotPieChart",
       with_item_fields(
           {"labels=''",
            "values=[]",
            "x=0.5",
            "y=0.5",
            "radius=0.4",
            "normalize=false",
            "label_fmt='%.1f%%'",
            "angle0=90"})},
      {"PlotText",
       with_item_fields(
           {"font=''", "font_size=0", "text_color=''", "text=''", "x=0", "y=0", "offset_x=0", "offset_y=0"})},
      {"PlotInfLines", with_item_fields({"values=[]", "horizontal=false"})},
  };
  for (const auto& [type, fields] : expected) {
    EXPECT_EQ(schema_of(type), fields) << type;
  }
  size_t plot2d_classes = 0;
  for (const element_class* cls : builtin_registry().classes()) {
    plot2d_classes += (cls->slot() == "plot2d.item" || cls->name() == "Plot") ? 1 : 0;
  }
  EXPECT_EQ(plot2d_classes, expected.size());
}

TEST(plot2d, SeriesGoOnlyInAPlot) {
  const error e = source_error("root: {type: PlotLine, ys: [1]}\n");
  EXPECT_NE(std::string(e.what()).find("goes inside: Plot"), std::string::npos) << e.what();
}

// ── Every sample renders ─────────────────────────────────────────────────────

std::vector<fs::path> samples() {
  std::vector<fs::path> out;
  for (const auto& entry : fs::directory_iterator(kDataDir)) {
    if (entry.path().extension() == ".yaml") {
      out.push_back(entry.path());
    }
  }
  std::sort(out.begin(), out.end());
  return out;
}

TEST(plot2d, EverySampleRendersAndDrawsSomething) {
  const std::vector<fs::path> files = samples();
  ASSERT_EQ(files.size(), 14u) << "one sample per 2D series";
  for (const fs::path& file : files) {
    SCOPED_TRACE(file.filename().string());
    const std::string source = read_file(file);
    const figure f = bind_source(source, builtin_registry());
    const image with_series = render_figure(f);
    figure empty = f;
    empty.root.children.clear();
    const image without = render_figure(empty);
    ASSERT_EQ(with_series.rgba.size(), without.rgba.size());
    EXPECT_FALSE(with_series.rgba == without.rgba) << "the series drew nothing";
  }
}

TEST(plot2d, PngRoundTripAndDeterminism) {
  const std::string source = read_file(kDataDir / "line.yaml");
  const std::vector<uint8_t> a = render_png(source);
  const std::vector<uint8_t> b = render_png(source);
  EXPECT_EQ(a, b);
  EXPECT_EQ(extract_source(a).value_or(""), source);
}

// ── Fields change the image ──────────────────────────────────────────────────

const char* const kLine =
    "image: {width: 300, height: 200}\n"
    "root:\n"
    "  type: Plot\n"
    "  children:\n"
    "    - {type: PlotLine, label: a, ys: [1, 3, 2, 4]";

TEST(plot2d, SeriesColourAndWeightAreUsed) {
  const image red = render(std::string(kLine) + ", color: \"#FF0000\", line_weight: 3}\n");
  const image plain = render(std::string(kLine) + "}\n");
  EXPECT_GT(count_color(red, {255, 0, 0, 255}), 100u);
  EXPECT_EQ(count_color(plain, {255, 0, 0, 255}), 0u);
}

TEST(plot2d, AxisLimitsAndFlagsChangeTheImage) {
  const image base = render(std::string(kLine) + "}\n");
  const image limited = render(
      "image: {width: 300, height: 200}\nroot:\n  type: Plot\n  x_min: -5\n  x_max: 10\n  children:\n"
      "    - {type: PlotLine, label: a, ys: [1, 3, 2, 4]}\n");
  EXPECT_FALSE(base.rgba == limited.rgba);
  // The legend sits top-left inside the plot by default.
  const image no_legend = render(
      "image: {width: 300, height: 200}\nroot:\n  type: Plot\n  flags: NoLegend\n  children:\n"
      "    - {type: PlotLine, label: a, ys: [1, 3, 2, 4]}\n");
  EXPECT_NE(distinct_colors(base, 40, 10, 120, 50), distinct_colors(no_legend, 40, 10, 120, 50));
}

TEST(plot2d, Histogram2DShowsCountsNotOneColour) {
  // Regression for ImPlot v1.0's PlotHistogram2D range bug (see plot2d.cpp).
  const image img = render(read_file(kDataDir / "histogram2d.yaml"));
  EXPECT_GE(distinct_colors(img, 80, 60, 400, 260), 3u);
}

TEST(plot2d, RootPlotFillsTheImageAndPlotsInsideKeepTheirHeight) {
  const figure f = bind_source("root: {type: Plot}\n", builtin_registry());
  EXPECT_EQ(f.root.get<double>(4), -1.0);
  const figure g = bind_source("root: {type: Plot, height: 120}\n", builtin_registry());
  EXPECT_EQ(g.root.get<double>(4), 120.0);
}

// ── Checks between fields ────────────────────────────────────────────────────

/// A Plot with one child written as `child`.
std::string with_child(const std::string& child) {
  return "root:\n  type: Plot\n  children:\n    - " + child + "\n";
}

/// Expects `source` (a with_child() source: the child is on line 4) to fail
/// where `at` first appears on that line, with a message containing `part`.
void expect_error(const std::string& source, const std::string& at, const std::string& part) {
  const error e = source_error(source);
  const std::string line4 = source.substr(source.find("    - "));
  EXPECT_EQ(e.line(), 4) << e.what();
  EXPECT_EQ(e.column(), static_cast<int>(line4.find(at)) + 1) << e.what() << "\nexpected at: " << at;
  EXPECT_NE(std::string(e.what()).find(part), std::string::npos) << e.what() << "\nexpected: " << part;
}

TEST(plot2d, SeriesNeedData) {
  expect_error(with_child("{type: PlotLine, label: a}"), "{", "PlotLine.ys is empty: the series has no data");
  expect_error(
      with_child("{type: PlotLine, xs: [1, 2, 3], ys: [1, 2]}"), "[1, 2, 3]", "PlotLine.xs has 3 values but ys has 2");
  expect_error(
      with_child("{type: PlotShaded, ys: [1, 2], ys2: [1]}"), "[1]}", "PlotShaded.ys2 has 1 values but ys has 2");
  expect_error(with_child("{type: PlotHistogram, values: [1], bins: 0}"), "0}", "PlotHistogram.bins cannot be 0");
  expect_error(with_child("{type: PlotHistogram2D, xs: [1, 2], ys: [1]}"), "[1]}", "ys has 1 values but xs has 2");
  expect_error(with_child("{type: PlotText, x: 1}"), "{", "PlotText.text is empty");
  expect_error(with_child("{type: PlotInfLines}"), "{", "PlotInfLines.values is empty");
}

TEST(plot2d, YsAloneIsDrawnAgainstItsIndex) {
  const figure f = bind_source(with_child("{type: PlotLine, ys: [1, 2, 3]}"), builtin_registry());
  EXPECT_TRUE(f.root.children[0].get<std::vector<double>>(7).empty());
  EXPECT_NO_THROW(render_figure(f));
}

TEST(plot2d, HeatmapGridMustMatchItsValues) {
  expect_error(
      with_child("{type: PlotHeatmap, rows: 2, cols: 3, values: [1, 2, 3, 4]}"),
      "[1, 2, 3, 4]",
      "PlotHeatmap.values has 4 values; rows x cols needs 6 (2 x 3)");
}

TEST(plot2d, PieNeedsALabelPerSlice) {
  expect_error(with_child("{type: PlotPieChart, values: [1, 2, 3]}"), "{", "PlotPieChart.labels must name each slice");
  expect_error(with_child("{type: PlotPieChart, labels: \"a\\nb\", values: [1, 2, 3]}"), "\"a", "(3 values, 2 labels)");
}

TEST(plot2d, PrintfFormatsAreValidated) {
  // Formats reach printf inside ImPlot: anything but one %f/%e/%g is refused.
  for (const char* bad : {"%s", "%d", "%n", "%.1f %.1f", "%*f", "%f%s", "%100f", "%.123f", "%"}) {
    const std::string source = with_child(std::string("{type: PlotHeatmap, values: [1], format: \"") + bad + "\"}");
    const error e = source_error(source);
    EXPECT_NE(std::string(e.what()).find("PlotHeatmap.format"), std::string::npos) << bad << ": " << e.what();
  }
  for (const char* good : {"", "%.0f", "%g", "%+08.2e", "%.1f%%", "value: %5.2f units", "100%%"}) {
    const std::string source = with_child(std::string("{type: PlotHeatmap, values: [1], format: \"") + good + "\"}");
    EXPECT_NO_THROW(check_source(source)) << good;
  }
  const error pie = source_error(with_child("{type: PlotPieChart, labels: a, values: [1], label_fmt: \"%s\"}"));
  EXPECT_NE(std::string(pie.what()).find("PlotPieChart.label_fmt"), std::string::npos) << pie.what();
}

} // namespace
} // namespace bdg::nymph
