// MIT License © 2026 Binary Dice Games
/// @file text_style.cpp
/// @brief text_style_fields(), text_style_of(), text_style_scope.
#include "render/text_style.hpp"
#include <imgui.h>
#include <implot.h>
#include <implot_internal.h> // ImPlot::GetStyleColorVec4
#include <implot3d.h>
#include <imuml.h>
#include "elements/plot_style.hpp"
#include "fonts/embedded_fonts.hpp"
#include "schema/element_class.hpp"

namespace bdg::nymph {

std::vector<field_desc> text_style_fields(bool with_color) {
  field_desc font = text_field(
      "font",
      "",
      "Font of this element's text and of everything inside it: an embedded font file name (" + font_name_list() +
          "). Empty (default) inherits the enclosing element's font, or image.font.");
  font.check = [](std::string_view name) -> std::string {
    const std::string problem = check_font_name(name);
    return problem.empty() ? problem : ": " + problem;
  };
  std::vector<field_desc> out{
      font,
      number_field(
          "font_size",
          0.0,
          "Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing "
          "element's size, or image.font_size.",
          value_range{0, kMaxFontSize}),
  };
  if (with_color) {
    out.push_back(color_field(
        "text_color",
        "Text colour (\"#RRGGBB\" or \"#RRGGBBAA\") for this element and everything inside it: titles, axis and "
        "tick labels, legends and labels. Empty (default) inherits, or uses the theme's."));
  }
  return out;
}

text_style text_style_of(const node& n) {
  text_style s;
  if (n.cls == nullptr) {
    return s;
  }
  if (const int i = n.cls->field_index("font"); i >= 0) {
    s.font = n.get<std::string>(static_cast<size_t>(i));
  }
  if (const int i = n.cls->field_index("font_size"); i >= 0) {
    s.font_size = n.get<double>(static_cast<size_t>(i));
  }
  if (const int i = n.cls->field_index("text_color"); i >= 0) {
    s.color = n.get<std::string>(static_cast<size_t>(i));
  }
  return s;
}

text_style_scope::text_style_scope(const text_style& style, const draw_context& dc) {
  if (style.empty() || ImGui::GetCurrentContext() == nullptr) {
    return;
  }
  if (!style.font.empty() || style.font_size > 0.0) {
    // PushFont(nullptr, size) keeps the font; PushFont(font, 0) keeps the size.
    ImFont* font = style.font.empty() ? nullptr : dc.font(style.font);
    ImGui::PushFont(font, static_cast<float>(style.font_size));
    font_ = true;
  }
  if (!style.color.empty()) {
    const ImVec4 color = parse_hex_color(style.color);
    if (ImPlot::GetCurrentContext() != nullptr) {
      // Grid and ticks default to a fraction of the axis text colour: keep
      // them as they are.
      const ImVec4 grid = ImPlot::GetStyleColorVec4(ImPlotCol_AxisGrid);
      const ImVec4 tick = ImPlot::GetStyleColorVec4(ImPlotCol_AxisTick);
      ImPlot::PushStyleColor(ImPlotCol_AxisGrid, grid);
      ImPlot::PushStyleColor(ImPlotCol_AxisTick, tick);
      for (ImPlotCol c : {ImPlotCol_TitleText, ImPlotCol_InlayText, ImPlotCol_AxisText, ImPlotCol_LegendText}) {
        ImPlot::PushStyleColor(c, color);
      }
      implot_colors_ = 6;
    }
    if (ImPlot3D::GetCurrentContext() != nullptr) {
      const ImVec4 grid = ImPlot3D::GetStyleColorVec4(ImPlot3DCol_AxisGrid);
      const ImVec4 tick = ImPlot3D::GetStyleColorVec4(ImPlot3DCol_AxisTick);
      ImPlot3D::PushStyleColor(ImPlot3DCol_AxisGrid, grid);
      ImPlot3D::PushStyleColor(ImPlot3DCol_AxisTick, tick);
      for (ImPlot3DCol c :
           {ImPlot3DCol_TitleText, ImPlot3DCol_InlayText, ImPlot3DCol_AxisText, ImPlot3DCol_LegendText}) {
        ImPlot3D::PushStyleColor(c, color);
      }
      implot3d_colors_ = 6;
    }
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    imgui_colors_ = 1;
    if (ImUml::GetCurrentContext() != nullptr) {
      ImUml::PushStyleColor(ImUmlCol_Text, color);
      imuml_colors_ = 1;
    }
  }
}

text_style_scope::~text_style_scope() {
  if (imuml_colors_ > 0) {
    ImUml::PopStyleColor(imuml_colors_);
  }
  if (imgui_colors_ > 0) {
    ImGui::PopStyleColor(imgui_colors_);
  }
  if (implot3d_colors_ > 0) {
    ImPlot3D::PopStyleColor(implot3d_colors_);
  }
  if (implot_colors_ > 0) {
    ImPlot::PopStyleColor(implot_colors_);
  }
  if (font_) {
    ImGui::PopFont();
  }
}

} // namespace bdg::nymph
