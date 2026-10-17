// MIT License © 2026 Binary Dice Games
/// @file test_layout.cpp
/// @brief Plan step 11: distribute(), the axis arithmetic shared by both
///        layout directions. Pure, no ImGui.
#include <gtest/gtest.h>
#include <vector>
#include "layout/layout.hpp"

namespace bdg::nymph {
namespace {

std::vector<float> sizes(std::vector<axis_item> items, float available, float spacing) {
  return distribute(items, available, spacing).sizes;
}

TEST(layout, FixedNaturalAndStretchShareTheAxis) {
  // 100 fixed + 40 natural + 2 x 10 spacing = 160; the stretch child gets 140.
  const axis_plan plan = distribute(std::vector<axis_item>{{100, 0}, {0, 40}, {-1, 0}}, 300, 10);
  EXPECT_EQ(plan.sizes, (std::vector<float>{100, 40, 140}));
  EXPECT_EQ(plan.needed, 160);
}

TEST(layout, StretchWeightsSplitInProportion) {
  EXPECT_EQ(sizes({{-1, 0}, {-3, 0}}, 400, 0), (std::vector<float>{100, 300}));
  EXPECT_EQ(sizes({{-1, 0}, {-1, 0}}, 401, 1), (std::vector<float>{200, 200}));
}

TEST(layout, NaturalSizeIsUsedOnlyForAZeroHint) {
  // A fixed or stretch child's natural size is ignored.
  EXPECT_EQ(sizes({{50, 999}, {-1, 999}, {0, 30}}, 200, 0), (std::vector<float>{50, 120, 30}));
}

TEST(layout, ZeroWeightSpringCollapses) {
  // Spring weight 0 has hint 0 and natural size 0.
  EXPECT_EQ(sizes({{0, 0}, {80, 0}, {-1, 0}}, 200, 0), (std::vector<float>{0, 80, 120}));
}

TEST(layout, OverflowReportsWhatIsNeeded) {
  const axis_plan plan = distribute(std::vector<axis_item>{{300, 0}, {0, 300}, {-1, 0}}, 484, 10);
  EXPECT_EQ(plan.needed, 620);
  EXPECT_EQ(plan.sizes[2], 0); // nothing left to stretch
}

TEST(layout, EmptyAndSingle) {
  EXPECT_TRUE(sizes({}, 100, 10).empty());
  EXPECT_EQ(distribute(std::vector<axis_item>{{0, 20}}, 100, 10).needed, 20); // no spacing for one child
}

} // namespace
} // namespace bdg::nymph
