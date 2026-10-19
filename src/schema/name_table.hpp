// MIT License © 2026 Binary Dice Games
/// @file name_table.hpp
/// @brief Names for integer field values: enumerations (`marker: Circle`)
///        and flag sets (`flags: NoLegend|NoTitle`).
#pragma once

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bdg::nymph {

/// @brief One named value.
struct name_entry {
  std::string name;
  int64_t value = 0;
};

/// @brief An ordered list of named values. For flags, list single bits
///        before composites, so format_flags() prefers them (wish's
///        convention for its EnumFlags tables).
class name_table {
 public:
  name_table(std::initializer_list<name_entry> entries) : entries_(entries) {}
  /// @brief A table built at run time (names from a catalog).
  explicit name_table(std::vector<name_entry> entries) : entries_(std::move(entries)) {}

  const std::vector<name_entry>& entries() const {
    return entries_;
  }

  /// @brief The value of `name`, or nullopt.
  std::optional<int64_t> find(std::string_view name) const;

  /// @brief Whether some entry has exactly `value`.
  bool has_value(int64_t value) const;

  /// @brief "a, b, c": every name, for error messages and help.
  std::string names() const;

  /// @brief Parses `A|B|C` (spaces around names allowed) into the OR of
  ///        their values.
  /// @return nullopt and the offending name in `unknown` if a name is not
  ///         in the table (an empty part is reported as "").
  std::optional<int64_t> parse_flags(std::string_view text, std::string& unknown) const;

  /// @brief The name of `value`, or its decimal form if it has none.
  std::string format_enum(int64_t value) const;

  /// @brief `value` as `A|B`, taking names in table order whose bits are all
  ///         still unexplained; leftover bits are appended as a number. 0 is
  ///         the name with value 0, or "0"; a negative value (a sentinel,
  ///         not bits) is its name or its decimal form.
  std::string format_flags(int64_t value) const;

 private:
  std::vector<name_entry> entries_;
};

} // namespace bdg::nymph
