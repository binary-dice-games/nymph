// MIT License © 2026 Binary Dice Games
/// @file mcp_server.hpp
/// @brief `nymph mcp`: the MCP server's stdio loop and its command line
///        (spec/06_claude_integration/DESIGN.md sections 4.1 and 5.3).
#pragma once

#include <iosfwd>
#include <span>
#include <string_view>
#include "mcp/mcp_session.hpp"

namespace bdg::nymph::mcp {

/// @brief Serves one MCP session: reads one JSON-RPC message per line of
///        `in` and writes one response per line to `out`, until `in` ends.
/// @param log Diagnostics; never protocol messages.
/// @return 0 when `in` ends; 1 when `out` cannot be written. Never throws
///         for a bad message: it gets an error response.
int run_mcp(std::istream& in, std::ostream& out, std::ostream& log, const server_options& options = {});

/// @brief `nymph mcp [--rasterizer software|opengl]`: parses `args` (the
///        arguments after `mcp`) and runs run_mcp().
/// @return run_mcp()'s code; 0 for --help; 2 for bad arguments (message
///         and usage on `err`).
int run_mcp_cli(std::span<const std::string_view> args, std::istream& in, std::ostream& out, std::ostream& err);

} // namespace bdg::nymph::mcp
