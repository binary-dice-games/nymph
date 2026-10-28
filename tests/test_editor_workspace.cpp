// MIT License © 2026 Binary Dice Games
/// @file test_editor_workspace.cpp
/// @brief spec/15_web_app plan steps 2 and 3: the editor workspace both hosts
///        drive, headless (no GL): pointer and key routing, the divider,
///        shortcuts, and the save and CSV requests it makes of its host.
#include <gtest/gtest.h>
#include <cmath>
#include <string>
#include <vector>
#include "workspace/editor_workspace.hpp"

namespace bdg::nymph {
namespace {

const char* const kSource =
    "image: {width: 400, height: 300}\n"
    "root:\n"
    "  type: Plot\n"
    "  title: Revenue\n"
    "  children:\n"
    "    - {type: PlotLine, label: a, ys: $y}\n"
    "data: {y: [1, 3, 2]}\n";

struct fake_host : workspace_host {
  void set_cursor(cursor_shape shape) override {
    cursor = shape;
  }
  void request_save() override {
    ++saves;
  }
  void request_csv_import(const std::string& table) override {
    imports.push_back(table);
  }
  void request_csv_export(const std::string& table, const std::string& file_name, const std::string& csv) override {
    exports.push_back(table + "|" + file_name + "|" + csv);
  }
  void request_size(float w, float h) override {
    sizes.emplace_back(w, h);
  }
  void changed(const commit_result& r) override {
    changes.push_back(r);
  }
  cursor_shape cursor = cursor_shape::arrow;
  int saves = 0;
  std::vector<std::string> imports;
  std::vector<std::string> exports;
  std::vector<std::pair<float, float>> sizes;
  std::vector<commit_result> changes;
};

/// A headless workspace on kSource with the panel shown, 900 x 400 CSS px.
struct workspace_fixture : ::testing::Test {
  void SetUp() override {
    ws = std::make_unique<editor_workspace>(host, view_options{view_fit::fill, /*cpu_textures=*/true, true}, "", workspace_gl{});
    ws->view().set_source(kSource);
    ws->set_saved_source(kSource);
    ws->resize(900.0f, 400.0f, 1.0f);
    ASSERT_EQ(ws->show_panel(true), "");
    run_frames();
  }
  void run_frames(int n = 4) {
    for (int i = 0; i < n; ++i) {
      now += 1.0 / 60.0;
      ws->frame(now);
    }
  }
  void click(float x, float y, int button = 0) {
    ws->pointer({pointer_event::kind::move, x, y, 0});
    ws->pointer({pointer_event::kind::down, x, y, button});
    run_frames(1);
    ws->pointer({pointer_event::kind::up, x, y, button});
    run_frames(2);
  }
  fake_host host;
  std::unique_ptr<editor_workspace> ws;
  double now = 100.0;
};

TEST_F(workspace_fixture, LaysOutTheFigureLeftOfTheStripUnderTheTabs) {
  EXPECT_FLOAT_EQ(ws->strip_width(), kDefaultStripWidth);
  EXPECT_FLOAT_EQ(ws->tab_height(), editor_panel::kTabBarHeight);
  const layout_box figure = ws->panel()->figure_box();
  EXPECT_FLOAT_EQ(figure.width, 900.0f - kDefaultStripWidth);
  EXPECT_FLOAT_EQ(figure.y, editor_panel::kTabBarHeight);
}

TEST_F(workspace_fixture, GoesIdleWhenNothingHappens) {
  run_frames(12);
  EXPECT_FALSE(ws->view().wants_frame());
  EXPECT_FALSE(ws->panel()->wants_frame());
  // Also with a selection (the toolbar is shown) and the source pane.
  ws->panel()->model().select(edit_target_id{"root", ""});
  ws->panel()->refresh();
  ws->show_source(true);
  run_frames(30);
  EXPECT_FALSE(ws->wants_frame());
}

TEST_F(workspace_fixture, AClickOnTheTitleSelectsIt) {
  const std::optional<edit_target> title = ws->view().target_for("root", "title");
  ASSERT_TRUE(title);
  const float y0 = ws->panel()->figure_box().y;
  click(title->x + title->width / 2, y0 + title->y + title->height / 2);
  ASSERT_TRUE(ws->panel()->model().selection());
  EXPECT_EQ(ws->panel()->model().selection()->element, "root");
}

TEST_F(workspace_fixture, ANaturalLayoutIsCentredInTheFigureAreaAndStillClickable) {
  EXPECT_FLOAT_EQ(ws->figure_offset().x, 0.0f); // fill: the layout is the box
  ws->view().set_fit(view_fit::natural);
  run_frames();
  const layout_box box = ws->panel()->figure_box();
  const ImVec2 off = ws->figure_offset();
  EXPECT_FLOAT_EQ(off.x, std::floor((box.width - 400.0f) * 0.5f));
  EXPECT_GT(off.y, 0.0f);
  const std::optional<edit_target> title = ws->view().target_for("root", "title");
  ASSERT_TRUE(title);
  click(off.x + title->x + title->width / 2, box.y + off.y + title->y + title->height / 2);
  ASSERT_TRUE(ws->panel()->model().selection());
  EXPECT_EQ(ws->panel()->model().selection()->element, "root");
}

TEST_F(workspace_fixture, ThePointerOverTheStripGoesToThePanelNotTheFigure) {
  const float strip_x = ws->panel()->strip_x();
  ws->pointer({pointer_event::kind::move, strip_x + 50.0f, 100.0f, 0});
  EXPECT_FALSE(ws->view().hovered());
  EXPECT_TRUE(ws->wants_frame());
}

TEST_F(workspace_fixture, DraggingTheDividerResizesTheStripAndDoubleClickResetsIt) {
  const float strip_x = ws->panel()->strip_x();
  ws->pointer({pointer_event::kind::move, strip_x, 200.0f, 0});
  EXPECT_EQ(host.cursor, cursor_shape::resize_horizontal);
  ws->pointer({pointer_event::kind::down, strip_x, 200.0f, 0});
  ws->pointer({pointer_event::kind::move, strip_x - 100.0f, 200.0f, 0});
  ws->pointer({pointer_event::kind::up, strip_x - 100.0f, 200.0f, 0});
  EXPECT_FLOAT_EQ(ws->strip_width(), kDefaultStripWidth + 100.0f);
  EXPECT_FLOAT_EQ(ws->view().geometry().layout_width, 900.0f - kDefaultStripWidth - 100.0f);
  const float x = ws->panel()->strip_x();
  ws->pointer({pointer_event::kind::down, x, 200.0f, 0});
  ws->pointer({pointer_event::kind::up, x, 200.0f, 0});
  ws->pointer({pointer_event::kind::down, x, 200.0f, 0});
  ws->pointer({pointer_event::kind::up, x, 200.0f, 0});
  EXPECT_FLOAT_EQ(ws->strip_width(), kDefaultStripWidth);
}

TEST_F(workspace_fixture, CtrlZUndoesACommitAndTellsTheHost) {
  ws->panel()->model().select(edit_target_id{"root", ""});
  const commit_result r = ws->panel()->model().commit("title", "Sales");
  ASSERT_TRUE(r.ok);
  inspector_events e;
  e.source = r.source;
  ws->apply(e);
  ASSERT_FALSE(host.changes.empty());
  EXPECT_EQ(host.changes.back().label, "Set title");
  EXPECT_TRUE(ws->modified());
  EXPECT_EQ(ws->key(window_key::z, key_mods{true, false, false}, true, false), key_outcome::consumed);
  EXPECT_EQ(std::string(ws->view().source()), kSource);
  EXPECT_FALSE(ws->modified());
  EXPECT_EQ(host.changes.back().label, "Undo Set title");
}

TEST_F(workspace_fixture, CtrlSAsksTheHostToSave) {
  EXPECT_EQ(ws->key(window_key::s, key_mods{true, false, false}, true, false), key_outcome::consumed);
  EXPECT_EQ(host.saves, 1);
}

TEST_F(workspace_fixture, TheHostKeepsItsOwnKeys) {
  EXPECT_EQ(ws->key(window_key::q, key_mods{}, true, false), key_outcome::unhandled);
  EXPECT_EQ(ws->key(window_key::r, key_mods{}, true, false), key_outcome::unhandled);
}

TEST_F(workspace_fixture, ARightClickOnAnElementOpensItsMenu) {
  const std::optional<edit_target> title = ws->view().target_for("root", "title");
  ASSERT_TRUE(title);
  const float y0 = ws->panel()->figure_box().y;
  click(title->x + 4.0f, y0 + title->y + 4.0f, 1);
  EXPECT_TRUE(ws->panel()->popup_open());
  EXPECT_TRUE(ws->wants_keyboard()); // Escape closes the menu, not the window
}

TEST_F(workspace_fixture, CsvRequestsGoToTheHostWithHostFiles) {
  ws->set_host_files(true);
  ws->panel()->grid().ask_export("");
  ws->panel()->grid().ask_import("");
  ws->panel()->refresh();
  run_frames(1);
  ASSERT_EQ(host.exports.size(), 1u);
  EXPECT_EQ(host.exports[0], "|data.csv|y\n1\n3\n2\n");
  ASSERT_EQ(host.imports.size(), 1u);
  EXPECT_EQ(host.imports[0], "");
}

TEST_F(workspace_fixture, ImportCsvReplacesTheTableAsOneUndoStep) {
  EXPECT_EQ(ws->import_csv("", "y\n5\n6\n"), "");
  EXPECT_NE(std::string(ws->view().source()).find("5"), std::string::npos);
  ws->undo();
  EXPECT_EQ(std::string(ws->view().source()), kSource);
}

TEST_F(workspace_fixture, ABadCsvIsRefusedWithTheReason) {
  EXPECT_NE(ws->import_csv("", "a,b\n\"1\n"), "");
  EXPECT_EQ(std::string(ws->view().source()), kSource);
  EXPECT_FALSE(ws->panel()->panel().error().empty());
}

TEST_F(workspace_fixture, HidingThePanelGivesTheFigureTheWholeBox) {
  ws->show_panel(false);
  EXPECT_FLOAT_EQ(ws->strip_width(), 0.0f);
  run_frames(2);
  EXPECT_FLOAT_EQ(ws->view().geometry().layout_width, 900.0f);
}

} // namespace
} // namespace bdg::nymph
