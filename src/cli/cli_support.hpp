// MIT License © 2026 Binary Dice Games
/// @file cli_support.hpp
/// @brief File input and output, and the text of `nymph schema`,
///        `nymph fonts` and `nymph example`, shared by the command line and the MCP server
///        (spec/06_claude_integration/DESIGN.md section 5.1).
#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>
#include "nymph.hpp"

namespace bdg::nymph {

/// @brief A failure about one file (or command), reported as
///        `<file>: <message>`.
struct cli_failure {
  std::string file;
  std::string message;
};

using bytes_t = std::vector<uint8_t>;

/// @brief The name of an input in messages: `<stdin>` for "-".
std::string label_of(const std::string& path);

/// @brief Reads `path` ("-" for `in`), at most 512 MiB.
/// @throws cli_failure for a missing, unreadable or too large file.
bytes_t read_input(const std::string& path, std::istream& in);

/// @brief Writes `data` to `path` ("-" for `out`) through a temporary file
///        and a rename, so the target is either the new content or
///        untouched. A symbolic link is followed (the file it points to is
///        replaced); a device or pipe is written directly.
/// @throws cli_failure when the file cannot be written.
void write_output(const std::string& path, std::string_view data, std::ostream& out);

/// @brief The bytes as text, without copying.
std::string_view as_text(const bytes_t& bytes);

/// @brief The YAML a command works on: `input` itself, or the source a
///        nymph PNG carries.
/// @param path The input's name, for messages.
/// @throws cli_failure for a corrupt PNG or a PNG without a nymph source.
std::string source_of(const bytes_t& input, const std::string& path);

/// @brief The nymph source of an input and how it was obtained
///        (spec/16_mermaid_import/DESIGN.md section 3, `load_source()`).
struct loaded_source {
  std::string source;                   ///< nymph YAML
  std::string mermaid_kind;             ///< the mermaid keyword when converted, else empty
  std::vector<import_warning> warnings; ///< what the mermaid import dropped
};

/// @brief The nymph source of `input`: the source a nymph PNG carries,
///        mermaid converted (import_mermaid()), or the text itself.
/// @throws cli_failure as source_of(); nymph::error positioned in the
///         mermaid for a mermaid problem.
loaded_source load_source(const bytes_t& input, const std::string& path);

/// @brief The same for text (no PNG): mermaid converted, else as is.
loaded_source load_source_text(std::string_view text);

/// @brief One warning as `<label>:<line>:<column>: warning: <message>`.
std::string warning_text(const std::string& label, const import_warning& w);

/// @brief `nymph fonts`: `default` and every catalog font, one per line,
///        those this build does not embed marked `(not embedded)`.
std::string fonts_text();

/// @brief The `nymph shapes [<name>]` text (also the MCP tool `list_shapes`):
///        every shape as "category/name", or the details of one.
/// @throws cli_failure for an unknown shape name.
std::string shapes_listing(const std::string& name);

/// @brief The `nymph themes [<name>]` text (also the MCP tool
///        `list_themes`): every chart theme, one per line ("ocean  Ocean
///        (dark): ..."), or the YAML file of the one named.
/// @throws cli_failure for an unknown theme name.
std::string themes_listing(const std::string& name);

/// @brief `source` with `image.theme` set to `theme` (`--theme`), every
///        other byte as written.
/// @throws cli_failure for an unknown theme; nymph::error for a source
///         that does not parse.
std::string source_with_theme(std::string_view source, const std::string& theme);

/// @brief `nymph schema <type>`: the reference of one element type.
/// @throws cli_failure (file "schema") for an unknown type, with the
///         closest name and the list of types.
std::string schema_type_text(const std::string& type);

/// @brief `nymph example [<name or Type>]`: a complete, working chart
///        source to start from.
///
/// With an empty `query`, one line per example: its name, its description
/// and the element types it uses. With an example's name, that source. With
/// an element type, the smallest example that uses it, then the names of the
/// others. The sources are the files of `examples/`, compiled in.
/// @throws cli_failure (file "example") for a name or type no example has,
///         with the closest name and the list of examples.
std::string example_text(const std::string& query);

} // namespace bdg::nymph
