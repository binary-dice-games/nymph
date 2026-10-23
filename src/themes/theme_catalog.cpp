// MIT License © 2026 Binary Dice Games
/// @file theme_catalog.cpp
/// @brief The chart theme catalog (see theme_catalog.hpp).
#include "themes/theme_catalog.hpp"
#include <imgui.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <mutex>
#include "core/error.hpp"
#include "core/suggest.hpp"
#include "render/frame_setup.hpp"
#include "render/image_options.hpp"
#include "schema/convert.hpp"
#include "schema/element_class.hpp"
#include "schema/registry.hpp"

namespace bdg::nymph {

namespace {

[[noreturn]] void fail(const std::string& id, const yaml_node& at, const std::string& message) {
  throw error(at.line, at.column, "theme '" + id + "': " + message);
}

std::string lower(std::string_view text) {
  std::string out(text);
  for (char& c : out) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return out;
}

/// The text style fields: inherited by everything inside an element, so a
/// bare rule for them is the image's.
bool is_text_style(std::string_view field) {
  return field == "font" || field == "font_size" || field == "text_color";
}

/// How specific `qualifier` is for `cls`: -1 when it does not cover it,
/// 0 for no qualifier, the length of the slot prefix, or above any slot
/// for the class's own type.
int specificity(const std::string& qualifier, const element_class& cls) {
  if (qualifier.empty()) {
    return 0;
  }
  if (qualifier == lower(cls.name())) {
    return 100000;
  }
  int best = -1;
  for (const std::string& slot : cls.slots()) {
    const bool covers = slot == qualifier ||
        (slot.size() > qualifier.size() && slot.compare(0, qualifier.size(), qualifier) == 0 &&
         slot[qualifier.size()] == '.');
    if (covers) {
      best = std::max(best, static_cast<int>(qualifier.size()));
    }
  }
  return best;
}

std::string hex(const ImVec4& c) {
  char text[8];
  const auto byte = [](float v) { return static_cast<int>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); };
  std::snprintf(text, sizeof text, "#%02x%02x%02x", byte(c.x), byte(c.y), byte(c.z));
  return text;
}

/// ImPlot's Deep colormap, the series colours of a theme without a palette.
const std::vector<std::string>& deep_palette() {
  static const std::vector<std::string> p{
      "#4c72b0", "#dd8452", "#55a868", "#c44e52", "#8172b3", "#937860", "#da8bc3", "#8c8c8c", "#ccb974", "#64b5cd"};
  return p;
}

/// Fills the swatch fields from the base colours and the image options.
void derive_swatch(chart_theme& t) {
  ImGuiStyle style;
  find_theme(t.base)->imgui(&style);
  t.background = hex(style.Colors[ImGuiCol_WindowBg]);
  t.text_color = hex(style.Colors[ImGuiCol_Text]);
  t.palette = deep_palette();
  if (const yaml_node* bg = t.image_value("background")) {
    t.background = bg->text;
  }
  if (const yaml_node* text = t.image_value("text_color")) {
    t.text_color = text->text;
  }
  if (const yaml_node* palette = t.image_value("palette")) {
    t.palette.clear();
    for (const yaml_node& c : palette->children) {
      t.palette.push_back(c.text);
    }
  }
  const auto channel = [&](size_t i) { return std::stoi(t.background.substr(i, 2), nullptr, 16) / 255.0; };
  const double luma = 0.2126 * channel(1) + 0.7152 * channel(3) + 0.0722 * channel(5);
  t.dark = luma < 0.5;
}

bool is_color_text(const yaml_node& v) {
  return v.is_scalar() && is_hex_color(v.text);
}

void check_image_value(const std::string& id, const std::string& key, const yaml_node& v) {
  if (key == "background" || key == "text_color") {
    if (!is_color_text(v)) {
      fail(id, v, "image." + key + " must be a colour, \"#RRGGBB\" or \"#RRGGBBAA\"");
    }
  } else if (key == "palette") {
    if (!v.is_sequence() || v.children.empty()) {
      fail(id, v, "image.palette must be a list of colours, like [\"#4c72b0\", \"#dd8452\"]");
    }
    for (const yaml_node& c : v.children) {
      if (!is_color_text(c)) {
        fail(id, c, "image.palette: '" + c.text + "' is not a colour (\"#RRGGBB\" or \"#RRGGBBAA\")");
      }
    }
  } else if (!v.is_scalar()) {
    fail(id, v, "image." + key + " must be a single value");
  }
}

} // namespace

const theme_rule* chart_theme::rule_for(const element_class& cls, std::string_view field, bool bare) const {
  const auto it = rules.find(field);
  if (it == rules.end()) {
    return nullptr;
  }
  const theme_rule* best = nullptr;
  int best_score = -1;
  for (const theme_rule& r : it->second) {
    if (!bare && r.qualifier.empty()) {
      continue;
    }
    const int score = specificity(r.qualifier, cls);
    if (score > best_score) {
      best = &r;
      best_score = score;
    }
  }
  return best;
}

const yaml_node* chart_theme::image_value(std::string_view key) const {
  for (const auto& [k, v] : image) {
    if (k == key) {
      return &v;
    }
  }
  return nullptr;
}

chart_theme parse_chart_theme(std::string id, std::string_view yaml) {
  const yaml_node doc = parse_yaml(yaml);
  if (!doc.is_mapping()) {
    fail(id, doc, "a theme is a mapping with name, description, base, image and fields");
  }
  chart_theme t;
  t.id = std::move(id);
  t.name = t.id;
  t.base = "light";
  static constexpr std::string_view kKeys[] = {"name", "description", "base", "image", "fields"};
  for (size_t i = 0; i < doc.keys.size(); ++i) {
    const yaml_node& key = doc.keys[i];
    const yaml_node& v = doc.children[i];
    if (key.text == "name" || key.text == "description" || key.text == "base") {
      if (!v.is_scalar()) {
        fail(t.id, v, "'" + key.text + "' must be text");
      }
      (key.text == "name" ? t.name : key.text == "description" ? t.description : t.base) = v.text;
    } else if (key.text == "image") {
      if (!v.is_mapping()) {
        fail(t.id, v, "'image' must be a mapping of image options");
      }
      for (size_t j = 0; j < v.keys.size(); ++j) {
        const std::string& k = v.keys[j].text;
        if (std::find(std::begin(kThemeImageKeys), std::end(kThemeImageKeys), k) == std::end(kThemeImageKeys)) {
          fail(
              t.id,
              v.keys[j],
              "image has no option '" + k + "' a theme can set" + did_you_mean(k, kThemeImageKeys) +
                  " (options: padding, font, font_size, text_color, background, palette)");
        }
        check_image_value(t.id, k, v.children[j]);
        t.image.emplace_back(k, v.children[j]);
      }
    } else if (key.text == "fields") {
      if (!v.is_mapping()) {
        fail(t.id, v, "'fields' must be a mapping of [type or slot:]field: value");
      }
      for (size_t j = 0; j < v.keys.size(); ++j) {
        const std::string& k = v.keys[j].text;
        const size_t colon = k.rfind(':');
        const std::string qualifier = colon == std::string::npos ? "" : lower(k.substr(0, colon));
        const std::string field = colon == std::string::npos ? k : k.substr(colon + 1);
        if (field.empty() || (colon != std::string::npos && qualifier.empty())) {
          fail(t.id, v.keys[j], "'" + k + "' is not [type or slot:]field");
        }
        if (qualifier.empty() && is_text_style(field)) {
          // Inherited by everything: the image's own text style.
          if (t.image_value(field) != nullptr) {
            fail(t.id, v.keys[j], "'" + field + "' is set twice (here and under image)");
          }
          check_image_value(t.id, field, v.children[j]);
          t.image.emplace_back(field, v.children[j]);
          continue;
        }
        t.rules[field].push_back(theme_rule{qualifier, k, v.children[j]});
      }
    } else {
      fail(
          t.id,
          key,
          "unknown key '" + key.text + "'" + did_you_mean(key.text, kKeys) +
              " (keys: name, description, base, image, fields)");
    }
  }
  if (find_theme(t.base) == nullptr) {
    fail(t.id, doc.find("base") ? *doc.find("base") : doc, "base must be light, dark or classic, not '" + t.base + "'");
  }
  derive_swatch(t);
  return t;
}

std::span<const chart_theme> chart_themes() {
  static std::once_flag once;
  static std::vector<chart_theme> themes;
  std::call_once(once, [] {
    for (const embedded_file& f : embedded_themes()) {
      themes.push_back(parse_chart_theme(f.name, f.text()));
    }
    // The three base themes first, in their usual order; then by name.
    const auto rank = [](const chart_theme& t) {
      return t.id == "light" ? 0 : t.id == "dark" ? 1 : t.id == "classic" ? 2 : 3;
    };
    std::stable_sort(themes.begin(), themes.end(), [&](const chart_theme& a, const chart_theme& b) {
      return rank(a) != rank(b) ? rank(a) < rank(b) : a.id < b.id;
    });
  });
  return themes;
}

const chart_theme* find_chart_theme(std::string_view id) {
  for (const chart_theme& t : chart_themes()) {
    if (t.id == id) {
      return &t;
    }
  }
  return nullptr;
}

bool is_known_theme(std::string_view name) {
  return find_chart_theme(name) != nullptr;
}

std::string chart_theme_list() {
  std::string out;
  for (const chart_theme& t : chart_themes()) {
    out += (out.empty() ? "" : ", ") + t.id;
  }
  return out;
}

namespace {

void json_text(std::string& out, std::string_view text) {
  out += '"';
  for (const char c : text) {
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if (static_cast<unsigned char>(c) < 0x20) {
      char escaped[8];
      std::snprintf(escaped, sizeof escaped, "\\u%04x", static_cast<unsigned>(c));
      out += escaped;
    } else {
      out += c;
    }
  }
  out += '"';
}

} // namespace

std::string chart_themes_json() {
  std::string j = "[";
  for (const chart_theme& t : chart_themes()) {
    j += j.size() == 1 ? "{" : ",{";
    const std::pair<const char*, const std::string*> texts[] = {
        {"id", &t.id},
        {"name", &t.name},
        {"description", &t.description},
        {"base", &t.base},
        {"background", &t.background},
        {"text_color", &t.text_color}};
    for (const auto& [key, value] : texts) {
      json_text(j, key);
      j += ':';
      json_text(j, *value);
      j += ',';
    }
    j += std::string("\"dark\":") + (t.dark ? "true" : "false") + ",\"palette\":[";
    for (size_t i = 0; i < t.palette.size(); ++i) {
      if (i > 0) {
        j += ',';
      }
      json_text(j, t.palette[i]);
    }
    j += "]}";
  }
  j += ']';
  return j;
}

std::vector<std::string> check_chart_theme(const chart_theme& t, const registry& reg) {
  std::vector<std::string> problems;
  const std::vector<const element_class*> classes = reg.classes();
  for (const auto& [field, rules] : t.rules) {
    for (const theme_rule& r : rules) {
      const std::string where = "theme '" + t.id + "', line " + std::to_string(r.value.line) + ": " + r.key;
      bool covers_any = false;
      bool has_field = false;
      for (const element_class* cls : classes) {
        if (specificity(r.qualifier, *cls) < 0) {
          continue;
        }
        covers_any = true;
        const int index = cls->field_index(field);
        if (index < 0) {
          continue;
        }
        has_field = true;
        try {
          convert_value(r.value, cls->fields()[static_cast<size_t>(index)], r.key);
        } catch (const error& e) {
          problems.push_back(where + ": " + std::string(cls->name()) + ": " + e.what());
        }
      }
      if (!covers_any) {
        problems.push_back(where + ": '" + r.qualifier + "' is neither an element type nor a slot");
      } else if (!has_field) {
        problems.push_back(where + ": no element it covers has a field '" + field + "'");
      }
    }
  }
  return problems;
}

} // namespace bdg::nymph
