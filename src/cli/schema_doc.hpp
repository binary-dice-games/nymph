// MIT License © 2026 Binary Dice Games
/// @file schema_doc.hpp
/// @brief The element reference printed by `nymph schema`, generated from
///        the registry so it can never disagree with what binding accepts.
#pragma once

#include <string>
#include "schema/element_class.hpp"
#include "schema/registry.hpp"

namespace bdg::nymph {

/// @brief Every class in reading order: `figure` elements first, then the
///        children of each container slot, each group by name.
std::vector<const element_class*> classes_in_doc_order(const registry& reg);

/// @brief Plain-text reference of one class: name, where it goes, what it
///        takes, description, and one line per field (name, type, default,
///        names, range, description).
std::string schema_text(const element_class& cls, const registry& reg);

/// @brief Plain-text reference of every class (schema_text() of each).
std::string schema_text(const registry& reg);

/// @brief Markdown reference of every class: the content of
///        docs/elements.md.
std::string schema_markdown(const registry& reg);

/// @brief A field's default as it would be written in a source.
std::string format_default(const field_desc& f);

} // namespace bdg::nymph
