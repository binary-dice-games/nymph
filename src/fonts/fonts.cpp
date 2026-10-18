// MIT License © 2026 Binary Dice Games
/// @file fonts.cpp
/// @brief The font catalog: lookup by file name, and fonts registered at
///        run time (spec/04_web_viewer/DESIGN.md, "Fonts on demand").
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>
#include "core/sha256.hpp"
#include "core/suggest.hpp"
#include "fonts/embedded_fonts.hpp"
#include "fonts/font_testing.hpp"

namespace bdg::nymph {

namespace {

/// Bytes registered with register_font(). Entries are never removed while
/// the process runs (except by the testing reset), so spans into them stay
/// valid.
struct font_store {
  std::mutex mutex;
  std::map<std::string, std::unique_ptr<const std::vector<uint8_t>>, std::less<>> registered;
  bool embedded_hidden = false;
};

font_store& store() {
  static font_store s;
  return s;
}

} // namespace

// ── Lookup ───────────────────────────────────────────────────────────────────

const font_entry* find_font(std::string_view name) {
  for (const font_entry& f : font_catalog()) {
    if (name == f.name) {
      return &f;
    }
  }
  return nullptr;
}

std::span<const uint8_t> font_data(std::string_view name) {
  const font_entry* entry = find_font(name);
  if (entry == nullptr) {
    return {};
  }
  font_store& s = store();
  const std::lock_guard<std::mutex> lock(s.mutex);
  if (entry->data != nullptr && !s.embedded_hidden) {
    return {entry->data, entry->size};
  }
  const auto it = s.registered.find(name);
  if (it == s.registered.end()) {
    return {};
  }
  return {it->second->data(), it->second->size()};
}

void register_font(std::string_view name, std::span<const uint8_t> bytes) {
  const font_entry* entry = find_font(name);
  if (entry == nullptr) {
    throw std::invalid_argument("register_font: '" + std::string(name) + "' is not a font of this build's catalog");
  }
  if (!font_data(name).empty()) {
    return;
  }
  if (bytes.size() != entry->size) {
    throw std::invalid_argument(
        "register_font: '" + std::string(name) + "' has " + std::to_string(bytes.size()) + " bytes, expected " +
        std::to_string(entry->size));
  }
  if (sha256_hex(bytes) != entry->sha256) {
    throw std::invalid_argument("register_font: '" + std::string(name) + "' does not match the catalog's SHA-256");
  }
  font_store& s = store();
  const std::lock_guard<std::mutex> lock(s.mutex);
  if (s.registered.find(name) == s.registered.end()) {
    s.registered.emplace(std::string(name), std::make_unique<const std::vector<uint8_t>>(bytes.begin(), bytes.end()));
  }
}

std::string italic_sibling_name(std::string_view name) {
  for (std::string_view style : {std::string_view("-Regular."), std::string_view("-Bold.")}) {
    const size_t at = name.find(style);
    if (at != std::string_view::npos) {
      std::string sibling(name);
      sibling.replace(at, style.size(), "-Italic.");
      return find_font(sibling) != nullptr ? sibling : std::string();
    }
  }
  return {};
}

// ── Names ────────────────────────────────────────────────────────────────────

std::string font_name_list() {
  std::string out(kDefaultFontName);
  for (const font_entry& f : font_catalog()) {
    out += std::string(", ") + f.name;
  }
  return out;
}

std::string check_font_name(std::string_view name) {
  if (name.empty() || name == kDefaultFontName || find_font(name) != nullptr) {
    return {};
  }
  std::vector<std::string> names{std::string(kDefaultFontName)};
  for (const font_entry& f : font_catalog()) {
    names.emplace_back(f.name);
  }
  return "unknown font '" + std::string(name) + "'" + did_you_mean(name, names) + " (fonts: " + font_name_list() + ")";
}

// ── Testing ──────────────────────────────────────────────────────────────────

void hide_embedded_fonts_for_testing(bool hide) {
  font_store& s = store();
  const std::lock_guard<std::mutex> lock(s.mutex);
  s.embedded_hidden = hide;
}

} // namespace bdg::nymph
