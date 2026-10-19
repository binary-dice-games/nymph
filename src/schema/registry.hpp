// MIT License © 2026 Binary Dice Games
/// @file registry.hpp
/// @brief The element types a source may use, by name.
#pragma once

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include "schema/element_class.hpp"

namespace bdg::nymph {

/// @brief Element classes by type name. A plain object, not a global: the
///        built-in one comes from register_builtin_elements(), tests build
///        their own.
class registry {
 public:
  /// @brief Adds a class.
  /// @throws std::logic_error if a class with the same name exists, or two
  ///         of its fields share a name.
  void add(std::unique_ptr<element_class> cls);

  /// @brief The class named `name`, or nullptr.
  const element_class* find(std::string_view name) const;

  /// @brief Every class, ordered by name.
  std::vector<const element_class*> classes() const;

  /// @brief Names of the classes that fill slot `slot`, "A, B, C", for
  ///        error messages.
  std::string names_in_slot(std::string_view slot) const;

  /// @brief Names of the classes that `parent` takes as children (fill any
  ///        slot it accepts), "A, B, C", each once.
  std::string names_taken_by(const element_class& parent) const;

  /// @brief Names of the classes that accept children of slot `slot`, "A,
  ///        B", for "X goes in: ..." messages.
  std::string names_accepting(std::string_view slot) const;

  /// @brief Names of the classes that accept `child` in any of its slots,
  ///        "A, B", for "X goes in: ..." messages.
  std::string names_accepting(const element_class& child) const;

 private:
  std::map<std::string, std::unique_ptr<element_class>, std::less<>> classes_;
};

} // namespace bdg::nymph
