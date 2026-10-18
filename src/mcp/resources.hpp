// MIT License © 2026 Binary Dice Games
/// @file resources.hpp
/// @brief The documents `nymph mcp` hands to clients: the server
///        instructions and the `nymph://docs/` resources
///        (spec/06_claude_integration/DESIGN.md sections 4.5 and 5.2).
#pragma once

#include <string>
#include <string_view>
#include "mcp/json_rpc.hpp"

namespace bdg::nymph::mcp {

/// @brief The `initialize` instructions: `claude/skill/nymph/SKILL.md`
///        without its front matter, as compiled in.
std::string server_instructions();

/// @brief The URI of the MCP App page.
inline constexpr const char* kViewerUri = "ui://nymph/viewer";

/// @brief The MCP App page compiled in with `NYMPH_MCP_VIEWER`, or empty.
std::string_view embedded_viewer_html();

/// @brief The `resources/list` result; the App page is listed when
///        `viewer_html` is not empty.
json list_resources(std::string_view viewer_html);

/// @brief Runs `resources/read`.
/// @return `{"contents": [{uri, mimeType, text, ...}]}`.
/// @throws rpc_error invalid_params without a string `uri`;
///         resource_not_found for a URI the server does not have.
json read_resource(const json& params, std::string_view viewer_html);

} // namespace bdg::nymph::mcp
