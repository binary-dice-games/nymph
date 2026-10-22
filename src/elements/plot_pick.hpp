// MIT License © 2026 Binary Dice Games
/// @file plot_pick.hpp
/// @brief Picks inside an open ImPlot plot: what the pointer is over, with
///        values formatted as the axes show them, and the tooltip
///        (spec/04_web_viewer/DESIGN.md section 4, "Interactive drawing").
///        Everything here does nothing unless the draw_context is
///        interactive, so PNG output never changes.
#pragma once

#include <imgui.h>
#include <implot.h>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "render/draw_context.hpp"
#include "schema/node.hpp"

struct ImPlotPlot;

namespace bdg::nymph {

/// @brief The pointer in the open plot's pixel space, when the figure is
///        interactive, the plot is hovered and series `label` (if not
///        null) is shown.
std::optional<ImVec2> plot_pointer(const draw_context& dc, const char* label);

/// @brief plot_pointer() in plot coordinates of the current axes.
std::optional<ImPlotPoint> plot_pointer_value(const draw_context& dc, const char* label);

/// @brief A UTC time in seconds as ISO 8601: "2026-03-02", with
///        " 12:00:00" when not midnight.
std::string format_iso_time(double seconds);

/// @brief `value` on axis `axis` of the open plot as its ticks show it: a
///        tick's own label when the value sits on a labelled tick, an ISO
///        8601 date on a time axis, else the axis format ("%g" by default).
std::string format_axis_value(ImAxis axis, double value);

/// @brief `label` without ImPlot's "##" id suffix.
std::string display_label(const char* label);

/// @brief Adds values to a pick of point `index` (e.g. an error).
using pick_extra = std::function<void(int index, pick& p)>;

/// @brief Offers the point of (xs, ys) nearest the pointer, against the
///        current axes, as a pick of `n` with values "x" and "y".
/// @param extra    Adds values for the chosen point; may be empty.
/// @param priority Subtracted from the distance, so a series drawn over
///                 the same points (error bars over a line) can win ties.
void offer_nearest_point(
    draw_context& dc,
    const node& n,
    const char* label,
    const double* xs,
    const double* ys,
    int count,
    const pick_extra& extra = {},
    float priority = 0.0f);

/// @brief Offers `hit` (distance 0, outlined) when the pointer is inside
///        the pixel rectangle `a`-`b` (any corner order).
void offer_rect_px(draw_context& dc, const ImVec2& pointer, pick hit, ImVec2 a, ImVec2 b, ImU32 color);

/// @brief One bar: its position along the category axis and its extent
///        along the value axis.
struct bar_shape {
  double pos = 0.0;
  double lo = 0.0;
  double hi = 0.0;
};

/// @brief Offers the bar under the pointer, of `count` bars of width
///        `size` (plot units) whose shape `shape(i)` gives; `values` fills
///        the pick's values. Vertical bars stand on the X axis; horizontal
///        ones lie along it.
void offer_bars(
    draw_context& dc,
    const node& n,
    const char* label,
    int count,
    double size,
    bool horizontal,
    const std::function<bar_shape(int)>& shape,
    const pick_extra& values,
    ImU32 color = 0);

/// @brief Draws the highlight and (when enabled) the tooltip of the best
///        candidate offered, on `dl`, and makes it the frame's hovered
///        pick. For elements drawn outside an ImPlot plot.
void finish_picks(draw_context& dc, ImDrawList* dl);

// ── Saved view: hidden legend entries ─────────────────────────────────────

/// @brief After an item labelled `label` is plotted in the current 2D
///        plot: hides it when the plot's saved view lists it and views
///        apply this frame (draw_context::apply_view()), then records it if
///        hidden, for the live view to save. Hiding after plotting leaves
///        ImPlot's colour assignment alone; it shows from the next frame,
///        and PNGs and fresh live views run several frames first.
void sync_hidden(draw_context& dc, const char* label);

/// @brief Drops the items of every 2D and 3D plot in the current ImPlot
///        and ImPlot3D contexts, and with them the colours ImPlot gave
///        them, so the next frame gives each series its colour as a fresh
///        render does. ImPlot keeps an item's colour for as long as the
///        item lives: without this a live view keeps showing a colour the
///        new source no longer sets. For the live view, on a new source.
/// @return The IDs of the items that were hidden, for
///         draw_context::set_rehidden().
std::vector<ImGuiID> forget_plot_items();

/// @brief Ends picking for the open plot: takes the best candidate, draws
///        its highlight ring and (when enabled) the tooltip, and makes it
///        the frame's hovered pick. Call before EndPlot().
void finish_plot_picks(draw_context& dc);

/// @brief The texts of a plot as ImPlot placed them.
struct plot_texts {
  const node* element = nullptr;
  std::string title; ///< "" for none
  std::string x_label;
  std::string y_label;
  std::string y2_label;
  bool has_y2 = false;
  std::string legend_field = "legend_location"; ///< The field of the legend's location; "" for none.
  /// Legend entries select the element's children that they name (series).
  bool element_children = true;
};

/// @brief In edit mode, records the plot's title, axis labels and legend
///        as edit targets (fields `title`, `x_label`, `y_label`,
///        `y2_label`, `legend_location`; spec/08_view_editor/DESIGN.md
///        section 4), and each legend entry as a target of the series it
///        names (its `label`; a pie chart's slice selects the chart). Call right after EndPlot(), with the pointer
///        ImPlot::GetCurrentPlot() returned before it.
void record_plot_texts(draw_context& dc, const plot_texts& texts, const ImPlotPlot* plot);

} // namespace bdg::nymph
