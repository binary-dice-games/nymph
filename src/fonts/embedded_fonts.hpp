// MIT License © 2026 Binary Dice Games
/// @file embedded_fonts.hpp
/// @brief The fonts compiled into nymph from the `fonts/` folder, found by
///        file name (spec/01_core_features/DESIGN.md section 3, "Text style").
///
/// The table behind embedded_fonts() and embedded_licenses() is generated
/// at build time by `cmake/NymphFonts.cmake`; the binary reads no font file
/// at run time.
#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <string_view>

namespace bdg::nymph {

/// @brief One embedded file: its file name and bytes.
struct embedded_file {
  const char* name; ///< File name, e.g. "FiraSans-Regular.ttf".
  const unsigned char* data; ///< `size` bytes, followed by a 0 byte.
  std::size_t size;

  std::string_view text() const {
    return std::string_view(reinterpret_cast<const char*>(data), size);
  }
};

/// @brief Every embedded font (`*.ttf`, `*.otf`), sorted by file name.
std::span<const embedded_file> embedded_fonts();

/// @brief Every embedded licence text (`*.txt` next to the fonts), sorted
///        by file name.
std::span<const embedded_file> embedded_licenses();

/// @brief Name meaning ImGui's built-in font (the default for `image.font`).
inline constexpr std::string_view kDefaultFontName = "default";

/// @brief The embedded font called `name`, or nullptr.
const embedded_file* find_font(std::string_view name);

/// @brief kDefaultFontName and every embedded font name, joined as
///        "a, b, c" (for messages and the schema).
std::string font_name_list();

/// @brief Checks a `font` field: empty (inherit), kDefaultFontName or an
///        embedded font name.
/// @return An error message ("unknown font 'x' (did you mean ...)"), or
///         empty when acceptable.
std::string check_font_name(std::string_view name);

} // namespace bdg::nymph
