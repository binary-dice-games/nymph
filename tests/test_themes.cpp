// MIT License © 2026 Binary Dice Games
/// @file test_themes.cpp
/// @brief Chart themes (docs/themes.md): the built-in catalog, which rule
///        a field takes (type, slot prefix, bare name), the source and
///        enclosing elements winning over the theme, image options from the
///        theme, overrides, and the colours of a render.
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <vector>
#include "core/error.hpp"
#include "document/bind.hpp"
#include "elements/register_builtin.hpp"
#include "render/figure_renderer.hpp"
#include "schema/element_class.hpp"
#include "themes/theme_catalog.hpp"

namespace bdg::nymph {
namespace {

/// Field `name` of `n` as text (colours, fonts).
std::string text_of(const node& n, const char* name) {
  const int i = n.cls->field_index(name);
  EXPECT_GE(i, 0) << n.cls->name() << " has no " << name;
  return i < 0 ? std::string() : n.get<std::string>(static_cast<size_t>(i));
}

double number_of(const node& n, const char* name) {
  const int i = n.cls->field_index(name);
  EXPECT_GE(i, 0) << n.cls->name() << " has no " << name;
  return i < 0 ? 0.0 : n.get<double>(static_cast<size_t>(i));
}

figure bind_with(std::string_view source, const chart_theme& t) {
  bind_options o;
  o.custom_theme = &t;
  return bind_source(source, builtin_registry(), o);
}

constexpr const char* kFlowchart = R"(root:
  type: Flowchart
  children:
    - {type: FlowNode, id: a, text: A}
    - {type: FlowNode, id: b, text: B, fill_color: "#010203"}
    - {type: FlowLink, from: a, to: b}
)";

// ── The catalog ──────────────────────────────────────────────────────────────

TEST(Themes, EveryBuiltInThemeIsValid) {
  const auto themes = chart_themes();
  ASSERT_GE(themes.size(), 8u);
  EXPECT_EQ(themes[0].id, "light");
  EXPECT_EQ(themes[1].id, "dark");
  EXPECT_EQ(themes[2].id, "classic");
  for (const chart_theme& t : themes) {
    SCOPED_TRACE(t.id);
    EXPECT_FALSE(t.name.empty());
    EXPECT_FALSE(t.description.empty());
    EXPECT_FALSE(t.palette.empty());
    EXPECT_EQ(t.background.size(), 7u);
    const std::vector<std::string> problems = check_chart_theme(t, builtin_registry());
    for (const std::string& p : problems) {
      ADD_FAILURE() << p;
    }
  }
  EXPECT_NE(find_chart_theme("ocean"), nullptr);
  EXPECT_EQ(find_chart_theme("neon"), nullptr);
  EXPECT_TRUE(find_chart_theme("midnight")->dark);
  EXPECT_FALSE(find_chart_theme("sunset")->dark);
}

TEST(Themes, EveryBuiltInThemeBindsEveryExampleKind) {
  // A plot, a diagram and a mockup in each theme: the theme's values fit
  // every field they reach.
  const char* sources[] = {
      kFlowchart,
      "data: {x: [1, 2], y: [3, 4]}\nroot: {type: Plot, children: [{type: PlotLine, xs: $x, ys: $y}, "
      "{type: PlotBars, xs: $x, ys: $y}]}\n",
      "root: {type: Screen, children: [{type: Window, title: W, children: [{type: Button, label: OK}]}]}\n"};
  for (const chart_theme& t : chart_themes()) {
    for (const char* s : sources) {
      EXPECT_NO_THROW(bind_source(s, builtin_registry(), bind_options{t.id})) << t.id << "\n" << s;
    }
  }
}

TEST(Themes, MalformedThemesAreRefused) {
  EXPECT_THROW(parse_chart_theme("x", "base: neon\n"), error);
  EXPECT_THROW(parse_chart_theme("x", "colour: 1\n"), error);
  EXPECT_THROW(parse_chart_theme("x", "image: {width: 100}\n"), error);
  EXPECT_THROW(parse_chart_theme("x", "image: {palette: [red]}\n"), error);
  EXPECT_THROW(parse_chart_theme("x", "fields: {':fill_color': \"#000000\"}\n"), error);
  EXPECT_THROW(parse_chart_theme("x", "image: {text_color: \"#000000\"}\nfields: {text_color: \"#111111\"}\n"), error);
  const chart_theme unknown = parse_chart_theme("x", "fields: {Gizmo:fill_color: \"#000000\", Plot:shininess: 3}\n");
  EXPECT_EQ(check_chart_theme(unknown, builtin_registry()).size(), 2u);
  const chart_theme wrong = parse_chart_theme("x", "fields: {FlowNode:fill_color: blue}\n");
  EXPECT_EQ(check_chart_theme(wrong, builtin_registry()).size(), 1u);
}

// ── Which rule a field takes ─────────────────────────────────────────────────

TEST(Themes, TheMostSpecificRuleWins) {
  const chart_theme t = parse_chart_theme("t", R"(fields:
  color: "#000001"
  uml:color: "#000002"
  uml.flow.item:color: "#000003"
  flownode:color: "#000004"
  figure:line_color: "#000005"
)");
  const figure f = bind_with(kFlowchart, t);
  EXPECT_EQ(text_of(f.root, "line_color"), "#000005"); // slot figure
  EXPECT_EQ(text_of(f.root.children[0], "color"), "#000004"); // its type, any letter case
  EXPECT_EQ(text_of(f.root.children[2], "color"), "#000003"); // FlowLink: the longest slot prefix
  EXPECT_EQ(f.image.theme, "t");
}

TEST(Themes, TheSourceAndEnclosingElementsWin) {
  const chart_theme t = parse_chart_theme("t", R"(fields:
  fill_color: "#0000aa"
  uml:text_color: "#0000bb"
)");
  // The node's own value.
  const figure f = bind_with(kFlowchart, t);
  EXPECT_EQ(text_of(f.root.children[0], "fill_color"), "#0000aa");
  EXPECT_EQ(text_of(f.root.children[1], "fill_color"), "#010203");
  // The diagram writes fill_color and text_color: its nodes inherit them.
  const figure g = bind_with(
      "root:\n  type: Flowchart\n  fill_color: \"#00ff00\"\n  text_color: \"#00ee00\"\n  children:\n"
      "    - {type: FlowNode, id: a, text: A}\n",
      t);
  EXPECT_EQ(text_of(g.root, "fill_color"), "#00ff00");
  EXPECT_EQ(text_of(g.root.children[0], "fill_color"), "");
  EXPECT_EQ(text_of(g.root.children[0], "text_color"), "");
}

TEST(Themes, BareTextStyleIsTheImages) {
  const chart_theme t = parse_chart_theme("t", R"(fields:
  text_color: "#123456"
  font: FiraSans-Regular.ttf
  FlowNode:text_color: "#654321"
)");
  const figure f = bind_with(kFlowchart, t);
  EXPECT_EQ(f.image.text_color, "#123456");
  EXPECT_EQ(f.image.font, "FiraSans-Regular.ttf");
  EXPECT_EQ(text_of(f.root, "text_color"), ""); // inherits the image's
  EXPECT_EQ(text_of(f.root.children[0], "text_color"), "#654321");
  // The image writing text_color leaves it to the image everywhere.
  const figure g = bind_with(std::string("image: {text_color: \"#ffffff\"}\n") + kFlowchart, t);
  EXPECT_EQ(g.image.text_color, "#ffffff");
  EXPECT_EQ(text_of(g.root.children[0], "text_color"), "");
}

TEST(Themes, ImageOptionsComeFromTheThemeUnlessWritten) {
  const chart_theme t = parse_chart_theme("t", R"(base: dark
image:
  background: "#102030"
  palette: ["#aa0000", "#00aa00"]
  padding: 20
)");
  const figure f = bind_with(kFlowchart, t);
  EXPECT_EQ(f.image.base, "dark");
  EXPECT_EQ(f.image.background, "#102030");
  EXPECT_EQ(f.image.palette, (std::vector<std::string>{"#aa0000", "#00aa00"}));
  EXPECT_EQ(f.image.padding, 20);
  const figure g = bind_with(std::string("image: {background: \"#ffffff\", padding: 4}\n") + kFlowchart, t);
  EXPECT_EQ(g.image.background, "#ffffff");
  EXPECT_EQ(g.image.padding, 4);
}

TEST(Themes, PlotSeriesTakeTheirSlotsRules) {
  const figure f = bind_source(
      "image: {theme: midnight}\ndata: {x: [1, 2], y: [3, 4]}\nroot: {type: Plot, children: [{type: PlotLine, "
      "xs: $x, ys: $y}, {type: PlotLine, xs: $x, ys: $y, line_weight: 1}]}\n",
      builtin_registry());
  EXPECT_EQ(number_of(f.root.children[0], "line_weight"), 2.5);
  EXPECT_EQ(number_of(f.root.children[1], "line_weight"), 1.0);
  EXPECT_EQ(f.image.palette.size(), 6u);
}

// ── Choosing a theme ─────────────────────────────────────────────────────────

TEST(Themes, TheSourceOrAnOverrideChoosesIt) {
  const figure plain = bind_source(kFlowchart, builtin_registry());
  EXPECT_EQ(plain.image.theme, "light");
  EXPECT_EQ(plain.image.base, "light");
  EXPECT_EQ(text_of(plain.root, "fill_color"), "");
  const figure ocean = bind_source(std::string("image: {theme: ocean}\n") + kFlowchart, builtin_registry());
  EXPECT_EQ(ocean.image.base, "dark");
  EXPECT_EQ(ocean.image.background, "#0f2233");
  EXPECT_FALSE(text_of(ocean.root, "fill_color").empty());
  const figure over = bind_source(std::string("image: {theme: ocean}\n") + kFlowchart, builtin_registry(), {"paper"});
  EXPECT_EQ(over.image.theme, "paper");
  EXPECT_EQ(over.image.font, "PT_Serif-Web-Regular.ttf");
  EXPECT_THROW(bind_source(kFlowchart, builtin_registry(), {"neon"}), std::invalid_argument);
  try {
    bind_source("image: {theme: oceans}\nroot: {type: Label, text: x}\n", builtin_registry());
    FAIL() << "unknown theme accepted";
  } catch (const error& e) {
    EXPECT_EQ(e.line(), 1);
    EXPECT_NE(std::string(e.what()).find("did you mean 'ocean'"), std::string::npos) << e.what();
  }
}

TEST(Themes, ARenderShowsTheBackground) {
  const figure f =
      bind_source("image: {theme: ocean, width: 64, height: 48}\nroot: {type: Label, text: x}\n", builtin_registry());
  const image img = render_figure(f);
  ASSERT_GT(img.rgba.size(), 4u);
  EXPECT_EQ(img.rgba[0], 0x0f);
  EXPECT_EQ(img.rgba[1], 0x22);
  EXPECT_EQ(img.rgba[2], 0x33);
}

TEST(Themes, JsonListsEveryTheme) {
  const std::string j = chart_themes_json();
  EXPECT_EQ(j.front(), '[');
  EXPECT_NE(j.find("\"id\":\"ocean\""), std::string::npos);
  EXPECT_NE(j.find("\"palette\":[\"#4cc9b0\""), std::string::npos);
}

} // namespace
} // namespace bdg::nymph
