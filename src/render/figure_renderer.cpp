// MIT License © 2026 Binary Dice Games
/// @file figure_renderer.cpp
/// @brief render_frames(): the platform and renderer backend nymph plays for
///        its private ImGui context.
#include "render/figure_renderer.hpp"
#include <imgui.h>
#include <imuml.h>
#include <implot.h>
#include <implot3d.h>
#include <cfloat>
#include <cstdio>
#include <stdexcept>
#include <string>
#include "fonts/embedded_fonts.hpp"
#include "layout/layout.hpp"
#include "render/raster_backend.hpp"
#include "render/soft_rasterizer.hpp"
#include "render/text_style.hpp"
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
  void (*imuml)(ImUmlStyle*);
};

constexpr theme kThemes[] = {
    {"light", &ImGui::StyleColorsLight, &ImPlot::StyleColorsLight, &ImPlot3D::StyleColorsLight, &ImUml::StyleColorsLight},
    {"dark", &ImGui::StyleColorsDark, &ImPlot::StyleColorsDark, &ImPlot3D::StyleColorsDark, &ImUml::StyleColorsDark},
    {"classic",
     &ImGui::StyleColorsClassic,
     &ImPlot::StyleColorsClassic,
     &ImPlot3D::StyleColorsClassic,
     &ImUml::StyleColorsClassic},
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
  if (const std::string problem = check_font_name(o.font); !problem.empty()) {
    throw std::invalid_argument(problem);
  }
  if (!(o.font_size > 0.0 && o.font_size <= kMaxFontSize)) {
    throw std::invalid_argument("image font size must be above 0 and at most 96");
  }
}

// ── Context scope ────────────────────────────────────────────────────────────

/// Creates the private ImGui, ImPlot, ImPlot3D and ImUml contexts and makes
/// them current; on destruction destroys them and makes the previous
/// contexts current again.
class context_scope {
 public:
  context_scope()
      : prev_imgui_(ImGui::GetCurrentContext()),
        prev_implot_(ImPlot::GetCurrentContext()),
        prev_implot3d_(ImPlot3D::GetCurrentContext()),
        prev_imuml_(ImUml::GetCurrentContext()) {
    imgui_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(imgui_);
    implot_ = ImPlot::CreateContext();
    ImPlot::SetCurrentContext(implot_);
    implot3d_ = ImPlot3D::CreateContext();
    ImPlot3D::SetCurrentContext(implot3d_);
    imuml_ = ImUml::CreateContext();
    ImUml::SetCurrentContext(imuml_);
  }

  ~context_scope() {
    ImGui::SetCurrentContext(imgui_);
    ImUml::DestroyContext(imuml_);
    ImPlot3D::DestroyContext(implot3d_);
    ImPlot::DestroyContext(implot_);
    ImGui::DestroyContext(imgui_);
    ImGui::SetCurrentContext(prev_imgui_);
    ImPlot::SetCurrentContext(prev_implot_);
    ImPlot3D::SetCurrentContext(prev_implot3d_);
    ImUml::SetCurrentContext(prev_imuml_);
  }

  context_scope(const context_scope&) = delete;
  context_scope& operator=(const context_scope&) = delete;

 private:
  ImGuiContext* prev_imgui_;
  ImPlotContext* prev_implot_;
  ImPlot3DContext* prev_implot3d_;
  ImUmlContext* prev_imuml_;
  ImGuiContext* imgui_ = nullptr;
  ImPlotContext* implot_ = nullptr;
  ImPlot3DContext* implot3d_ = nullptr;
  ImUmlContext* imuml_ = nullptr;
};

/// Configures the current (fresh) context as nymph's backend.
void configure(const theme& t, const char* renderer_name) {
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.LogFilename = nullptr;
  io.BackendPlatformName = "nymph";
  io.BackendRendererName = renderer_name;
  io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures | ImGuiBackendFlags_RendererHasVtxOffset;
  ImGui::GetPlatformIO().DrawCallback_ResetRenderState = &reset_render_state_callback;

  io.Fonts->TexDesiredFormat = ImTextureFormat_Alpha8;

  ImGuiStyle& style = ImGui::GetStyle();
  t.imgui(&style);
  t.implot(&ImPlot::GetStyle());
  t.implot3d(&ImPlot3D::GetStyle());
  t.imuml(&ImUml::GetStyle());
  style.FontSizeBase = kFigureFontSize;
  // ImGui's textured line anti-aliasing relies on GPU-exact bilinear
  // filtering of a small line texture; geometry fringes need no texture
  // and rasterize evenly (lesson from wish nymph).
  style.AntiAliasedLinesUseTex = false;
  style.WindowRounding = 0.0f;
  style.WindowBorderSize = 0.0f;
  style.WindowPadding = ImVec2(0.0f, 0.0f);
}

/// Adds ImGui's built-in font (the default) and every embedded font to the
/// atlas and registers them by name. Glyphs are only rasterized when used
/// (ImGui 1.92's dynamic atlas), so unused fonts cost little.
void load_fonts(draw_context& ctx) {
  ImFontAtlas& atlas = *ImGui::GetIO().Fonts;
  ctx.add_font(std::string(kDefaultFontName), atlas.AddFontDefaultVector());
  for (const embedded_file& f : embedded_fonts()) {
    ImFontConfig cfg;
    cfg.FontDataOwnedByAtlas = false; // static data, never freed or written
    std::snprintf(cfg.Name, sizeof(cfg.Name), "%s", f.name);
    // ImGui takes a non-const pointer but only reads the font data.
    void* data = const_cast<unsigned char*>(f.data);
    ImFont* font = atlas.AddFontFromMemoryTTF(data, static_cast<int>(f.size), kFigureFontSize, &cfg);
    if (font == nullptr) {
      throw std::runtime_error(std::string("embedded font ") + f.name + " could not be loaded");
    }
    ctx.add_font(f.name, font);
  }
}

/// The `image` text style: the outermost one, which elements inherit.
text_style image_text_style(const image_options& o) {
  return {o.font.empty() ? std::string(kDefaultFontName) : o.font, o.font_size, o.text_color};
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
    {
      const text_style_scope style(image_text_style(options), ctx);
      draw(ctx);
    }
    ImGui::End();
    ImGui::Render();
    // Texture requests are answered every frame, as a GPU backend would.
    textures.apply_all(*ImGui::GetDrawData());
  }
}

} // namespace

image render_frames(const image_options& options, const draw_fn& draw, raster_backend& backend) {
  check_options(options);
  context_scope scope;
  configure(*find_theme(options.theme), backend.name());
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
  return render_frames(
      f.image,
      [&](draw_context& dc) {
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
  context_scope scope;
  configure(*find_theme(f.image.theme), "nymph_soft_rasterizer");
  texture_store textures;
  draw_context ctx(f.image);
  load_fonts(ctx);
  run_frames(f.image, [&](draw_context& dc) { layout_root(f.root, dc); }, 1, textures, ctx);
  return ctx.rects();
}

} // namespace bdg::nymph
