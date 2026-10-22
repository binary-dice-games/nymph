// MIT License © 2026 Binary Dice Games
/// @file test_live_view_edit.cpp
/// @brief spec/08_view_editor plan step 3: edit mode records what the
///        figure draws, hit-tests it and outlines it, and changes nothing
///        for a view that is not in edit mode.
#include <gtest/gtest.h>
#include <imgui.h>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <variant>
#include <string>
#include <utility>
#include <vector>
#include "core/error.hpp"
#include "document/bind.hpp"
#include "edit/editor_model.hpp"
#include "document/view.hpp"
#include "elements/register_builtin.hpp"
#include "live_view_driver.hpp"
#include "png/png_meta.hpp"
#include "render/soft_rasterizer.hpp"
#include "view/live_view.hpp"

namespace bdg::nymph {
namespace {

namespace fs = std::filesystem;

const char* const kTwoPlots =
    "image: {width: 400, height: 300}\n"
    "root:\n"
    "  type: VerticalLayout\n"
    "  children:\n"
    "    - {type: Label, text: Report}\n"
    "    - type: HorizontalLayout\n"
    "      height: -1\n"
    "      children:\n"
    "        - type: Plot\n"
    "          title: Left\n"
    "          children:\n"
    "            - {type: PlotLine, label: a, ys: [1, 3, 2]}\n"
    "        - type: Plot\n"
    "          title: Right\n"
    "          children:\n"
    "            - {type: PlotLine, label: b, ys: [3, 1, 2]}\n";

const char* const kOnePlot =
    "image: {width: 400, height: 300}\n"
    "root:\n"
    "  type: Plot\n"
    "  title: Line\n"
    "  children:\n"
    "    - {type: PlotLine, label: a, ys: [1, 3, 2, 4]}\n";

/// The settled frame of `d` rasterized.
image rasterized(driven_view& d) {
  EXPECT_NE(d.last, nullptr);
  return d.last == nullptr ? image{} : rasterize(*d.last, d.v.textures(), d.v.clear_color());
}

/// The path of the first target under (`x`, `y`) with `type`, or "".
std::string path_of(const std::vector<edit_target>& found, const std::string& type) {
  for (const edit_target& t : found) {
    if (t.type == type) {
      return t.element;
    }
  }
  return {};
}

TEST(live_view_edit, OutsideEditModeThereAreNoTargets) {
  driven_view d(kTwoPlots);
  EXPECT_FALSE(d.v.edit_mode());
  EXPECT_TRUE(d.v.targets_at(100, 100).empty());
  EXPECT_FALSE(d.v.target_for("root", ""));
  EXPECT_FALSE(d.v.take_edit_click());
  d.click(100, 100);
  EXPECT_FALSE(d.v.take_edit_click());
}

TEST(live_view_edit, TargetsAtGivesTheSmallestElementFirst) {
  driven_view d(kTwoPlots);
  d.v.set_edit_mode(true);
  d.run();
  const std::vector<edit_target> list = d.v.targets_at(100, 150);
  ASSERT_FALSE(list.empty());
  // A point in the left half of the lower row: the left plot, then the row,
  // then the root layout, then the image options (the whole figure).
  EXPECT_EQ(list.front().type, "Plot");
  EXPECT_EQ(list.front().element, "root/children/1/children/0");
  ASSERT_GE(list.size(), 4u);
  EXPECT_EQ(list[1].type, "HorizontalLayout");
  EXPECT_EQ(list[list.size() - 2].element, "root");
  EXPECT_EQ(list.back().type, "Image");
  EXPECT_EQ(list.back().element, "image");
  // The right half selects the other plot.
  EXPECT_EQ(path_of(d.v.targets_at(300, 150), "Plot"), "root/children/1/children/1");
  // The label sits above the plots.
  EXPECT_EQ(path_of(d.v.targets_at(20, 8), "Label"), "root/children/0");
  // Outside the figure there is nothing.
  EXPECT_TRUE(d.v.targets_at(-5, -5).empty());
  EXPECT_TRUE(d.v.targets_at(1000, 1000).empty());
}

TEST(live_view_edit, TheMarginAwayFromEveryElementIsTheImageOptions) {
  driven_view d(kTwoPlots);
  d.v.set_edit_mode(true);
  d.run();
  // The default padding is 8 px: a point in it belongs to no element.
  const std::vector<edit_target> margin = d.v.targets_at(3, 3);
  ASSERT_EQ(margin.size(), 1u);
  EXPECT_EQ(margin[0].element, "image");
  EXPECT_EQ(margin[0].type, "Image");
  EXPECT_EQ(margin[0].field, "");
  EXPECT_FLOAT_EQ(margin[0].width, 400.0f);
  EXPECT_FLOAT_EQ(margin[0].height, 300.0f);
  d.click(3, 3);
  const std::optional<edit_target> clicked = d.v.take_edit_click();
  ASSERT_TRUE(clicked);
  EXPECT_EQ(clicked->element, "image");
  // It can be selected and outlined like an element, and survives a new source.
  d.v.set_selection(edit_target_id{"image", ""});
  d.v.set_source(kTwoPlots);
  EXPECT_TRUE(d.v.selection());
}

TEST(live_view_edit, ElementListFollowsTheTreeWithDepth) {
  driven_view d(kTwoPlots);
  const std::vector<edit_element> list = d.v.element_list();
  ASSERT_EQ(list.size(), 8u);
  EXPECT_EQ(list[0].path, "image");
  EXPECT_EQ(list[0].type, "Image");
  EXPECT_EQ(list[1].path, "root");
  EXPECT_EQ(list[1].depth, 0);
  EXPECT_EQ(list[2].path, "root/children/0");
  EXPECT_EQ(list[2].type, "Label");
  EXPECT_EQ(list[2].label, "Report");
  EXPECT_EQ(list[4].path, "root/children/1/children/0");
  EXPECT_EQ(list[4].label, "Left");
  EXPECT_EQ(list[4].depth, 2);
  EXPECT_EQ(list[5].type, "PlotLine");
  EXPECT_EQ(list[5].label, "a");
  EXPECT_EQ(list[5].depth, 3);
}

TEST(live_view_edit, TargetForFindsAnElementByPath) {
  driven_view d(kTwoPlots);
  d.v.set_edit_mode(true);
  d.run();
  const std::optional<edit_target> right = d.v.target_for("root/children/1/children/1", "");
  ASSERT_TRUE(right);
  EXPECT_EQ(right->type, "Plot");
  EXPECT_GT(right->x, 150.0f);
  EXPECT_GT(right->width, 50.0f);
  EXPECT_TRUE(d.v.target_for("root/children/1/children/1", "title"));
  EXPECT_FALSE(d.v.target_for("root/children/9", ""));
}

TEST(live_view_edit, AClickSelectsButADragDoesNot) {
  driven_view d(kTwoPlots);
  d.v.set_edit_mode(true);
  d.run();
  d.click(300, 150);
  const std::optional<edit_target> clicked = d.v.take_edit_click();
  ASSERT_TRUE(clicked);
  EXPECT_EQ(clicked->element, "root/children/1/children/1");
  EXPECT_FALSE(d.v.take_edit_click()); // once

  d.drag(280, 150, 340, 190);
  EXPECT_FALSE(d.v.take_edit_click());
}

TEST(live_view_edit, EditModeWithoutHoverOrSelectionDrawsTheSameImage) {
  driven_view plain(kTwoPlots);
  driven_view edit(kTwoPlots);
  edit.v.set_edit_mode(true);
  edit.run();
  const image a = rasterized(plain);
  const image b = rasterized(edit);
  ASSERT_EQ(a.width, b.width);
  EXPECT_TRUE(a.rgba == b.rgba);
}

TEST(live_view_edit, HoverAndSelectionAreOutlinedAndTurnOffAgain) {
  driven_view plain(kOnePlot);
  driven_view edit(kOnePlot);
  edit.v.set_edit_mode(true);
  edit.run();
  const image before = rasterized(edit);

  edit.v.set_selection(edit_target_id{"root", ""});
  edit.run();
  const image selected = rasterized(edit);
  EXPECT_FALSE(before.rgba == selected.rgba);

  edit.v.set_selection(std::nullopt);
  edit.run();
  EXPECT_TRUE(before.rgba == rasterized(edit).rgba);

  edit.move(200, 150);
  EXPECT_FALSE(before.rgba == rasterized(edit).rgba); // dashed outline under the pointer
  edit.send({pointer_event::kind::leave, 0, 0});
  edit.v.set_edit_mode(false);
  edit.run();
  EXPECT_TRUE(rasterized(plain).rgba == rasterized(edit).rgba);
}

TEST(live_view_edit, TheSelectionIsDroppedWhenItsElementDisappears) {
  driven_view d(kTwoPlots);
  d.v.set_edit_mode(true);
  d.v.set_selection(edit_target_id{"root/children/1/children/1", ""});
  d.run();
  d.v.set_source(kOnePlot); // no children/1/children/1
  EXPECT_FALSE(d.v.selection());

  d.v.set_source(kTwoPlots);
  d.v.set_selection(edit_target_id{"root/children/0", ""});
  d.v.set_source(kTwoPlots); // same figure: the selection stays
  ASSERT_TRUE(d.v.selection());
  EXPECT_EQ(d.v.selection()->element, "root/children/0");
}

// ── Plot texts ───────────────────────────────────────────────────────────────

const char* const kLabelled =
    "image: {width: 480, height: 320}\n"
    "root:\n"
    "  type: Plot\n"
    "  title: Revenue\n"
    "  x_label: month\n"
    "  y_label: kUSD\n"
    "  y2_label: share\n"
    "  children:\n"
    "    - {type: PlotLine, label: revenue, ys: [1, 3, 2, 4]}\n"
    "    - {type: PlotLine, label: share, ys: [10, 20, 15, 30], y_axis: Y2}\n";

/// The target of `field` in the settled view, with a failure when missing.
edit_target field_target(driven_view& d, const std::string& field, const std::string& element = "root") {
  const std::optional<edit_target> t = d.v.target_for(element, field);
  EXPECT_TRUE(t) << field;
  return t ? *t : edit_target{};
}

TEST(live_view_edit, PlotTextsAreTargetsWithTheirFields) {
  driven_view d(kLabelled, 480, 320);
  d.v.set_edit_mode(true);
  d.run();
  const edit_target title = field_target(d, "title");
  const edit_target x_label = field_target(d, "x_label");
  const edit_target y_label = field_target(d, "y_label");
  const edit_target y2_label = field_target(d, "y2_label");
  const edit_target legend = field_target(d, "legend_location");
  EXPECT_EQ(title.text, "Revenue");
  EXPECT_EQ(x_label.text, "month");

  // Each one is found by a click on its centre, before the plot itself.
  // The legend by its frame: its entries select their series.
  for (const edit_target& t : {title, x_label, y_label, y2_label, legend}) {
    const bool frame = t.field == "legend_location";
    const std::vector<edit_target> hit =
        d.v.targets_at(frame ? t.x + 1 : t.x + t.width / 2, frame ? t.y + 1 : t.y + t.height / 2);
    ASSERT_FALSE(hit.empty()) << t.field;
    EXPECT_EQ(hit.front().field, t.field);
    EXPECT_EQ(hit.front().element, "root");
  }
  // The title is above the plot area, the x label under it, the Y labels
  // on either side.
  const plot_view plot = d.plot();
  EXPECT_LT(title.y + title.height, plot.y + 2);
  EXPECT_GE(x_label.y, plot.y + plot.height);
  EXPECT_LT(y_label.x + y_label.width, plot.x);
  EXPECT_GE(y2_label.x, plot.x + plot.width);
  EXPECT_NEAR(title.x + title.width / 2, plot.x + plot.width / 2, 2.0f);
  // A click inside the plot area, away from the legend, is the plot's.
  const std::vector<edit_target> inside = d.v.targets_at(plot.x + plot.width / 2, plot.y + plot.height / 2);
  ASSERT_FALSE(inside.empty());
  EXPECT_EQ(inside.front().field, "");
}

TEST(live_view_edit, LegendEntriesSelectTheirSeries) {
  driven_view d(kLabelled, 480, 320);
  d.v.set_edit_mode(true);
  d.run();
  for (const char* path : {"root/children/0", "root/children/1"}) {
    const std::optional<edit_target> entry = d.v.target_for(path, "label");
    ASSERT_TRUE(entry) << path;
    EXPECT_EQ(entry->type, "PlotLine");
    const std::vector<edit_target> hit = d.v.targets_at(entry->x + entry->width / 2, entry->y + entry->height / 2);
    ASSERT_FALSE(hit.empty());
    EXPECT_EQ(hit.front().element, path);
    EXPECT_EQ(hit.front().field, "label");
  }
  EXPECT_EQ(d.v.target_for("root/children/0", "label")->text, "revenue");

  // A pie chart's slices select the chart.
  driven_view pie(
      "root: {type: Plot, title: P, children: [{type: PlotPieChart, values: [1, 2], labels: \"a\\nb\"}]}\n");
  pie.v.set_edit_mode(true);
  pie.run();
  const std::optional<edit_target> slice = pie.v.target_for("root/children/0", "");
  ASSERT_TRUE(slice);
  EXPECT_EQ(slice->type, "PlotPieChart");
}

TEST(live_view_edit, APlotWithoutATitleOffersNoTitleTarget) {
  driven_view d("root: {type: Plot, x_label: t, children: [{type: PlotLine, label: a, ys: [1, 2]}]}\n");
  d.v.set_edit_mode(true);
  d.run();
  EXPECT_FALSE(d.v.target_for("root", "title"));
  EXPECT_FALSE(d.v.target_for("root", "y_label"));
  EXPECT_TRUE(d.v.target_for("root", "x_label"));
  EXPECT_TRUE(d.v.target_for("root", ""));
}

TEST(live_view_edit, ALegendlessPlotOffersNoLegendTarget) {
  driven_view d("root: {type: Plot, title: T, flags: NoLegend, children: [{type: PlotLine, label: a, ys: [1, 2]}]}\n");
  d.v.set_edit_mode(true);
  d.run();
  EXPECT_FALSE(d.v.target_for("root", "legend_location"));
}

const fs::path kSourceDir(NYMPH_SOURCE_DIR);

std::string read_text(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

TEST(live_view_edit, EveryChartKindOffersItsTitleAsATarget) {
  struct expectation {
    const char* example;
    const char* element;
    const char* field;
  };
  for (const expectation& e : {
           expectation{"gantt", "root", "title"},
           expectation{"sankey", "root", "title"},
           expectation{"surface", "root", "title"},
           expectation{"line", "root", "title"},
       }) {
    SCOPED_TRACE(e.example);
    const figure f = bind_source(read_text(kSourceDir / "examples" / (std::string(e.example) + ".yaml")), builtin_registry());
    driven_view d(read_text(kSourceDir / "examples" / (std::string(e.example) + ".yaml")), static_cast<float>(f.image.width), static_cast<float>(f.image.height));
    d.v.set_edit_mode(true);
    d.run();
    const std::optional<edit_target> t = d.v.target_for(e.element, e.field);
    ASSERT_TRUE(t);
    EXPECT_FALSE(t->text.empty());
    EXPECT_GT(t->width, 10.0f);
    EXPECT_LT(t->height, 40.0f);
  }
}

TEST(live_view_edit, ALabelsTextIsATargetInsideItsElement) {
  driven_view d(kTwoPlots);
  d.v.set_edit_mode(true);
  d.run();
  const std::optional<edit_target> text = d.v.target_for("root/children/0", "text");
  ASSERT_TRUE(text);
  EXPECT_EQ(text->text, "Report");
  const std::vector<edit_target> hit = d.v.targets_at(text->x + 2, text->y + 2);
  ASSERT_FALSE(hit.empty());
  EXPECT_EQ(hit.front().field, "text");
  // Label text and element are both there; the text is the smaller.
  const std::optional<edit_target> whole = d.v.target_for("root/children/0", "");
  ASSERT_TRUE(whole);
  EXPECT_LE(text->width * text->height, whole->width * whole->height);
}

// ── Diagram texts ────────────────────────────────────────────────────────────

const char* const kFlow =
    "image: {width: 500, height: 360}\n"
    "root:\n"
    "  type: Flowchart\n"
    "  title: Deploy\n"
    "  children:\n"
    "    - {type: FlowNode, id: build, text: Build it}\n"
    "    - {type: FlowNode, id: tests}\n"
    "    - type: FlowGroup\n"
    "      id: prod\n"
    "      title: Production\n"
    "      children:\n"
    "        - {type: FlowNode, id: ship, text: \"Ship\\nit\"}\n"
    "    - {type: FlowLink, from: build, to: tests, label: next}\n"
    "    - {type: FlowLink, from: tests, to: ship}\n";

TEST(live_view_edit, FlowchartTextsMapToTheFieldsThatShowThem) {
  driven_view d(kFlow, 500, 360);
  d.v.set_edit_mode(true);
  d.run();
  const auto text_of = [&](const std::string& element, const std::string& field) {
    const std::optional<edit_target> t = d.v.target_for(element, field);
    EXPECT_TRUE(t) << element << "." << field;
    return t ? t->text : std::string();
  };
  EXPECT_EQ(text_of("root", "title"), "Deploy");
  EXPECT_EQ(text_of("root/children/0", "text"), "Build it");
  EXPECT_EQ(text_of("root/children/2", "title"), "Production");
  EXPECT_EQ(text_of("root/children/2/children/0", "text"), "Ship\nit");
  EXPECT_EQ(text_of("root/children/3", "label"), "next");
  // A node without text shows its id; the text field is what to edit.
  EXPECT_EQ(text_of("root/children/1", "text"), "tests");
  // A link without a label draws no text.
  EXPECT_FALSE(d.v.target_for("root/children/4", "label"));
}

TEST(live_view_edit, ADiagramTextIsFoundBeforeItsNodeAndTheDiagram) {
  driven_view d(kFlow, 500, 360);
  d.v.set_edit_mode(true);
  d.run();
  const edit_target text = *d.v.target_for("root/children/0", "text");
  const std::vector<edit_target> hit = d.v.targets_at(text.x + text.width / 2, text.y + text.height / 2);
  ASSERT_EQ(hit.size(), 4u); // the text, its node, the diagram, the image
  EXPECT_EQ(hit[0].element, "root/children/0");
  EXPECT_EQ(hit[0].field, "text");
  EXPECT_EQ(hit[1].element, "root/children/0");
  EXPECT_EQ(hit[1].field, "");
  EXPECT_EQ(hit[2].element, "root");
  EXPECT_EQ(hit[2].field, "");
}

/// A point in `whole` where `element` is the best target, if any.
std::optional<std::pair<float, float>> point_selecting(
    const live_view& v, const edit_target& whole, const std::string& element) {
  for (float y = whole.y; y <= whole.y + whole.height; y += 1.0f) {
    for (float x = whole.x; x <= whole.x + whole.width; x += 1.0f) {
      const std::vector<edit_target> hit = v.targets_at(x, y);
      if (!hit.empty() && hit[0].element == element && hit[0].field.empty()) {
        return std::pair<float, float>{x, y};
      }
    }
  }
  return std::nullopt;
}

TEST(live_view_edit, ADiagramNodesBoxAwayFromItsTextSelectsTheNode) {
  driven_view d(kFlow, 500, 360);
  d.v.set_edit_mode(true);
  d.run();
  const std::optional<edit_target> box = d.v.target_for("root/children/0", "");
  const std::optional<edit_target> text = d.v.target_for("root/children/0", "text");
  ASSERT_TRUE(box);
  ASSERT_TRUE(text);
  EXPECT_GT(box->width, text->width);
  const std::vector<edit_target> hit = d.v.targets_at(box->x + 2.0f, box->y + box->height / 2);
  ASSERT_FALSE(hit.empty());
  EXPECT_EQ(hit[0].element, "root/children/0");
  EXPECT_EQ(hit[0].field, "");
  // A click there selects the node.
  d.click(box->x + 2.0f, box->y + box->height / 2);
  const std::optional<edit_target> clicked = d.v.take_edit_click();
  ASSERT_TRUE(clicked);
  EXPECT_EQ(clicked->element, "root/children/0");
}

TEST(live_view_edit, ALinksPathSelectsTheLinkAndIsReportedWhole) {
  driven_view d(kFlow, 500, 360);
  d.v.set_edit_mode(true);
  d.run();
  // The link without a label: only its path can be clicked.
  const std::optional<edit_target> whole = d.v.target_for("root/children/4", "");
  ASSERT_TRUE(whole);
  const auto at = point_selecting(d.v, *whole, "root/children/4");
  ASSERT_TRUE(at);
  const std::vector<edit_target> hit = d.v.targets_at(at->first, at->second);
  // The hit is the whole path, not the piece under the pointer.
  EXPECT_EQ(hit[0].x, whole->x);
  EXPECT_EQ(hit[0].y, whole->y);
  EXPECT_EQ(hit[0].width, whole->width);
  EXPECT_EQ(hit[0].height, whole->height);
  // Far from every path and node (a corner of the diagram): the diagram.
  const std::optional<edit_target> diagram = d.v.target_for("root", "");
  ASSERT_TRUE(diagram);
  const std::vector<edit_target> margin = d.v.targets_at(diagram->x + 2.0f, diagram->y + diagram->height - 2.0f);
  ASSERT_FALSE(margin.empty());
  EXPECT_EQ(margin[0].element, "root");
}

TEST(live_view_edit, SequenceArrowsAndParticipantsSelectTheirElements) {
  const std::string source = read_text(kSourceDir / "examples" / "uml_sequence.yaml");
  const figure f = bind_source(source, builtin_registry());
  driven_view d(source, static_cast<float>(f.image.width), static_cast<float>(f.image.height));
  d.v.set_edit_mode(true);
  d.run();
  int messages = 0, participants = 0;
  for (const edit_element& e : d.v.element_list()) {
    if (e.type != "Message" && e.type != "Participant") {
      continue;
    }
    SCOPED_TRACE(e.path);
    const std::optional<edit_target> whole = d.v.target_for(e.path, "");
    ASSERT_TRUE(whole);
    EXPECT_TRUE(point_selecting(d.v, *whole, e.path));
    ++(e.type == "Message" ? messages : participants);
  }
  EXPECT_GE(messages, 4);
  EXPECT_GE(participants, 2);
}

TEST(live_view_edit, EditModeDrawsADiagramLikeThePlainView) {
  driven_view plain(kFlow, 500, 360);
  driven_view edit(kFlow, 500, 360);
  edit.v.set_edit_mode(true);
  edit.run();
  EXPECT_TRUE(rasterized(plain).rgba == rasterized(edit).rgba);
}

TEST(live_view_edit, ATextBoundToAColumnIsReadOnly) {
  driven_view d(
      "image: {width: 300, height: 120}\n"
      "root:\n"
      "  type: VerticalLayout\n"
      "  children:\n"
      "    - {type: Label, text: $names}\n"
      "    - {type: Label, text: literal}\n"
      "data: |\n"
      "  names\n"
      "  Ada\n");
  d.v.set_edit_mode(true);
  d.run();
  const std::optional<edit_target> bound = d.v.target_for("root/children/0", "text");
  ASSERT_TRUE(bound);
  EXPECT_EQ(bound->text, "Ada");
  EXPECT_EQ(bound->read_only_reason, "bound to $names");
  const std::optional<edit_target> literal = d.v.target_for("root/children/1", "text");
  ASSERT_TRUE(literal);
  EXPECT_EQ(literal->read_only_reason, "");
}

TEST(live_view_edit, AutonumberedSequenceMessagesMapToTheirText) {
  const std::string source = read_text(kSourceDir / "examples" / "uml_sequence.yaml");
  const figure f = bind_source(source, builtin_registry());
  driven_view d(source, static_cast<float>(f.image.width), static_cast<float>(f.image.height));
  d.v.set_edit_mode(true);
  d.run();
  int messages = 0;
  for (const edit_element& e : d.v.element_list()) {
    if (e.type == "Message" && d.v.target_for(e.path, "text")) {
      ++messages;
    }
  }
  EXPECT_GE(messages, 4);
}

/// Every diagram example: the text targets agree with the source, and the
/// fraction of texts that map to a field is reported.
TEST(live_view_edit, EveryDiagramExampleMapsItsTextsToFields) {
  const bool verbose = std::getenv("NYMPH_EDIT_VERBOSE") != nullptr;
  for (const char* name : {"uml_class", "uml_sequence", "uml_state", "uml_activity", "uml_usecase", "uml_er",
                           "uml_dataflow", "uml_network", "flowchart"}) {
    SCOPED_TRACE(name);
    const std::string source = read_text(kSourceDir / "examples" / (std::string(name) + ".yaml"));
    const figure f = bind_source(source, builtin_registry());
    driven_view d(source, static_cast<float>(f.image.width), static_cast<float>(f.image.height));
    d.v.set_edit_mode(true);
    d.run();
    const std::map<std::string, const node*, std::less<>> paths = element_paths(f.root);
    int mapped = 0;
    for (const edit_element& e : d.v.element_list()) {
      if (e.path == "image") {
        continue;
      }
      const node* n = paths.at(e.path);
      for (const field_desc& fd : n->cls->fields()) {
        const std::optional<edit_target> t = d.v.target_for(e.path, fd.name);
        if (!t) {
          continue;
        }
        ++mapped;
        const size_t index = static_cast<size_t>(n->cls->field_index(fd.name));
        ASSERT_TRUE(std::holds_alternative<std::string>(n->values[index])) << e.path << "." << fd.name;
        const std::string& value = n->get<std::string>(index);
        // The text drawn is the field's value, or the id a node shows in place of an empty text.
        EXPECT_TRUE(t->text == value || value.empty()) << e.path << "." << fd.name << ": '" << t->text << "'";
        EXPECT_GT(t->width, 0.0f);
        if (verbose) {
          std::printf("%s %s.%s = '%s'\n", name, e.path.c_str(), fd.name.c_str(), t->text.c_str());
        }
      }
    }
    EXPECT_GE(mapped, 2) << "no text of " << name << " maps to a field";
  }
}

TEST(live_view_edit, TheThemeIsTheOneTheFigureIsDrawnWith) {
  live_view v;
  EXPECT_EQ(v.theme(), "light"); // no figure yet
  v.set_source("image: {width: 200, height: 120, theme: dark}\nroot: {type: Plot, title: T}\n");
  EXPECT_EQ(v.theme(), "dark"); // the source's own
  v.set_theme("classic");
  EXPECT_EQ(v.theme(), "classic"); // the override wins
  v.set_theme("");
  EXPECT_EQ(v.theme(), "dark");
}

/// Set NYMPH_EDIT_DUMP to a directory to look at the outlines by eye.
TEST(live_view_edit, DumpOutlinesForInspection) {
  const char* dir = std::getenv("NYMPH_EDIT_DUMP");
  if (dir == nullptr) {
    GTEST_SKIP();
  }
  driven_view d(kLabelled, 480, 320);
  d.v.set_edit_mode(true);
  d.run();
  for (const char* field : {"title", "x_label", "y_label", "y2_label", "legend_location"}) {
    d.v.set_selection(edit_target_id{"root", field});
    d.run();
    const image img = rasterized(d);
    const std::vector<uint8_t> png = encode_png(img, "", "");
    std::ofstream out(std::string(dir) + "/" + field + ".png", std::ios::binary);
    out.write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
  }
}

TEST(live_view_edit, AnEmptyLabelInARootLayoutDrawsInEditMode) {
  driven_view d("root: {type: VerticalLayout, children: [{type: Label}]}\n");
  d.v.set_edit_mode(true);
  d.run();
  EXPECT_NE(d.last, nullptr);
}

TEST(live_view_edit, ALabelAfterAnOrgChartAndGanttDrawsInEditMode) {
  driven_view d(
      "root: {type: VerticalLayout, children: ["
      "{type: OrgChart, title: Acme, children: [{type: Person, id: a, name: Ana}]}, "
      "{type: GanttChart, children: [{type: GanttTask, name: Task, start: 0, duration: 1}]}, "
      "{type: Label}]}\n");
  d.v.set_edit_mode(true);
  d.run();
  EXPECT_NE(d.last, nullptr);
}

TEST(live_view_edit, AnImageInARootLayoutDrawsInEditMode) {
  driven_view d("root: {type: VerticalLayout, children: [{type: Label, text: a}, {type: Image}]}\n");
  d.v.set_edit_mode(true);
  d.v.set_selection(edit_target_id{"root/children/1", ""});
  d.run();
  ASSERT_NE(d.last, nullptr);
  const auto list = d.v.element_list();
  EXPECT_EQ(list.size(), 4u);
}

TEST(live_view_edit, ASourceTheModelAcceptsShowsInTheView) {
  editor_model m(
      "image: {width: 400, height: 100}\n"
      "root: {type: VerticalLayout, children: [{type: Label, text: a}]}\n");
  driven_view d(m.source());
  // An Image does not fit: refused, so the model never holds what the view cannot show.
  const commit_result r = m.add_element("root", kAppend, "Image");
  EXPECT_FALSE(r.ok);
  EXPECT_NO_THROW(d.v.set_source(m.source()));
}


// ── Style edits redraw ───────────────────────────────────────────────────────

/// A plot with one series of `type`, its `style` fields and `body`.
std::string styled_plot(const std::string& type, const std::string& body, const std::string& style) {
  const std::string plot = type.rfind("Plot3D", 0) == 0 ? "Plot3D" : "Plot";
  return "image: {width: 300, height: 200}\n"
         "root:\n"
         "  type: " +
         plot +
         "\n"
         "  children:\n"
         "    - {type: " +
         type + ", label: s, " + body + (style.empty() ? "" : ", " + style) + "}\n";
}

/// Editing a series' style in an open view draws what a fresh view of the
/// edited source draws: nothing kept from the first source wins.
TEST(live_view_edit, StyleEditsInAnOpenViewMatchAFreshView) {
  const std::string xy = "xs: [0, 1, 2, 3], ys: [1, 3, 2, 4]";
  const std::vector<std::pair<std::string, std::string>> series{
      {"PlotLine", xy},
      {"PlotScatter", xy},
      {"PlotBars", xy},
      {"PlotShaded", xy},
      {"PlotStems", xy},
      {"PlotHistogram", "values: [1, 2, 2, 3, 3, 3]"},
      {"PlotInfLines", "values: [1, 2]"},
      {"Plot3DLine", "xs: [0, 1], ys: [0, 1], zs: [0, 1]"},
      {"Plot3DScatter", "xs: [0, 1], ys: [0, 1], zs: [0, 1]"},
  };
  const std::vector<std::pair<std::string, std::string>> edits{
      {"", "color: \"#00FF00\""},
      {"color: \"#00FF00\"", "color: \"#FF0000\""},
      {"color: \"#00FF00\"", ""},
      {"", "fill_color: \"#FF00FF\""},
      {"", "line_weight: 5"},
  };
  for (const auto& [type, body] : series) {
    for (const auto& [before, after] : edits) {
      const std::string field = (after.empty() ? before : after).substr(0, (after.empty() ? before : after).find(':'));
      if (builtin_registry().find(type)->field_index(field) < 0) {
        continue; // not a field of this series type
      }
      SCOPED_TRACE(type + ": [" + before + "] -> [" + after + "]");
      driven_view edited(styled_plot(type, body, before));
      edited.v.set_source(styled_plot(type, body, after));
      edited.run();
      driven_view fresh(styled_plot(type, body, after));
      const image a = rasterized(edited);
      const image b = rasterized(fresh);
      EXPECT_TRUE(a.rgba == b.rgba);
    }
  }
}


/// A legend entry hidden in the open view stays hidden through an edit.
TEST(live_view_edit, AHiddenSeriesStaysHiddenThroughAnEdit) {
  for (const std::string type : {"PlotLine", "Plot3DLine"}) {
    SCOPED_TRACE(type);
    const bool flat = type == "PlotLine";
    const std::string body = flat ? "xs: [0, 1, 2], ys: [1, 3, 2]" : "xs: [0, 1, 2], ys: [1, 3, 2], zs: [0, 1, 2]";
    const std::string plot = flat ? "Plot" : "Plot3D";
    const auto source = [&](const std::string& color) {
      return "image: {width: 300, height: 200}\n"
             "root:\n"
             "  type: " +
          plot +
          "\n"
          "  children:\n"
          "    - {type: " +
          type + ", label: a, " + body + ", color: \"" + color +
          "\"}\n"
          "    - {type: " +
          type + ", label: b, " + body +
          "}\n"
          "view:\n"
          "  root: {hidden: [a]}\n";
    };
    driven_view d(source("#00FF00"));
    ASSERT_EQ(d.plot().hidden, std::vector<std::string>{"a"});
    d.v.set_source(source("#FF0000"));
    d.run();
    EXPECT_EQ(d.plot().hidden, std::vector<std::string>{"a"});
  }
}

} // namespace
} // namespace bdg::nymph
