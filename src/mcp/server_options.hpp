// MIT License © 2026 Binary Dice Games
/// @file server_options.hpp
/// @brief What `nymph mcp` was started with.
#pragma once

#include <cstddef>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include "nymph.hpp"
#include "render/renderer_options.hpp"

namespace bdg::nymph::mcp {

class tool_registry;

/// @brief What `nymph mcp` was started with.
struct server_options {
  rasterizer raster = rasterizer::opengl; ///< --rasterizer (default opengl, falling back silently)
  bool raster_explicit = false; ///< --rasterizer was given: warn on an OpenGL fallback
  /// Results whose image would exceed this much base64 leave the image out
  /// (DESIGN.md section 5.1). Tests lower it.
  size_t max_image_base64 = 8u * 1024u * 1024u;
  renderer_test_options gl_options; ///< tests: simulate machines without OpenGL
  /// The MCP App page (DESIGN.md section 4.2); empty for a PNG-only server.
  /// `nymph mcp` passes embedded_viewer_html(); tests pass their own.
  std::string_view viewer_html;
  /// The editor's own session (spec/14_editor_workspace/DESIGN.md section
  /// 3): no chart tools, only those `extra_tools` adds, and its own name and
  /// instructions. Empty for `nymph mcp`.
  std::function<void(tool_registry&)> extra_tools;
  bool builtin_tools = true; ///< render_chart and the others.
  std::string server_name = "nymph"; ///< serverInfo.name
  std::string instructions; ///< "" for the skill text
  /// Where `nymph mcp`'s editor tools look for running editors; empty for
  /// ipc::editors_dir(). Tests pass their own.
  std::filesystem::path editors_dir;
};

} // namespace bdg::nymph::mcp
