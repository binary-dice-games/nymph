// MIT License © 2026 Binary Dice Games
/// @file legacy.hpp
/// @brief Reading wish nymph sources: three parts (description, format YAML,
///        CSV) separated by `--` lines, converted to nymph's single YAML
///        document.
#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace bdg::nymph {

/// @brief Whether `text` is a wish nymph source: it has two separator lines
///        (`--`, trailing whitespace ignored) and is not a YAML mapping with
///        a `root` key. A nymph source with `--` lines inside a block
///        scalar is valid YAML, so it is never taken for a wish source.
bool is_wish_source(std::string_view text);

/// @brief Converts a wish nymph source to nymph YAML: `nymph: 1`, the
///        description as `description: |-`, the format part verbatim, the
///        CSV as `data: |`. Empty parts are left out; line breaks inside the
///        block scalars become `\n`.
/// @return nullopt if `text` is not a wish source (see is_wish_source()).
std::optional<std::string> convert_wish_source(std::string_view text);

} // namespace bdg::nymph
