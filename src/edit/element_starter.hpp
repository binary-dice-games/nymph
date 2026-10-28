// MIT License © 2026 Binary Dice Games
/// @file element_starter.hpp
/// @brief The YAML of a new element: the least a type needs to be valid
///        where it goes (a series' data, a link's ends, a fresh id)
///        (spec/14_editor_workspace/DESIGN.md section 3, `element_starter`).
#pragma once

#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "schema/element_class.hpp"

namespace bdg::nymph {

/// @brief The fields a new element of `type` starts with, written as the
///        inside of a flow mapping (`ys: [1, 3, 2]`); empty when the type
///        needs none. `?` marks a value the editor fills with an id of the
///        diagram (a link's `from` and `to`).
/// @param diagram The type of the figure the element goes in: a few types
///        need other fields in some diagrams (a `Note` in a
///        `SequenceDiagram` spans a participant).
std::string_view starter_fields(std::string_view type, std::string_view diagram = {});

/// @brief A new element, ready to insert.
struct new_element {
  std::string yaml; ///< One line of flow YAML: `{type: X, id: node3, ...}`.
  std::string id; ///< The fresh id it was given; empty when its type has no `id`.
  /// Fields left empty (`""`) because there were too few ids to fill
  /// them; the element does not bind until they are given.
  std::vector<std::string> unfilled;
};

/// @brief Builds a new element of `cls`.
/// @param used_ids Ids already in the source: a fresh `id` avoids them.
/// @param candidate_ids Ids the `?` fields take in order (the selected node
///        first, then the others of the diagram); a second `?` takes the
///        next one.
/// @param diagram As starter_fields().
new_element make_element(
    const element_class& cls,
    const std::set<std::string>& used_ids,
    const std::vector<std::string>& candidate_ids,
    std::string_view diagram = {});

/// @brief The base of the fresh ids of `type`: "node" for node types
///        (`FlowNode`, `ClassNode`), else the type in lower case.
std::string id_base(std::string_view type);

} // namespace bdg::nymph
