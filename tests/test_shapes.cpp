// MIT License © 2026 Binary Dice Games
/// @file test_shapes.cpp
/// @brief spec/13_more_diagrams_and_shapes PLAN steps 1 to 3 and 6: the
///        shape file format, the catalog and its lint, ImUml's shape
///        drawing through the `Shape` element, and `nymph shapes`.
#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include "cli/cli.hpp"
#include "core/error.hpp"
#include "nymph.hpp"
#include "shapes/shape_catalog.hpp"
#include "ui_test_util.hpp"

namespace bdg::nymph {
namespace {

using test::pixel;
using test::render_ui;
using test::rgba;

// ── The format ───────────────────────────────────────────────────────────────

std::string refusal(const std::string& svg) {
  try {
    parse_svg(svg, svg_dialect::shape);
  } catch (const svg_error& e) {
    return e.what();
  }
  return "accepted";
}

const std::string kHead = "<svg viewBox=\"0 0 80 48\" preserveAspectRatio=\"none\"";

TEST(shape_format, ArcsBecomeCubicsOnTheCircle) {
  const svg_image image = parse_svg(
      "<svg viewBox=\"0 0 20 20\"><path d=\"M10 0 A10 10 0 0 1 0 10\" fill=\"none\" stroke=\"#000000\"/></svg>",
      svg_dialect::shape);
  ASSERT_EQ(image.shapes.size(), 1u);
  const std::vector<ImVec2> pts = flatten(image.shapes[0].subpaths[0], 24);
  ASSERT_GT(pts.size(), 8u);
  // A quarter circle about (0, 0): the arc from (10, 0) to (0, 10).
  for (const ImVec2& p : pts) {
    EXPECT_NEAR(std::hypot(p.x, p.y), 10.0f, 0.05f);
  }
  EXPECT_NEAR(pts.back().x, 0.0f, 1e-4f);
  EXPECT_NEAR(pts.back().y, 10.0f, 1e-4f);
}

TEST(shape_format, PlaceholdersStillRefuseArcs) {
  EXPECT_THROW(parse_svg("<svg viewBox=\"0 0 20 20\"><path d=\"M10 0 A10 10 0 0 1 0 10\"/></svg>"), svg_error);
}

TEST(shape_format, MetadataIsParsed) {
  const svg_image image = parse_svg(
      kHead + " data-slice=\"10 0 10 0\" data-grow=\"proportional\">"
              "<rect x=\"0\" y=\"0\" width=\"80\" height=\"48\" fill=\"#FFFFFF\" stroke=\"#000000\" stroke-dasharray=\"6 3\"/>"
              "<rect id=\"text\" x=\"10\" y=\"6\" width=\"60\" height=\"36\" fill=\"none\" stroke=\"none\"/>"
              "<rect id=\"content\" x=\"4\" y=\"4\" width=\"70\" height=\"40\" fill=\"none\" stroke=\"none\"/>"
              "</svg>",
      svg_dialect::shape);
  EXPECT_TRUE(image.stretch);
  EXPECT_EQ(image.slice[0], 10.0f);
  EXPECT_EQ(image.slice[2], 10.0f);
  EXPECT_TRUE(image.grow_proportional);
  ASSERT_TRUE(image.text.present);
  EXPECT_EQ(image.text.max.x, 70.0f);
  ASSERT_TRUE(image.content.present);
  EXPECT_EQ(image.shapes.size(), 2u); // the text and content rects are never drawn
  EXPECT_EQ(image.shapes[1].dash, 6.0f);
  EXPECT_EQ(image.shapes[1].gap, 3.0f);
}

TEST(shape_format, BrokenMetadataIsRefusedNamingTheRule) {
  const std::string body = "<rect width=\"80\" height=\"48\"/>";
  EXPECT_NE(refusal(kHead + " data-slice=\"1 2 3\">" + body + "</svg>").find("data-slice"), std::string::npos);
  EXPECT_NE(refusal(kHead + " data-slice=\"40 0 40 0\">" + body + "</svg>").find("smaller than the viewBox"), std::string::npos);
  EXPECT_NE(refusal("<svg viewBox=\"0 0 80 48\" data-slice=\"1 1 1 1\">" + body + "</svg>").find("icon"), std::string::npos);
  EXPECT_NE(refusal(kHead + " data-grow=\"big\">" + body + "</svg>").find("data-grow"), std::string::npos);
  EXPECT_NE(
      refusal(kHead + ">" + body + "<rect id=\"text\" x=\"0\" y=\"0\" width=\"4\" height=\"4\"/></svg>").find("never drawn"),
      std::string::npos);
  EXPECT_NE(
      refusal(kHead + ">" + body +
              "<rect id=\"text\" x=\"70\" y=\"0\" width=\"40\" height=\"4\" fill=\"none\" stroke=\"none\"/></svg>")
          .find("inside the viewBox"),
      std::string::npos);
  EXPECT_NE(
      refusal(kHead + ">" + body + "<path id=\"outline\" d=\"M0 0 L10 0\" fill=\"none\"/></svg>").find("closed"), std::string::npos);
  EXPECT_NE(
      refusal(kHead + ">" + body + "<rect width=\"4\" height=\"4\" stroke=\"#000000\" stroke-dasharray=\"0\"/></svg>")
          .find("stroke-dasharray"),
      std::string::npos);
  EXPECT_EQ(refusal(kHead + ">" + body + "</svg>"), "accepted");
}

// ── The catalog ──────────────────────────────────────────────────────────────

/// Whether segments p1-p2 and p3-p4 cross (touching ends do not count).
bool cross(ImVec2 p1, ImVec2 p2, ImVec2 p3, ImVec2 p4) {
  const auto orient = [](ImVec2 a, ImVec2 b, ImVec2 c) {
    const double v = static_cast<double>(b.x - a.x) * (c.y - a.y) - static_cast<double>(b.y - a.y) * (c.x - a.x);
    return std::fabs(v) < 1e-6 ? 0 : (v > 0 ? 1 : -1);
  };
  return orient(p1, p2, p3) * orient(p1, p2, p4) < 0 && orient(p3, p4, p1) * orient(p3, p4, p2) < 0;
}

bool self_intersects(const std::vector<ImVec2>& pts) {
  const size_t n = pts.size();
  for (size_t i = 0; i < n; ++i) {
    for (size_t j = i + 2; j < n; ++j) {
      if ((i == 0 && j == n - 1) || (pts[i].x == pts[j].x && pts[i].y == pts[j].y)) {
        continue;
      }
      if (cross(pts[i], pts[(i + 1) % n], pts[j], pts[(j + 1) % n])) {
        return true;
      }
    }
  }
  return false;
}

TEST(shape_catalog, HasTheDesignedShapesSortedAndUnique) {
  const std::vector<std::string>& names = shape_names();
  EXPECT_TRUE(std::is_sorted(names.begin(), names.end()));
  EXPECT_EQ(std::set<std::string>(names.begin(), names.end()).size(), names.size());
  for (const char* name :
       {"rectangle", "rounded_rectangle", "stadium", "ellipse", "circle", "diamond", "hexagon", "cylinder", "cloud",
        "decision", "terminator", "document", "arrow_right", "chevron", "note", "speech_bubble", "actor", "boundary",
        "control", "entity", "component", "package", "node_3d", "server", "router", "firewall", "user",
        "tabbed_folder", "bordered_frame"}) {
    EXPECT_NE(std::find(names.begin(), names.end(), name), names.end()) << name;
  }
  EXPECT_GE(names.size(), 90u);
  EXPECT_EQ(shape_category("actor"), "uml");
  EXPECT_EQ(shape_category("cloud"), "basic");
  EXPECT_EQ(shape_category("nope"), "");
}

TEST(shape_catalog, EveryFileKeepsTheAuthoringRules) {
  for (const embedded_file& f : embedded_shapes()) {
    SCOPED_TRACE(f.name);
    const std::string_view full = f.name;
    const std::string name(full.substr(full.find('/') + 1));
    const svg_image* image = find_shape(name);
    ASSERT_NE(image, nullptr);
    EXPECT_LE(f.size, 16u * 1024u);
    // Compiled into the web viewer, which is inlined into HTML.
    EXPECT_EQ(f.text().find(std::string("<!") + "--"), std::string_view::npos) << "no XML comments";
    if (!image->stretch) {
      for (const float v : image->slice) {
        EXPECT_EQ(v, 0.0f) << "an icon has no slices";
      }
    }
    size_t segments = 0;
    for (const svg_shape& shape : image->shapes) {
      for (const svg_subpath& sub : shape.subpaths) {
        segments += sub.segments.size();
        if (!shape.stroke) {
          EXPECT_FALSE(self_intersects(flatten(sub))) << "a filled shape crosses itself";
        }
      }
    }
    EXPECT_LE(segments, 200u);
    if (image->has_outline) {
      EXPECT_LE(flatten(image->outline, 12).size(), 64u);
    }
  }
}

// ── Drawing ──────────────────────────────────────────────────────────────────

std::string shape_root(const std::string& fields, int width, int height) {
  return "image: {width: " + std::to_string(width) + ", height: " + std::to_string(height) +
      ", padding: 0}\nroot: {type: Shape" + fields + "}\n";
}

bool dark(const rgba& p) {
  return p[0] < 128 && p[1] < 128 && p[2] < 128;
}

bool red(const rgba& p) {
  return p[0] > 200 && p[1] < 60 && p[2] < 60;
}

TEST(shape_draw, EveryShapeRendersTheSameTwiceAtThreeSizes) {
  for (const std::string& name : shape_names()) {
    for (const int size : {40, 120, 300}) {
      const std::string source = shape_root(", shape: " + name + ", text: x", size, size);
      SCOPED_TRACE(name + " at " + std::to_string(size));
      EXPECT_EQ(render_png(source), render_png(source));
    }
  }
}

TEST(shape_draw, SlicedCornersKeepTheirRadiusWhenResized) {
  // The rounded rectangle's corners have a radius of 10 whatever its size.
  for (const auto& [w, h] : {std::pair{200, 100}, std::pair{400, 300}}) {
    const auto img = render_ui(shape_root(", shape: rounded_rectangle, fill_color: \"#FF0000\"", w, h)).img;
    EXPECT_FALSE(red(pixel(img, 1, 1))) << w << "x" << h << ": outside the rounded corner";
    EXPECT_TRUE(red(pixel(img, 14, 14))) << w << "x" << h << ": inside it";
    EXPECT_TRUE(red(pixel(img, w - 15, h - 15)));
    EXPECT_TRUE(red(pixel(img, w / 2, h / 2)));
  }
}

TEST(shape_draw, ACylinderKeepsItsCapsWhenResized) {
  // The cap's ellipse (ry 12) has its lower edge 24 px below the top, in a
  // short and in a tall cylinder alike.
  for (const int h : {80, 240}) {
    const auto img = render_ui(shape_root(", shape: cylinder", 64, h)).img;
    int dark_rows = 0;
    for (int y = 22; y <= 26; ++y) {
      dark_rows += dark(pixel(img, 32, y)) ? 1 : 0;
    }
    EXPECT_GE(dark_rows, 1) << "height " << h;
  }
}

TEST(shape_draw, AnIconKeepsItsAspectRatio) {
  // The actor is 40 x 72: in a wide box it is centred, the sides stay clear.
  const auto img = render_ui(shape_root(", shape: actor", 300, 72), false).img;
  EXPECT_FALSE(dark(pixel(img, 20, 36)));
  EXPECT_FALSE(dark(pixel(img, 280, 36)));
  int dark_columns = 0;
  for (int x = 100; x < 200; ++x) {
    for (int y = 0; y < 72; ++y) {
      if (dark(pixel(img, x, y))) {
        ++dark_columns;
        break;
      }
    }
  }
  EXPECT_GT(dark_columns, 10);
  EXPECT_LT(dark_columns, 60);
}

TEST(shape_draw, TheLabelIsDrawnInsideTheTextRect) {
  const auto with = render_ui(shape_root(", shape: rectangle, text: Hello", 160, 80)).img;
  const auto without = render_ui(shape_root(", shape: rectangle", 160, 80)).img;
  int differing = 0;
  for (int y = 6; y < 74; ++y) {
    for (int x = 8; x < 152; ++x) {
      differing += pixel(with, x, y) != pixel(without, x, y) ? 1 : 0;
    }
  }
  EXPECT_GT(differing, 20);
}

TEST(shape_draw, ADashedStrokeHasGaps) {
  const auto img = render_ui(shape_root(", shape: dashed_region", 120, 80)).img;
  int runs = 0;
  bool in_dark = false;
  for (int x = 4; x < 116; ++x) {
    const bool d = dark(pixel(img, x, 0)) || dark(pixel(img, x, 1));
    if (d && !in_dark) {
      ++runs;
    }
    in_dark = d;
  }
  EXPECT_GE(runs, 5);
}

TEST(shape_draw, AnUnknownShapeIsAnErrorWithASuggestion) {
  try {
    render_png(shape_root(", shape: clod", 100, 100));
    FAIL() << "accepted";
  } catch (const error& e) {
    EXPECT_NE(std::string(e.what()).find("cloud"), std::string::npos) << e.what();
    EXPECT_GT(e.line(), 0);
  }
}

// ── nymph shapes ─────────────────────────────────────────────────────────────

int run(std::initializer_list<std::string_view> args, std::string* out, std::string* err = nullptr) {
  std::vector<std::string_view> v(args);
  std::istringstream in;
  std::ostringstream o, e;
  const int code = run_cli(v, in, o, e);
  *out = o.str();
  if (err != nullptr) {
    *err = e.str();
  }
  return code;
}

TEST(shapes_command, ListsEveryShapeByCategory) {
  std::string out;
  ASSERT_EQ(run({"shapes"}, &out), 0);
  std::istringstream in(out);
  std::vector<std::string> lines;
  for (std::string line; std::getline(in, line);) {
    lines.push_back(line);
  }
  EXPECT_EQ(lines.size(), shape_names().size());
  EXPECT_TRUE(std::is_sorted(lines.begin(), lines.end()));
  EXPECT_NE(std::find(lines.begin(), lines.end(), "uml/actor"), lines.end());
}

TEST(shapes_command, DescribesOneShapeAndSuggestsForATypo) {
  std::string out, err;
  ASSERT_EQ(run({"shapes", "cylinder"}, &out), 0);
  EXPECT_NE(out.find("mode: resizable"), std::string::npos) << out;
  EXPECT_NE(out.find("slice: 0 24 0 12"), std::string::npos) << out;
  ASSERT_EQ(run({"shapes", "actor"}, &out), 0);
  EXPECT_NE(out.find("mode: icon"), std::string::npos) << out;
  EXPECT_EQ(run({"shapes", "clod"}, &out, &err), 1);
  EXPECT_NE(err.find("did you mean 'cloud'"), std::string::npos) << err;
  EXPECT_EQ(run({"shapes", "a", "b"}, &out, &err), 2);
}

} // namespace
} // namespace bdg::nymph
