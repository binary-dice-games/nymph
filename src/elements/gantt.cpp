// MIT License © 2026 Binary Dice Games
/// @file gantt.cpp
/// @brief `GanttChart`, `GanttSection` and `GanttTask`.
///
/// The chart is an ImPlot plot: one row per task (top to bottom, named by
/// the Y ticks), time on the X axis (a Time axis when the tasks give
/// dates). Each section is a legend entry whose colour its tasks take;
/// bars, milestones and dependency arrows are drawn on the plot's draw
/// list. Times are resolved once per check and once per frame by
/// schedule_of(), which also reports every source error.
#include "elements/gantt.hpp"
#include <implot.h>
#include <implot_internal.h> // BeginItem: a coloured legend entry
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "core/dates.hpp"
#include "core/error.hpp"
#include "core/limits.hpp"
#include "core/suggest.hpp"
#include "elements/plot_pick.hpp"
#include "elements/plot_style.hpp"
#include "render/draw_context.hpp"
#include "render/text_style.hpp"
#include "schema/element_class.hpp"

namespace bdg::nymph {

namespace {

constexpr const char* kGanttTaskSlot = "gantt.task";
constexpr const char* kGanttSectionSlot = "gantt.section";

const char* const kTimeHelp = "a date (YYYY-MM-DD, or YYYY-MM-DD HH:MM in UTC) or a number";

// ── GanttTask ────────────────────────────────────────────────────────────────

/// `GanttTask.status` values.
constexpr int64_t kStatusNone = 0, kStatusDone = 1, kStatusActive = 2, kStatusCritical = 3;

const name_table& status_names() {
  static const name_table t{
      {"None", kStatusNone}, {"Done", kStatusDone}, {"Active", kStatusActive}, {"Critical", kStatusCritical}};
  return t;
}

namespace task_field {
constexpr size_t id = 0, name = 1, start = 2, end = 3, duration = 4, after = 5, milestone = 6, status = 7, color = 8;
}

class task_class : public element_class {
 public:
  task_class()
      : element_class(
            "GanttTask",
            kGanttTaskSlot,
            {},
            {
                text_field("id", "", "Id that other tasks' after names; optional unless named."),
                text_field("name", "", "Name shown on the task's row; empty shows the id."),
                text_field(
                    "start",
                    "",
                    std::string("When the task starts: ") + kTimeHelp + ". Leave empty and give after instead."),
                text_field(
                    "end",
                    "",
                    "When the task ends, in the same form as start (a date means its midnight: 2026-03-02 to "
                    "2026-03-05 is three days). Leave empty and give duration instead."),
                number_field(
                    "duration",
                    0.0,
                    "Length of the task instead of end: days when start is a date, else start's units.",
                    value_range{0, 1e9}),
                texts_field(
                    "after",
                    "Ids of the tasks this one waits for: it starts when the last of them ends, with an arrow "
                    "from each (mermaid's after)."),
                bool_field("milestone", false, "When true, the task is a moment, drawn as a diamond at its start."),
                enum_field(
                    "status",
                    kStatusNone,
                    status_names(),
                    "Done (pale), Active (outlined), Critical (red outline) or None."),
                color_field("color", "Bar colour; empty uses its section's colour."),
            },
            "A task (a bar from start to end) or a milestone of a GanttChart, on its own row.") {}

  void draw(const node&, draw_context&) const override {} // drawn by GanttChart
};

class section_class : public element_class {
 public:
  section_class()
      : element_class(
            "GanttSection",
            kGanttSectionSlot,
            {kGanttTaskSlot},
            {
                text_field("name", "", "Section name, shown in the legend."),
                color_field("color", "Colour of its tasks; empty takes the next colour of the chart's colormap."),
            },
            "A group of tasks (a phase, a team) sharing a colour and a legend entry. Children: GanttTask.") {}

  void check(const node& n) const override {
    if (n.get<std::string>(0).empty()) {
      fail_at(n, 0, "is empty; a section needs a name for the legend");
    }
  }

  void draw(const node&, draw_context&) const override {} // drawn by GanttChart
};

// ── Schedule ─────────────────────────────────────────────────────────────────

/// One task with its resolved times.
struct scheduled_task {
  const node* n = nullptr;
  int section = -1; ///< Index into schedule::sections, or -1.
  double start = 0.0;
  double end = 0.0;
  bool milestone = false;
  std::vector<size_t> after; ///< Indices of the tasks it waits for.
};

struct schedule {
  std::vector<scheduled_task> tasks; ///< In row order, top to bottom.
  std::vector<const node*> sections;
  bool dates = false; ///< Times are UNIX seconds (a Time axis).
  double lo = 0.0, hi = 0.0; ///< Extent of every task.
};

const std::string& task_name(const node& t) {
  const std::string& name = t.get<std::string>(task_field::name);
  return name.empty() ? t.get<std::string>(task_field::id) : name;
}

/// `text` as a number, if it is one (all of it).
std::optional<double> parse_number(const std::string& text) {
  char* stop = nullptr;
  const double d = std::strtod(text.c_str(), &stop);
  if (stop != text.c_str() && *stop == '\0' && std::isfinite(d)) {
    return d;
  }
  return std::nullopt;
}

/// The tasks of `chart` with their times; throws nymph::error at the field
/// for any problem (spec/01_core_features/DESIGN.md section 3).
schedule schedule_of(const node& chart) {
  using namespace task_field;
  schedule s;
  for (const node& child : chart.children) {
    if (child.cls->name() == "GanttSection") {
      s.sections.push_back(&child);
      for (const node& t : child.children) {
        s.tasks.push_back({&t, static_cast<int>(s.sections.size()) - 1});
      }
    } else {
      s.tasks.push_back({&child, -1});
    }
  }
  if (s.tasks.size() > kMaxGanttTasks) {
    throw error(
        chart.line,
        chart.column,
        "GanttChart has " + std::to_string(s.tasks.size()) + " tasks; at most " + std::to_string(kMaxGanttTasks) +
            " are allowed");
  }
  // Ids.
  std::map<std::string, size_t, std::less<>> by_id;
  std::vector<std::string> ids;
  for (size_t i = 0; i < s.tasks.size(); ++i) {
    const node& t = *s.tasks[i].n;
    const std::string& tid = t.get<std::string>(id);
    if (tid.empty() && t.get<std::string>(name).empty()) {
      t.cls->fail_at(t, name, "is empty; give the task a name (or an id)");
    }
    if (tid.empty()) {
      continue;
    }
    if (const auto it = by_id.find(tid); it != by_id.end()) {
      t.cls->fail_at(
          t,
          id,
          ": id '" + tid + "' is already used by the task at line " + std::to_string(s.tasks[it->second].n->line));
    }
    by_id.emplace(tid, i);
    ids.push_back(tid);
  }
  // Fields of each task, and the kind of time (dates or numbers) the first
  // written time sets.
  std::optional<bool> dates;
  const node* dates_set_by = nullptr;
  auto parse = [&](const node& t, size_t field, const std::string& text) {
    std::optional<double> v = parse_date_time(text);
    const bool is_date = v.has_value();
    if (!is_date) {
      v = parse_number(text);
    }
    if (!v) {
      t.cls->fail_at(t, field, "is not " + std::string(kTimeHelp) + ": '" + text + "'");
    }
    if (!dates) {
      dates = is_date;
      dates_set_by = &t;
    } else if (*dates != is_date) {
      t.cls->fail_at(
          t,
          field,
          std::string("is a ") + (is_date ? "date" : "number") + " but the task at line " +
              std::to_string(dates_set_by->line) + " uses " + (*dates ? "dates" : "numbers") +
              "; a chart uses one or the other");
    }
    return *v;
  };
  std::vector<std::optional<double>> starts(s.tasks.size()), ends(s.tasks.size());
  for (size_t i = 0; i < s.tasks.size(); ++i) {
    scheduled_task& st = s.tasks[i];
    const node& t = *st.n;
    st.milestone = t.get<bool>(milestone);
    const std::string& start_text = t.get<std::string>(start);
    const std::string& end_text = t.get<std::string>(end);
    const double dur = t.get<double>(duration);
    const auto& after_ids = t.get<std::vector<std::string>>(after);
    if (!start_text.empty() && !after_ids.empty()) {
      t.cls->fail_at(t, after, "cannot be used with start: the task starts when the tasks in after end");
    }
    if (start_text.empty() && after_ids.empty()) {
      t.cls->fail_at(t, start, "is empty: give start, or after (the tasks it waits for)");
    }
    if (st.milestone && (!end_text.empty() || dur > 0.0)) {
      t.cls->fail_at(t, end_text.empty() ? duration : end, "is not used by a milestone, which is a moment");
    }
    if (!st.milestone && end_text.empty() && dur <= 0.0) {
      t.cls->fail_at(t, end, "is empty: give end, or a duration above 0");
    }
    if (!end_text.empty() && dur > 0.0) {
      t.cls->fail_at(t, duration, "cannot be used with end: give one of them");
    }
    if (!start_text.empty()) {
      starts[i] = parse(t, start, start_text);
    }
    if (!end_text.empty()) {
      ends[i] = parse(t, end, end_text);
    }
    for (const std::string& a : after_ids) {
      const auto it = by_id.find(a);
      if (it == by_id.end()) {
        t.cls->fail_at(t, after, ": unknown task id '" + a + "'" + did_you_mean(a, ids));
      }
      if (it->second == i) {
        t.cls->fail_at(t, after, "names the task itself");
      }
      st.after.push_back(it->second);
    }
  }
  s.dates = dates.value_or(false);
  // Resolve `after` in dependency order; a cycle is an error.
  std::vector<int> state(s.tasks.size(), 0); // 0 new, 1 in progress, 2 done
  std::function<void(size_t)> resolve = [&](size_t i) {
    scheduled_task& st = s.tasks[i];
    const node& t = *st.n;
    if (state[i] == 2) {
      return;
    }
    if (state[i] == 1) {
      t.cls->fail_at(t, after, "makes a cycle: the task ends up waiting for itself");
    }
    state[i] = 1;
    if (starts[i]) {
      st.start = *starts[i];
    } else {
      st.start = -INFINITY;
      for (size_t p : st.after) {
        resolve(p);
        st.start = std::max(st.start, s.tasks[p].end);
      }
    }
    const double unit = s.dates ? 86400.0 : 1.0;
    st.end = st.milestone ? st.start : (ends[i] ? *ends[i] : st.start + t.get<double>(duration) * unit);
    if (st.end < st.start) {
      t.cls->fail_at(t, ends[i] ? end : start, "makes the task end before it starts");
    }
    state[i] = 2;
  };
  for (size_t i = 0; i < s.tasks.size(); ++i) {
    resolve(i);
  }
  for (size_t i = 0; i < s.tasks.size(); ++i) {
    s.lo = i == 0 ? s.tasks[i].start : std::min(s.lo, s.tasks[i].start);
    s.hi = i == 0 ? s.tasks[i].end : std::max(s.hi, s.tasks[i].end);
  }
  return s;
}

// ── Date ticks ───────────────────────────────────────────────────────────────

/// Ticks of a date axis from `lo` to `hi` (UNIX seconds) that fit
/// `width_px`: whole hours, days, Mondays, months or years, labelled in
/// ISO 8601 ("2026-03-08", "2026-03", "03-08 14:00"). ImPlot's own Time
/// axis labels days "--03-08", which reads poorly on a schedule.
void date_ticks(double lo, double hi, float width_px, std::vector<double>* at, std::vector<std::string>* labels) {
  constexpr double kHour = 3600.0, kDay = 86400.0;
  struct step {
    double seconds; ///< 0 for a step in months.
    int months;
  };
  static constexpr step kSteps[] = {
      {kHour, 0},
      {3 * kHour, 0},
      {6 * kHour, 0},
      {12 * kHour, 0},
      {kDay, 0},
      {2 * kDay, 0},
      {7 * kDay, 0},
      {14 * kDay, 0},
      {0, 1},
      {0, 3},
      {0, 6},
      {0, 12},
      {0, 60},
  };
  const float label_w = ImGui::CalcTextSize("2026-03-08").x * 1.3f;
  const double max_ticks = std::max(2.0, std::floor(static_cast<double>(width_px / label_w)));
  const double span = hi - lo;
  // A tick closer to the right end than half a label (less the plot's
  // padding) would have its label cut; on the left, the labels have the
  // room under the row names.
  const double margin = span * std::max(0.0f, label_w / 1.3f * 0.5f - 10.0f) / std::max(1.0f, width_px);
  const double first_ok = lo, last_ok = hi - margin;
  step chosen = kSteps[std::size(kSteps) - 1];
  for (const step& st : kSteps) {
    const double length = st.months == 0 ? st.seconds : st.months * 30.44 * kDay;
    if (span / length <= max_ticks) {
      chosen = st;
      break;
    }
  }
  char text[32];
  if (chosen.months == 0) {
    // Weeks start on Monday; 1970-01-01 was a Thursday.
    const double origin = chosen.seconds >= 7 * kDay ? 4 * kDay : 0.0;
    for (double t = origin + std::ceil((lo - origin) / chosen.seconds) * chosen.seconds; t <= hi; t += chosen.seconds) {
      const int64_t days = static_cast<int64_t>(std::floor(t / kDay));
      const civil_date c = civil_from_days(days);
      if (chosen.seconds < kDay) {
        const int64_t secs = static_cast<int64_t>(t) - days * 86400;
        std::snprintf(text, sizeof(text), "%02u-%02u %02d:00", c.month, c.day, static_cast<int>(secs / 3600));
      } else {
        std::snprintf(text, sizeof(text), "%04lld-%02u-%02u", static_cast<long long>(c.year), c.month, c.day);
      }
      if (t >= first_ok && t <= last_ok) {
        at->push_back(t);
        labels->push_back(text);
      }
    }
    return;
  }
  const civil_date first = civil_from_days(static_cast<int64_t>(std::floor(lo / kDay)));
  int64_t month = first.year * 12 + (first.month - 1); // months since year 0
  month = (month + chosen.months - 1) / chosen.months * chosen.months;
  for (;; month += chosen.months) {
    const int64_t year = month / 12;
    const unsigned m = static_cast<unsigned>(month % 12) + 1;
    const double t = static_cast<double>(days_from_civil(year, m, 1)) * kDay;
    if (t < lo) {
      continue;
    }
    if (t > hi) {
      break;
    }
    if (chosen.months >= 12) {
      std::snprintf(text, sizeof(text), "%04lld", static_cast<long long>(year));
    } else {
      std::snprintf(text, sizeof(text), "%04lld-%02u", static_cast<long long>(year), m);
    }
    if (t >= first_ok && t <= last_ok) {
      at->push_back(t);
      labels->push_back(text);
    }
  }
}

// ── GanttChart ───────────────────────────────────────────────────────────────

namespace chart_field {
constexpr size_t title = 0, width = 1, height = 2, x_label = 3, today = 4, colormap = 5, bar_height = 6,
                 show_dependencies = 7;
}

/// `color` (or the section's / item's colour) with alpha `a`.
ImU32 with_alpha(ImVec4 c, float a) {
  c.w *= a;
  return ImGui::ColorConvertFloat4ToU32(c);
}

class chart_class : public element_class {
 public:
  chart_class()
      : element_class(
            "GanttChart",
            std::string(kFigureSlot),
            {kGanttTaskSlot, kGanttSectionSlot},
            with_text_style({
                text_field("title", "", "Title above the chart; empty for none."),
                number_field(
                    "width",
                    0.0,
                    "Width in pixels; 0 (default) is 600 in a HorizontalLayout and fills elsewhere. In a "
                    "HorizontalLayout a negative value is a stretch weight.",
                    value_range{-1000, 8192}),
                number_field(
                    "height",
                    0.0,
                    "Height in pixels; 0 (default) is the natural height (enough for every row) in a "
                    "VerticalLayout, and fills elsewhere. In a VerticalLayout a negative value is a stretch weight.",
                    value_range{-1000, 8192}),
                text_field("x_label", "", "Label under the time axis; empty for none."),
                text_field(
                    "today",
                    "",
                    std::string("Draws a red line marked \"today\" at this time: ") + kTimeHelp +
                        ", like the tasks'. Empty for none."),
                colormap_field(),
                number_field("bar_height", 0.6, "Height of a bar as a fraction of its row.", value_range{0.1, 1.0}),
                bool_field("show_dependencies", true, "When true, draws an arrow from each task in after."),
            }),
            "A Gantt chart: tasks as bars on a time axis, one row each, top to bottom, in sections with a colour "
            "each. Children: GanttSection, GanttTask.") {}

  void check(const node& n) const override {
    const schedule s = schedule_of(n);
    if (s.tasks.empty()) {
      throw error(n.line, n.column, "GanttChart has no tasks; add GanttTask children");
    }
    if (const std::string& today = n.get<std::string>(chart_field::today); !today.empty()) {
      today_of(n, s);
    }
  }

  /// Natural size: 600 wide; rows of 1.8 lines plus the title, the axis
  /// and the legend.
  ImVec2 measure(const node& n, draw_context&, float) const override {
    const float line = ImGui::GetTextLineHeight();
    const ImPlotStyle& style = ImPlot::GetStyle();
    size_t rows = 0;
    bool sections = false;
    for (const node& child : n.children) {
      sections = sections || child.cls->name() == "GanttSection";
      rows += child.cls->name() == "GanttSection" ? child.children.size() : 1;
    }
    float h = static_cast<float>(rows) * std::ceil(line * 1.8f) + 2.0f * style.PlotPadding.y;
    h += n.get<std::string>(chart_field::title).empty() ? 0.0f : line + style.LabelPadding.y;
    h += 2.0f * (line + style.LabelPadding.y); // tick labels (a Time axis has two lines)
    h += n.get<std::string>(chart_field::x_label).empty() ? 0.0f : line + style.LabelPadding.y;
    h += sections ? line + 2.0f * style.LegendPadding.y + style.LegendInnerPadding.y * 2.0f : 0.0f;
    return ImVec2(600.0f, std::ceil(h));
  }

  view_kind viewable() const override {
    return view_kind::gantt;
  }

  void draw(const node& n, draw_context& dc) const override {
    using namespace chart_field;
    const schedule s = schedule_of(n);
    const rect r = dc.rect_of(n);
    ImGui::SetCursorScreenPos(r.min);
    const int64_t cmap = n.get<int64_t>(colormap);
    if (cmap != kColormapDefault) {
      ImPlot::PushColormap(static_cast<ImPlotColormap>(cmap));
    }
    const std::string& title_text = n.get<std::string>(title);
    const std::string id = (title_text.empty() ? std::string() : title_text) + "##gantt";
    const ImPlotFlags flags = ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect | ImPlotFlags_NoMouseText |
        (s.sections.empty() ? ImPlotFlags_NoLegend : 0);
    dc.add_input_area(r);
    if (ImPlot::BeginPlot(id.c_str(), ImVec2(r.width(), r.height()), flags)) {
      setup(n, s, r.width(), dc);
      plot(n, s, dc);
      finish_plot_picks(dc);
      dc.record_current_plot(n, false, view_kind::gantt);
      ImPlot::EndPlot();
    }
    if (cmap != kColormapDefault) {
      ImPlot::PopColormap();
    }
  }

 private:
  /// The `today` time, in the chart's kind of time.
  double today_of(const node& n, const schedule& s) const {
    const std::string& text = n.get<std::string>(chart_field::today);
    const std::optional<double> v = s.dates ? parse_date_time(text) : parse_number(text);
    if (!v) {
      fail_at(
          n,
          chart_field::today,
          std::string("must be ") + (s.dates ? "a date" : "a number") + " like the tasks' times, not '" + text + "'");
    }
    return *v;
  }

  static void setup(const node& n, const schedule& s, float width, const draw_context& dc) {
    using namespace chart_field;
    const std::string& xl = n.get<std::string>(x_label);
    ImPlot::SetupAxes(
        xl.empty() ? nullptr : xl.c_str(),
        nullptr,
        ImPlotAxisFlags_None,
        ImPlotAxisFlags_Invert | ImPlotAxisFlags_NoGridLines | ImPlotAxisFlags_NoTickMarks);
    const double span = s.hi > s.lo ? s.hi - s.lo : (s.dates ? 86400.0 : 1.0);
    // Room for a milestone's diamond and a dependency's elbow at the ends:
    // about 20 px of the plot area (three quarters of the width).
    const double pad = span * std::max(0.03, 20.0 / std::max(100.0, static_cast<double>(width) * 0.75));
    const double x0 = s.lo - pad, x1 = s.hi + pad;
    // In a live view the time axis pans and zooms (the rows stay fixed),
    // and the date ticks follow the range on screen.
    ImPlot::SetupAxisLimits(ImAxis_X1, x0, x1, dc.limits_cond());
    double t0 = x0, t1 = x1;
    if (const element_view* saved = dc.view_of(n); saved != nullptr && saved->x) {
      ImPlot::SetupAxisLimits(ImAxis_X1, saved->x->min, saved->x->max, dc.limits_cond());
      t0 = saved->x->min;
      t1 = saved->x->max;
    }
    if (const plot_view_record* view = dc.previous_plot_view(n)) {
      t0 = view->x_min;
      t1 = view->x_max;
    }
    if (s.dates) {
      // The row names take about a quarter of the width.
      std::vector<double> at;
      std::vector<std::string> labels;
      date_ticks(t0, t1, width * 0.75f, &at, &labels);
      std::vector<const char*> texts;
      for (const std::string& l : labels) {
        texts.push_back(l.c_str());
      }
      if (!at.empty()) {
        ImPlot::SetupAxisTicks(ImAxis_X1, at.data(), static_cast<int>(at.size()), texts.data());
      }
    }
    const double rows = static_cast<double>(s.tasks.size());
    ImPlot::SetupAxisLimits(ImAxis_Y1, -0.5, rows - 0.5, ImPlotCond_Always);
    std::vector<double> positions;
    std::vector<const char*> names;
    for (size_t i = 0; i < s.tasks.size(); ++i) {
      positions.push_back(static_cast<double>(i));
      names.push_back(task_name(*s.tasks[i].n).c_str());
    }
    ImPlot::SetupAxisTicks(ImAxis_Y1, positions.data(), static_cast<int>(positions.size()), names.data());
    if (!s.sections.empty()) {
      ImPlot::SetupLegend(ImPlotLocation_South, ImPlotLegendFlags_Outside | ImPlotLegendFlags_Horizontal);
    }
  }

  void plot(const node& n, const schedule& s, draw_context& dc) const {
    using namespace chart_field;
    // One legend item per section (tasks outside sections share a hidden
    // one), whose colour the section's tasks take.
    std::vector<ImVec4> colors(s.sections.size() + 1);
    for (size_t k = 0; k <= s.sections.size(); ++k) {
      const bool loose = k == s.sections.size();
      if (loose &&
          std::none_of(s.tasks.begin(), s.tasks.end(), [](const scheduled_task& t) { return t.section < 0; })) {
        continue;
      }
      // A legend item with no data, as PlotDummy, but with the section's
      // colour (PlotDummy always takes the next colormap colour).
      const std::string& own = loose ? std::string() : s.sections[k]->get<std::string>(1);
      const char* label = loose ? "##tasks" : s.sections[k]->get<std::string>(0).c_str();
      if (ImPlot::BeginItem(label, ImPlotSpec(), own.empty() ? IMPLOT_AUTO_COL : parse_hex_color(own))) {
        ImPlot::EndItem();
      }
      colors[k] = ImPlot::GetLastItemColor();
    }
    ImDrawList* dl = ImPlot::GetPlotDrawList();
    ImPlot::PushPlotClipRect();
    const float half = static_cast<float>(n.get<double>(bar_height)) * 0.5f;
    const float row_px = std::abs(ImPlot::PlotToPixels(0.0, 1.0).y - ImPlot::PlotToPixels(0.0, 0.0).y);
    const float bar_half_px = row_px * half;
    const ImVec4 text = ImGui::GetStyleColorVec4(ImGuiCol_Text);
    if (n.get<bool>(show_dependencies)) {
      for (size_t i = 0; i < s.tasks.size(); ++i) {
        for (size_t p : s.tasks[i].after) {
          draw_dependency(dl, s, p, i, bar_half_px, row_px, with_alpha(text, 0.65f));
        }
      }
    }
    for (size_t i = 0; i < s.tasks.size(); ++i) {
      const scheduled_task& t = s.tasks[i];
      const std::string& own = t.n->get<std::string>(task_field::color);
      const ImVec4 col = !own.empty() ? parse_hex_color(own)
                                      : colors[t.section < 0 ? s.sections.size() : static_cast<size_t>(t.section)];
      const int64_t status = t.n->get<int64_t>(task_field::status);
      const double row = static_cast<double>(i);
      const float outline = status == kStatusNone || status == kStatusDone ? 1.0f : 2.0f;
      const ImU32 border = status == kStatusCritical ? IM_COL32(214, 39, 40, 255)
          : status == kStatusActive                  ? ImGui::ColorConvertFloat4ToU32(text)
                                                     : with_alpha(col, 1.0f);
      const ImU32 fill = with_alpha(col, status == kStatusDone ? 0.4f : 1.0f);
      if (t.milestone) {
        const ImVec2 c = ImPlot::PlotToPixels(t.start, row);
        const float h = std::max(bar_half_px, 4.0f);
        offer_task(dc, s, i, ImVec2(c.x - h, c.y - h), ImVec2(c.x + h, c.y + h), border);
        dl->AddQuadFilled(ImVec2(c.x, c.y - h), ImVec2(c.x + h, c.y), ImVec2(c.x, c.y + h), ImVec2(c.x - h, c.y), fill);
        dl->AddQuad(
            ImVec2(c.x, c.y - h), ImVec2(c.x + h, c.y), ImVec2(c.x, c.y + h), ImVec2(c.x - h, c.y), border, outline);
        continue;
      }
      ImVec2 a = ImPlot::PlotToPixels(t.start, row);
      ImVec2 b = ImPlot::PlotToPixels(t.end, row);
      a = ImVec2(std::floor(a.x), std::floor(a.y - bar_half_px));
      b = ImVec2(std::max(std::floor(b.x), a.x + 1.0f), std::floor(b.y + bar_half_px));
      dl->AddRectFilled(a, b, fill, 3.0f);
      dl->AddRect(a, b, border, 3.0f, ImDrawFlags_None, outline);
      offer_task(dc, s, i, a, b, border);
    }
    ImPlot::PopPlotClipRect();
    if (const std::string& today = n.get<std::string>(chart_field::today); !today.empty()) {
      const double x = today_of(n, s);
      ImPlotSpec spec;
      spec.LineColor = ImVec4(214 / 255.0f, 39 / 255.0f, 40 / 255.0f, 1.0f);
      spec.LineWeight = 2.0f;
      ImPlot::PlotInfLines("##today", &x, 1, spec);
      ImPlot::TagX(x, spec.LineColor, "%s", "today");
    }
  }

  /// In a live view, offers task `i` (drawn in `a`-`b`) when the pointer
  /// is on it.
  static void offer_task(draw_context& dc, const schedule& s, size_t i, ImVec2 a, ImVec2 b, ImU32 color) {
    const std::optional<ImVec2> pointer = plot_pointer(dc, nullptr);
    if (!pointer) {
      return;
    }
    const scheduled_task& t = s.tasks[i];
    const auto time = [&](double v) {
      char buf[64];
      std::snprintf(buf, sizeof(buf), "%g", v);
      return s.dates ? format_iso_time(v) : std::string(buf);
    };
    pick hit;
    hit.element = t.n;
    hit.type = std::string(t.n->cls->name());
    hit.label = task_name(*t.n);
    hit.index = static_cast<int>(i);
    if (t.milestone) {
      hit.values = {{"at", time(t.start)}};
    } else {
      hit.values = {{"start", time(t.start)}, {"end", time(t.end)}};
    }
    if (t.section >= 0) {
      hit.values.push_back({"section", s.sections[static_cast<size_t>(t.section)]->get<std::string>(0)});
    }
    offer_rect_px(dc, *pointer, std::move(hit), a, b, color);
  }

  /// An elbow arrow from the end of task `from` into the start of task
  /// `to`: out to the right, across to the target's row and in from the
  /// left; when the target starts too close to (or before) that end, the
  /// line runs back along the gap above the target's row first.
  static void draw_dependency(
      ImDrawList* dl,
      const schedule& s,
      size_t from,
      size_t to,
      float bar_half_px,
      float row_px,
      ImU32 col) {
    const scheduled_task& a = s.tasks[from];
    const scheduled_task& b = s.tasks[to];
    ImVec2 p0 = ImPlot::PlotToPixels(a.end, static_cast<double>(from));
    ImVec2 p1 = ImPlot::PlotToPixels(b.start, static_cast<double>(to));
    if (a.milestone) {
      p0.x += std::max(bar_half_px, 4.0f);
    }
    if (b.milestone) {
      p1.x -= std::max(bar_half_px, 4.0f);
    }
    const float gap = 8.0f;
    std::vector<ImVec2> pts{p0, ImVec2(p0.x + gap, p0.y)};
    if (p1.x - gap >= p0.x + gap) {
      pts.push_back(ImVec2(p0.x + gap, p1.y));
    } else {
      const float between = p1.y + (p1.y > p0.y ? -0.5f : 0.5f) * row_px;
      pts.push_back(ImVec2(p0.x + gap, between));
      pts.push_back(ImVec2(p1.x - gap, between));
      pts.push_back(ImVec2(p1.x - gap, p1.y));
    }
    pts.push_back(ImVec2(p1.x - 6.0f, p1.y));
    dl->AddPolyline(pts.data(), static_cast<int>(pts.size()), col, ImDrawFlags_None, 1.5f);
    dl->AddTriangleFilled(ImVec2(p1.x, p1.y), ImVec2(p1.x - 7.0f, p1.y - 4.0f), ImVec2(p1.x - 7.0f, p1.y + 4.0f), col);
  }
};

} // namespace

void register_gantt(registry& reg) {
  reg.add(std::make_unique<chart_class>());
  reg.add(std::make_unique<section_class>());
  reg.add(std::make_unique<task_class>());
}

} // namespace bdg::nymph
