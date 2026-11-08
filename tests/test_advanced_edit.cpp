// MIT License © 2026 Binary Dice Games
/// @file test_advanced_edit.cpp
/// @brief The editor's advanced edit mode (spec/14_editor_workspace/DESIGN.md
///        section 9), headless: the source editor in place of the elements,
///        fields and data tabs; the text is the user's alone, a text that
///        loads becomes the figure, one that does not is logged and leaves
///        the last figure; basic edits and agents' changes are off.
#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "mcp/editor_tools.hpp"
#include "workspace/editor_workspace.hpp"
#include "workspace/workspace_tools.hpp"
#include "workspace/yaml_language.hpp"

namespace bdg::nymph {
namespace {

using mcp::json;

const char* const kSource =
    "image: {width: 400, height: 300}\n"
    "root:\n"
    "  type: Plot\n"
    "  title: Revenue\n"
    "  children:\n"
    "    - {type: PlotLine, label: a, ys: $y}\n"
    "data: {y: [1, 3, 2]}\n";

/// kSource with another title.
std::string retitled(const std::string& title) {
  std::string s = kSource;
  s.replace(s.find("Revenue"), 7, title);
  return s;
}

struct fake_host : workspace_host {
  void request_save() override {
    ++saves;
  }
  void changed(const commit_result& r) override {
    changes.push_back(r);
  }
  int saves = 0;
  std::vector<commit_result> changes;
};

struct advanced_fixture : ::testing::Test {
  void SetUp() override {
    ws = std::make_unique<editor_workspace>(
        host, view_options{view_fit::fill, /*cpu_textures=*/true, true}, "", workspace_gl{});
    ws->resize(1200.0f, 600.0f, 1.0f);
    ws->open(kSource, true, true);
    run(3);
    ASSERT_EQ(ws->set_advanced(true), "");
    run(3);
  }
  void run(int frames = 1, double step = 1.0 / 60.0) {
    for (int i = 0; i < frames; ++i) {
      now += step;
      ws->frame(now);
    }
  }
  /// Waits out the pause after typing.
  void pause() {
    run(1, source_editor::kParseDelay + 0.05);
    run(2);
  }
  /// Replaces the whole text as an edit of the user's (not reset()).
  void type_all(const std::string& text) {
    TextEditor& e = ws->panel()->source().editor();
    e.SelectAll();
    e.ReplaceTextInCurrentCursor(text);
    ws->panel()->refresh();
    run(1);
  }
  void press(window_key key, key_mods mods = {}) {
    ws->key(key, mods, true, false);
    run(1);
    ws->key(key, mods, false, false);
    run(1);
  }
  void type(const std::string& chars) {
    for (const char c : chars) {
      ws->key(window_key::other, key_mods{}, true, false);
      ws->text(static_cast<char32_t>(c));
      ws->key(window_key::other, key_mods{}, false, false);
      run(1);
    }
  }
  std::string text() const {
    return ws->panel()->source().text();
  }
  fake_host host;
  std::unique_ptr<editor_workspace> ws;
  double now = 100.0;
};

// ── The mode ─────────────────────────────────────────────────────────────────

TEST_F(advanced_fixture, ShowsTheSourceInAWiderStripWithoutDataTabs) {
  ASSERT_TRUE(ws->advanced());
  EXPECT_EQ(text(), kSource);
  EXPECT_GE(ws->strip_width(), kAdvancedStripWidth);
  EXPECT_FALSE(ws->view().edit_mode());
  EXPECT_FALSE(ws->panel()->data_tab());
  EXPECT_TRUE(ws->panel()->source().focused());
  EXPECT_TRUE(ws->wants_keyboard());
}

TEST_F(advanced_fixture, ATextThatLoadsBecomesTheFigure) {
  type_all(retitled("Sales"));
  EXPECT_EQ(std::string(ws->view().source()), kSource); // not before the pause
  pause();
  EXPECT_EQ(std::string(ws->view().source()), retitled("Sales"));
  EXPECT_EQ(ws->panel()->model().source(), retitled("Sales"));
  EXPECT_EQ(ws->panel()->source().error(), "");
  ASSERT_FALSE(host.changes.empty());
  EXPECT_EQ(host.changes.back().label, "Edit source");
  EXPECT_TRUE(ws->modified());
}

TEST_F(advanced_fixture, ATextThatDoesNotLoadIsLoggedAndTheLastFigureStays) {
  const std::string bad = "root: {type: Plot, bogus: 1}\n";
  type_all(bad);
  pause();
  EXPECT_EQ(std::string(ws->view().source()), kSource);
  EXPECT_EQ(ws->panel()->model().source(), kSource);
  EXPECT_NE(ws->panel()->source().error(), "");
  EXPECT_TRUE(ws->panel()->source_error());
  EXPECT_EQ(text(), bad); // the text is never rewritten
  EXPECT_TRUE(ws->modified());
  // Fixed: the figure follows and the log clears.
  type_all(retitled("Fixed"));
  pause();
  EXPECT_EQ(std::string(ws->view().source()), retitled("Fixed"));
  EXPECT_EQ(ws->panel()->source().error(), "");
}

TEST_F(advanced_fixture, KeysTypeIntoTheTextAndNothingRewritesIt) {
  // The end of the text (its last, empty line), then empty lines and a
  // comment, as keys: the regression the earlier source pane had.
  press(window_key::end, key_mods{true, false, false});
  for (int i = 0; i < 3; ++i) {
    press(window_key::enter);
  }
  type("# n");
  // The editor keeps the text's last line break.
  const std::string typed = std::string(kSource) + "\n\n\n# n\n";
  EXPECT_EQ(text(), typed);
  pause(); // a comment: it loads
  EXPECT_EQ(ws->panel()->model().source(), typed);
  EXPECT_EQ(text(), typed);
  const TextEditor::DocPos at = ws->panel()->source().editor().GetMainCursorPosition();
  type("ote");
  pause();
  EXPECT_EQ(text(), std::string(kSource) + "\n\n\n# note\n");
  EXPECT_EQ(ws->panel()->source().editor().GetMainCursorPosition().line, at.line);
  // Backspace over the empty lines, one key at a time.
  for (int i = 0; i < 9; ++i) {
    press(window_key::backspace);
  }
  pause();
  EXPECT_EQ(text(), kSource);
  EXPECT_EQ(ws->panel()->model().source(), kSource);
}

TEST_F(advanced_fixture, CtrlZUndoesTheTextNotTheModel) {
  press(window_key::end, key_mods{true, false, false});
  type("#");
  pause();
  ASSERT_EQ(text(), std::string(kSource) + "#\n");
  press(window_key::z, key_mods{true, false, false});
  pause();
  EXPECT_EQ(text(), kSource);
  EXPECT_EQ(ws->panel()->model().source(), kSource);
}

// ── Basic edits are off ──────────────────────────────────────────────────────

TEST_F(advanced_fixture, TheFigureTakesNoEditClicksOrMenus) {
  const std::optional<edit_target> title = ws->view().target_for("root", "title");
  if (title) { // edit mode is off: targets may not be recorded at all
    const float y0 = ws->panel()->figure_box().y;
    for (int button : {0, 1}) {
      const float x = title->x + 4.0f;
      const float y = y0 + title->y + 4.0f;
      ws->pointer({pointer_event::kind::move, x, y, 0});
      ws->pointer({pointer_event::kind::down, x, y, button});
      run(1);
      ws->pointer({pointer_event::kind::up, x, y, button});
      run(2);
    }
  }
  EXPECT_FALSE(ws->panel()->model().selection());
  EXPECT_FALSE(ws->panel()->popup_open());
  EXPECT_TRUE(ws->panel()->toolbar().rect().empty());
}

TEST_F(advanced_fixture, TreeShortcutsUndoAndCsvImportAreOff) {
  type_all(retitled("Sales"));
  pause();
  // A click on the log takes the keyboard from the text.
  const float x = ws->panel()->strip_x() + 40.0f;
  const float y = ws->css_height() - 12.0f;
  ws->pointer({pointer_event::kind::move, x, y, 0});
  ws->pointer({pointer_event::kind::down, x, y, 0});
  run(1);
  ws->pointer({pointer_event::kind::up, x, y, 0});
  run(2);
  ASSERT_FALSE(ws->panel()->source().focused());
  ws->key(window_key::z, key_mods{true, false, false}, true, false);
  ws->undo();
  run(2);
  EXPECT_EQ(ws->panel()->model().source(), retitled("Sales"));
  EXPECT_EQ(text(), retitled("Sales"));
  EXPECT_NE(ws->import_csv("", "y\n5\n"), "");
}

TEST_F(advanced_fixture, AgentsCanReadButNotChange) {
  workspace_tools tools(*ws, [] { return std::string("chart.yaml"); });
  const json source = mcp::call_editor_tool(tools, "editor_source", json::object());
  EXPECT_FALSE(source.value("isError", false));
  const json set = mcp::call_editor_tool(
      tools, "editor_set_fields", json{{"path", "root"}, {"fields", {{"title", "Hacked"}}}}, "agent:test");
  EXPECT_TRUE(set.value("isError", false));
  EXPECT_NE(set.dump().find("advanced"), std::string::npos);
  run(2);
  EXPECT_EQ(text(), kSource);
  EXPECT_EQ(ws->panel()->model().source(), kSource);
  // Back in the basic editor they can.
  ASSERT_EQ(ws->set_advanced(false), "");
  const json again = mcp::call_editor_tool(
      tools, "editor_set_fields", json{{"path", "root"}, {"fields", {{"title", "Agent"}}}}, "agent:test");
  EXPECT_FALSE(again.value("isError", false)) << again.dump();
}

TEST_F(advanced_fixture, TheLogIsThePanelErrorAgentsSee) {
  workspace_tools tools(*ws, [] { return std::string("chart.yaml"); });
  type_all("root: {type: Plot, bogus: 1}\n");
  pause();
  EXPECT_EQ(tools.panel_error(), ws->panel()->source().error());
  EXPECT_NE(tools.panel_error(), "");
}

// ── Saving and leaving ───────────────────────────────────────────────────────

TEST_F(advanced_fixture, SavingTriesTheTextTypedAMomentAgo) {
  type_all(retitled("Now"));
  EXPECT_EQ(ws->key(window_key::s, key_mods{true, false, false}, true, false), key_outcome::consumed);
  EXPECT_EQ(host.saves, 1);
  EXPECT_EQ(std::string(ws->view().source()), retitled("Now"));
}

TEST_F(advanced_fixture, LeavingKeepsAValidTextAsOneUndoStep) {
  type_all(retitled("One"));
  pause();
  type_all(retitled("Two"));
  pause();
  ASSERT_EQ(ws->set_advanced(false), "");
  EXPECT_FALSE(ws->advanced());
  EXPECT_TRUE(ws->view().edit_mode());
  EXPECT_FLOAT_EQ(ws->strip_width(), kDefaultStripWidth); // back to its basic width
  EXPECT_EQ(ws->panel()->model().source(), retitled("Two"));
  EXPECT_EQ(ws->panel()->model().undo_label(), "Edit source");
  ws->undo();
  EXPECT_EQ(std::string(ws->view().source()), kSource);
}

TEST_F(advanced_fixture, LeavingWithATextThatDoesNotLoadAsksFirst) {
  type_all(retitled("Good"));
  pause();
  type_all("root: [\n");
  pause();
  EXPECT_NE(ws->set_advanced(false), "");
  EXPECT_TRUE(ws->advanced());
  // Ctrl+U opens the dialog instead.
  EXPECT_EQ(ws->key(window_key::u, key_mods{true, false, false}, true, false), key_outcome::consumed);
  run(3);
  EXPECT_TRUE(ws->panel()->modal_open());
  EXPECT_TRUE(ws->advanced());
  // Discarding goes back with the last text that loaded.
  ASSERT_EQ(ws->set_advanced(false, true), "");
  EXPECT_FALSE(ws->advanced());
  EXPECT_EQ(ws->panel()->model().source(), retitled("Good"));
  EXPECT_EQ(std::string(ws->view().source()), retitled("Good"));
}

TEST_F(advanced_fixture, CtrlUTogglesTheMode) {
  EXPECT_EQ(ws->key(window_key::u, key_mods{true, false, false}, true, false), key_outcome::consumed);
  run(2);
  EXPECT_FALSE(ws->advanced());
  EXPECT_EQ(ws->key(window_key::u, key_mods{true, false, false}, true, false), key_outcome::consumed);
  run(2);
  EXPECT_TRUE(ws->advanced());
  EXPECT_EQ(text(), kSource);
}

TEST_F(advanced_fixture, GoesIdleWhenNothingHappens) {
  ws->pointer({pointer_event::kind::leave, 0.0f, 0.0f, 0});
  run(30);
  EXPECT_FALSE(ws->wants_frame());
}

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

} // namespace
} // namespace bdg::nymph
