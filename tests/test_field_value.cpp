// MIT License © 2026 Binary Dice Games
/// @file test_field_value.cpp
/// @brief spec/08_view_editor plan step 2: typed values become YAML that
///        reads back unchanged, and an invalid edited source is detected.
#include <gtest/gtest.h>
#include <string>
#include "document/bind.hpp"
#include "edit/field_value.hpp"
#include "edit/source_edit.hpp"
#include "elements/register_builtin.hpp"

namespace bdg::nymph {
namespace {

/// The title of a plot after `title: <encoded>` is read back.
std::string round_trip_title(const std::string& text) {
  const std::string src = set_field("root: {type: Plot, children: []}\n", "root", "title", encode_text(text));
  const figure f = bind_source(src, builtin_registry());
  const size_t title = static_cast<size_t>(f.root.cls->field_index("title"));
  return f.root.get<std::string>(title);
}

TEST(field_value, TextReadsBackAsTheSameText) {
  for (const char* text : {"Revenue", "2025", "a: b", "a:b", "x #y", "#hash", "- dash", "[list]", "{map}", "a, b",
                           "yes", "null", "~", " lead", "trail ", "it's", "say \"hi\"", "back\\slash", "tab\there",
                           "two\nlines", "é€ ünï", "100%", "@at", "*star", "&amp", "!bang", "|pipe", ">fold", "50 %"}) {
    EXPECT_EQ(round_trip_title(text), text) << text;
  }
}

TEST(field_value, SimpleTextStaysPlain) {
  EXPECT_EQ(encode_text("Revenue vs cost"), "Revenue vs cost");
  EXPECT_EQ(encode_text("2025"), "2025");
  EXPECT_EQ(encode_text("a: b"), "\"a: b\"");
  EXPECT_EQ(encode_text(""), "\"\"");
  EXPECT_EQ(encode_text("a\nb"), "\"a\\nb\"");
}

TEST(field_value, NumbersAndWholeNumbers) {
  EXPECT_EQ(encode_number(5.0), "5");
  EXPECT_EQ(encode_number(0.1), "0.1");
  EXPECT_EQ(encode_number(1.0 / 3.0), "0.3333333333333333");
  EXPECT_EQ(encode_number(1e20), "1e+20");
  EXPECT_EQ(encode_number(-2.5), "-2.5");
  EXPECT_EQ(encode_number(std::nan("")), "0");
  EXPECT_EQ(encode_int(-12), "-12");
  EXPECT_EQ(encode_bool(true), "true");
  EXPECT_EQ(encode_bool(false), "false");
}

TEST(field_value, ColoursAreQuotedAndChecked) {
  EXPECT_EQ(encode_color("#ff8800"), "\"#ff8800\"");
  EXPECT_EQ(encode_color("#FF880080"), "\"#FF880080\"");
  EXPECT_FALSE(encode_color("ff8800"));
  EXPECT_FALSE(encode_color("#ff880"));
  EXPECT_FALSE(encode_color("#gg8800"));
  EXPECT_FALSE(encode_color(""));
}

TEST(field_value, EnumsAndFlagsUseTheirNames) {
  const element_class* plot = builtin_registry().find("Plot");
  ASSERT_NE(plot, nullptr);
  const field_desc& location = plot->fields()[static_cast<size_t>(plot->field_index("legend_location"))];
  ASSERT_NE(location.names, nullptr);
  const std::optional<int64_t> north_west = location.names->find("NorthWest");
  ASSERT_TRUE(north_west);
  EXPECT_EQ(encode_enum(location, *north_west), "NorthWest");
  const field_desc& flags = plot->fields()[static_cast<size_t>(plot->field_index("legend_flags"))];
  ASSERT_NE(flags.names, nullptr);
  const std::string two = encode_flags(flags, *flags.names->find("Outside") | *flags.names->find("Horizontal"));
  EXPECT_EQ(two, "\"Outside|Horizontal\"");
  EXPECT_EQ(check_source("root: {type: Plot, legend_flags: " + two + "}\n"), std::nullopt);
}

TEST(field_value, EveryEditedValueIsAcceptedByTheLoader) {
  const std::string base = "root:\n  type: Plot\n  children: []\n";
  EXPECT_EQ(check_source(set_field(base, "root", "x_label", encode_text("time (s)"))), std::nullopt);
  EXPECT_EQ(check_source(set_field(base, "root", "text_color", *encode_color("#2471a3"))), std::nullopt);
  EXPECT_EQ(check_source(set_field(base, "root", "font_size", encode_number(18))), std::nullopt);
}

TEST(field_value, AnInvalidSourceIsReportedWithItsPosition) {
  const std::string base = "root:\n  type: Plot\n  children: []\n";
  const auto too_big = check_source(set_field(base, "root", "font_size", encode_number(500)));
  ASSERT_TRUE(too_big);
  EXPECT_EQ(too_big->line, 4);
  EXPECT_NE(too_big->message.find("font_size"), std::string::npos);
  const auto bad_color = check_source(set_field(base, "root", "text_color", encode_text("red")));
  ASSERT_TRUE(bad_color);
  const auto bad_enum = check_source(set_field(base, "root", "legend_location", encode_text("Middle")));
  ASSERT_TRUE(bad_enum);
  const auto bad_font = check_source(set_field(base, "root", "font", encode_text("NoSuchFont.ttf")));
  ASSERT_TRUE(bad_font);
  EXPECT_EQ(check_source(base), std::nullopt);
}

} // namespace
} // namespace bdg::nymph
