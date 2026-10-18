// MIT License © 2026 Binary Dice Games
/// @file dates.hpp
/// @brief ISO 8601 dates and times as UNIX seconds (UTC), for Gantt charts.
///        Pure arithmetic: no time zone, no locale, no OS call.
#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace bdg::nymph {

/// @brief Days from 1970-01-01 to the given civil date (proleptic
///        Gregorian; Howard Hinnant's days_from_civil).
inline int64_t days_from_civil(int64_t y, unsigned m, unsigned d) {
  y -= m <= 2 ? 1 : 0;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<int64_t>(doe) - 719468;
}

/// @brief A civil date (year, month 1-12, day 1-31).
struct civil_date {
  int64_t year;
  unsigned month;
  unsigned day;
};

/// @brief The civil date `days` after 1970-01-01 (Howard Hinnant's
///        civil_from_days).
inline civil_date civil_from_days(int64_t days) {
  days += 719468;
  const int64_t era = (days >= 0 ? days : days - 146096) / 146097;
  const unsigned doe = static_cast<unsigned>(days - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  const unsigned d = doy - (153 * mp + 2) / 5 + 1;
  const unsigned m = mp < 10 ? mp + 3 : mp - 9;
  return {static_cast<int64_t>(yoe) + era * 400 + (m <= 2 ? 1 : 0), m, d};
}

/// @brief `YYYY-MM-DD`, optionally followed by `T` or a space and `HH:MM` or
///        `HH:MM:SS`, and an optional `Z`, as UNIX seconds in UTC.
/// @return std::nullopt when `text` is not such a date, or names a day or
///         time that does not exist (2026-02-30, 24:00).
inline std::optional<double> parse_date_time(std::string_view text) {
  size_t pos = 0;
  auto number = [&](size_t digits, int& out) {
    if (pos + digits > text.size()) {
      return false;
    }
    out = 0;
    for (size_t i = 0; i < digits; ++i) {
      const char c = text[pos + i];
      if (c < '0' || c > '9') {
        return false;
      }
      out = out * 10 + (c - '0');
    }
    pos += digits;
    return true;
  };
  auto expect = [&](char c) {
    if (pos < text.size() && text[pos] == c) {
      ++pos;
      return true;
    }
    return false;
  };
  int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
  if (!number(4, year) || !expect('-') || !number(2, month) || !expect('-') || !number(2, day)) {
    return std::nullopt;
  }
  if (pos < text.size() && (text[pos] == 'T' || text[pos] == ' ')) {
    ++pos;
    if (!number(2, hour) || !expect(':') || !number(2, minute)) {
      return std::nullopt;
    }
    if (expect(':') && !number(2, second)) {
      return std::nullopt;
    }
  }
  expect('Z');
  if (pos != text.size()) {
    return std::nullopt;
  }
  static constexpr int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
  if (month < 1 || month > 12 || day < 1 || day > kDays[month - 1] + (month == 2 && leap ? 1 : 0) || hour > 23 ||
      minute > 59 || second > 59) {
    return std::nullopt;
  }
  const int64_t days = days_from_civil(year, static_cast<unsigned>(month), static_cast<unsigned>(day));
  return static_cast<double>(days * 86400 + hour * 3600 + minute * 60 + second);
}

} // namespace bdg::nymph
