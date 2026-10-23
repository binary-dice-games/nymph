// MIT License © 2026 Binary Dice Games
/// @file bind.hpp
/// @brief bind_source(): a nymph source to a fully checked figure.
///
/// Not called `bind`: with a std::string argument, argument-dependent
/// lookup would pick std::bind.
#pragma once

#include <string>
#include <string_view>
#include "document/view.hpp"
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
  /// The `view` key by element path (spec/04_web_viewer/DESIGN.md
  /// section 3, "Saved view"); entries for missing elements are dropped.
  view_map views;
};

struct chart_theme;

/// @brief What bind_source() may change about a source.
struct bind_options {
  /// A chart theme to use instead of the source's `image.theme`
  /// (themes/theme_catalog.hpp); empty for the source's own.
  std::string theme;
  /// A theme that is not built in (tests, tools), used instead of both;
  /// must outlive the call.
  const chart_theme* custom_theme = nullptr;
};

/// @brief Parses and checks a source (spec/01_core_features/DESIGN.md section 3).
///
/// Reads the top-level keys (`nymph`, `description`, `image`, `root`,
/// `data`, and `source`, which is accepted as any value and ignored),
/// parses `data` (one table or named tables), walks the element tree
/// checking every type, placement (slot), field, value, name, range and
/// colour, and replaces every `$column` / `$table:column` reference with
/// literal data. Every field and image option the source leaves out takes
/// the chart theme's value, when it has one and no enclosing element
/// writes that field (docs/themes.md). Pure: no file access, no ImGui.
///
/// @param source The whole source text.
/// @param reg    The element types the source may use.
/// @param options A theme override.
/// @return The figure.
/// @throws nymph::error, positioned in the source (line 0 for a size
///         limit), for the first problem found; std::invalid_argument for
///         an unknown theme in `options`.
figure bind_source(std::string_view source, const registry& reg, const bind_options& options = {});

/// @brief A source whose top-level `data` value was replaced.
struct data_replacement {
  std::string source; ///< The new source text.
  int first_line = 0; ///< 1-based line of the new data's first line in `source`.
  int line_count = 0; ///< Lines of the new data.
};

/// @brief Replaces the top-level `data` value of `source` with
///        `data_yaml` (YAML or JSON, the value only), keeping every other
///        line as written; appends `data` when the source has none. The new
///        value goes below a `data:` line, indented by two spaces.
/// @throws nymph::error if `source` does not parse, or its top level is
///         not a block mapping (`key: value` lines starting at column 1).
data_replacement replace_data(std::string_view source, std::string_view data_yaml);

/// @brief replace_data() for any top-level key: replaces (or appends) the
///        value of `key`; an empty `value_yaml` removes the key.
/// @throws nymph::error as replace_data().
data_replacement replace_top_level(std::string_view source, std::string_view key, std::string_view value_yaml);

} // namespace bdg::nymph
