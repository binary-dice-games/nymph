// MIT License © 2026 Binary Dice Games
/// @file test_source_pane.cpp
/// @brief spec/15_web_app plan step 5: the source pane (YAML highlighting,
///        typing applied after the pause, refused text marked and locking the
///        panel, model changes patched in, undo routing, selection and cursor
///        in step, repair state), headless.
#include <gtest/gtest.h>
#include <string>
#include "core/error.hpp"
#include "mcp/editor_tools.hpp"
#include "workspace/editor_workspace.hpp"
#include "workspace/workspace_tools.hpp"
#include "workspace/yaml_language.hpp"

namespace bdg::nymph {
namespace {

// ── Highlighting ─────────────────────────────────────────────────────────────

TEST(YamlLanguage, KeysTypeNamesReferencesAndValues) {
  const std::vector<yaml_token> t = yaml_tokens("  - {type: PlotLine, ys: $revenue, size: 2.5, on: true} # note");
  auto color_of = [&](const std::string& text) {
    for (const yaml_token& k : t) {
      if (k.text == text) {
        return k.color;
      }
    }
    ADD_FAILURE() << "no token " << text;
    return TextEditor::Color::text;
  };
  EXPECT_EQ(color_of("type"), TextEditor::Color::keyword);
  EXPECT_EQ(color_of("PlotLine"), TextEditor::Color::knownIdentifier);
  EXPECT_EQ(color_of("ys"), TextEditor::Color::keyword);
  EXPECT_EQ(color_of("$revenue"), TextEditor::Color::preprocessor);
  EXPECT_EQ(color_of("2.5"), TextEditor::Color::number);
  EXPECT_EQ(color_of("true"), TextEditor::Color::declaration);
  EXPECT_EQ(color_of("# note"), TextEditor::Color::comment);
}

TEST(YamlLanguage, QuotedStringsAndPlainWords) {
  const std::vector<yaml_token> t = yaml_tokens("title: \"a: b\" then words");
  ASSERT_GE(t.size(), 4u);
  EXPECT_EQ(t[0].text, "title");
  EXPECT_EQ(t[0].color, TextEditor::Color::keyword);
  EXPECT_EQ(t[2].text, "\"a: b\"");
  EXPECT_EQ(t[2].color, TextEditor::Color::string);
  EXPECT_EQ(t[3].color, TextEditor::Color::identifier);
}

// ── The pane in a workspace ──────────────────────────────────────────────────

const char* const kSource =
    "root:\n"
    "  type: Flowchart\n"
    "  title: Steps\n"
    "  children:\n"
    "    - {type: FlowNode, id: a, text: Start}\n"
    "    - {type: FlowNode, id: b, text: End}\n"
    "    - {type: FlowLink, from: a, to: b}\n";

struct recording_host : workspace_host {
  void changed(const commit_result& r) override {
    labels.push_back(r.label);
  }
  std::vector<std::string> labels;
};

struct pane_fixture : ::testing::Test {
  void SetUp() override {
    ws = std::make_unique<editor_workspace>(host, view_options{view_fit::fill, true, true}, "", workspace_gl{});
    ws->resize(1200.0f, 600.0f, 1.0f);
    ws->open(kSource, true, true);
    ws->show_source(true);
    run(3);
  }
  void run(int frames = 1, double step = 1.0 / 60.0) {
    for (int i = 0; i < frames; ++i) {
      now += step;
      ws->frame(now);
    }
  }
  /// Types `text` into the pane as replacement of its whole text, as the
  /// user would (an edit of the editor), then waits out the pause.
  void type_all(const std::string& text) {
    TextEditor& e = ws->panel()->pane().editor();
    e.SelectAll();
    e.ReplaceTextInCurrentCursor(text);
    ws->panel()->refresh(); // typing reaches the panel as keys, which ask for frames
    run(1);
    run(1, source_pane::kApplyDelay + 0.05);
    run(1);
  }
  editor_model& model() {
    return ws->panel()->model();
  }
  recording_host host;
  std::unique_ptr<editor_workspace> ws;
  double now = 50.0;
};

TEST_F(pane_fixture, ThePaneSitsBetweenTheFigureAndTheStrip) {
  const workspace_layout l = ws->panel()->layout();
  EXPECT_GT(l.source.width, 0.0f);
  EXPECT_FLOAT_EQ(l.left.x, 0.0f);
  EXPECT_FLOAT_EQ(l.source.x, l.left.width);
  EXPECT_FLOAT_EQ(l.source.x + l.source.width, l.strip.x);
  EXPECT_EQ(ws->panel()->pane().text(), kSource);
}

TEST_F(pane_fixture, TypingIsAppliedAfterThePauseAsOneUndoStep) {
  std::string changed = kSource;
  changed.replace(changed.find("Steps"), 5, "Pipeline");
  TextEditor& e = ws->panel()->pane().editor();
  e.SelectAll();
  e.ReplaceTextInCurrentCursor(changed);
  ws->panel()->refresh();
  run(1);
  EXPECT_EQ(model().source(), kSource) << "not before the pause";
  EXPECT_TRUE(ws->pending());
  run(1, source_pane::kApplyDelay + 0.05);
  EXPECT_EQ(model().source(), changed);
  EXPECT_NE(std::string(ws->view().source()).find("Pipeline"), std::string::npos);
  ASSERT_FALSE(host.labels.empty());
  EXPECT_EQ(host.labels.back(), "Edit source");
  ws->undo();
  EXPECT_EQ(model().source(), kSource);
}

TEST_F(pane_fixture, ARefusedTextIsMarkedAndLocksThePanelAndAgents) {
  std::string broken = kSource;
  broken.replace(broken.find("type: FlowLink"), 14, "type: FlowLinkk");
  type_all(broken);
  EXPECT_EQ(model().source(), kSource) << "the model keeps the last good source";
  EXPECT_TRUE(ws->panel()->source_ahead());
  EXPECT_TRUE(ws->panel()->pane().editor().HasSquiggles());
  EXPECT_FALSE(ws->panel()->locked_reason().empty());
  workspace_tools tools(*ws, [] { return std::string("chart.yaml"); });
  const mcp::json r = mcp::call_editor_tool(tools, "editor_set_fields", {{"path", "root"}, {"fields", {{"title", "X"}}}}, "agent:test");
  EXPECT_TRUE(r.value("isError", false));
  EXPECT_EQ(model().source(), kSource);
  // Fixing the text applies it and unlocks.
  std::string fixed = kSource;
  fixed.replace(fixed.find("Steps"), 5, "Fixed");
  type_all(fixed);
  EXPECT_FALSE(ws->panel()->source_ahead());
  EXPECT_FALSE(ws->panel()->pane().editor().HasSquiggles());
  EXPECT_EQ(model().source(), fixed);
}

TEST_F(pane_fixture, AChangeElsewhereReplacesOnlyTheSpanThatDiffers) {
  TextEditor& e = ws->panel()->pane().editor();
  e.SetCursor(TextEditor::DocPos(5, 4));
  run(2);
  const commit_result r = model().set_fields("root", {{"title", std::optional<std::string>("Renamed")}});
  ASSERT_TRUE(r.ok) << r.error;
  inspector_events ev;
  ev.source = r.source;
  ws->apply(ev);
  run(2);
  EXPECT_EQ(ws->panel()->pane().text(), r.source);
  EXPECT_EQ(e.GetMainCursorPosition().line, 5u) << "the cursor stays";
}

TEST_F(pane_fixture, CtrlZInThePaneIsTheDocumentsUndoWhileInStep) {
  model().select(edit_target_id{"root", ""});
  const commit_result r = model().commit("title", "Renamed");
  ASSERT_TRUE(r.ok);
  inspector_events ev;
  ev.source = r.source;
  ws->apply(ev);
  run(2);
  // Focus is the pane's when it had the keyboard: give it the keyboard.
  ws->panel()->show_source(true, 3);
  run(3);
  ASSERT_TRUE(ws->panel()->source_focused());
  EXPECT_EQ(ws->key(window_key::z, key_mods{true, false, false}, true, false), key_outcome::consumed);
  EXPECT_EQ(model().source(), kSource);
  run(2);
  EXPECT_EQ(ws->panel()->pane().text(), kSource);
}

TEST_F(pane_fixture, SelectionMovesTheCursorAndTheCursorSelects) {
  ws->panel()->model().select(edit_target_id{"root/children/1", ""});
  ws->panel()->refresh();
  run(2);
  EXPECT_EQ(ws->panel()->pane().editor().GetMainCursorPosition().line, 5u);
  // The user puts the cursor on the link's line.
  ws->panel()->pane().editor().SetCursor(TextEditor::DocPos(6, 2));
  ws->panel()->refresh();
  run(2);
  run(1, source_pane::kApplyDelay + 0.05);
  run(1);
  ASSERT_TRUE(model().selection());
  EXPECT_EQ(model().selection()->element, "root/children/2");
}

struct clipboard_host : recording_host {
  bool has_clipboard() const override {
    return true;
  }
  std::string clipboard() override {
    return "# pasted\n";
  }
};

TEST(SourcePaneKeys, CtrlVPastesTheHostsClipboardIntoThePane) {
  clipboard_host host;
  editor_workspace ws(host, view_options{view_fit::fill, true, true}, "", workspace_gl{});
  ws.resize(1200.0f, 600.0f, 1.0f);
  ws.open(kSource, true, true);
  ws.show_source(true, 1);
  double now = 1.0;
  for (int i = 0; i < 4; ++i) {
    ws.frame(now += 0.02);
  }
  ASSERT_TRUE(ws.panel()->source_focused());
  ws.key(window_key::other, key_mods{true, false, false}, true, false); // Control
  ws.key(window_key::v, key_mods{true, false, false}, true, false);
  ws.key(window_key::v, key_mods{true, false, false}, false, false);
  ws.key(window_key::other, key_mods{}, false, false);
  for (int i = 0; i < 4; ++i) {
    ws.frame(now += 0.02);
  }
  EXPECT_EQ(ws.panel()->pane().text().rfind("# pasted\n", 0), 0u) << ws.panel()->pane().text().substr(0, 40);
}

TEST_F(pane_fixture, AClickInThePaneGivesItTheKeyboard) {
  const layout_box src = ws->panel()->layout().source;
  const float x = src.x + 150.0f, y = src.y + 20.0f;
  ws->pointer({pointer_event::kind::move, x, y, 0});
  ws->pointer({pointer_event::kind::down, x, y, 0});
  run(1);
  ws->pointer({pointer_event::kind::up, x, y, 0});
  run(2);
  ASSERT_TRUE(ws->wants_keyboard());
  ws->text(U'Q');
  run(2);
  EXPECT_NE(ws->panel()->pane().text().find('Q'), std::string::npos) << ws->panel()->pane().text().substr(0, 60);
}

TEST_F(pane_fixture, CtrlUTogglesThePane) {
  EXPECT_TRUE(ws->panel()->source_shown());
  ws->key(window_key::u, key_mods{true, false, false}, true, false);
  EXPECT_FALSE(ws->panel()->source_shown());
  EXPECT_FLOAT_EQ(ws->panel()->layout().source.width, 0.0f);
}

TEST(SourcePaneRepair, ABrokenSourceOpensInRepairStateUntilItBinds) {
  recording_host host;
  editor_workspace ws(host, view_options{view_fit::fill, true, true}, "", workspace_gl{});
  ws.resize(1000.0f, 600.0f, 1.0f);
  const std::string broken = "root:\n  type: Plot\n  children:\n    - {type: PlotLine, ys: [1, 2], bogus: 3}\n";
  EXPECT_THROW(ws.open(broken, true, false), error);
  ws.open(broken, true, true);
  ASSERT_TRUE(ws.repairing());
  EXPECT_TRUE(ws.panel()->source_shown());
  EXPECT_FALSE(ws.panel()->locked_reason().empty());
  double now = 1.0;
  ws.frame(now += 0.02);
  TextEditor& e = ws.panel()->pane().editor();
  e.SelectAll();
  e.ReplaceTextInCurrentCursor("root:\n  type: Plot\n  children:\n    - {type: PlotLine, ys: [1, 2]}\n");
  ws.panel()->refresh();
  ws.frame(now += 0.02);
  ws.frame(now += 0.4);
  ws.frame(now += 0.02);
  EXPECT_FALSE(ws.repairing());
  EXPECT_TRUE(ws.view().has_figure());
  EXPECT_FALSE(ws.panel()->model().can_undo()) << "the broken text is no state to go back to";
  EXPECT_TRUE(ws.modified());
}

} // namespace
} // namespace bdg::nymph
