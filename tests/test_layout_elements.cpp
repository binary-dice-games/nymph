// MIT License © 2026 Binary Dice Games
/// @file test_layout_elements.cpp
/// @brief Plan step 11: layout and text elements, checked through their
///        laid-out rectangles (layout_figure) and the rendered pixels.
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <cmath>
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

using rgba = std::array<uint8_t, 4>;

rgba pixel(const image& img, int x, int y) {
  const uint8_t* p = img.at(x, y);
  return {p[0], p[1], p[2], p[3]};
}

/// A bound figure plus the rectangles of its `figure` nodes.
struct laid_out {
  figure f;
  std::unordered_map<const node*, rect> rects;

  rect of(const node& n) const {
    return rects.at(&n);
  }
};

laid_out lay_out(const std::string& source) {
  laid_out out{bind_source(source, builtin_registry()), {}};
  out.rects = layout_figure(out.f);
  return out;
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

// ── Sizes along the axis ─────────────────────────────────────────────────────

TEST(layout_elements, StretchPlotsShareTheHeight) {
  const laid_out l = lay_out(
      "image: {width: 400, height: 416, padding: 8}\n"
      "root:\n"
      "  type: VerticalLayout\n"
      "  children:\n"
      "    - {type: Plot, height: -1}\n"
      "    - {type: Plot, height: -1}\n");
  const rect a = l.of(l.f.root.children[0]);
  const rect b = l.of(l.f.root.children[1]);
  EXPECT_EQ(a.height(), 200);
  EXPECT_EQ(b.height(), 200);
  EXPECT_EQ(a.min.y, 8);
  EXPECT_EQ(b.min.y, 208);
  EXPECT_EQ(a.width(), 384); // fills across
}

TEST(layout_elements, WeightsSplitOneToThree) {
  const laid_out l = lay_out(
      "image: {width: 416, height: 300, padding: 8}\n"
      "root:\n"
      "  type: HorizontalLayout\n"
      "  children:\n"
      "    - {type: Plot, width: -1, height: -1}\n"
      "    - {type: Plot, width: -3, height: -1}\n");
  EXPECT_EQ(l.of(l.f.root.children[0]).width(), 100);
  EXPECT_EQ(l.of(l.f.root.children[1]).width(), 300);
  EXPECT_EQ(l.of(l.f.root.children[0]).height(), 284);
}

TEST(layout_elements, FixedNaturalAndSpacing) {
  const laid_out l = lay_out(
      "image: {width: 400, height: 500, padding: 0}\n"
      "root:\n"
      "  type: VerticalLayout\n"
      "  spacing: 10\n"
      "  children:\n"
      "    - {type: Plot, height: 120}\n"
      "    - {type: Label, text: caption}\n"
      "    - {type: Plot, height: -1}\n");
  const rect fixed = l.of(l.f.root.children[0]);
  const rect label = l.of(l.f.root.children[1]);
  const rect stretch = l.of(l.f.root.children[2]);
  EXPECT_EQ(fixed.height(), 120);
  EXPECT_EQ(label.min.y, 130);
  EXPECT_GT(label.height(), 10); // one line of the 16 px font
  EXPECT_LT(label.height(), 24);
  EXPECT_EQ(stretch.min.y, label.max.y + 10);
  EXPECT_EQ(stretch.max.y, 500);
}

TEST(layout_elements, SpringsCentreALabel) {
  const laid_out l = lay_out(
      "image: {width: 400, height: 100, padding: 0}\n"
      "root:\n"
      "  type: HorizontalLayout\n"
      "  children:\n"
      "    - {type: Spring}\n"
      "    - {type: Label, text: Quarterly sales}\n"
      "    - {type: Spring}\n");
  const rect label = l.of(l.f.root.children[1]);
  EXPECT_NEAR((label.min.x + label.max.x) / 2, 200, 1.0);
  EXPECT_GT(label.width(), 50);

  // The text pixels lie inside the label's rectangle.
  const image img = render_figure(l.f);
  for (int y = 0; y < img.height; ++y) {
    for (int x = 0; x < img.width; ++x) {
      if (pixel(img, x, y)[0] < 128) { // dark text on the light background
        EXPECT_GE(x, label.min.x - 1);
        EXPECT_LE(x, label.max.x + 1);
      }
    }
  }
}

TEST(layout_elements, ZeroWeightSpringCollapsesAndAlignRight) {
  const laid_out collapsed = lay_out(
      "image: {width: 400, height: 100, padding: 0}\n"
      "root: {type: HorizontalLayout, children: [{type: Spring, weight: 0}, {type: Plot, width: 100}]}\n");
  EXPECT_EQ(collapsed.of(collapsed.f.root.children[1]).min.x, 0);

  const laid_out right = lay_out(
      "image: {width: 400, height: 100, padding: 0}\n"
      "root: {type: HorizontalLayout, align: right, spacing: 10, children: [{type: Plot, width: 100}, {type: Plot, "
      "width: 50}]}\n");
  EXPECT_EQ(right.of(right.f.root.children[0]).min.x, 240);
  EXPECT_EQ(right.of(right.f.root.children[1]).max.x, 400);
}

// ── Text ─────────────────────────────────────────────────────────────────────

TEST(layout_elements, WrappedLabelIsTallerAndFits) {
  const std::string text = "A fairly long caption that cannot fit on one line of a narrow image like this one.";
  const laid_out wrapped = lay_out(
      "image: {width: 200, height: 300, padding: 0}\n"
      "root: {type: VerticalLayout, children: [{type: Label, wrap: true, text: \"" +
      text + "\"}]}\n");
  const laid_out plain = lay_out(
      "image: {width: 200, height: 300, padding: 0}\n"
      "root: {type: VerticalLayout, children: [{type: Label, text: \"" +
      text + "\"}]}\n");
  const rect w = wrapped.of(wrapped.f.root.children[0]);
  const rect p = plain.of(plain.f.root.children[0]);
  EXPECT_GT(w.height(), 2 * p.height());
  EXPECT_LE(w.width(), 200);
}

TEST(layout_elements, TextColoursFollowTheTheme) {
  auto source = [](const char* theme) {
    return std::string("image: {width: 200, height: 40, theme: ") + theme +
        "}\nroot: {type: VerticalLayout, children: [{type: Label, text: WWWWWWW, text_color: \"#0000FF\", "
        "text_color_dark: \"#FF0000\"}]}\n";
  };
  auto count = [](const image& img, rgba c) {
    size_t n = 0;
    for (int y = 0; y < img.height; ++y) {
      for (int x = 0; x < img.width; ++x) {
        n += pixel(img, x, y) == c ? 1 : 0;
      }
    }
    return n;
  };
  const image light = render_figure(bind_source(source("light"), builtin_registry()));
  const image dark = render_figure(bind_source(source("dark"), builtin_registry()));
  EXPECT_GT(count(light, {0, 0, 255, 255}), 20u); // text_color (no text_color_light set)
  EXPECT_EQ(count(light, {255, 0, 0, 255}), 0u);
  EXPECT_GT(count(dark, {255, 0, 0, 255}), 20u); // text_color_dark wins
  EXPECT_EQ(count(dark, {0, 0, 255, 255}), 0u);
}

// ── Card and separators ──────────────────────────────────────────────────────

TEST(layout_elements, CardInsetsItsChildrenByPadding) {
  const laid_out l = lay_out(
      "image: {width: 300, height: 200, padding: 8}\n"
      "root: {type: VerticalLayout, card: true, padding: 16, children: [{type: Plot, height: -1}]}\n");
  const rect card = l.of(l.f.root);
  const rect plot = l.of(l.f.root.children[0]);
  EXPECT_EQ(plot.min.x, card.min.x + 16);
  EXPECT_EQ(plot.min.y, card.min.y + 16);
  EXPECT_EQ(plot.max.x, card.max.x - 16);
  EXPECT_EQ(plot.max.y, card.max.y - 16);

  // The panel differs from the window background between its border and
  // the plot.
  const image img = render_figure(l.f);
  EXPECT_EQ(pixel(img, 2, 100), (rgba{240, 240, 240, 255}));
  EXPECT_NE(pixel(img, 16, 100), (rgba{240, 240, 240, 255}));
}

TEST(layout_elements, SeparatorIsOnePixelRuleAcrossTheLayout) {
  const laid_out l = lay_out(
      "image: {width: 200, height: 60, padding: 0}\n"
      "root: {type: VerticalLayout, children: [{type: Separator}]}\n");
  const rect sep = l.of(l.f.root.children[0]);
  const image img = render_figure(l.f);
  const rgba bg{240, 240, 240, 255};
  int ruled_rows = 0;
  for (int y = 0; y < img.height; ++y) {
    int inked = 0;
    for (int x = 0; x < img.width; ++x) {
      inked += pixel(img, x, y) != bg ? 1 : 0;
    }
    if (inked > 0) {
      EXPECT_EQ(inked, 200) << "row " << y; // all the way across
      EXPECT_GE(y, sep.min.y);
      EXPECT_LT(y, sep.max.y);
      ++ruled_rows;
    }
  }
  EXPECT_EQ(ruled_rows, 1);
}

// ── Errors ───────────────────────────────────────────────────────────────────

TEST(layout_elements, OverflowIsAPositionedError) {
  const std::string source =
      "image: {width: 400, height: 300}\n"
      "root:\n"
      "  type: VerticalLayout\n"
      "  spacing: 10\n"
      "  children:\n"
      "    - {type: Plot, height: 200}\n"
      "    - {type: Plot, height: 200}\n";
  const error e = source_error(source);
  EXPECT_EQ(e.line(), 3);
  EXPECT_EQ(e.column(), 3);
  EXPECT_NE(std::string(e.what()).find("VerticalLayout needs 410 px of height, has 284"), std::string::npos)
      << e.what();
  // render_png reports the same error.
  try {
    render_png(source);
    ADD_FAILURE() << "render_png did not throw";
  } catch (const error& r) {
    EXPECT_STREQ(r.what(), e.what());
  }
}

TEST(layout_elements, RootLargerThanTheImageIsAnError) {
  const error e = source_error("image: {width: 300, height: 200}\nroot: {type: Plot, height: 600}\n");
  EXPECT_NE(std::string(e.what()).find("Plot needs 600 px of height, has 184"), std::string::npos) << e.what();
}

TEST(layout_elements, SeriesAreNotLayoutChildren) {
  const error e = source_error("root: {type: VerticalLayout, children: [{type: PlotLine, ys: [1]}]}\n");
  EXPECT_NE(std::string(e.what()).find("cannot be placed in VerticalLayout; it goes inside: Plot"), std::string::npos)
      << e.what();
}

// ── Pinned schema ──────────────────────────────────────────────────

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
      def = f.names != nullptr ? f.names->format_enum(*i) : std::to_string(*i);
    } else if (const auto* d = std::get_if<double>(&f.default_value)) {
      std::ostringstream os;
      os << *d;
      def = os.str();
    } else if (const auto* s = std::get_if<std::string>(&f.default_value)) {
      def = "'" + *s + "'";
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

TEST(layout_elements, SchemaIsPinned) {
  // Started from wish's src/ui/ui_elements/{layout,spring,label,
  // separator}.cpp (without `scroll`, which needs a live window), plus
  // nymph's text style fields.
  const std::vector<std::string> layout{
      "spacing=0",
      "width=0",
      "height=0",
      "card=false",
      "padding=12",
      "rounding=10",
      "font=''",
      "font_size=0",
      "text_color=''"};
  std::vector<std::string> horizontal = layout;
  horizontal.push_back("align=left");
  EXPECT_EQ(schema_of("VerticalLayout"), sorted(layout));
  EXPECT_EQ(schema_of("HorizontalLayout"), sorted(horizontal));
  EXPECT_EQ(schema_of("Spring"), (std::vector<std::string>{"weight=1"}));
  EXPECT_EQ(
      schema_of("Label"),
      sorted(
          {"text=''",
           "text_color=''",
           "text_color_light=''",
           "text_color_dark=''",
           "wrap=false",
           "font=''",
           "font_size=0"}));
  EXPECT_TRUE(schema_of("Separator").empty());
  EXPECT_EQ(schema_of("SeparatorText"), sorted({"label=''", "font=''", "font_size=0", "text_color=''"}));
}

// ── Samples ──────────────────────────────────────────────────────────────────

TEST(layout_elements, EverySampleRenders) {
  const fs::path dir = fs::path(NYMPH_SOURCE_DIR) / "tests" / "data" / "layout";
  size_t count = 0;
  for (const auto& entry : fs::directory_iterator(dir)) {
    if (entry.path().extension() != ".yaml") {
      continue;
    }
    SCOPED_TRACE(entry.path().filename().string());
    std::ifstream in(entry.path(), std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    const std::vector<uint8_t> png = render_png(ss.str());
    EXPECT_EQ(extract_source(png).value_or(""), ss.str());
    ++count;
  }
  EXPECT_EQ(count, 5u);
}

} // namespace
} // namespace bdg::nymph
