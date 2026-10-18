// MIT License © 2026 Binary Dice Games
/// @file embedded_fonts.hpp
/// @brief The font catalog: every font of the `fonts/` folder, found by
///        file name (spec/01_core_features/DESIGN.md section 3, "Text
///        style"), with the bytes of those compiled in and of those
///        registered at run time (spec/04_web_viewer/DESIGN.md, "Fonts on
///        demand").
///
/// The tables behind font_catalog(), embedded_fonts() and
/// embedded_licenses() are generated at build time by
/// `cmake/NymphFonts.cmake`; nymph reads no font file at run time.
#pragma once

#include <cstddef>
#include <cstdint>
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

/// @brief One font of the catalog. Every build knows every font of the
///        fonts folder; `NYMPH_EMBED_FONTS` decides which carry `data`.
struct font_entry {
  const char* name; ///< File name, e.g. "FiraSans-Regular.ttf".
  std::size_t size; ///< File size in bytes.
  const char* sha256; ///< SHA-256 of the file, 64 lowercase hex digits.
  const unsigned char* data; ///< The embedded bytes, or nullptr when not embedded.
};

/// @brief Every font of the fonts folder, embedded or not, sorted by name.
std::span<const font_entry> font_catalog();

/// @brief The fonts whose bytes are compiled in (`*.ttf`, `*.otf`),
///        sorted by file name.
std::span<const embedded_file> embedded_fonts();

/// @brief Every embedded licence text (`*.txt` next to the fonts), sorted
///        by file name.
std::span<const embedded_file> embedded_licenses();

/// @brief Name meaning ImGui's built-in font (the default for `image.font`).
inline constexpr std::string_view kDefaultFontName = "default";

/// @brief The catalog entry called `name`, or nullptr.
const font_entry* find_font(std::string_view name);

/// @brief Makes a catalog font that is not embedded available for drawing.
///
/// Process-wide and thread-safe. The bytes are copied and kept until the
/// process ends (ImGui atlases point at them). Registering a font that
/// already has bytes (embedded, or registered before) does nothing.
/// @throws std::invalid_argument for a name not in the catalog, or bytes
///         whose size or SHA-256 differ from the catalog's.
void register_font(std::string_view name, std::span<const uint8_t> bytes);

/// @brief The bytes of catalog font `name`: embedded or registered. Empty
///        when the font is unknown or not available.
std::span<const uint8_t> font_data(std::string_view name);

/// @brief The `-Italic` sibling of a `-Regular` or `-Bold` font name
///        ("FiraSans-Bold.ttf" -> "FiraSans-Italic.ttf") if it is in the
///        catalog, else "".
std::string italic_sibling_name(std::string_view name);

/// @brief kDefaultFontName and every catalog font name, joined as
///        "a, b, c" (for messages and the schema).
std::string font_name_list();

/// @brief Checks a `font` field: empty (inherit), kDefaultFontName or a
///        catalog font name (whether or not its bytes are available).
/// @return An error message ("unknown font 'x' (did you mean ...)"), or
///         empty when acceptable.
std::string check_font_name(std::string_view name);

} // namespace bdg::nymph
