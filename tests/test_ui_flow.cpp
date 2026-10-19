// MIT License © 2026 Binary Dice Games
/// @file test_ui_flow.cpp
/// @brief spec/09_ui_mockups PLAN step 3: WindowTransition arrows between the
///        windows of a Screen.
#include <gtest/gtest.h>
#include <string>
#include "core/error.hpp"
#include "live_view_driver.hpp"
#include "nymph.hpp"
#include "ui_test_util.hpp"

namespace bdg::nymph {
namespace {

using test::pixel;
using test::render_ui;
using test::rgba;
using test::ui_render;

/// Two windows side by side, centres on y = 150, and `extra` children.
std::string two_windows(const std::string& extra, const std::string& b_fields = "") {
  return "image: {width: 640, height: 300, padding: 0}\n"
         "root:\n"
         "  type: Screen\n"
         "  children:\n"
         "    - {type: Window, id: a, title: A, x: 20, y: 100, width: 200, height: 100}\n"
         "    - {type: Window, id: b, title: B, x: 420, y: 100, width: 200, height: 100" +
      b_fields + "}\n" + extra;
}

/// The label target of the element at `path`.
rect label_of(const ui_render& r, const std::string& path) {
  for (const edit_record& t : r.targets) {
    if (t.field == "label" && r.paths.at(t.element) == path) {
      return t.area;
    }
  }
  ADD_FAILURE() << "no label target for " << path;
  return {};
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

TEST(ui_flow, AnArrowRunsFromEdgeToEdge) {
  const ui_render plain = render_ui(two_windows(""));
  const ui_render arrow = render_ui(two_windows("    - {type: WindowTransition, from: a, to: b}\n"));
  const rgba background = pixel(plain.img, 300, 150);
  // Just outside both windows, on the centre line: the line.
  EXPECT_NE(pixel(arrow.img, 230, 150), background);
  EXPECT_NE(pixel(arrow.img, 410, 150), background);
  // Nothing drawn inside either window.
  for (int y = 100; y < 200; y += 5) {
    for (const int x : {30, 120, 210, 430, 520, 610}) {
      ASSERT_EQ(pixel(arrow.img, x, y), pixel(plain.img, x, y)) << x << ", " << y;
    }
  }
}

TEST(ui_flow, TheArrowheadIsAtTo) {
  const ui_render plain = render_ui(two_windows(""));
  const ui_render arrow = render_ui(two_windows("    - {type: WindowTransition, from: a, to: b}\n"));
  const rgba background = pixel(plain.img, 300, 150);
  // The head is wider than the line: 3 px off the line it is drawn near b,
  // not near a.
  EXPECT_NE(pixel(arrow.img, 410, 153), background);
  EXPECT_EQ(pixel(arrow.img, 230, 153), background);
}

TEST(ui_flow, TransitionsBetweenOnePairDoNotOverlap) {
  const ui_render r = render_ui(
      two_windows(
          "    - {type: WindowTransition, from: a, to: b, label: ok}\n"
          "    - {type: WindowTransition, from: b, to: a, label: cancel}\n"
          "    - {type: WindowTransition, from: a, to: b, label: retry}\n"),
      true);
  const rect ok = label_of(r, "root/children/2");
  const rect cancel = label_of(r, "root/children/3");
  const rect retry = label_of(r, "root/children/4");
  const auto disjoint = [](const rect& p, const rect& q) {
    return p.max.x <= q.min.x || q.max.x <= p.min.x || p.max.y <= q.min.y || q.max.y <= p.min.y;
  };
  EXPECT_TRUE(disjoint(ok, cancel));
  EXPECT_TRUE(disjoint(ok, retry));
  EXPECT_TRUE(disjoint(cancel, retry));
}

TEST(ui_flow, LabelsStayClearOfTheWindows) {
  // A diagonal arrow whose middle is next to b: centred there, the label
  // would cover b's left edge; it slides towards a instead.
  const ui_render r = render_ui(
      "image: {width: 640, height: 320, padding: 0}\n"
      "root:\n"
      "  type: Screen\n"
      "  children:\n"
      "    - {type: Window, id: a, title: A, x: 20, y: 20, width: 200, height: 80}\n"
      "    - {type: Window, id: b, title: B, x: 300, y: 100, width: 200, height: 200}\n"
      "    - {type: WindowTransition, from: a, to: b, label: finished loading}\n",
      true);
  const rect label = label_of(r, "root/children/2");
  const auto clear_of = [&](float x0, float y0, float x1, float y1) {
    return label.max.x <= x0 || label.min.x >= x1 || label.max.y <= y0 || label.min.y >= y1;
  };
  EXPECT_TRUE(clear_of(20, 20, 220, 100)) << label.min.x << ", " << label.min.y;
  EXPECT_TRUE(clear_of(300, 100, 500, 300)) << label.max.x << ", " << label.max.y;
}

TEST(ui_flow, ASelfTransitionLoopsOnTheSideWithRoom) {
  const ui_render r = render_ui(
      "image: {width: 640, height: 300, padding: 0}\n"
      "root:\n"
      "  type: Screen\n"
      "  children:\n"
      "    - {type: Window, id: a, title: A, x: 20, y: 100, width: 200, height: 100}\n"
      "    - {type: WindowTransition, from: a, to: a, label: refresh}\n",
      true);
  EXPECT_GT(label_of(r, "root/children/1").min.x, 220); // right of the window
  const ui_render left = render_ui(
      "image: {width: 640, height: 300, padding: 0}\n"
      "root:\n"
      "  type: Screen\n"
      "  children:\n"
      "    - {type: Window, id: a, title: A, x: 420, y: 100, width: 200, height: 100}\n"
      "    - {type: WindowTransition, from: a, to: a, label: refresh}\n",
      true);
  EXPECT_LT(label_of(left, "root/children/1").max.x, 420); // no room on the right
}

TEST(ui_flow, UnknownIdsAreErrorsWithASuggestion) {
  const error e = source_error(
      "root:\n"
      "  type: Screen\n"
      "  children:\n"
      "    - {type: Window, id: login, title: A, x: 10, y: 10, width: 100, height: 80}\n"
      "    - {type: WindowTransition, from: login, to: logn}\n");
  EXPECT_EQ(e.line(), 5);
  EXPECT_NE(std::string(e.what()).find("WindowTransition.to names no Window id 'logn' (did you mean 'login'?)"),
            std::string::npos)
      << e.what();
}

TEST(ui_flow, WindowIdsAreUnique) {
  const error e = source_error(
      "root:\n"
      "  type: Screen\n"
      "  children:\n"
      "    - {type: Window, id: a, x: 10, y: 10, width: 100, height: 80}\n"
      "    - {type: Window, id: a, x: 200, y: 10, width: 100, height: 80}\n");
  EXPECT_EQ(e.line(), 5);
  EXPECT_NE(std::string(e.what()).find("Window.id 'a' is used by another Window"), std::string::npos) << e.what();
}

TEST(ui_flow, OnlyAScreenTakesTransitions) {
  const error e = source_error(
      "root:\n"
      "  type: VerticalLayout\n"
      "  children:\n"
      "    - {type: WindowTransition, from: a, to: b}\n");
  EXPECT_NE(std::string(e.what()).find("it goes inside: Screen"), std::string::npos) << e.what();
}

// ── Live views ───────────────────────────────────────────────────────────────

TEST(ui_flow, TheLabelIsAPickAndFollowsItsWindow) {
  const std::string source =
      two_windows("    - {type: WindowTransition, from: a, to: b, label: ok}\n", ", closable: true");
  const rect label = label_of(render_ui(source, true), "root/children/2");
  const ImVec2 centre((label.min.x + label.max.x) / 2, (label.min.y + label.max.y) / 2);
  driven_view d(source, 640, 300);
  d.move(centre.x, centre.y);
  ASSERT_TRUE(d.v.hovered());
  EXPECT_EQ(d.v.hovered()->type, "WindowTransition");
  EXPECT_EQ(d.v.hovered()->label, "ok");
  EXPECT_EQ(d.v.hovered()->element, "root/children/2");

  // Drag b's title bar down by 80: the arrow, and its label, move.
  d.drag(520, 110, 520, 190);
  d.move(centre.x, centre.y);
  EXPECT_FALSE(d.v.hovered() && d.v.hovered()->type == "WindowTransition");
  d.move(centre.x, centre.y + 40);
  ASSERT_TRUE(d.v.hovered());
  EXPECT_EQ(d.v.hovered()->label, "ok");
}

TEST(ui_flow, AClosedWindowLosesItsArrows) {
  const std::string source =
      two_windows("    - {type: WindowTransition, from: a, to: b, label: ok}\n", ", closable: true");
  const rect label = label_of(render_ui(source, true), "root/children/2");
  const ImVec2 centre((label.min.x + label.max.x) / 2, (label.min.y + label.max.y) / 2);
  driven_view d(source, 640, 300);
  // b's close button: the right end of its title bar.
  d.click(620 - 14, 100 + 10);
  d.move(centre.x, centre.y);
  EXPECT_FALSE(d.v.hovered());
}

} // namespace
} // namespace bdg::nymph
