// MIT License © 2026 Binary Dice Games
/// @file test_charconv.cpp
/// @brief from_chars_double() and its fallback parse like std::from_chars
///        (spec/07_release_pipeline/PLAN.md step 2).
#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <string>
#include <system_error>
#include "core/charconv.hpp"

using namespace bdg::nymph;

namespace {

struct parsed {
  double value = -12345.0;
  size_t length = 0;
  std::errc ec = std::errc{};
};

parsed parse_fallback(const std::string& text) {
  parsed p;
  const auto r = from_chars_double_fallback(text.data(), text.data() + text.size(), p.value);
  p.length = static_cast<size_t>(r.ptr - text.data());
  p.ec = r.ec;
  return p;
}

parsed parse_selected(const std::string& text) {
  parsed p;
  const auto r = from_chars_double(text.data(), text.data() + text.size(), p.value);
  p.length = static_cast<size_t>(r.ptr - text.data());
  p.ec = r.ec;
  return p;
}

} // namespace

// ── Numbers ─────────────────────────────────────────────────────────────────

TEST(charconv, ParsesPlainNumbersExactly) {
  for (const char* text :
       {"0",
        "1",
        "-1",
        "1.5",
        "-0.25",
        ".5",
        "5.",
        "1e3",
        "1E-3",
        "2.5e+10",
        "123456789012345678",
        "0.1",
        "3.141592653589793",
        "1.7976931348623157e308",
        "2.2250738585072014e-308"}) {
    const parsed p = parse_fallback(text);
    EXPECT_EQ(p.ec, std::errc{}) << text;
    EXPECT_EQ(p.length, std::strlen(text)) << text;
    EXPECT_EQ(p.value, std::strtod(text, nullptr)) << text;
  }
}

TEST(charconv, StopsAtTheFirstCharacterThatIsNotPartOfTheNumber) {
  EXPECT_EQ(parse_fallback("12abc").length, 2u);
  EXPECT_EQ(parse_fallback("1e").length, 1u);
  EXPECT_EQ(parse_fallback("1e+").length, 1u);
  EXPECT_EQ(parse_fallback("0x10").length, 1u);
  EXPECT_EQ(parse_fallback("1.5.2").length, 3u);
  EXPECT_EQ(parse_fallback("2 ").length, 1u);
}

TEST(charconv, RefusesTextWithoutANumber) {
  for (const char* text : {"", "+1", " 1", "-", ".", "e5", "abc", "-.e1"}) {
    const parsed p = parse_fallback(text);
    EXPECT_EQ(p.ec, std::errc::invalid_argument) << text;
    EXPECT_EQ(p.length, 0u) << text;
    EXPECT_EQ(p.value, -12345.0) << "value untouched for " << text;
  }
}

TEST(charconv, ParsesInfinityAndNanInAnyCase) {
  EXPECT_TRUE(std::isinf(parse_fallback("inf").value));
  EXPECT_EQ(parse_fallback("Infinity").length, 8u);
  EXPECT_LT(parse_fallback("-INF").value, 0.0);
  EXPECT_TRUE(std::isnan(parse_fallback("NaN").value));
  EXPECT_EQ(parse_fallback("nan(123)").length, 8u);
  EXPECT_EQ(parse_fallback("nan(").length, 3u);
}

TEST(charconv, ValuesOutsideDoubleAreOutOfRange) {
  EXPECT_EQ(parse_fallback("1e400").ec, std::errc::result_out_of_range);
  EXPECT_EQ(parse_fallback("-1e400").ec, std::errc::result_out_of_range);
  EXPECT_EQ(parse_fallback("1e-400").ec, std::errc::result_out_of_range);
  EXPECT_EQ(parse_fallback("0e-400").ec, std::errc{});
}

TEST(charconv, SelectedImplementationAgreesWithTheFallback) {
  for (const char* text : {"0", "-2.5e3", "12abc", "abc", "inf", "1e400", ".5"}) {
    const parsed a = parse_selected(text);
    const parsed b = parse_fallback(text);
    EXPECT_EQ(a.ec, b.ec) << text;
    EXPECT_EQ(a.length, b.length) << text;
    if (a.ec == std::errc{}) {
      EXPECT_EQ(a.value, b.value) << text;
    }
  }
}
