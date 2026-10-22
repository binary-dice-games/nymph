// MIT License © 2026 Binary Dice Games
/// @file test_tree_menu.cpp
/// @brief spec/14_editor_workspace plan step 4: what the tree's menu offers,
///        the New element dialog's draft, deleting referenced elements, and
///        the dialogs drawn headless.
#include <gtest/gtest.h>
#include <imgui.h>
#include <algorithm>
#include <string>
#include <vector>
#include "edit/editor_model.hpp"
#include "edit/inspector.hpp"
#include "edit/tree_menu.hpp"
#include "elements/register_builtin.hpp"
#include "render/draw_context.hpp"
#include "render/frame_setup.hpp"
#include "schema/registry.hpp"

namespace bdg::nymph {
namespace {

const char* const kChart =
    "root:\n"
    "  type: Flowchart\n"
    "  children:\n"
    "    - {type: FlowNode, id: cart, text: Cart}\n"
    "    - {type: FlowNode, id: buy, text: Purchases}\n"
    "    - {type: FlowLink, from: cart, to: buy}\n";

TEST(tree_menu, AddChildOffersExactlyTheTypesTheParentTakes) {
  const element_class* flowchart = builtin_registry().find("Flowchart");
  ASSERT_NE(flowchart, nullptr);
  std::vector<const element_class*> offered;
  for (const type_group& g : child_types(*flowchart)) {
    EXPECT_FALSE(g.types.empty());
    offered.insert(offered.end(), g.types.begin(), g.types.end());
  }
  EXPECT_EQ(offered, builtin_registry().classes_taken_by(*flowchart));
  const auto has = [&](const char* name) {
    return std::any_of(offered.begin(), offered.end(), [&](const element_class* c) { return c->name() == name; });
  };
  EXPECT_TRUE(has("FlowNode"));
  EXPECT_TRUE(has("FlowLink"));
  EXPECT_FALSE(has("PlotLine"));
}

TEST(tree_menu, ChartTypesAreTheFigureSlot) {
  const std::vector<const element_class*> charts = chart_types();
  EXPECT_TRUE(std::all_of(charts.begin(), charts.end(), [](const element_class* c) { return c->fills_slot(kFigureSlot); }));
  EXPECT_TRUE(std::any_of(charts.begin(), charts.end(), [](const element_class* c) { return c->name() == "Plot"; }));
}

TEST(tree_menu, AddingALinkFromTheSelectedNodeInsertsAtOnce) {
  editor_model m(kChart);
  m.select(edit_target_id{"root/children/1", ""});
  tree_menu menu;
  inspector_events events;
  menu.add(m, "root", kAppend, "FlowLink", events);
  EXPECT_FALSE(menu.draft()); // bound as started: no dialog
  ASSERT_TRUE(events.source);
  EXPECT_NE(m.source().find("{type: FlowLink, from: buy, to: cart}"), std::string::npos) << m.source();
  ASSERT_TRUE(events.select);
  EXPECT_EQ(events.select->element, "root/children/3");
}

TEST(tree_menu, ALinkWithNothingToNameOpensTheDialogUntilItsEndsAreGiven) {
  editor_model m("root:\n  type: Flowchart\n  children:\n    - {type: Note, text: empty}\n");
  tree_menu menu;
  inspector_events events;
  menu.add(m, "root", 0, "FlowLink", events);
  ASSERT_TRUE(menu.draft());
  EXPECT_FALSE(events.source);
  element_draft& d = *menu.draft();
  EXPECT_EQ(d.type, "FlowLink");
  EXPECT_FALSE(d.error.empty());
  EXPECT_EQ(d.fields[0].first, "from"); // the starter's fields first
  EXPECT_EQ(d.fields[1].first, "to");
  EXPECT_EQ(d.starter_count, 2u);
  EXPECT_FALSE(menu.insert_draft(m, events)); // still invalid: refused, the dialog stays

  // Give it nodes to name (another edit), then the ends.
  ASSERT_TRUE(m.add_element("root", kAppend, "FlowNode").ok);
  ASSERT_TRUE(m.add_element("root", kAppend, "FlowNode").ok);
  d.fields[0].second = "node1";
  d.fields[1].second = "node2";
  EXPECT_TRUE(d.check(m)) << d.error;
  EXPECT_TRUE(menu.insert_draft(m, events));
  EXPECT_FALSE(menu.draft());
  EXPECT_NE(m.source().find("{type: FlowLink, from: node1, to: node2}"), std::string::npos) << m.source();
}

TEST(tree_menu, DeletingAReferencedNodeAsksAndCanTakeTheReferencesAlong) {
  editor_model m(kChart);
  tree_menu menu;
  inspector_events events;
  menu.remove(m, "root/children/1", events);
  ASSERT_TRUE(menu.pending_delete());
  EXPECT_EQ(menu.pending_delete()->second, std::vector<std::string>{"root/children/2"});
  EXPECT_FALSE(events.source); // nothing yet
  menu.confirm_delete(m, true, events);
  EXPECT_FALSE(menu.pending_delete());
  ASSERT_TRUE(events.source);
  EXPECT_EQ(m.bound().root.children.size(), 1u);
}

TEST(tree_menu, DeletingAnUnreferencedElementNeedsNoQuestion) {
  editor_model m(kChart);
  tree_menu menu;
  inspector_events events;
  menu.remove(m, "root/children/2", events);
  EXPECT_FALSE(menu.pending_delete());
  ASSERT_TRUE(events.source);
  EXPECT_EQ(m.bound().root.children.size(), 2u);
}

TEST(tree_menu, DuplicateAndMoveReportTheirChange) {
  editor_model m(kChart);
  tree_menu menu;
  inspector_events events;
  menu.duplicate(m, "root/children/0", events);
  ASSERT_TRUE(events.select);
  EXPECT_EQ(events.select->element, "root/children/1");
  inspector_events moved;
  menu.move(m, "root/children/1", -1, moved);
  ASSERT_TRUE(moved.source);
  EXPECT_FALSE(moved.paths.empty());
  inspector_events none;
  menu.move(m, "root/children/0", -1, none); // already first
  EXPECT_FALSE(none.source);
}

/// An ImGui context that runs frames without a rendering backend.
class headless_imgui {
 public:
  headless_imgui() {
    const context_switch on(contexts_);
    configure(*find_theme("light"), "test", ImGuiBackendFlags_RendererHasTextures);
    load_fonts(dc_);
  }
  template <typename Fn>
  void frame(Fn f) {
    const context_switch on(contexts_);
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(500, 700);
    io.DeltaTime = 1.0f / 60.0f;
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(500, 700));
    ImGui::Begin("panel");
    f();
    ImGui::End();
    ImGui::Render();
  }

 private:
  imgui_contexts contexts_;
  image_options options_;
  draw_context dc_{options_};
};

TEST(tree_menu, TheDialogsDrawHeadless) {
  editor_model m("root:\n  type: Flowchart\n  children:\n    - {type: FlowNode, id: a}\n    - {type: FlowLink, from: a, to: a}\n");
  inspector panel;
  const std::vector<edit_element> tree{{"image", "Image", "", 0}, {"root", "Flowchart", "", 0},
                                       {"root/children/0", "FlowNode", "a", 1}, {"root/children/1", "FlowLink", "", 1}};
  inspector_events events;
  panel.menu().remove(m, "root/children/0", events); // referenced: the Delete dialog
  headless_imgui ui;
  bool delete_open = false;
  for (int i = 0; i < 3; ++i) {
    ui.frame([&] {
      (void)panel.draw(m, tree, "");
      delete_open = delete_open || ImGui::IsPopupOpen("Delete element");
    });
  }
  EXPECT_TRUE(delete_open);
  bool new_open = false;
  editor_model empty("root:\n  type: Flowchart\n  children:\n    - {type: Note, text: n}\n");
  panel.menu().add(empty, "root", 0, "FlowLink", events);
  for (int i = 0; i < 3; ++i) {
    ui.frame([&] {
      (void)panel.draw(empty, tree, "");
      new_open = new_open || ImGui::IsPopupOpen("New element");
    });
  }
  EXPECT_TRUE(new_open);
}

TEST(tree_menu, PlanDropFollowsThePointerAndWhatTheParentTakes) {
  editor_model m(kChart);
  const std::string link = "root/children/2";
  const edit_element node0{"root/children/0", "FlowNode", "", 1};
  const edit_element root{"root", "Flowchart", "", 0};
  // Top of the first node: before it, in the chart.
  const auto top = plan_drop(m, link, node0, 0.1f, false);
  ASSERT_TRUE(top);
  EXPECT_EQ(top->parent, "root");
  EXPECT_EQ(top->index, 0u);
  // Bottom of the second node: after it, which is where the link already is.
  EXPECT_FALSE(plan_drop(m, link, edit_element{"root/children/1", "FlowNode", "", 1}, 0.9f, false));
  // A node takes no children; the root takes the link as its last child.
  const auto in_root = plan_drop(m, "root/children/0", root, 0.5f, true);
  ASSERT_TRUE(in_root);
  EXPECT_EQ(in_root->parent, "root");
  EXPECT_EQ(in_root->index, 3u);
  EXPECT_FALSE(plan_drop(m, link, node0, 0.5f, false) && plan_drop(m, link, node0, 0.5f, false)->parent == node0.path);
  // The root cannot be dragged.
  EXPECT_FALSE(plan_drop(m, "root", node0, 0.5f, false));
}

TEST(tree_menu, DropMovesTheElementAndKeepsItSelected) {
  editor_model m(kChart);
  m.select(edit_target_id{"root/children/2", ""});
  tree_menu menu;
  inspector_events events;
  menu.drop(m, "root/children/2", drop_place{"root", 0}, events);
  EXPECT_TRUE(events.error.empty()) << events.error;
  EXPECT_EQ(m.selection()->element, "root/children/0");
  EXPECT_LT(m.source().find("FlowLink"), m.source().find("id: cart"));
}

} // namespace
} // namespace bdg::nymph
