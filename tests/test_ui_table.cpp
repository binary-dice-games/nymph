// MIT License © 2026 Binary Dice Games
/// @file test_ui_table.cpp
/// @brief spec/09_ui_mockups PLAN step 7: Table, TableColumn, TableRow.
#include <gtest/gtest.h>
#include <string>
#include "core/error.hpp"
#include "nymph.hpp"
#include "ui_test_util.hpp"

namespace bdg::nymph {
namespace {

using test::render_ui;
using test::ui_render;

std::string table(const std::string& children, const std::string& fields = "") {
  return "image: {width: 640, height: 480, padding: 0}\n"
         "data:\n"
         "  files: {name: [a.cpp, b.hpp, c.md], size: [1, 2, 3]}\n"
         "root:\n"
         "  type: Window\n"
         "  children:\n"
         "    - type: Table\n" +
      fields +
      "      children:\n" + children;
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

bool says(const error& e, const std::string& part) {
  return std::string(e.what()).find(part) != std::string::npos;
}

TEST(ui_table, ColumnsOfDataGiveOneRowPerCell) {
  const ui_render r = render_ui(
      table(
          "        - {type: TableColumn, label: Name, cells: $files:name}\n"
          "        - {type: TableColumn, label: Size, cells: $files:size}\n"),
      true);
  std::vector<std::string> names;
  float last_y = -1;
  for (const edit_record& t : r.targets) {
    if (t.field == "cells" && r.paths.at(t.element) == "root/children/0/children/0") {
      names.push_back(t.text);
      EXPECT_GT(t.area.min.y, last_y);
      last_y = t.area.min.y;
    }
  }
  EXPECT_EQ(names, (std::vector<std::string>{"a.cpp", "b.hpp", "c.md"}));
}

TEST(ui_table, RowsOfWidgetsFollowTheData) {
  const ui_render r = render_ui(
      table(
          "        - {type: TableColumn, label: Name, cells: $files:name}\n"
          "        - {type: TableColumn, label: Done}\n"
          "        - type: TableRow\n"
          "          children: [{type: Text, text: x}, {type: Checkbox, label: \"##done\", value: true}]\n"
          "        - type: TableRow\n"
          "          children: [{type: Text, text: y}, {type: Checkbox, label: \"##done\"}]\n"),
      true);
  EXPECT_TRUE(r.state.at("root/children/0/children/2/children/1").flag);
  EXPECT_FALSE(r.state.at("root/children/0/children/3/children/1").flag);
  // The widget rows come after the data rows.
  float last_data = 0, first_widget = 1e9f;
  for (const edit_record& t : r.targets) {
    const std::string& path = r.paths.at(t.element);
    if (t.field == "cells") {
      last_data = std::max(last_data, t.area.max.y);
    } else if (path.rfind("root/children/0/children/2/", 0) == 0) {
      first_widget = std::min(first_widget, t.area.min.y);
    }
  }
  EXPECT_GE(first_widget, last_data);
}

TEST(ui_table, WithoutColumnsTheWidestRowDecides) {
  EXPECT_NO_THROW(render_ui(table(
      "        - {type: TableRow, children: [{type: Text, text: a}, {type: Text, text: b}, {type: Text, text: c}]}\n"
      "        - {type: TableRow, children: [{type: Text, text: d}]}\n")));
}

TEST(ui_table, MismatchedCellsAreErrors) {
  const error lengths = source_error(table(
      "        - {type: TableColumn, cells: [a, b, c]}\n"
      "        - {type: TableColumn, cells: [d, e]}\n"));
  EXPECT_EQ(lengths.line(), 10);
  EXPECT_TRUE(says(lengths, "TableColumn.cells has 2 cells, but an earlier column has 3")) << lengths.what();
  const error extra = source_error(table(
      "        - {type: TableColumn, label: A}\n"
      "        - {type: TableRow, children: [{type: Text, text: a}, {type: Text, text: b}]}\n"));
  EXPECT_TRUE(says(extra, "TableRow has 2 cells, but the Table has 1 columns")) << extra.what();
}

TEST(ui_table, FlagCombinationsImGuiRefusesAreErrors) {
  EXPECT_NO_THROW(check_source(table("        - {type: TableColumn, label: A}\n", "      flags: Borders|RowBg\n")));
  const error sizing = source_error(
      table("        - {type: TableColumn, label: A}\n", "      flags: SizingFixedFit|SizingStretchSame\n"));
  EXPECT_TRUE(says(sizing, "Table.flags may have only one Sizing policy")) << sizing.what();
  const error width = source_error(table("        - {type: TableColumn, label: A, flags: WidthStretch|WidthFixed}\n"));
  EXPECT_TRUE(says(width, "may not have both WidthStretch and WidthFixed")) << width.what();
}

TEST(ui_table, AScrollingTableKeepsItsHeight) {
  std::string rows;
  for (int i = 0; i < 40; ++i) {
    rows += "        - {type: TableRow, children: [{type: Text, text: row}]}\n";
  }
  const ui_render r = render_ui(table(rows, "      flags: ScrollY\n      height: 120\n"), true);
  for (const edit_record& t : r.targets) {
    if (t.element->cls->name() == "Table") {
      EXPECT_EQ(t.area.height(), 120);
    }
  }
}

TEST(ui_table, RepeatedLabelsInRowsKeepTheirIds) {
  std::string rows;
  for (int i = 0; i < 5; ++i) {
    rows += "        - {type: TableRow, children: [{type: Checkbox, label: \"##sel\"}, {type: Button, label: Open}]}\n";
  }
  EXPECT_NO_THROW(render_ui(table("        - {type: TableColumn, label: A}\n        - {type: TableColumn, label: A}\n" + rows)));
}

} // namespace
} // namespace bdg::nymph
