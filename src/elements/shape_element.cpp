// MIT License © 2026 Binary Dice Games
/// @file shape_element.cpp
/// @brief The `Shape` figure element: one shape of the built-in library
///        (spec/13_more_diagrams_and_shapes/DESIGN.md sections 3 and 5).
#include "elements/shape_element.hpp"
#include <imgui.h>
#include <imuml.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include "elements/plot_style.hpp"
#include "layout/layout.hpp"
#include "render/draw_context.hpp"
#include "render/text_style.hpp"
#include "schema/element_class.hpp"
#include "shapes/shape_catalog.hpp"

namespace bdg::nymph {

namespace {

namespace field {
constexpr size_t shape = 0, text = 1, width = 2, height = 3, color = 4, fill_color = 5, line_weight = 6;
}

/// The shape names as an enumeration, in catalog order.
const name_table& shape_table() {
  static const name_table t = [] {
    std::vector<name_entry> entries;
    int64_t i = 0;
    for (const std::string& name : shape_names()) {
      entries.push_back({name, i++});
    }
    return name_table(std::move(entries));
  }();
  return t;
}

class shape_class : public element_class {
 public:
  shape_class()
      : element_class(
            "Shape",
            std::string(kFigureSlot),
            {},
            with_text_style({
                enum_field(
                    "shape",
                    shape_table().find("rectangle").value_or(0),
                    shape_table(),
                    "Which shape of the library to draw (`nymph shapes` lists them, docs/shapes.md shows them)."),
                text_field("text", "", "The label: inside the shape, or below an icon-style shape. `\\n` breaks lines."),
                number_field(
                    "width",
                    0.0,
                    "Width hint in pixels: 0 (default) is the natural width, a positive value fixed, a negative value "
                    "a stretch weight, as for any layout child.",
                    value_range{-1000, 8192}),
                number_field(
                    "height",
                    0.0,
                    "Height hint in pixels, as for width. An icon-style shape keeps its aspect ratio inside the box.",
                    value_range{-1000, 8192}),
                color_field("color", "Line colour as \"#RRGGBB[AA]\"; empty is the theme's text colour."),
                color_field("fill_color", "Fill colour as \"#RRGGBB[AA]\"; empty is the theme's frame colour."),
                number_field("line_weight", 1.5, "Line width in pixels.", value_range{0.25, 16}),
            }),
            "One shape of the built-in library (boxes, flowchart symbols, arrows, callouts, UML and network icons) "
            "with an optional label. Use it for legends, annotations and dashboards; diagram nodes take the same "
            "names in their `shape` field.") {}

  ImVec2 measure(const node& n, draw_context&, float) const override {
    const std::string name = shape_table().format_enum(n.get<int64_t>(field::shape));
    const std::string& text = n.get<std::string>(field::text);
    const ImVec2 label = text.empty() ? ImVec2(0, 0) : ImGui::CalcTextSize(text.c_str());
    ImVec2 size = ImUml::GetShapeNodeSize(name.c_str(), label);
    if (ImUml::IsShapeIcon(name.c_str()) && !text.empty()) {
      size = ImVec2(std::max(size.x, label.x), size.y + label.y + 4.0f);
    }
    return size;
  }

  void draw(const node& n, draw_context& dc) const override {
    const rect r = dc.rect_of(n);
    const std::string name = shape_table().format_enum(n.get<int64_t>(field::shape));
    const std::string& text = n.get<std::string>(field::text);
    const std::string& color = n.get<std::string>(field::color);
    const std::string& fill = n.get<std::string>(field::fill_color);
    const ImU32 line = color.empty() ? ImGui::GetColorU32(ImGuiCol_Text) : to_u32(color);
    const ImU32 body = fill.empty() ? ImGui::GetColorU32(ImGuiCol_FrameBg) : to_u32(fill);
    const float weight = static_cast<float>(n.get<double>(field::line_weight));
    ImDrawList* dl = ImGui::GetWindowDrawList();

    const bool icon = ImUml::IsShapeIcon(name.c_str());
    const ImVec2 label = text.empty() ? ImVec2(0, 0) : ImGui::CalcTextSize(text.c_str());
    ImVec2 min = r.min, max = r.max;
    if (icon && !text.empty()) {
      max.y = std::max(min.y + 1.0f, max.y - label.y - 4.0f);
    }
    ImUml::RenderShape(dl, name.c_str(), min, max, line, body, weight);
    if (!text.empty()) {
      ImVec2 text_min = min, text_max = max;
      if (icon) {
        text_min = ImVec2(r.min.x, max.y + 4.0f);
        text_max = r.max;
      } else {
        ImUml::GetShapeTextRect(name.c_str(), min, max, 1.0f, &text_min, &text_max);
      }
      const ImVec2 at(
          std::round((text_min.x + text_max.x - label.x) * 0.5f), std::round((text_min.y + text_max.y - label.y) * 0.5f));
      dl->PushClipRect(r.min, r.max, true);
      dl->AddText(at, ImGui::GetColorU32(ImGuiCol_Text), text.c_str());
      dl->PopClipRect();
    }
    if (dc.edit_mode()) {
      dc.record_target(n, "text", {r.min, r.max}, text);
    }
  }

 private:
  static ImU32 to_u32(const std::string& hex) {
    return ImGui::ColorConvertFloat4ToU32(parse_hex_color(hex));
  }
};

} // namespace

void register_shape_elements(registry& reg) {
  reg.add(std::make_unique<shape_class>());
}

} // namespace bdg::nymph
