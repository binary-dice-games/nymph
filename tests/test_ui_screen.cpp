// MIT License © 2026 Binary Dice Games
/// @file test_ui_screen.cpp
/// @brief spec/09_ui_mockups PLAN step 2: Screen, Window and the first
///        widgets render to a deterministic PNG; IDs never collide.
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include "core/error.hpp"
#include "nymph.hpp"
#include "ui_test_util.hpp"

namespace bdg::nymph {
namespace {

using test::render_ui;
using test::ui_render;

const char* const kTwoWindows =
    "image: {width: 640, height: 360, padding: 0}\n"
    "root:\n"
    "  type: Screen\n"
    "  children:\n"
    "    - type: Window\n"
    "      title: Scene\n"
    "      x: 16\n"
    "      y: 16\n"
    "      width: 260\n"
    "      height: 200\n"
    "      children:\n"
    "        - {type: Text, text: Hello}\n"
    "        - {type: Checkbox, label: Shadows, value: true}\n"
    "    - type: Window\n"
    "      title: Properties\n"
    "      x: 200\n"
    "      y: 120\n"
    "      width: 300\n"
    "      height: 150\n"
    "      children:\n"
    "        - {type: Button, label: OK}\n";

TEST(ui_screen, WindowsAreAtTheirPositions) {
  const ui_render r = render_ui(kTwoWindows);
  const ui_entry& a = r.state.at("root/children/0");
  const ui_entry& b = r.state.at("root/children/1");
  EXPECT_EQ(a.pos.x, 16);
  EXPECT_EQ(a.pos.y, 16);
  EXPECT_EQ(a.size.x, 260);
  EXPECT_EQ(a.size.y, 200);
  EXPECT_EQ(b.pos.x, 200);
  EXPECT_EQ(b.pos.y, 120);
  // The Screen's background is not the windows' background.
  EXPECT_NE(test::pixel(r.img, 4, 4), test::pixel(r.img, 100, 100));
}

TEST(ui_screen, TheLaterWindowIsOnTop) {
  const ui_render r = render_ui(kTwoWindows);
  // Inside the first window's body, under the second window's title bar
  // (y 120 to 120 + title height): the second's title bar colour, which the
  // first window's body does not have.
  const test::rgba title = test::pixel(r.img, 240, 124);
  const test::rgba body_of_first = test::pixel(r.img, 100, 150);
  EXPECT_NE(title, body_of_first);
  // The second window's title bar is the focused one (active colour),
  // different from the first window's title bar.
  EXPECT_NE(title, test::pixel(r.img, 100, 20));
}

TEST(ui_screen, ZeroSizeFitsTheContent) {
  const auto source = [](const std::string& text) {
    return "image: {width: 640, height: 360}\n"
           "root:\n"
           "  type: Screen\n"
           "  children:\n"
           "    - type: Window\n"
           "      title: Auto\n"
           "      x: 10\n"
           "      y: 10\n"
           "      children:\n"
           "        - {type: Text, text: \"" +
        text + "\"}\n";
  };
  const ui_entry narrow = render_ui(source("Short")).state.at("root/children/0");
  const ui_entry wide = render_ui(source("A much longer line of text than the other")).state.at("root/children/0");
  EXPECT_GT(narrow.size.x, 40);
  EXPECT_LT(narrow.size.x, 200);
  EXPECT_GT(wide.size.x, narrow.size.x + 100);
  EXPECT_EQ(narrow.size.y, wide.size.y);
}

TEST(ui_screen, AutoFitStaysInsideTheScreen) {
  const ui_render r = render_ui(
      "image: {width: 300, height: 200, padding: 0}\n"
      "root:\n"
      "  type: Screen\n"
      "  children:\n"
      "    - type: Window\n"
      "      x: 100\n"
      "      y: 100\n"
      "      children:\n"
      "        - {type: Text, text: \"A line of text that is far too long for the room left\"}\n");
  const ui_entry& w = r.state.at("root/children/0");
  EXPECT_LE(w.pos.x + w.size.x, 300);
  EXPECT_LE(w.pos.y + w.size.y, 200);
}

TEST(ui_screen, NegativeSizeReachesTheScreenEdge) {
  const ui_render r = render_ui(
      "image: {width: 640, height: 360, padding: 0}\n"
      "root:\n"
      "  type: Screen\n"
      "  children:\n"
      "    - {type: Window, title: Fill, x: 100, y: 50, width: -1, height: -1}\n");
  const ui_entry& w = r.state.at("root/children/0");
  EXPECT_EQ(w.pos.x + w.size.x, 640);
  EXPECT_EQ(w.pos.y + w.size.y, 360);
}

TEST(ui_screen, WindowsWithoutAPositionCascade) {
  const ui_render r = render_ui(
      "image: {width: 640, height: 360, padding: 0}\n"
      "root:\n"
      "  type: Screen\n"
      "  children:\n"
      "    - {type: Window, title: A, width: 100, height: 80}\n"
      "    - {type: Window, title: B, width: 100, height: 80}\n");
  EXPECT_EQ(r.state.at("root/children/0").pos.x, 16);
  EXPECT_EQ(r.state.at("root/children/1").pos.x, 40);
  EXPECT_EQ(r.state.at("root/children/1").pos.y, 40);
}

TEST(ui_screen, AWindowAsRootFillsTheImage) {
  const ui_render r = render_ui(
      "image: {width: 400, height: 300, padding: 10}\n"
      "root:\n"
      "  type: Window\n"
      "  title: Settings\n"
      "  children:\n"
      "    - {type: Checkbox, label: Enabled}\n");
  const ui_entry& w = r.state.at("root");
  EXPECT_EQ(w.pos.x, 0);
  EXPECT_EQ(w.size.x, 380);
  EXPECT_EQ(w.size.y, 280);
}

TEST(ui_screen, AWindowPlacedByALayoutTakesNoPosition) {
  try {
    check_source("root: {type: Window, title: A, x: 10}\n");
    FAIL() << "no error";
  } catch (const error& e) {
    EXPECT_EQ(e.line(), 1);
    EXPECT_NE(std::string(e.what()).find("Window.x is not allowed here"), std::string::npos) << e.what();
  }
}

TEST(ui_screen, AWindowPastTheScreenEdgeIsAnError) {
  try {
    check_source(
        "image: {width: 400, height: 300, padding: 0}\n"
        "root:\n"
        "  type: Screen\n"
        "  children:\n"
        "    - type: Window\n"
        "      x: 300\n"
        "      y: 10\n"
        "      width: 200\n");
    FAIL() << "no error";
  } catch (const error& e) {
    EXPECT_EQ(e.line(), 8);
    EXPECT_NE(
        std::string(e.what()).find("Window.width puts the window's right edge at x = 500, the Screen is 400 wide"),
        std::string::npos)
        << e.what();
  }
}

TEST(ui_screen, CheckboxValueShows) {
  const std::string ticked = kTwoWindows;
  std::string unticked = ticked;
  unticked.replace(unticked.find("value: true"), 11, "value: false");
  const ui_render a = render_ui(ticked);
  const ui_render b = render_ui(unticked);
  EXPECT_NE(a.img.rgba, b.img.rgba);
  EXPECT_TRUE(a.state.at("root/children/0/children/1").flag);
  EXPECT_FALSE(b.state.at("root/children/0/children/1").flag);
}

TEST(ui_screen, RenderingIsDeterministic) {
  const std::vector<uint8_t> a = render_png(kTwoWindows);
  const std::vector<uint8_t> b = render_png(kTwoWindows);
  EXPECT_EQ(a, b);
}

TEST(ui_screen, SameTitlesGiveSeparateWindows) {
  const ui_render r = render_ui(
      "image: {width: 640, height: 360, padding: 0}\n"
      "root:\n"
      "  type: Screen\n"
      "  children:\n"
      "    - {type: Window, title: Tool, x: 10, y: 10, width: 100, height: 80}\n"
      "    - {type: Window, title: Tool, x: 300, y: 10, width: 100, height: 80}\n"
      "    - {type: Window, title: \"Tool###x\", x: 10, y: 200, width: 100, height: 80}\n"
      "    - {type: Window, title: \"Tool###x\", x: 300, y: 200, width: 100, height: 80}\n");
  EXPECT_EQ(r.state.at("root/children/1").pos.x, 300);
  EXPECT_EQ(r.state.at("root/children/3").pos.y, 200);
}

// ── Unique IDs (DESIGN section 3) ────────────────────────────────────────────

TEST(ui_ids, RepeatedLabelsNeverShareAnId) {
  // render_ui() runs with the ID check on; it throws on a repeat.
  EXPECT_NO_THROW(render_ui(
      "root:\n"
      "  type: Screen\n"
      "  children:\n"
      "    - type: Window\n"
      "      title: Dialog\n"
      "      children:\n"
      "        - {type: Button, label: OK}\n"
      "        - {type: Button, label: OK}\n"
      "        - {type: Button, label: \"##\"}\n"
      "        - {type: Button, label: \"##\"}\n"
      "        - {type: Checkbox, label: OK}\n"
      "        - {type: Checkbox, label: \"OK###same\"}\n"
      "        - {type: Checkbox, label: \"OK###same\"}\n"
      "    - type: Window\n"
      "      title: Dialog\n"
      "      children:\n"
      "        - {type: Button, label: OK}\n"));
}

TEST(ui_ids, TheCheckReportsARepeatWithBothPaths) {
  ui_state state;
  state.set_check_ids(true);
  state.begin_frame();
  state.note_id(42, "root/children/0");
  try {
    state.note_id(42, "root/children/1");
    FAIL() << "no error";
  } catch (const std::logic_error& e) {
    EXPECT_NE(std::string(e.what()).find("root/children/0 and root/children/1"), std::string::npos) << e.what();
  }
  state.begin_frame(); // a new frame starts over
  EXPECT_NO_THROW(state.note_id(42, "root/children/1"));
}

TEST(ui_ids, TheCheckIsOffUnlessTurnedOn) {
  ui_state state;
  state.begin_frame();
  state.note_id(7, "a");
  EXPECT_NO_THROW(state.note_id(7, "b"));
}

} // namespace
} // namespace bdg::nymph
