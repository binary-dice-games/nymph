// MIT License © 2026 Binary Dice Games
/// @file test_imuml_tree_layout.cpp
/// @brief spec/13_more_diagrams_and_shapes PLAN step 11: the tree layout,
///        a pure function (no ImGui context).
#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include "imuml_internal.h"

namespace {

/// A tree spec: parents[i] is the parent of node i (-1 for the root).
ImUmlTreeInput tree(const std::vector<int>& parents, ImVec2 size = ImVec2(60, 30)) {
  ImUmlTreeInput in;
  for (const int p : parents) {
    ImUmlTreeNodeIn n;
    n.Size = size;
    n.Parent = p;
    in.Nodes.push_back(n);
  }
  return in;
}

ImUmlTreeOutput layout(const ImUmlTreeInput& in) {
  ImUmlTreeOutput out;
  ImUml::LayoutTree(in, &out);
  return out;
}

float left(const ImUmlTreeInput& in, const ImUmlTreeOutput& out, int i) {
  return out.Pos[i].x - in.Nodes[i].Size.x * 0.5f;
}
float right(const ImUmlTreeInput& in, const ImUmlTreeOutput& out, int i) {
  return out.Pos[i].x + in.Nodes[i].Size.x * 0.5f;
}

TEST(imuml_tree_layout, AParentIsCentredOverItsFirstAndLastChild) {
  // 0 -> 1, 2, 3
  const ImUmlTreeInput in = tree({-1, 0, 0, 0});
  const ImUmlTreeOutput out = layout(in);
  EXPECT_NEAR(out.Pos[0].x, (out.Pos[1].x + out.Pos[3].x) * 0.5f, 1.0f);
  EXPECT_LT(out.Pos[0].y, out.Pos[1].y);
  EXPECT_EQ(out.Pos[1].y, out.Pos[2].y);
}

TEST(imuml_tree_layout, SiblingsKeepTheirWrittenOrderAndSpacing) {
  const ImUmlTreeInput in = tree({-1, 0, 0, 0, 0});
  const ImUmlTreeOutput out = layout(in);
  for (int i = 1; i < 4; ++i) {
    EXPECT_LT(out.Pos[i].x, out.Pos[i + 1].x);
    EXPECT_GE(left(in, out, i + 1) - right(in, out, i), in.NodeSpacing - 1.0f);
  }
}

TEST(imuml_tree_layout, SubtreesAreKeptApartOnEveryLevelButPackedByContour) {
  // 0 -> 1, 2; 1 -> 3; 3 -> 4, 5, 6 (wide, deep); 2 -> 7, 8
  const ImUmlTreeInput in = tree({-1, 0, 0, 1, 3, 3, 3, 2, 2});
  const ImUmlTreeOutput out = layout(in);
  EXPECT_GE(left(in, out, 2) - right(in, out, 1), in.NodeSpacing - 1.0f);
  EXPECT_GE(left(in, out, 7) - right(in, out, 3), in.NodeSpacing - 1.0f) << "the level of node 3";
  // Packed by contour: node 7 sits under the wide leaves of the other
  // subtree (a level above them), not beside their bounding box.
  EXPECT_LT(left(in, out, 7), right(in, out, 6));
}

TEST(imuml_tree_layout, AllFourDirectionsAreMirrorsOfTopDown) {
  const std::vector<int> parents = {-1, 0, 0, 1, 1};
  ImUmlTreeInput top = tree(parents, ImVec2(40, 40));
  const ImUmlTreeOutput base = layout(top);
  ImUmlTreeInput up = top, lr = top, rl = top;
  up.Direction = ImUmlDirection_BottomUp;
  lr.Direction = ImUmlDirection_LeftRight;
  rl.Direction = ImUmlDirection_RightLeft;
  const ImUmlTreeOutput o_up = layout(up), o_lr = layout(lr), o_rl = layout(rl);
  for (int i = 0; i < 5; ++i) {
    EXPECT_NEAR(o_up.Pos[i].x, base.Pos[i].x, 1.0f);
    EXPECT_NEAR(o_up.Size.y - o_up.Pos[i].y, base.Pos[i].y, 1.0f);
    EXPECT_NEAR(o_lr.Pos[i].x, base.Pos[i].y, 1.0f); // levels run along x
    EXPECT_NEAR(o_lr.Pos[i].y, base.Pos[i].x, 1.0f);
    EXPECT_NEAR(o_rl.Size.x - o_rl.Pos[i].x, base.Pos[i].y, 1.0f);
  }
}

TEST(imuml_tree_layout, HangingChildrenFormOneIndentedColumn) {
  // 0 (hanging) -> 1, 2, 3, with 3 a child of 2.
  ImUmlTreeInput in = tree({-1, 0, 0, 2});
  in.Nodes[0].Hanging = true;
  const ImUmlTreeOutput out = layout(in);
  EXPECT_LT(out.Pos[0].y, out.Pos[1].y);
  EXPECT_LT(out.Pos[1].y, out.Pos[2].y);
  EXPECT_LT(out.Pos[2].y, out.Pos[3].y);
  // Direct reports share a left edge, indented from the parent's.
  EXPECT_NEAR(left(in, out, 1), left(in, out, 2), 1.0f);
  EXPECT_NEAR(left(in, out, 1) - left(in, out, 0), in.Indent, 1.0f);
  EXPECT_NEAR(left(in, out, 3) - left(in, out, 2), in.Indent, 1.0f);
  EXPECT_EQ(out.HangRoot[1], 0);
  EXPECT_EQ(out.HangRoot[3], 0);
}

TEST(imuml_tree_layout, AStaffCardSitsBesideItsParentAndDoesNotMoveTheReports) {
  // 0 -> 1 (staff), 2, 3
  ImUmlTreeInput with = tree({-1, 0, 0, 0});
  with.Nodes[1].Staff = true;
  const ImUmlTreeOutput a = layout(with);
  ImUmlTreeInput without = tree({-1, 0, 0});
  const ImUmlTreeOutput b = layout(without);
  EXPECT_NEAR(a.Pos[1].y, a.Pos[0].y, 1.0f) << "beside the parent";
  EXPECT_GT(left(with, a, 1), right(with, a, 0));
  // The reports are centred under the parent as they are without the assistant.
  EXPECT_NEAR(a.Pos[0].x - a.Pos[2].x, b.Pos[0].x - b.Pos[1].x, 1.0f);
  EXPECT_NEAR(a.Pos[3].x - a.Pos[2].x, b.Pos[2].x - b.Pos[1].x, 1.0f);
}

TEST(imuml_tree_layout, AMindMapSplitsTheRootsChildrenBetweenTwoSidesInOrder) {
  // 0 -> 1..6, each with two leaves for height.
  std::vector<int> parents = {-1, 0, 0, 0, 0, 0, 0};
  for (int c = 1; c <= 6; ++c) {
    parents.push_back(c);
    parents.push_back(c);
  }
  ImUmlTreeInput in = tree(parents);
  in.Mode = ImUmlTreeMode_MindMap;
  in.Direction = ImUmlDirection_LeftRight;
  const ImUmlTreeOutput out = layout(in);
  int on_right = 0;
  for (int c = 1; c <= 6; ++c) {
    on_right += out.OnRight[c] ? 1 : 0;
    EXPECT_EQ(out.OnRight[c] ? out.Pos[c].x > out.Pos[0].x : out.Pos[c].x < out.Pos[0].x, true) << c;
  }
  EXPECT_EQ(on_right, 3);
  // The first half is on the right, in written order top to bottom.
  EXPECT_TRUE(out.OnRight[1] && out.OnRight[2] && out.OnRight[3]);
  EXPECT_LT(out.Pos[1].y, out.Pos[2].y);
  EXPECT_LT(out.Pos[4].y, out.Pos[5].y);
  // Descendants stay on their branch's side.
  EXPECT_EQ(out.OnRight[7], out.OnRight[1]);
  EXPECT_NE(out.Branch[1], out.Branch[2]);
  EXPECT_EQ(out.Branch[7], out.Branch[1]);
}

TEST(imuml_tree_layout, APinnedSideIsKept) {
  ImUmlTreeInput in = tree({-1, 0, 0, 0});
  in.Mode = ImUmlTreeMode_MindMap;
  in.Direction = ImUmlDirection_LeftRight;
  in.Nodes[1].Side = ImUmlSide_Left;
  in.Nodes[2].Side = ImUmlSide_Left;
  const ImUmlTreeOutput out = layout(in);
  EXPECT_FALSE(out.OnRight[1]);
  EXPECT_FALSE(out.OnRight[2]);
  EXPECT_LT(out.Pos[1].x, out.Pos[0].x);
}

TEST(imuml_tree_layout, ResultIsWholePixelsDeterministicAndInsideItsSize) {
  std::vector<int> parents = {-1};
  for (int i = 1; i < 60; ++i) {
    parents.push_back((i - 1) / 3);
  }
  ImUmlTreeInput in = tree(parents, ImVec2(53, 29));
  const ImUmlTreeOutput a = layout(in), b = layout(in);
  ASSERT_EQ(a.Pos.Size, 60);
  for (int i = 0; i < 60; ++i) {
    EXPECT_EQ(a.Pos[i].x, b.Pos[i].x);
    EXPECT_EQ(a.Pos[i].y, b.Pos[i].y);
    EXPECT_EQ(std::floor(left(in, a, i)), left(in, a, i)) << "node " << i;
    EXPECT_GE(left(in, a, i), 0.0f);
    EXPECT_LE(right(in, a, i), a.Size.x);
  }
}

TEST(imuml_tree_layout, FiveHundredNodesLayOutAndDepthIsRecorded) {
  std::vector<int> parents = {-1};
  for (int i = 1; i < 500; ++i) {
    parents.push_back((i - 1) / 4);
  }
  const ImUmlTreeInput in = tree(parents, ImVec2(40, 20));
  const ImUmlTreeOutput out = layout(in);
  EXPECT_EQ(out.Pos.Size, 500);
  EXPECT_EQ(out.Depth[0], 0);
  EXPECT_EQ(out.Depth[1], 1);
  EXPECT_EQ(out.Depth[499], out.Depth[(499 - 1) / 4] + 1);
}

} // namespace
