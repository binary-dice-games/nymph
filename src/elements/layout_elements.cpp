// MIT License © 2026 Binary Dice Games
/// @file layout_elements.cpp
/// @brief `VerticalLayout`, `HorizontalLayout`, `Spring`, `Label`,
///        `Separator`, `SeparatorText`.
///
/// Fields and defaults are wish's (`src/ui/ui_elements/{layout,spring,
/// label,separator}.cpp`); descriptions are rewritten for nymph's layout
/// engine, which sizes everything before drawing instead of one frame late.
#include "elements/layout_elements.hpp"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>
#include "elements/plot_style.hpp"
#include "layout/layout.hpp"
#include "render/draw_context.hpp"
#include "render/text_style.hpp"
#include "schema/element_class.hpp"

namespace bdg::nymph {

namespace {

const std::string kFigure(kFigureSlot);

ImU32 to_u32(const std::string& hex) {
  return ImGui::ColorConvertFloat4ToU32(parse_hex_color(hex));
}

/// The slot of widgets in user-interface windows (spec/09_ui_mockups).
constexpr std::string_view kUiWidgetSlot = "ui.widget";

// ── VerticalLayout / HorizontalLayout ────────────────────────────────────────

const name_table& align_names() {
  static const name_table t{{"left", 0}, {"right", 1}};
  return t;
}

namespace layout_field {
constexpr size_t spacing = 0, width = 1, height = 2, card = 3, padding = 4, rounding = 5, align = 6;
}

std::vector<field_desc> layout_fields(bool horizontal) {
  std::vector<field_desc> f{
      number_field("spacing", 0.0, "Space between child elements in pixels.", value_range{0, 256}),
      number_field(
          "width",
          0.0,
          "Width hint: 0 (default) is the natural width, a positive value fixed pixels, a negative value a stretch "
          "weight inside a HorizontalLayout (it shares the space left, in proportion to its magnitude). Outside a "
          "HorizontalLayout anything but a positive value fills the width.",
          value_range{-1000, 8192}),
      number_field(
          "height",
          0.0,
          "Height hint: 0 (default) is the natural height, a positive value fixed pixels, a negative value a stretch "
          "weight inside a VerticalLayout. Outside a VerticalLayout anything but a positive value fills the height.",
          value_range{-1000, 8192}),
      bool_field(
          "card",
          false,
          "Draws the children on a rounded, filled, bordered panel, inset by \"padding\". Colors follow the theme."),
      number_field(
          "padding", 12.0, "Inner margin of a card, in pixels. Ignored unless \"card\" is true.", value_range{0, 128}),
      number_field(
          "rounding", 10.0, "Corner radius of a card, in pixels. Ignored unless \"card\" is true.", value_range{0, 64}),
  };
  if (horizontal) {
    f.push_back(enum_field(
        "align",
        0,
        align_names(),
        "Horizontal alignment of the children when they do not fill the row (no stretch child): \"left\" (default) "
        "or \"right\"."));
  }
  return with_text_style(std::move(f));
}

class linear_layout_class : public element_class {
 public:
  explicit linear_layout_class(bool horizontal)
      : element_class(
            horizontal ? "HorizontalLayout" : "VerticalLayout",
            kFigure,
            {kFigure},
            layout_fields(horizontal),
            horizontal ? "Arranges children left-to-right." : "Stacks children top-to-bottom."),
        horizontal_(horizontal) {}

  ImVec2 measure(const node& n, draw_context& dc, float width) const override {
    const float pad = padding(n);
    const ImVec2 inner = measure_linear(n, dc, std::max(0.0f, width - 2 * pad), how(n));
    return ImVec2(inner.x + 2 * pad, inner.y + 2 * pad);
  }

  void arrange(const node& n, draw_context& dc, const rect& r) const override {
    const float pad = padding(n);
    const rect inner{ImVec2(r.min.x + pad, r.min.y + pad), ImVec2(r.max.x - pad, r.max.y - pad)};
    arrange_linear(n, dc, inner, how(n));
  }

  void draw(const node& n, draw_context& dc) const override {
    if (n.get<bool>(layout_field::card)) {
      const rect r = dc.rect_of(n);
      const float rounding = static_cast<float>(n.get<double>(layout_field::rounding));
      ImDrawList* dl = ImGui::GetWindowDrawList();
      dl->AddRectFilled(r.min, r.max, ImGui::GetColorU32(ImGuiCol_FrameBg), rounding);
      dl->AddRect(r.min, r.max, ImGui::GetColorU32(ImGuiCol_Border), rounding);
    }
    dc.draw_children(n);
  }

 private:
  linear_layout how(const node& n) const {
    linear_layout h;
    h.vertical = !horizontal_;
    h.spacing = static_cast<float>(n.get<double>(layout_field::spacing));
    h.align_right = horizontal_ && n.get<int64_t>(layout_field::align) == 1;
    return h;
  }

  static float padding(const node& n) {
    return n.get<bool>(layout_field::card) ? static_cast<float>(n.get<double>(layout_field::padding)) : 0.0f;
  }

  bool horizontal_;
};

// ── Spring ───────────────────────────────────────────────────────────────────

class spring_class : public element_class {
 public:
  spring_class()
      : element_class(
            "Spring",
            kFigure,
            {},
            {number_field(
                "weight",
                1.0,
                "Expandable, invisible space in a HorizontalLayout (width) or VerticalLayout (height): shares the space "
                "left over with the other stretch children, in proportion to this weight. Two Springs around an element "
                "centre it; one between two elements pushes them to opposite edges. 0 collapses it.",
                value_range{0, 10000})},
            "Expandable, invisible space that shares leftover room with sibling stretch children.") {}

  size_hints hints(const node& n) const override {
    const float w = static_cast<float>(n.get<double>(0));
    return {-w, -w}; // a weight of 0 is a natural size of 0
  }

  void draw(const node&, draw_context&) const override {}
};

// ── Label ────────────────────────────────────────────────────────────────────

namespace label_field {
constexpr size_t text = 0, text_color = 1, text_color_light = 2, text_color_dark = 3, wrap = 4;
}

class label_class : public element_class {
 public:
  label_class()
      : element_class(
            "Label",
            kFigure,
            {},
            with_text_style(
                {
                    text_field("text", "", "Displayed text content."),
                    color_field("text_color", "Optional \"#RRGGBBAA\"/\"#RRGGBB\" text color; empty uses the theme's."),
                    color_field(
                        "text_color_light",
                        "Text color used instead of text_color when the theme's colours are light. Ignored when empty."),
                    color_field(
                        "text_color_dark",
                        "Text color used instead of text_color when the theme's colours are dark or classic. Ignored when empty."),
                    bool_field(
                        "wrap",
                        false,
                        "When true, wraps text to the available width instead of running past it."),
                },
                false),
            "A read-only text element.") {}

  ImVec2 measure(const node& n, draw_context&, float width) const override {
    const std::string& text = n.get<std::string>(label_field::text);
    const bool wrap = n.get<bool>(label_field::wrap) && width > 0.0f;
    return ImGui::CalcTextSize(text.c_str(), nullptr, false, wrap ? width : -1.0f);
  }

  void draw(const node& n, draw_context& dc) const override {
    const rect r = dc.rect_of(n);
    const std::string& color = text_color(n, dc.options().base);
    ImGui::PushClipRect(r.min, r.max, true);
    if (!color.empty()) {
      ImGui::PushStyleColor(ImGuiCol_Text, to_u32(color));
    }
    if (n.get<bool>(label_field::wrap)) {
      ImGui::PushTextWrapPos(r.max.x);
    }
    ImGui::SetCursorScreenPos(r.min);
    ImGui::TextUnformatted(n.get<std::string>(label_field::text).c_str());
    if (n.get<bool>(label_field::wrap)) {
      ImGui::PopTextWrapPos();
    }
    if (dc.edit_mode()) {
      const ImVec2 size = measure(n, dc, r.width());
      dc.record_target(n, "text", {r.min, ImVec2(r.min.x + size.x, r.min.y + size.y)}, n.get<std::string>(label_field::text));
    }
    if (!color.empty()) {
      ImGui::PopStyleColor();
    }
    ImGui::PopClipRect();
  }

 private:
  /// The theme-specific colour if set, else `text_color`, else "".
  static const std::string& text_color(const node& n, const std::string& theme) {
    const std::string& specific =
        n.get<std::string>(theme == "light" ? label_field::text_color_light : label_field::text_color_dark);
    return specific.empty() ? n.get<std::string>(label_field::text_color) : specific;
  }
};

// ── Separator / SeparatorText ────────────────────────────────────────────────

/// Draws a horizontal rule across [x0, x1) at the vertical centre of `r`.
void rule(const rect& r, float x0, float x1) {
  if (x1 <= x0) {
    return;
  }
  const float y = std::floor((r.min.y + r.max.y) * 0.5f);
  // A filled one-pixel rectangle, not AddLine: crisp and exactly one row.
  ImGui::GetWindowDrawList()->AddRectFilled(
      ImVec2(x0, y), ImVec2(x1, y + 1.0f), ImGui::GetColorU32(ImGuiCol_Separator));
}

class separator_class : public element_class {
 public:
  separator_class() : element_class("Separator", kFigure, {}, {}, "A horizontal rule between elements.") {
    also_fits(std::string(kUiWidgetSlot)); // ImGui::Separator() in a window
  }

  ImVec2 measure(const node&, draw_context&, float) const override {
    return ImVec2(0.0f, 1.0f + 2.0f * ImGui::GetStyle().ItemSpacing.y);
  }

  void draw(const node& n, draw_context& dc) const override {
    if (placed_as(n) == kUiWidgetSlot) {
      ImGui::Separator();
      return;
    }
    const rect r = dc.rect_of(n);
    rule(r, r.min.x, r.max.x);
  }
};

class separator_text_class : public element_class {
 public:
  separator_text_class()
      : element_class(
            "SeparatorText",
            kFigure,
            {},
            with_text_style({text_field("label", "", "Text shown inline with the separator line.")}),
            "A horizontal rule with a text label embedded in the line.") {
    also_fits(std::string(kUiWidgetSlot)); // ImGui::SeparatorText() in a window
  }

  ImVec2 measure(const node& n, draw_context&, float) const override {
    const ImGuiStyle& style = ImGui::GetStyle();
    const ImVec2 text = ImGui::CalcTextSize(n.get<std::string>(0).c_str());
    return ImVec2(text.x + 2.0f * style.SeparatorTextPadding.x, text.y + 2.0f * style.SeparatorTextPadding.y);
  }

  void draw(const node& n, draw_context& dc) const override {
    if (placed_as(n) == kUiWidgetSlot) {
      ImGui::SeparatorText(n.get<std::string>(0).c_str());
      dc.record_target(n, "label", {ImGui::GetItemRectMin(), ImGui::GetItemRectMax()}, n.get<std::string>(0));
      return;
    }
    const rect r = dc.rect_of(n);
    const ImGuiStyle& style = ImGui::GetStyle();
    const std::string& label = n.get<std::string>(0);
    const ImVec2 size = ImGui::CalcTextSize(label.c_str());
    // As ImGui::SeparatorText with its default left alignment: a short rule,
    // the text, then a rule to the edge.
    const float text_x = r.min.x + style.SeparatorTextPadding.x;
    const float text_y = std::floor((r.min.y + r.max.y - size.y) * 0.5f);
    rule(r, r.min.x, text_x - style.ItemSpacing.x);
    ImGui::GetWindowDrawList()->AddText(ImVec2(text_x, text_y), ImGui::GetColorU32(ImGuiCol_Text), label.c_str());
    rule(r, text_x + size.x + style.ItemSpacing.x, r.max.x);
    dc.record_target(n, "label", {ImVec2(text_x, text_y), ImVec2(text_x + size.x, text_y + size.y)}, label);
  }
};

} // namespace

void register_layout(registry& reg) {
  reg.add(std::make_unique<linear_layout_class>(false));
  reg.add(std::make_unique<linear_layout_class>(true));
  reg.add(std::make_unique<spring_class>());
  reg.add(std::make_unique<label_class>());
  reg.add(std::make_unique<separator_class>());
  reg.add(std::make_unique<separator_text_class>());
}

} // namespace bdg::nymph
