// MIT License © 2026 Binary Dice Games
/// @file image_fields.cpp
/// @brief The image options as fields (see image_fields.hpp).
#include "edit/image_fields.hpp"
#include "render/text_style.hpp"
#include "themes/theme_catalog.hpp"

namespace bdg::nymph {

namespace {

/// The chart themes, in catalog order (light, dark, classic first).
const name_table& theme_names() {
  static const name_table t = [] {
    std::vector<name_entry> entries;
    for (const chart_theme& th : chart_themes()) {
      entries.push_back({th.id, static_cast<int64_t>(entries.size())});
    }
    return name_table(std::move(entries));
  }();
  return t;
}

} // namespace

const std::vector<field_desc>& image_fields() {
  static const std::vector<field_desc> fields = [] {
    std::vector<field_desc> f;
    f.push_back(int_field("width", 800, "Logical width of the image in pixels, 16 to 4096.", value_range{16, 4096}));
    f.push_back(int_field("height", 500, "Logical height of the image in pixels, 16 to 4096.", value_range{16, 4096}));
    f.push_back(int_field(
        "scale",
        1,
        "Supersampling factor of a saved PNG, 1 to 4: it is width x scale by height x scale pixels. The window "
        "does not show it.",
        value_range{1, 4}));
    f.push_back(enum_field(
        "theme",
        0,
        theme_names(),
        "Chart theme: the colours, fonts and series palette every field left out takes (" + chart_theme_list() +
            ")."));
    f.push_back(int_field(
        "padding", 8, "Pixels between the edge of the image and the figure.", value_range{0, 2048}));
    for (field_desc style : text_style_fields()) {
      style.description = "Text style of the whole figure: " + style.description;
      f.push_back(std::move(style));
    }
    f.push_back(color_field("background", "Background of the whole figure; empty for the theme's."));
    f.push_back(texts_field(
        "palette", "Series colours (\"#RRGGBB\" each) plots take in turn when they set none; empty for the theme's."));
    return f;
  }();
  return fields;
}

value image_field_value(const image_options& o, size_t index) {
  const std::string& name = image_fields()[index].name;
  if (name == "width") {
    return static_cast<int64_t>(o.width);
  }
  if (name == "height") {
    return static_cast<int64_t>(o.height);
  }
  if (name == "scale") {
    return static_cast<int64_t>(o.scale);
  }
  if (name == "theme") {
    return theme_names().find(o.theme).value_or(0);
  }
  if (name == "padding") {
    return static_cast<int64_t>(o.padding);
  }
  if (name == "font") {
    return o.font;
  }
  if (name == "font_size") {
    return o.font_size;
  }
  if (name == "background") {
    return o.background;
  }
  if (name == "palette") {
    return o.palette;
  }
  return o.text_color;
}

} // namespace bdg::nymph
