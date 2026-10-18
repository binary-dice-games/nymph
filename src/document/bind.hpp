// MIT License © 2026 Binary Dice Games
/// @file bind.hpp
/// @brief bind_source(): a nymph source to a fully checked figure.
///
/// Not called `bind`: with a std::string argument, argument-dependent
/// lookup would pick std::bind.
#pragma once

#include <string>
#include <string_view>
#include "render/image_options.hpp"
#include "schema/node.hpp"
#include "schema/registry.hpp"

namespace bdg::nymph {

/// @brief A bound source: everything the renderer needs, already checked.
///
/// Nodes point at classes owned by the registry passed to bind(), which
/// must outlive the figure.
struct figure {
  image_options image;
  source_pos image_font_pos; ///< Where `image.font` was written; line 0 if not.
  std::string description;
  node root;
};

/// @brief Parses and checks a source (spec/01_core_features/DESIGN.md section 3).
///
/// Reads the top-level keys (`nymph`, `description`, `image`, `root`,
/// `data`, and `source`, which is accepted as any value and ignored),
/// parses `data` (one table or named tables), walks the element tree
/// checking every type, placement (slot), field, value, name, range and
/// colour, and replaces every `$column` / `$table:column` reference with
/// literal data. Pure: no file access, no ImGui.
///
/// @param source The whole source text.
/// @param reg    The element types the source may use.
/// @return The figure.
/// @throws nymph::error, positioned in the source (line 0 for a size
///         limit), for the first problem found.
figure bind_source(std::string_view source, const registry& reg);

} // namespace bdg::nymph
