// MIT License © 2026 Binary Dice Games
/// @file gantt.cpp
/// @brief mermaid `gantt` → `GanttChart`, with mermaid's schedule resolved
///        to explicit dates (spec/16_mermaid_import/DESIGN.md section 4.6).
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <map>
#include "core/dates.hpp"
#include "core/limits.hpp"
#include "mermaid/mermaid_internal.hpp"

namespace bdg::nymph::mermaid {

namespace {

/// Times are in days since 1970-01-01, UTC.
using days = double;

/// A date in mermaid's `dateFormat`; nullopt if `text` does not match.
/// @throws (through ctx) for a format token nymph does not read.
std::optional<days> parse_date(std::string_view text, std::string_view format, pos at, context& ctx) {
  text = trim(text);
  int year = 1970, month = 1, day = 1, hour = 0, minute = 0, second = 0;
  std::optional<double> epoch_seconds;
  size_t t = 0;
  size_t f = 0;
  auto digits = [&](size_t min, size_t max, int& out) {
    size_t n = 0;
    out = 0;
    while (n < max && t < text.size() && text[t] >= '0' && text[t] <= '9') {
      out = out * 10 + (text[t] - '0');
      ++t;
      ++n;
    }
    return n >= min;
  };
  static constexpr std::string_view tokens[] = {"YYYY", "YY", "MM", "M", "DD", "D", "HH", "H", "mm", "m", "ss", "s", "X", "x"};
  while (f < format.size()) {
    std::string_view token;
    for (std::string_view tok : tokens) {
      if (format.substr(f, tok.size()) == tok) {
        token = tok;
        break;
      }
    }
    if (token.empty()) {
      const char c = format[f];
      if (std::isalpha(static_cast<unsigned char>(c)) != 0) {
        size_t n = f;
        while (n < format.size() && format[n] == c) {
          ++n;
        }
        ctx.fail(at, "dateFormat token '" + std::string(format.substr(f, n - f)) +
                         "' is not supported (nymph reads YYYY, YY, MM, M, DD, D, HH, H, mm, m, ss, s, X, x)");
      }
      if (t >= text.size() || text[t] != c) {
        return std::nullopt;
      }
      ++t;
      ++f;
      continue;
    }
    f += token.size();
    int v = 0;
    if (token == "YYYY") {
      if (!digits(4, 4, year)) {
        return std::nullopt;
      }
    } else if (token == "YY") {
      if (!digits(2, 2, v)) {
        return std::nullopt;
      }
      year = v < 69 ? 2000 + v : 1900 + v;
    } else if (token == "MM" || token == "M") {
      if (!digits(token.size(), 2, month)) {
        return std::nullopt;
      }
    } else if (token == "DD" || token == "D") {
      if (!digits(token.size(), 2, day)) {
        return std::nullopt;
      }
    } else if (token == "HH" || token == "H") {
      if (!digits(token.size(), 2, hour)) {
        return std::nullopt;
      }
    } else if (token == "mm" || token == "m") {
      if (!digits(token.size(), 2, minute)) {
        return std::nullopt;
      }
    } else if (token == "ss" || token == "s") {
      if (!digits(token.size(), 2, second)) {
        return std::nullopt;
      }
    } else {
      size_t end = t;
      while (end < text.size() && ((text[end] >= '0' && text[end] <= '9') || text[end] == '.' || text[end] == '-')) {
        ++end;
      }
      const std::optional<double> n = parse_number(text.substr(t, end - t));
      if (!n) {
        return std::nullopt;
      }
      epoch_seconds = token == "X" ? *n : *n / 1000.0;
      t = end;
    }
  }
  if (t != text.size()) {
    return std::nullopt;
  }
  if (epoch_seconds) {
    return *epoch_seconds / 86400.0;
  }
  if (month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 || minute > 59 || second > 59) {
    return std::nullopt;
  }
  const civil_date check = civil_from_days(days_from_civil(year, unsigned(month), unsigned(day)));
  if (check.month != unsigned(month) || check.day != unsigned(day)) {
    return std::nullopt;
  }
  return static_cast<double>(days_from_civil(year, unsigned(month), unsigned(day))) +
      (hour * 3600.0 + minute * 60.0 + second) / 86400.0;
}

/// `3d`, `1.5w`, `12h`, `30m`, `10s`, `500ms` in days; nullopt if not one.
std::optional<days> parse_duration(std::string_view text, pos at, context& ctx) {
  text = trim(text);
  size_t unit = 0;
  while (unit < text.size() && ((text[unit] >= '0' && text[unit] <= '9') || text[unit] == '.')) {
    ++unit;
  }
  if (unit == 0 || unit == text.size()) {
    return std::nullopt;
  }
  const std::optional<double> n = parse_number(text.substr(0, unit));
  if (!n) {
    return std::nullopt;
  }
  const std::string_view u = text.substr(unit);
  if (u == "ms") {
    return *n / 86400000.0;
  }
  if (u == "s") {
    return *n / 86400.0;
  }
  if (u == "m") {
    return *n / 1440.0;
  }
  if (u == "h") {
    return *n / 24.0;
  }
  if (u == "d") {
    return *n;
  }
  if (u == "w") {
    return *n * 7.0;
  }
  if (u == "M" || u == "y") {
    ctx.warn(at, "duration '" + std::string(text) + "' is approximated (" + (u == "M" ? "30" : "365") + " days)");
    return *n * (u == "M" ? 30.0 : 365.0);
  }
  return std::nullopt;
}

/// A day as `YYYY-MM-DD`, or `YYYY-MM-DD HH:MM` when it has a time.
std::string date_text(days d) {
  // Round to the minute, the finest nymph writes.
  const double minutes = std::round(d * 1440.0);
  const int64_t whole = static_cast<int64_t>(std::floor(minutes / 1440.0));
  const int64_t rest = static_cast<int64_t>(minutes) - whole * 1440;
  const civil_date c = civil_from_days(whole);
  char buf[64];
  if (rest == 0) {
    std::snprintf(buf, sizeof(buf), "%04lld-%02u-%02u", static_cast<long long>(c.year), c.month, c.day);
  } else {
    std::snprintf(buf, sizeof(buf), "%04lld-%02u-%02u %02lld:%02lld", static_cast<long long>(c.year), c.month, c.day,
                  static_cast<long long>(rest / 60), static_cast<long long>(rest % 60));
  }
  return buf;
}

struct task {
  std::string id;  ///< written or generated
  bool written_id = false;
  std::string name;
  std::string status = "None";
  bool milestone = false;
  days start = 0, end = 0;
  std::vector<std::string> after; ///< as written
  std::optional<days> start_date; ///< written or implicit start
  std::optional<days> end_date;   ///< written end, or `until`
  std::optional<days> duration;   ///< written duration
  int section = -1;
  pos at;
};

std::pair<std::string_view, std::string_view> word_and_rest(std::string_view text) {
  text = trim(text);
  const size_t space = text.find_first_of(" \t");
  if (space == std::string_view::npos) {
    return {text, {}};
  }
  return {text.substr(0, space), trim(text.substr(space))};
}

} // namespace

document convert_gantt(const source_text& src, context& ctx) {
  std::string format = "YYYY-MM-DD";
  std::string title = src.title;
  std::vector<std::pair<std::string, pos>> sections;
  std::vector<task> tasks;
  std::map<std::string, size_t> by_id;
  for (const line& l : src.body) {
    const pos at = l.start();
    const std::string_view text = trim(l.text);
    const auto [word, rest] = word_and_rest(text);
    if (word == "title") {
      title = label_text(rest);
      continue;
    }
    if (word == "dateFormat") {
      format = std::string(rest);
      continue;
    }
    if (word == "section") {
      sections.push_back({label_text(rest), at});
      continue;
    }
    if (word == "axisFormat" || word == "tickInterval" || word == "excludes" || word == "includes" ||
        word == "weekday" || word == "todayMarker" || word == "inclusiveEndDates" || word == "topAxis" ||
        word == "weekend" || word == "displayMode") {
      ctx.warn(at, std::string(word) + " is ignored" + (word == "excludes" ? " (durations count every day)" : ""));
      continue;
    }
    if (word == "click") {
      ctx.warn(at, "click is ignored (a nymph figure has no links or callbacks)");
      continue;
    }
    const size_t colon = text.find(':');
    if (colon == std::string_view::npos) {
      ctx.fail(at, "expected a task (Name : start, duration), a section or a setting, found '" + std::string(text) + "'");
    }
    task t;
    t.at = at;
    t.name = label_text(text.substr(0, colon));
    t.section = static_cast<int>(sections.size()) - 1;
    std::vector<std::string> items = split_outside_quotes(text.substr(colon + 1), ',');
    // Tags first.
    size_t first = 0;
    for (; first < items.size(); ++first) {
      const std::string& tag = items[first];
      if (tag == "done") {
        t.status = t.status == "None" ? "Done" : t.status;
      } else if (tag == "active") {
        t.status = t.status == "Critical" ? t.status : "Active";
      } else if (tag == "crit") {
        t.status = "Critical";
      } else if (tag == "milestone") {
        t.milestone = true;
      } else if (tag == "vert") {
        ctx.warn(at, "vert is ignored (drawn as an ordinary task)");
      } else {
        break;
      }
    }
    items.erase(items.begin(), items.begin() + static_cast<std::ptrdiff_t>(first));
    if (items.empty() || items.size() > 3) {
      ctx.fail(at, "a task takes [id,] [start,] and an end or duration after the ':'");
    }
    if (items.size() == 3) {
      t.id = items[0];
      t.written_id = true;
      items.erase(items.begin());
    }
    const task* previous = tasks.empty() ? nullptr : &tasks.back();
    // Start.
    if (items.size() == 2) {
      const std::string& s = items[0];
      if (starts_with(s, "after ")) {
        for (const std::string& id : split_outside_quotes(std::string_view(s).substr(6), ' ')) {
          if (id.empty()) {
            continue;
          }
          if (by_id.count(id) == 0) {
            std::vector<std::string> known;
            for (const auto& [k, v] : by_id) {
              known.push_back(k);
            }
            ctx.fail(at, "after names unknown task '" + id + "'" + suggestion(id, known));
          }
          t.after.push_back(id);
        }
        t.start = 0;
        for (const std::string& id : t.after) {
          t.start = std::max(t.start, tasks[by_id[id]].end);
        }
      } else if (const std::optional<days> d = parse_date(s, format, at, ctx)) {
        t.start_date = *d;
        t.start = *d;
      } else {
        ctx.fail(at, "'" + s + "' is not a date in the dateFormat " + format + " nor 'after <id>'");
      }
    } else {
      if (previous == nullptr) {
        ctx.fail(at, "the first task needs a start date");
      }
      t.start_date = previous->end;
      t.start = previous->end;
    }
    // End.
    const std::string& e = items.back();
    if (starts_with(e, "until ")) {
      const std::string id(trim(std::string_view(e).substr(6)));
      if (by_id.count(id) == 0) {
        ctx.fail(at, "until names unknown (or later) task '" + id + "'");
      }
      t.end = tasks[by_id[id]].start;
      t.end_date = t.end;
    } else if (const std::optional<days> d = parse_date(e, format, at, ctx)) {
      t.end = *d;
      t.end_date = *d;
    } else if (const std::optional<days> dur = parse_duration(e, at, ctx)) {
      t.duration = *dur;
      t.end = t.start + *dur;
    } else {
      ctx.fail(at, "'" + e + "' is not a date in the dateFormat " + format + ", a duration (3d) nor 'until <id>'");
    }
    if (t.end < t.start) {
      ctx.fail(at, "the task ends before it starts");
    }
    if (!t.id.empty()) {
      if (by_id.count(t.id) != 0) {
        ctx.fail(at, "task id '" + t.id + "' is used twice");
      }
      by_id[t.id] = tasks.size();
    }
    tasks.push_back(t);
    check_limit(tasks.size(), kMaxGanttTasks, "tasks", at, ctx);
  }
  if (tasks.empty()) {
    ctx.fail(src.header_pos, "the gantt chart has no tasks");
  }
  // Which ids the output needs.
  std::map<std::string, bool> needed;
  for (const task& t : tasks) {
    for (const std::string& id : t.after) {
      needed[id] = true;
    }
  }
  auto task_element = [&](const task& t) {
    element e("GanttTask", t.at);
    if (needed.count(t.id) != 0) {
      e.text("id", t.id);
    }
    e.text("name", t.name.empty() ? t.id : t.name);
    if (!t.after.empty()) {
      e.texts("after", t.after);
    } else {
      e.text("start", date_text(t.start));
    }
    if (t.milestone) {
      e.flag("milestone", true);
    } else if (t.duration) {
      // Days, to the second (a few decimals rather than the float's 17 digits).
      e.number("duration", std::round(*t.duration * 1e6) / 1e6);
    } else {
      e.text("end", date_text(t.end));
    }
    if (t.status != "None") {
      e.name("status", t.status);
    }
    return e;
  };
  element root("GanttChart", src.header_pos);
  root.text("title", title);
  for (const task& t : tasks) {
    if (t.section < 0) {
      root.children.push_back(task_element(t));
    }
  }
  for (size_t s = 0; s < sections.size(); ++s) {
    element sec("GanttSection", sections[s].second);
    sec.text("name", sections[s].first);
    for (const task& t : tasks) {
      if (t.section == static_cast<int>(s)) {
        sec.children.push_back(task_element(t));
      }
    }
    if (!sec.children.empty()) {
      root.children.push_back(sec);
    }
  }
  document doc;
  doc.root = root;
  doc.width = 800;
  doc.fit_width_only = true;
  return doc;
}

} // namespace bdg::nymph::mermaid
