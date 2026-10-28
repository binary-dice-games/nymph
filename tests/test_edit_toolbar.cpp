// MIT License © 2026 Binary Dice Games
/// @file test_edit_toolbar.cpp
/// @brief The editor's number box (click to type, drag to change) and its
///        floating toolbar: which tools an element gets, where the toolbar
///        goes, and the style actions (spec/08_view_editor/DESIGN.md
///        section 3).
#include <gtest/gtest.h>
#include <imgui.h>
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <string>
#include <vector>
#include "edit/edit_toolbar.hpp"
#include "edit/editor_model.hpp"
#include "edit/inspector.hpp"
#include "edit/number_box.hpp"
#include "png/png_meta.hpp"
#include "render/frame_setup.hpp"
#include "render/soft_rasterizer.hpp"
#include "render/texture_store.hpp"

namespace bdg::nymph {
namespace {

const char* const kSource =
    "image: {width: 500, height: 400}\n"
    "root:\n"
    "  type: VerticalLayout\n"
    "  font_size: 20\n"
    "  children:\n"
    "    - type: Plot\n"
    "      title: Revenue\n"
    "      children:\n"
    "        - {type: PlotLine, label: a, ys: [1, 3, 2]}\n"
    "    - type: Flowchart\n"
    "      children:\n"
    "        - {type: FlowNode, id: a, text: Start, shape: Stadium, fill_color: \"#BEF6AC\", text_color: \"#C0392B\"}\n"
    "        - {type: FlowNode, id: b, text: Stop}\n"
    "        - {type: FlowLink, from: a, to: b}\n"
    "    - {type: Shape, shape: cloud, width: 80, height: 50}\n"
    "    - type: Plot\n"
    "      children:\n"
    "        - {type: PlotPieChart, values: [1, 2], labels: \"a\\nb\"}\n";

constexpr const char* kPie = "root/children/3/children/0";

constexpr const char* kNodeA = "root/children/1/children/0";
constexpr const char* kNodeB = "root/children/1/children/1";

/// An ImGui context that runs frames without a rendering backend, with
/// pointer and key input.
class headless_imgui {
 public:
  headless_imgui() {
    const context_switch on(contexts_);
    configure(*find_theme("light"), "test", ImGuiBackendFlags_RendererHasTextures);
    load_fonts(dc_);
  }

  /// One frame of `f` inside a window at the top-left corner (`windowed`)
  /// or at the top level (the toolbar opens its own window).
  void frame(const std::function<void()>& f, bool windowed = true) {
    const context_switch on(contexts_);
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(600, 500);
    io.DeltaTime = 1.0f / 60.0f;
    ImGui::NewFrame();
    if (windowed) {
      ImGui::SetNextWindowPos(ImVec2(0, 0));
      ImGui::SetNextWindowSize(ImVec2(300, 500));
      ImGui::Begin("panel");
    }
    f();
    if (windowed) {
      ImGui::End();
    }
    ImGui::Render();
    textures_.apply_all(*ImGui::GetDrawData());
  }

  /// Runs `f` with the context current (to queue input events).
  void input(const std::function<void(ImGuiIO&)>& f) {
    const context_switch on(contexts_);
    f(ImGui::GetIO());
  }

  image snapshot() {
    const context_switch on(contexts_);
    return rasterize(*ImGui::GetDrawData(), textures_, IM_COL32(255, 255, 255, 255));
  }

 private:
  imgui_contexts contexts_;
  texture_store textures_;
  image_options options_;
  draw_context dc_{options_};
};

// ── Number box: arithmetic ──────────────────────────────────────────────────

TEST(number_box, DragStepFollowsTheLimitsOrTheValue) {
  number_box_options limited;
  limited.min = 0.0;
  limited.max = 1.0;
  EXPECT_DOUBLE_EQ(number_box::drag_step(0.5, limited), 0.01); // an alpha: 100 px for the range
  limited.max = 100.0;
  EXPECT_DOUBLE_EQ(number_box::drag_step(3.0, limited), 1.0);
  number_box_options open;
  EXPECT_DOUBLE_EQ(number_box::drag_step(12.0, open), 0.1);
  EXPECT_DOUBLE_EQ(number_box::drag_step(5000.0, open), 10.0);
  number_box_options whole;
  whole.integer = true;
  whole.min = 0.0;
  whole.max = 10.0;
  EXPECT_DOUBLE_EQ(number_box::drag_step(2.0, whole), 0.1); // ten pixels per unit
  whole.min = 16.0;
  whole.max = 4096.0; // a width: never more than one unit per pixel
  EXPECT_DOUBLE_EQ(number_box::drag_step(700.0, whole), 1.0);
  number_box_options wide; // a Plot's width: -1000..8192, not whole
  wide.min = -1000.0;
  wide.max = 8192.0;
  EXPECT_DOUBLE_EQ(number_box::drag_step(700.0, wide), 1.0);
  number_box_options open_whole;
  open_whole.integer = true;
  EXPECT_DOUBLE_EQ(number_box::drag_step(50000.0, open_whole), 100.0);
}

TEST(number_box, TidyClampsRoundsAndKeepsShortDecimals) {
  number_box_options o;
  o.min = 0.0;
  o.max = 10.0;
  EXPECT_DOUBLE_EQ(number_box::tidy(12.0, o), 10.0);
  EXPECT_DOUBLE_EQ(number_box::tidy(-1.0, o), 0.0);
  EXPECT_EQ(number_box::format(number_box::tidy(0.1 * 3.0, o, 0.1), false), "0.3");
  EXPECT_DOUBLE_EQ(number_box::tidy(2.345, o, 0.01), 2.35);
  o.integer = true;
  EXPECT_DOUBLE_EQ(number_box::tidy(2.6, o), 3.0);
}

TEST(number_box, ParsesTypedValues) {
  EXPECT_EQ(number_box::parse("12"), 12.0);
  EXPECT_EQ(number_box::parse(" -1.5e3 "), -1500.0);
  EXPECT_EQ(number_box::parse("2,5"), 2.5);
  EXPECT_FALSE(number_box::parse(""));
  EXPECT_FALSE(number_box::parse("abc"));
  EXPECT_FALSE(number_box::parse("1x"));
  EXPECT_EQ(number_box::format(14.0, false), "14");
  EXPECT_EQ(number_box::format(1.25, false), "1.25");
  EXPECT_EQ(number_box::format(7.0, true), "7");
}

// ── Number box: the pointer and the keyboard ────────────────────────────────

/// A box in a headless window: where it is and what it did last frame.
struct box_rig {
  headless_imgui gui;
  number_box box;
  number_box_options options;
  double value = 10.0;
  ImVec2 center;
  number_box_change last = number_box_change::none;
  std::vector<number_box_change> changes;

  void frame() {
    gui.frame([&] {
      ImGui::SetNextItemWidth(120.0f);
      last = box.draw("size", value, options);
      center = ImVec2(
          (ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) * 0.5f,
          (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f);
    });
    if (last != number_box_change::none) {
      changes.push_back(last);
    }
  }
  void mouse(float x, float y) {
    gui.input([&](ImGuiIO& io) { io.AddMousePosEvent(x, y); });
    frame();
  }
  void button(bool down) {
    gui.input([&](ImGuiIO& io) { io.AddMouseButtonEvent(0, down); });
    frame();
  }
  void key(ImGuiKey k) {
    gui.input([&](ImGuiIO& io) { io.AddKeyEvent(k, true); });
    frame();
    gui.input([&](ImGuiIO& io) { io.AddKeyEvent(k, false); });
    frame();
  }
  void type(const char* text) {
    gui.input([&](ImGuiIO& io) { io.AddInputCharactersUTF8(text); });
    frame();
  }
  /// Clicks the box (no movement) and waits for its text field.
  void click_to_type() {
    mouse(center.x, center.y);
    button(true);
    button(false);
    frame();
    frame();
  }
};

TEST(number_box, AClickTypesAValueAndEnterAcceptsIt) {
  box_rig rig;
  rig.frame();
  rig.click_to_type();
  EXPECT_TRUE(rig.box.typing());
  EXPECT_DOUBLE_EQ(rig.value, 10.0); // a click alone changes nothing
  rig.key(ImGuiKey_A); // nothing: digits only
  rig.gui.input([&](ImGuiIO& io) { io.AddKeyEvent(ImGuiMod_Ctrl, true); });
  rig.key(ImGuiKey_A); // select all, then type over it
  rig.gui.input([&](ImGuiIO& io) { io.AddKeyEvent(ImGuiMod_Ctrl, false); });
  rig.type("42.5");
  EXPECT_DOUBLE_EQ(rig.value, 10.0); // not before Enter
  rig.key(ImGuiKey_Enter);
  EXPECT_DOUBLE_EQ(rig.value, 42.5);
  EXPECT_FALSE(rig.box.typing());
  ASSERT_EQ(rig.changes.size(), 1u);
  EXPECT_EQ(rig.changes.front(), number_box_change::typed);
}

TEST(number_box, EscapeCancelsTheTypedValueAndLimitsClampIt) {
  box_rig rig;
  rig.options.min = 1.0;
  rig.options.max = 50.0;
  rig.frame();
  rig.click_to_type();
  rig.gui.input([&](ImGuiIO& io) { io.AddKeyEvent(ImGuiMod_Ctrl, true); });
  rig.key(ImGuiKey_A);
  rig.gui.input([&](ImGuiIO& io) { io.AddKeyEvent(ImGuiMod_Ctrl, false); });
  rig.type("33");
  rig.key(ImGuiKey_Escape);
  EXPECT_DOUBLE_EQ(rig.value, 10.0);
  EXPECT_TRUE(rig.changes.empty());

  rig.click_to_type();
  rig.gui.input([&](ImGuiIO& io) { io.AddKeyEvent(ImGuiMod_Ctrl, true); });
  rig.key(ImGuiKey_A);
  rig.gui.input([&](ImGuiIO& io) { io.AddKeyEvent(ImGuiMod_Ctrl, false); });
  rig.type("900");
  rig.key(ImGuiKey_Enter);
  EXPECT_DOUBLE_EQ(rig.value, 50.0);
}

TEST(number_box, DraggingSidewaysChangesTheValueByTheStep) {
  box_rig rig;
  rig.options.step = 0.5; // per pixel
  rig.frame();
  rig.mouse(rig.center.x, rig.center.y);
  rig.button(true);
  rig.mouse(rig.center.x + 20.0f, rig.center.y);
  EXPECT_DOUBLE_EQ(rig.value, 20.0);
  rig.mouse(rig.center.x + 10.0f, rig.center.y);
  EXPECT_DOUBLE_EQ(rig.value, 15.0);
  rig.button(false);
  rig.frame();
  EXPECT_FALSE(rig.box.typing()); // a drag is not a click
  ASSERT_FALSE(rig.changes.empty());
  EXPECT_TRUE(std::all_of(
      rig.changes.begin(), rig.changes.end(), [](number_box_change c) { return c == number_box_change::dragged; }));

  // Shift: ten times finer.
  rig.changes.clear();
  rig.gui.input([&](ImGuiIO& io) { io.AddKeyEvent(ImGuiMod_Shift, true); });
  rig.mouse(rig.center.x, rig.center.y);
  rig.button(true);
  rig.mouse(rig.center.x + 20.0f, rig.center.y);
  rig.button(false);
  EXPECT_DOUBLE_EQ(rig.value, 16.0);
}

TEST(number_box, ZeroShowsItsTextAndDragsFromTheInheritedValue) {
  box_rig rig;
  rig.value = 0.0;
  rig.options.zero_text = "auto";
  rig.options.zero_start = 16.0;
  rig.options.step = 1.0;
  rig.frame();
  rig.mouse(rig.center.x, rig.center.y);
  rig.button(true);
  rig.mouse(rig.center.x + 8.0f, rig.center.y);
  rig.button(false);
  EXPECT_DOUBLE_EQ(rig.value, 24.0);
}

// ── Toolbar: tools, place, style ───────────────────────────────────────────

TEST(edit_toolbar, EachElementGetsTheToolsForItsFields) {
  editor_model model(kSource);
  EXPECT_TRUE(edit_toolbar::tools_for(model).empty()); // nothing selected

  model.select(edit_target_id{kNodeA, ""});
  const std::vector<toolbar_tool> node = edit_toolbar::tools_for(model);
  const std::vector<toolbar_tool> expected = {
      toolbar_tool::shape,
      toolbar_tool::fill_color,
      toolbar_tool::line_color,
      toolbar_tool::text_color,
      toolbar_tool::font,
      toolbar_tool::font_size,
      toolbar_tool::copy_style,
      toolbar_tool::clear_style,
      toolbar_tool::duplicate,
      toolbar_tool::remove};
  EXPECT_EQ(node, expected);

  model.select(edit_target_id{"root/children/1/children/2", ""});
  const std::vector<toolbar_tool> link = edit_toolbar::tools_for(model);
  EXPECT_NE(std::find(link.begin(), link.end(), toolbar_tool::line), link.end());
  EXPECT_NE(std::find(link.begin(), link.end(), toolbar_tool::arrow), link.end());
  EXPECT_EQ(std::find(link.begin(), link.end(), toolbar_tool::shape), link.end());

  model.select(edit_target_id{"root/children/0", "title"});
  const std::vector<toolbar_tool> plot = edit_toolbar::tools_for(model);
  EXPECT_EQ(std::find(plot.begin(), plot.end(), toolbar_tool::shape), plot.end());
  EXPECT_NE(std::find(plot.begin(), plot.end(), toolbar_tool::font_size), plot.end());

  // The root and the image options cannot be duplicated or deleted.
  model.select(edit_target_id{"root", ""});
  const std::vector<toolbar_tool> root = edit_toolbar::tools_for(model);
  EXPECT_EQ(std::find(root.begin(), root.end(), toolbar_tool::remove), root.end());
  model.select(edit_target_id{"image", ""});
  const std::vector<toolbar_tool> image = edit_toolbar::tools_for(model);
  EXPECT_EQ(std::find(image.begin(), image.end(), toolbar_tool::duplicate), image.end());
  EXPECT_EQ(std::find(image.begin(), image.end(), toolbar_tool::copy_style), image.end());
}

bool has(const std::vector<toolbar_tool>& tools, toolbar_tool tool) {
  return std::find(tools.begin(), tools.end(), tool) != tools.end();
}

TEST(edit_toolbar, ChartsGetColourSchemesAndSeriesGetMarkers) {
  editor_model model(kSource);
  model.select(edit_target_id{"root/children/0/children/0", ""}); // a PlotLine
  const std::vector<toolbar_tool> line = edit_toolbar::tools_for(model);
  EXPECT_TRUE(has(line, toolbar_tool::marker));
  EXPECT_TRUE(has(line, toolbar_tool::marker_size));
  EXPECT_TRUE(has(line, toolbar_tool::line_weight));
  EXPECT_FALSE(has(line, toolbar_tool::colormap)); // it has a colour of its own

  model.select(edit_target_id{"root/children/0", ""}); // the Plot
  EXPECT_TRUE(has(edit_toolbar::tools_for(model), toolbar_tool::colormap));
  EXPECT_EQ(edit_toolbar::colormap_owner(model), "root/children/0");

  // A pie chart's colours are its plot's scheme.
  model.select(edit_target_id{kPie, ""});
  const std::vector<toolbar_tool> pie = edit_toolbar::tools_for(model);
  EXPECT_TRUE(has(pie, toolbar_tool::colormap));
  EXPECT_FALSE(has(pie, toolbar_tool::copy_style)); // nothing of its own to copy
  EXPECT_EQ(edit_toolbar::colormap_owner(model), "root/children/3");
  const commit_result r = edit_toolbar::set_colormap(model, "Pastel");
  ASSERT_TRUE(r.ok) << r.error;
  EXPECT_NE(r.source.find("  - type: Plot\n      children:"), std::string::npos);
  EXPECT_NE(r.source.find("colormap: Pastel"), std::string::npos) << r.source;
  ASSERT_TRUE(model.selection());
  EXPECT_EQ(model.selection()->element, kPie); // still the pie

  // Nodes have colours of their own and no plot: no scheme.
  model.select(edit_target_id{kNodeA, ""});
  EXPECT_EQ(edit_toolbar::colormap_owner(model), "");
}

TEST(edit_toolbar, SitsAboveTheSelectionThenUnderItThenAtTheTop) {
  const layout_box area{0, 30, 600, 400};
  // Room above: centred over it, 10 px up.
  const layout_box above = edit_toolbar::place(layout_box{200, 200, 100, 50}, area, 200, 40);
  EXPECT_FLOAT_EQ(above.x, 150.0f);
  EXPECT_FLOAT_EQ(above.y, 150.0f);
  // No room above: under it.
  const layout_box under = edit_toolbar::place(layout_box{200, 40, 100, 50}, area, 200, 40);
  EXPECT_FLOAT_EQ(under.y, 100.0f);
  // Neither (the selection fills the figure): at the top, inside.
  const layout_box top = edit_toolbar::place(layout_box{0, 30, 600, 400}, area, 200, 40);
  EXPECT_FLOAT_EQ(top.y, 36.0f);
  EXPECT_FLOAT_EQ(top.x, 200.0f);
  // Near an edge: kept inside.
  const layout_box edge = edit_toolbar::place(layout_box{560, 300, 30, 30}, area, 200, 40);
  EXPECT_FLOAT_EQ(edge.x + edge.width, 594.0f);
  // No selection drawn: top centre.
  const layout_box none = edit_toolbar::place(std::nullopt, area, 200, 40);
  EXPECT_FLOAT_EQ(none.x, 200.0f);
  EXPECT_FLOAT_EQ(none.y, 36.0f);
}

TEST(edit_toolbar, CopiedStyleGoesToTheNextElementAsOneUndoStep) {
  editor_model model(kSource);
  edit_toolbar bar;
  model.select(edit_target_id{kNodeA, ""});
  ASSERT_TRUE(bar.copy_style(model));
  EXPECT_TRUE(bar.style_copied());
  model.select(edit_target_id{kNodeB, ""});
  const commit_result r = bar.paste_style(model);
  ASSERT_TRUE(r.ok) << r.error;
  EXPECT_FALSE(bar.style_copied());
  EXPECT_NE(
      r.source.find(
          "{type: FlowNode, id: b, text: Stop, shape: Stadium, fill_color: \"#BEF6AC\", text_color: \"#C0392B\"}"),
      std::string::npos)
      << r.source;
  ASSERT_TRUE(model.undo());
  EXPECT_EQ(model.source(), kSource);
}

TEST(edit_toolbar, DrawingTheToolbarPastesACopiedStyleOnTheNextSelection) {
  headless_imgui gui;
  editor_model model(kSource);
  edit_toolbar bar;
  tree_menu menu;
  const layout_box area{0, 0, 600, 500};
  model.select(edit_target_id{kNodeA, ""});
  bar.copy_style(model);
  inspector_events events;
  gui.frame([&] { events = bar.draw(model, menu, area, layout_box{100, 300, 80, 40}); }, false);
  EXPECT_FALSE(events.source); // still on the element copied from
  EXPECT_TRUE(bar.style_copied());
  model.select(edit_target_id{kNodeB, ""});
  gui.frame([&] { events = bar.draw(model, menu, area, layout_box{300, 300, 80, 40}); }, false);
  ASSERT_TRUE(events.source);
  EXPECT_NE(events.source->find("id: b, text: Stop, shape: Stadium"), std::string::npos);
  EXPECT_FALSE(bar.style_copied());
}

TEST(edit_toolbar, ClearStyleRemovesColoursAndTextStyleOnly) {
  editor_model model(kSource);
  edit_toolbar bar;
  model.select(edit_target_id{kNodeA, ""});
  const commit_result r = bar.clear_style(model);
  ASSERT_TRUE(r.ok) << r.error;
  EXPECT_NE(r.source.find("{type: FlowNode, id: a, text: Start, shape: Stadium}"), std::string::npos) << r.source;
}

TEST(edit_toolbar, InheritedFontSizeComesFromTheNearestAncestor) {
  editor_model model(kSource);
  model.select(edit_target_id{kNodeA, ""});
  EXPECT_DOUBLE_EQ(model.inherited_font_size(), 20.0); // the layout's
  model.select(edit_target_id{"image", ""});
  EXPECT_DOUBLE_EQ(model.inherited_font_size(), 16.0); // the image's default
}

TEST(edit_toolbar, DrawsOverTheFigureAndOwnsItsBox) {
  headless_imgui gui;
  editor_model model(kSource);
  edit_toolbar bar;
  tree_menu menu;
  const layout_box area{0, 0, 600, 500};
  gui.frame([&] { bar.draw(model, menu, area, std::nullopt); }, false);
  EXPECT_FALSE(bar.contains(300, 20)); // nothing selected: no toolbar
  model.select(edit_target_id{kNodeA, ""});
  const layout_box anchor{250, 300, 100, 50};
  for (int i = 0; i < 3; ++i) {
    inspector_events events;
    gui.frame([&] { events = bar.draw(model, menu, area, anchor); }, false);
    EXPECT_FALSE(events.source);
    EXPECT_TRUE(events.error.empty());
  }
  const layout_box r = bar.rect();
  EXPECT_GT(r.width, 200.0f); // ten tools
  EXPECT_LE(r.y + r.height, anchor.y); // above the node
  EXPECT_TRUE(bar.contains(r.x + 5, r.y + 5));
  EXPECT_FALSE(bar.contains(anchor.x + 5, anchor.y + 5));
  // A hidden figure (a data table shown) hides it.
  gui.frame([&] { bar.draw(model, menu, layout_box{}, anchor); }, false);
  EXPECT_FALSE(bar.contains(r.x + 5, r.y + 5));
}

TEST(edit_toolbar, LibraryShapesOpenInTabsAndAClickChoosesOne) {
  headless_imgui gui;
  editor_model model(kSource);
  edit_toolbar bar;
  tree_menu menu;
  model.select(edit_target_id{"root/children/2", ""});
  const std::vector<toolbar_tool> tools = edit_toolbar::tools_for(model);
  ASSERT_FALSE(tools.empty());
  EXPECT_EQ(tools.front(), toolbar_tool::shape);
  EXPECT_NE(std::find(tools.begin(), tools.end(), toolbar_tool::line_weight), tools.end());
  const layout_box area{0, 0, 600, 500};
  const layout_box anchor{250, 300, 80, 50};
  inspector_events events;
  const auto frame = [&] { gui.frame([&] { events = bar.draw(model, menu, area, anchor); }, false); };
  frame();
  frame();
  const layout_box r = bar.rect();
  const float button = r.height - 12.0f; // the bar less its padding
  const auto click = [&](float x, float y) {
    gui.input([&](ImGuiIO& io) { io.AddMousePosEvent(x, y); });
    frame();
    gui.input([&](ImGuiIO& io) { io.AddMouseButtonEvent(0, true); });
    frame();
    gui.input([&](ImGuiIO& io) { io.AddMouseButtonEvent(0, false); });
    frame();
  };
  click(r.x + 6 + button * 0.5f, r.y + r.height * 0.5f);
  frame();
  EXPECT_FALSE(events.source);
  // The grid starts under the bar, after the category tabs: its first cell
  // is the first shape of the open tab.
  bool chosen = false;
  for (float y = r.y + r.height + 30.0f; y < r.y + r.height + 120.0f && !chosen; y += 6.0f) {
    click(r.x + 30.0f, y);
    chosen = events.source.has_value();
  }
  ASSERT_TRUE(chosen);
  EXPECT_EQ(events.source->find("shape: cloud"), std::string::npos) << *events.source;
}

TEST(edit_toolbar, DumpToolbarForInspection) {
  const char* dir = std::getenv("NYMPH_EDIT_DUMP");
  if (dir == nullptr) {
    GTEST_SKIP();
  }
  headless_imgui gui;
  editor_model model(kSource);
  edit_toolbar bar;
  tree_menu menu;
  model.select(edit_target_id{kNodeA, ""});
  const layout_box area{0, 0, 600, 500};
  const layout_box anchor{150, 120, 100, 50};
  for (int i = 0; i < 3; ++i) {
    gui.frame([&] { bar.draw(model, menu, area, anchor); }, false);
  }
  const auto dump = [&](const char* name) {
    const std::vector<uint8_t> png = encode_png(gui.snapshot(), "", "");
    std::ofstream out(std::string(dir) + "/" + name, std::ios::binary);
    out.write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
  };
  dump("toolbar.png");
  // Open the fill palette and the shape grid by clicking their buttons.
  const layout_box r = bar.rect();
  const float s = r.height - 12.0f; // the button size: the bar less its padding
  const auto click = [&](float x) {
    gui.input([&](ImGuiIO& io) { io.AddMousePosEvent(x, r.y + r.height * 0.5f); });
    gui.frame([&] { bar.draw(model, menu, area, anchor); }, false);
    gui.input([&](ImGuiIO& io) { io.AddMouseButtonEvent(0, true); });
    gui.frame([&] { bar.draw(model, menu, area, anchor); }, false);
    gui.input([&](ImGuiIO& io) { io.AddMouseButtonEvent(0, false); });
    for (int i = 0; i < 3; ++i) {
      gui.frame([&] { bar.draw(model, menu, area, anchor); }, false);
    }
  };
  click(r.x + 6 + s * 1.5f + 4);
  dump("toolbar_fill.png");
  click(r.x + r.width + 100.0f); // away: the palette closes
  click(r.x + 6 + s * 0.5f);
  dump("toolbar_shape.png");
  click(r.x + r.width + 100.0f);

  // A series: its markers; a pie chart: the plot's colour schemes.
  for (const auto& [path, name] :
       {std::pair{"root/children/0/children/0", "toolbar_series.png"}, std::pair{kPie, "toolbar_pie.png"}}) {
    model.select(edit_target_id{path, ""});
    for (int i = 0; i < 3; ++i) {
      gui.frame([&] { bar.draw(model, menu, area, anchor); }, false);
    }
    const layout_box b = bar.rect();
    const std::vector<toolbar_tool> tools = edit_toolbar::tools_for(model);
    const bool series = tools.front() != toolbar_tool::colormap;
    // The marker is the fifth button (fill, line, weight box, marker); the
    // scheme the first.
    gui.input([&](ImGuiIO& io) { io.AddMousePosEvent(b.x + 8 + s * (series ? 5.7f : 0.6f), b.y + b.height * 0.5f); });
    gui.frame([&] { bar.draw(model, menu, area, anchor); }, false);
    gui.input([&](ImGuiIO& io) { io.AddMouseButtonEvent(0, true); });
    gui.frame([&] { bar.draw(model, menu, area, anchor); }, false);
    gui.input([&](ImGuiIO& io) { io.AddMouseButtonEvent(0, false); });
    for (int i = 0; i < 3; ++i) {
      gui.frame([&] { bar.draw(model, menu, area, anchor); }, false);
    }
    dump(name);
    click(b.x + b.width + 100.0f);
  }
}

} // namespace
} // namespace bdg::nymph
