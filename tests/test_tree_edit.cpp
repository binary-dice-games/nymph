// MIT License © 2026 Binary Dice Games
/// @file test_tree_edit.cpp
/// @brief spec/14_editor_workspace plan step 2: inserting, removing, moving,
///        duplicating and wrapping elements in the YAML text, the paths that
///        change, the `view` key renames and references by id.
#include <gtest/gtest.h>
#include <fstream>
#include <sstream>
#include <string>
#include "core/error.hpp"
#include "document/bind.hpp"
#include "edit/tree_edit.hpp"
#include "elements/register_builtin.hpp"

namespace bdg::nymph {
namespace {

/// Whether `source` binds; the loader's message otherwise.
::testing::AssertionResult binds(const std::string& source) {
  try {
    (void)bind_source(source, builtin_registry());
    return ::testing::AssertionSuccess();
  } catch (const std::exception& e) {
    return ::testing::AssertionFailure() << e.what() << "\n" << source;
  }
}

const char* const kBlock =
    "root:\n"
    "  type: VerticalLayout\n"
    "  children:\n"
    "    # the first plot\n"
    "    - type: Plot\n"
    "      title: A\n"
    "    - {type: Plot, title: B}\n"
    "data: {x: [1, 2]}\n";

const char* const kFlow =
    "root: {type: HorizontalLayout, children: [{type: Plot, title: A}, {type: Plot, title: B}]}\n";

// ── path_map ─────────────────────────────────────────────────────────────────

TEST(tree_edit, PathMapMovesDescendantsWithTheirElement) {
  path_map m;
  m.add("root/children/1", std::string("root/children/2"));
  m.add("root/children/0", std::nullopt);
  EXPECT_EQ(m.apply("root/children/1/children/3"), "root/children/2/children/3");
  EXPECT_EQ(m.apply("root/children/0/children/0"), std::nullopt);
  EXPECT_EQ(m.apply("root/children/10"), "root/children/10"); // not a descendant of .../1
  EXPECT_EQ(m.apply("root"), "root");
}

TEST(tree_edit, PathMapRulesOfAStageApplyOnce) {
  path_map m; // an insert at 0: 1 -> 2 and 0 -> 1 must not cascade
  m.add("root/children/1", std::string("root/children/2"));
  m.add("root/children/0", std::string("root/children/1"));
  EXPECT_EQ(m.apply("root/children/0"), "root/children/1");
  path_map next;
  next.add("root", std::string("root/children/0"));
  m.then(next);
  EXPECT_EQ(m.apply("root/children/0"), "root/children/0/children/1");
}

// ── Insert ───────────────────────────────────────────────────────────────────

TEST(tree_edit, InsertsIntoABlockListAtEveryPlace) {
  const tree_edit_result first = insert_child(kBlock, "root", 0, "{type: Label, text: hi}");
  EXPECT_EQ(
      first.source,
      "root:\n"
      "  type: VerticalLayout\n"
      "  children:\n"
      "    # the first plot\n"
      "    - {type: Label, text: hi}\n"
      "    - type: Plot\n"
      "      title: A\n"
      "    - {type: Plot, title: B}\n"
      "data: {x: [1, 2]}\n");
  EXPECT_EQ(first.path, "root/children/0");
  EXPECT_EQ(first.paths.apply("root/children/0"), "root/children/1");
  EXPECT_EQ(first.paths.apply("root/children/1"), "root/children/2");

  const tree_edit_result last = insert_child(kBlock, "root", kAppend, "{type: Label, text: hi}");
  EXPECT_EQ(
      last.source,
      "root:\n"
      "  type: VerticalLayout\n"
      "  children:\n"
      "    # the first plot\n"
      "    - type: Plot\n"
      "      title: A\n"
      "    - {type: Plot, title: B}\n"
      "    - {type: Label, text: hi}\n"
      "data: {x: [1, 2]}\n");
  EXPECT_EQ(last.path, "root/children/2");
  EXPECT_EQ(last.paths.apply("root/children/1"), "root/children/1");

  const tree_edit_result middle = insert_child(kBlock, "root", 1, "{type: Label, text: hi}");
  EXPECT_NE(middle.source.find("      title: A\n    - {type: Label, text: hi}\n    - {type: Plot, title: B}"), std::string::npos)
      << middle.source;
  EXPECT_TRUE(binds(first.source));
  EXPECT_TRUE(binds(last.source));
  EXPECT_TRUE(binds(middle.source));
}

TEST(tree_edit, InsertsIntoAFlowList) {
  EXPECT_EQ(
      insert_child(kFlow, "root", 0, "{type: Label}").source,
      "root: {type: HorizontalLayout, children: [{type: Label}, {type: Plot, title: A}, {type: Plot, title: B}]}\n");
  EXPECT_EQ(
      insert_child(kFlow, "root", kAppend, "{type: Label}").source,
      "root: {type: HorizontalLayout, children: [{type: Plot, title: A}, {type: Plot, title: B}, {type: Label}]}\n");
  EXPECT_EQ(
      insert_child("root: {type: VerticalLayout, children: []}\n", "root", 0, "{type: Label}").source,
      "root: {type: VerticalLayout, children: [{type: Label}]}\n");
}

TEST(tree_edit, InsertsIntoAnElementWithoutChildren) {
  const tree_edit_result block = insert_child("root:\n  type: VerticalLayout\n", "root", kAppend, "{type: Label}");
  EXPECT_EQ(block.source, "root:\n  type: VerticalLayout\n  children:\n    - {type: Label}\n");
  EXPECT_EQ(block.path, "root/children/0");
  EXPECT_TRUE(binds(block.source));
  const tree_edit_result flow = insert_child("root: {type: VerticalLayout}\n", "root", 0, "{type: Label}");
  EXPECT_EQ(flow.source, "root: {type: VerticalLayout, children: [{type: Label}]}\n");
}

TEST(tree_edit, InsertsIntoANestedElement) {
  const std::string source =
      "root:\n"
      "  type: VerticalLayout\n"
      "  children:\n"
      "    - type: HorizontalLayout\n"
      "      children:\n"
      "        - {type: Label, text: a}\n";
  const tree_edit_result r = insert_child(source, "root/children/0", kAppend, "{type: Label, text: b}");
  EXPECT_EQ(r.source, source + "        - {type: Label, text: b}\n");
  EXPECT_EQ(r.path, "root/children/0/children/1");
}

TEST(tree_edit, InsertRejectsBadRequests) {
  EXPECT_THROW(insert_child(kBlock, "root/children/9", 0, "{type: Label}"), error);
  EXPECT_THROW(insert_child(kBlock, "root", 5, "{type: Label}"), error);
  EXPECT_THROW(insert_child(kBlock, "root", 0, "{type: Label,\n text: x}"), error);
}

// ── Remove ───────────────────────────────────────────────────────────────────

TEST(tree_edit, RemovesABlockItemWithItsLines) {
  const tree_edit_result r = remove_element(kBlock, "root/children/0");
  EXPECT_EQ(
      r.source,
      "root:\n"
      "  type: VerticalLayout\n"
      "  children:\n"
      "    # the first plot\n"
      "    - {type: Plot, title: B}\n"
      "data: {x: [1, 2]}\n");
  EXPECT_EQ(r.paths.apply("root/children/0"), std::nullopt);
  EXPECT_EQ(r.paths.apply("root/children/1"), "root/children/0");
  EXPECT_TRUE(binds(r.source));
}

TEST(tree_edit, RemovingTheLastChildLeavesAnEmptyList) {
  const std::string one = "root:\n  type: VerticalLayout\n  children:\n    - {type: Label}\n";
  EXPECT_EQ(remove_element(one, "root/children/0").source, "root:\n  type: VerticalLayout\n  children: []\n");
  EXPECT_EQ(
      remove_element("root: {type: VerticalLayout, children: [{type: Label}]}\n", "root/children/0").source,
      "root: {type: VerticalLayout, children: []}\n");
}

TEST(tree_edit, RemovesFlowItems) {
  EXPECT_EQ(
      remove_element(kFlow, "root/children/0").source,
      "root: {type: HorizontalLayout, children: [{type: Plot, title: B}]}\n");
  EXPECT_EQ(
      remove_element(kFlow, "root/children/1").source,
      "root: {type: HorizontalLayout, children: [{type: Plot, title: A}]}\n");
}

TEST(tree_edit, RootCannotBeRemoved) {
  EXPECT_THROW(remove_element(kBlock, "root"), error);
  EXPECT_THROW(remove_element(kBlock, "root/children/7"), error);
}

// ── Move ─────────────────────────────────────────────────────────────────────

TEST(tree_edit, MovesBlockItemsUpAndDown) {
  const tree_edit_result down = move_element(kBlock, "root/children/0", 1);
  EXPECT_EQ(
      down.source,
      "root:\n"
      "  type: VerticalLayout\n"
      "  children:\n"
      "    # the first plot\n"
      "    - {type: Plot, title: B}\n"
      "    - type: Plot\n"
      "      title: A\n"
      "data: {x: [1, 2]}\n");
  EXPECT_EQ(down.path, "root/children/1");
  EXPECT_EQ(down.paths.apply("root/children/0"), "root/children/1");
  EXPECT_EQ(down.paths.apply("root/children/1"), "root/children/0");
  const tree_edit_result up = move_element(down.source, "root/children/1", 0);
  EXPECT_NE(up.source.find("    - type: Plot\n      title: A\n    - {type: Plot, title: B}\n"), std::string::npos);
  EXPECT_TRUE(binds(down.source));
}

TEST(tree_edit, MovesFlowItems) {
  const tree_edit_result r = move_element(kFlow, "root/children/1", 0);
  EXPECT_EQ(r.source, "root: {type: HorizontalLayout, children: [{type: Plot, title: B}, {type: Plot, title: A}]}\n");
  EXPECT_EQ(move_element(kFlow, "root/children/0", 0).source, kFlow); // in place: unchanged
}

// ── Duplicate ────────────────────────────────────────────────────────────────

TEST(tree_edit, DuplicatesWithFreshIdsAndTheCopysOwnReferences) {
  const std::string source =
      "root:\n"
      "  type: Flowchart\n"
      "  children:\n"
      "    - type: FlowGroup\n"
      "      id: g\n"
      "      children:\n"
      "        - {type: FlowNode, id: a1, text: A}\n"
      "        - {type: FlowNode, id: b, text: B}\n"
      "        - {type: FlowLink, from: a1, to: b}\n"
      "    - {type: FlowNode, id: c, text: C}\n"
      "    - {type: FlowLink, from: c, to: a1}\n";
  const tree_edit_result r = duplicate_element(source, "root/children/0", {"g", "a1", "b", "c"});
  EXPECT_EQ(r.path, "root/children/1");
  EXPECT_EQ(r.paths.apply("root/children/1"), "root/children/2");
  EXPECT_NE(r.source.find("      id: g1\n"), std::string::npos) << r.source;
  EXPECT_NE(r.source.find("{type: FlowNode, id: a2, text: A}"), std::string::npos) << r.source;
  EXPECT_NE(r.source.find("{type: FlowNode, id: b1, text: B}"), std::string::npos) << r.source;
  EXPECT_NE(r.source.find("{type: FlowLink, from: a2, to: b1}"), std::string::npos) << r.source;
  EXPECT_NE(r.source.find("{type: FlowLink, from: c, to: a1}"), std::string::npos); // outside the copy: unchanged
  EXPECT_TRUE(binds(r.source));
}

// ── Wrap the root ────────────────────────────────────────────────────────────

TEST(tree_edit, WrapsABlockRoot) {
  const std::string source =
      "image: {width: 400, height: 300}\n"
      "root:\n"
      "  type: Plot\n"
      "  title: Sales\n"
      "  children:\n"
      "    - {type: PlotLine, label: a, ys: [1, 2]}\n"
      "data: {x: [1]}\n";
  const tree_edit_result r = wrap_root(source, "VerticalLayout");
  EXPECT_EQ(
      r.source,
      "image: {width: 400, height: 300}\n"
      "root:\n"
      "  type: VerticalLayout\n"
      "  children:\n"
      "    - type: Plot\n"
      "      title: Sales\n"
      "      children:\n"
      "        - {type: PlotLine, label: a, ys: [1, 2]}\n"
      "data: {x: [1]}\n");
  EXPECT_EQ(r.paths.apply("root"), "root/children/0");
  EXPECT_EQ(r.paths.apply("root/children/0"), "root/children/0/children/0");
  EXPECT_TRUE(binds(r.source));
}

TEST(tree_edit, WrapsAFlowRoot) {
  const tree_edit_result r = wrap_root("root: {type: Plot, title: x}\n", "HorizontalLayout");
  EXPECT_EQ(r.source, "root:\n  type: HorizontalLayout\n  children:\n    - {type: Plot, title: x}\n");
  EXPECT_TRUE(binds(r.source));
}

TEST(tree_edit, WrapsTheExampleFlowchart) {
  std::ifstream in(std::string(NYMPH_SOURCE_DIR) + "/examples/flowchart.yaml", std::ios::binary);
  std::stringstream ss;
  ss << in.rdbuf();
  const tree_edit_result r = wrap_root(ss.str(), "VerticalLayout");
  EXPECT_TRUE(binds(r.source));
  const tree_edit_result added = insert_child(r.source, "root", kAppend, "{type: Label, text: notes}");
  EXPECT_TRUE(binds(added.source));
}

// ── The view key ─────────────────────────────────────────────────────────────

TEST(tree_edit, RenamesAndDropsBlockViewEntries) {
  const std::string source =
      "root: {type: VerticalLayout, children: [{type: Plot}, {type: Plot}, {type: Plot}]}\n"
      "view:\n"
      "  root/children/0: {x: [1, 2]}\n"
      "  root/children/2: {x: [3, 4]}\n";
  const tree_edit_result removed = remove_element(source, "root/children/0");
  EXPECT_EQ(
      rename_view_paths(removed.source, removed.paths),
      "root: {type: VerticalLayout, children: [{type: Plot}, {type: Plot}]}\n"
      "view:\n"
      "  root/children/1: {x: [3, 4]}\n");
}

TEST(tree_edit, RenamesFlowViewEntriesAndDropsAnEmptyView) {
  path_map m;
  m.add("root/children/0", std::string("root/children/1"));
  EXPECT_EQ(
      rename_view_paths("root: {type: Plot}\nview: {root/children/0: {zoom: 2}}\n", m),
      "root: {type: Plot}\nview: {root/children/1: {zoom: 2}}\n");
  path_map gone;
  gone.add("root/children/0", std::nullopt);
  EXPECT_EQ(rename_view_paths("root: {type: Plot}\nview:\n  root/children/0: {zoom: 2}\n", gone), "root: {type: Plot}\n");
  EXPECT_EQ(rename_view_paths("root: {type: Plot}\n", gone), "root: {type: Plot}\n");
}

// ── Ids and references ───────────────────────────────────────────────────────

TEST(tree_edit, FreshIdsSkipUsedOnes) {
  EXPECT_EQ(fresh_id("node", {}), "node1");
  EXPECT_EQ(fresh_id("node", {"node1", "node2"}), "node3");
  EXPECT_EQ(fresh_id("build7", {"build1"}), "build2");
  EXPECT_EQ(fresh_id("", {}), "id1");
}

TEST(tree_edit, PathsSortInDocumentOrder) {
  EXPECT_TRUE(path_before("root/children/2", "root/children/10"));
  EXPECT_TRUE(path_before("root/children/1", "root/children/1/children/0"));
  EXPECT_FALSE(path_before("root/children/3", "root/children/1/children/0"));
}

TEST(tree_edit, FindsReferencesInTheExampleFlowchart) {
  std::ifstream in(std::string(NYMPH_SOURCE_DIR) + "/examples/flowchart.yaml", std::ios::binary);
  std::stringstream ss;
  ss << in.rdbuf();
  const figure f = bind_source(ss.str(), builtin_registry());
  // root/children/1 is "build": two links and the note attached to it.
  const std::vector<std::string> refs = find_references(f.root, "root/children/1");
  EXPECT_EQ(
      refs,
      (std::vector<std::string>{"root/children/8", "root/children/9", "root/children/13", "root/children/16"}));
  // The group "prod": its link, and the links to its nodes from outside; not
  // the links inside it.
  const std::vector<std::string> group = find_references(f.root, "root/children/4");
  EXPECT_EQ(group, (std::vector<std::string>{"root/children/11", "root/children/14"}));
}

// ── reparent_element ─────────────────────────────────────────────────────────

const char* const kNested =
    "root:\n"
    "  type: VerticalLayout\n"
    "  children:\n"
    "    - type: Plot\n"
    "      title: A\n"
    "    - type: HorizontalLayout\n"
    "      children:\n"
    "        - {type: Plot, title: B}\n"
    "    - {type: Plot, title: C}\n";

TEST(tree_edit, ReparentMovesBlockTextIntoAnotherListReIndented) {
  const tree_edit_result r = reparent_element(kNested, "root/children/0", "root/children/1", 0);
  EXPECT_TRUE(binds(r.source));
  EXPECT_EQ(r.path, "root/children/0/children/0");
  EXPECT_NE(r.source.find("      children:\n"
                          "        - type: Plot\n"
                          "          title: A\n"
                          "        - {type: Plot, title: B}\n"),
            std::string::npos)
      << r.source;
  EXPECT_EQ(r.paths.apply("root/children/0"), "root/children/0/children/0");
  EXPECT_EQ(r.paths.apply("root/children/1/children/0"), "root/children/0/children/1"); // B, one place down
  EXPECT_EQ(r.paths.apply("root/children/2"), "root/children/1");
}

TEST(tree_edit, ReparentOutOfANestedListAndIntoAFlowList) {
  const tree_edit_result out = reparent_element(kNested, "root/children/1/children/0", "root", 0);
  EXPECT_TRUE(binds(out.source));
  EXPECT_EQ(out.path, "root/children/0");
  EXPECT_EQ(out.paths.apply("root/children/0"), "root/children/1");
  EXPECT_NE(out.source.find("children: []"), std::string::npos) << out.source;

  const char* const flow_source =
      "root: {type: VerticalLayout, children: [{type: Plot, title: A}, {type: HorizontalLayout}]}\n";
  const tree_edit_result flow = reparent_element(flow_source, "root/children/0", "root/children/1", kAppend);
  EXPECT_TRUE(binds(flow.source));
  EXPECT_EQ(flow.path, "root/children/0/children/0");
}

TEST(tree_edit, ReparentFlattensBlockTextForAnEmptyParent) {
  const char* const source =
      "root:\n"
      "  type: VerticalLayout\n"
      "  children:\n"
      "    - type: Plot\n"
      "      title: A\n"
      "    - type: HorizontalLayout\n";
  const tree_edit_result r = reparent_element(source, "root/children/0", "root/children/1", kAppend);
  EXPECT_TRUE(binds(r.source));
  EXPECT_EQ(r.path, "root/children/0/children/0");
  EXPECT_NE(r.source.find("{type: Plot, title: A}"), std::string::npos) << r.source;
}

TEST(tree_edit, ReparentRefusesTheElementItselfAndItsDescendants) {
  EXPECT_THROW(reparent_element(kNested, "root/children/1", "root/children/1", 0), error);
  EXPECT_THROW(reparent_element(kNested, "root/children/1", "root/children/1/children/0", 0), error);
  EXPECT_THROW(reparent_element(kNested, "root", "root/children/1", 0), error);
}

TEST(tree_edit, ReparentWithinOneParentIsAMove) {
  const tree_edit_result r = reparent_element(kNested, "root/children/0", "root", 1);
  EXPECT_EQ(r.source, move_element(kNested, "root/children/0", 1).source);
}

} // namespace
} // namespace bdg::nymph
