// MIT License © 2026 Binary Dice Games
/// @file test_ui_menus.cpp
/// @brief spec/09_ui_mockups PLAN step 8: menu bars, menus, popups, modals
///        and tooltips, shown in a PNG and opened by `opens`.
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include "core/error.hpp"
#include "live_view_driver.hpp"
#include "nymph.hpp"
#include "ui_test_util.hpp"

namespace bdg::nymph {
namespace {

using test::pixel;
using test::render_ui;
using test::ui_render;

/// A Screen of 600 x 400 at the image's padding of 8, with `children`.
std::string screen(const std::string& children) {
  return "image: {width: 616, height: 416, padding: 8}\n"
         "root:\n"
         "  type: Screen\n"
         "  children:\n" +
      children;
}

std::optional<rect> target(const ui_render& r, const std::string& path, const std::string& field = {}) {
  for (const edit_record& t : r.targets) {
    if (t.field == field && r.paths.at(t.element) == path) {
      return t.area;
    }
  }
  return std::nullopt;
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

bool says(const error& e, const std::string& part) {
  return std::string(e.what()).find(part) != std::string::npos;
}

const char* const kMenuBar =
    "    - type: MainMenuBar\n"
    "      children:\n"
    "        - type: Menu\n"
    "          label: File\n"
    "          open: OPEN\n"
    "          children:\n"
    "            - {type: MenuItem, label: New, shortcut: Ctrl+N}\n"
    "            - {type: MenuItem, label: Autosave, checked: true}\n"
    "        - {type: Menu, label: Edit, children: [{type: MenuItem, label: Undo}]}\n";

std::string menu_bar(bool open) {
  std::string s = kMenuBar;
  s.replace(s.find("OPEN"), 4, open ? "true" : "false");
  return s;
}

TEST(ui_menus, AnOpenMenuShowsItsItemsBelowTheBar) {
  const ui_render r = render_ui(screen(menu_bar(true)), true);
  const rect bar = *target(r, "root/children/0");
  const std::optional<rect> item = target(r, "root/children/0/children/0/children/0");
  ASSERT_TRUE(item);
  EXPECT_GE(item->min.y, bar.max.y - 1);
  EXPECT_EQ(bar.min.x, 8);
  EXPECT_EQ(bar.width(), 600);
  EXPECT_TRUE(r.state.at("root/children/0/children/0/children/1").flag); // Autosave checked
  const ui_render closed = render_ui(screen(menu_bar(false)), true);
  EXPECT_FALSE(target(closed, "root/children/0/children/0/children/0"));
}

TEST(ui_menus, AModalSitsInTheMiddleAndDimsOnlyTheScreen) {
  const std::string window = "    - {type: Window, title: W, x: 20, y: 20, width: 200, height: 100}\n";
  const std::string modal =
      "    - type: PopupModal\n"
      "      title: Delete?\n"
      "      open: true\n"
      "      children: [{type: Text, text: Sure?}, {type: Button, label: OK}]\n";
  const ui_render with = render_ui(screen(window + modal), true);
  const ui_render without = render_ui(screen(window));
  const rect m = *target(with, "root/children/1");
  EXPECT_NEAR((m.min.x + m.max.x) / 2, 8 + 300, 1);
  EXPECT_NEAR((m.min.y + m.max.y) / 2, 8 + 200, 1);
  // The image margin, outside the Screen, is not dimmed; the Screen is.
  EXPECT_EQ(pixel(with.img, 2, 2), pixel(without.img, 2, 2));
  EXPECT_EQ(pixel(with.img, 612, 412), pixel(without.img, 612, 412));
  EXPECT_NE(pixel(with.img, 500, 350), pixel(without.img, 500, 350));
  EXPECT_NE(pixel(with.img, 100, 80), pixel(without.img, 100, 80)); // the window too
}

TEST(ui_menus, APopupOpensUnderItsOpener) {
  const ui_render r = render_ui(
      screen(
          "    - type: Window\n"
          "      x: 20\n"
          "      y: 20\n"
          "      width: 300\n"
          "      height: 200\n"
          "      children:\n"
          "        - {type: Text, text: above}\n"
          "        - {type: Button, label: Settings, opens: settings}\n"
          "    - type: Popup\n"
          "      id: settings\n"
          "      open: true\n"
          "      children: [{type: Checkbox, label: Dark}]\n"),
      true);
  const rect button = *target(r, "root/children/0/children/1");
  const rect popup = *target(r, "root/children/1");
  EXPECT_EQ(popup.min.x, button.min.x);
  EXPECT_EQ(popup.min.y, button.max.y);
}

TEST(ui_menus, APopupAtItsPosition) {
  const ui_render r = render_ui(
      screen("    - {type: Popup, id: p, open: true, x: 100, y: 50, children: [{type: Text, text: hi}]}\n"), true);
  const rect popup = *target(r, "root/children/0");
  EXPECT_EQ(popup.min.x, 108);
  EXPECT_EQ(popup.min.y, 58);
}

TEST(ui_menus, AnOpenTooltipSitsUnderItsWidget) {
  const ui_render r = render_ui(
      screen(
          "    - type: Window\n"
          "      x: 20\n"
          "      y: 20\n"
          "      width: 300\n"
          "      height: 200\n"
          "      children:\n"
          "        - {type: Button, label: Save}\n"
          "        - {type: Tooltip, open: true, children: [{type: Text, text: Saves the file}]}\n"),
      true);
  const rect button = *target(r, "root/children/0/children/0");
  const std::optional<rect> tip = target(r, "root/children/0/children/1/children/0");
  ASSERT_TRUE(tip);
  EXPECT_GT(tip->min.y, button.max.y);
  const ui_render hidden = render_ui(
      screen(
          "    - type: Window\n"
          "      children:\n"
          "        - {type: Button, label: Save}\n"
          "        - {type: Tooltip, children: [{type: Text, text: Saves the file}]}\n"),
      true);
  EXPECT_FALSE(target(hidden, "root/children/0/children/1/children/0")); // only on hover
}

TEST(ui_menus, PopupAndMenuErrors) {
  const error unknown = source_error(screen(
      "    - {type: Window, children: [{type: Button, label: b, opens: setings}]}\n"
      "    - {type: Popup, id: settings}\n"));
  EXPECT_TRUE(says(unknown, "Button.opens names no Popup or PopupModal id 'setings' (did you mean 'settings'?)"))
      << unknown.what();
  const error duplicate = source_error(screen("    - {type: Popup, id: a}\n    - {type: PopupModal, id: a}\n"));
  EXPECT_TRUE(says(duplicate, "PopupModal.id 'a' is used by another popup")) << duplicate.what();
  const error two_menus = source_error(screen(
      "    - type: MainMenuBar\n"
      "      children: [{type: Menu, label: A, open: true}, {type: Menu, label: B, open: true}]\n"));
  EXPECT_TRUE(says(two_menus, "Menu.open is true for a second Menu of this MainMenuBar")) << two_menus.what();
  const error two_open = source_error(screen(menu_bar(true) + "    - {type: Popup, id: p, open: true}\n"));
  EXPECT_TRUE(says(two_open, "one open popup or menu at a time")) << two_open.what();
  const error rootless = source_error("root: {type: Window, children: [{type: Button, label: b, opens: x}]}\n");
  EXPECT_TRUE(says(rootless, "popups go on a Screen")) << rootless.what();
}

TEST(ui_menus, RepeatedLabelsInMenusAndPopupsKeepTheirIds) {
  const std::string window =
      "    - type: Window\n"
      "      title: Same\n"
      "      children:\n"
      "        - type: MenuBar\n"
      "          children:\n"
      "            - {type: Menu, label: File, open: OPEN, children: [{type: MenuItem, label: Same}, {type: MenuItem, "
      "label: Same}]}\n"
      "            - {type: Menu, label: File, children: [{type: MenuItem, label: Same}]}\n";
  std::string first = window, second = window;
  first.replace(first.find("OPEN"), 4, "true");
  second.replace(second.find("OPEN"), 4, "false");
  EXPECT_NO_THROW(render_ui(screen(
      first + second +
      "    - {type: Popup, id: a, children: [{type: Button, label: Same}]}\n"
      "    - {type: PopupModal, id: b, title: Same, children: [{type: Button, label: Same}]}\n")));
}

// ── Live views ───────────────────────────────────────────────────────────────

TEST(ui_menus, AButtonInAChildOpensAPopupOfTheScreen) {
  const std::string source = screen(
      "    - type: Window\n"
      "      x: 20\n"
      "      y: 20\n"
      "      width: 300\n"
      "      height: 200\n"
      "      children:\n"
      "        - type: Child\n"
      "          size: [0, 120]\n"
      "          children:\n"
      "            - {type: Button, label: Settings, opens: settings}\n"
      "    - type: Popup\n"
      "      id: settings\n"
      "      children: [{type: Checkbox, label: Dark mode}]\n");
  const rect button = *target(render_ui(source, true), "root/children/0/children/0/children/0");
  driven_view d(source, 616, 416);
  d.click((button.min.x + button.max.x) / 2, (button.min.y + button.max.y) / 2);
  // The popup opens under the button; its checkbox is its first item.
  d.move(button.min.x + 14, button.max.y + 16);
  ASSERT_TRUE(d.v.hovered());
  EXPECT_EQ(d.v.hovered()->type, "Checkbox");
  EXPECT_EQ(d.v.hovered()->element, "root/children/1/children/0");
}

TEST(ui_menus, AClickOnAMenuItemIsReported) {
  const std::string source = screen(menu_bar(false));
  driven_view d(source, 616, 416);
  d.click(30, 18); // "File" in the bar
  d.move(40, 18 + 26); // the first item, "New"
  ASSERT_TRUE(d.v.hovered());
  EXPECT_EQ(d.v.hovered()->label, "New");
  d.click(40, 18 + 26);
  const std::optional<view_pick> click = d.v.take_click();
  ASSERT_TRUE(click);
  EXPECT_EQ(click->type, "MenuItem");
  EXPECT_EQ(click->label, "New");
}

} // namespace
} // namespace bdg::nymph
