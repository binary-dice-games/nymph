// MIT License © 2026 Binary Dice Games
/// @file test_plot3d.cpp
/// @brief Plan step 10: `Plot3D` and the 3D series, with wish's schema,
///        drawn through the whole pipeline.
#include <gtest/gtest.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
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

const fs::path kDataDir = fs::path(NYMPH_SOURCE_DIR) / "tests" / "data" / "plot3d";

std::string read_file(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

error source_error(const std::string& source) {
  try {
    check_source(source);
  } catch (const error& e) {
    return e;
  }
  ADD_FAILURE() << "no error for:\n" << source;
  return error(0, 0, "");
}

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
    if (const auto* i = std::get_if<int64_t>(&f.default_value)) {
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

std::vector<std::string> with_item_fields(std::vector<std::string> own) {
  own.insert(
      own.end(),
      {"label=''", "color=''", "fill_color=''", "line_weight=1", "fill_alpha=-1", "marker=-3", "marker_size=0"});
  return sorted(own);
}

// ── Pinned schema ──────────────────────────────────────────────────

TEST(plot3d, SchemaIsPinned) {
  // Started from wish's src/ui/plot3d_elements/*.cpp registrations, plus
  // nymph's text style fields.
  const std::map<std::string, std::vector<std::string>> expected{
      {"Plot3D",
       sorted(
           {"font=''",
            "font_size=0",
            "text_color=''",
            "title='##plot3d'",
            "x_label=''",
            "y_label=''",
            "z_label=''",
            "width=-1",
            "height=400",
            "flags=0",
            "x_flags=0",
            "y_flags=0",
            "z_flags=0",
            "colormap=-1"})},
      {"Plot3DLine", with_item_fields({"xs=[]", "ys=[]", "zs=[]"})},
      {"Plot3DScatter", with_item_fields({"xs=[]", "ys=[]", "zs=[]"})},
      {"Plot3DTriangle", with_item_fields({"xs=[]", "ys=[]", "zs=[]"})},
      {"Plot3DQuad", with_item_fields({"xs=[]", "ys=[]", "zs=[]"})},
      {"Plot3DSurface",
       with_item_fields({"xs=[]", "ys=[]", "zs=[]", "x_count=2", "y_count=2", "scale_min=0", "scale_max=0"})},
      {"Plot3DMesh", with_item_fields({"xs=[]", "ys=[]", "zs=[]", "indices=[]"})},
      {"Plot3DText",
       with_item_fields(
           {"font=''",
            "font_size=0",
            "text_color=''",
            "text=''",
            "x=0",
            "y=0",
            "z=0",
            "angle=0",
            "offset_x=0",
            "offset_y=0"})},
  };
  for (const auto& [type, fields] : expected) {
    EXPECT_EQ(schema_of(type), fields) << type;
  }
  size_t plot3d_classes = 0;
  for (const element_class* cls : builtin_registry().classes()) {
    plot3d_classes += (cls->slot() == "plot3d.item" || cls->name() == "Plot3D") ? 1 : 0;
  }
  EXPECT_EQ(plot3d_classes, expected.size());
}

// ── Every sample renders ─────────────────────────────────────────────────────

TEST(plot3d, EverySampleRendersAndDrawsSomething) {
  std::vector<fs::path> files;
  for (const auto& entry : fs::directory_iterator(kDataDir)) {
    if (entry.path().extension() == ".yaml") {
      files.push_back(entry.path());
    }
  }
  std::sort(files.begin(), files.end());
  ASSERT_EQ(files.size(), 7u) << "one sample per 3D series";
  for (const fs::path& file : files) {
    SCOPED_TRACE(file.filename().string());
    const figure f = bind_source(read_file(file), builtin_registry());
    const image with_series = render_figure(f);
    figure empty = f;
    empty.root.children.clear();
    EXPECT_FALSE(with_series.rgba == render_figure(empty).rgba) << "the series drew nothing";
  }
}

TEST(plot3d, SurfacePortedFromWishRoundTrips) {
  const std::string source = read_file(kDataDir / "surface.yaml");
  const std::vector<uint8_t> png = render_png(source);
  EXPECT_EQ(extract_source(png).value_or(""), source);
  EXPECT_EQ(png, render_png(source));
}

// ── Placement and root ───────────────────────────────────────────────────────

TEST(plot3d, SeriesKindsDoNotMix) {
  EXPECT_NE(
      std::string(
          source_error("root: {type: Plot, children: [{type: Plot3DLine, xs: [0], ys: [0], zs: [0]}]}\n").what())
          .find("goes inside: Plot3D"),
      std::string::npos);
  EXPECT_NE(
      std::string(source_error("root: {type: Plot3D, children: [{type: PlotLine, ys: [0]}]}\n").what())
          .find("cannot be placed in Plot3D"),
      std::string::npos);
  EXPECT_NE(
      std::string(source_error("root: {type: Plot, children: [{type: Plot3D}]}\n").what())
          .find("'Plot3D' cannot be placed in Plot"),
      std::string::npos);
}

TEST(plot3d, RootPlot3DFillsTheImage) {
  EXPECT_EQ(bind_source("root: {type: Plot3D}\n", builtin_registry()).root.get<double>(5), -1.0);
  EXPECT_EQ(bind_source("root: {type: Plot3D, height: 200}\n", builtin_registry()).root.get<double>(5), 200.0);
}

// ── Checks between fields ────────────────────────────────────────────────────

std::string with_child(const std::string& child) {
  return "root:\n  type: Plot3D\n  children:\n    - " + child + "\n";
}

/// Expects the error on line 4, where `at` first appears, containing `part`.
void expect_error(const std::string& source, const std::string& at, const std::string& part) {
  const error e = source_error(source);
  const std::string line4 = source.substr(source.find("    - "));
  EXPECT_EQ(e.line(), 4) << e.what();
  EXPECT_EQ(e.column(), static_cast<int>(line4.find(at)) + 1) << e.what() << "\nexpected at: " << at;
  EXPECT_NE(std::string(e.what()).find(part), std::string::npos) << e.what() << "\nexpected: " << part;
}

TEST(plot3d, CoordinatesMustBeComplete) {
  expect_error(with_child("{type: Plot3DLine, label: a}"), "{", "Plot3DLine.xs is empty");
  expect_error(
      with_child("{type: Plot3DScatter, xs: [1, 2], ys: [1, 2], zs: [1]}"),
      "[1]}",
      "Plot3DScatter.zs has 1 values but xs has 2");
}

TEST(plot3d, TrianglesAndQuadsComeInWholeGroups) {
  expect_error(
      with_child("{type: Plot3DTriangle, xs: [0, 1, 0, 1], ys: [0, 0, 1, 1], zs: [0, 0, 0, 0]}"),
      "[0, 1, 0, 1]",
      "Plot3DTriangle.xs has 4 vertices; each triangle takes 3");
  expect_error(
      with_child("{type: Plot3DQuad, xs: [0, 1, 1], ys: [0, 0, 1], zs: [0, 0, 0]}"), "[0, 1, 1]", "each quad takes 4");
}

TEST(plot3d, SurfaceGridMustMatch) {
  expect_error(
      with_child("{type: Plot3DSurface, x_count: 3, y_count: 2, xs: [0, 1, 2], ys: [0, 0, 0], zs: [1, 2, 3]}"),
      "[0, 1, 2]",
      "Plot3DSurface.xs has 3 values; x_count x y_count needs 6 (3 x 2)");
}

TEST(plot3d, MeshIndicesAreBoundsChecked) {
  // An out-of-range index would make ImPlot3D read past the vertex arrays.
  const std::string vertices = "xs: [0, 1, 0], ys: [0, 0, 1], zs: [0, 0, 0]";
  expect_error(
      with_child("{type: Plot3DMesh, " + vertices + ", indices: [0, 1, 3]}"),
      "[0, 1, 3]",
      "Plot3DMesh.indices entry 3 is 3, but the vertices are numbered 0 to 2");
  expect_error(with_child("{type: Plot3DMesh, " + vertices + ", indices: [0, -1, 2]}"), "[0, -1, 2]", "entry 2 is -1");
  expect_error(
      with_child("{type: Plot3DMesh, " + vertices + ", indices: [0, 1]}"), "[0, 1]}", "needs a non-zero multiple of 3");
  expect_error(with_child("{type: Plot3DMesh, " + vertices + "}"), "{", "Plot3DMesh.indices has 0 entries");
  EXPECT_NO_THROW(check_source(with_child("{type: Plot3DMesh, " + vertices + ", indices: [0, 1, 2]}")));
}

TEST(plot3d, TextNeedsText) {
  expect_error(with_child("{type: Plot3DText, x: 1}"), "{", "Plot3DText.text is empty");
}

} // namespace
} // namespace bdg::nymph
