// MIT License © 2026 Binary Dice Games
/// @file test_schema.cpp
/// @brief Plan step 7: element classes, the registry, named values, and
///        YAML-to-field conversion by field type.
#include <gtest/gtest.h>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include "core/error.hpp"
#include "document/yaml_doc.hpp"
#include "render/draw_context.hpp"
#include "schema/convert.hpp"
#include "schema/element_class.hpp"
#include "schema/name_table.hpp"
#include "schema/registry.hpp"

namespace bdg::nymph {
namespace {

// ── Fixtures ─────────────────────────────────────────────────────────────────

const name_table& marker_names() {
  static const name_table t{{"None", -2}, {"Auto", -1}, {"Circle", 0}, {"Square", 1}};
  return t;
}

const name_table& flag_names() {
  static const name_table t{{"None", 0}, {"NoTitle", 1}, {"NoLegend", 2}, {"NoMenus", 4}, {"NoChrome", 3}};
  return t;
}

/// Every field type, for conversion tests.
std::vector<field_desc> all_fields() {
  return {
      bool_field("visible", true, "Shown."),
      int_field("bins", -1, "Bin count.", value_range{-4, 1000}),
      number_field("weight", 1.0, "Line weight.", value_range{0, 32}),
      text_field("title", "", "Title."),
      color_field("color", "Colour."),
      enum_field("marker", -1, marker_names(), "Marker."),
      flags_field("flags", 0, flag_names(), "Flags."),
      numbers_field("ys", "Values."),
      integers_field("indices", "Indices."),
  };
}

/// Records the order in which nodes are drawn, by their `title` field.
class recording_class : public element_class {
 public:
  recording_class(std::string name, std::string slot, std::vector<std::string> accepts, std::vector<std::string>* log)
      : element_class(std::move(name), std::move(slot), std::move(accepts), all_fields(), "A test element."),
        log_(log) {}

  void draw(const node& n, draw_context& dc) const override {
    log_->push_back(n.get<std::string>(3));
    dc.draw_children(n);
  }

 private:
  std::vector<std::string>* log_;
};

/// Converts `yaml_value` (the text after "v: ") for the field named `field`.
value convert(const std::string& yaml_value, const std::string& field) {
  const yaml_node doc = parse_yaml("v: " + yaml_value + "\n");
  for (const field_desc& f : all_fields()) {
    if (f.name == field) {
      return convert_value(*doc.find("v"), f, "Fake." + field);
    }
  }
  ADD_FAILURE() << "no field " << field;
  return false;
}

/// The error converting `yaml_value` for `field`.
error convert_error(const std::string& yaml_value, const std::string& field) {
  try {
    convert(yaml_value, field);
  } catch (const error& e) {
    return e;
  }
  ADD_FAILURE() << "no error for " << field << ": " << yaml_value;
  return error(0, 0, "");
}

bool contains(const error& e, const std::string& part) {
  return std::string(e.what()).find(part) != std::string::npos;
}

// ── Registry and classes ─────────────────────────────────────────────────────

TEST(schema, RegistryFindsClassesAndRejectsDuplicates) {
  std::vector<std::string> log;
  registry reg;
  reg.add(std::make_unique<recording_class>("Plot", "figure", std::vector<std::string>{"item"}, &log));
  reg.add(std::make_unique<recording_class>("Line", "item", std::vector<std::string>{}, &log));
  EXPECT_NE(reg.find("Plot"), nullptr);
  EXPECT_EQ(reg.find("Nope"), nullptr);
  EXPECT_EQ(reg.classes().size(), 2u);
  EXPECT_EQ(reg.names_in_slot("item"), "Line");
  EXPECT_THROW(
      reg.add(std::make_unique<recording_class>("Plot", "figure", std::vector<std::string>{}, &log)), std::logic_error);
}

TEST(schema, DuplicateFieldNamesAreRejected) {
  class twice : public element_class {
   public:
    twice() : element_class("Twice", "figure", {}, {bool_field("a", false, ""), number_field("a", 0, "")}, "") {}
    void draw(const node&, draw_context&) const override {}
  };
  registry reg;
  EXPECT_THROW(reg.add(std::make_unique<twice>()), std::logic_error);
}

TEST(schema, ClassesKnowSlotsFieldsAndDefaults) {
  std::vector<std::string> log;
  recording_class plot("Plot", "figure", {"item"}, &log);
  EXPECT_TRUE(plot.accepts_slot("item"));
  EXPECT_FALSE(plot.accepts_slot("figure"));
  EXPECT_EQ(plot.field_index("weight"), 2);
  EXPECT_EQ(plot.field_index("nope"), -1);
  const node n = plot.make_node();
  EXPECT_EQ(n.cls, &plot);
  ASSERT_EQ(n.values.size(), plot.fields().size());
  EXPECT_EQ(n.get<bool>(0), true);
  EXPECT_EQ(n.get<int64_t>(1), -1);
  EXPECT_EQ(n.get<double>(2), 1.0);
  EXPECT_TRUE(n.get<std::vector<double>>(7).empty());
}

TEST(schema, DrawChildrenDrawsInOrder) {
  std::vector<std::string> log;
  recording_class plot("Plot", "figure", {"item"}, &log);
  recording_class line("Line", "item", {}, &log);
  node root = plot.make_node();
  root.values[3] = std::string("root");
  for (const char* title : {"a", "b", "c"}) {
    node child = line.make_node();
    child.values[3] = std::string(title);
    root.children.push_back(child);
  }
  image_options options;
  draw_context dc(options);
  plot.draw(root, dc);
  EXPECT_EQ(log, (std::vector<std::string>{"root", "a", "b", "c"}));
}

// ── Named values ─────────────────────────────────────────────────────────────

TEST(schema, NameTablesParseAndFormat) {
  std::string unknown;
  EXPECT_EQ(flag_names().parse_flags("NoTitle|NoLegend", unknown), 3);
  EXPECT_EQ(flag_names().parse_flags(" NoTitle | NoMenus ", unknown), 5);
  EXPECT_FALSE(flag_names().parse_flags("NoTitle|Nope", unknown).has_value());
  EXPECT_EQ(unknown, "Nope");
  EXPECT_FALSE(flag_names().parse_flags("NoTitle|", unknown).has_value());
  EXPECT_EQ(unknown, "");
  EXPECT_EQ(flag_names().format_flags(0), "None");
  EXPECT_EQ(flag_names().format_flags(6), "NoLegend|NoMenus");
  EXPECT_EQ(flag_names().format_flags(3), "NoTitle|NoLegend"); // single bits first
  EXPECT_EQ(flag_names().format_flags(9), "NoTitle|8");
  EXPECT_EQ(marker_names().format_enum(0), "Circle");
  EXPECT_EQ(marker_names().format_enum(7), "7");
  EXPECT_EQ(marker_names().names(), "None, Auto, Circle, Square");
}

// ── Conversion: valid values ─────────────────────────────────────────────────

TEST(schema, ConvertsEachFieldType) {
  EXPECT_EQ(std::get<bool>(convert("false", "visible")), false);
  EXPECT_EQ(std::get<bool>(convert("yes", "visible")), true);
  EXPECT_EQ(std::get<bool>(convert("Off", "visible")), false);
  EXPECT_EQ(std::get<int64_t>(convert("42", "bins")), 42);
  EXPECT_EQ(std::get<int64_t>(convert("+3", "bins")), 3);
  EXPECT_EQ(std::get<double>(convert("2.5", "weight")), 2.5);
  EXPECT_EQ(std::get<double>(convert("3", "weight")), 3.0);
  EXPECT_EQ(std::get<double>(convert("1e1", "weight")), 10.0);
  EXPECT_EQ(std::get<std::string>(convert("\"#11223344\"", "color")), "#11223344");
  EXPECT_EQ(std::get<std::string>(convert("'#abcDEF'", "color")), "#abcDEF");
  EXPECT_EQ(std::get<std::string>(convert("\"\"", "color")), ""); // automatic
  EXPECT_EQ(std::get<int64_t>(convert("Square", "marker")), 1);
  EXPECT_EQ(std::get<int64_t>(convert("-2", "marker")), -2);
  EXPECT_EQ(std::get<std::vector<double>>(convert("[1, 2.5, -3]", "ys")), (std::vector<double>{1, 2.5, -3}));
  EXPECT_EQ(std::get<std::vector<int64_t>>(convert("[0, 1, 2]", "indices")), (std::vector<int64_t>{0, 1, 2}));
}

TEST(schema, TextStaysTextAndDollarDollarIsADollar) {
  EXPECT_EQ(std::get<std::string>(convert("2025", "title")), "2025");
  EXPECT_EQ(std::get<std::string>(convert("true", "title")), "true");
  EXPECT_EQ(std::get<std::string>(convert("\"quoted\"", "title")), "quoted");
  EXPECT_EQ(std::get<std::string>(convert("$$5 per unit", "title")), "$5 per unit");
  EXPECT_EQ(std::get<std::string>(convert("", "title")), "");
}

TEST(schema, FlagNamesAndNumbersGiveTheSameValue) {
  EXPECT_EQ(std::get<int64_t>(convert("NoTitle|NoLegend", "flags")), 3);
  EXPECT_EQ(std::get<int64_t>(convert("3", "flags")), 3);
  EXPECT_EQ(std::get<int64_t>(convert("NoChrome", "flags")), 3);
}

TEST(schema, NullItemsInNumberListsAreGaps) {
  const auto ys = std::get<std::vector<double>>(convert("[1, ~, null, 4]", "ys"));
  ASSERT_EQ(ys.size(), 4u);
  EXPECT_TRUE(std::isnan(ys[1]));
  EXPECT_TRUE(std::isnan(ys[2]));
  EXPECT_EQ(ys[3], 4.0);
}

// ── Conversion: errors, at the value ─────────────────────────────────────────

TEST(schema, WrongKindsAreErrorsAtTheValue) {
  const error list_for_number = convert_error("[1, 2]", "weight");
  EXPECT_EQ(list_for_number.line(), 1);
  EXPECT_EQ(list_for_number.column(), 4);
  EXPECT_TRUE(contains(list_for_number, "Fake.weight must be a single value (number), not a list"));
  EXPECT_TRUE(contains(convert_error("{a: 1}", "title"), "not a mapping"));
  EXPECT_TRUE(contains(convert_error("5", "ys"), "must be a list of numbers"));
  EXPECT_TRUE(contains(convert_error("maybe", "visible"), "must be true or false"));
}

TEST(schema, QuotedNumbersAndBooleansAreErrorsWithAHint) {
  EXPECT_TRUE(contains(convert_error("\"2.5\"", "weight"), "must be a number, not '2.5' (remove the quotes)"));
  EXPECT_TRUE(contains(convert_error("'42'", "bins"), "(remove the quotes)"));
  EXPECT_TRUE(contains(convert_error("\"true\"", "visible"), "(remove the quotes)"));
}

TEST(schema, NumbersMustFitTheirType) {
  EXPECT_TRUE(contains(convert_error("2.5", "bins"), "must be a whole number"));
  EXPECT_TRUE(contains(convert_error("abc", "weight"), "must be a number, not 'abc'"));
  EXPECT_TRUE(contains(convert_error("3000000000", "bins"), "out of range"));
  EXPECT_TRUE(contains(convert_error("nan", "weight"), "must be a number"));
}

TEST(schema, OutOfRangeValuesAreErrors) {
  EXPECT_TRUE(contains(convert_error("33", "weight"), "Fake.weight must be between 0 and 32"));
  EXPECT_TRUE(contains(convert_error("-5", "bins"), "must be between -4 and 1000"));
  EXPECT_NO_THROW(convert("[1, 40]", "ys")); // ys has no limits
}

TEST(schema, UnknownNamesListTheKnownOnes) {
  const error e = convert_error("Triangle", "marker");
  EXPECT_EQ(e.column(), 4);
  EXPECT_TRUE(contains(e, "Fake.marker: unknown value 'Triangle' (one of: None, Auto, Circle, Square)")) << e.what();
  EXPECT_TRUE(contains(convert_error("7", "marker"), "has no value 7"));
  const error f = convert_error("NoTitle|NoLegnd", "flags");
  EXPECT_TRUE(contains(f, "unknown value 'NoLegnd'")) << f.what();
}

TEST(schema, ColourErrors) {
  // `color: #112233` is a YAML comment: the value is empty.
  const error comment = convert_error("#112233", "color");
  EXPECT_TRUE(contains(comment, "an unquoted '#' starts a YAML comment")) << comment.what();
  EXPECT_TRUE(contains(convert_error("\"#12345\"", "color"), "is not a colour"));
  EXPECT_TRUE(contains(convert_error("red", "color"), "is not a colour"));
  EXPECT_TRUE(contains(convert_error("\"#GG0000\"", "color"), "is not a colour"));
}

TEST(schema, ListItemErrorsPointAtTheItem) {
  const error e = convert_error("[1, 2, x]", "ys");
  EXPECT_EQ(e.line(), 1);
  EXPECT_EQ(e.column(), 11);
  EXPECT_TRUE(contains(convert_error("[1, [2]]", "ys"), "items must be numbers"));
  EXPECT_TRUE(contains(convert_error("[0, 1.5]", "indices"), "must be a whole number"));
}

} // namespace
} // namespace bdg::nymph
