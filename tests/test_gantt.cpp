// MIT License © 2026 Binary Dice Games
/// @file test_gantt.cpp
/// @brief `GanttChart`, `GanttSection`, `GanttTask`, and the ISO date
///        parser they use (src/core/dates.hpp).
#include <gtest/gtest.h>
#include <array>
#include <cstdint>
#include <string>
#include "core/dates.hpp"
#include "core/error.hpp"
#include "document/bind.hpp"
#include "elements/register_builtin.hpp"
#include "nymph.hpp"
#include "render/figure_renderer.hpp"

namespace bdg::nymph {
namespace {

// ── Dates ────────────────────────────────────────────────────────────────────

TEST(dates, ParsesIsoDatesAndTimesAsUtcSeconds) {
  EXPECT_EQ(parse_date_time("1970-01-01"), 0.0);
  EXPECT_EQ(parse_date_time("1970-01-02"), 86400.0);
  EXPECT_EQ(parse_date_time("2026-03-02"), 1772409600.0);
  EXPECT_EQ(parse_date_time("2026-03-02 13:30"), 1772409600.0 + 13.5 * 3600);
  EXPECT_EQ(parse_date_time("2026-03-02T13:30:15Z"), 1772409600.0 + 13.5 * 3600 + 15);
  EXPECT_EQ(parse_date_time("2024-02-29"), 1709164800.0);
  EXPECT_EQ(parse_date_time("1969-12-31"), -86400.0);
  for (const char* bad :
       {"",
        "2026",
        "2026-3-2",
        "2026-02-30",
        "2025-02-29",
        "2026-13-01",
        "2026-03-02 24:00",
        "2026-03-02 10",
        "2026-03-02x",
        "03/02/2026",
        "12"}) {
    EXPECT_FALSE(parse_date_time(bad).has_value()) << bad;
  }
}

TEST(dates, CivilDaysRoundTrip) {
  for (int64_t days : {-1000000LL, -719468LL, -1LL, 0LL, 59LL, 20514LL, 2932896LL}) {
    const civil_date c = civil_from_days(days);
    EXPECT_EQ(days_from_civil(c.year, c.month, c.day), days) << days;
  }
  const civil_date c = civil_from_days(20514); // 2026-03-02
  EXPECT_EQ(c.year, 2026);
  EXPECT_EQ(c.month, 3u);
  EXPECT_EQ(c.day, 2u);
}

// ── Charts ───────────────────────────────────────────────────────────────────

image render(const std::string& source) {
  return render_figure(bind_source(source, builtin_registry()));
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

/// Expects `source` to fail at `line` with a message containing `part`.
void expect_error(const std::string& source, int line, const std::string& part) {
  const error e = source_error(source);
  EXPECT_EQ(e.line(), line) << e.what();
  EXPECT_NE(std::string(e.what()).find(part), std::string::npos) << e.what() << "\nexpected: " << part;
}

std::string with(const std::string& src, const std::string& from, const std::string& to) {
  std::string s = src;
  const size_t at = s.find(from);
  EXPECT_NE(at, std::string::npos) << from;
  return s.replace(at, from.size(), to);
}

using rgba = std::array<uint8_t, 4>;

size_t count_color(const image& img, rgba c) {
  size_t n = 0;
  for (int y = 0; y < img.height; ++y) {
    for (int x = 0; x < img.width; ++x) {
      const uint8_t* p = img.at(x, y);
      n += (rgba{p[0], p[1], p[2], p[3]} == c) ? 1 : 0;
    }
  }
  return n;
}

const char* const kPlan =
    "image: {width: 640, height: 360}\n" // 1
    "root:\n" // 2
    "  type: GanttChart\n" // 3
    "  title: Plan\n" // 4
    "  children:\n" // 5
    "    - type: GanttSection\n" // 6
    "      name: Design\n" // 7
    "      color: \"#FF0000\"\n" // 8
    "      children:\n" // 9
    "        - {type: GanttTask, id: spec, name: Spec, start: 2026-03-02, duration: 4}\n" // 10
    "        - {type: GanttTask, id: review, name: Review, after: [spec], duration: 2}\n" // 11
    "    - type: GanttSection\n" // 12
    "      name: Build\n" // 13
    "      children:\n" // 14
    "        - {type: GanttTask, id: code, name: Code, after: [review], end: 2026-03-16}\n" // 15
    "        - {type: GanttTask, id: ship, name: Ship, after: [code, review], milestone: true}\n"; // 16

TEST(gantt, PlanRendersDeterministicallyAndRoundTrips) {
  const std::vector<uint8_t> a = render_png(kPlan);
  EXPECT_EQ(render_png(kPlan), a);
  EXPECT_EQ(extract_source(a).value_or(""), kPlan);
}

TEST(gantt, SectionColourFillsItsBars) {
  // Spec and Review are red bars; recolouring the section removes them.
  const size_t red = count_color(render(kPlan), {255, 0, 0, 255});
  EXPECT_GT(red, 1000u);
  EXPECT_LT(count_color(render(with(kPlan, "#FF0000", "#00FF00")), {255, 0, 0, 255}), 50u);
  // A task's own colour wins over its section's.
  const std::string own = with(kPlan, "name: Code, after", "name: Code, color: \"#FF0000\", after");
  EXPECT_GT(count_color(render(own), {255, 0, 0, 255}), red + 500);
}

TEST(gantt, FieldsChangeTheImage) {
  const image base = render(kPlan);
  for (const auto& [from, to] : std::vector<std::pair<std::string, std::string>>{
           {"title: Plan\n", "title: Plan\n  today: 2026-03-09\n"},
           {"title: Plan\n", "title: Plan\n  show_dependencies: false\n"},
           {"title: Plan\n", "title: Plan\n  bar_height: 0.9\n"},
           {"duration: 4}", "duration: 4, status: Done}"},
           {"duration: 4}", "duration: 4, status: Critical}"},
           {"duration: 4}", "duration: 4, status: Active}"},
       }) {
    SCOPED_TRACE(to);
    EXPECT_FALSE(render(with(kPlan, from, to)).rgba == base.rgba);
  }
}

TEST(gantt, NumbersInsteadOfDates) {
  const std::string src =
      "root:\n  type: GanttChart\n  x_label: sprint\n  today: 2.5\n  children:\n"
      "    - {type: GanttTask, id: a, name: A, start: 0, end: 2}\n"
      "    - {type: GanttTask, name: B, after: [a], duration: 1.5}\n";
  EXPECT_NO_THROW(render(src));
}

TEST(gantt, NaturalHeightGrowsWithTheRows) {
  // In a VerticalLayout with no height, the chart takes its natural height:
  // a chart of many tasks needs more room than a short image gives.
  std::string tasks;
  for (int i = 0; i < 30; ++i) {
    tasks += "        - {type: GanttTask, name: T" + std::to_string(i) + ", start: " + std::to_string(i) +
        ", duration: 1}\n";
  }
  const std::string src =
      "image: {width: 400, height: 300}\nroot:\n  type: VerticalLayout\n  children:\n"
      "    - type: GanttChart\n      children:\n" +
      tasks;
  const error e = source_error(src);
  EXPECT_NE(std::string(e.what()).find("px of height"), std::string::npos) << e.what();
}

TEST(gantt, TaskErrors) {
  expect_error(with(kPlan, "start: 2026-03-02, duration: 4", "duration: 4"), 10, "GanttTask.start is empty");
  expect_error(
      with(kPlan, "after: [spec], duration: 2", "start: 2026-03-02, after: [spec], duration: 2"),
      11,
      "GanttTask.after cannot be used with start");
  expect_error(with(kPlan, "after: [spec]", "after: [spce]"), 11, "unknown task id 'spce' (did you mean 'spec'?)");
  expect_error(with(kPlan, ", duration: 4}", "}"), 10, "GanttTask.end is empty");
  expect_error(
      with(kPlan, "start: 2026-03-02, duration: 4", "start: 2026-03-02, end: 2026-03-01"), 10, "end before it starts");
  expect_error(
      with(kPlan, "start: 2026-03-02, duration: 4", "start: 2026-03-02, end: 2026-03-04, duration: 4"),
      10,
      "GanttTask.duration cannot be used with end");
  expect_error(with(kPlan, "milestone: true", "milestone: true, duration: 1"), 16, "not used by a milestone");
  expect_error(with(kPlan, "start: 2026-03-02,", "start: March 2,"), 10, "is not a date");
  expect_error(with(kPlan, "end: 2026-03-16", "end: 16"), 15, "is a number but the task at line 10 uses dates");
  expect_error(with(kPlan, "start: 2026-03-02, duration: 4", "after: [ship], duration: 4"), 10, "makes a cycle");
  expect_error(with(kPlan, "id: review,", "id: spec,"), 11, "id 'spec' is already used");
  expect_error(with(kPlan, "title: Plan\n", "title: Plan\n  today: 9\n"), 5, "GanttChart.today must be a date");
  expect_error(with(kPlan, "      name: Build\n", ""), 12, "GanttSection.name is empty");
  expect_error("root: {type: GanttChart}\n", 1, "GanttChart has no tasks");
}

} // namespace
} // namespace bdg::nymph
