// MIT License © 2026 Binary Dice Games
/// @file cli_support.hpp
/// @brief File input and output, and the text of `nymph schema` and
///        `nymph fonts`, shared by the command line and the MCP server
///        (spec/06_claude_integration/DESIGN.md section 5.1).
#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

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

/// @brief `nymph fonts`: `default` and every catalog font, one per line,
///        those this build does not embed marked `(not embedded)`.
std::string fonts_text();

/// @brief The `nymph shapes [<name>]` text (also the MCP tool `list_shapes`):
///        every shape as "category/name", or the details of one.
/// @throws cli_failure for an unknown shape name.
std::string shapes_listing(const std::string& name);

/// @brief `nymph schema <type>`: the reference of one element type.
/// @throws cli_failure (file "schema") for an unknown type, with the
///         closest name and the list of types.
std::string schema_type_text(const std::string& type);

} // namespace bdg::nymph
