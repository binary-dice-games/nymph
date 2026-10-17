// MIT License © 2026 Binary Dice Games
/// @file soft_rasterizer.hpp
/// @brief CPU rasterizer for Dear ImGui draw data: nymph's renderer backend.
///
/// Written for ImGui 1.92. emilk's imgui_software_renderer
/// (https://github.com/emilk/imgui_software_renderer, public domain) was the
/// reference for the structure and the fast paths; see
/// spec/01_core_features/DESIGN.md section 4, "soft_rasterizer", for what was
/// taken from it and what is done differently.
#pragma once

#include <imgui.h>
#include "render/image.hpp"
#include "render/texture_store.hpp"

namespace bdg::nymph {

/// @brief Options for rasterize().
struct raster_options {
  /// Use the rectangle, text-quad and blend-cache fast paths. They give the
  /// same bytes as the general path; turning them off is for tests.
  bool fast_paths = true;
};

/// @brief Draws ImGui draw data into a new image.
///
/// The image is `round(DisplaySize * FramebufferScale)` pixels, cleared to
/// `clear_color` (an `IM_COL32` value) before drawing. Triangles are filled
/// with the top-left rule at pixel centres (fixed point, 8 sub-pixel bits),
/// textures are sampled bilinearly, and colours are blended with ImGui's
/// straight-alpha "over" blend in 8-bit integer arithmetic, so the result is
/// repeatable bit for bit.
///
/// @param draw_data  `ImGui::GetDrawData()` or hand-built draw data.
/// @param textures   Every texture the draw commands refer to.
/// @param clear_color Background, `IM_COL32(r, g, b, a)`.
/// @param options    See raster_options.
/// @return The rendered pixels.
/// @throws std::runtime_error if a command refers to a texture not in
///         `textures`, or carries a draw callback other than a reset-render-
///         state request.
image rasterize(
    const ImDrawData& draw_data,
    const texture_store& textures,
    ImU32 clear_color,
    const raster_options& options = {});

/// @brief No-op draw callback nymph installs as
///        `ImGuiPlatformIO::DrawCallback_ResetRenderState`; rasterize()
///        skips commands carrying it (and the legacy
///        `ImDrawCallback_ResetRenderState` value).
void reset_render_state_callback(const ImDrawList* list, const ImDrawCmd* cmd);

} // namespace bdg::nymph
