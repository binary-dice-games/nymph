// MIT License © 2026 Binary Dice Games
/// @file frame_setup.cpp
/// @brief Contexts, theme, backend settings and fonts of a nymph ImGui
///        context.
#include "render/frame_setup.hpp"
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>
#include "fonts/embedded_fonts.hpp"
#include "render/figure_renderer.hpp"
#include "render/soft_rasterizer.hpp"
#include "shapes/shape_catalog.hpp"

namespace bdg::nymph {

namespace {

constexpr theme kThemes[] = {
    {"light",
     &ImGui::StyleColorsLight,
     &ImPlot::StyleColorsLight,
     &ImPlot3D::StyleColorsLight,
     &ImUml::StyleColorsLight},
    {"dark", &ImGui::StyleColorsDark, &ImPlot::StyleColorsDark, &ImPlot3D::StyleColorsDark, &ImUml::StyleColorsDark},
    {"classic",
     &ImGui::StyleColorsClassic,
     &ImPlot::StyleColorsClassic,
     &ImPlot3D::StyleColorsClassic,
     &ImUml::StyleColorsClassic},
};

} // namespace

// ── Themes and options ───────────────────────────────────────────────────────

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
  if (find_theme(o.base) == nullptr) {
    throw std::invalid_argument("unknown base colours '" + o.base + "' (light, dark or classic)");
  }
  if (const std::string problem = check_font_name(o.font); !problem.empty()) {
    throw std::invalid_argument(problem);
  }
  if (!(o.font_size > 0.0 && o.font_size <= kMaxFontSize)) {
    throw std::invalid_argument("image font size must be above 0 and at most 96");
  }
}

// ── Context scope ────────────────────────────────────────────────────────────

imgui_contexts::imgui_contexts() {
  ImGuiContext* const prev_imgui = ImGui::GetCurrentContext();
  ImPlotContext* const prev_implot = ImPlot::GetCurrentContext();
  ImPlot3DContext* const prev_implot3d = ImPlot3D::GetCurrentContext();
  ImUmlContext* const prev_imuml = ImUml::GetCurrentContext();
  imgui_ = ImGui::CreateContext();
  ImGui::SetCurrentContext(imgui_);
  implot_ = ImPlot::CreateContext();
  ImPlot::SetCurrentContext(implot_);
  implot3d_ = ImPlot3D::CreateContext();
  ImPlot3D::SetCurrentContext(implot3d_);
  imuml_ = ImUml::CreateContext();
  ImUml::SetCurrentContext(imuml_);
  register_shapes();
  ImGui::SetCurrentContext(prev_imgui);
  ImPlot::SetCurrentContext(prev_implot);
  ImPlot3D::SetCurrentContext(prev_implot3d);
  ImUml::SetCurrentContext(prev_imuml);
}

imgui_contexts::~imgui_contexts() {
  ImGuiContext* prev_imgui = ImGui::GetCurrentContext();
  ImPlotContext* prev_implot = ImPlot::GetCurrentContext();
  ImPlot3DContext* prev_implot3d = ImPlot3D::GetCurrentContext();
  ImUmlContext* prev_imuml = ImUml::GetCurrentContext();
  ImGui::SetCurrentContext(imgui_);
  ImUml::DestroyContext(imuml_);
  ImPlot3D::DestroyContext(implot3d_);
  ImPlot::DestroyContext(implot_);
  ImGui::DestroyContext(imgui_);
  // Never leave a destroyed context current.
  ImGui::SetCurrentContext(prev_imgui == imgui_ ? nullptr : prev_imgui);
  ImPlot::SetCurrentContext(prev_implot == implot_ ? nullptr : prev_implot);
  ImPlot3D::SetCurrentContext(prev_implot3d == implot3d_ ? nullptr : prev_implot3d);
  ImUml::SetCurrentContext(prev_imuml == imuml_ ? nullptr : prev_imuml);
}

context_switch::context_switch(const imgui_contexts& contexts)
    : prev_imgui_(ImGui::GetCurrentContext()),
      prev_implot_(ImPlot::GetCurrentContext()),
      prev_implot3d_(ImPlot3D::GetCurrentContext()),
      prev_imuml_(ImUml::GetCurrentContext()) {
  ImGui::SetCurrentContext(contexts.imgui());
  ImPlot::SetCurrentContext(contexts.implot());
  ImPlot3D::SetCurrentContext(contexts.implot3d());
  ImUml::SetCurrentContext(contexts.imuml());
}

context_switch::~context_switch() {
  ImGui::SetCurrentContext(prev_imgui_);
  ImPlot::SetCurrentContext(prev_implot_);
  ImPlot3D::SetCurrentContext(prev_implot3d_);
  ImUml::SetCurrentContext(prev_imuml_);
}

// ── Backend settings and fonts ───────────────────────────────────────────────

void configure(const theme& t, const char* renderer_name, ImGuiBackendFlags backend_flags) {
  configure_backend(renderer_name, backend_flags);
  configure_style(t);
}

void configure(const image_options& o, const char* renderer_name, ImGuiBackendFlags backend_flags) {
  configure_backend(renderer_name, backend_flags);
  configure_style(o);
}

void configure_backend(const char* renderer_name, ImGuiBackendFlags backend_flags, bool own_textures) {
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.LogFilename = nullptr;
  io.BackendPlatformName = "nymph";
  // A focused text field (UI mockups, spec/09_ui_mockups) needs no timer:
  // live views stay idle, and the screen matches a saved PNG.
  io.ConfigInputTextCursorBlink = false;
  if (!own_textures) {
    io.Fonts->TexDesiredFormat = ImTextureFormat_RGBA32;
    return;
  }
  io.BackendRendererName = renderer_name;
  io.BackendFlags |= backend_flags;
  ImGui::GetPlatformIO().DrawCallback_ResetRenderState = &reset_render_state_callback;
  io.Fonts->TexDesiredFormat = ImTextureFormat_Alpha8;
}

void configure_style(const theme& t) {
  ImGuiStyle& style = ImGui::GetStyle();
  t.imgui(&style);
  t.implot(&ImPlot::GetStyle());
  // Time axes: unambiguous ISO 8601 dates and a 24-hour clock, in UTC
  // (UseLocalTime stays false, so images do not depend on the machine).
  ImPlot::GetStyle().UseISO8601 = true;
  ImPlot::GetStyle().Use24HourClock = true;
  t.implot3d(&ImPlot3D::GetStyle());
  t.imuml(&ImUml::GetStyle());
  style.FontSizeBase = kFigureFontSize;
  // Textured lines: ImPlot anti-aliases its lines only this way, and the
  // software rasterizer's integer bilinear sampling (soft_rasterizer.cpp,
  // sample()) matches a GPU's filtering within one level, so both
  // rasterizers draw the same smooth lines (spec/01_core_features/DESIGN.md
  // section 7).
  style.AntiAliasedLinesUseTex = true;
  style.WindowRounding = 0.0f;
  style.WindowBorderSize = 0.0f;
  style.WindowPadding = ImVec2(0.0f, 0.0f);
}

namespace {

/// "#RRGGBB[AA]" (checked by bind) as ImGui's floats.
ImVec4 hex_color(const std::string& text) {
  const auto byte = [&](size_t i) { return static_cast<float>(std::stoi(text.substr(i, 2), nullptr, 16)) / 255.0f; };
  return ImVec4(byte(1), byte(3), byte(5), text.size() >= 9 ? byte(7) : 1.0f);
}

} // namespace

void configure_style(const image_options& o) {
  configure_style(*find_theme(o.base));
  if (!o.background.empty()) {
    const ImVec4 bg = hex_color(o.background);
    ImGui::GetStyle().Colors[ImGuiCol_WindowBg] = bg;
    // Plot frames show the figure's background, not the base's frame
    // colour, and plot areas a faint step from it toward the text colour
    // (the base's tint would clash with the theme's background).
    const ImVec4 text = o.text_color.empty() ? ImGui::GetStyle().Colors[ImGuiCol_Text] : hex_color(o.text_color);
    const float k = 0.05f;
    const ImVec4 area(bg.x + (text.x - bg.x) * k, bg.y + (text.y - bg.y) * k, bg.z + (text.z - bg.z) * k, 1.0f);
    ImPlot::GetStyle().Colors[ImPlotCol_FrameBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    ImPlot::GetStyle().Colors[ImPlotCol_PlotBg] = area;
    ImPlot3D::GetStyle().Colors[ImPlot3DCol_FrameBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    ImPlot3D::GetStyle().Colors[ImPlot3DCol_PlotBg] = area;
    const ImVec4 legend(bg.x, bg.y, bg.z, 0.92f);
    ImPlot::GetStyle().Colors[ImPlotCol_LegendBg] = legend;
    ImPlot3D::GetStyle().Colors[ImPlot3DCol_LegendBg] = legend;
    // Diagrams: edge labels and hollow markers sit on the background.
    ImUml::GetStyle().Colors[ImUmlCol_EdgeLabelBg] = legend;
    ImUml::GetStyle().Colors[ImUmlCol_Bg] = bg;
  }
  ImPlot::GetStyle().Colormap = ImPlotColormap_Deep; // the defaults, when switching themes
  ImPlot3D::GetStyle().Colormap = ImPlot3DColormap_Deep;
  if (!o.palette.empty()) {
    // One colormap per palette, added once per context (ImPlot cannot
    // remove one), named after its colours.
    std::string name = "nymph";
    std::vector<ImVec4> colors;
    for (const std::string& c : o.palette) {
      name += c;
      colors.push_back(hex_color(c));
    }
    const int count = static_cast<int>(colors.size());
    ImPlotColormap map = ImPlot::GetColormapIndex(name.c_str());
    if (map < 0) {
      map = ImPlot::AddColormap(name.c_str(), colors.data(), count);
    }
    ImPlot::GetStyle().Colormap = map;
    ImPlot3DColormap map3d = ImPlot3D::GetColormapIndex(name.c_str());
    if (map3d < 0) {
      map3d = ImPlot3D::AddColormap(name.c_str(), colors.data(), count);
    }
    ImPlot3D::GetStyle().Colormap = map3d;
  }
}

void load_fonts(draw_context& ctx) {
  ImFontAtlas& atlas = *ImGui::GetIO().Fonts;
  if (ctx.font(kDefaultFontName) == nullptr) {
    ctx.add_font(std::string(kDefaultFontName), atlas.AddFontDefaultVector());
  }
  for (const font_entry& f : font_catalog()) {
    const std::span<const uint8_t> bytes = font_data(f.name);
    if (bytes.empty() || ctx.font(f.name) != nullptr) {
      continue;
    }
    ImFontConfig cfg;
    cfg.FontDataOwnedByAtlas = false; // embedded or registered: never freed or written
    std::snprintf(cfg.Name, sizeof(cfg.Name), "%s", f.name);
    // ImGui takes a non-const pointer but only reads the font data.
    void* data = const_cast<uint8_t*>(bytes.data());
    ImFont* font = atlas.AddFontFromMemoryTTF(data, static_cast<int>(bytes.size()), kFigureFontSize, &cfg);
    if (font == nullptr) {
      throw std::runtime_error(std::string("font ") + f.name + " could not be loaded");
    }
    ctx.add_font(f.name, font);
  }
}

void run_figure_frame(const image_options& options, draw_context& ctx, const std::function<void(draw_context&)>& draw) {
  constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollWithMouse |
      ImGuiWindowFlags_NoBringToFrontOnFocus;
  ImGui::NewFrame();
  ctx.ui().begin_frame();
  ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
  ImGui::Begin(kFigureWindowName, nullptr, kFlags);
  {
    const text_style_scope style(image_text_style(options), ctx);
    draw(ctx);
  }
  ImGui::End();
  ImGui::Render();
}

text_style image_text_style(const image_options& o) {
  return {o.font.empty() ? std::string(kDefaultFontName) : o.font, o.font_size, o.text_color};
}

} // namespace bdg::nymph
