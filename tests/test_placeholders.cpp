// MIT License © 2026 Binary Dice Games
/// @file test_placeholders.cpp
/// @brief spec/09_ui_mockups PLAN step 5: the built-in placeholder images,
///        their authoring rules, and Image / ImageButton.
#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>
#include "core/error.hpp"
#include "nymph.hpp"
#include "placeholders/placeholders.hpp"
#include "ui_test_util.hpp"

namespace bdg::nymph {
namespace {

using test::pixel;
using test::render_ui;
using test::rgba;
using test::ui_render;

// ── Authoring rules (placeholders/README.md) ─────────────────────────────────

/// Whether segments p1-p2 and p3-p4 cross (touching ends do not count).
bool cross(ImVec2 p1, ImVec2 p2, ImVec2 p3, ImVec2 p4) {
  const auto orient = [](ImVec2 a, ImVec2 b, ImVec2 c) {
    const double v = static_cast<double>(b.x - a.x) * (c.y - a.y) - static_cast<double>(b.y - a.y) * (c.x - a.x);
    return std::fabs(v) < 1e-6 ? 0 : (v > 0 ? 1 : -1);
  };
  const int o1 = orient(p1, p2, p3), o2 = orient(p1, p2, p4), o3 = orient(p3, p4, p1), o4 = orient(p3, p4, p2);
  return o1 * o2 < 0 && o3 * o4 < 0;
}

/// Whether the closed polygon `pts` crosses itself.
bool self_intersects(const std::vector<ImVec2>& pts) {
  const size_t n = pts.size();
  for (size_t i = 0; i < n; ++i) {
    for (size_t j = i + 2; j < n; ++j) {
      if (i == 0 && j == n - 1) {
        continue; // neighbours through the closing edge
      }
      if (cross(pts[i], pts[(i + 1) % n], pts[j], pts[(j + 1) % n])) {
        return true;
      }
    }
  }
  return false;
}

TEST(placeholders, TheCatalogHasTheDesignedImages) {
  const std::vector<std::string> names = placeholder_names();
  for (const char* name :
       {"cross", "person", "people", "landscape", "product", "logo", "video", "map", "chart", "document", "icon",
        "building"}) {
    EXPECT_NE(std::find(names.begin(), names.end(), name), names.end()) << name;
  }
}

TEST(placeholders, EveryFileKeepsTheAuthoringRules) {
  for (const embedded_file& f : embedded_placeholders()) {
    SCOPED_TRACE(f.name);
    svg_image image;
    ASSERT_NO_THROW(image = parse_svg(f.text()));
    EXPECT_EQ(image.view_size.x, image.view_size.y) << "the viewBox must be square";
    // Compiled into the web viewer, which is inlined into HTML.
    EXPECT_EQ(f.text().find(std::string("<!") + "--"), std::string_view::npos) << "no XML comments";
    for (const svg_shape& shape : image.shapes) {
      if (shape.stroke) {
        continue;
      }
      for (const svg_subpath& sub : shape.subpaths) {
        EXPECT_FALSE(self_intersects(flatten(sub))) << "a filled shape crosses itself";
      }
    }
  }
}

TEST(placeholders, ThingsOutsideTheSubsetAreRefused) {
  const auto refusal = [](const std::string& body) {
    try {
      parse_svg("<svg viewBox=\"0 0 100 100\">" + body + "</svg>");
    } catch (const svg_error& e) {
      return std::string(e.what());
    }
    return std::string("accepted");
  };
  EXPECT_NE(refusal("<path d=\"M0 0 A10 10 0 0 1 20 20 Z\"/>").find("arcs"), std::string::npos);
  EXPECT_NE(refusal("<rect width=\"10\" height=\"10\" fill=\"#FF0000\"/>").find("placeholder colours"), std::string::npos);
  EXPECT_NE(refusal("<rect width=\"10\" height=\"10\" transform=\"scale(2)\"/>").find("'transform'"), std::string::npos);
  EXPECT_NE(refusal("<g><rect width=\"10\" height=\"10\"/></g>").find("self-closing"), std::string::npos);
  EXPECT_NE(refusal("<text x=\"1\" y=\"1\"/>").find("<text> is not allowed"), std::string::npos);
  EXPECT_NE(refusal("hello").find("text outside tags"), std::string::npos);
  EXPECT_NE(refusal("").find("draws nothing"), std::string::npos);
  EXPECT_EQ(refusal("<path d=\"m10 10 l20 0 q5 5 0 10 t-10 0 c-5 0 -5 -5 -10 -5 s-2 -2 0 -5 h5 v-2 z\"/>"), "accepted");
}

// ── Drawing ──────────────────────────────────────────────────────────────────

/// An Image as root, the whole image, on a white image background.
std::string image_root(const std::string& fields, int width = 200, int height = 200, int scale = 1) {
  return "image: {width: " + std::to_string(width) + ", height: " + std::to_string(height) +
      ", padding: 0, scale: " + std::to_string(scale) + "}\nroot: {type: Image" + fields + "}\n";
}

bool near(const rgba& a, const rgba& b, int tolerance) {
  for (int i = 0; i < 3; ++i) {
    if (std::abs(static_cast<int>(a[i]) - static_cast<int>(b[i])) > tolerance) {
      return false;
    }
  }
  return true;
}

TEST(placeholders, EveryImageRendersAtAnySizeTheSameTwice) {
  for (const std::string& name : placeholder_names()) {
    for (const int size : {32, 160, 640}) {
      const std::string source = image_root(", image: " + name, size, size);
      SCOPED_TRACE(name + " at " + std::to_string(size));
      EXPECT_EQ(render_png(source), render_png(source));
    }
  }
}

TEST(placeholders, EdgesStaySharpAtScaleTwo) {
  // The icon's rounded square spans x 14..86 of 100: along the middle row,
  // the step from background to tint takes at most 2 device pixels.
  const ui_render r = render_ui(image_root(", image: icon, tint: \"#000000\", background: \"#FFFFFF\"", 100, 100, 2));
  const int y = 100 + 30; // device pixels: logical 65, inside the square, below the star
  int first = -1, full = -1;
  for (int x = 0; x < 200; ++x) {
    const rgba p = pixel(r.img, x, y);
    if (first < 0 && p[0] < 250) {
      first = x;
    }
    if (p[0] <= 2) {
      full = x;
      break;
    }
  }
  ASSERT_GE(first, 0);
  EXPECT_LE(full - first, 2);
  EXPECT_NEAR(first, 28, 1); // 14 logical px
}

TEST(placeholders, ContainCentresCoverFillsStretchStretches) {
  const std::string common = ", image: icon, width: 300, height: 100, tint: \"#000000\", background: \"#FFFFFF\"";
  const rgba black{0, 0, 0, 255}, white{255, 255, 255, 255};
  const ui_render contain = render_ui(image_root(common, 400, 200));
  EXPECT_EQ(pixel(contain.img, 50, 50), white); // left of the square
  EXPECT_EQ(pixel(contain.img, 120, 80), black); // inside it (x 114..186)
  const ui_render cover = render_ui(image_root(common + ", fit: cover", 400, 200));
  EXPECT_EQ(pixel(cover.img, 50, 50), black); // the square now spans 42..258
  // Nothing outside the box (300 x 100 at the top left).
  const rgba image_background = pixel(contain.img, 350, 150);
  for (int x = 0; x < 400; x += 7) {
    EXPECT_EQ(pixel(cover.img, x, 150), image_background);
  }
  for (int y = 0; y < 200; y += 7) {
    EXPECT_EQ(pixel(cover.img, 350, y), image_background);
  }
  const ui_render stretch = render_ui(image_root(common + ", fit: stretch", 400, 200));
  EXPECT_EQ(pixel(stretch.img, 60, 50), black); // the square spans 42..258 across, 14..86 down
  EXPECT_EQ(pixel(stretch.img, 50, 5), white);
}

TEST(placeholders, TintColoursFullAndHalfTones) {
  // chart: bar 1 (x 14..30, y 56..86) full tone, bar 2 (x 38..54) half tone.
  const ui_render r = render_ui(image_root(", image: chart, tint: \"#2F80ED\", background: \"#FFFFFF\"", 100, 100));
  EXPECT_EQ(pixel(r.img, 22, 75), (rgba{0x2F, 0x80, 0xED, 255}));
  EXPECT_TRUE(near(pixel(r.img, 46, 75), rgba{(0x2F + 255) / 2, (0x80 + 255) / 2, (0xED + 255) / 2, 255}, 2))
      << int(pixel(r.img, 46, 75)[0]);
  const ui_render alpha = render_ui(image_root(", image: chart, tint: \"#00000080\", background: \"#FFFFFF\"", 100, 100));
  EXPECT_TRUE(near(pixel(alpha.img, 22, 75), rgba{127, 127, 127, 255}, 2)) << int(pixel(alpha.img, 22, 75)[0]);
}

TEST(placeholders, BackgroundColoursTheBoxAndTheCutOuts) {
  // icon: the star (centre 50, 50) is a cut-out.
  const ui_render r = render_ui(image_root(", image: icon, tint: \"#000000\", background: \"#FF0000\"", 100, 100));
  EXPECT_EQ(pixel(r.img, 5, 5), (rgba{255, 0, 0, 255}));
  EXPECT_EQ(pixel(r.img, 50, 50), (rgba{255, 0, 0, 255}));
  // Transparent: the box shows what is behind it, and so does the star.
  const ui_render clear = render_ui(image_root(", image: icon, tint: \"#000000\", background: \"#00000000\"", 100, 100));
  const ui_render plain = render_ui("image: {width: 100, height: 100, padding: 0}\nroot: {type: Spring}\n");
  EXPECT_EQ(pixel(clear.img, 5, 5), pixel(plain.img, 5, 5));
  EXPECT_EQ(pixel(clear.img, 50, 50), pixel(plain.img, 5, 5));
}

TEST(placeholders, DefaultColoursFollowTheTheme) {
  const std::string light = image_root(", image: person", 100, 100);
  std::string dark = light;
  dark.replace(dark.find("scale: 1"), 8, "scale: 1, theme: dark");
  EXPECT_NE(pixel(render_ui(light).img, 50, 40), pixel(render_ui(dark).img, 50, 40));
  EXPECT_NE(pixel(render_ui(light).img, 5, 5), pixel(render_ui(dark).img, 5, 5));
}

TEST(placeholders, RoundingMasksTheCorners) {
  const ui_render r = render_ui(image_root(", image: cross, background: \"#FF0000\", rounding: 50", 100, 100));
  const ui_render plain = render_ui("image: {width: 100, height: 100, padding: 0}\nroot: {type: Spring}\n");
  EXPECT_EQ(pixel(r.img, 2, 2), pixel(plain.img, 2, 2)); // outside the circle
  EXPECT_EQ(pixel(r.img, 50, 20), (rgba{255, 0, 0, 255}));
  // Inside the circle, yet inside the corner's triangle: a mask filled as
  // its convex hull would cover it.
  EXPECT_EQ(pixel(r.img, 15, 30), (rgba{255, 0, 0, 255}));
}

TEST(placeholders, ConcaveShapesFillInEitherWinding) {
  // An L shape, wound both ways: the notch (top right) stays empty.
  for (const char* points : {"10,10 40,10 40,60 90,60 90,90 10,90", "10,90 90,90 90,60 40,60 40,10 10,10"}) {
    SCOPED_TRACE(points);
    const svg_image shape =
        parse_svg(std::string("<svg viewBox=\"0 0 100 100\"><polygon points=\"") + points + "\"/></svg>");
    image_options options;
    options.width = 100;
    options.height = 100;
    options.padding = 0;
    const image img = render_frames(options, [&](draw_context&) {
      placeholder_style style;
      style.tint = IM_COL32(0, 0, 0, 255);
      style.background = IM_COL32(255, 255, 255, 255);
      draw_placeholder(ImGui::GetWindowDrawList(), shape, ImVec2(0, 0), ImVec2(100, 100), style);
    });
    EXPECT_EQ(pixel(img, 70, 30), (rgba{255, 255, 255, 255}));
    EXPECT_EQ(pixel(img, 20, 50), (rgba{0, 0, 0, 255}));
  }
}

// ── Image and ImageButton ────────────────────────────────────────────────────

TEST(placeholders, UnknownNamesGetASuggestion) {
  try {
    check_source("root: {type: Image, image: persn}\n");
    FAIL() << "no error";
  } catch (const error& e) {
    EXPECT_NE(std::string(e.what()).find("did you mean 'person'"), std::string::npos) << e.what();
  }
}

TEST(placeholders, AnImageInALayoutTakesItsNaturalSize) {
  const figure f = bind_source(
      "root:\n  type: VerticalLayout\n  children:\n    - {type: Image, image: map}\n    - {type: Spring}\n",
      builtin_registry());
  const auto rects = layout_figure(f);
  const rect r = rects.at(&f.root.children[0]);
  EXPECT_EQ(r.height(), 120);
}

TEST(placeholders, ImagesInWindows) {
  const ui_render r = render_ui(
      "image: {width: 400, height: 500, padding: 0}\n"
      "root:\n"
      "  type: Window\n"
      "  children:\n"
      "    - {type: Image, image: person, width: -1, height: 100, label: Portrait}\n"
      "    - {type: Image, image: person, width: 64, height: 64, rounding: 32}\n"
      "    - {type: ImageButton, label: a, image: icon}\n"
      "    - {type: ImageButton, label: a, image: icon}\n",
      true);
  bool wide = false;
  for (const edit_record& t : r.targets) {
    if (t.field.empty() && t.element->cls->name() == "Image" && t.area.width() > 370) {
      wide = true;
    }
  }
  EXPECT_TRUE(wide); // -1 fills the window's width (less its padding)
}

} // namespace
} // namespace bdg::nymph
