// MIT License © 2026 Binary Dice Games
/// @file plot_style.cpp
/// @brief Shared series fields, colormap field and colour parsing. Names,
///        defaults and descriptions are wish's (`plot_style_fields.hpp`).
#include "elements/plot_style.hpp"

namespace bdg::nymph {

const name_table& marker_names() {
  static const name_table t{
      {"Default", kMarkerDefault},
      {"None", -2},
      {"Auto", -1},
      {"Circle", 0},
      {"Square", 1},
      {"Diamond", 2},
      {"Up", 3},
      {"Down", 4},
      {"Left", 5},
      {"Right", 6},
      {"Cross", 7},
      {"Plus", 8},
      {"Asterisk", 9},
  };
  return t;
}

const name_table& colormap_names() {
  static const name_table t{
      {"Default", kColormapDefault},
      {"Deep", 0},
      {"Dark", 1},
      {"Pastel", 2},
      {"Paired", 3},
      {"Viridis", 4},
      {"Plasma", 5},
      {"Hot", 6},
      {"Cool", 7},
      {"Pink", 8},
      {"Jet", 9},
      {"Twilight", 10},
      {"RdBu", 11},
      {"BrBG", 12},
      {"PiYG", 13},
      {"Spectral", 14},
      {"Greys", 15},
  };
  return t;
}

std::vector<field_desc> series_fields(std::vector<field_desc> own) {
  std::vector<field_desc> fields{
      text_field("label", "", "Series name shown in the plot legend."),
      color_field(
          "color",
          "Series colour as \"#RRGGBB\" or \"#RRGGBBAA\", used for its lines, fills and markers. "
          "Empty (default) takes the next colour of the plot's colormap."),
      color_field(
          "fill_color",
          "Colour of filled areas (bar faces, shaded regions, surfaces) as \"#RRGGBB\" or \"#RRGGBBAA\". "
          "Empty (default) uses color."),
      number_field(
          "line_weight", 1.0, "Line thickness in pixels (lines, bar edges, marker edges).", value_range{0.0, 32.0}),
      number_field(
          "fill_alpha",
          -1.0,
          "Opacity of fills from 0 (transparent) to 1 (opaque). -1 (default) keeps the series type's own default.",
          value_range{-1.0, 1.0}),
      enum_field(
          "marker",
          kMarkerDefault,
          marker_names(),
          "Point marker: None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus or Asterisk. "
          "Default keeps the series type's own choice (markers on a scatter, none on a line)."),
      number_field(
          "marker_size", 0.0, "Marker radius in pixels. 0 (default) keeps the automatic size.", value_range{0.0, 64.0}),
  };
  for (field_desc& f : own) {
    fields.push_back(std::move(f));
  }
  return fields;
}

field_desc colormap_field() {
  return enum_field(
      "colormap",
      kColormapDefault,
      colormap_names(),
      "Colormap for this plot: the colours series take in turn when they set none, and the scale of heatmaps "
      "and surfaces. Deep (the usual default), Dark, Pastel, Paired, Viridis, Plasma, Hot, Cool, Pink, Jet, "
      "Twilight, RdBu, BrBG, PiYG, Spectral or Greys. Default leaves the current colormap.");
}

ImVec4 parse_hex_color(const std::string& text) {
  auto hex = [&](size_t i) {
    auto digit = [](char c) { return c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10; };
    return static_cast<float>(digit(text[i]) * 16 + digit(text[i + 1])) / 255.0f;
  };
  return ImVec4(hex(1), hex(3), hex(5), text.size() >= 9 ? hex(7) : 1.0f);
}

} // namespace bdg::nymph
