// MIT License © 2026 Binary Dice Games
/// @file image_widgets.cpp
/// @brief Image and ImageButton: built-in placeholder images in a mockup
///        (spec/09_ui_mockups/DESIGN.md section 3, "Placeholder images").
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include "elements/ui/ui_common.hpp"
#include "placeholders/placeholders.hpp"
#include "schema/registry.hpp"

namespace bdg::nymph::ui {

namespace {

/// Natural size of an Image, pixels.
constexpr float kNaturalWidth = 160.0f, kNaturalHeight = 120.0f;

/// The placeholder names as an enumeration, in catalog order.
const name_table& placeholder_table() {
  static const name_table t = [] {
    std::vector<name_entry> entries;
    int64_t i = 0;
    for (const std::string& name : placeholder_names()) {
      entries.push_back({name, i++});
    }
    return name_table(std::move(entries));
  }();
  return t;
}

int64_t default_placeholder() {
  return placeholder_table().find("cross").value_or(0);
}

const name_table& fit_names() {
  static const name_table t{
      {"contain", static_cast<int64_t>(placeholder_fit::contain)},
      {"cover", static_cast<int64_t>(placeholder_fit::cover)},
      {"stretch", static_cast<int64_t>(placeholder_fit::stretch)},
  };
  return t;
}

/// The fields Image and ImageButton share, from `image` on.
std::vector<field_desc> image_fields(float default_size) {
  return {
      enum_field(
          "image",
          default_placeholder(),
          placeholder_table(),
          "Which built-in placeholder image to show (`nymph schema Image` lists them; docs/examples.md shows them)."),
      number_field(
          "width",
          default_size,
          "Width in pixels; in a window a negative value reaches that many pixels before the right edge (-1 fills); "
          "in a layout the usual size hint.",
          value_range{-1000, 8192}),
      number_field(
          "height",
          default_size == 0.0f ? 0.0 : default_size,
          "Height in pixels; in a window a negative value fills the height left; in a layout the usual size hint.",
          value_range{-1000, 8192}),
      enum_field(
          "fit",
          static_cast<int64_t>(placeholder_fit::contain),
          fit_names(),
          "contain keeps the aspect ratio and shows the whole image; cover fills the box and clips; stretch fills "
          "it in both directions."),
      color_field("tint", "Colour of the image's shapes; its half tones use it at half alpha. Empty: the theme's disabled text colour."),
      color_field(
          "background",
          "Colour of the box behind the image and of its cut-outs; \"#00000000\" is transparent. Empty: the theme's "
          "frame colour."),
      number_field("rounding", 0.0, "Corner radius of the box; half its size gives a circle.", value_range{0, 4096}),
      bool_field("border", false, "Outlines the box in the theme's border colour."),
  };
}

/// Indices of image_fields() after `first` fields of the class's own.
struct image_field_index {
  size_t image, width, height, fit, tint, background, rounding, border;
  explicit image_field_index(size_t first)
      : image(first),
        width(first + 1),
        height(first + 2),
        fit(first + 3),
        tint(first + 4),
        background(first + 5),
        rounding(first + 6),
        border(first + 7) {}
};

/// The drawing style of `n` (`behind`: what lies under the box).
placeholder_style style_of(const node& n, const image_field_index& f, ImU32 default_background, ImU32 behind) {
  placeholder_style s;
  s.fit = static_cast<placeholder_fit>(n.get<int64_t>(f.fit));
  const std::string& tint = n.get<std::string>(f.tint);
  s.tint = tint.empty() ? ImGui::GetColorU32(ImGuiCol_TextDisabled) : ImGui::ColorConvertFloat4ToU32(color_of(tint));
  const std::string& background = n.get<std::string>(f.background);
  s.background = background.empty() ? default_background : ImGui::ColorConvertFloat4ToU32(color_of(background));
  s.behind = behind;
  s.rounding = static_cast<float>(n.get<double>(f.rounding));
  s.border = n.get<bool>(f.border);
  return s;
}

const svg_image& image_of(const node& n, const image_field_index& f) {
  const name_table& names = placeholder_table();
  const std::string name = names.format_enum(n.get<int64_t>(f.image));
  const svg_image* image = find_placeholder(name);
  if (image == nullptr) {
    throw std::logic_error("placeholder '" + name + "' is not embedded");
  }
  return *image;
}

/// A size along one axis inside a window: positive fixed, negative up to
/// that far before the edge, 0 the natural size.
float flow_size(float hint, float available, float natural) {
  if (hint > 0.0f) {
    return hint;
  }
  if (hint < 0.0f) {
    return std::max(1.0f, available + hint + 1.0f);
  }
  return natural;
}

// ── Image ────────────────────────────────────────────────────────────────────

class image_class : public widget_class {
 public:
  image_class()
      : widget_class(
            "Image",
            {},
            with_label(image_fields(0.0f)),
            "A built-in placeholder image (a portrait silhouette, a landscape, a logo, ...) to show where an image "
            "goes. "
            "Also a layout element.",
            false),
        f_(0) {
    also_fits(std::string(kFigureSlot));
  }

  ImVec2 measure(const node& n, draw_context&, float) const override {
    return ImVec2(
        n.get<double>(f_.width) > 0 ? static_cast<float>(n.get<double>(f_.width)) : kNaturalWidth,
        n.get<double>(f_.height) > 0 ? static_cast<float>(n.get<double>(f_.height)) : kNaturalHeight);
  }

  std::string label_of(const node& n) const override {
    return n.get<std::string>(label_);
  }

 protected:
  item_result draw_item(const node& n, draw_context& dc) const override {
    ImVec2 min, max;
    const bool as_figure = placed_as(n) == kFigureSlot;
    if (as_figure) {
      const rect r = dc.rect_of(n);
      min = r.min;
      max = r.max;
    } else {
      const ImVec2 avail = ImGui::GetContentRegionAvail();
      const ImVec2 size(
          flow_size(static_cast<float>(n.get<double>(f_.width)), avail.x, kNaturalWidth),
          flow_size(static_cast<float>(n.get<double>(f_.height)), avail.y, kNaturalHeight));
      ImGui::Dummy(size);
      min = ImGui::GetItemRectMin();
      max = ImGui::GetItemRectMax();
    }
    const ImU32 behind = ImGui::GetColorU32(ImGuiCol_WindowBg);
    const placeholder_style style = style_of(n, f_, ImGui::GetColorU32(ImGuiCol_FrameBg), behind);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    draw_placeholder(dl, image_of(n, f_), min, max, style);
    draw_caption(dl, n.get<std::string>(label_), min, max, (style.background & IM_COL32_A_MASK) ? style.background : behind);
    item_result r;
    if (as_figure) {
      // Placed by a layout: no ImGui item; the element is its rectangle.
      dc.record_target(n, {}, {min, max}, n.get<std::string>(label_));
      r.finished = true;
    }
    r.values = {{"image", placeholder_table().format_enum(n.get<int64_t>(f_.image))}};
    return r;
  }

 private:
  static std::vector<field_desc> with_label(std::vector<field_desc> fields) {
    fields.push_back(text_field(
        "label", "", "A caption over the bottom of the image, for intent: \"Hero photo, 1600 x 600\"."));
    return fields;
  }

  /// The caption centred over the bottom of the box, on a panel.
  static void draw_caption(ImDrawList* dl, const std::string& label, ImVec2 min, ImVec2 max, ImU32 panel) {
    if (label.empty()) {
      return;
    }
    const ImVec2 size = ImGui::CalcTextSize(label.c_str());
    const ImVec2 at(std::round((min.x + max.x - size.x) * 0.5f), std::round(max.y - size.y - 6.0f));
    dl->PushClipRect(min, max, true);
    dl->AddRectFilled(ImVec2(at.x - 4.0f, at.y - 1.0f), ImVec2(at.x + size.x + 4.0f, at.y + size.y + 1.0f), panel, 3.0f);
    dl->AddText(at, ImGui::GetColorU32(ImGuiCol_Text), label.c_str());
    dl->PopClipRect();
  }

  image_field_index f_;
  size_t label_ = 8;
};

// ── ImageButton ──────────────────────────────────────────────────────────────

class image_button_class : public widget_class {
 public:
  image_button_class()
      : widget_class(
            "ImageButton",
            {},
            with_id(image_fields(64.0f)),
            "ImGui::ImageButton with a built-in placeholder image: a button showing the image inside its frame.",
            false),
        f_(1) {}

 protected:
  item_result draw_item(const node& n, draw_context&) const override {
    const ImVec2 pad = ImGui::GetStyle().FramePadding;
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const ImVec2 image(
        flow_size(static_cast<float>(n.get<double>(f_.width)), avail.x - 2.0f * pad.x, 64.0f),
        flow_size(static_cast<float>(n.get<double>(f_.height)), avail.y - 2.0f * pad.y, 64.0f));
    const std::string& label = n.get<std::string>(0);
    item_result r;
    r.clicked = ImGui::InvisibleButton(
        label.empty() ? "##image_button" : label.c_str(), ImVec2(image.x + 2.0f * pad.x, image.y + 2.0f * pad.y));
    const ImVec2 min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
    const ImU32 frame = ImGui::GetColorU32(
        ImGui::IsItemActive()      ? ImGuiCol_ButtonActive
            : ImGui::IsItemHovered() ? ImGuiCol_ButtonHovered
                                     : ImGuiCol_Button);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(min, max, frame, ImGui::GetStyle().FrameRounding);
    const placeholder_style style = style_of(n, f_, IM_COL32(0, 0, 0, 0), frame);
    draw_placeholder(dl, image_of(n, f_), ImVec2(min.x + pad.x, min.y + pad.y), ImVec2(max.x - pad.x, max.y - pad.y), style);
    r.values = {{"image", placeholder_table().format_enum(n.get<int64_t>(f_.image))}};
    return r;
  }

 private:
  static std::vector<field_desc> with_id(std::vector<field_desc> fields) {
    fields.insert(fields.begin(), text_field("label", "", "ID of the button (not shown); \"##\" as in ImGui labels."));
    return fields;
  }

  image_field_index f_;
};

} // namespace

void register_image_widgets(registry& reg) {
  reg.add(std::make_unique<image_class>());
  reg.add(std::make_unique<image_button_class>());
}

} // namespace bdg::nymph::ui
