// MIT License © 2026 Binary Dice Games
/// @file test_live_view_ui.cpp
/// @brief spec/09_ui_mockups PLAN step 9: user interfaces in a live view:
///        static parity with the PNG, and pointer interaction.
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include "core/error.hpp"
#include "document/bind.hpp"
#include "elements/register_builtin.hpp"
#include "live_view_driver.hpp"
#include "nymph.hpp"
#include "render/figure_renderer.hpp"
#include "render/soft_rasterizer.hpp"
#include "ui_test_util.hpp"

namespace bdg::nymph {
namespace {

using test::render_ui;
using test::ui_render;

/// One window of widgets on a 600 x 400 Screen (no image padding).
const char* const kForm =
    "image: {width: 600, height: 400, padding: 0}\n"
    "root:\n"
    "  type: Screen\n"
    "  children:\n"
    "    - type: Window\n"
    "      title: Form\n"
    "      x: 20\n"
    "      y: 20\n"
    "      width: 300\n"
    "      height: 300\n"
    "      children:\n"
    "        - type: TreeNode\n"
    "          label: Node\n"
    "          children: [{type: Button, label: Inside}]\n"
    "        - {type: SliderFloat, label: Gain, value: 0.5, width: 200}\n"
    "        - {type: RadioButton, label: One, active: true}\n"
    "        - {type: RadioButton, label: Two}\n"
    "        - {type: Button, label: Apply}\n"
    "        - type: TabBar\n"
    "          children:\n"
    "            - {type: TabItem, label: First, children: [{type: Text, text: first}]}\n"
    "            - {type: TabItem, label: Second, children: [{type: Checkbox, label: Second box}]}\n";

/// The rectangle (CSS px) of element `path` in the view's last frame,
/// through the editor's targets.
std::optional<rect> target_in(live_view& v, const std::string& path) {
  v.set_edit_mode(true);
  double clock = 1000.0;
  for (int i = 0; i < 4; ++i) {
    v.frame(clock);
    clock += 1.0 / 60.0;
  }
  const std::optional<edit_target> t = v.target_for(path, "");
  v.set_edit_mode(false);
  if (!t) {
    return std::nullopt;
  }
  return rect{ImVec2(t->x, t->y), ImVec2(t->x + t->width, t->y + t->height)};
}

/// The target of `path` in the PNG frames (edit mode on).
rect png_target(const std::string& source, const std::string& path) {
  const ui_render r = render_ui(source, true);
  for (const edit_record& t : r.targets) {
    if (t.field.empty() && r.paths.at(t.element) == path) {
      return t.area;
    }
  }
  ADD_FAILURE() << "no target " << path;
  return {};
}

ImVec2 centre(const rect& r) {
  return ImVec2((r.min.x + r.max.x) / 2, (r.min.y + r.max.y) / 2);
}

std::string value_of(const view_pick& p, const std::string& key) {
  for (const auto& [k, v] : p.values) {
    if (k == key) {
      return v;
    }
  }
  return {};
}

// ── Static parity ────────────────────────────────────────────────────────────

TEST(live_view_ui, WithNoInputDrawsThePng) {
  const std::string sources[] = {
      kForm,
      "image: {width: 500, height: 300}\n"
      "root:\n"
      "  type: Screen\n"
      "  children:\n"
      "    - {type: Window, id: a, title: A, x: 10, y: 10, width: 150, height: 100}\n"
      "    - {type: Window, id: b, title: B, x: 300, y: 150, children: [{type: Text, text: auto}]}\n"
      "    - {type: WindowTransition, from: a, to: b, label: next}\n"
      "    - type: PopupModal\n"
      "      title: Sure?\n"
      "      open: true\n"
      "      children: [{type: Button, label: OK}]\n",
      "image: {width: 400, height: 300, theme: dark}\n"
      "root:\n"
      "  type: Window\n"
      "  children:\n"
      "    - {type: Image, image: person, width: 64, height: 64, rounding: 32}\n"
      "    - {type: InputText, label: Name, value: Ada}\n"
      "    - {type: Table, children: [{type: TableColumn, label: A, cells: [x, y]}]}\n",
  };
  for (const std::string& source : sources) {
    SCOPED_TRACE(source);
    const figure f = bind_source(source, builtin_registry());
    live_view v;
    v.set_source(source);
    v.resize({static_cast<float>(f.image.width), static_cast<float>(f.image.height), 1.0f});
    double clock = 0.0;
    const ImDrawData* last = nullptr;
    for (int i = 0; i < 10 && v.wants_frame(); ++i) {
      last = v.frame(clock);
      clock += 1.0 / 60.0;
    }
    ASSERT_NE(last, nullptr);
    const image live = rasterize(*last, v.textures(), v.clear_color());
    EXPECT_TRUE(live.rgba == render_figure(f).rgba);
  }
}

// ── Pointer interaction ──────────────────────────────────────────────────────

TEST(live_view_ui, ClickingATreeNodeOpensIt) {
  driven_view d(kForm, 600, 400);
  const rect node = png_target(kForm, "root/children/0/children/0");
  EXPECT_FALSE(target_in(d.v, "root/children/0/children/0/children/0"));
  d.click(node.min.x + 30, centre(node).y);
  EXPECT_FALSE(d.v.wants_frame()); // idle again once the click settled
  const std::optional<rect> inside = target_in(d.v, "root/children/0/children/0/children/0");
  ASSERT_TRUE(inside);
  EXPECT_GT(inside->min.y, node.max.y - 1);
}

TEST(live_view_ui, ClickingATabShowsIt) {
  driven_view d(kForm, 600, 400);
  const std::string second = "root/children/0/children/5/children/1";
  const rect tab = png_target(kForm, second);
  EXPECT_FALSE(target_in(d.v, second + "/children/0"));
  d.click(centre(tab).x, centre(tab).y);
  EXPECT_TRUE(target_in(d.v, second + "/children/0"));
}

TEST(live_view_ui, DraggingASliderChangesItsValue) {
  driven_view d(kForm, 600, 400);
  const rect slider = png_target(kForm, "root/children/0/children/1");
  d.move(centre(slider).x, centre(slider).y);
  ASSERT_TRUE(d.v.hovered());
  EXPECT_EQ(value_of(*d.v.hovered(), "value"), "0.5");
  d.drag(centre(slider).x, centre(slider).y, slider.max.x - 2, centre(slider).y);
  d.move(centre(slider).x, centre(slider).y);
  ASSERT_TRUE(d.v.hovered());
  EXPECT_EQ(value_of(*d.v.hovered(), "value"), "1");
}

TEST(live_view_ui, ARadioButtonClearsItsSiblings) {
  driven_view d(kForm, 600, 400);
  const rect one = png_target(kForm, "root/children/0/children/2");
  const rect two = png_target(kForm, "root/children/0/children/3");
  d.click(two.min.x + 8, centre(two).y);
  d.move(one.min.x + 8, centre(one).y);
  ASSERT_TRUE(d.v.hovered());
  EXPECT_EQ(value_of(*d.v.hovered(), "active"), "false");
  d.move(two.min.x + 8, centre(two).y);
  ASSERT_TRUE(d.v.hovered());
  EXPECT_EQ(value_of(*d.v.hovered(), "active"), "true");
}

TEST(live_view_ui, AButtonClickIsReported) {
  driven_view d(kForm, 600, 400);
  const rect apply = png_target(kForm, "root/children/0/children/4");
  d.click(centre(apply).x, centre(apply).y);
  const std::optional<view_pick> click = d.v.take_click();
  ASSERT_TRUE(click);
  EXPECT_EQ(click->type, "Button");
  EXPECT_EQ(click->label, "Apply");
  EXPECT_EQ(click->element, "root/children/0/children/4");
}

TEST(live_view_ui, AWindowMovesAndStaysOnTheScreen) {
  driven_view d(kForm, 600, 400);
  d.drag(150, 30, 250, 80); // the title bar, 100 right and 50 down
  std::optional<rect> w = target_in(d.v, "root/children/0");
  ASSERT_TRUE(w);
  EXPECT_EQ(w->min.x, 120);
  EXPECT_EQ(w->min.y, 70);
  d.drag(250, 80, 590, 395); // far past the bottom right corner
  d.run();
  w = target_in(d.v, "root/children/0");
  ASSERT_TRUE(w);
  EXPECT_LE(w->max.x, 600);
  EXPECT_LE(w->max.y, 400);
}

TEST(live_view_ui, TheWheelBelongsToWindows) {
  driven_view d(kForm, 600, 400);
  EXPECT_TRUE(d.v.takes_wheel(100, 100)); // inside the window
  EXPECT_FALSE(d.v.takes_wheel(500, 380)); // the Screen's background
}

TEST(live_view_ui, ResetViewReturnsToTheSource) {
  driven_view d(kForm, 600, 400);
  const rect node = png_target(kForm, "root/children/0/children/0");
  d.click(node.min.x + 30, centre(node).y);
  ASSERT_TRUE(target_in(d.v, "root/children/0/children/0/children/0"));
  d.v.reset_view();
  d.run();
  EXPECT_FALSE(target_in(d.v, "root/children/0/children/0/children/0"));
}

// ── Keyboard and text (PLAN step 10) ─────────────────────────────────────────

const char* const kFields =
    "image: {width: 600, height: 400, padding: 0}\n"
    "root:\n"
    "  type: Screen\n"
    "  children:\n"
    "    - type: Window\n"
    "      title: Fields\n"
    "      x: 20\n"
    "      y: 20\n"
    "      width: 400\n"
    "      height: 200\n"
    "      children:\n"
    "        - {type: InputText, label: Name, value: Sun, width: 200}\n"
    "        - {type: SliderFloat, label: Gain, value: 0.5, width: 200}\n";

/// Presses and releases `key`, with Ctrl held while it is down.
void press(driven_view& d, ImGuiKey key, bool ctrl = false) {
  d.v.input_key({key, true, ctrl});
  d.run();
  d.v.input_key({key, false, false});
  d.run();
}

TEST(live_view_ui, TypingGoesIntoTheActiveField) {
  driven_view d(kFields, 600, 400);
  const rect field = png_target(kFields, "root/children/0/children/0");
  EXPECT_FALSE(d.v.wants_keyboard());
  d.click(centre(field).x, centre(field).y);
  EXPECT_TRUE(d.v.wants_keyboard());
  press(d, ImGuiKey_A, true); // select all
  d.v.input_text(U"Moon");
  d.run();
  press(d, ImGuiKey_Backspace);
  d.move(centre(field).x, centre(field).y);
  ASSERT_TRUE(d.v.hovered());
  EXPECT_EQ(value_of(*d.v.hovered(), "value"), "Moo");
  press(d, ImGuiKey_Escape); // ImGui leaves the field
  EXPECT_FALSE(d.v.wants_keyboard());
}

TEST(live_view_ui, CtrlClickTypesASliderValue) {
  driven_view d(kFields, 600, 400);
  const rect slider = png_target(kFields, "root/children/0/children/1");
  d.v.input_key({ImGuiKey_None, true, true}); // Ctrl held
  d.click(centre(slider).x, centre(slider).y);
  d.v.input_key({ImGuiKey_None, true, false});
  d.run();
  EXPECT_TRUE(d.v.wants_keyboard());
  press(d, ImGuiKey_A, true);
  d.v.input_text(U"0.75");
  d.run();
  press(d, ImGuiKey_Enter);
  d.move(centre(slider).x, centre(slider).y);
  ASSERT_TRUE(d.v.hovered());
  EXPECT_EQ(value_of(*d.v.hovered(), "value"), "0.75");
}

TEST(live_view_ui, CopyAndPasteUseTheHostsClipboard) {
  driven_view d(kFields, 600, 400);
  std::string clipboard = "pasted";
  d.v.set_clipboard_hooks([&](std::string_view text) { clipboard = text; }, [&] { return clipboard; });
  const rect field = png_target(kFields, "root/children/0/children/0");
  d.click(centre(field).x, centre(field).y);
  press(d, ImGuiKey_A, true);
  press(d, ImGuiKey_C, true);
  EXPECT_EQ(clipboard, "Sun");
  clipboard = "Moon";
  press(d, ImGuiKey_V, true);
  d.move(centre(field).x, centre(field).y);
  ASSERT_TRUE(d.v.hovered());
  EXPECT_EQ(value_of(*d.v.hovered(), "value"), "Moon");
}

// ── Saved view (PLAN step 11) ────────────────────────────────────────────────

TEST(live_view_ui, WhatTheUserChangedIsSavedAndRendersTheSame) {
  driven_view d(kForm, 600, 400);
  const rect node = png_target(kForm, "root/children/0/children/0");
  const rect slider = png_target(kForm, "root/children/0/children/1");
  // The slider first: opening the node moves it down.
  d.drag(centre(slider).x, centre(slider).y, slider.max.x - 2, centre(slider).y); // gain to 1
  d.click(node.min.x + 30, centre(node).y); // open the tree node
  d.drag(150, 30, 250, 80); // move the window by (100, 50)
  d.send({pointer_event::kind::leave, 0, 0});
  const std::string saved = d.v.source_with_view();
  EXPECT_NE(saved.find("root/children/0: {x: 120, y: 70}"), std::string::npos) << saved;
  EXPECT_NE(saved.find("root/children/0/children/0: {open: true}"), std::string::npos) << saved;
  EXPECT_NE(saved.find("root/children/0/children/1: {value: 1}"), std::string::npos) << saved;
  EXPECT_EQ(saved.find("children/2:"), std::string::npos) << saved; // untouched widgets are not saved
  // The saved source draws what is on screen.
  const ImDrawData* last = d.last;
  ASSERT_NE(last, nullptr);
  const image live = rasterize(*last, d.v.textures(), d.v.clear_color());
  const figure f = bind_source(saved, builtin_registry());
  EXPECT_TRUE(live.rgba == render_figure(f).rgba);
}

TEST(live_view_ui, NothingChangedGivesTheSourceBack) {
  driven_view d(kForm, 600, 400);
  d.move(500, 380);
  EXPECT_EQ(d.v.source_with_view(), kForm);
}

TEST(live_view_ui, ANewSourceValueReplacesTheUsersOne) {
  driven_view d(kForm, 600, 400);
  const rect slider = png_target(kForm, "root/children/0/children/1");
  const rect two = png_target(kForm, "root/children/0/children/3");
  d.drag(centre(slider).x, centre(slider).y, slider.max.x - 2, centre(slider).y); // gain to 1
  d.click(two.min.x + 8, centre(two).y); // radio Two
  // An agent changes the slider's value: its new value wins; the radio
  // the user chose stays.
  std::string next = kForm;
  next.replace(next.find("value: 0.5"), 10, "value: 0.25");
  d.v.set_source(next);
  d.run();
  d.move(centre(slider).x, centre(slider).y);
  ASSERT_TRUE(d.v.hovered());
  EXPECT_EQ(value_of(*d.v.hovered(), "value"), "0.25");
  d.move(two.min.x + 8, centre(two).y);
  ASSERT_TRUE(d.v.hovered());
  EXPECT_EQ(value_of(*d.v.hovered(), "active"), "true");
}

TEST(live_view_ui, SavedEntriesApplyInPngsAndAreChecked) {
  const std::string with_view = std::string(kForm) +
      "view:\n"
      "  root/children/0: {x: 200, y: 50, width: 320, height: 280}\n"
      "  root/children/0/children/0: {open: true}\n"
      "  root/children/0/children/1: {value: 0.9}\n"
      "  root/children/9: {open: true}\n"; // an element that is gone: ignored
  const ui_render r = render_ui(with_view, true);
  EXPECT_EQ(r.state.at("root/children/0").pos.x, 200);
  EXPECT_EQ(r.state.at("root/children/0").size.x, 320);
  EXPECT_TRUE(r.state.at("root/children/0/children/0").flag);
  EXPECT_FLOAT_EQ(r.state.at("root/children/0/children/1").floats[0], 0.9f);
  try {
    check_source(std::string(kForm) + "view:\n  root/children/0/children/0: {open: 3}\n");
    FAIL() << "no error";
  } catch (const error& e) {
    EXPECT_NE(std::string(e.what()).find("root/children/0/children/0.open must be true or false"), std::string::npos)
        << e.what();
  }
  try {
    check_source(std::string(kForm) + "view:\n  root/children/0/children/1: {valeu: 1}\n");
    FAIL() << "no error";
  } catch (const error& e) {
    EXPECT_NE(std::string(e.what()).find("unknown key 'valeu' (did you mean 'value'?)"), std::string::npos) << e.what();
  }
}

} // namespace
} // namespace bdg::nymph
