// MIT License © 2026 Binary Dice Games
/// @file test_editor_attribution.cpp
/// @brief spec/15_web_app plan step 4: every change records what it was and
///        who made it; groups make one undo step; the editor tools run in
///        process (call_editor_tool) with their origin.
#include <gtest/gtest.h>
#include <string>
#include "edit/editor_model.hpp"
#include "mcp/editor_tools.hpp"

namespace bdg::nymph {
namespace {

const char* const kSource =
    "root:\n"
    "  type: Plot\n"
    "  title: Revenue\n"
    "  x_label: month\n"
    "  children:\n"
    "    - {type: PlotLine, label: a, ys: [1, 3, 2]}\n";

const edit_target_id kPlot{"root", ""};

// ── Labels and origins ───────────────────────────────────────────────────────

TEST(EditorAttribution, ACommitIsLabelledFromItsFieldAndAttributedToTheUser) {
  editor_model m(kSource);
  m.select(kPlot);
  const commit_result r = m.commit("title", "Sales");
  ASSERT_TRUE(r.ok) << r.error;
  EXPECT_EQ(r.label, "Set title");
  EXPECT_EQ(r.origin, "user");
  EXPECT_EQ(m.undo_label(), "Set title");
  EXPECT_EQ(m.last_label(), "Set title");
}

TEST(EditorAttribution, OriginAndLabelAreRecordedOnTheUndoEntry) {
  editor_model m(kSource);
  m.select(kPlot);
  m.set_origin("agent:claude");
  m.set_label("Rename the plot");
  const commit_result r = m.commit("title", "Sales");
  ASSERT_TRUE(r.ok) << r.error;
  EXPECT_EQ(r.origin, "agent:claude");
  EXPECT_EQ(m.undo_label(), "Rename the plot");
  m.set_origin("");
  m.set_label("");
  EXPECT_EQ(m.origin(), "user");
  ASSERT_TRUE(m.undo());
  EXPECT_EQ(m.redo_label(), "Rename the plot");
  EXPECT_EQ(m.last_label(), "Undo Rename the plot");
}

TEST(EditorAttribution, ClearAndStructureHaveTheirOwnLabels) {
  editor_model m(kSource);
  m.select(kPlot);
  EXPECT_EQ(m.clear("x_label").label, "Clear x_label");
  EXPECT_EQ(m.add_element("root", 1, "PlotScatter").label, "Add element");
}

// ── Groups ───────────────────────────────────────────────────────────────────

TEST(EditorAttribution, ChangesInAGroupAreOneUndoAndOneRedoStep) {
  editor_model m(kSource);
  const std::string before = m.source();
  m.select(kPlot);
  m.begin_group("Agent turn", "agent:gemini");
  m.set_origin("agent:gemini");
  ASSERT_TRUE(m.commit("title", "One").ok);
  ASSERT_TRUE(m.commit("x_label", "When").ok);
  ASSERT_TRUE(m.add_element("root", 1, "PlotScatter").ok);
  m.end_group();
  const std::string after = m.source();
  EXPECT_EQ(m.undo_label(), "Agent turn");
  ASSERT_TRUE(m.undo());
  EXPECT_EQ(m.source(), before);
  EXPECT_FALSE(m.can_undo());
  ASSERT_TRUE(m.redo());
  EXPECT_EQ(m.source(), after);
}

TEST(EditorAttribution, AnEmptyGroupAddsNothing) {
  editor_model m(kSource);
  m.begin_group("Nothing", "agent:x");
  m.end_group();
  EXPECT_FALSE(m.can_undo());
}

TEST(EditorAttribution, ARefusedChangeInAGroupLeavesTheOthers) {
  editor_model m(kSource);
  m.select(kPlot);
  m.begin_group("Agent turn", "agent:x");
  m.set_origin("agent:x");
  ASSERT_TRUE(m.commit("title", "One").ok);
  EXPECT_FALSE(m.commit("marker_size", "\"big\"").ok);
  ASSERT_TRUE(m.commit("x_label", "When").ok);
  m.end_group();
  EXPECT_NE(m.source().find("title: One"), std::string::npos);
  EXPECT_NE(m.source().find("x_label: When"), std::string::npos);
  ASSERT_TRUE(m.undo());
  EXPECT_EQ(m.source(), kSource);
}

TEST(EditorAttribution, AChangeByAnotherOriginEndsTheGroup) {
  editor_model m(kSource);
  m.select(kPlot);
  m.begin_group("Agent turn", "agent:mcp-claude-code");
  m.set_origin("agent:mcp-claude-code");
  ASSERT_TRUE(m.commit("title", "One").ok);
  ASSERT_TRUE(m.commit("x_label", "When").ok);
  m.set_origin("user");
  ASSERT_TRUE(m.commit("y_label", "How much").ok);
  EXPECT_FALSE(m.in_group());
  m.set_origin("agent:mcp-claude-code");
  m.begin_group("Agent turn", "agent:mcp-claude-code");
  ASSERT_TRUE(m.commit("title", "Two").ok);
  m.end_group();
  EXPECT_EQ(m.undo_label(), "Agent turn");
  EXPECT_EQ(m.last_origin(), "agent:mcp-claude-code");
  ASSERT_TRUE(m.undo()); // the agent's second part only
  EXPECT_NE(m.source().find("title: One"), std::string::npos);
  EXPECT_NE(m.source().find("y_label: How much"), std::string::npos);
  EXPECT_EQ(m.undo_label(), "Set y_label");
  ASSERT_TRUE(m.undo()); // the user's change
  EXPECT_EQ(m.source().find("y_label"), std::string::npos);
  EXPECT_NE(m.source().find("x_label: When"), std::string::npos);
  EXPECT_EQ(m.undo_label(), "Agent turn");
  ASSERT_TRUE(m.undo()); // the agent's first part
  EXPECT_EQ(m.source(), kSource);
}

TEST(EditorAttribution, AChangeWithTheGroupsOriginJoinsIt) {
  editor_model m(kSource);
  m.select(kPlot);
  m.set_origin("agent:x");
  m.begin_group("Agent turn", "agent:x");
  ASSERT_TRUE(m.commit("title", "One").ok);
  ASSERT_TRUE(m.commit("x_label", "When").ok);
  EXPECT_TRUE(m.in_group());
  m.end_group();
  ASSERT_TRUE(m.undo());
  EXPECT_EQ(m.source(), kSource);
}

// ── The editor tools in process ──────────────────────────────────────────────

struct model_host : mcp::editor_host {
  explicit model_host(editor_model& m) : m_(m) {}
  editor_model* model() override {
    return &m_;
  }
  std::string file() const override {
    return "chart.yaml";
  }
  std::optional<std::string> data_tab() const override {
    return std::nullopt;
  }
  void show_tab(std::optional<std::string>) override {}
  std::vector<edit_element> tree() const override {
    return {};
  }
  std::string view_source() const override {
    return m_.source();
  }
  std::string panel_error() const override {
    return {};
  }
  void changed(const commit_result& r, const std::string& tool) override {
    last_origin = r.origin;
    last_tool = tool;
  }
  void select(const edit_target_id&) override {}
  std::vector<uint8_t> view_image(int, int&, int&) override {
    return {};
  }
  editor_model& m_;
  std::string last_origin;
  std::string last_tool;
};

TEST(EditorAttribution, CallEditorToolAttributesItsChange) {
  editor_model m(kSource);
  model_host host(m);
  const mcp::json r = mcp::call_editor_tool(host, "editor_set_fields", {{"path", "root"}, {"fields", {{"title", "Sales"}}}}, "agent:cursor");
  ASSERT_FALSE(r.value("isError", false)) << r.dump();
  EXPECT_EQ(host.last_origin, "agent:cursor");
  EXPECT_EQ(m.undo_label(), "Editor set fields");
  EXPECT_EQ(m.origin(), "user"); // put back after the call
}

TEST(EditorAttribution, CallEditorToolRefusesAStaleVersion) {
  editor_model m(kSource);
  model_host host(m);
  const mcp::json r = mcp::call_editor_tool(
      host, "editor_set_fields", {{"path", "root"}, {"fields", {{"title", "X"}}}, {"expected_version", 99}});
  EXPECT_TRUE(r.value("isError", false));
  EXPECT_EQ(m.source(), kSource);
}

TEST(EditorAttribution, CallEditorToolAddsAndUndoes) {
  editor_model m(kSource);
  model_host host(m);
  mcp::json r = mcp::call_editor_tool(host, "editor_add_element", {{"parent", "root"}, {"type", "PlotScatter"}});
  ASSERT_FALSE(r.value("isError", false)) << r.dump();
  EXPECT_NE(m.source().find("PlotScatter"), std::string::npos);
  r = mcp::call_editor_tool(host, "editor_undo", mcp::json::object());
  ASSERT_FALSE(r.value("isError", false)) << r.dump();
  EXPECT_EQ(m.source(), kSource);
  r = mcp::call_editor_tool(host, "editor_state", mcp::json::object());
  EXPECT_FALSE(r.value("isError", false)) << r.dump();
}

TEST(EditorAttribution, CallEditorToolThrowsForAnUnknownTool) {
  editor_model m(kSource);
  model_host host(m);
  EXPECT_THROW(mcp::call_editor_tool(host, "editor_fly", mcp::json::object()), mcp::tool_error);
}

} // namespace
} // namespace bdg::nymph
