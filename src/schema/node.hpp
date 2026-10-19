// MIT License © 2026 Binary Dice Games
/// @file node.hpp
/// @brief One element of a bound figure: its class, field values and
///        children.
#pragma once

#include <cstddef>
#include <string>
#include <variant>
#include <vector>
#include "schema/value.hpp"

namespace bdg::nymph {

class element_class;

/// @brief A position in the source; line 0 means "not written there".
struct source_pos {
  int line = 0;
  int column = 0;
};

/// @brief An element instance, fully checked by bind(): `values[i]` holds
///        field `i` of `cls->fields()` with that field's value alternative.
struct node {
  const element_class* cls = nullptr;
  std::vector<value> values;
  std::vector<node> children;
  int line = 0; ///< Source position of the element's mapping.
  int column = 0;
  /// Where each field's value was written (parallel to `values`); line 0
  /// for a field left at its default. For error messages from
  /// element_class::check().
  std::vector<source_pos> field_pos;
  /// Index in `cls->slots()` of the slot the element was placed in
  /// (element_class::placed_as()); 0, the primary slot, unless its parent
  /// took it as another.
  unsigned placed_as = 0;

  /// @brief Field `index` as `T`; throws std::bad_variant_access if the
  ///        field holds another alternative (a bug, never input-dependent).
  template <typename T>
  const T& get(size_t index) const {
    return std::get<T>(values.at(index));
  }
};

} // namespace bdg::nymph
