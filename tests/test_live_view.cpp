// MIT License © 2026 Binary Dice Games
/// @file test_live_view.cpp
/// @brief spec/04_web_viewer plan step 3: live_view keeps its contexts,
///        runs frames on demand, sizes the layout to its box, and with no
///        input draws exactly what render_figure() draws.
#include <gtest/gtest.h>
#include <imgui.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include "core/error.hpp"
#include "document/bind.hpp"
#include "elements/register_builtin.hpp"
#include "fonts/embedded_fonts.hpp"
#include "fonts/font_testing.hpp"
#include "render/figure_renderer.hpp"
#include "render/soft_rasterizer.hpp"
#include "view/live_view.hpp"

namespace bdg::nymph {
namespace {

namespace fs = std::filesystem;

const fs::path kSourceDir(NYMPH_SOURCE_DIR);

std::string read_text(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::vector<uint8_t> read_bytes(const fs::path& p) {
  const std::string s = read_text(p);
  return {s.begin(), s.end()};
}

std::vector<fs::path> example_sources() {
  std::vector<fs::path> out;
  for (const fs::directory_entry& e : fs::directory_iterator(kSourceDir / "examples")) {
    if (e.path().extension() == ".yaml") {
      out.push_back(e.path());
    }
  }
  std::sort(out.begin(), out.end());
  return out;
}

/// Runs frames until the view settles, one 60th of a second apart, and
/// rasterizes the last one.
image settle(live_view& v, double& clock) {
  const ImDrawData* last = nullptr;
  for (int i = 0; i < 10 && v.wants_frame(); ++i) {
    last = v.frame(clock);
    clock += 1.0 / 60.0;
  }
  EXPECT_NE(last, nullptr);
  EXPECT_FALSE(v.wants_frame());
  return last == nullptr ? image{} : rasterize(*last, v.textures(), v.clear_color());
}

image settle(live_view& v) {
  double clock = 0.0;
  return settle(v, clock);
}

const char* const kLine =
    "description: one line\n"
    "image: {width: 400, height: 300}\n"
    "root:\n"
    "  type: Plot\n"
    "  title: Line\n"
    "  children:\n"
    "    - {type: PlotLine, label: a, ys: [1, 3, 2, 4]}\n";

const char* const kBars =
    "image: {width: 300, height: 200, theme: dark}\n"
    "root: {type: Plot, title: Bars, children: [{type: PlotBars, label: b, ys: [3, 1, 2]}]}\n";

// ── Static parity ────────────────────────────────────────────────────────────

TEST(live_view, WithNoInputDrawsTheSameBytesAsRenderFigureForEveryExample) {
  ASSERT_GE(example_sources().size(), 20u);
  for (const fs::path& path : example_sources()) {
    SCOPED_TRACE(path.filename().string());
    const std::string source = read_text(path);
    const figure f = bind_source(source, builtin_registry());
    live_view v;
    v.set_source(source);
    v.resize(
        {static_cast<float>(f.image.width), static_cast<float>(f.image.height), static_cast<float>(f.image.scale)});
    const image live = settle(v);
    const image png = render_figure(f);
    ASSERT_EQ(live.width, png.width);
    ASSERT_EQ(live.height, png.height);
    EXPECT_TRUE(live.rgba == png.rgba);
  }
}

// ── Frames on demand ─────────────────────────────────────────────────────────

TEST(live_view, RunsFramesOnlyWhileSomethingChanged) {
  live_view v;
  EXPECT_FALSE(v.wants_frame());
  EXPECT_EQ(v.frame(0.0), nullptr); // no figure yet
  v.set_source(kLine);
  EXPECT_TRUE(v.wants_frame());
  int frames = 0;
  while (v.frame(frames / 60.0) != nullptr) {
    ++frames;
  }
  EXPECT_EQ(frames, kFigureFrames);
  EXPECT_FALSE(v.wants_frame());
  v.resize({400, 300, 1});
  EXPECT_TRUE(v.wants_frame());
  settle(v);
  v.set_theme("dark");
  EXPECT_TRUE(v.wants_frame());
}

TEST(live_view, ABadSourceThrowsAndKeepsThePreviousFigure) {
  live_view v;
  v.set_source(kLine);
  const image before = settle(v);
  try {
    v.set_source("root: {type: Plot, children: [{type: PlotLyne}]}\n");
    FAIL() << "expected nymph::error";
  } catch (const error& e) {
    EXPECT_EQ(e.line(), 1);
  }
  EXPECT_TRUE(v.has_figure());
  EXPECT_EQ(v.description(), "one line");
  EXPECT_EQ(v.source(), kLine);
  v.resize({400, 300, 1});
  EXPECT_TRUE(v.wants_frame());
  EXPECT_TRUE(settle(v).rgba == before.rgba);
}

TEST(live_view, ALayoutThatDoesNotFitItsOwnSizeIsAnError) {
  live_view v;
  EXPECT_THROW(v.set_source("image: {width: 200, height: 100}\nroot: {type: Plot, width: 500, children: []}\n"), error);
  EXPECT_FALSE(v.has_figure());
}

// ── Sizing ───────────────────────────────────────────────────────────────────

TEST(live_view, FitSourceShrinksWithCssInANarrowBox) {
  live_view v;
  v.set_source(kLine);
  v.resize({200, 0, 2});
  const view_geometry g = v.geometry();
  EXPECT_EQ(g.layout_width, 400.0f);
  EXPECT_EQ(g.layout_height, 300.0f);
  EXPECT_FLOAT_EQ(g.css_scale, 0.5f);
  EXPECT_FLOAT_EQ(g.framebuffer_scale, 1.0f); // half size, twice the density
  const image img = settle(v);
  EXPECT_EQ(img.width, 400);
  EXPECT_EQ(img.height, 300);
}

TEST(live_view, FitWidthLaysOutAtTheBoxWidth) {
  live_view v({view_fit::width});
  v.set_source(kLine);
  v.resize({800, 0, 1.5f});
  const view_geometry g = v.geometry();
  EXPECT_EQ(g.layout_width, 800.0f);
  EXPECT_EQ(g.layout_height, 600.0f); // aspect ratio kept
  EXPECT_FLOAT_EQ(g.css_scale, 1.0f);
  EXPECT_EQ(g.pixel_width, 1200);
  EXPECT_FALSE(g.fit_fallback);
  const ImDrawData* d = v.frame(0.0);
  ASSERT_NE(d, nullptr);
  EXPECT_EQ(d->DisplaySize.x, 800.0f);
  EXPECT_EQ(d->FramebufferScale.x, 1.5f);
}

TEST(live_view, FitFillUsesTheWholeBox) {
  live_view v({view_fit::fill});
  v.set_source(kLine);
  v.resize({500, 250, 1});
  EXPECT_EQ(v.geometry().layout_width, 500.0f);
  EXPECT_EQ(v.geometry().layout_height, 250.0f);
}

TEST(live_view, ABoxTooSmallForTheImageOptionsFallsBackToTheSourceSize) {
  live_view v({view_fit::fill});
  v.set_source(kLine);
  v.resize({10, 10, 1});
  const view_geometry g = v.geometry();
  EXPECT_TRUE(g.fit_fallback);
  EXPECT_EQ(g.layout_width, 400.0f);
  EXPECT_FLOAT_EQ(g.css_scale, 10.0f / 400.0f);
}

TEST(live_view, ALayoutOverflowAtTheBoxSizeFallsBackToTheSourceSize) {
  live_view v({view_fit::width});
  v.set_source(
      "image: {width: 700, height: 300}\n"
      "root: {type: HorizontalLayout, children: [{type: Plot, width: 600, children: []}]}\n");
  v.resize({300, 0, 1});
  EXPECT_EQ(v.geometry().layout_width, 300.0f); // tried first
  settle(v);
  const view_geometry g = v.geometry();
  EXPECT_TRUE(g.fit_fallback);
  EXPECT_EQ(g.layout_width, 700.0f);
  EXPECT_FLOAT_EQ(g.css_scale, 300.0f / 700.0f);
  // A wide enough box lays out at the box width again.
  v.resize({900, 0, 1});
  settle(v);
  EXPECT_FALSE(v.geometry().fit_fallback);
  EXPECT_EQ(v.geometry().layout_width, 900.0f);
}

TEST(live_view, ThePixelRatioIsCappedAndTheImageStaysWithinLimits) {
  live_view v;
  v.set_source("image: {width: 4000, height: 100}\nroot: {type: Label, text: wide}\n");
  v.resize({4000, 0, 8});
  EXPECT_LE(v.geometry().pixel_width, 8192);
  EXPECT_FLOAT_EQ(v.geometry().framebuffer_scale, 8192.0f / 4000.0f);
}

// ── Theme ────────────────────────────────────────────────────────────────────

TEST(live_view, SetThemeOverridesTheSourceTheme) {
  live_view v;
  v.set_source(kBars); // dark
  const ImU32 dark = v.clear_color();
  v.set_theme("light");
  EXPECT_NE(v.clear_color(), dark);
  v.set_theme("");
  EXPECT_EQ(v.clear_color(), dark);
  EXPECT_THROW(v.set_theme("neon"), std::invalid_argument);
}

// ── Several views ────────────────────────────────────────────────────────────

TEST(live_view, TwoViewsAlternateWithoutDisturbingEachOtherOrTheCurrentContext) {
  live_view a;
  live_view b;
  a.set_source(kLine);
  b.set_source(kBars);
  a.resize({400, 300, 1});
  b.resize({300, 200, 1});
  const ImDrawData* da = nullptr;
  const ImDrawData* db = nullptr;
  for (int i = 0; i < kFigureFrames; ++i) {
    da = a.frame(i / 60.0);
    db = b.frame(i / 60.0);
    EXPECT_EQ(ImGui::GetCurrentContext(), nullptr);
  }
  ASSERT_NE(da, nullptr);
  ASSERT_NE(db, nullptr);
  EXPECT_TRUE(
      rasterize(*da, a.textures(), a.clear_color()).rgba == render_figure(bind_source(kLine, builtin_registry())).rgba);
  EXPECT_TRUE(
      rasterize(*db, b.textures(), b.clear_color()).rgba == render_figure(bind_source(kBars, builtin_registry())).rgba);
}

TEST(live_view, KeepsTheSameDrawingAfterAnUpdateWithTheSameSource) {
  live_view v;
  v.set_source(kLine);
  const image first = settle(v);
  v.set_source(kLine);
  EXPECT_TRUE(settle(v).rgba == first.rgba);
  v.set_source(kLine, true); // fresh state
  EXPECT_TRUE(settle(v).rgba == first.rgba);
}

// ── Fonts ────────────────────────────────────────────────────────────────────

TEST(live_view, AFontWithoutBytesThrowsFontsNeededUntilRegistered) {
  if (find_font("PT_Serif-Web-Bold.ttf") == nullptr) {
    GTEST_SKIP() << "built with another NYMPH_FONTS_DIR";
  }
  hide_embedded_fonts_for_testing(true);
  live_view v;
  v.set_source(kLine);
  const std::string serif =
      "image: {width: 300, height: 80}\nroot: {type: Label, text: Hi, font: PT_Serif-Web-Bold.ttf}\n";
  try {
    v.set_source(serif);
    FAIL() << "expected fonts_needed";
  } catch (const fonts_needed& e) {
    EXPECT_EQ(e.fonts(), std::vector<std::string>{"PT_Serif-Web-Bold.ttf"});
  }
  EXPECT_EQ(v.source(), kLine);
  register_font("PT_Serif-Web-Bold.ttf", read_bytes(kSourceDir / "fonts/PT_Serif-Web-Bold.ttf"));
  v.set_source(serif);
  v.resize({300, 80, 1});
  const image live = settle(v);
  hide_embedded_fonts_for_testing(false);
  // Drawn in the font, not the built-in one: same pixels as with it embedded.
  EXPECT_TRUE(live.rgba == render_figure(bind_source(serif, builtin_registry())).rgba);
}

} // namespace
} // namespace bdg::nymph
