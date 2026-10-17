// MIT License © 2026 Binary Dice Games
/// @file convert.hpp
/// @brief A YAML value to a field value, by the field's type.
#pragma once

#include <string_view>
#include "document/yaml_doc.hpp"
#include "schema/field_desc.hpp"
#include "schema/value.hpp"

namespace bdg::nymph {

/// @brief Converts a literal YAML value for field `field`.
///
/// Typing follows the field, not YAML's guess
/// (spec/01_core_features/DESIGN.md section 3):
/// - boolean: unquoted true/false, yes/no, on/off (any of their usual
///   capitalisations).
/// - integer: an unquoted whole number in the 32-bit range.
/// - number: an unquoted number (`std::from_chars`, no locale; a leading
///   `+` is allowed).
/// - text: any scalar as written; a leading `$$` stands for `$`.
/// - color: `"#RRGGBB"` or `"#RRGGBBAA"`, or `""` for automatic. An unquoted
///   empty value is an error: `color: #C0392B` is a YAML comment.
/// - enumeration: one name of the field's table, or one of its values.
/// - flags: names joined with `|`, or an integer.
/// - number_array / integer_array: a list of such numbers; an unquoted
///   empty item, `~` or `null` in a number list is NaN (a gap).
/// Integers and numbers are checked against the field's limits.
///
/// `$column` references are not handled here: bind() resolves them before
/// calling this.
///
/// @param what  "Type.field", used in error messages.
/// @throws nymph::error at the offending node (for a list, the item).
value convert_value(const yaml_node& node, const field_desc& field, std::string_view what);

/// @brief Whether `text` is `#RRGGBB` or `#RRGGBBAA` (hex digits, any case).
bool is_hex_color(std::string_view text);

} // namespace bdg::nymph
