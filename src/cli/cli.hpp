// MIT License © 2026 Binary Dice Games
/// @file cli.hpp
/// @brief The `nymph` command line as a function, so it is tested in-process
///        (DESIGN.md section 6).
#pragma once

#include <iosfwd>
#include <span>
#include <string_view>

namespace bdg::nymph {

/// @brief Runs one `nymph` command.
///
/// Commands: `<in> [-o <out>]` (render YAML, or extract from a PNG, chosen
/// by the input's first bytes), `render`, `extract`, `check`,
/// `schema [<Type>] [--markdown]`, `fonts [--licenses]`, `--version`,
/// `--help`. `-` as input is
/// `in`; `-o -` writes to `out`. Files are written through a temporary file
/// and a rename, so a failure never leaves a partial or changed output.
///
/// @param args Arguments after the program name.
/// @param in   Standard input (binary).
/// @param out  Standard output (binary): extracted YAML, `-o -` output,
///             schema, version, help.
/// @param err  Standard error: one line per failure.
/// @return 0 on success; 1 on failure, with
///         `nymph: <file>:<line>:<column>: <message>` (a problem in the
///         source) or `nymph: <file>: <message>` on `err`; 2 with the usage
///         for bad arguments.
int run_cli(std::span<const std::string_view> args, std::istream& in, std::ostream& out, std::ostream& err);

} // namespace bdg::nymph
