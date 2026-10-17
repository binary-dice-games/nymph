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

  /// @brief Names of the classes whose slot is `slot`, "A, B, C", for
  ///        error messages.
  std::string names_in_slot(std::string_view slot) const;

 private:
  std::map<std::string, std::unique_ptr<element_class>, std::less<>> classes_;
};

} // namespace bdg::nymph
