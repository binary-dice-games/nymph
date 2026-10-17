// MIT License © 2026 Binary Dice Games
/// @file field_desc.hpp
/// @brief One field of an element class: name, type, default, help text,
///        names for its values and its range.
#pragma once

#include <optional>
#include <string>
#include <vector>
#include "schema/name_table.hpp"
#include "schema/value.hpp"

namespace bdg::nymph {

/// @brief How a field's YAML value is read (DESIGN.md section 3).
enum class field_type {
  boolean, ///< true / false (also yes/no, on/off), unquoted.
  integer, ///< Whole number in the 32-bit range, unquoted.
  number, ///< Any number, unquoted.
  text, ///< Any scalar, kept as written (`title: 2025` is text).
  color, ///< "#RRGGBB" or "#RRGGBBAA"; empty means automatic.
  enumeration, ///< One name from `names`, or one of their values.
  flags, ///< Names from `names` joined with `|`, or an integer.
  number_array, ///< List of numbers (`~` / `null` is a gap), or a $column.
  integer_array, ///< List of whole numbers, or a $column.
};

/// @brief Inclusive limits of a number or integer field.
struct value_range {
  double min = 0.0;
  double max = 0.0;
};

/// @brief One field of an element class.
struct field_desc {
  std::string name;
  field_type type = field_type::text;
  value default_value;
  std::string description; ///< One or two sentences, shown by `nymph schema`.
  const name_table* names = nullptr; ///< enumeration / flags only; must outlive the class.
  std::optional<value_range> limits; ///< integer / number only.
};

/// @brief Human-readable name of a field type ("number", "list of numbers").
const char* type_name(field_type type);

// ── Builders ─────────────────────────────────────────────────────────────────

inline field_desc bool_field(std::string name, bool def, std::string description) {
  return {std::move(name), field_type::boolean, def, std::move(description), nullptr, std::nullopt};
}

inline field_desc
int_field(std::string name, int64_t def, std::string description, std::optional<value_range> limits = std::nullopt) {
  return {std::move(name), field_type::integer, def, std::move(description), nullptr, limits};
}

inline field_desc
number_field(std::string name, double def, std::string description, std::optional<value_range> limits = std::nullopt) {
  return {std::move(name), field_type::number, def, std::move(description), nullptr, limits};
}

inline field_desc text_field(std::string name, std::string def, std::string description) {
  return {std::move(name), field_type::text, std::move(def), std::move(description), nullptr, std::nullopt};
}

inline field_desc color_field(std::string name, std::string description) {
  return {std::move(name), field_type::color, std::string{}, std::move(description), nullptr, std::nullopt};
}

inline field_desc enum_field(std::string name, int64_t def, const name_table& names, std::string description) {
  return {std::move(name), field_type::enumeration, def, std::move(description), &names, std::nullopt};
}

inline field_desc flags_field(std::string name, int64_t def, const name_table& names, std::string description) {
  return {std::move(name), field_type::flags, def, std::move(description), &names, std::nullopt};
}

inline field_desc numbers_field(std::string name, std::string description) {
  return {
      std::move(name), field_type::number_array, std::vector<double>{}, std::move(description), nullptr, std::nullopt};
}

inline field_desc integers_field(std::string name, std::string description) {
  return {
      std::move(name),
      field_type::integer_array,
      std::vector<int64_t>{},
      std::move(description),
      nullptr,
      std::nullopt};
}

} // namespace bdg::nymph
