// MIT License © 2026 Binary Dice Games
/// @file yaml_language.hpp
/// @brief The source pane's highlighting of nymph YAML
///        (spec/15_web_app/DESIGN.md section 5.4): comments, mapping keys,
///        strings, numbers, true / false / null, element type names and
///        `$column` references.
#pragma once

#include <TextEditor.h>
#include <string>
#include <string_view>
#include <vector>

namespace bdg::nymph {

/// @brief The language definition (built once). Keys show as keywords,
///        element type names (from the built-in registry) as known
///        identifiers, `$name` references as preprocessor text.
const TextEditor::Language* yaml_language();

/// @brief One highlighted token of a line, for tests.
struct yaml_token {
  std::string text;
  TextEditor::Color color;
};

/// @brief The tokens yaml_language() gives `line` (whitespace dropped).
std::vector<yaml_token> yaml_tokens(std::string_view line);

} // namespace bdg::nymph
