// MIT License © 2026 Binary Dice Games
/// @file field_desc.hpp
/// @brief One field of an element class: name, type, default, help text,
///        names for its values and its range.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "schema/name_table.hpp"
#include "schema/value.hpp"

namespace bdg::nymph {

/// @brief How a field's YAML value is read (spec/01_core_features/DESIGN.md section 3).
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
  text_array, ///< List of texts, each kept as written, or a $column (its cells).
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
  /// text only: returns the problem with an unacceptable value, appended
  /// to "<Type>.<field>" (start it with ':' for "<Type>.<field>: ..."), or
  /// "" when the value is fine. Used for printf formats, which must not
  /// reach printf unchecked, and font names.
  std::string (*check)(std::string_view text) = nullptr;
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

/// @brief Checks a printf format for one floating-point value: literal text,
///        `%%`, and at most one `%[flags][width][.precision]` conversion of
///        type f, F, e, E, g or G, width and precision at most 99.
/// @return nullptr if acceptable, else what is wrong.
const char* check_double_format(std::string_view text);

/// @brief A text field holding a printf format for one floating-point value
///        (see check_double_format()).
inline field_desc double_format_field(std::string name, std::string def, std::string description) {
  field_desc f{std::move(name), field_type::text, std::move(def), std::move(description), nullptr, std::nullopt};
  f.check = [](std::string_view text) -> std::string {
    const char* problem = check_double_format(text);
    return problem == nullptr ? std::string() : std::string(problem) + ", not '" + std::string(text) + "'";
  };
  return f;
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

inline field_desc texts_field(std::string name, std::string description) {
  return {
      std::move(name),
      field_type::text_array,
      std::vector<std::string>{},
      std::move(description),
      nullptr,
      std::nullopt};
}

} // namespace bdg::nymph
