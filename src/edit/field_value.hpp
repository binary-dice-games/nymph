// MIT License © 2026 Binary Dice Games
/// @file field_value.hpp
/// @brief Typed values to the one-line YAML a field is written with, and
///        the check of a whole edited source
///        (spec/08_view_editor/DESIGN.md section 3, `editor_model`).
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include "schema/field_desc.hpp"

namespace bdg::nymph {

/// @brief A text as a YAML scalar on one line: plain when that reads back
///        as the same text, else double-quoted (line breaks as `\n`).
std::string encode_text(std::string_view text);

/// @brief `true` or `false`.
std::string encode_bool(bool value);

/// @brief A whole number.
std::string encode_int(int64_t value);

/// @brief The shortest decimal that reads back as `value` (no `nan` / `inf`;
///        those give "0").
std::string encode_number(double value);

/// @brief A colour as `"#rrggbb"` / `"#rrggbbaa"` (quoted: `#` starts a
///        YAML comment).
/// @return nullopt when `color` is not `#RRGGBB` or `#RRGGBBAA`.
std::optional<std::string> encode_color(std::string_view color);

/// @brief The name of enumeration value `value` in the field's table, or
///        its number when it has none. `f.names` must be set.
std::string encode_enum(const field_desc& f, int64_t value);

/// @brief Flags as `A|B` (quoted, `|` starts a YAML block scalar).
std::string encode_flags(const field_desc& f, int64_t value);

/// @brief A problem found in an edited source.
struct source_problem {
  int line = 0; ///< 1-based, 0 when the problem has no position.
  int column = 0;
  std::string message;
};

/// @brief Reads `source` as nymph does (bind_source() with the built-in
///        elements): the problem with it, or nullopt when it is fine.
std::optional<source_problem> check_source(std::string_view source);

} // namespace bdg::nymph
