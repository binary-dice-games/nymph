// MIT License © 2026 Binary Dice Games
/// @file test_fonts.cpp
/// @brief Plan step 17: fonts embedded from `fonts/`, and the font,
///        font_size and text_color text style, inherited down the tree.
#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "core/error.hpp"
#include "document/bind.hpp"
#include "elements/register_builtin.hpp"
#include "fonts/embedded_fonts.hpp"
#include "render/figure_renderer.hpp"

namespace bdg::nymph {
namespace {

namespace fs = std::filesystem;

const fs::path kFontsDir(NYMPH_FONTS_DIR);

/// The tests below name fonts of nymph's own `fonts/` folder; a build with
/// another NYMPH_FONTS_DIR skips them.
#define NYMPH_REQUIRE_STANDARD_FONTS()                                                                 \
  if (find_font("FiraSans-Regular.ttf") == nullptr || find_font("PT_Serif-Web-Bold.ttf") == nullptr) { \
    GTEST_SKIP() << "built with another NYMPH_FONTS_DIR";                                              \
  }

std::string read_file(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

/// File names in the fonts folder with one of `extensions`, sorted.
std::vector<std::string> files_in_fonts_dir(const std::vector<std::string>& extensions) {
  std::vector<std::string> out;
  for (const fs::directory_entry& e : fs::directory_iterator(kFontsDir)) {
    if (std::find(extensions.begin(), extensions.end(), e.path().extension().string()) != extensions.end()) {
      out.push_back(e.path().filename().string());
    }
  }
  std::sort(out.begin(), out.end());
  return out;
}

/// Width of a one-label row: the label's natural width, i.e. its text.
float label_width(const std::string& label_fields, const std::string& image_fields = "") {
  const figure f = bind_source(
      "image: {width: 800, height: 200" + image_fields + "}\n" +
          "root: {type: HorizontalLayout, children: [{type: Label, text: Hello nymph" + label_fields + "}]}\n",
      builtin_registry());
  return layout_figure(f).at(&f.root.children[0]).width();
}

/// Pixels of `img` within 40 of `rgb` in every channel.
size_t pixels_near(const image& img, int r, int g, int b) {
  size_t n = 0;
  for (size_t i = 0; i + 3 < img.rgba.size(); i += 4) {
    if (std::abs(img.rgba[i] - r) < 40 && std::abs(img.rgba[i + 1] - g) < 40 && std::abs(img.rgba[i + 2] - b) < 40) {
      ++n;
    }
  }
  return n;
}

image render(const std::string& source) {
  return render_figure(bind_source(source, builtin_registry()));
}

// ── Embedding ────────────────────────────────────────────────────────────────

TEST(fonts, EveryFileInTheFontsFolderIsEmbeddedByteForByte) {
  std::vector<std::string> names;
  for (const embedded_file& f : embedded_fonts()) {
    names.emplace_back(f.name);
    EXPECT_EQ(f.text(), read_file(kFontsDir / f.name)) << f.name;
    EXPECT_EQ(f.data[f.size], 0) << f.name; // terminator
    EXPECT_EQ(find_font(f.name), &f);
  }
  EXPECT_EQ(names, files_in_fonts_dir({".ttf", ".otf"}));

  std::vector<std::string> licenses;
  for (const embedded_file& f : embedded_licenses()) {
    licenses.emplace_back(f.name);
    EXPECT_EQ(f.text(), read_file(kFontsDir / f.name)) << f.name;
  }
  EXPECT_EQ(licenses, files_in_fonts_dir({".txt"}));
  EXPECT_EQ(find_font("nope.ttf"), nullptr);
}

TEST(fonts, EveryEmbeddedFontLoadsAndDrawsText) {
  const float builtin = label_width("");
  for (const embedded_file& f : embedded_fonts()) {
    SCOPED_TRACE(f.name);
    const float w = label_width(std::string(", font: ") + f.name);
    EXPECT_GT(w, 20.0f);
    EXPECT_NE(w, builtin); // its own metrics, not the built-in font's
    const image img = render(
        std::string(
            "image: {width: 200, height: 40}\nroot: {type: Label, text: Hello, text_color: \"#FF0000\", font: ") +
        f.name + "}\n");
    EXPECT_GT(pixels_near(img, 255, 0, 0), 10u); // glyphs were drawn
  }
}

// ── Text style ───────────────────────────────────────────────────────────────

TEST(fonts, FontSizeScalesText) {
  NYMPH_REQUIRE_STANDARD_FONTS();
  const float base = label_width(", font: FiraSans-Regular.ttf, font_size: 16");
  const float twice = label_width(", font: FiraSans-Regular.ttf, font_size: 32");
  EXPECT_NEAR(twice / base, 2.0f, 0.1f);
}

TEST(fonts, StyleIsInheritedFromLayoutsAndTheImage) {
  NYMPH_REQUIRE_STANDARD_FONTS();
  const float direct = label_width(", font: PT_Serif-Web-Bold.ttf, font_size: 22");
  EXPECT_FLOAT_EQ(label_width("", ", font: PT_Serif-Web-Bold.ttf, font_size: 22"), direct);
  const figure f = bind_source(
      "image: {width: 800, height: 200}\n"
      "root:\n"
      "  type: HorizontalLayout\n"
      "  font: PT_Serif-Web-Bold.ttf\n"
      "  children:\n"
      "    - type: HorizontalLayout\n"
      "      font_size: 22\n"
      "      children: [{type: Label, text: Hello nymph}]\n",
      builtin_registry());
  EXPECT_FLOAT_EQ(layout_figure(f).at(&f.root.children[0].children[0]).width(), direct);
  // An element overrides what it inherits; "default" is the built-in font.
  EXPECT_FLOAT_EQ(label_width(", font: default", ", font: FiraSans-Regular.ttf"), label_width(""));
}

TEST(fonts, TextColorReachesLabelsAndPlotText) {
  const std::string plot =
      "image: {width: 300, height: 200}\n"
      "root: {type: Plot, title: Big Title, flags: NoLegend, children: [{type: PlotLine, ys: [1, 2]}]}\n";
  EXPECT_EQ(pixels_near(render(plot), 255, 0, 0), 0u);
  std::string red = plot;
  red.replace(red.find("flags:"), 0, "text_color: \"#FF0000\", ");
  EXPECT_GT(pixels_near(render(red), 255, 0, 0), 30u); // title and tick labels

  std::string image_red = plot;
  image_red.replace(image_red.find("height: 200"), 11, "height: 200, text_color: \"#FF0000\"");
  EXPECT_EQ(render(image_red).rgba, render(red).rgba); // same style from the image
}

// ── Errors ───────────────────────────────────────────────────────────────────

TEST(fonts, UnknownFontIsAnErrorWithASuggestion) {
  NYMPH_REQUIRE_STANDARD_FONTS();
  try {
    bind_source("root: {type: Label, text: x, font: FiraSans-Regular.tff}\n", builtin_registry());
    FAIL() << "no error";
  } catch (const error& e) {
    EXPECT_EQ(e.line(), 1);
    EXPECT_EQ(e.column(), 36);
    const std::string m = e.what();
    EXPECT_EQ(
        m.rfind(
            "Label.font: unknown font 'FiraSans-Regular.tff' (did you mean 'FiraSans-Regular.ttf'?) (fonts: default, ",
            0),
        0u)
        << m;
  }
  try {
    bind_source("image: {font: Arial.ttf}\nroot: {type: Plot}\n", builtin_registry());
    FAIL() << "no error";
  } catch (const error& e) {
    EXPECT_NE(std::string(e.what()).find("image.font: unknown font 'Arial.ttf'"), std::string::npos) << e.what();
  }
  EXPECT_THROW(bind_source("image: {font_size: 200}\nroot: {type: Plot}\n", builtin_registry()), error);
  EXPECT_THROW(bind_source("root: {type: Plot, font_size: -3}\n", builtin_registry()), error);
}

} // namespace
} // namespace bdg::nymph
