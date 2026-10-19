// MIT License © 2026 Binary Dice Games
/// @file test_uml_item_style.cpp
/// @brief spec/08_view_editor plan step 5: every diagram item takes optional
///        `font`, `font_size` and `text_color`; unset, it inherits the
///        diagram's.
#include <gtest/gtest.h>
#include <algorithm>
#include <string>
#include "core/error.hpp"
#include "document/bind.hpp"
#include "elements/register_builtin.hpp"
#include "render/figure_renderer.hpp"

namespace bdg::nymph {
namespace {

std::string flow(const std::string& node_extra, const std::string& link_extra = {}) {
  return "image: {width: 400, height: 240, font_size: 18}\n"
         "root:\n"
         "  type: Flowchart\n"
         "  children:\n"
         "    - {type: FlowNode, id: a, text: Alpha" + node_extra + "}\n"
         "    - {type: FlowNode, id: b, text: Beta}\n"
         "    - {type: FlowLink, from: a, to: b, label: go" + link_extra + "}\n";
}

image render(const std::string& source) {
  return render_figure(bind_source(source, builtin_registry()));
}

/// Pixels that are clearly red.
int red_pixels(const image& img) {
  int count = 0;
  for (int y = 0; y < img.height; ++y) {
    for (int x = 0; x < img.width; ++x) {
      const uint8_t* p = img.at(x, y);
      count += (p[0] > 150 && p[0] - std::max(p[1], p[2]) > 80) ? 1 : 0;
    }
  }
  return count;
}

TEST(uml_item_style, EveryDiagramItemHasTheThreeTextFields) {
  int items = 0;
  for (const element_class* cls : builtin_registry().classes()) {
    if (cls->slot().find("uml") == std::string_view::npos && cls->slot().find("flow") == std::string_view::npos) {
      continue;
    }
    ++items;
    for (const char* field : {"font", "font_size", "text_color"}) {
      EXPECT_GE(cls->field_index(field), 0) << cls->name() << " lacks " << field;
    }
  }
  EXPECT_GT(items, 20);
}

TEST(uml_item_style, UnsetItemStyleDrawsLikeNoStyleAtAll) {
  const image plain = render(flow(""));
  const image same = render(flow(", font_size: 0, text_color: \"\"", ", font: \"\""));
  ASSERT_EQ(plain.width, same.width);
  EXPECT_TRUE(plain.rgba == same.rgba);
}

TEST(uml_item_style, TheDetectorSeesADiagramLevelColour) {
  std::string src = flow("");
  src.replace(src.find("type: Flowchart"), 15, "text_color: \"#ff0000\"\n  type: Flowchart");
  EXPECT_GT(red_pixels(render(src)), 100);
}

TEST(uml_item_style, ANodesTextColourAppliesToThatNodeOnly) {
  EXPECT_EQ(red_pixels(render(flow(""))), 0);
  const int one = red_pixels(render(flow(", text_color: \"#ff0000\"")));
  EXPECT_GT(one, 50);
  // A link's own colour.
  const int link = red_pixels(render(flow("", ", text_color: \"#ff0000\"")));
  EXPECT_GT(link, 10);
  // Both: the pixels are the sum, so the two styles are independent.
  const int both = red_pixels(render(flow(", text_color: \"#ff0000\"", ", text_color: \"#ff0000\"")));
  EXPECT_NEAR(both, one + link, 10);
}

TEST(uml_item_style, ALargerFontSizeMakesTheNodeLargerAndStillLaysOut) {
  const image plain = render(flow(""));
  const image big = render(flow(", font_size: 30"));
  EXPECT_FALSE(plain.rgba == big.rgba);
  // The bigger text is drawn in more dark pixels.
  const auto dark = [](const image& img) {
    int n = 0;
    for (int y = 0; y < img.height; ++y) {
      for (int x = 0; x < img.width; ++x) {
        n += img.at(x, y)[0] < 90 ? 1 : 0;
      }
    }
    return n;
  };
  EXPECT_GT(dark(big), dark(plain));
}

TEST(uml_item_style, ABadItemStyleIsRejectedLikeAnElementOne) {
  EXPECT_THROW(render(flow(", font: NoSuchFont.ttf")), error);
  EXPECT_THROW(render(flow(", font_size: 500")), error);
  EXPECT_THROW(render(flow(", text_color: red")), error);
}

} // namespace
} // namespace bdg::nymph
