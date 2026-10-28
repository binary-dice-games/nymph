// MIT License © 2026 Binary Dice Games
/// @file plot_style.hpp
/// @brief Fields and helpers shared by the 2D and 3D plot elements: the
///        series base fields (label and style, each series type taking the
///        ones it draws with) and the plot colormap.
///
/// Port of wish's `plot_style_fields.hpp` (schema) and
/// `imgui_plot_style.hpp` (applying it to ImPlotSpec / ImPlot3DSpec, whose
/// members have the same names).
#pragma once

#include <imgui.h>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "schema/field_desc.hpp"
#include "schema/name_table.hpp"
#include "schema/node.hpp"

namespace bdg::nymph {

/// @brief `marker` value meaning "leave the series type's own default".
inline constexpr int64_t kMarkerDefault = -3;
/// @brief `colormap` value meaning "leave the current colormap".
inline constexpr int64_t kColormapDefault = -1;

/// @brief Names for `marker` (ImPlotMarker / ImPlot3DMarker values).
const name_table& marker_names();
/// @brief Names for `colormap` (ImPlotColormap / ImPlot3DColormap values).
const name_table& colormap_names();

/// @brief Which of the shared series fields a series type takes, as bits
///        of `style`. A type takes only the fields ImPlot uses when it
///        draws that type, so none is accepted and then ignored. The fields
///        it takes come first, in the order of the bits, before its own.
using series_style = unsigned;

namespace style {
inline constexpr series_style label = 1u << 0;
inline constexpr series_style color = 1u << 1;
inline constexpr series_style fill_color = 1u << 2;
inline constexpr series_style line_weight = 1u << 3;
inline constexpr series_style fill_alpha = 1u << 4;
inline constexpr series_style marker = 1u << 5;
inline constexpr series_style marker_size = 1u << 6;
/// Every shared field: series drawn with lines, markers and fills.
inline constexpr series_style all = (1u << 7) - 1;
/// Filled shapes with an edge and no markers (bars, digital traces).
inline constexpr series_style filled = label | color | fill_color | line_weight | fill_alpha;
} // namespace style

/// @brief Index of shared field `field` in a series type taking `s`
///        (`field` must be one of `s`).
constexpr size_t style_index(series_style s, series_style field) {
  return static_cast<size_t>(std::popcount(s & (field - 1)));
}

/// @brief Index of the first own field of a series type taking `s`.
constexpr size_t first_own(series_style s) {
  return static_cast<size_t>(std::popcount(s));
}

namespace item_field {
/// Index of `label`, in every series type that takes it (style::label).
inline constexpr size_t label = 0;
} // namespace item_field

/// @brief The shared series fields in `s` (`label`, then the style fields),
///        with `own` appended.
std::vector<field_desc> series_fields(series_style s, std::vector<field_desc> own);

/// @brief The `colormap` field of `Plot` / `Plot3D`.
field_desc colormap_field();

/// @brief "#RRGGBB" / "#RRGGBBAA" (already validated) to a colour.
ImVec4 parse_hex_color(const std::string& text);

/// @brief Copies the style fields of `n`, a series taking `s`, into an
///        ImPlotSpec or ImPlot3DSpec. Only non-automatic values are
///        written, so a series that sets none keeps the library's own
///        defaults. `fill_color` also colours marker faces, which ImPlot
///        otherwise fills with the line colour. `Flags` is not touched.
template <typename Spec>
void apply_item_style(Spec& spec, const node& n, series_style s) {
  const auto has = [&](series_style f) { return (s & f) != 0; };
  const auto at = [&](series_style f) { return style_index(s, f); };
  if (has(style::color)) {
    if (const std::string& color = n.get<std::string>(at(style::color)); !color.empty()) {
      spec.LineColor = parse_hex_color(color);
      spec.FillColor = spec.LineColor;
    }
  }
  if (has(style::fill_color)) {
    if (const std::string& fill = n.get<std::string>(at(style::fill_color)); !fill.empty()) {
      spec.FillColor = parse_hex_color(fill);
      spec.MarkerFillColor = spec.FillColor;
    }
  }
  if (has(style::line_weight)) {
    if (const double weight = n.get<double>(at(style::line_weight)); weight > 0.0) {
      spec.LineWeight = static_cast<float>(weight);
    }
  }
  if (has(style::fill_alpha)) {
    if (const double alpha = n.get<double>(at(style::fill_alpha)); alpha >= 0.0) {
      spec.FillAlpha = static_cast<float>(alpha);
    }
  }
  if (has(style::marker)) {
    if (const int64_t marker = n.get<int64_t>(at(style::marker)); marker != kMarkerDefault) {
      spec.Marker = static_cast<int>(marker);
    }
  }
  if (has(style::marker_size)) {
    if (const double size = n.get<double>(at(style::marker_size)); size > 0.0) {
      spec.MarkerSize = static_cast<float>(size);
    }
  }
}

} // namespace bdg::nymph
