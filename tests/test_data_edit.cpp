// MIT License © 2026 Binary Dice Games
/// @file test_data_edit.cpp
/// @brief spec/14_editor_workspace plan step 5: tables rewritten in their
///        form, added (with the single-to-named conversion), removed and
///        renamed, column renames, references kept in step.
#include <gtest/gtest.h>
#include <string>
#include "core/error.hpp"
#include "document/bind.hpp"
#include "edit/data_edit.hpp"
#include "elements/register_builtin.hpp"

namespace bdg::nymph {
namespace {

::testing::AssertionResult binds(const std::string& source) {
  try {
    (void)bind_source(source, builtin_registry());
    return ::testing::AssertionSuccess();
  } catch (const std::exception& e) {
    return ::testing::AssertionFailure() << e.what() << "\n" << source;
  }
}

const char* const kCsv =
    "# sales\n"
    "root:\n"
    "  type: Plot\n"
    "  children:\n"
    "    - {type: PlotLine, label: r, xs: $month, ys: \"$unit price\"}\n"
    "data: |\n"
    "  month, \"unit price\"\n"
    "  1, 3.5\n"
    "  2, 3.75\n"
    "source: {prompt: made by hand}\n";

const char* const kColumns =
    "root:\n"
    "  type: Plot\n"
    "  children:\n"
    "    - {type: PlotLine, label: r, xs: $x, ys: $y}\n"
    "data:\n"
    "  x: [1, 2]\n"
    "  y: [5, ~]\n";

const char* const kNamed =
    "root:\n"
    "  type: Plot\n"
    "  children:\n"
    "    - {type: PlotLine, label: m, xs: $monthly:month, ys: $monthly:2}\n"
    "    - {type: PlotBars, label: s, ys: $regions:share}\n"
    "data:\n"
    "  monthly: |\n"
    "    month, revenue\n"
    "    1, 92\n"
    "  regions:\n"
    "    name: [north, south]\n"
    "    share: [60, 40]\n";

table_text cells(std::vector<std::string> headers, std::vector<std::vector<std::string>> rows) {
  return table_text{std::move(headers), std::move(rows)};
}

// ── Text ─────────────────────────────────────────────────────────────────────

TEST(data_edit, CsvCellsAreQuotedOnlyWhenNeeded) {
  EXPECT_EQ(csv_cell("12"), "12");
  EXPECT_EQ(csv_cell("a, b"), "\"a, b\"");
  EXPECT_EQ(csv_cell("said \"hi\""), "\"said \"\"hi\"\"\"");
  EXPECT_EQ(csv_cell(" pad"), "\" pad\"");
  EXPECT_EQ(csv_cell("two\nlines"), "\"two\nlines\"");
  EXPECT_EQ(csv_cell(""), "");
}

TEST(data_edit, CsvTextRoundTripsThroughTheParser) {
  const table_text t = cells({"name", "note"}, {{"a, b", "said \"hi\""}, {"", " x"}, {"two\nlines", "3"}});
  EXPECT_EQ(table_from_csv(csv_text(t)), t);
}

TEST(data_edit, ReadsBothFormsAndNamedTables) {
  const std::vector<data_table> one = read_tables(kCsv);
  ASSERT_EQ(one.size(), 1u);
  EXPECT_EQ(one[0].name, "");
  EXPECT_EQ(one[0].form, table_form::csv);
  EXPECT_EQ(one[0].cells, cells({"month", "unit price"}, {{"1", "3.5"}, {"2", "3.75"}}));
  const std::vector<data_table> named = read_tables(kNamed);
  ASSERT_EQ(named.size(), 2u);
  EXPECT_EQ(named[1].name, "regions");
  EXPECT_EQ(named[1].form, table_form::columns);
  EXPECT_TRUE(read_tables("root: {type: Plot}\n").empty());
}

// ── Set ──────────────────────────────────────────────────────────────────────

TEST(data_edit, SetsACsvTableInPlace) {
  const std::string out =
      set_table(kCsv, "", cells({"month", "unit price"}, {{"1", "3.5"}, {"2", "4"}, {"3", "a, b"}}));
  EXPECT_EQ(
      out,
      "# sales\n"
      "root:\n"
      "  type: Plot\n"
      "  children:\n"
      "    - {type: PlotLine, label: r, xs: $month, ys: \"$unit price\"}\n"
      "data: |\n"
      "  month, unit price\n"
      "  1, 3.5\n"
      "  2, 4\n"
      "  3, \"a, b\"\n"
      "source: {prompt: made by hand}\n");
}

TEST(data_edit, SetsAColumnsTableInPlace) {
  const std::string out = set_table(kColumns, "", cells({"x", "y"}, {{"1", "5"}, {"2", ""}, {"3", "7"}}));
  EXPECT_EQ(
      out,
      "root:\n"
      "  type: Plot\n"
      "  children:\n"
      "    - {type: PlotLine, label: r, xs: $x, ys: $y}\n"
      "data:\n"
      "  x: [1, 2, 3]\n"
      "  y: [5, ~, 7]\n");
  EXPECT_TRUE(binds(out));
  EXPECT_EQ(read_tables(out)[0].cells.rows[1][1], "");
}

TEST(data_edit, SetsOneNamedTableAndLeavesTheOtherAsWritten) {
  const std::string out = set_table(kNamed, "monthly", cells({"month", "revenue"}, {{"1", "92"}, {"2", "88"}}));
  EXPECT_NE(out.find("  monthly: |\n    month, revenue\n    1, 92\n    2, 88\n  regions:\n    name: [north, south]\n"), std::string::npos)
      << out;
  EXPECT_TRUE(binds(out));
  const std::string flow = set_table("root: {type: Label}\ndata: {t: {a: [1]}, u: {b: [2]}}\n", "u", cells({"b"}, {{"5"}}));
  EXPECT_EQ(flow, "root: {type: Label}\ndata:\n  t:\n    a: [1]\n  u:\n    b: [5]\n");
}

TEST(data_edit, UnknownTablesAreErrors) {
  EXPECT_THROW(set_table(kNamed, "nope", cells({"a"}, {})), error);
  EXPECT_THROW(set_table(kCsv, "named", cells({"a"}, {})), error);
}

// ── Add ──────────────────────────────────────────────────────────────────────

TEST(data_edit, AddsTheFirstTable) {
  EXPECT_EQ(
      add_table("root: {type: Label}\n", "", cells({"x"}, {{""}, {""}})),
      "root: {type: Label}\ndata: |\n  x\n  \"\"\n  \"\"\n");
  // The empty rows survive the parser (a blank line would be skipped).
  EXPECT_EQ(read_tables(add_table("root: {type: Label}\n", "", cells({"x"}, {{""}, {""}})))[0].cells.rows.size(), 2u);
  EXPECT_EQ(
      add_table("root: {type: Label}\n", "t", cells({"x", "y"}, {{"1", "2"}})),
      "root: {type: Label}\ndata:\n  t: |\n    x, y\n    1, 2\n");
}

TEST(data_edit, AddsANamedTableAfterTheOthers) {
  const std::string out = add_table(kNamed, "costs", cells({"item", "cost"}, {{"rent", "10"}}));
  EXPECT_NE(out.find("    share: [60, 40]\n  costs: |\n    item, cost\n    rent, 10\n"), std::string::npos) << out;
  EXPECT_TRUE(binds(out));
  EXPECT_THROW(add_table(kNamed, "regions", cells({"a"}, {})), error);
  EXPECT_THROW(add_table(kNamed, "a:b", cells({"a"}, {})), error);
  EXPECT_THROW(add_table(kNamed, "", cells({"a"}, {})), error);
}

TEST(data_edit, ASecondTableNamesTheFirstAndItsReferences) {
  const std::string out = add_table(kCsv, "extra", cells({"k"}, {{"1"}}), "sales");
  EXPECT_NE(out.find("data:\n  sales: |\n    month, unit price\n    1, 3.5\n    2, 3.75\n  extra: |\n    k\n    1\n"), std::string::npos)
      << out;
  EXPECT_NE(out.find("xs: $sales:month, ys: $sales:unit price}"), std::string::npos) << out;
  EXPECT_NE(out.find("source: {prompt: made by hand}\n"), std::string::npos);
  EXPECT_TRUE(binds(out));
}

// ── Remove and rename ────────────────────────────────────────────────────────

TEST(data_edit, RemovesATableAndDataWithTheLast) {
  const std::string out = remove_table(kNamed, "monthly");
  EXPECT_NE(out.find("data:\n  regions:\n"), std::string::npos) << out;
  EXPECT_EQ(out.find("monthly: |"), std::string::npos);
  EXPECT_FALSE(binds(out)); // a reference to it is left: the model refuses this
  EXPECT_EQ(remove_table(kColumns, "").find("data"), std::string::npos);
}

TEST(data_edit, RenamesATableAndItsReferences) {
  const std::string out = rename_table(kNamed, "monthly", "months");
  EXPECT_NE(out.find("  months: |\n"), std::string::npos);
  EXPECT_NE(out.find("xs: $months:month, ys: $months:2"), std::string::npos) << out;
  EXPECT_NE(out.find("ys: $regions:share"), std::string::npos);
  EXPECT_TRUE(binds(out));
  EXPECT_THROW(rename_table(kNamed, "monthly", "regions"), error);
  EXPECT_THROW(rename_table(kCsv, "", "x"), error);
}

TEST(data_edit, RenamesAColumnAndItsReferencesButNotPositionalOnes) {
  const std::string named = rename_column(kNamed, "monthly", "month", "period");
  EXPECT_NE(named.find("    period, revenue\n"), std::string::npos) << named;
  EXPECT_NE(named.find("xs: $monthly:period, ys: $monthly:2"), std::string::npos) << named;
  EXPECT_TRUE(binds(named));
  const std::string one = rename_column(kCsv, "", "unit price", "price");
  EXPECT_NE(one.find("ys: $price"), std::string::npos) << one;
  EXPECT_TRUE(binds(one));
  const std::string columns = rename_column(kColumns, "", "y", "sales");
  EXPECT_NE(columns.find("  sales: [5, ~]\n"), std::string::npos) << columns;
  EXPECT_TRUE(binds(columns));
  EXPECT_THROW(rename_column(kColumns, "", "y", "x"), error);
  EXPECT_THROW(rename_column(kColumns, "", "nope", "z"), error);
}

TEST(data_edit, DollarDollarIsNotAReference) {
  const std::string source = "root: {type: Label, text: $$5 each}\ndata: {x: [1]}\n";
  EXPECT_EQ(rewrite_references(source, [](std::string_view) { return std::optional<std::string>("y"); }), source);
}

} // namespace
} // namespace bdg::nymph
