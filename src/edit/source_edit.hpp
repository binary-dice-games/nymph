// MIT License © 2026 Binary Dice Games
/// @file source_edit.hpp
/// @brief Text patches to a nymph YAML source: replace, add or remove one
///        field of one element, leaving every other byte as written
///        (spec/08_view_editor/DESIGN.md section 3, `source_edit`). Works on
///        the parsed positions only; it does not know element classes, so
///        the caller validates the result (bind_source()).
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include "document/yaml_doc.hpp"

namespace bdg::nymph {

/// @brief The element mapping at `path` ("root", "root/children/1", ...) in
///        a parsed source, or nullptr when the source has no such element.
const yaml_node* find_element(const yaml_node& doc, std::string_view path);

/// @brief The column reference (`$names`, `$table:column`) field `field` of
///        the element at `element_path` is written as, or nullopt when the
///        element, the field or a reference is not there.
std::optional<std::string>
field_reference(const yaml_node& doc, std::string_view element_path, std::string_view field);

/// @brief Sets field `field` of the element at `element_path` to
///        `yaml_value`, replacing the value in place, or adding the key to
///        the element's mapping (flow `{...}` or block style, as the
///        element is written) when it is absent.
/// @param yaml_value One line of YAML (a scalar or a flow list), already
///        quoted as needed.
/// @return The new source.
/// @throws nymph::error for a source that does not parse or whose top
///         level is not a mapping, an unknown element path, a multi-line
///         `yaml_value`, or an element that is not a mapping.
std::string
set_field(std::string_view source, std::string_view element_path, std::string_view field, std::string_view yaml_value);

/// @brief Removes field `field` from the element at `element_path`; the
///        source is returned unchanged when the field is not written.
/// @throws nymph::error as set_field(), and for the first key of a list
///         item written on the line of its dash.
std::string remove_field(std::string_view source, std::string_view element_path, std::string_view field);

} // namespace bdg::nymph
