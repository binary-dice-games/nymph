// MIT License © 2026 Binary Dice Games
/// @file charconv.cpp
/// @brief from_chars_double() and its portable fallback.
#include "core/charconv.hpp"
#include <cmath>
#include <limits>
#include <locale>
#include <sstream>
#include <string>
#include <system_error>

namespace bdg::nymph {

namespace {

bool is_digit(char c) {
  return c >= '0' && c <= '9';
}

char lower(char c) {
  return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

/// Whether [p, last) starts with `word`, ignoring case.
bool starts_with_word(const char* p, const char* last, const char* word) {
  for (; *word != '\0'; ++p, ++word) {
    if (p == last || lower(*p) != *word) {
      return false;
    }
  }
  return true;
}

} // namespace

std::from_chars_result from_chars_double_fallback(const char* first, const char* last, double& value) {
  const char* p = first;
  const bool negative = p != last && *p == '-';
  if (negative) {
    ++p;
  }

  // inf, infinity, nan, nan(n-char-sequence).
  if (starts_with_word(p, last, "inf")) {
    p += starts_with_word(p, last, "infinity") ? 8 : 3;
    value = negative ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity();
    return {p, std::errc{}};
  }
  if (starts_with_word(p, last, "nan")) {
    p += 3;
    if (p != last && *p == '(') {
      const char* q = p + 1;
      while (q != last && (is_digit(*q) || (lower(*q) >= 'a' && lower(*q) <= 'z') || *q == '_')) {
        ++q;
      }
      if (q != last && *q == ')') {
        p = q + 1;
      }
    }
    value = negative ? -std::numeric_limits<double>::quiet_NaN() : std::numeric_limits<double>::quiet_NaN();
    return {p, std::errc{}};
  }

  // [digits][.digits] with at least one digit, then an optional exponent.
  const char* mantissa = p;
  while (p != last && is_digit(*p)) {
    ++p;
  }
  bool digits = p != mantissa;
  if (p != last && *p == '.') {
    const char* fraction = ++p;
    while (p != last && is_digit(*p)) {
      ++p;
    }
    digits = digits || p != fraction;
  }
  if (!digits) {
    return {first, std::errc::invalid_argument};
  }
  if (p != last && (*p == 'e' || *p == 'E')) {
    const char* q = p + 1;
    if (q != last && (*q == '+' || *q == '-')) {
      ++q;
    }
    const char* exponent = q;
    while (q != last && is_digit(*q)) {
      ++q;
    }
    if (q != exponent) {
      p = q;
    }
  }

  // The text is now a plain decimal number: the classic-locale stream
  // conversion rounds it correctly.
  std::istringstream in(std::string(first, p));
  in.imbue(std::locale::classic());
  double parsed = 0.0;
  in >> parsed;
  if (in.fail() || !std::isfinite(parsed)) {
    return {p, std::errc::result_out_of_range};
  }
  if (parsed == 0.0) {
    // Underflow: a non-zero digit that rounded to zero.
    for (const char* d = mantissa; d != p && *d != 'e' && *d != 'E'; ++d) {
      if (*d >= '1' && *d <= '9') {
        return {p, std::errc::result_out_of_range};
      }
    }
  }
  value = parsed;
  return {p, std::errc{}};
}

std::from_chars_result from_chars_double(const char* first, const char* last, double& value) {
#if defined(NYMPH_HAS_FLOAT_FROM_CHARS)
  return std::from_chars(first, last, value);
#else
  return from_chars_double_fallback(first, last, value);
#endif
}

} // namespace bdg::nymph
