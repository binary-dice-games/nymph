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
#include "render/draw_context.hpp"
#include "schema/node.hpp"

namespace bdg::nymph {

/// @brief The pointer in the open plot's pixel space, when the figure is
///        interactive, the plot is hovered and series `label` is shown.
std::optional<ImVec2> plot_pointer(const draw_context& dc, const char* label);

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

/// @brief Ends picking for the open plot: takes the best candidate, draws
///        its highlight ring and (when enabled) the tooltip, and makes it
///        the frame's hovered pick. Call before EndPlot().
void finish_plot_picks(draw_context& dc);

} // namespace bdg::nymph
