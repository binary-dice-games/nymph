// MIT License © 2026 Binary Dice Games
/// @file frame_setup.hpp
/// @brief What every nymph ImGui context needs before its first frame:
///        private contexts, the theme, nymph's backend settings and the
///        fonts. Shared by the PNG frame driver (figure_renderer) and the
///        live view (spec/04_web_viewer/DESIGN.md section 4).
#pragma once

#include <imgui.h>
#include <implot.h>
#include <implot3d.h>
#include <imuml.h>
#include <functional>
#include "render/draw_context.hpp"
#include "render/image_options.hpp"
#include "render/text_style.hpp"

namespace bdg::nymph {

/// @brief The ImGui, ImPlot, ImPlot3D and ImUml style functions of one
///        `image.theme`.
struct theme {
  const char* name;
  void (*imgui)(ImGuiStyle*);
  void (*implot)(ImPlotStyle*);
  void (*implot3d)(ImPlot3DStyle*);
  void (*imuml)(ImUmlStyle*);
};

/// @brief The theme called `name`, or nullptr.
const theme* find_theme(const std::string& name);

/// @brief Checks the options render_frames() accepts.
/// @throws std::invalid_argument for a size, scale, padding, theme, font or
///         font size out of range.
void check_options(const image_options& o);

/// @brief Owns a set of ImGui, ImPlot, ImPlot3D and ImUml contexts,
///        without changing which contexts are current: the ones current
///        before construction or destruction are current afterwards
///        (unless they are the ones destroyed).
class imgui_contexts {
 public:
  imgui_contexts();
  ~imgui_contexts();
  imgui_contexts(const imgui_contexts&) = delete;
  imgui_contexts& operator=(const imgui_contexts&) = delete;

  ImGuiContext* imgui() const {
    return imgui_;
  }
  ImPlotContext* implot() const {
    return implot_;
  }
  ImPlot3DContext* implot3d() const {
    return implot3d_;
  }
  ImUmlContext* imuml() const {
    return imuml_;
  }

 private:
  ImGuiContext* imgui_ = nullptr;
  ImPlotContext* implot_ = nullptr;
  ImPlot3DContext* implot3d_ = nullptr;
  ImUmlContext* imuml_ = nullptr;
};

/// @brief Makes `contexts` current for its lifetime, then makes the
///        previously current contexts current again (also on exception).
class context_switch {
 public:
  explicit context_switch(const imgui_contexts& contexts);
  ~context_switch();
  context_switch(const context_switch&) = delete;
  context_switch& operator=(const context_switch&) = delete;

 private:
  ImGuiContext* prev_imgui_;
  ImPlotContext* prev_implot_;
  ImPlot3DContext* prev_implot3d_;
  ImUmlContext* prev_imuml_;
};

/// @brief Private contexts for one render: created and made current by the
///        constructor, destroyed by the destructor, which makes the
///        contexts current before construction current again.
class context_scope {
 public:
  context_scope() : switch_(contexts_) {}
  context_scope(const context_scope&) = delete;
  context_scope& operator=(const context_scope&) = delete;

 private:
  imgui_contexts contexts_; // declared first: outlives the switch
  context_switch switch_;
};

/// @brief Configures the current (fresh) contexts as nymph's backend with
///        theme `t`: no ini or log file, `renderer_name` as
///        `BackendRendererName`, `backend_flags` added to the backend
///        flags, the theme's styles, and nymph's style overrides.
/// @param backend_flags The PNG path passes `RendererHasTextures |
///        RendererHasVtxOffset`; a GPU backend that sets its own flags
///        passes 0.
void configure(const theme& t, const char* renderer_name, ImGuiBackendFlags backend_flags);

/// @brief Adds ImGui's built-in font (the default) and every catalog font
///        with bytes (embedded or registered) to the current context's
///        atlas and registers them in `ctx` by name. Fonts already in
///        `ctx` are skipped, so calling it again adds only fonts
///        registered since; call it outside a frame.
/// @throws std::runtime_error if a font cannot be loaded.
void load_fonts(draw_context& ctx);

/// @brief The `image` text style: the outermost one, which elements
///        inherit.
text_style image_text_style(const image_options& o);

/// @brief Runs one frame in the current (configured) contexts, inputs
///        already set: NewFrame(), a borderless window covering the
///        display with the `image` text style, `draw`, Render(). If `draw`
///        throws, the frame is left open and the contexts must be
///        destroyed.
void run_figure_frame(const image_options& options, draw_context& ctx, const std::function<void(draw_context&)>& draw);

} // namespace bdg::nymph
