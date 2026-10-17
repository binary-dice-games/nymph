// MIT License © 2026 Binary Dice Games
/// @file plot_style.hpp
/// @brief Fields and helpers shared by the 2D and 3D plot elements: the
///        series base fields (label and style) and the plot colormap.
///
/// Port of wish's `plot_style_fields.hpp` (schema) and
/// `imgui_plot_style.hpp` (applying it to ImPlotSpec / ImPlot3DSpec, whose
/// members have the same names).
#pragma once

#include <imgui.h>
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

/// @brief Field indices every series shares, in this order, before its own.
namespace item_field {
inline constexpr size_t label = 0;
inline constexpr size_t color = 1;
inline constexpr size_t fill_color = 2;
inline constexpr size_t line_weight = 3;
inline constexpr size_t fill_alpha = 4;
inline constexpr size_t marker = 5;
inline constexpr size_t marker_size = 6;
inline constexpr size_t first_own = 7; ///< Index of a series type's first own field.
} // namespace item_field

/// @brief The shared series fields (`label`, then the style fields), with
///        `own` appended.
std::vector<field_desc> series_fields(std::vector<field_desc> own);

/// @brief The `colormap` field of `Plot` / `Plot3D`.
field_desc colormap_field();

/// @brief "#RRGGBB" / "#RRGGBBAA" (already validated) to a colour.
ImVec4 parse_hex_color(const std::string& text);

/// @brief Copies the series style fields of `n` into an ImPlotSpec or
///        ImPlot3DSpec. Only non-automatic values are written, so a series
///        that sets none keeps the library's own defaults. `Flags` is not
///        touched.
template <typename Spec>
void apply_item_style(Spec& spec, const node& n) {
  if (const std::string& color = n.get<std::string>(item_field::color); !color.empty()) {
    spec.LineColor = parse_hex_color(color);
    spec.FillColor = spec.LineColor;
  }
  if (const std::string& fill = n.get<std::string>(item_field::fill_color); !fill.empty()) {
    spec.FillColor = parse_hex_color(fill);
  }
  if (const double weight = n.get<double>(item_field::line_weight); weight > 0.0) {
    spec.LineWeight = static_cast<float>(weight);
  }
  if (const double alpha = n.get<double>(item_field::fill_alpha); alpha >= 0.0) {
    spec.FillAlpha = static_cast<float>(alpha);
  }
  if (const int64_t marker = n.get<int64_t>(item_field::marker); marker != kMarkerDefault) {
    spec.Marker = static_cast<int>(marker);
  }
  if (const double size = n.get<double>(item_field::marker_size); size > 0.0) {
    spec.MarkerSize = static_cast<float>(size);
  }
}

} // namespace bdg::nymph
