// MIT License © 2026 Binary Dice Games
/// @file test_editor_model.cpp
/// @brief spec/08_view_editor plan step 9: selection, the fields of the
///        selection, validated changes, undo and redo.
#include <gtest/gtest.h>
#include <string>
#include "core/error.hpp"
#include "edit/editor_model.hpp"
#include "edit/field_value.hpp"
#include "elements/register_builtin.hpp"

namespace bdg::nymph {
namespace {

const char* const kSource =
    "# a chart\n"
    "image: {width: 400, height: 300}\n"
    "root:\n"
    "  type: VerticalLayout\n"
    "  children:\n"
    "    - {type: Label, text: Report}\n"
    "    - type: Plot\n"
    "      title: Revenue\n"
    "      x_label: month\n"
    "      children:\n"
    "        - {type: PlotLine, label: a, ys: [1, 3, 2]}\n";

const edit_target_id kPlot{"root/children/1", ""};
const edit_target_id kLabel{"root/children/0", "text"};

const editable_field* find(const std::vector<editable_field>& fields, const std::string& name) {
  for (const editable_field& f : fields) {
    if (f.desc->name == name) {
      return &f;
    }
  }
  return nullptr;
}

TEST(editor_model, ABadSourceThrowsAtConstruction) {
  EXPECT_THROW(editor_model("root: {type: Nope}\n"), error);
}

TEST(editor_model, NothingIsSelectedAtFirstAndAPathOutsideTheSourceSelectsNothing) {
  editor_model m(kSource);
  EXPECT_FALSE(m.selection());
  EXPECT_TRUE(m.fields().empty());
  m.select(edit_target_id{"root/children/9", ""});
  EXPECT_FALSE(m.selection());
  m.select(kPlot);
  ASSERT_TRUE(m.selection());
  EXPECT_EQ(m.selected_type(), "Plot");
}

TEST(editor_model, FieldsAreTheSchemaWithTheCurrentValues) {
  editor_model m(kSource);
  m.select(kPlot);
  const std::vector<editable_field> fields = m.fields();
  const editable_field* title = find(fields, "title");
  ASSERT_NE(title, nullptr);
  EXPECT_TRUE(title->written);
  EXPECT_EQ(std::get<std::string>(title->current), "Revenue");
  EXPECT_EQ(title->desc->type, field_type::text);
  const editable_field* y_label = find(fields, "y_label");
  ASSERT_NE(y_label, nullptr);
  EXPECT_FALSE(y_label->written);
  EXPECT_EQ(std::get<std::string>(y_label->current), "");
  // The schema's order and its text style fields, with what they cover.
  ASSERT_NE(find(fields, "font_size"), nullptr);
  EXPECT_EQ(find(fields, "font_size")->scope_note, "applies to all text of this Plot");
  EXPECT_EQ(find(fields, "title")->scope_note, "");
  m.select(kLabel);
  EXPECT_EQ(find(m.fields(), "text_color")->scope_note, "");
}

TEST(editor_model, ACommitPatchesTheSourceAndKeepsEverythingElse) {
  editor_model m(kSource);
  m.select(kPlot);
  const commit_result r = m.commit("title", encode_text("Sales: 2026"));
  ASSERT_TRUE(r.ok) << r.error;
  EXPECT_EQ(r.source, m.source());
  EXPECT_NE(r.source.find("title: \"Sales: 2026\""), std::string::npos);
  EXPECT_NE(r.source.find("# a chart\n"), std::string::npos);
  EXPECT_TRUE(m.modified());
  EXPECT_EQ(std::get<std::string>(find(m.fields(), "title")->current), "Sales: 2026");
}

TEST(editor_model, ACommitCanAddAFieldAndClearTakesItAwayAgain) {
  editor_model m(kSource);
  m.select(kPlot);
  ASSERT_TRUE(m.commit("text_color", *encode_color("#2471a3")).ok);
  EXPECT_NE(m.source().find("text_color: \"#2471a3\""), std::string::npos);
  EXPECT_TRUE(find(m.fields(), "text_color")->written);
  ASSERT_TRUE(m.clear("text_color").ok);
  EXPECT_EQ(m.source().find("text_color"), std::string::npos);
  EXPECT_FALSE(find(m.fields(), "text_color")->written);
}

TEST(editor_model, AnInvalidChangeIsRefusedAndChangesNothing) {
  editor_model m(kSource);
  m.select(kPlot);
  const std::string before = m.source();
  const commit_result r = m.commit("font_size", encode_number(500));
  EXPECT_FALSE(r.ok);
  EXPECT_NE(r.error.find("font_size"), std::string::npos);
  EXPECT_GT(r.line, 0);
  EXPECT_EQ(m.source(), before);
  EXPECT_FALSE(m.can_undo());
  EXPECT_FALSE(m.modified());
  EXPECT_FALSE(m.commit("no_such_field", "1").ok);
  EXPECT_FALSE(m.commit("title", "a\nb").ok);
  EXPECT_EQ(m.source(), before);
}

TEST(editor_model, WithoutASelectionNothingCanBeCommitted) {
  editor_model m(kSource);
  EXPECT_FALSE(m.commit("title", "x").ok);
  EXPECT_FALSE(m.clear("title").ok);
}

TEST(editor_model, UndoAndRedoRestoreTheSource) {
  editor_model m(kSource);
  m.select(kPlot);
  ASSERT_TRUE(m.commit("title", "One").ok);
  ASSERT_TRUE(m.commit("x_label", "t").ok);
  const std::string two = m.source();
  EXPECT_TRUE(m.can_undo());
  EXPECT_FALSE(m.can_redo());
  ASSERT_TRUE(m.undo());
  EXPECT_EQ(m.source().find("x_label: t"), std::string::npos);
  EXPECT_NE(m.source().find("title: One"), std::string::npos);
  ASSERT_TRUE(m.undo());
  EXPECT_EQ(m.source(), kSource);
  EXPECT_FALSE(m.can_undo());
  EXPECT_FALSE(m.undo());
  ASSERT_TRUE(m.redo());
  ASSERT_TRUE(m.redo());
  EXPECT_EQ(m.source(), two);
  EXPECT_FALSE(m.redo());
  // A new change drops what could be redone.
  ASSERT_TRUE(m.undo());
  ASSERT_TRUE(m.commit("title", "Other").ok);
  EXPECT_FALSE(m.can_redo());
}

TEST(editor_model, ChangesOfOneGestureAreOneUndoStep) {
  editor_model m(kSource);
  m.select(kPlot);
  ASSERT_TRUE(m.commit("font_size", "10").ok);
  for (const char* size : {"11", "12", "13", "14"}) {
    ASSERT_TRUE(m.commit("font_size", size, /*merge=*/true).ok);
  }
  EXPECT_NE(m.source().find("font_size: 14"), std::string::npos);
  ASSERT_TRUE(m.undo());
  EXPECT_EQ(m.source(), kSource);
  EXPECT_FALSE(m.can_undo());
  // Another field in between starts a new step.
  ASSERT_TRUE(m.commit("font_size", "10").ok);
  ASSERT_TRUE(m.commit("title", "x").ok);
  ASSERT_TRUE(m.commit("font_size", "12", /*merge=*/true).ok);
  ASSERT_TRUE(m.undo());
  EXPECT_NE(m.source().find("font_size: 10"), std::string::npos);
}

TEST(editor_model, UndoIsCapped) {
  editor_model m(kSource);
  m.select(kPlot);
  for (size_t i = 0; i < editor_model::kMaxUndo + 20; ++i) {
    ASSERT_TRUE(m.commit("title", "T" + std::to_string(i)).ok);
  }
  size_t steps = 0;
  while (m.undo()) {
    ++steps;
  }
  EXPECT_EQ(steps, editor_model::kMaxUndo);
}

TEST(editor_model, ATextBoundToTheDataIsReadOnlyUntilReplaced) {
  editor_model m(
      "root: {type: VerticalLayout, children: [{type: Label, text: $names}]}\n"
      "data: |\n"
      "  names\n"
      "  Ada\n");
  m.select(edit_target_id{"root/children/0", "text"});
  const std::vector<editable_field> fields = m.fields();
  const editable_field* text = find(fields, "text");
  ASSERT_NE(text, nullptr);
  EXPECT_EQ(text->bound_to, "$names");
  EXPECT_EQ(std::get<std::string>(text->current), "Ada");
  const commit_result refused = m.commit("text", "Grace");
  EXPECT_FALSE(refused.ok);
  EXPECT_NE(refused.error.find("data"), std::string::npos);
  EXPECT_FALSE(m.modified());
  const commit_result replaced = m.commit("text", "Grace", false, /*replace_binding=*/true);
  ASSERT_TRUE(replaced.ok) << replaced.error;
  EXPECT_EQ(find(m.fields(), "text")->bound_to, "");
}

TEST(editor_model, TheSelectionSurvivesTextEditsAndGoesWhenItsElementDoes) {
  editor_model m(kSource);
  m.select(kLabel);
  ASSERT_TRUE(m.commit("text", "New").ok);
  ASSERT_TRUE(m.selection());
  EXPECT_EQ(m.selection()->element, "root/children/0");
  // Undoing it to the start keeps the selection too; a source without the
  // element drops it.
  ASSERT_TRUE(m.undo());
  EXPECT_TRUE(m.selection());
  m.reset("root: {type: Plot}\n");
  EXPECT_FALSE(m.selection());
}

// ── The image options ────────────────────────────────────────────────────────

TEST(editor_model, TheImageOptionsAreSelectedLikeAnElement) {
  editor_model m(kSource);
  m.select(edit_target_id{"image", ""});
  ASSERT_TRUE(m.selection());
  EXPECT_EQ(m.selected_type(), "Image");
  const std::vector<editable_field> fields = m.fields();
  ASSERT_EQ(fields.size(), 8u);
  const editable_field* width = find(fields, "width");
  ASSERT_NE(width, nullptr);
  EXPECT_TRUE(width->written);
  EXPECT_EQ(std::get<int64_t>(width->current), 400);
  EXPECT_FALSE(find(fields, "theme")->written);
  EXPECT_EQ(std::get<int64_t>(find(fields, "theme")->current), 0); // light
  EXPECT_EQ(find(fields, "theme")->desc->type, field_type::enumeration);
  EXPECT_EQ(std::get<double>(find(fields, "font_size")->current), 16.0);
}

TEST(editor_model, ImageOptionsAreChangedAddedAndRemoved) {
  editor_model m(kSource);
  m.select(edit_target_id{"image", ""});
  ASSERT_TRUE(m.commit("width", encode_int(640)).ok);
  EXPECT_NE(m.source().find("width: 640"), std::string::npos);
  EXPECT_EQ(m.image().width, 640);
  // A new option joins the section; the theme is written by name.
  const field_desc* theme = m.find_field("theme");
  ASSERT_NE(theme, nullptr);
  ASSERT_TRUE(m.commit("theme", encode_enum(*theme, 1)).ok);
  EXPECT_NE(m.source().find("theme: dark"), std::string::npos);
  EXPECT_EQ(m.image().theme, "dark");
  // Out of range, or too big for the scale, is refused.
  EXPECT_FALSE(m.commit("width", encode_int(5000)).ok);
  ASSERT_TRUE(m.commit("width", encode_int(4096)).ok);
  EXPECT_FALSE(m.commit("scale", encode_int(3)).ok); // 4096 x 3 is over 8192
  ASSERT_TRUE(m.commit("width", encode_int(640)).ok);
  EXPECT_EQ(m.image().width, 640);
  ASSERT_TRUE(m.clear("theme").ok);
  EXPECT_EQ(m.image().theme, "light");
  ASSERT_TRUE(m.undo());
  EXPECT_EQ(m.image().theme, "dark");
}

TEST(editor_model, TheFirstImageOptionCreatesTheSectionAndTheLastRemovalDropsIt) {
  editor_model m("root: {type: Plot, title: T}\n");
  m.select(edit_target_id{"image", ""});
  EXPECT_FALSE(find(m.fields(), "width")->written);
  ASSERT_TRUE(m.commit("width", encode_int(500)).ok);
  EXPECT_EQ(m.image().width, 500);
  EXPECT_NE(m.source().find("image:"), std::string::npos);
  ASSERT_TRUE(m.clear("width").ok);
  EXPECT_EQ(m.source().find("image"), std::string::npos) << m.source();
  EXPECT_EQ(m.image().width, 800);
  // Clearing what is not written changes nothing.
  EXPECT_TRUE(m.clear("height").ok);
}

TEST(editor_model, SavingMakesTheModelUnmodifiedAndKeepsTheHistory) {
  editor_model m(kSource);
  m.select(kPlot);
  ASSERT_TRUE(m.commit("title", "Saved").ok);
  EXPECT_TRUE(m.modified());
  m.mark_saved();
  EXPECT_FALSE(m.modified());
  EXPECT_TRUE(m.can_undo());
  ASSERT_TRUE(m.commit("title", "After").ok);
  EXPECT_TRUE(m.modified());
  ASSERT_TRUE(m.undo()); // back to what the file holds
  EXPECT_FALSE(m.modified());
}

TEST(editor_model, ASourceTheFileDoesNotHoldIsModified) {
  editor_model m("root: {type: Plot, title: Edited}\n");
  EXPECT_FALSE(m.modified());
  m.set_original("root: {type: Plot, title: OnDisk}\n");
  EXPECT_TRUE(m.modified());
}

TEST(editor_model, ResetReplacesTheSourceAndForgetsTheHistory) {
  editor_model m(kSource);
  m.select(kPlot);
  ASSERT_TRUE(m.commit("title", "x").ok);
  m.reset("root: {type: Plot, title: Fresh}\n");
  EXPECT_FALSE(m.can_undo());
  EXPECT_FALSE(m.modified());
  EXPECT_FALSE(m.selection()); // root/children/1 is gone
  EXPECT_THROW(m.reset("root: {type: Nope}\n"), error);
  EXPECT_EQ(m.source(), "root: {type: Plot, title: Fresh}\n"); // unchanged by the failed reset
}


// ── Structure (spec/14_editor_workspace plan step 3) ─────────────────────────

const char* const kChart =
    "root:\n"
    "  type: Flowchart\n"
    "  children:\n"
    "    - {type: FlowNode, id: cart, text: Cart}\n"
    "    - {type: FlowNode, id: buy, text: Purchases}\n"
    "    - {type: FlowLink, from: cart, to: buy}\n"
    "    - {type: Note, text: n, attach: [buy]}\n"
    "view:\n"
    "  root: {zoom: 2}\n";

TEST(editor_model, AddElementInsertsTheStarterAndSelectsIt) {
  editor_model m(kChart);
  m.select(edit_target_id{"root/children/1", ""});
  const uint64_t before = m.version();
  const commit_result r = m.add_element("root", kAppend, "FlowLink");
  ASSERT_TRUE(r.ok) << r.error;
  EXPECT_EQ(r.path, "root/children/4");
  EXPECT_NE(m.source().find("    - {type: FlowLink, from: buy, to: cart}\n"), std::string::npos) << m.source();
  ASSERT_TRUE(m.selection());
  EXPECT_EQ(m.selection()->element, "root/children/4");
  EXPECT_GT(m.version(), before);
}

TEST(editor_model, AddElementSetsTheGivenFields) {
  editor_model m(kChart);
  const commit_result r = m.add_element("root", 2, "FlowNode", {{"text", "Pay"}, {"shape", "Diamond"}});
  ASSERT_TRUE(r.ok) << r.error;
  EXPECT_NE(m.source().find("{type: FlowNode, id: node1, text: Pay, shape: Diamond}"), std::string::npos) << m.source();
  EXPECT_EQ(r.paths.apply("root/children/2"), "root/children/3");
}

TEST(editor_model, AddElementRefusesATypeTheParentDoesNotTake) {
  editor_model m(kChart);
  const std::string source = m.source();
  const commit_result r = m.add_element("root", kAppend, "PlotLine");
  EXPECT_FALSE(r.ok);
  EXPECT_NE(r.error.find("does not take"), std::string::npos) << r.error;
  EXPECT_EQ(m.source(), source);
  EXPECT_FALSE(m.can_undo());
}

TEST(editor_model, AddChartWrapsARootThatIsNotALayout) {
  editor_model m(kChart);
  const commit_result r = m.add_chart("Plot", chart_placement::right);
  ASSERT_TRUE(r.ok) << r.error;
  EXPECT_EQ(m.bound().root.cls->name(), "HorizontalLayout");
  EXPECT_EQ(r.path, "root/children/1");
  EXPECT_EQ(r.paths.apply("root/children/2"), "root/children/0/children/2");
  // The view entry follows the old root.
  EXPECT_NE(m.source().find("  root/children/0: {zoom: 2}"), std::string::npos) << m.source();
  // Both share the width: they stretch.
  EXPECT_NE(m.source().find("{type: Plot, width: -1}"), std::string::npos) << m.source();
  EXPECT_NE(m.source().find("      width: -1\n"), std::string::npos) << m.source();
  // A second chart joins the layout.
  ASSERT_TRUE(m.add_chart("Label", chart_placement::below).ok);
  EXPECT_EQ(m.bound().root.children.size(), 3u);
}

TEST(editor_model, RemoveWithReferencesTakesTheLinksAndTheNoteToo) {
  editor_model m(kChart);
  EXPECT_EQ(m.references("root/children/1"), (std::vector<std::string>{"root/children/2", "root/children/3"}));
  // Alone it would leave a link to nothing: refused.
  EXPECT_FALSE(m.remove_element("root/children/1").ok);
  m.select(edit_target_id{"root/children/1", ""});
  const commit_result r = m.remove_element("root/children/1", true);
  ASSERT_TRUE(r.ok) << r.error;
  EXPECT_EQ(m.bound().root.children.size(), 1u);
  ASSERT_TRUE(m.selection());
  EXPECT_EQ(m.selection()->element, "root/children/0"); // the previous sibling
  EXPECT_TRUE(m.undo());
  EXPECT_EQ(m.source(), kChart);
}

TEST(editor_model, TheRootCannotBeRemoved) {
  editor_model m(kChart);
  EXPECT_FALSE(m.remove_element("root").ok);
}

TEST(editor_model, MoveKeepsTheSelectionOnTheMovedElement) {
  editor_model m(kChart);
  m.select(edit_target_id{"root/children/0", "text"});
  const commit_result r = m.move_element("root/children/0", 1);
  ASSERT_TRUE(r.ok) << r.error;
  EXPECT_EQ(m.selection()->element, "root/children/1");
  EXPECT_EQ(m.selection()->field, "text");
  EXPECT_NE(m.source().find("{type: FlowNode, id: buy, text: Purchases}\n    - {type: FlowNode, id: cart"), std::string::npos);
}

TEST(editor_model, DuplicateGivesFreshIds) {
  editor_model m(kChart);
  const commit_result r = m.duplicate_element("root/children/0");
  ASSERT_TRUE(r.ok) << r.error;
  EXPECT_EQ(m.selection()->element, "root/children/1");
  EXPECT_NE(m.source().find("{type: FlowNode, id: cart1, text: Cart}"), std::string::npos) << m.source();
}

TEST(editor_model, CandidateIdsPutTheSelectionFirst) {
  editor_model m(kChart);
  EXPECT_EQ(m.candidate_ids("root"), (std::vector<std::string>{"cart", "buy"}));
  m.select(edit_target_id{"root/children/1", ""});
  EXPECT_EQ(m.candidate_ids("root"), (std::vector<std::string>{"buy", "cart"}));
}

TEST(editor_model, SetSourceReplacesTheWholeSourceWithUndo) {
  editor_model m(kChart);
  EXPECT_FALSE(m.set_source("root: {type: Nope}\n").ok);
  ASSERT_TRUE(m.set_source("root: {type: Label, text: hi}\n").ok);
  EXPECT_TRUE(m.undo());
  EXPECT_EQ(m.source(), kChart);
}

// ── Data (spec/14_editor_workspace plan step 6) ──────────────────────────────

const char* const kData =
    "root:\n"
    "  type: Plot\n"
    "  children:\n"
    "    - {type: PlotLine, label: r, xs: $x, ys: $y}\n"
    "data: |\n"
    "  x, y\n"
    "  1, 5\n"
    "  2, 6\n";

TEST(editor_model, EachDataChangeIsOneUndoStep) {
  editor_model m(kData);
  ASSERT_TRUE(m.set_cell("", 1, 1, "8").ok);
  EXPECT_EQ(m.tables()[0].cells.rows[1][1], "8");
  ASSERT_TRUE(m.insert_rows("", 2).ok);
  EXPECT_EQ(m.tables()[0].cells.rows.size(), 3u);
  ASSERT_TRUE(m.insert_column("", 2, "z").ok);
  ASSERT_TRUE(m.rename_column("", "y", "sales").ok);
  EXPECT_NE(m.source().find("ys: $sales"), std::string::npos) << m.source();
  for (int i = 0; i < 4; ++i) {
    ASSERT_TRUE(m.undo());
  }
  EXPECT_EQ(m.source(), kData);
}

TEST(editor_model, ACellOutsideTheTableOrAnInvalidValueIsRefused) {
  editor_model m(kData);
  EXPECT_FALSE(m.set_cell("", 5, 0, "1").ok);
  const commit_result bad = m.set_cell("", 0, 1, "lots");
  EXPECT_FALSE(bad.ok); // y is used as numbers
  EXPECT_EQ(m.source(), kData);
  EXPECT_FALSE(m.remove_column("", 1).ok); // the line uses it
  EXPECT_FALSE(m.remove_table("").ok);
}

TEST(editor_model, APastedBlockGrowsTheTable) {
  editor_model m(kData);
  const commit_result r = m.paste_cells("", 1, 1, "7\t70\n8\t80\n9\t90\n");
  ASSERT_TRUE(r.ok) << r.error;
  const table_text t = m.tables()[0].cells;
  EXPECT_EQ(t.headers, (std::vector<std::string>{"x", "y", "column3"}));
  ASSERT_EQ(t.rows.size(), 4u);
  EXPECT_EQ(t.rows[3], (std::vector<std::string>{"", "9", "90"}));
  EXPECT_TRUE(m.undo());
  EXPECT_EQ(m.source(), kData);
}

TEST(editor_model, TablesAreAddedRenamedAndRemoved) {
  editor_model m(kData);
  EXPECT_EQ(m.fresh_table_name(), "table1");
  ASSERT_TRUE(m.add_table("costs", editor_model::starter_table(), "sales").ok);
  EXPECT_NE(m.source().find("xs: $sales:x"), std::string::npos) << m.source();
  ASSERT_EQ(m.tables().size(), 2u);
  ASSERT_TRUE(m.rename_table("costs", "spend").ok);
  EXPECT_EQ(m.tables()[1].name, "spend");
  ASSERT_TRUE(m.remove_table("spend").ok);
  EXPECT_FALSE(m.remove_table("sales").ok); // still used
  editor_model empty("root: {type: Label, text: hi}\n");
  ASSERT_TRUE(empty.add_table("", editor_model::starter_table()).ok);
  EXPECT_EQ(empty.tables()[0].cells, editor_model::starter_table());
}

} // namespace
} // namespace bdg::nymph
