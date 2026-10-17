// MIT License © 2026 Binary Dice Games
/// @file yaml_doc.hpp
/// @brief A YAML source parsed into an immutable tree in which every node
///        knows its line and column. The only place libyaml is used.
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace bdg::nymph {

/// @brief How a scalar was written. Quoted scalars are text whatever they
///        look like (`"2025"`); block scalars (`|`, `>`) carry multi-line text.
enum class scalar_style { plain, single_quoted, double_quoted, literal, folded };

/// @brief One node of a parsed YAML document.
///
/// A mapping keeps its entries in source order as two parallel vectors,
/// `keys` and `children` (key `i` maps to value `i`); a sequence keeps its
/// items in `children`; a scalar has `text` and `style`. Aliases are
/// expanded into copies, so the tree has no shared nodes.
struct yaml_node {
  enum class kind_t { scalar, sequence, mapping };

  kind_t kind = kind_t::scalar;
  std::string text; ///< Scalar value, after YAML unescaping/folding.
  scalar_style style = scalar_style::plain; ///< Scalars only.
  int line = 0; ///< 1-based start of the node.
  int column = 0; ///< 1-based, in characters.
  /// For a literal (`|`) or folded (`>`) block scalar: the 1-based source
  /// line of the first line of its text, and the 1-based column where its
  /// text starts (its indentation + 1; 0 if the scalar is empty). Line `k`
  /// (0-based) of a literal scalar's text is source line `content_line + k`.
  int content_line = 0;
  int content_column = 0;
  std::vector<yaml_node> keys; ///< Mapping keys, in source order.
  std::vector<yaml_node> children; ///< Mapping values (parallel to keys) or sequence items.

  bool is_scalar() const {
    return kind == kind_t::scalar;
  }
  bool is_sequence() const {
    return kind == kind_t::sequence;
  }
  bool is_mapping() const {
    return kind == kind_t::mapping;
  }
  /// @brief True for single- or double-quoted scalars.
  bool quoted() const {
    return style == scalar_style::single_quoted || style == scalar_style::double_quoted;
  }
  /// @brief Mapping lookup by scalar key text; nullptr if absent or not a
  ///        mapping.
  const yaml_node* find(std::string_view key) const;
  /// @brief The key node for a mapping key text, for error positions;
  ///        nullptr if absent.
  const yaml_node* find_key(std::string_view key) const;
};

/// @brief Largest number of nodes a document may expand to (aliases
///        included), so a few nested aliases cannot exhaust memory.
inline constexpr size_t kMaxYamlNodes = 4u * 1000u * 1000u;

/// @brief Parses `text` as exactly one YAML document.
/// @return The root node.
/// @throws nymph::error, positioned, for: a YAML syntax error (with
///         libyaml's description; tab indentation gets its own message),
///         a duplicate mapping key (at the second occurrence), a non-scalar
///         mapping key, more than one document (at the second), an empty
///         document, or more than kMaxYamlNodes nodes.
yaml_node parse_yaml(std::string_view text);

} // namespace bdg::nymph
