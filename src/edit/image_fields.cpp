// MIT License © 2026 Binary Dice Games
/// @file image_fields.cpp
/// @brief The image options as fields (see image_fields.hpp).
#include "edit/image_fields.hpp"
#include "render/text_style.hpp"

namespace bdg::nymph {

namespace {

const name_table& theme_names() {
  static const name_table t{{"light", 0}, {"dark", 1}, {"classic", 2}};
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
    f.push_back(enum_field("theme", 0, theme_names(), "Colours of the whole figure: light, dark or classic."));
    f.push_back(int_field(
        "padding", 8, "Pixels between the edge of the image and the figure.", value_range{0, 2048}));
    for (field_desc style : text_style_fields()) {
      style.description = "Text style of the whole figure: " + style.description;
      f.push_back(std::move(style));
    }
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
  return o.text_color;
}

} // namespace bdg::nymph
