// MIT License © 2026 Binary Dice Games
/// @file fonts_used.cpp
/// @brief fonts_used(), fonts_missing(), require_fonts().
#include "fonts/fonts_used.hpp"
#include <algorithm>
#include "core/error.hpp"
#include "fonts/embedded_fonts.hpp"
#include "render/text_style.hpp"
#include "schema/element_class.hpp"

namespace bdg::nymph {

namespace {

/// A font named in the source, with where it was written.
struct font_use {
  std::string name;
  int line = 0;
  int column = 0;
};

/// Collects the fonts named under `n`, which inherits font `inherited`;
/// implied fonts go to `implied`.
void collect(
    const node& n,
    const std::string& inherited,
    std::vector<font_use>& named,
    std::vector<std::string>& implied) {
  std::string font = inherited;
  const text_style style = text_style_of(n);
  if (!style.font.empty()) {
    font = style.font;
    const int i = n.cls->field_index("font");
    const size_t at = static_cast<size_t>(i);
    const source_pos pos = at < n.field_pos.size() ? n.field_pos[at] : source_pos{n.line, n.column};
    named.push_back({font, pos.line, pos.column});
  }
  if (n.cls != nullptr) {
    for (std::string& extra : n.cls->implied_fonts(n, font)) {
      implied.push_back(std::move(extra));
    }
  }
  for (const node& child : n.children) {
    collect(child, font, named, implied);
  }
}

bool is_catalog_font(const std::string& name) {
  return name != kDefaultFontName && find_font(name) != nullptr;
}

} // namespace

std::vector<std::string> fonts_used(const figure& f) {
  std::vector<font_use> named;
  std::vector<std::string> implied;
  const std::string root_font = f.image.font.empty() ? std::string(kDefaultFontName) : f.image.font;
  if (!f.image.font.empty()) {
    named.push_back({f.image.font, f.image_font_pos.line, f.image_font_pos.column});
  }
  collect(f.root, root_font, named, implied);
  std::vector<std::string> out;
  for (const font_use& u : named) {
    out.push_back(u.name);
  }
  out.insert(out.end(), implied.begin(), implied.end());
  out.erase(
      std::remove_if(out.begin(), out.end(), [](const std::string& n) { return !is_catalog_font(n); }), out.end());
  std::sort(out.begin(), out.end());
  out.erase(std::unique(out.begin(), out.end()), out.end());
  return out;
}

std::vector<std::string> fonts_missing(const figure& f) {
  std::vector<std::string> out;
  for (std::string& name : fonts_used(f)) {
    if (font_data(name).empty()) {
      out.push_back(std::move(name));
    }
  }
  return out;
}

void require_fonts(const figure& f) {
  std::vector<font_use> named;
  std::vector<std::string> implied;
  if (!f.image.font.empty()) {
    named.push_back({f.image.font, f.image_font_pos.line, f.image_font_pos.column});
  }
  collect(f.root, f.image.font, named, implied);
  for (const font_use& u : named) {
    if (is_catalog_font(u.name) && font_data(u.name).empty()) {
      throw error(
          u.line,
          u.column,
          "font '" + u.name + "' is not embedded in this build; register its file with register_font()");
    }
  }
}

} // namespace bdg::nymph
