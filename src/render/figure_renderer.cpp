// MIT License © 2026 Binary Dice Games
/// @file figure_renderer.cpp
/// @brief render_frames(): the platform and renderer backend nymph plays for
///        its private ImGui context.
#include "render/figure_renderer.hpp"
#include <imgui.h>
#include <implot.h>
#include <implot3d.h>
#include <cfloat>
#include <stdexcept>
#include <string>
#include "layout/layout.hpp"
#include "render/soft_rasterizer.hpp"
#include "render/texture_store.hpp"
#include "schema/element_class.hpp"

namespace bdg::nymph {

namespace {

// ── Themes ───────────────────────────────────────────────────────────────────

struct theme {
  const char* name;
  void (*imgui)(ImGuiStyle*);
  void (*implot)(ImPlotStyle*);
  void (*implot3d)(ImPlot3DStyle*);
};

constexpr theme kThemes[] = {
    {"light", &ImGui::StyleColorsLight, &ImPlot::StyleColorsLight, &ImPlot3D::StyleColorsLight},
    {"dark", &ImGui::StyleColorsDark, &ImPlot::StyleColorsDark, &ImPlot3D::StyleColorsDark},
    {"classic", &ImGui::StyleColorsClassic, &ImPlot::StyleColorsClassic, &ImPlot3D::StyleColorsClassic},
};

const theme* find_theme(const std::string& name) {
  for (const theme& t : kThemes) {
    if (name == t.name) {
      return &t;
    }
  }
  return nullptr;
}

void check_options(const image_options& o) {
  if (o.width < 16 || o.width > 4096 || o.height < 16 || o.height > 4096) {
    throw std::invalid_argument("image width and height must be between 16 and 4096");
  }
  if (o.scale < 1 || o.scale > 4) {
    throw std::invalid_argument("image scale must be between 1 and 4");
  }
  if (o.width * o.scale > 8192 || o.height * o.scale > 8192) {
    throw std::invalid_argument("image width and height times scale must not exceed 8192");
  }
  if (o.padding < 0 || o.padding * 2 >= o.width || o.padding * 2 >= o.height) {
    throw std::invalid_argument("image padding does not fit the image size");
  }
  if (find_theme(o.theme) == nullptr) {
    throw std::invalid_argument("unknown theme '" + o.theme + "' (themes: light, dark, classic)");
  }
}

// ── Context scope ────────────────────────────────────────────────────────────

/// Creates the private context trio and makes it current; on destruction
/// destroys it and makes the previous contexts current again.
class context_scope {
 public:
  context_scope()
      : prev_imgui_(ImGui::GetCurrentContext()),
        prev_implot_(ImPlot::GetCurrentContext()),
        prev_implot3d_(ImPlot3D::GetCurrentContext()) {
    imgui_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(imgui_);
    implot_ = ImPlot::CreateContext();
    ImPlot::SetCurrentContext(implot_);
    implot3d_ = ImPlot3D::CreateContext();
    ImPlot3D::SetCurrentContext(implot3d_);
  }

  ~context_scope() {
    ImGui::SetCurrentContext(imgui_);
    ImPlot3D::DestroyContext(implot3d_);
    ImPlot::DestroyContext(implot_);
    ImGui::DestroyContext(imgui_);
    ImGui::SetCurrentContext(prev_imgui_);
    ImPlot::SetCurrentContext(prev_implot_);
    ImPlot3D::SetCurrentContext(prev_implot3d_);
  }

  context_scope(const context_scope&) = delete;
  context_scope& operator=(const context_scope&) = delete;

 private:
  ImGuiContext* prev_imgui_;
  ImPlotContext* prev_implot_;
  ImPlot3DContext* prev_implot3d_;
  ImGuiContext* imgui_ = nullptr;
  ImPlotContext* implot_ = nullptr;
  ImPlot3DContext* implot3d_ = nullptr;
};

/// Configures the current (fresh) context as nymph's backend.
void configure(const theme& t) {
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.LogFilename = nullptr;
  io.BackendPlatformName = "nymph";
  io.BackendRendererName = "nymph_soft_rasterizer";
  io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures | ImGuiBackendFlags_RendererHasVtxOffset;
  ImGui::GetPlatformIO().DrawCallback_ResetRenderState = &reset_render_state_callback;

  io.Fonts->TexDesiredFormat = ImTextureFormat_Alpha8;
  io.Fonts->AddFontDefaultVector();

  ImGuiStyle& style = ImGui::GetStyle();
  t.imgui(&style);
  t.implot(&ImPlot::GetStyle());
  t.implot3d(&ImPlot3D::GetStyle());
  style.FontSizeBase = kFigureFontSize;
  // ImGui's textured line anti-aliasing relies on GPU-exact bilinear
  // filtering of a small line texture; geometry fringes need no texture
  // and rasterize evenly (lesson from wish nymph).
  style.AntiAliasedLinesUseTex = false;
  style.WindowRounding = 0.0f;
  style.WindowBorderSize = 0.0f;
  style.WindowPadding = ImVec2(0.0f, 0.0f);
}

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
  constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollWithMouse |
      ImGuiWindowFlags_NoBringToFrontOnFocus;
  for (int frame = 0; frame < frames; ++frame) {
    set_frame_inputs(options);
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
    ImGui::Begin("##nymph_figure", nullptr, kFlags);
    draw(ctx);
    ImGui::End();
    ImGui::Render();
    // Texture requests are answered every frame, as a GPU backend would.
    textures.apply_all(*ImGui::GetDrawData());
  }
}

} // namespace

image render_frames(const image_options& options, const draw_fn& draw) {
  check_options(options);
  context_scope scope;
  configure(*find_theme(options.theme));
  texture_store textures;
  draw_context ctx(options);
  run_frames(options, draw, kFigureFrames, textures, ctx);

  ImVec4 bg = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
  bg.w = 1.0f; // the image is opaque
  return rasterize(*ImGui::GetDrawData(), textures, ImGui::ColorConvertFloat4ToU32(bg));
}

image render_figure(const figure& f) {
  return render_frames(f.image, [&](draw_context& dc) {
    layout_root(f.root, dc);
    f.root.cls->draw(f.root, dc);
  });
}

std::unordered_map<const node*, rect> layout_figure(const figure& f) {
  check_options(f.image);
  context_scope scope;
  configure(*find_theme(f.image.theme));
  texture_store textures;
  draw_context ctx(f.image);
  run_frames(f.image, [&](draw_context& dc) { layout_root(f.root, dc); }, 1, textures, ctx);
  return ctx.rects();
}

} // namespace bdg::nymph
