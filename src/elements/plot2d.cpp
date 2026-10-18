// MIT License © 2026 Binary Dice Games
/// @file plot2d.cpp
/// @brief `Plot` and the 2D series.
///
/// Schema (names, defaults, ranges, descriptions, flag tables) from wish's
/// `src/ui/plot_elements/`; drawing from wish's
/// `src/imgui/imgui_plot_renderer.cpp`. Differences, all stricter or
/// additive: rules between fields are checked when binding (check()), a
/// series with only `ys` is drawn against 0, 1, 2, ..., and printf formats
/// are validated.
#include "elements/plot2d.hpp"
#include <implot.h>
#include <implot_internal.h> // ImPlotContext: error-bar item colours
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "elements/plot_pick.hpp"
#include "elements/plot_style.hpp"
#include "render/draw_context.hpp"
#include "render/text_style.hpp"
#include "schema/element_class.hpp"

namespace bdg::nymph {

namespace {

using doubles = std::vector<double>;

// ── Flag tables (wish's plot.cpp) ────────────────────────────────────────────

const name_table& plot_flag_names() {
  static const name_table t{
      {"None", 0},
      {"NoTitle", 1 << 0},
      {"NoLegend", 1 << 1},
      {"NoMouseText", 1 << 2},
      {"NoInputs", 1 << 3},
      {"NoMenus", 1 << 4},
      {"NoBoxSelect", 1 << 5},
      {"NoFrame", 1 << 6},
      {"Equal", 1 << 7},
      {"Crosshairs", 1 << 8},
      {"CanvasOnly", (1 << 0) | (1 << 1) | (1 << 4) | (1 << 5) | (1 << 2)},
  };
  return t;
}

const name_table& axis_flag_names() {
  static const name_table t{
      {"None", 0},
      {"NoLabel", 1 << 0},
      {"NoGridLines", 1 << 1},
      {"NoTickMarks", 1 << 2},
      {"NoTickLabels", 1 << 3},
      {"NoInitialFit", 1 << 4},
      {"NoMenus", 1 << 5},
      {"NoSideSwitch", 1 << 6},
      {"NoHighlight", 1 << 7},
      {"Opposite", 1 << 8},
      {"Foreground", 1 << 9},
      {"Invert", 1 << 10},
      {"AutoFit", 1 << 11},
      {"RangeFit", 1 << 12},
      {"PanStretch", 1 << 13},
      {"LockMin", 1 << 14},
      {"LockMax", 1 << 15},
      {"Lock", (1 << 14) | (1 << 15)},
      {"NoDecorations", (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3)},
  };
  return t;
}

const name_table& legend_location_names() {
  static const name_table t{
      {"Center", 0},
      {"North", 1 << 0},
      {"South", 1 << 1},
      {"West", 1 << 2},
      {"East", 1 << 3},
      {"NorthWest", (1 << 0) | (1 << 2)},
      {"NorthEast", (1 << 0) | (1 << 3)},
      {"SouthWest", (1 << 1) | (1 << 2)},
      {"SouthEast", (1 << 1) | (1 << 3)},
  };
  return t;
}

const name_table& legend_flag_names() {
  static const name_table t{
      {"None", 0},
      {"NoButtons", 1 << 0},
      {"NoHighlightItem", 1 << 1},
      {"NoHighlightAxis", 1 << 2},
      {"NoMenus", 1 << 3},
      {"Outside", 1 << 4},
      {"Horizontal", 1 << 5},
      {"Sort", 1 << 6},
      {"Reverse", 1 << 7},
  };
  return t;
}

const name_table& axis_scale_names() {
  static const name_table t{
      {"Linear", ImPlotScale_Linear},
      {"Time", ImPlotScale_Time},
      {"Log10", ImPlotScale_Log10},
      {"SymLog", ImPlotScale_SymLog},
  };
  return t;
}

/// Values of a series' `y_axis` field.
constexpr int64_t kOnY1 = 0;
constexpr int64_t kOnY2 = 1;

const name_table& y_axis_names() {
  static const name_table t{{"Y1", kOnY1}, {"Y2", kOnY2}};
  return t;
}

const char* str(const node& n, size_t i) {
  return n.get<std::string>(i).c_str();
}

int count_of(const doubles& v) {
  return static_cast<int>(v.size());
}

/// 0, 1, 2, ... for a series written with `ys` only.
doubles index_xs(size_t count) {
  doubles xs(count);
  for (size_t i = 0; i < count; ++i) {
    xs[i] = static_cast<double>(i);
  }
  return xs;
}

ImPlotSpec item_spec(const node& n) {
  ImPlotSpec spec;
  apply_item_style(spec, n);
  return spec;
}

// ── Plot ─────────────────────────────────────────────────────────────────────

namespace plot_field {
constexpr size_t title = 0, x_label = 1, y_label = 2, width = 3, height = 4, flags = 5, x_flags = 6, y_flags = 7,
                 x_min = 8, x_max = 9, y_min = 10, y_max = 11, legend_location = 12, legend_flags = 13, colormap = 14;
// 15 to 17: font, font_size, text_color. Then with_axis_fields():
constexpr size_t x_scale = 18, y_scale = 19, x_format = 20, y_format = 21, x_ticks = 22, x_tick_labels = 23,
                 y_ticks = 24, y_tick_labels = 25, y2_label = 26, y2_flags = 27, y2_min = 28, y2_max = 29,
                 y2_scale = 30, y2_format = 31, y2_ticks = 32, y2_tick_labels = 33;
} // namespace plot_field

/// The fields of one plot axis.
struct axis_fields {
  ImAxis axis;
  size_t label, flags, min, max, scale, format, ticks, tick_labels;
};

constexpr axis_fields kAxes[] = {
    {ImAxis_X1,
     plot_field::x_label,
     plot_field::x_flags,
     plot_field::x_min,
     plot_field::x_max,
     plot_field::x_scale,
     plot_field::x_format,
     plot_field::x_ticks,
     plot_field::x_tick_labels},
    {ImAxis_Y1,
     plot_field::y_label,
     plot_field::y_flags,
     plot_field::y_min,
     plot_field::y_max,
     plot_field::y_scale,
     plot_field::y_format,
     plot_field::y_ticks,
     plot_field::y_tick_labels},
    {ImAxis_Y2,
     plot_field::y2_label,
     plot_field::y2_flags,
     plot_field::y2_min,
     plot_field::y2_max,
     plot_field::y2_scale,
     plot_field::y2_format,
     plot_field::y2_ticks,
     plot_field::y2_tick_labels},
};

/// The `x_scale`, `x_format`, `x_ticks` and `x_tick_labels` fields of an
/// axis called `axis` ("x", "y", "y2").
std::vector<field_desc> axis_fields_of(const std::string& axis) {
  const std::string upper = axis == "x" ? "X" : (axis == "y" ? "Y" : "second Y");
  return {
      enum_field(
          axis + "_scale",
          ImPlotScale_Linear,
          axis_scale_names(),
          "Scale of the " + upper +
              " axis: Linear (default); Log10, powers of ten, for values spanning several orders of magnitude "
              "(fixed limits must be above 0); SymLog, log-like but allowing 0 and negative values; or Time, "
              "values in UNIX seconds labelled as UTC dates and times."),
      double_format_field(
          axis + "_format",
          "",
          "printf format of the " + upper +
              " tick labels, with one %f, %e or %g conversion (\"%.0f ms\", \"%.1f%%\"). Empty (default) is "
              "\"%g\". Not allowed on a Time axis."),
      numbers_field(
          axis + "_ticks",
          "Positions of the " + upper + " ticks, in data units. Empty (default) places them automatically."),
      texts_field(
          axis + "_tick_labels",
          "Text of each tick in " + axis +
              "_ticks, in order, such as category names under bars. Empty (default) "
              "labels the ticks with their values."),
  };
}

/// `fields` (Plot's own and its text style fields) followed by the axis
/// fields, in the index order of plot_field: x_ and y_ scale, format, ticks
/// and tick labels, then the second Y axis.
std::vector<field_desc> with_axis_fields(std::vector<field_desc> f) {
  const std::vector<field_desc> x = axis_fields_of("x");
  const std::vector<field_desc> y = axis_fields_of("y");
  // x_scale, y_scale, x_format, y_format, x_ticks, x_tick_labels, y_ticks,
  // y_tick_labels (the order of plot_field).
  f.push_back(x[0]);
  f.push_back(y[0]);
  f.push_back(x[1]);
  f.push_back(y[1]);
  f.push_back(x[2]);
  f.push_back(x[3]);
  f.push_back(y[2]);
  f.push_back(y[3]);
  f.push_back(text_field(
      "y2_label",
      "",
      "Label of the second Y axis, on the right. Setting any y2_ field, or giving a series y_axis: Y2, shows "
      "the axis."));
  f.push_back(flags_field("y2_flags", 0, axis_flag_names(), "ImPlotAxisFlags for the second Y axis."));
  f.push_back(number_field(
      "y2_min",
      0.0,
      "Fixed lower limit of the second Y axis.  Ignored (axis auto-fits to its series) when equal to y2_max."));
  f.push_back(number_field(
      "y2_max",
      0.0,
      "Fixed upper limit of the second Y axis.  Ignored (axis auto-fits to its series) when equal to y2_min."));
  for (field_desc& d : axis_fields_of("y2")) {
    f.push_back(std::move(d));
  }
  return f;
}

/// Whether a child of `plot` is drawn against its second Y axis (defined
/// after series_class).
bool any_child_on_y2(const node& plot);

class plot_class : public element_class {
 public:
  plot_class()
      : element_class(
            "Plot",
            std::string(kFigureSlot),
            {kPlot2dItemSlot},
            with_axis_fields(with_text_style({
                text_field(
                    "title",
                    "##plot",
                    "Plot title shown at the top of the frame; prefix ## to make the ID hidden (no visible title)."),
                text_field("x_label", "", "X axis label; empty string = no label."),
                text_field("y_label", "", "Y axis label; empty string = no label."),
                number_field(
                    "width",
                    -1.0,
                    "Plot width in pixels; -1 (default) fills the available width. In a HorizontalLayout a negative "
                    "value is a stretch weight (-1 and -3 split the space left 1:3) and 0 the natural width.",
                    value_range{-1000, 8192}),
                number_field(
                    "height",
                    300.0,
                    "Plot height in pixels. A Plot used as root with no height fills the image. In a VerticalLayout "
                    "a negative value is a stretch weight (-1 and -3 split the space left 1:3) and 0 the natural "
                    "height (300).",
                    value_range{-1000, 8192}),
                flags_field(
                    "flags",
                    0,
                    plot_flag_names(),
                    "ImPlotFlags bitmask (combine names with '|'; see implot.h)."),
                flags_field(
                    "x_flags",
                    0,
                    axis_flag_names(),
                    "ImPlotAxisFlags for the primary X axis (combine names with '|'). Use NoDecorations for pie "
                    "charts."),
                flags_field(
                    "y_flags",
                    0,
                    axis_flag_names(),
                    "ImPlotAxisFlags for the primary Y axis (combine names with '|'). Use NoDecorations for pie "
                    "charts."),
                number_field(
                    "x_min",
                    0.0,
                    "Fixed lower X-axis limit, locked every frame.  Ignored (axis auto-fits normally) when equal to "
                    "x_max."),
                number_field(
                    "x_max",
                    0.0,
                    "Fixed upper X-axis limit, locked every frame.  Ignored (axis auto-fits normally) when equal to "
                    "x_min."),
                number_field(
                    "y_min",
                    0.0,
                    "Fixed lower Y-axis limit, locked every frame.  Ignored (axis auto-fits normally) when equal to "
                    "y_max."),
                number_field(
                    "y_max",
                    0.0,
                    "Fixed upper Y-axis limit, locked every frame.  Ignored (axis auto-fits normally) when equal to "
                    "y_min."),
                flags_field(
                    "legend_location",
                    -1,
                    legend_location_names(),
                    "Where the legend sits (ImPlotLocation: North/South/West/East and the corner combinations, or "
                    "Center). -1 (default) keeps ImPlot's own default of top-left inside the plot. Combine with "
                    "legend_flags Outside to move the legend out of the plot area entirely."),
                flags_field(
                    "legend_flags",
                    0,
                    legend_flag_names(),
                    "ImPlotLegendFlags bitmask (combine names with '|'): Outside renders the legend outside the plot "
                    "frame, Horizontal lays entries in a row, plus NoButtons / NoMenus / Sort / Reverse."),
                colormap_field(),
            })),
            "A 2D plot (ImPlot). Children are 2D series: PlotLine, PlotBars, PlotScatter, ...") {
    // plot_field must follow with_axis_fields(): a mismatch is a bug.
    const char* const prefixes[] = {"x", "y", "y2"};
    for (size_t i = 0; i < std::size(kAxes); ++i) {
      const axis_fields& a = kAxes[i];
      const std::string p = prefixes[i];
      for (const auto& [suffix, index] : {
               std::pair<const char*, size_t>{"_label", a.label},
               {"_flags", a.flags},
               {"_min", a.min},
               {"_max", a.max},
               {"_scale", a.scale},
               {"_format", a.format},
               {"_ticks", a.ticks},
               {"_tick_labels", a.tick_labels},
           }) {
        if (field_index(p + suffix) != static_cast<int>(index)) {
          throw std::logic_error("Plot field index of " + p + suffix + " is wrong");
        }
      }
    }
  }

  void check(const node& n) const override {
    for (const axis_fields& a : kAxes) {
      const size_t ticks = n.get<doubles>(a.ticks).size();
      const size_t labels = n.get<std::vector<std::string>>(a.tick_labels).size();
      if (labels != 0 && labels != ticks) {
        fail_at(
            n,
            a.tick_labels,
            "has " + std::to_string(labels) + " labels but " + fields()[a.ticks].name + " has " +
                std::to_string(ticks) + " values");
      }
      const int64_t scale = n.get<int64_t>(a.scale);
      if (scale == ImPlotScale_Time && !n.get<std::string>(a.format).empty()) {
        fail_at(n, a.format, "does not apply to a Time axis, whose ticks show dates and times");
      }
      const double lo = n.get<double>(a.min);
      const double hi = n.get<double>(a.max);
      if (scale == ImPlotScale_Log10 && lo != hi && std::min(lo, hi) <= 0.0) {
        fail_at(n, lo <= 0.0 ? a.min : a.max, "must be above 0 on a Log10 axis");
      }
    }
  }

  void adjust_root(node& n, const std::vector<bool>& set) const override {
    if (!set[plot_field::height]) {
      n.values[plot_field::height] = -1.0; // fill the image (wish nymph's rule)
    }
  }

  /// Natural size (a 0 hint): wish's default height, 400 wide.
  ImVec2 measure(const node&, draw_context&, float) const override {
    return ImVec2(400.0f, 300.0f);
  }

  void draw(const node& n, draw_context& dc) const override {
    using namespace plot_field;
    const rect r = dc.rect_of(n);
    // The layout engine already applied width / height: draw at the rectangle.
    const ImVec2 size(r.width(), r.height());
    ImGui::SetCursorScreenPos(r.min);

    const int64_t colormap_value = n.get<int64_t>(colormap);
    if (colormap_value != kColormapDefault) {
      ImPlot::PushColormap(static_cast<ImPlotColormap>(colormap_value));
    }
    if (ImPlot::BeginPlot(str(n, title), size, static_cast<ImPlotFlags>(n.get<int64_t>(flags)))) {
      const std::string& xl = n.get<std::string>(x_label);
      const std::string& yl = n.get<std::string>(y_label);
      const auto xf = static_cast<ImPlotAxisFlags>(n.get<int64_t>(x_flags));
      const auto yf = static_cast<ImPlotAxisFlags>(n.get<int64_t>(y_flags));
      if (!xl.empty() || !yl.empty() || xf != 0 || yf != 0) {
        ImPlot::SetupAxes(xl.empty() ? nullptr : xl.c_str(), yl.empty() ? nullptr : yl.c_str(), xf, yf);
      }
      const bool y2 = uses_y2(n);
      if (y2) {
        const std::string& l = n.get<std::string>(y2_label);
        // On the right, without its own grid lines (ImPlot's usual setup
        // for a secondary axis).
        ImPlot::SetupAxis(
            ImAxis_Y2,
            l.empty() ? nullptr : l.c_str(),
            static_cast<ImPlotAxisFlags>(n.get<int64_t>(y2_flags)) | ImPlotAxisFlags_AuxDefault);
      }
      for (const axis_fields& a : kAxes) {
        if (a.axis != ImAxis_Y2 || y2) {
          setup_axis(n, a, dc);
        }
      }
      const int64_t location = n.get<int64_t>(legend_location);
      const int64_t lflags = n.get<int64_t>(legend_flags);
      if (location >= 0 || lflags != 0) {
        ImPlot::SetupLegend(
            static_cast<ImPlotLocation>(location < 0 ? int64_t{ImPlotLocation_NorthWest} : location),
            static_cast<ImPlotLegendFlags>(lflags));
      }
      dc.draw_children(n);
      finish_plot_picks(dc);
      dc.record_current_plot(n, y2);
      ImPlot::EndPlot();
    }
    if (colormap_value != kColormapDefault) {
      ImPlot::PopColormap();
    }
  }

 private:
  /// Whether the second Y axis is shown: any y2_ field set, or a series
  /// drawn against it.
  static bool uses_y2(const node& n) {
    using namespace plot_field;
    return !n.get<std::string>(y2_label).empty() || n.get<int64_t>(y2_flags) != 0 ||
        n.get<double>(y2_min) != n.get<double>(y2_max) || n.get<int64_t>(y2_scale) != ImPlotScale_Linear ||
        !n.get<std::string>(y2_format).empty() || !n.get<doubles>(y2_ticks).empty() || any_child_on_y2(n);
  }

  /// Limits, scale, tick format and ticks of one (enabled) axis.
  static void setup_axis(const node& n, const axis_fields& a, const draw_context& dc) {
    if (n.get<double>(a.min) != n.get<double>(a.max)) {
      ImPlot::SetupAxisLimits(a.axis, n.get<double>(a.min), n.get<double>(a.max), dc.limits_cond());
    }
    if (const int64_t scale = n.get<int64_t>(a.scale); scale != ImPlotScale_Linear) {
      ImPlot::SetupAxisScale(a.axis, static_cast<ImPlotScale>(scale));
    }
    if (const std::string& fmt = n.get<std::string>(a.format); !fmt.empty()) {
      ImPlot::SetupAxisFormat(a.axis, fmt.c_str());
    }
    const doubles& ticks = n.get<doubles>(a.ticks);
    if (!ticks.empty()) {
      const auto& labels = n.get<std::vector<std::string>>(a.tick_labels);
      std::vector<const char*> texts;
      for (const std::string& s : labels) {
        texts.push_back(s.c_str());
      }
      ImPlot::SetupAxisTicks(a.axis, ticks.data(), count_of(ticks), texts.empty() ? nullptr : texts.data());
    }
  }
};

// ── Series base ──────────────────────────────────────────────────────────────

/// The shared series fields, `own`, then `y_axis` (last).
std::vector<field_desc> series_fields_2d(std::vector<field_desc> own) {
  std::vector<field_desc> f = series_fields(std::move(own));
  f.push_back(enum_field(
      "y_axis",
      kOnY1,
      y_axis_names(),
      "Y axis the series is drawn against: Y1 (default, left) or Y2, the plot's second Y axis on the right, for "
      "a quantity in other units. Y2's range, scale and ticks are the Plot's y2_ fields."));
  return f;
}

/// Base of the 2D series: draws its node against the Y axis chosen by
/// `y_axis` through plot().
class series_class : public element_class {
 public:
  series_class(std::string name, std::vector<field_desc> own, std::string description)
      : element_class(
            std::move(name),
            kPlot2dItemSlot,
            {},
            series_fields_2d(std::move(own)),
            std::move(description) + "  Must be a child of a Plot element."),
        y_axis_(fields().size() - 1) {}

  /// Whether `n` is drawn against the plot's second Y axis.
  bool on_y2(const node& n) const {
    return n.get<int64_t>(y_axis_) == kOnY2;
  }

  void draw(const node& n, draw_context& dc) const final {
    // The plot enables Y2 whenever a child uses it (any_child_on_y2()).
    const bool y2 = on_y2(n);
    if (y2) {
      ImPlot::SetAxes(ImAxis_X1, ImAxis_Y2);
    }
    plot(n, dc);
    if (y2) {
      ImPlot::SetAxes(ImAxis_X1, ImAxis_Y1);
    }
  }

 protected:
  /// Draws `n` inside the open plot, against the axes already selected.
  virtual void plot(const node& n, draw_context& dc) const = 0;

  /// `field` must not be empty.
  void require_values(const node& n, size_t field) const {
    if (n.get<doubles>(field).empty()) {
      fail_at(n, field, "is empty: the series has no data");
    }
  }

  /// `field` must be empty or as long as `like`.
  void require_same_length(const node& n, size_t field, size_t like) const {
    const size_t a = n.get<doubles>(field).size();
    const size_t b = n.get<doubles>(like).size();
    if (a != 0 && a != b) {
      fail_at(
          n, field, "has " + std::to_string(a) + " values but " + fields()[like].name + " has " + std::to_string(b));
    }
  }

 private:
  size_t y_axis_; ///< Index of the `y_axis` field (the last).
};

bool any_child_on_y2(const node& plot) {
  for (const node& child : plot.children) {
    const auto* series = dynamic_cast<const series_class*>(child.cls);
    if (series != nullptr && series->on_y2(child)) {
      return true;
    }
  }
  return false;
}

/// The colour a series without its own `color` takes when it belongs to
/// the series drawn just before it (error bars, annotations, tags): that
/// series' colour, or transparent (alpha 0) when it is the plot's first.
ImVec4 color_or_last_item(const node& n) {
  const std::string& color = n.get<std::string>(item_field::color);
  return color.empty() ? ImPlot::GetLastItemColor() : parse_hex_color(color);
}

// ── xs / ys series ───────────────────────────────────────────────────────────

enum class xy_kind { line, scatter, stairs, stems, shaded, digital, bars, bars_h };

constexpr size_t kXs = item_field::first_own;
constexpr size_t kYs = kXs + 1;
constexpr size_t kExtra = kYs + 1; // stems.ref, shaded.ys2, bars.bar_size

std::vector<field_desc> xy_fields(std::vector<field_desc> extra) {
  std::vector<field_desc> f{
      numbers_field("xs", "Array of X coordinates, one per data point. If empty, 0, 1, 2, ... are used."),
      numbers_field("ys", "Array of Y coordinates, one per data point."),
  };
  for (field_desc& e : extra) {
    f.push_back(std::move(e));
  }
  return f;
}

std::vector<field_desc> bar_fields() {
  return {
      numbers_field(
          "xs", "Array of bar X positions.  If empty, bars are placed at 0, 1, 2, ... using the ys values only."),
      numbers_field("ys", "Array of bar heights (or lengths for horizontal bars)."),
      number_field("bar_size", 0.67, "Width (or height for horizontal) of each bar in plot units.", value_range{0, 16}),
  };
}

class xy_series : public series_class {
 public:
  xy_series(std::string name, xy_kind kind, std::vector<field_desc> fields, std::string description)
      : series_class(std::move(name), std::move(fields), std::move(description)), kind_(kind) {}

  void check(const node& n) const override {
    require_values(n, kYs);
    require_same_length(n, kXs, kYs);
    if (kind_ == xy_kind::shaded) {
      require_same_length(n, kExtra, kYs); // ys2
    }
  }

  void plot(const node& n, draw_context& dc) const override {
    const doubles& ys = n.get<doubles>(kYs);
    const doubles generated = n.get<doubles>(kXs).empty() ? index_xs(ys.size()) : doubles{};
    const doubles& xs = generated.empty() ? n.get<doubles>(kXs) : generated;
    const char* label = str(n, item_field::label);
    const int count = count_of(ys);
    ImPlotSpec spec = item_spec(n);
    switch (kind_) {
      case xy_kind::line:
        ImPlot::PlotLine(label, xs.data(), ys.data(), count, spec);
        break;
      case xy_kind::scatter:
        ImPlot::PlotScatter(label, xs.data(), ys.data(), count, spec);
        break;
      case xy_kind::stairs:
        ImPlot::PlotStairs(label, xs.data(), ys.data(), count, spec);
        break;
      case xy_kind::stems:
        ImPlot::PlotStems(label, xs.data(), ys.data(), count, n.get<double>(kExtra), spec);
        break;
      case xy_kind::shaded: {
        const doubles& ys2 = n.get<doubles>(kExtra);
        if (!ys2.empty()) {
          ImPlot::PlotShaded(label, xs.data(), ys.data(), ys2.data(), count, spec);
        } else {
          ImPlot::PlotShaded(label, xs.data(), ys.data(), count, n.get<double>(kExtra + 1), spec);
        }
        break;
      }
      case xy_kind::digital:
        ImPlot::PlotDigital(label, xs.data(), ys.data(), count, spec);
        break;
      case xy_kind::bars:
      case xy_kind::bars_h:
        if (kind_ == xy_kind::bars_h) {
          spec.Flags = ImPlotBarsFlags_Horizontal;
        }
        if (n.get<doubles>(kXs).empty()) {
          // ImPlot's values overload, as wish does: bars at 0, 1, 2, ... with
          // ys as heights (or, horizontal, as lengths).
          ImPlot::PlotBars(label, ys.data(), count, n.get<double>(kExtra), 0.0, spec);
        } else {
          ImPlot::PlotBars(label, xs.data(), ys.data(), count, n.get<double>(kExtra), spec);
        }
        break;
    }
    if (kind_ != xy_kind::bars && kind_ != xy_kind::bars_h) {
      offer_nearest_point(dc, n, label, xs.data(), ys.data(), count);
    }
  }

 private:
  xy_kind kind_;
};

// ── Histograms ───────────────────────────────────────────────────────────────

const char* const kBinsHelp = "-1 = square root, -2 = Sturges, -3 = Rice, -4 = Scott (ImPlotBin), >0 = explicit count.";

class histogram_class : public series_class {
 public:
  static constexpr size_t values = item_field::first_own, bins = values + 1, cumulative = values + 2,
                          density = values + 3, range_min = values + 4, range_max = values + 5;

  histogram_class()
      : series_class(
            "PlotHistogram",
            {
                numbers_field("values", "Array of sample values to bin into the histogram."),
                int_field("bins", -1, std::string("Number of bins.  ") + kBinsHelp, value_range{-4, 100000}),
                bool_field("cumulative", false, "When true, renders a cumulative distribution instead."),
                bool_field("density", false, "When true, normalizes bar heights to form a probability density."),
                number_field(
                    "range_min",
                    0.0,
                    "Minimum sample value to include.  When range_min == range_max (default 0/0), ImPlot auto-ranges."),
                number_field(
                    "range_max",
                    0.0,
                    "Maximum sample value to include.  When range_min == range_max (default 0/0), ImPlot auto-ranges."),
            },
            "Draws a 1-D frequency histogram of the values array.") {}

  void check(const node& n) const override {
    require_values(n, values);
    if (n.get<int64_t>(bins) == 0) {
      fail_at(n, bins, "cannot be 0; " + std::string(kBinsHelp));
    }
  }

  void plot(const node& n, draw_context&) const override {
    ImPlotSpec spec = item_spec(n);
    spec.Flags = (n.get<bool>(cumulative) ? ImPlotHistogramFlags_Cumulative : 0) |
        (n.get<bool>(density) ? ImPlotHistogramFlags_Density : 0);
    ImPlotRange range;
    if (n.get<double>(range_min) != n.get<double>(range_max)) {
      range = ImPlotRange(n.get<double>(range_min), n.get<double>(range_max));
    }
    const doubles& v = n.get<doubles>(values);
    ImPlot::PlotHistogram(
        str(n, item_field::label), v.data(), count_of(v), static_cast<int>(n.get<int64_t>(bins)), 1.0, range, spec);
  }
};

class histogram2d_class : public series_class {
 public:
  static constexpr size_t xs = item_field::first_own, ys = xs + 1, x_bins = xs + 2, y_bins = xs + 3;

  histogram2d_class()
      : series_class(
            "PlotHistogram2D",
            {
                numbers_field("xs", "Array of sample X coordinates."),
                numbers_field("ys", "Array of sample Y coordinates (must match xs in length)."),
                int_field("x_bins", -1, std::string("Bin count for X.  ") + kBinsHelp, value_range{-4, 100000}),
                int_field("y_bins", -1, std::string("Bin count for Y.  ") + kBinsHelp, value_range{-4, 100000}),
            },
            "Draws a 2-D frequency heatmap of the (xs, ys) sample pairs.") {}

  void check(const node& n) const override {
    require_values(n, xs);
    require_values(n, ys);
    require_same_length(n, ys, xs);
    for (size_t f : {x_bins, y_bins}) {
      if (n.get<int64_t>(f) == 0) {
        fail_at(n, f, "cannot be 0; " + std::string(kBinsHelp));
      }
    }
  }

  void plot(const node& n, draw_context&) const override {
    const doubles& x = n.get<doubles>(xs);
    const doubles& y = n.get<doubles>(ys);
    // The range is always passed explicitly: ImPlot v1.0's PlotHistogram2D
    // computes a missing X range wrongly (both ends written to X.Min), so
    // every count is 0 and the grid draws in one colour. wish has the bug.
    const ImPlotRect range(data_range(x).Min, data_range(x).Max, data_range(y).Min, data_range(y).Max);
    ImPlot::PlotHistogram2D(
        str(n, item_field::label),
        x.data(),
        y.data(),
        count_of(x),
        static_cast<int>(n.get<int64_t>(x_bins)),
        static_cast<int>(n.get<int64_t>(y_bins)),
        range);
  }

 private:
  /// Finite min / max of `v`, widened by 0.5 each way when they are equal
  /// (a zero-size range would make the bin size zero).
  static ImPlotRange data_range(const doubles& v) {
    double lo = 0.0;
    double hi = 0.0;
    bool any = false;
    for (double d : v) {
      if (std::isfinite(d)) {
        lo = any ? std::min(lo, d) : d;
        hi = any ? std::max(hi, d) : d;
        any = true;
      }
    }
    if (lo == hi) {
      lo -= 0.5;
      hi += 0.5;
    }
    return ImPlotRange(lo, hi);
  }
};

// ── Heatmap ──────────────────────────────────────────────────────────────────

class heatmap_class : public series_class {
 public:
  static constexpr size_t values = item_field::first_own, rows = values + 1, cols = values + 2, scale_min = values + 3,
                          scale_max = values + 4, format = values + 5, x_min = values + 6, x_max = values + 7,
                          y_min = values + 8, y_max = values + 9;

  heatmap_class()
      : series_class(
            "PlotHeatmap",
            {
                numbers_field("values", "Flattened row-major data array of size rows x cols."),
                int_field("rows", 1, "Number of rows in the 2-D grid.", value_range{1, 10000}),
                int_field("cols", 1, "Number of columns in the 2-D grid.", value_range{1, 10000}),
                number_field("scale_min", 0.0, "Value mapped to the low end of the colormap."),
                number_field("scale_max", 1.0, "Value mapped to the high end of the colormap."),
                double_format_field(
                    "format",
                    "%.1f",
                    "printf format for per-cell value labels; empty string disables cell labels."),
                number_field("x_min", 0.0, "X coordinate of the left edge of the heatmap bounds."),
                number_field("x_max", 1.0, "X coordinate of the right edge of the heatmap bounds."),
                number_field("y_min", 0.0, "Y coordinate of the bottom edge of the heatmap bounds."),
                number_field("y_max", 1.0, "Y coordinate of the top edge of the heatmap bounds."),
            },
            "Draws a 2-D heatmap from a flat row-major values array, inside a Plot whose axes match its bounds.") {}

  void check(const node& n) const override {
    const size_t want = static_cast<size_t>(n.get<int64_t>(rows) * n.get<int64_t>(cols));
    const size_t have = n.get<doubles>(values).size();
    if (have != want) {
      fail_at(
          n,
          values,
          "has " + std::to_string(have) + " values; rows x cols needs " + std::to_string(want) + " (" +
              std::to_string(n.get<int64_t>(rows)) + " x " + std::to_string(n.get<int64_t>(cols)) + ")");
    }
  }

  void plot(const node& n, draw_context&) const override {
    const std::string& fmt = n.get<std::string>(format);
    ImPlot::PlotHeatmap(
        str(n, item_field::label),
        n.get<doubles>(values).data(),
        static_cast<int>(n.get<int64_t>(rows)),
        static_cast<int>(n.get<int64_t>(cols)),
        n.get<double>(scale_min),
        n.get<double>(scale_max),
        fmt.empty() ? nullptr : fmt.c_str(),
        ImPlotPoint(n.get<double>(x_min), n.get<double>(y_min)),
        ImPlotPoint(n.get<double>(x_max), n.get<double>(y_max)));
  }
};

// ── Pie chart ────────────────────────────────────────────────────────────────

std::vector<std::string> split_lines(const std::string& text) {
  std::vector<std::string> out;
  size_t pos = 0;
  for (;;) {
    const size_t end = text.find('\n', pos);
    out.push_back(text.substr(pos, end == std::string::npos ? std::string::npos : end - pos));
    if (end == std::string::npos) {
      return out;
    }
    pos = end + 1;
  }
}

class pie_class : public series_class {
 public:
  static constexpr size_t labels = item_field::first_own, values = labels + 1, x = labels + 2, y = labels + 3,
                          radius = labels + 4, normalize = labels + 5, label_fmt = labels + 6, angle0 = labels + 7;

  pie_class()
      : series_class(
            "PlotPieChart",
            {
                text_field("labels", "", "Newline-separated list of slice labels, one per entry in the values array."),
                numbers_field(
                    "values",
                    "Array of slice magnitudes.  When normalize is false, values are used as-is; when true, they are "
                    "normalized to sum to 1."),
                number_field(
                    "x",
                    0.5,
                    "X coordinate of the pie center in plot (data) units [0, 1].",
                    value_range{0, 1}),
                number_field(
                    "y",
                    0.5,
                    "Y coordinate of the pie center in plot (data) units [0, 1].",
                    value_range{0, 1}),
                number_field("radius", 0.4, "Pie radius in plot (data) units.", value_range{0, 1}),
                bool_field(
                    "normalize",
                    false,
                    "When true, slices are proportional to their values relative to the total sum."),
                double_format_field("label_fmt", "%.1f%%", "printf format for the per-slice value label."),
                number_field(
                    "angle0",
                    90.0,
                    "Angle in degrees at which the first slice begins (0 = 3 o'clock, 90 = 12 o'clock).",
                    value_range{0, 360}),
            },
            "Draws a pie chart.  Place inside a Plot with x_flags and y_flags NoDecorations, and axis limits "
            "[0,1] x [0,1] for correct scaling.") {}

  void check(const node& n) const override {
    require_values(n, values);
    const size_t slices = n.get<doubles>(values).size();
    const size_t names = split_lines(n.get<std::string>(labels)).size();
    if (n.get<std::string>(labels).empty() || names != slices) {
      fail_at(
          n,
          labels,
          "must name each slice, one per line or a $column (" + std::to_string(slices) + " values, " +
              std::to_string(n.get<std::string>(labels).empty() ? 0 : names) + " labels)");
    }
  }

  void plot(const node& n, draw_context&) const override {
    const std::vector<std::string> names = split_lines(n.get<std::string>(labels));
    std::vector<const char*> ids;
    for (const std::string& s : names) {
      ids.push_back(s.c_str());
    }
    ImPlotSpec spec;
    spec.Flags = n.get<bool>(normalize) ? ImPlotPieChartFlags_Normalize : 0;
    const doubles& v = n.get<doubles>(values);
    ImPlot::PlotPieChart(
        ids.data(),
        v.data(),
        count_of(v),
        n.get<double>(x),
        n.get<double>(y),
        n.get<double>(radius),
        str(n, label_fmt),
        n.get<double>(angle0),
        spec);
  }
};

// ── Annotations ──────────────────────────────────────────────────────────────

class text_class : public series_class {
 public:
  static constexpr size_t text = item_field::first_own, x = text + 1, y = text + 2, offset_x = text + 3,
                          offset_y = text + 4;

  text_class()
      : series_class(
            "PlotText",
            {
                text_field("text", "", "The string to display at the (x, y) plot coordinate."),
                number_field("x", 0.0, "X coordinate in plot (data) space."),
                number_field("y", 0.0, "Y coordinate in plot (data) space."),
                number_field("offset_x", 0.0, "Horizontal pixel offset applied after coordinate projection."),
                number_field("offset_y", 0.0, "Vertical pixel offset applied after coordinate projection."),
                text_style_fields()[0],
                text_style_fields()[1],
                text_style_fields()[2],
            },
            "Draws a text string at a plot coordinate.") {}

  void check(const node& n) const override {
    if (n.get<std::string>(text).empty()) {
      fail_at(n, text, "is empty: nothing to draw");
    }
  }

  void plot(const node& n, draw_context&) const override {
    ImPlot::PlotText(
        str(n, text),
        n.get<double>(x),
        n.get<double>(y),
        ImVec2(static_cast<float>(n.get<double>(offset_x)), static_cast<float>(n.get<double>(offset_y))));
  }
};

class inf_lines_class : public series_class {
 public:
  static constexpr size_t values = item_field::first_own, horizontal = values + 1;

  inf_lines_class()
      : series_class(
            "PlotInfLines",
            {
                numbers_field(
                    "values",
                    "Array of axis positions at which to draw lines.  When horizontal is false these are X positions "
                    "(vertical lines); when horizontal is true these are Y positions (horizontal lines)."),
                bool_field(
                    "horizontal",
                    false,
                    "When false, draws vertical lines at each X value.  When true, draws horizontal lines at each Y "
                    "value."),
            },
            "Draws infinite reference lines across the plot.") {}

  void check(const node& n) const override {
    require_values(n, values);
  }

  void plot(const node& n, draw_context&) const override {
    ImPlotSpec spec = item_spec(n);
    spec.Flags = n.get<bool>(horizontal) ? ImPlotInfLinesFlags_Horizontal : 0;
    const doubles& v = n.get<doubles>(values);
    ImPlot::PlotInfLines(str(n, item_field::label), v.data(), count_of(v), spec);
  }
};

class annotation_class : public series_class {
 public:
  static constexpr size_t text = item_field::first_own, x = text + 1, y = text + 2, offset_x = text + 3,
                          offset_y = text + 4, clamp = text + 5;

  annotation_class()
      : series_class(
            "PlotAnnotation",
            {
                text_field("text", "", "Text in the callout box. Empty (default) shows the point's coordinates."),
                number_field("x", 0.0, "X coordinate of the point the callout marks, in plot (data) space."),
                number_field("y", 0.0, "Y coordinate of the point the callout marks, in plot (data) space."),
                number_field(
                    "offset_x",
                    10.0,
                    "Horizontal position of the box from the point, in pixels (positive is right)."),
                number_field(
                    "offset_y",
                    -10.0,
                    "Vertical position of the box from the point, in pixels (negative is up)."),
                bool_field("clamp", true, "When true, keeps the box inside the plot area."),
            },
            "Draws a callout box with text next to a point, to point out an event or a value. Without a color it "
            "takes the colour of the series drawn just before it (no background if none); its text is black or "
            "white, whichever reads better.") {}

  void plot(const node& n, draw_context&) const override {
    const ImVec4 col = color_or_last_item(n);
    const ImVec2 offset(static_cast<float>(n.get<double>(offset_x)), static_cast<float>(n.get<double>(offset_y)));
    const std::string& t = n.get<std::string>(text);
    if (t.empty()) {
      ImPlot::Annotation(n.get<double>(x), n.get<double>(y), col, offset, n.get<bool>(clamp));
    } else {
      ImPlot::Annotation(n.get<double>(x), n.get<double>(y), col, offset, n.get<bool>(clamp), "%s", t.c_str());
    }
  }
};

const name_table& tag_axis_names() {
  static const name_table t{{"X", 0}, {"Y", 1}};
  return t;
}

class tag_class : public series_class {
 public:
  static constexpr size_t value = item_field::first_own, axis = value + 1, text = value + 2;

  tag_class()
      : series_class(
            "PlotTag",
            {
                number_field("value", 0.0, "Position of the tag on its axis, in data units."),
                enum_field(
                    "axis",
                    1,
                    tag_axis_names(),
                    "Axis the tag sits on: Y (default; the left axis, or the right one with y_axis: Y2) or X."),
                text_field(
                    "text",
                    "",
                    "Text in the tag. Empty (default) shows the value, formatted like the tick labels."),
            },
            "Draws a tag on an axis: a coloured box at a value, among the tick labels. Pair it with PlotInfLines "
            "to label a target or a threshold. Without a color it takes the colour of the series drawn just "
            "before it (no background if none).") {}

  void plot(const node& n, draw_context&) const override {
    const ImVec4 col = color_or_last_item(n);
    const double v = n.get<double>(value);
    const std::string& t = n.get<std::string>(text);
    const bool on_x = n.get<int64_t>(axis) == 0;
    if (t.empty() && on_x) {
      ImPlot::TagX(v, col);
    } else if (t.empty()) {
      ImPlot::TagY(v, col);
    } else if (on_x) {
      ImPlot::TagX(v, col, "%s", t.c_str());
    } else {
      ImPlot::TagY(v, col, "%s", t.c_str());
    }
  }
};

// ── Error bars and bar groups ────────────────────────────────────────────────

class error_bars_class : public series_class {
 public:
  static constexpr size_t xs = item_field::first_own, ys = xs + 1, err = xs + 2, neg = xs + 3, pos = xs + 4,
                          horizontal = xs + 5, cap_size = xs + 6;

  error_bars_class()
      : series_class(
            "PlotErrorBars",
            {
                numbers_field("xs", "X coordinates, one per point. If empty, 0, 1, 2, ... are used."),
                numbers_field("ys", "Y coordinates the bars are centred on, one per point."),
                numbers_field(
                    "err",
                    "Symmetric error of each point: its bar spans ys - err to ys + err (xs when horizontal). Leave "
                    "empty and give neg and pos for asymmetric errors."),
                numbers_field("neg", "Error below each point (left when horizontal); asymmetric errors, with pos."),
                numbers_field("pos", "Error above each point (right when horizontal); asymmetric errors, with neg."),
                bool_field("horizontal", false, "When true, the bars are horizontal: the errors apply to xs."),
                number_field("cap_size", 4.0, "Width of the cap at each end of a bar, in pixels.", value_range{0, 64}),
            },
            "Draws error bars at each (xs[i], ys[i]): a line spanning the error, with caps. Without a color it "
            "takes the colour of the series drawn just before it, so write the PlotLine, PlotScatter or PlotBars "
            "of the same data first.") {}

  void check(const node& n) const override {
    require_values(n, ys);
    require_same_length(n, xs, ys);
    const bool symmetric = !n.get<doubles>(err).empty();
    const bool has_neg = !n.get<doubles>(neg).empty();
    const bool has_pos = !n.get<doubles>(pos).empty();
    if (symmetric && (has_neg || has_pos)) {
      fail_at(n, has_neg ? neg : pos, "cannot be used with err: give err, or neg and pos");
    }
    if (!symmetric && !has_neg && !has_pos) {
      fail_at(n, err, "is empty: give err, or neg and pos");
    }
    if (symmetric) {
      require_same_length(n, err, ys);
      return;
    }
    if (!has_neg || !has_pos) {
      fail_at(n, has_neg ? pos : neg, "is empty: asymmetric errors need both neg and pos");
    }
    require_same_length(n, neg, ys);
    require_same_length(n, pos, ys);
  }

  void plot(const node& n, draw_context& dc) const override {
    const doubles& y = n.get<doubles>(ys);
    const doubles generated = n.get<doubles>(xs).empty() ? index_xs(y.size()) : doubles{};
    const doubles& x = generated.empty() ? n.get<doubles>(xs) : generated;
    ImPlotSpec spec = item_spec(n);
    if (n.get<std::string>(item_field::color).empty()) {
      // The previous series' colour; ImPlot's error-bar colour when none.
      if (const ImVec4 last = ImPlot::GetLastItemColor(); last.w > 0.0f) {
        spec.LineColor = last;
      }
    }
    spec.Size = static_cast<float>(n.get<double>(cap_size));
    spec.Flags = n.get<bool>(horizontal) ? ImPlotErrorBarsFlags_Horizontal : 0;
    const char* label = str(n, item_field::label);
    ImPlotContext& gp = *ImPlot::GetCurrentContext();
    const int colormap_idx = gp.CurrentItems->ColormapIdx;
    if (const doubles& e = n.get<doubles>(err); !e.empty()) {
      ImPlot::PlotErrorBars(label, x.data(), y.data(), e.data(), count_of(y), spec);
    } else {
      ImPlot::PlotErrorBars(
          label, x.data(), y.data(), n.get<doubles>(neg).data(), n.get<doubles>(pos).data(), count_of(y), spec);
    }
    // The error bars usually sit on a line's points: win the tie, and say
    // the error too.
    const bool vertical = !n.get<bool>(horizontal);
    offer_nearest_point(
        dc,
        n,
        label,
        x.data(),
        y.data(),
        count_of(y),
        [&](int i, pick& p) {
          const ImAxis axis = vertical ? ImPlot::GetCurrentPlot()->CurrentY : ImPlot::GetCurrentPlot()->CurrentX;
          if (const doubles& e = n.get<doubles>(err); !e.empty()) {
            p.values.push_back({"error", "+/-" + format_axis_value(axis, e[i])});
          } else {
            p.values.push_back(
                {"error",
                 "-" + format_axis_value(axis, n.get<doubles>(neg)[i]) + " / +" +
                     format_axis_value(axis, n.get<doubles>(pos)[i])});
          }
        },
        0.5f);
    // ImPlot v1.0 gives a new error-bar item the next colormap colour, then
    // draws it in spec.LineColor (or the text colour). Give that colormap
    // colour back to the next series, and make the item's colour (legend
    // icon, GetLastItemColor()) the one it is drawn in.
    gp.CurrentItems->ColormapIdx = colormap_idx;
    if (gp.PreviousItem != nullptr) {
      const ImVec4 drawn =
          ImPlot::IsColorAuto(spec.LineColor) ? ImGui::GetStyleColorVec4(ImGuiCol_Text) : spec.LineColor;
      gp.PreviousItem->Color = ImGui::ColorConvertFloat4ToU32(drawn);
    }
  }
};

class bar_groups_class : public series_class {
 public:
  static constexpr size_t labels = item_field::first_own, values = labels + 1, group_size = labels + 2,
                          shift = labels + 3, horizontal = labels + 4, stacked = labels + 5;

  bar_groups_class()
      : series_class(
            "PlotBarGroups",
            {
                texts_field("labels", "Name of each series of bars (its legend entry), one per bar in a group."),
                numbers_field(
                    "values",
                    "Bar values, one row per group: the first row (one value per label) is group 0, the next "
                    "group 1, and so on. [$a, $b] gives this layout from the columns a and b."),
                number_field(
                    "group_size",
                    0.67,
                    "Width of a group (all its bars together), in plot units.",
                    value_range{0, 16}),
                number_field("shift", 0.0, "Offset of every group along the axis, in plot units."),
                bool_field("horizontal", false, "When true, the bars are horizontal and the groups go up the Y axis."),
                bool_field(
                    "stacked",
                    false,
                    "When true, a group's bars are stacked into one bar instead of side by side."),
            },
            "Draws groups of bars, one bar per label in each group, coloured in turn from the plot's colormap "
            "(color and fill_color are ignored). Groups sit at 0, 1, 2, ...: name them with the Plot's x_ticks "
            "and x_tick_labels (y_ when horizontal).") {}

  void check(const node& n) const override {
    const size_t names = n.get<std::vector<std::string>>(labels).size();
    if (names == 0) {
      fail_at(n, labels, "is empty: name each series of bars");
    }
    require_values(n, values);
    const size_t count = n.get<doubles>(values).size();
    if (count % names != 0) {
      fail_at(
          n,
          values,
          "has " + std::to_string(count) + " values, not a whole number of groups of " + std::to_string(names) +
              " (one value per label in each group)");
    }
  }

  void plot(const node& n, draw_context&) const override {
    const auto& names = n.get<std::vector<std::string>>(labels);
    const doubles& v = n.get<doubles>(values);
    const size_t items = names.size();
    const size_t groups = v.size() / items;
    // ImPlot wants one row per item (label); the source has one per group.
    doubles by_item(v.size());
    for (size_t g = 0; g < groups; ++g) {
      for (size_t i = 0; i < items; ++i) {
        by_item[i * groups + g] = v[g * items + i];
      }
    }
    std::vector<const char*> ids;
    for (const std::string& s : names) {
      ids.push_back(s.c_str());
    }
    ImPlotSpec spec = item_spec(n);
    const ImPlotSpec automatic;
    spec.LineColor = automatic.LineColor; // each label takes its own colormap colour
    spec.FillColor = automatic.FillColor;
    spec.Flags = (n.get<bool>(horizontal) ? ImPlotBarGroupsFlags_Horizontal : 0) |
        (n.get<bool>(stacked) ? ImPlotBarGroupsFlags_Stacked : 0);
    ImPlot::PlotBarGroups(
        ids.data(),
        by_item.data(),
        static_cast<int>(items),
        static_cast<int>(groups),
        n.get<double>(group_size),
        n.get<double>(shift),
        spec);
  }
};

// ── ColorScale ───────────────────────────────────────────────────────────────

const name_table& color_scale_flag_names() {
  static const name_table t{
      {"None", ImPlotColormapScaleFlags_None},
      {"NoLabel", ImPlotColormapScaleFlags_NoLabel},
      {"Opposite", ImPlotColormapScaleFlags_Opposite},
      {"Invert", ImPlotColormapScaleFlags_Invert},
  };
  return t;
}

class color_scale_class : public element_class {
 public:
  static constexpr size_t label = 0, scale_min = 1, scale_max = 2, colormap = 3, format = 4, flags = 5, width = 6,
                          height = 7;

  color_scale_class()
      : element_class(
            "ColorScale",
            std::string(kFigureSlot),
            {},
            with_text_style({
                text_field("label", "", "Label along the scale (rotated). Empty (default) for none."),
                number_field("scale_min", 0.0, "Value at the low end of the colormap; match the heatmap's."),
                number_field("scale_max", 1.0, "Value at the high end of the colormap; match the heatmap's."),
                colormap_field(),
                double_format_field("format", "%g", "printf format of the tick labels."),
                flags_field(
                    "flags",
                    0,
                    color_scale_flag_names(),
                    "NoLabel hides the label, Opposite puts the ticks on the left, Invert puts the low end at "
                    "the top."),
                number_field(
                    "width",
                    0.0,
                    "Width in pixels; 0 (default) is the natural width, enough for the tick labels.",
                    value_range{-1000, 8192}),
                number_field(
                    "height",
                    300.0,
                    "Height in pixels; the default 300 matches a Plot's, so the two line up side by side (give both "
                    "the same height). A ColorScale used as root with no height fills the image.",
                    value_range{-1000, 8192}),
            }),
            "A colour bar: the colormap from scale_min to scale_max with ticks, to read the values of a heatmap "
            "or surface. Put it beside the plot in a HorizontalLayout and give it the plot's colormap.") {}

  void adjust_root(node& n, const std::vector<bool>& set) const override {
    if (!set[height]) {
      n.values[height] = -1.0; // fill the image, as a root Plot does
    }
  }

  void check(const node& n) const override {
    if (n.get<double>(scale_min) == n.get<double>(scale_max)) {
      fail_at(n, scale_max, "must differ from scale_min");
    }
  }

  /// Natural size: the bar, the widest tick label ImPlot may draw, the
  /// label and ImPlot's padding; 300 high, like a Plot.
  ImVec2 measure(const node& n, draw_context&, float) const override {
    const double lo = n.get<double>(scale_min);
    const double hi = n.get<double>(scale_max);
    float widest = 0.0f;
    for (int k = 0; k <= 40; ++k) {
      char text[64];
      std::snprintf(text, sizeof(text), str(n, format), lo + (hi - lo) * k / 40.0);
      widest = std::max(widest, ImGui::CalcTextSize(text).x);
    }
    const ImPlotStyle& style = ImPlot::GetStyle();
    float w = widest + kBarWidth + 2.0f * style.PlotPadding.x + 2.0f * style.LabelPadding.x + 4.0f;
    const std::string& l = n.get<std::string>(label);
    if (!l.empty() && (n.get<int64_t>(flags) & ImPlotColormapScaleFlags_NoLabel) == 0) {
      w += ImGui::GetTextLineHeight() + style.LabelPadding.x;
    }
    return ImVec2(std::ceil(w), 300.0f);
  }

  void draw(const node& n, draw_context& dc) const override {
    const rect r = dc.rect_of(n);
    const std::string& l = n.get<std::string>(label);
    ImGui::SetCursorScreenPos(r.min);
    ImGui::PushClipRect(r.min, r.max, true);
    // Width 0: ImPlot sizes the bar for its own tick labels.
    ImPlot::ColormapScale(
        l.empty() ? "##scale" : l.c_str(),
        n.get<double>(scale_min),
        n.get<double>(scale_max),
        ImVec2(0.0f, r.height()),
        str(n, format),
        static_cast<ImPlotColormapScaleFlags>(n.get<int64_t>(flags)),
        static_cast<ImPlotColormap>(n.get<int64_t>(colormap)));
    ImGui::PopClipRect();
  }

 private:
  static constexpr float kBarWidth = 20.0f;
};

} // namespace

void register_plot2d(registry& reg) {
  reg.add(std::make_unique<plot_class>());
  reg.add(std::make_unique<xy_series>(
      "PlotLine", xy_kind::line, xy_fields({}), "Draws a connected line through the (xs, ys) data points."));
  reg.add(std::make_unique<xy_series>(
      "PlotScatter",
      xy_kind::scatter,
      xy_fields({}),
      "Draws a marker at each (xs[i], ys[i]) coordinate without connecting lines."));
  reg.add(std::make_unique<xy_series>(
      "PlotStairs", xy_kind::stairs, xy_fields({}), "Draws a staircase (step-interpolated) line through the data."));
  reg.add(std::make_unique<xy_series>(
      "PlotStems",
      xy_kind::stems,
      xy_fields({number_field("ref", 0.0, "Y coordinate of the baseline from which stems grow.")}),
      "Draws vertical stems from the reference level to each (xs[i], ys[i])."));
  reg.add(std::make_unique<xy_series>(
      "PlotShaded",
      xy_kind::shaded,
      xy_fields({
          numbers_field(
              "ys2", "Second Y array for a band between ys and ys2.  Leave empty to shade between ys and ref."),
          number_field("ref", 0.0, "Horizontal reference line used when ys2 is empty."),
      }),
      "Draws a shaded region between two curves, or between ys and a reference."));
  reg.add(std::make_unique<xy_series>(
      "PlotDigital",
      xy_kind::digital,
      xy_fields({}),
      "Draws a digital signal trace: xs = timestamps, ys = 0 or 1. As in ImPlot, a trace is drawn at the "
      "bottom of the plot, 4 pixels tall per unit of ys, independent of the y axis (logic-analyser style)."));
  reg.add(std::make_unique<xy_series>(
      "PlotBars",
      xy_kind::bars,
      bar_fields(),
      "Draws vertical bars at each xs[i] with height ys[i].  If xs is empty, bars are placed at integer positions "
      "0, 1, 2, ..."));
  reg.add(std::make_unique<xy_series>(
      "PlotBarsH", xy_kind::bars_h, bar_fields(), "Draws horizontal bars at each ys[i] with length xs[i]."));
  reg.add(std::make_unique<histogram_class>());
  reg.add(std::make_unique<histogram2d_class>());
  reg.add(std::make_unique<heatmap_class>());
  reg.add(std::make_unique<pie_class>());
  reg.add(std::make_unique<text_class>());
  reg.add(std::make_unique<inf_lines_class>());
  reg.add(std::make_unique<annotation_class>());
  reg.add(std::make_unique<tag_class>());
  reg.add(std::make_unique<error_bars_class>());
  reg.add(std::make_unique<bar_groups_class>());
  reg.add(std::make_unique<color_scale_class>());
}

} // namespace bdg::nymph
