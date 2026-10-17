// MIT License © 2026 Binary Dice Games
/// @file test_table.cpp
/// @brief Plan step 6: both forms of `data` become the same table, and every
///        data error points at its cell in the YAML file.
#include <gtest/gtest.h>
#include <cmath>
#include <string>
#include "core/error.hpp"
#include "core/limits.hpp"
#include "document/table.hpp"
#include "document/yaml_doc.hpp"

namespace bdg::nymph {
namespace {

/// Runs `f`, expecting a nymph::error; returns it.
template <typename F>
error expect_error(F f) {
  try {
    f();
  } catch (const error& e) {
    return e;
  }
  ADD_FAILURE() << "no error";
  return error(0, 0, "");
}

/// The table of a whole source's `data` key.
table data_of(const std::string& source) {
  const yaml_node doc = parse_yaml(source);
  const yaml_node* data = doc.find("data");
  EXPECT_NE(data, nullptr);
  return parse_data(*data);
}

std::vector<std::string> texts(const table& t, int column) {
  std::vector<std::string> out;
  for (const cell& c : t.columns[static_cast<size_t>(column)]) {
    out.push_back(c.text);
  }
  return out;
}

// ── CSV rules ────────────────────────────────────────────────────────────────

TEST(table, CsvBasics) {
  const table t = parse_csv("month, revenue, cost\n1, 12, 9\n2, 15, 9.5\n");
  ASSERT_EQ(t.headers.size(), 3u);
  EXPECT_EQ(t.headers[1].text, "revenue");
  EXPECT_EQ(t.row_count(), 2u);
  EXPECT_EQ(t.numbers(2), (std::vector<double>{9.0, 9.5}));
}

TEST(table, CsvQuotingFollowsRfc4180) {
  const table t = parse_csv(
      "name,note\n"
      "\"Smith, J.\",\"said \"\"hi\"\"\"\n"
      "  \"padded\"  , \"two\nlines\"\n"
      "plain ,  trimmed  \n");
  ASSERT_EQ(t.row_count(), 3u);
  EXPECT_EQ(texts(t, 0), (std::vector<std::string>{"Smith, J.", "padded", "plain"}));
  EXPECT_EQ(texts(t, 1), (std::vector<std::string>{"said \"hi\"", "two\nlines", "trimmed"}));
  // The row after a quoted line break starts on the right line.
  EXPECT_EQ(t.row_lines, (std::vector<int>{2, 3, 5}));
}

TEST(table, BlankLinesCrlfAndBomAreHandled) {
  const table t = parse_csv(
      "\xEF\xBB\xBF"
      "a,b\r\n\r\n1,2\r\n   \r\n3,4\r\n");
  ASSERT_EQ(t.headers.size(), 2u);
  EXPECT_EQ(t.headers[0].text, "a");
  EXPECT_EQ(t.headers[1].text, "b");
  EXPECT_EQ(t.numbers(1), (std::vector<double>{2.0, 4.0}));
}

TEST(table, EmptyCsvIsAnEmptyTable) {
  EXPECT_EQ(parse_csv("").headers.size(), 0u);
  EXPECT_EQ(parse_csv("\n  \n").headers.size(), 0u);
  EXPECT_EQ(parse_csv("only,header\n").row_count(), 0u);
}

TEST(table, NumbersAcceptSignsExponentsAndGaps) {
  const table t = parse_csv("v\n+1.5\n-2e3\n\n0.25\n");
  // The blank line is skipped, not a row: three rows.
  EXPECT_EQ(t.numbers(0), (std::vector<double>{1.5, -2000.0, 0.25}));
  const table gaps = parse_csv("a,b\n1,\n,2\n");
  const std::vector<double> a = gaps.numbers(0);
  EXPECT_EQ(a[0], 1.0);
  EXPECT_TRUE(std::isnan(a[1]));
}

TEST(table, ColumnsByNameOrPosition) {
  const table t = parse_csv("x,unit price,3\n1,2,3\n");
  EXPECT_EQ(t.find_column("x"), 0);
  EXPECT_EQ(t.find_column("unit price"), 1);
  EXPECT_EQ(t.find_column("2"), 1);
  EXPECT_EQ(t.find_column("3"), 2); // a header named "3" wins over position 3
  EXPECT_EQ(t.find_column("4"), -1);
  EXPECT_EQ(t.find_column("0"), -1);
  EXPECT_EQ(t.find_column("nope"), -1);
  EXPECT_EQ(t.header_list(), "x, unit price, 3");
}

// ── CSV errors, positioned in the YAML file ──────────────────────────────────

const char* const kHeader =
    "root: {type: Plot}\n" // 1
    "data: |\n"; // 2: CSV text starts on line 3, column 3

TEST(table, NonNumberIsReportedAtItsCell) {
  const table t = data_of(std::string(kHeader) + "  a, b\n  1, 2\n  3, oops\n");
  const error e = expect_error([&] { t.numbers(1); });
  EXPECT_EQ(e.line(), 5);
  EXPECT_EQ(e.column(), 6);
  EXPECT_NE(std::string(e.what()).find("column 'b', row 2: 'oops' is not a number"), std::string::npos) << e.what();
  // A text column nothing uses as numbers is never checked.
  EXPECT_NO_THROW(data_of(std::string(kHeader) + "  name, v\n  x, 1\n"));
}

TEST(table, RaggedRowIsReportedAtItsLine) {
  const error e = expect_error([] { data_of(std::string(kHeader) + "  a, b\n  1, 2\n  3\n"); });
  EXPECT_EQ(e.line(), 5);
  EXPECT_EQ(e.column(), 3);
  EXPECT_NE(std::string(e.what()).find("1 cells, the header has 2"), std::string::npos) << e.what();
}

TEST(table, HeaderErrors) {
  const error empty = expect_error([] { data_of(std::string(kHeader) + "  a,,c\n"); });
  EXPECT_EQ(empty.line(), 3);
  EXPECT_EQ(empty.column(), 5);
  const error dup = expect_error([] { data_of(std::string(kHeader) + "  a, b, a\n"); });
  EXPECT_EQ(dup.line(), 3);
  EXPECT_EQ(dup.column(), 9);
  EXPECT_NE(std::string(dup.what()).find("duplicate CSV header 'a'"), std::string::npos);
}

TEST(table, QuoteErrors) {
  const error open = expect_error([] { parse_csv("a\n\"never closed\n", 10, 4); });
  EXPECT_EQ(open.line(), 11);
  EXPECT_EQ(open.column(), 4);
  const error trailing = expect_error([] { parse_csv("a\n\"x\" y\n"); });
  EXPECT_EQ(trailing.line(), 2);
  EXPECT_EQ(trailing.column(), 5);
}

TEST(table, PositionsCountCharacters) {
  const table t = parse_csv("\xc3\xa9t\xc3\xa9, v\n1, 2\n"); // "été, v"
  EXPECT_EQ(t.headers[1].column, 6);
}

TEST(table, RowLimit) {
  std::string csv = "v\n";
  for (size_t i = 0; i <= kMaxDataRows; ++i) {
    csv += "1\n";
  }
  const error e = expect_error([&] { parse_csv(csv); });
  EXPECT_NE(std::string(e.what()).find("more than 1000000 rows"), std::string::npos);
}

// ── The mapping form ─────────────────────────────────────────────────────────

TEST(table, MappingFormMatchesCsvForm) {
  const table csv = data_of(std::string(kHeader) + "  month, revenue\n  1, 12\n  2, 15.5\n");
  const table map = data_of("root: {type: Plot}\ndata:\n  month: [1, 2]\n  revenue: [12, 15.5]\n");
  ASSERT_EQ(map.headers.size(), csv.headers.size());
  for (size_t c = 0; c < csv.headers.size(); ++c) {
    EXPECT_EQ(map.headers[c].text, csv.headers[c].text);
    EXPECT_EQ(texts(map, static_cast<int>(c)), texts(csv, static_cast<int>(c)));
    EXPECT_EQ(map.numbers(static_cast<int>(c)), csv.numbers(static_cast<int>(c)));
  }
  EXPECT_EQ(map.row_count(), 2u);
  EXPECT_EQ(map.find_column("2"), 1);
}

TEST(table, MappingFormNullsAreGapsQuotedNullIsText) {
  const table t = data_of("data: {v: [1, ~, null, '', 4], s: [a, 'null', b, c, d]}\n");
  const std::vector<double> v = t.numbers(0);
  EXPECT_EQ(v[0], 1.0);
  EXPECT_TRUE(std::isnan(v[1]));
  EXPECT_TRUE(std::isnan(v[2]));
  EXPECT_TRUE(std::isnan(v[3]));
  EXPECT_EQ(texts(t, 1)[1], "null");
}

TEST(table, MappingFormErrors) {
  const error unequal = expect_error([] { data_of("data:\n  a: [1, 2, 3]\n  b: [1, 2]\n"); });
  EXPECT_EQ(unequal.line(), 3);
  EXPECT_EQ(unequal.column(), 3);
  EXPECT_NE(std::string(unequal.what()).find("'b' has 2 values, column 'a' has 3"), std::string::npos);

  const error nested = expect_error([] { data_of("data:\n  a: [1, [2, 3]]\n"); });
  EXPECT_EQ(nested.line(), 2);
  EXPECT_EQ(nested.column(), 10);

  const error not_list = expect_error([] { data_of("data:\n  a: 5\n"); });
  EXPECT_EQ(not_list.line(), 2);
  EXPECT_EQ(not_list.column(), 6);

  const error bad = expect_error([] { data_of("data: {a: [1, x]}\n").numbers(0); });
  EXPECT_EQ(bad.line(), 1);
  EXPECT_EQ(bad.column(), 15);
}

// ── Form selection ───────────────────────────────────────────────────────────

TEST(table, DataFormErrors) {
  const error seq = expect_error([] { data_of("data: [1, 2]\n"); });
  EXPECT_NE(std::string(seq.what()).find("CSV text"), std::string::npos);
  const error folded = expect_error([] { data_of("data: >\n  a,b\n  1,2\n"); });
  EXPECT_NE(std::string(folded.what()).find("literal block"), std::string::npos);
}

TEST(table, QuotedSingleLineCsvWorks) {
  const table t = data_of("data: \"a,b\\n1,2\\n\"\n");
  EXPECT_EQ(t.numbers(1), (std::vector<double>{2.0}));
}

} // namespace
} // namespace bdg::nymph
