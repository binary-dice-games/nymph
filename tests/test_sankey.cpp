// MIT License © 2026 Binary Dice Games
/// @file test_sankey.cpp
/// @brief `SankeyDiagram`, `SankeyNode`, `SankeyFlow` and layout_sankey().
#include <gtest/gtest.h>
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include "core/error.hpp"
#include "document/bind.hpp"
#include "elements/register_builtin.hpp"
#include "layout/sankey_layout.hpp"
#include "nymph.hpp"
#include "render/figure_renderer.hpp"

namespace bdg::nymph {
namespace {

// ── Layout ───────────────────────────────────────────────────────────────────

/// a -> c (30), b -> c (10), c -> d (25), c -> e (15), b -> e (5)
sankey_input sample() {
  sankey_input in;
  in.node_count = 5;
  in.links = {{0, 2, 30}, {1, 2, 10}, {2, 3, 25}, {2, 4, 15}, {1, 4, 5}};
  in.width = 400;
  in.height = 300;
  in.node_width = 10;
  in.node_spacing = 10;
  return in;
}

TEST(sankey_layout, ColumnsAreLongestPathsWithSinksOnTheRight) {
  const sankey_input in = sample();
  EXPECT_EQ(sankey_columns(in.node_count, in.links), (std::vector<int>{0, 0, 1, 2, 2}));
  // A cycle has no columns.
  EXPECT_TRUE(sankey_columns(2, {{0, 1, 1}, {1, 0, 1}}).empty());
}

TEST(sankey_layout, HeightsAreProportionalToValueAndLinksStackInsideNodes) {
  const sankey_input in = sample();
  const sankey_output out = layout_sankey(in);
  ASSERT_EQ(out.nodes.size(), 5u);
  EXPECT_EQ(out.columns, 3);
  EXPECT_DOUBLE_EQ(out.value[2], 40.0);
  EXPECT_DOUBLE_EQ(out.value[1], 15.0);
  const float ky = (out.nodes[2].y1 - out.nodes[2].y0) / 40.0f;
  EXPECT_GT(ky, 0.0f);
  for (size_t i = 0; i < 5; ++i) {
    EXPECT_NEAR(out.nodes[i].y1 - out.nodes[i].y0, out.value[i] * ky, 0.01) << i;
    EXPECT_GE(out.nodes[i].y0, -0.01f) << i;
    EXPECT_LE(out.nodes[i].y1, in.height + 0.01f) << i;
  }
  // The fullest column (a, b: 45 with one gap) fills the height.
  EXPECT_NEAR((out.value[0] + out.value[1]) * ky + in.node_spacing, in.height, 0.5);
  // Columns spread over the width.
  EXPECT_EQ(out.nodes[0].x0, 0.0f);
  EXPECT_EQ(out.nodes[3].x1, in.width);
  // Bands: as wide as their value, leaving and entering inside their nodes.
  for (size_t k = 0; k < in.links.size(); ++k) {
    const sankey_link_in& l = in.links[k];
    const sankey_link_out& o = out.links[k];
    EXPECT_NEAR(o.width, l.value * ky, 0.01);
    EXPECT_EQ(o.x0, out.nodes[l.from].x1);
    EXPECT_EQ(o.x1, out.nodes[l.to].x0);
    EXPECT_GE(o.y0 - o.width / 2, out.nodes[l.from].y0 - 0.01f);
    EXPECT_LE(o.y0 + o.width / 2, out.nodes[l.from].y1 + 0.01f);
    EXPECT_GE(o.y1 - o.width / 2, out.nodes[l.to].y0 - 0.01f);
    EXPECT_LE(o.y1 + o.width / 2, out.nodes[l.to].y1 + 0.01f);
  }
  // Nodes of a column do not overlap.
  const auto gap = [&](size_t a, size_t b) {
    return std::max(out.nodes[a].y0, out.nodes[b].y0) - std::min(out.nodes[a].y1, out.nodes[b].y1);
  };
  EXPECT_GE(gap(0, 1), in.node_spacing - 0.01f);
  EXPECT_GE(gap(3, 4), in.node_spacing - 0.01f);
}

// ── Element ──────────────────────────────────────────────────────────────────

image render(const std::string& source) {
  return render_figure(bind_source(source, builtin_registry()));
}

error source_error(const std::string& source) {
  try {
    check_source(source);
  } catch (const error& e) {
    return e;
  }
  ADD_FAILURE() << "no error for:\n" << source;
  return error(0, 0, "");
}

void expect_error(const std::string& source, int line, const std::string& part) {
  const error e = source_error(source);
  EXPECT_EQ(e.line(), line) << e.what();
  EXPECT_NE(std::string(e.what()).find(part), std::string::npos) << e.what() << "\nexpected: " << part;
}

std::string with(const std::string& src, const std::string& from, const std::string& to) {
  std::string s = src;
  const size_t at = s.find(from);
  EXPECT_NE(at, std::string::npos) << from;
  return s.replace(at, from.size(), to);
}

using rgba = std::array<uint8_t, 4>;

size_t count_color(const image& img, rgba c) {
  size_t n = 0;
  for (int y = 0; y < img.height; ++y) {
    for (int x = 0; x < img.width; ++x) {
      const uint8_t* p = img.at(x, y);
      n += (rgba{p[0], p[1], p[2], p[3]} == c) ? 1 : 0;
    }
  }
  return n;
}

const char* const kEnergy =
    "image: {width: 600, height: 360}\n" // 1
    "root:\n" // 2
    "  type: SankeyDiagram\n" // 3
    "  title: Energy\n" // 4
    "  from: $from\n" // 5
    "  to: $to\n" // 6
    "  values: $twh\n" // 7
    "  children:\n" // 8
    "    - {type: SankeyNode, id: Losses, name: Lost, color: \"#FF0000\"}\n" // 9
    "    - {type: SankeyFlow, from: Heat, to: Homes, value: 20}\n" // 10
    "data: |\n" // 11
    "  from, to, twh\n" // 12
    "  Coal, Power, 50\n" // 13
    "  Gas, Power, 30\n" // 14
    "  Gas, Heat, 25\n" // 15
    "  Power, Homes, 45\n" // 16
    "  Power, Losses, 35\n"; // 17

TEST(sankey, RendersDeterministicallyAndRoundTrips) {
  const std::vector<uint8_t> a = render_png(kEnergy);
  EXPECT_EQ(render_png(kEnergy), a);
  EXPECT_EQ(extract_source(a).value_or(""), kEnergy);
}

TEST(sankey, NodeColourAndOptionsChangeTheImage) {
  const image base = render(kEnergy);
  // The Losses bar is solid red: a 16 px wide bar, tens of pixels tall.
  EXPECT_GT(count_color(base, {255, 0, 0, 255}), 16u * 30u);
  for (const char* option :
       {"  link_color: Target\n",
        "  link_color: Gray\n",
        "  show_values: false\n",
        "  node_width: 30\n",
        "  colormap: Pastel\n",
        "  value_format: \"%.1f TWh\"\n"}) {
    SCOPED_TRACE(option);
    EXPECT_FALSE(
        render(with(kEnergy, "  title: Energy\n", std::string("  title: Energy\n") + option)).rgba == base.rgba);
  }
  // A flow's own colour paints its band.
  const image green = render(with(kEnergy, "to: Homes, value: 20}", "to: Homes, value: 20, color: \"#00FF00\"}"));
  EXPECT_GT(count_color(green, {0, 255, 0, 255}), 500u);
}

TEST(sankey, FlowErrors) {
  expect_error(with(kEnergy, "  to: $to\n", "  to: [Power]\n"), 6, "SankeyDiagram.to has 1 names but from has 5");
  expect_error(with(kEnergy, "Gas, Heat, 25", "Gas, Heat, 0"), 7, "(flow 3 of the lists) has value 0");
  expect_error(with(kEnergy, "Gas, Heat, 25", "Gas, Gas, 25"), 6, "(flow 3 of the lists) goes from 'Gas' to itself");
  expect_error(with(kEnergy, "value: 20}", "value: -1}"), 10, "SankeyFlow.value has value -1");
  // The first flow on the cycle Gas -> Power -> Homes -> Gas is a list flow.
  expect_error(
      with(kEnergy, "from: Heat, to: Homes", "from: Homes, to: Gas"),
      6,
      "(flow 2 of the lists) makes a cycle: 'Power' leads back to 'Gas'");
  expect_error(
      "root:\n  type: SankeyDiagram\n  children:\n"
      "    - {type: SankeyFlow, from: a, to: b, value: 1}\n"
      "    - {type: SankeyFlow, from: b, to: a, value: 1}\n",
      4,
      "SankeyFlow.to makes a cycle: 'b' leads back to 'a'");
  expect_error(with(kEnergy, "id: Losses", "id: Loses"), 9, "node 'Loses' is in no flow");
  expect_error("root: {type: SankeyDiagram}\n", 1, "SankeyDiagram has no flows");
}

TEST(sankey, NaturalHeightGrowsWithTheNodes) {
  std::string flows;
  for (int i = 0; i < 12; ++i) {
    flows += "        - {type: SankeyFlow, from: s" + std::to_string(i) + ", to: t, value: 1}\n";
  }
  const std::string src =
      "image: {width: 500, height: 300}\nroot:\n  type: VerticalLayout\n  children:\n"
      "    - type: SankeyDiagram\n      children:\n" +
      flows;
  const error e = source_error(src);
  EXPECT_NE(std::string(e.what()).find("px of height"), std::string::npos) << e.what();
}

} // namespace
} // namespace bdg::nymph
