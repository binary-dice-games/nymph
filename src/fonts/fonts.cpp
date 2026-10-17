// MIT License © 2026 Binary Dice Games
/// @file fonts.cpp
/// @brief Lookup of the embedded fonts by file name.
#include <string>
#include <vector>
#include "core/suggest.hpp"
#include "fonts/embedded_fonts.hpp"

namespace bdg::nymph {

const embedded_file* find_font(std::string_view name) {
  for (const embedded_file& f : embedded_fonts()) {
    if (name == f.name) {
      return &f;
    }
  }
  return nullptr;
}

std::string font_name_list() {
  std::string out(kDefaultFontName);
  for (const embedded_file& f : embedded_fonts()) {
    out += std::string(", ") + f.name;
  }
  return out;
}

std::string check_font_name(std::string_view name) {
  if (name.empty() || name == kDefaultFontName || find_font(name) != nullptr) {
    return {};
  }
  std::vector<std::string> names{std::string(kDefaultFontName)};
  for (const embedded_file& f : embedded_fonts()) {
    names.emplace_back(f.name);
  }
  return "unknown font '" + std::string(name) + "'" + did_you_mean(name, names) + " (fonts: " + font_name_list() + ")";
}

} // namespace bdg::nymph
