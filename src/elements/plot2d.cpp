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
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
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
}

class plot_class : public element_class {
 public:
  plot_class()
      : element_class(
            "Plot",
            std::string(kFigureSlot),
            {kPlot2dItemSlot},
            with_text_style({
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
            }),
            "A 2D plot (ImPlot). Children are 2D series: PlotLine, PlotBars, PlotScatter, ...") {}

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
      if (n.get<double>(x_min) != n.get<double>(x_max)) {
        ImPlot::SetupAxisLimits(ImAxis_X1, n.get<double>(x_min), n.get<double>(x_max), ImPlotCond_Always);
      }
      if (n.get<double>(y_min) != n.get<double>(y_max)) {
        ImPlot::SetupAxisLimits(ImAxis_Y1, n.get<double>(y_min), n.get<double>(y_max), ImPlotCond_Always);
      }
      const int64_t location = n.get<int64_t>(legend_location);
      const int64_t lflags = n.get<int64_t>(legend_flags);
      if (location >= 0 || lflags != 0) {
        ImPlot::SetupLegend(
            static_cast<ImPlotLocation>(location < 0 ? int64_t{ImPlotLocation_NorthWest} : location),
            static_cast<ImPlotLegendFlags>(lflags));
      }
      dc.draw_children(n);
      ImPlot::EndPlot();
    }
    if (colormap_value != kColormapDefault) {
      ImPlot::PopColormap();
    }
  }
};

// ── Series base ──────────────────────────────────────────────────────────────

class series_class : public element_class {
 public:
  series_class(std::string name, std::vector<field_desc> own, std::string description)
      : element_class(
            std::move(name),
            kPlot2dItemSlot,
            {},
            series_fields(std::move(own)),
            std::move(description) + "  Must be a child of a Plot element.") {}

 protected:
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
};

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

  void draw(const node& n, draw_context&) const override {
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

  void draw(const node& n, draw_context&) const override {
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

  void draw(const node& n, draw_context&) const override {
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

  void draw(const node& n, draw_context&) const override {
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

  void draw(const node& n, draw_context&) const override {
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

  void draw(const node& n, draw_context&) const override {
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

  void draw(const node& n, draw_context&) const override {
    ImPlotSpec spec = item_spec(n);
    spec.Flags = n.get<bool>(horizontal) ? ImPlotInfLinesFlags_Horizontal : 0;
    const doubles& v = n.get<doubles>(values);
    ImPlot::PlotInfLines(str(n, item_field::label), v.data(), count_of(v), spec);
  }
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
}

} // namespace bdg::nymph
