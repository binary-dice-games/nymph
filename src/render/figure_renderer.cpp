// MIT License © 2026 Binary Dice Games
/// @file figure_renderer.cpp
/// @brief render_frames(): the platform and renderer backend nymph plays for
///        its private ImGui context.
#include "render/figure_renderer.hpp"
#include <imgui.h>
#include <cfloat>
#include "fonts/fonts_used.hpp"
#include "layout/layout.hpp"
#include "render/frame_setup.hpp"
#include "render/raster_backend.hpp"
#include "render/text_style.hpp"
#include "render/texture_store.hpp"

namespace bdg::nymph {

namespace {

/// nymph answers texture requests itself (texture_store) and both
/// rasterizers honour vertex offsets.
constexpr ImGuiBackendFlags kPngBackendFlags =
    ImGuiBackendFlags_RendererHasTextures | ImGuiBackendFlags_RendererHasVtxOffset;

/// The same inputs every frame and every call: no clock, no mouse.
void set_frame_inputs(const image_options& options) {
  ImGuiIO& io = ImGui::GetIO();
  io.DisplaySize = ImVec2(static_cast<float>(options.width), static_cast<float>(options.height));
  io.DisplayFramebufferScale = ImVec2(static_cast<float>(options.scale), static_cast<float>(options.scale));
  io.DeltaTime = 1.0f / 60.0f;
  io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
}

/// Runs `frames` frames of `draw` in the current (configured) context.
void run_frames(
    const image_options& options,
    const draw_fn& draw,
    int frames,
    texture_store& textures,
    draw_context& ctx) {
  for (int frame = 0; frame < frames; ++frame) {
    set_frame_inputs(options);
    run_figure_frame(options, ctx, draw);
    // Texture requests are answered every frame, as a GPU backend would.
    textures.apply_all(*ImGui::GetDrawData());
  }
}

} // namespace

image render_frames(const image_options& options, const draw_fn& draw, raster_backend& backend) {
  check_options(options);
  context_scope scope;
  configure(*find_theme(options.theme), backend.name(), kPngBackendFlags);
  texture_store textures;
  draw_context ctx(options);
  load_fonts(ctx);
  run_frames(options, draw, kFigureFrames, textures, ctx);

  ImVec4 bg = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
  bg.w = 1.0f; // the image is opaque
  return backend.rasterize(*ImGui::GetDrawData(), textures, ImGui::ColorConvertFloat4ToU32(bg));
}

image render_frames(const image_options& options, const draw_fn& draw) {
  software_backend software;
  return render_frames(options, draw, software);
}

image render_figure(const figure& f, raster_backend& backend) {
  require_fonts(f);
  // The saved view (the `view` key) and UI state are found by element path.
  std::unordered_map<const node*, std::string> paths;
  for (const auto& [path, n] : element_paths(f.root)) {
    paths[n] = path;
  }
  return render_frames(
      f.image,
      [&](draw_context& dc) {
        dc.set_views(&f.views, &paths);
        layout_root(f.root, dc);
        dc.draw_node(f.root);
      },
      backend);
}

image render_figure(const figure& f) {
  software_backend software;
  return render_figure(f, software);
}

std::unordered_map<const node*, rect> layout_figure(const figure& f) {
  check_options(f.image);
  require_fonts(f);
  context_scope scope;
  configure(*find_theme(f.image.theme), "nymph_soft_rasterizer", kPngBackendFlags);
  texture_store textures;
  draw_context ctx(f.image);
  load_fonts(ctx);
  run_frames(f.image, [&](draw_context& dc) { layout_root(f.root, dc); }, 1, textures, ctx);
  return ctx.rects();
}

} // namespace bdg::nymph
