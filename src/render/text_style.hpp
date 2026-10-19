// MIT License © 2026 Binary Dice Games
/// @file text_style.hpp
/// @brief Font, font size and text colour: the `image` defaults and the
///        per-element overrides, inherited down the element tree
///        (spec/01_core_features/DESIGN.md section 3, "Text style").
#pragma once

#include <string>
#include <vector>
#include "render/draw_context.hpp"
#include "schema/field_desc.hpp"
#include "schema/node.hpp"

namespace bdg::nymph {

/// @brief A text style; an empty / 0 member inherits the enclosing one.
struct text_style {
  std::string font; ///< Embedded font file name, "default", or "" (inherit).
  double font_size = 0.0; ///< Logical pixels; 0 inherits.
  std::string color; ///< "#RRGGBB[AA]", or "" (inherit).

  bool empty() const {
    return font.empty() && font_size <= 0.0 && color.empty();
  }
};

/// @brief Largest `font_size` accepted, in logical pixels.
inline constexpr double kMaxFontSize = 96.0;

/// @brief The `font`, `font_size` and `text_color` fields an element class
///        appends to take part in text styling. text_style_of() finds them
///        by name, so any class with them is styled.
/// @param with_color False for a class that already has its own
///        `text_color` field (`Label`).
std::vector<field_desc> text_style_fields(bool with_color = true);

/// @brief `fields` followed by text_style_fields(with_color).
inline std::vector<field_desc> with_text_style(std::vector<field_desc> fields, bool with_color = true) {
  for (field_desc& f : text_style_fields(with_color)) {
    fields.push_back(std::move(f));
  }
  return fields;
}

/// @brief The style `n` sets: its `font`, `font_size` and `text_color`
///        fields, or an empty style for a class without them.
text_style text_style_of(const node& n);

/// @brief Applies a style to everything drawn or measured while it lives:
///        pushes the font (ImGui 1.92 PushFont with a size) and the text
///        colour of ImGui, ImUml, ImPlot (title, axis, inlay and legend text) and
///        ImPlot3D, and pops them on destruction. Grid and tick colours,
///        which ImPlot derives from the text colour by default, are kept as
///        they were. Does nothing without an ImGui context or for an empty
///        style.
class text_style_scope {
 public:
  text_style_scope(const text_style& style, const draw_context& dc);
  ~text_style_scope();
  text_style_scope(const text_style_scope&) = delete;
  text_style_scope& operator=(const text_style_scope&) = delete;

 private:
  bool font_ = false;
  int imgui_colors_ = 0;
  int implot_colors_ = 0;
  int implot3d_colors_ = 0;
  int imuml_colors_ = 0;
};

} // namespace bdg::nymph
