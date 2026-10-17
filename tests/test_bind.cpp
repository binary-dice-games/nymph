// MIT License © 2026 Binary Dice Games
/// @file test_bind.cpp
/// @brief Plan step 8: bind_source() turns a whole source into a checked figure, or
///        one positioned error. Adapted from wish's test_nymph_bind.cpp to
///        the YAML form, with fake element classes.
#include <gtest/gtest.h>
#include <cmath>
#include <memory>
#include <string>
#include <vector>
#include "core/error.hpp"
#include "core/limits.hpp"
#include "document/bind.hpp"
#include "schema/element_class.hpp"
#include "schema/registry.hpp"

namespace bdg::nymph {
namespace {

// ── Fake element classes ─────────────────────────────────────────────────────

const name_table& plot_flag_names() {
  static const name_table t{{"None", 0}, {"NoTitle", 1}, {"NoLegend", 2}};
  return t;
}

class fake : public element_class {
 public:
  using element_class::element_class;
  void draw(const node&, draw_context&) const override {}
};

/// Plot: fills the image as root when `height` is left out.
class fake_plot : public fake {
 public:
  fake_plot()
      : fake(
            "Plot",
            "figure",
            {"plot2d.item"},
            {text_field("title", "", "Title."),
             text_field("x_label", "", "X label."),
             number_field("height", 300, "Height.", value_range{-1, 8192}),
             flags_field("flags", 0, plot_flag_names(), "Flags.")},
            "A 2D plot.") {}
  void adjust_root(node& n, const std::vector<bool>& set) const override {
    if (!set[2]) {
      n.values[2] = -1.0;
    }
  }
};

std::vector<field_desc> series_fields() {
  return {
      text_field("label", "", "Legend label."),
      numbers_field("xs", "X values."),
      numbers_field("ys", "Y values."),
      color_field("color", "Colour.")};
}

registry fake_registry() {
  registry reg;
  reg.add(std::make_unique<fake_plot>());
  reg.add(std::make_unique<fake>("PlotLine", "plot2d.item", std::vector<std::string>{}, series_fields(), "Line."));
  reg.add(std::make_unique<fake>("PlotBars", "plot2d.item", std::vector<std::string>{}, series_fields(), "Bars."));
  reg.add(std::make_unique<fake>(
      "PlotPieChart",
      "plot2d.item",
      std::vector<std::string>{},
      std::vector<field_desc>{text_field("labels", "", "Labels."), numbers_field("values", "Values.")},
      "Pie."));
  reg.add(std::make_unique<fake>(
      "PlotHeatmap",
      "plot2d.item",
      std::vector<std::string>{},
      std::vector<field_desc>{numbers_field("values", "Grid."), int_field("rows", 1, "Rows.")},
      "Heatmap."));
  reg.add(std::make_unique<fake>(
      "Mesh",
      "plot2d.item",
      std::vector<std::string>{},
      std::vector<field_desc>{integers_field("indices", "I.")},
      "Mesh."));
  reg.add(std::make_unique<fake>(
      "VerticalLayout",
      "figure",
      std::vector<std::string>{"figure"},
      std::vector<field_desc>{number_field("spacing", 0, "Spacing.")},
      "Column."));
  return reg;
}

const registry& reg() {
  static const registry r = fake_registry();
  return r;
}

/// The error binding `source`.
error bind_error(const std::string& source) {
  try {
    bind_source(source, reg());
  } catch (const error& e) {
    return e;
  }
  ADD_FAILURE() << "no error for:\n" << source;
  return error(0, 0, "");
}

bool contains(const error& e, const std::string& part) {
  return std::string(e.what()).find(part) != std::string::npos;
}

/// Expects `source` to fail at `line`:`column` with a message containing
/// `part`.
void expect_error_at(const std::string& source, int line, int column, const std::string& part) {
  const error e = bind_error(source);
  EXPECT_EQ(e.line(), line) << e.what();
  EXPECT_EQ(e.column(), column) << e.what();
  EXPECT_TRUE(contains(e, part)) << "message: " << e.what() << "\nexpected: " << part;
}

const char* const kExample =
    "nymph: 1\n" // 1
    "description: Monthly revenue against cost, 2025.\n" // 2
    "image:\n" // 3
    "  width: 800\n" // 4
    "  height: 450\n" // 5
    "root:\n" // 6
    "  type: Plot\n" // 7
    "  title: Revenue vs cost\n" // 8
    "  x_label: month\n" // 9
    "  children:\n" // 10
    "    - type: PlotLine\n" // 11
    "      label: revenue\n" // 12
    "      xs: $month\n" // 13
    "      ys: $revenue\n" // 14
    "    - type: PlotBars\n" // 15
    "      label: cost\n" // 16
    "      xs: $month\n" // 17
    "      ys: $cost\n" // 18
    "data: |\n" // 19
    "  month, revenue, cost\n" // 20
    "  1, 12, 9\n" // 21
    "  2, 15, 9.5\n"; // 22

/// The spec/01_core_features/DESIGN.md example with `from` replaced by `to`.
std::string example_with(const std::string& from, const std::string& to) {
  std::string s = kExample;
  const size_t at = s.find(from);
  EXPECT_NE(at, std::string::npos) << from;
  return s.replace(at, from.size(), to);
}

// ── The example ──────────────────────────────────────────────────────────────

TEST(bind, DesignExampleBinds) {
  const figure f = bind_source(kExample, reg());
  EXPECT_EQ(f.description, "Monthly revenue against cost, 2025.");
  EXPECT_EQ(f.image.width, 800);
  EXPECT_EQ(f.image.height, 450);
  EXPECT_EQ(f.image.scale, 1);
  EXPECT_EQ(f.image.theme, "light");
  ASSERT_NE(f.root.cls, nullptr);
  EXPECT_EQ(f.root.cls->name(), "Plot");
  EXPECT_EQ(f.root.get<std::string>(0), "Revenue vs cost");
  EXPECT_EQ(f.root.line, 7);
  ASSERT_EQ(f.root.children.size(), 2u);
  const node& line = f.root.children[0];
  EXPECT_EQ(line.cls->name(), "PlotLine");
  EXPECT_EQ(line.get<std::string>(0), "revenue");
  EXPECT_EQ(line.get<std::vector<double>>(1), (std::vector<double>{1, 2}));
  EXPECT_EQ(line.get<std::vector<double>>(2), (std::vector<double>{12, 15}));
  EXPECT_EQ(f.root.children[1].get<std::vector<double>>(2), (std::vector<double>{9, 9.5}));
}

TEST(bind, RootPlotFillsUnlessItSetsHeight) {
  EXPECT_EQ(bind_source(kExample, reg()).root.get<double>(2), -1.0);
  EXPECT_EQ(
      bind_source(example_with("  title: Revenue vs cost\n", "  height: 200\n"), reg()).root.get<double>(2), 200.0);
  // Only the root: a plot inside a layout keeps its default.
  const figure f = bind_source("root: {type: VerticalLayout, children: [{type: Plot}]}\n", reg());
  EXPECT_EQ(f.root.children[0].get<double>(2), 300.0);
}

TEST(bind, MinimalSourceNeedsOnlyRoot) {
  const figure f = bind_source("root: {type: Plot}\n", reg());
  EXPECT_EQ(f.description, "");
  EXPECT_EQ(f.image.width, 800);
  EXPECT_EQ(f.image.height, 500);
  EXPECT_EQ(f.image.padding, 8);
}

TEST(bind, LiteralArraysNeedNoData) {
  const figure f = bind_source("root:\n  type: Plot\n  children:\n    - {type: PlotLine, ys: [1, 2.5, 4]}\n", reg());
  EXPECT_EQ(f.root.children[0].get<std::vector<double>>(2), (std::vector<double>{1, 2.5, 4}));
}

TEST(bind, NamedChildrenMappingIsAccepted) {
  const figure f = bind_source(
      "root:\n  type: Plot\n  children:\n    first: {type: PlotLine, label: a}\n    second: {type: PlotBars, label: b}\n",
      reg());
  ASSERT_EQ(f.root.children.size(), 2u);
  EXPECT_EQ(f.root.children[1].get<std::string>(0), "b");
}

// ── Column references ────────────────────────────────────────────────────────

TEST(bind, ReferencesByPositionAndQuotedName) {
  const figure f = bind_source(
      "root:\n  type: Plot\n  children:\n    - {type: PlotLine, xs: $1, ys: \"$unit price\"}\n"
      "data: |\n  n, unit price\n  1, 2.5\n",
      reg());
  EXPECT_EQ(f.root.children[0].get<std::vector<double>>(1), (std::vector<double>{1}));
  EXPECT_EQ(f.root.children[0].get<std::vector<double>>(2), (std::vector<double>{2.5}));
}

TEST(bind, ListOfReferencesInterleavesByRow) {
  const figure f = bind_source(
      "root:\n  type: Plot\n  children:\n    - {type: PlotHeatmap, values: [$a, $b, $c], rows: 2}\n"
      "data: |\n  a, b, c\n  1, 2, 3\n  4, 5, 6\n",
      reg());
  EXPECT_EQ(f.root.children[0].get<std::vector<double>>(0), (std::vector<double>{1, 2, 3, 4, 5, 6}));
}

TEST(bind, ReferenceOnATextFieldJoinsWithNewlines) {
  const figure f = bind_source(
      "root:\n  type: Plot\n  children:\n    - {type: PlotPieChart, labels: $name, values: $share}\n"
      "data: {name: [north, south, east], share: [1, 2, 3]}\n",
      reg());
  EXPECT_EQ(f.root.children[0].get<std::string>(0), "north\nsouth\neast");
}

TEST(bind, ReferenceOnAnIntegerListRounds) {
  const figure f = bind_source(
      "root:\n  type: Plot\n  children:\n    - {type: Mesh, indices: $i}\ndata: {i: [0, 1.4, 2.6]}\n", reg());
  EXPECT_EQ(f.root.children[0].get<std::vector<int64_t>>(0), (std::vector<int64_t>{0, 1, 3}));
}

TEST(bind, EscapedDollarIsALiteral) {
  const figure f = bind_source("root: {type: Plot, title: $$5 per unit}\n", reg());
  EXPECT_EQ(f.root.get<std::string>(0), "$5 per unit");
}

TEST(bind, EmptyCellIsAGap) {
  const figure f = bind_source(
      "root:\n  type: Plot\n  children:\n    - {type: PlotLine, ys: $v}\ndata: |\n  v\n  1\n  \"\"\n  3\n", reg());
  const auto ys = f.root.children[0].get<std::vector<double>>(2);
  ASSERT_EQ(ys.size(), 3u);
  EXPECT_TRUE(std::isnan(ys[1]));
}

TEST(bind, UnusedTextColumnsAreNeverChecked) {
  EXPECT_NO_THROW(bind_source(
      "root:\n  type: Plot\n  children:\n    - {type: PlotLine, ys: $v}\ndata: |\n  name, v\n  x, 1\n", reg()));
}

// ── Errors, positioned ───────────────────────────────────────────────────────

TEST(bind, TopLevelErrors) {
  expect_error_at("description: x\n", 1, 1, "missing 'root'");
  expect_error_at("root: {type: Plot}\nimgae: {}\n", 2, 1, "unknown key 'imgae' (did you mean 'image'?)");
  expect_error_at("nymph: 2\nroot: {type: Plot}\n", 1, 8, "unsupported format version '2'");
  expect_error_at("nymph: \"1\"\nroot: {type: Plot}\n", 1, 8, "unsupported format version");
  expect_error_at("- a\n- b\n", 1, 1, "a nymph source is a mapping");
  expect_error_at("description: [a]\nroot: {type: Plot}\n", 1, 14, "'description' must be text");
}

TEST(bind, ElementErrors) {
  expect_error_at(
      example_with("type: PlotLine", "type: PlotLyne"),
      11,
      13,
      "unknown element type 'PlotLyne' (did you mean 'PlotLine'?)");
  expect_error_at(
      example_with("label: revenue", "lable: revenue"), 12, 7, "PlotLine has no field 'lable' (did you mean 'label'?)");
  expect_error_at("root: {title: x}\n", 1, 7, "has no 'type'");
  expect_error_at("root: [1]\n", 1, 7, "an element must be a mapping");
  expect_error_at(
      "root:\n  type: Plot\n  children:\n    - {type: PlotLine, children: []}\n", 4, 24, "PlotLine takes no children");
}

TEST(bind, PlacementErrors) {
  expect_error_at("root: {type: PlotLine}\n", 1, 14, "'PlotLine' cannot be the root; it goes inside: Plot");
  expect_error_at(
      "root:\n  type: VerticalLayout\n  children:\n    - {type: PlotLine}\n",
      4,
      14,
      "'PlotLine' cannot be placed in VerticalLayout; it goes inside: Plot");
  expect_error_at("root:\n  type: Plot\n  children:\n    - {type: Plot}\n", 4, 14, "'Plot' cannot be placed in Plot");
}

TEST(bind, ReferenceErrors) {
  expect_error_at(
      example_with("ys: $revenue", "ys: $revenu"),
      14,
      11,
      "unknown column 'revenu' (did you mean 'revenue'?) (columns: month, revenue, cost)");
  expect_error_at(
      example_with("title: Revenue vs cost", "height: $cost"), 8, 11, "Plot.height cannot take a $column reference");
  expect_error_at(
      example_with("ys: $revenue", "ys: [$revenue, 2]"), 14, 22, "mixes $column references and literal values");
  expect_error_at(
      "root:\n  type: Plot\n  children:\n    - {type: PlotLine, ys: $v}\n", 4, 28, "the source has no data");
}

TEST(bind, DataErrorsPointAtTheCell) {
  // `oops` is in the revenue column, row 2, line 22 column 6.
  expect_error_at(example_with("2, 15, 9.5", "2, oops, 9.5"), 22, 6, "column 'revenue', row 2: 'oops' is not a number");
}

TEST(bind, ValueErrorsComeFromTheField) {
  expect_error_at(
      example_with("title: Revenue vs cost", "flags: NoTitel"), 8, 10, "Plot.flags: unknown value 'NoTitel'");
  expect_error_at(
      example_with("label: cost", "color: #ff0000"),
      16,
      13,
      "PlotBars.color is empty; an unquoted '#' starts a YAML comment");
}

TEST(bind, ImageOptionErrors) {
  expect_error_at("image: {width: 10}\nroot: {type: Plot}\n", 1, 16, "image.width must be between 16 and 4096");
  expect_error_at("image: {height: 5000}\nroot: {type: Plot}\n", 1, 17, "image.height must be between 16 and 4096");
  expect_error_at("image: {scale: 5}\nroot: {type: Plot}\n", 1, 16, "image.scale must be between 1 and 4");
  expect_error_at("image: {width: 4000, scale: 3}\nroot: {type: Plot}\n", 1, 29, "must not exceed 8192");
  expect_error_at("image: {width: 100, padding: 50}\nroot: {type: Plot}\n", 1, 30, "image.padding leaves no room");
  expect_error_at("image: {theme: solarized}\nroot: {type: Plot}\n", 1, 16, "unknown theme 'solarized'");
  expect_error_at(
      "image: {widht: 100}\nroot: {type: Plot}\n", 1, 9, "image has no option 'widht' (did you mean 'width'?)");
  expect_error_at("image: 5\nroot: {type: Plot}\n", 1, 8, "'image' must be a mapping");
}

TEST(bind, SizeLimitsComeBeforeParsing) {
  const error big = bind_error(std::string(kMaxSourceBytes + 1, ' '));
  EXPECT_EQ(big.line(), 0);
  EXPECT_TRUE(contains(big, "larger than 16 MiB"));

  std::string rows = "root: {type: Plot}\ndata: |\n  v\n";
  for (size_t i = 0; i <= kMaxDataRows; ++i) {
    rows += "  1\n";
  }
  EXPECT_TRUE(contains(bind_error(rows), "more than 1000000 rows"));
}

// ── Named tables ─────────────────────────────────────────────────────────────

const char* const kTables =
    "data:\n"
    "  sales: |\n"
    "    month, revenue\n"
    "    1, 12\n"
    "    2, 15\n"
    "  costs:\n"
    "    month: [1, 2, 3]\n"
    "    cost: [9, 9.5, 10]\n";

/// A one-series source over kTables, with `series` as the series mapping.
std::string with_tables(const std::string& series) {
  return "root:\n  type: Plot\n  children:\n    - " + series + "\n" + kTables; // series on line 4
}

TEST(bind, TableColonColumnPicksTheTable) {
  const figure f = bind_source(with_tables("{type: PlotLine, xs: $sales:month, ys: $sales:2}"), reg());
  EXPECT_EQ(f.root.children[0].get<std::vector<double>>(1), (std::vector<double>{1, 2}));
  EXPECT_EQ(f.root.children[0].get<std::vector<double>>(2), (std::vector<double>{12, 15}));
  const figure g = bind_source(with_tables("{type: PlotLine, xs: $costs:month, ys: $costs:cost}"), reg());
  EXPECT_EQ(g.root.children[0].get<std::vector<double>>(2), (std::vector<double>{9, 9.5, 10}));
}

TEST(bind, BareColumnWorksWithOneNamedTable) {
  const figure f = bind_source(
      "root:\n  type: Plot\n  children:\n    - {type: PlotLine, ys: $v}\ndata:\n  only: {v: [4, 5]}\n", reg());
  EXPECT_EQ(f.root.children[0].get<std::vector<double>>(2), (std::vector<double>{4, 5}));
}

TEST(bind, BareColumnWithSeveralTablesNamesThem) {
  expect_error_at(
      with_tables("{type: PlotLine, ys: $month}"),
      4,
      28,
      "'$month' needs a table, as there are several: write $sales:month (tables with column 'month': sales, costs)");
  expect_error_at(with_tables("{type: PlotLine, ys: $nope}"), 4, 28, "with several tables write $table:column");
}

TEST(bind, UnknownTableAndUnknownColumnOfATable) {
  expect_error_at(
      with_tables("{type: PlotLine, ys: $salse:revenue}"),
      4,
      28,
      "unknown table 'salse' (did you mean 'sales'?) (tables: sales, costs)");
  expect_error_at(
      with_tables("{type: PlotLine, ys: $sales:revnue}"),
      4,
      28,
      "unknown column 'revnue' in table 'sales' (did you mean 'revenue'?) (columns: month, revenue)");
}

TEST(bind, HeaderWithColonResolvesInASingleTable) {
  const figure f = bind_source(
      "root:\n  type: Plot\n  children:\n    - {type: PlotLine, ys: $time:ms}\ndata: {\"time:ms\": [7, 8]}\n", reg());
  EXPECT_EQ(f.root.children[0].get<std::vector<double>>(2), (std::vector<double>{7, 8}));
}

TEST(bind, InterleavingAcrossTablesNeedsEqualRows) {
  const std::string source =
      "root:\n  type: Plot\n  children:\n    - {type: PlotHeatmap, values: [$a:x, $b:y], rows: 2}\n"
      "data:\n  a: {x: [1, 3]}\n  b: {y: [2, 4]}\n  c: {z: [5]}\n";
  const figure f = bind_source(source, reg());
  EXPECT_EQ(f.root.children[0].get<std::vector<double>>(0), (std::vector<double>{1, 2, 3, 4}));
  std::string uneven = source;
  uneven.replace(uneven.find("$b:y"), 4, "$c:z");
  expect_error_at(
      uneven, 4, 42, "'$c:z' has 1 rows but '$a:x' has 2; interleaved columns need the same number of rows");
}

// ── source ───────────────────────────────────────────────────────────────────

TEST(bind, SourceKeyTakesAnyValueAndIsNotInterpreted) {
  for (const char* source : {
           "source: Fetched with $query from the sales database.\n",
           "source:\n  prompt: |\n    Plot revenue by $month.\n  code: \"print($x)\"\n  language: python\n",
           "source: [step one, {step: two}]\n",
       }) {
    SCOPED_TRACE(source);
    // `$...` inside source is not a reference, even with no data.
    const figure f = bind_source(std::string(source) + "root: {type: Plot, title: Sales}\n", reg());
    EXPECT_EQ(f.root.get<std::string>(0), "Sales");
  }
}

} // namespace
} // namespace bdg::nymph
