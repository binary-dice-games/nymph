// MIT License © 2026 Binary Dice Games
/// @file test_saved_view.cpp
/// @brief The `view` key (spec/04_web_viewer/DESIGN.md section 3, "Saved
///        view"): parsing, writing, PNGs that honour it, and a live view
///        that saves what the user did and reopens it.
#include <gtest/gtest.h>
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include "core/error.hpp"
#include "document/bind.hpp"
#include "document/view.hpp"
#include "elements/register_builtin.hpp"
#include "live_view_driver.hpp"
#include "nymph.hpp"
#include "render/figure_renderer.hpp"
#include "render/soft_rasterizer.hpp"

namespace bdg::nymph {
namespace {

const std::string kTwoPlots =
    "image: {width: 400, height: 400}\n"
    "root:\n"
    "  type: VerticalLayout\n"
    "  children:\n"
    "    - type: Plot\n"
    "      title: A\n"
    "      height: -1\n"
    "      x_min: 0\n"
    "      x_max: 10\n"
    "      y_min: 0\n"
    "      y_max: 10\n"
    "      children:\n"
    "        - {type: PlotLine, label: hide me, xs: [0, 10], ys: [0, 10], color: \"#FF0000\", line_weight: 3}\n"
    "        - {type: PlotLine, label: keep, xs: [0, 10], ys: [10, 0]}\n"
    "    - {type: Plot, title: B, height: -1, children: [{type: PlotBars, label: b, ys: [1, 2]}]}\n";

const std::string kDiagram =
    "image: {width: 400, height: 300}\n"
    "root:\n"
    "  type: ClassDiagram\n"
    "  children:\n"
    "    - {type: Class, id: Animal}\n"
    "    - {type: Class, id: Duck}\n"
    "    - {type: ClassRelation, from: Duck, to: Animal, kind: Inheritance}\n";

const std::string k3D =
    "image: {width: 400, height: 300}\n"
    "root:\n"
    "  type: Plot3D\n"
    "  title: S\n"
    "  children:\n"
    "    - {type: Plot3DScatter, label: p, xs: [0, 1, 0, 1], ys: [0, 0, 1, 1], zs: [0, 1, 1, 0]}\n";

const std::string kGantt =
    "image: {width: 400, height: 300}\n"
    "root:\n"
    "  type: GanttChart\n"
    "  children:\n"
    "    - type: GanttSection\n"
    "      name: red\n"
    "      color: \"#FF0000\"\n"
    "      children:\n"
    "        - {type: GanttTask, id: a, name: A, start: 0, duration: 4}\n"
    "        - {type: GanttTask, id: b, name: B, start: 4, duration: 4}\n"
    "    - type: GanttSection\n"
    "      name: other\n"
    "      children:\n"
    "        - {type: GanttTask, id: c, name: C, after: [b], duration: 4}\n";

figure bound(const std::string& source) {
  return bind_source(source, builtin_registry());
}

image png_pixels(const std::string& source) {
  return render_figure(bound(source));
}

/// Pixels close to pure red.
size_t red_pixels(const image& img) {
  size_t n = 0;
  for (size_t i = 0; i + 3 < img.rgba.size(); i += 4) {
    n += img.rgba[i] > 200 && img.rgba[i + 1] < 60 && img.rgba[i + 2] < 60 ? 1 : 0;
  }
  return n;
}

// ── Parsing and writing ──────────────────────────────────────────────────────

TEST(saved_view, BindsTheEntriesOfViewableElements) {
  const figure f = bound(
      kTwoPlots +
      "view:\n"
      "  root/children/0: {x: [2, 4], y: [1, 9], hidden: [hide me]}\n"
      "  root/children/9: {x: [0, 1]}\n" // no such element: ignored
      "  root: {zoom: 2}\n"); // a layout has no view: ignored
  ASSERT_EQ(f.views.size(), 1u);
  const element_view& v = f.views.at("root/children/0");
  EXPECT_EQ(v.x, (view_range{2, 4}));
  EXPECT_EQ(v.y, (view_range{1, 9}));
  EXPECT_EQ(v.hidden, std::vector<std::string>{"hide me"});
}

TEST(saved_view, MalformedEntriesArePositionedErrors) {
  const auto message = [](const std::string& entry) {
    try {
      bound(kDiagram + "view:\n  root: " + entry + "\n");
    } catch (const error& e) {
      EXPECT_EQ(e.line(), 9) << e.what();
      return std::string(e.what());
    }
    return std::string("no error");
  };
  EXPECT_NE(message("{zom: 2}").find("unknown key 'zom' (did you mean 'zoom'?) for ClassDiagram"), std::string::npos);
  EXPECT_NE(message("{x: [0, 1]}").find("(keys: zoom, center)"), std::string::npos);
  EXPECT_NE(message("{zoom: 9}").find("between 0.25 and 8"), std::string::npos);
  EXPECT_NE(message("{center: [1]}").find("a list of 2 numbers"), std::string::npos);
  EXPECT_NE(
      [&] {
        try {
          bound(kTwoPlots + "view:\n  root/children/0: {x: [4, 2]}\n");
        } catch (const error& e) {
          return std::string(e.what());
        }
        return std::string();
      }()
          .find("min < max"),
      std::string::npos);
}

TEST(saved_view, WritingAndReadingBackGivesTheSameViews) {
  view_map views;
  views["root/children/0"].x = view_range{0.5, 3.25};
  views["root/children/0"].hidden = {"hide me", "a \"quoted\" label"};
  views["root/children/1"].y = view_range{-1e-7, 2e9};
  const std::string text = write_view(views);
  EXPECT_EQ(
      text.substr(0, text.find('\n')),
      "root/children/0: {x: [0.5, 3.25], hidden: [\"hide me\", \"a \\\"quoted\\\" label\"]}");
  const figure f = bound(kTwoPlots + "view:\n" + [&] {
    std::string indented;
    size_t at = 0;
    while (at < text.size()) {
      const size_t nl = text.find('\n', at);
      indented += "  " + text.substr(at, nl - at + 1);
      at = nl + 1;
    }
    return indented;
  }());
  EXPECT_EQ(f.views, views);
  EXPECT_EQ(write_view({}), "");
}

TEST(saved_view, AnEmptyValueRemovesATopLevelKey) {
  EXPECT_EQ(
      replace_top_level("root: {type: Label, text: a}\nview:\n  root: {}\ndata: {}\n", "view", "").source,
      "root: {type: Label, text: a}\ndata: {}\n");
}

// ── PNGs honour the view ─────────────────────────────────────────────────────

TEST(saved_view, APngShowsTheSavedRangesAndHiddenSeries) {
  const image plain = png_pixels(kTwoPlots);
  const image hidden = png_pixels(kTwoPlots + "view:\n  root/children/0: {hidden: [hide me]}\n");
  EXPECT_GT(red_pixels(plain), 200u);
  EXPECT_EQ(red_pixels(hidden), 0u); // the legend swatch of a hidden entry is greyed
  const image zoomed = png_pixels(kTwoPlots + "view:\n  root/children/0: {x: [4, 6], y: [4, 6]}\n");
  EXPECT_FALSE(zoomed.rgba == plain.rgba);
}

TEST(saved_view, AHiddenGanttSectionDropsItsRows) {
  EXPECT_GT(red_pixels(png_pixels(kGantt)), 200u);
  const std::string hidden = kGantt + "view:\n  root: {hidden: [red]}\n";
  EXPECT_EQ(red_pixels(png_pixels(hidden)), 0u);
  // The rows of the hidden section close up: one row is left.
  driven_view d(hidden, 400, 300);
  EXPECT_NEAR(std::abs(d.plot().y_max - d.plot().y_min), 1.0, 1e-6);
  EXPECT_EQ(d.v.view_state()[0].hidden, std::vector<std::string>{"red"});
  EXPECT_EQ(d.v.source_with_view(), hidden); // nothing new
}

TEST(saved_view, ALiveViewOfASavedViewDrawsLikeItsPng) {
  for (const std::string& source :
       {kTwoPlots + "view:\n  root/children/0: {x: [4, 6], y: [4, 6], hidden: [keep]}\n",
        kDiagram + "view:\n  root: {zoom: 2, center: [40, 30]}\n",
        k3D + "view:\n  root: {rotation: [0.1, 0.2, 0.3, 0.9], x: [-1, 2]}\n"}) {
    SCOPED_TRACE(source);
    const figure f = bound(source);
    live_view v;
    v.set_source(source);
    v.resize({static_cast<float>(f.image.width), static_cast<float>(f.image.height), 1});
    double clock = 0;
    const ImDrawData* last = nullptr;
    while (v.wants_frame()) {
      last = v.frame(clock += 1.0 / 60);
    }
    ASSERT_NE(last, nullptr);
    EXPECT_TRUE(rasterize(*last, v.textures(), v.clear_color()).rgba == render_figure(f).rgba);
  }
}

// ── A live view saves what the user did ──────────────────────────────────────

TEST(saved_view, NothingChangedGivesTheSourceAsWritten) {
  const std::string with_view = kTwoPlots + "view:\n  root/children/0: {x: [1, 2]}  # kept as written\n";
  driven_view d(with_view, 400, 400);
  EXPECT_EQ(d.v.source_with_view(), with_view);
  // New data for an untouched chart saves no view either.
  driven_view e(kTwoPlots, 400, 400);
  std::string more = kTwoPlots;
  more.replace(more.find("ys: [1, 2]"), 10, "ys: [5, 6, 7]");
  e.v.set_source(more);
  e.run();
  EXPECT_EQ(e.v.source_with_view(), more);
}

TEST(saved_view, APanIsSavedForThatPlotOnlyAndReopensTheSame) {
  driven_view d(kTwoPlots, 400, 400);
  const plot_view a = d.plot(0);
  d.drag(a.x + 150, a.y + 50, a.x + 100, a.y + 50);
  const plot_view moved = d.plot(0);
  const std::string saved = d.v.source_with_view();
  EXPECT_NE(saved.find("view:\n  root/children/0: {x: ["), std::string::npos) << saved;
  EXPECT_EQ(saved.find("root/children/1"), std::string::npos) << saved; // untouched
  EXPECT_EQ(saved.find("hidden"), std::string::npos);
  driven_view reopened(saved, 400, 400);
  const plot_view r = reopened.plot(0);
  EXPECT_NEAR(r.x_min, moved.x_min, 1e-5 * 10);
  EXPECT_NEAR(r.x_max, moved.x_max, 1e-5 * 10);
  EXPECT_NEAR(r.y_min, moved.y_min, 1e-5 * 10);
  // The PNG of the saved source shows the same range.
  EXPECT_FALSE(png_pixels(saved).rgba == png_pixels(kTwoPlots).rgba);
}

TEST(saved_view, AHiddenSeriesIsSavedAndStaysHidden) {
  driven_view d(kTwoPlots, 400, 400);
  const plot_view a = d.plot(0);
  d.click(a.x + 25, a.y + 23); // the first legend entry (north-west)
  const std::string saved = d.v.source_with_view();
  EXPECT_NE(saved.find("hidden: [\"hide me\"]"), std::string::npos) << saved;
  EXPECT_EQ(red_pixels(png_pixels(saved)), 0u);
  driven_view reopened(saved, 400, 400);
  EXPECT_EQ(reopened.v.view_state()[0].hidden, std::vector<std::string>{"hide me"});
  EXPECT_EQ(reopened.v.source_with_view(), saved); // nothing new
}

TEST(saved_view, ADiagramZoomIsSavedAndReopensTheSame) {
  driven_view d(kDiagram);
  d.wheel(150, 120, 2);
  const plot_view z = d.plot();
  ASSERT_EQ(z.kind, "diagram");
  EXPECT_NEAR(z.zoom, 1.44, 1e-4);
  const std::string saved = d.v.source_with_view();
  EXPECT_NE(saved.find("view:\n  root: {zoom: 1.44, center: ["), std::string::npos) << saved;
  driven_view reopened(saved);
  EXPECT_NEAR(reopened.plot().zoom, z.zoom, 1e-4);
  EXPECT_NEAR(reopened.plot().center[0], z.center[0], 0.01);
  EXPECT_NEAR(reopened.plot().center[1], z.center[1], 0.01);
}

TEST(saved_view, AZoomedDiagramPassesUnderItsTitle) {
  std::string titled = kDiagram;
  titled.replace(titled.find("  children:"), 0, "  title: Zoo\n");
  const image plain = png_pixels(titled);
  const image zoomed = png_pixels(titled + "view:\n  root: {zoom: 3, center: [40, 60]}\n");
  ASSERT_EQ(plain.width, zoomed.width);
  EXPECT_FALSE(zoomed.rgba == plain.rgba);
  // The rows of the title look the same: the zoomed content is clipped below it.
  const size_t band = static_cast<size_t>(plain.width) * 18 * 4;
  EXPECT_TRUE(std::equal(plain.rgba.begin(), plain.rgba.begin() + band, zoomed.rgba.begin()));
}

TEST(saved_view, A3DRotationIsSavedAndReopensTheSame) {
  driven_view d(k3D);
  d.drag(200, 150, 260, 170);
  const plot_view r = d.plot();
  ASSERT_EQ(r.kind, "plot3d");
  const std::string saved = d.v.source_with_view();
  EXPECT_NE(saved.find("rotation: ["), std::string::npos) << saved;
  driven_view reopened(saved);
  for (int i = 0; i < 4; ++i) {
    EXPECT_NEAR(reopened.plot().rotation[i], r.rotation[i], 1e-5);
  }
}

TEST(saved_view, ResetViewForgetsWhatTheUserDid) {
  driven_view d(kDiagram);
  d.wheel(150, 120, 2);
  ASSERT_NE(d.v.source_with_view(), kDiagram);
  d.v.reset_view();
  d.run();
  EXPECT_EQ(d.v.source_with_view(), kDiagram);
}

TEST(saved_view, ANewSourceWithAnotherViewShowsThatView) {
  driven_view d(kTwoPlots, 400, 400);
  const plot_view a = d.plot(0);
  d.drag(a.x + 150, a.y + 50, a.x + 100, a.y + 50); // the user pans
  d.v.set_source(kTwoPlots + "view:\n  root/children/0: {x: [3, 5]}\n"); // the agent sends a view
  d.run();
  EXPECT_DOUBLE_EQ(d.plot(0).x_min, 3.0);
  EXPECT_DOUBLE_EQ(d.plot(0).x_max, 5.0);
}

} // namespace
} // namespace bdg::nymph
