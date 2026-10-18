// MIT License © 2026 Binary Dice Games
/// @file webgl_presenter.cpp
/// @brief The shared WebGL2 context and ImGui's OpenGL3 backend per view.
#include "webgl_presenter.hpp"
#include <GLES3/gl3.h>
#include <emscripten/html5.h>
#include <imgui_impl_opengl3.h>
#include <algorithm>
#include <cmath>
#include <string>

namespace bdg::nymph::web {

namespace {

struct shared_gl {
  EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context = 0;
  std::string target;
  int width = 0; ///< Canvas size; only grows.
  int height = 0;
};

shared_gl& gl() {
  static shared_gl g;
  return g;
}

void make_current() {
  emscripten_webgl_make_context_current(gl().context);
}

} // namespace

bool gl_init(const char* target) {
  shared_gl& g = gl();
  if (g.context > 0) {
    return true;
  }
  EmscriptenWebGLContextAttributes attrs;
  emscripten_webgl_init_context_attributes(&attrs);
  attrs.majorVersion = 2;
  attrs.minorVersion = 0;
  attrs.alpha = false; // charts are opaque
  attrs.depth = false;
  attrs.stencil = false;
  attrs.antialias = false; // ImGui anti-aliases with its own geometry
  attrs.preserveDrawingBuffer = false; // copied out within the same task
  attrs.powerPreference = EM_WEBGL_POWER_PREFERENCE_LOW_POWER;
  g.context = emscripten_webgl_create_context(target, &attrs);
  if (g.context <= 0) {
    g.context = 0;
    return false;
  }
  g.target = target;
  return true;
}

bool gl_ready() {
  return gl().context > 0;
}

void gl_attach(live_view& view) {
  view.set_context_hooks(
      [] {
        make_current();
        ImGui_ImplOpenGL3_Init("#version 300 es");
        // Normally made by ImGui_ImplOpenGL3_NewFrame(), which nymph's own
        // frame loop does not call.
        ImGui_ImplOpenGL3_CreateDeviceObjects();
      },
      [] {
        make_current();
        ImGui_ImplOpenGL3_Shutdown();
      });
}

void gl_draw(live_view& view, ImDrawData& draw_data, int* width, int* height) {
  shared_gl& g = gl();
  make_current();
  const int w = static_cast<int>(std::lround(draw_data.DisplaySize.x * draw_data.FramebufferScale.x));
  const int h = static_cast<int>(std::lround(draw_data.DisplaySize.y * draw_data.FramebufferScale.y));
  if (w > g.width || h > g.height) {
    g.width = std::max(g.width, w);
    g.height = std::max(g.height, h);
    emscripten_set_canvas_element_size(g.target.c_str(), g.width, g.height);
  }
  glDisable(GL_SCISSOR_TEST);
  glViewport(0, 0, g.width, g.height);
  const ImVec4 bg = ImGui::ColorConvertU32ToFloat4(view.clear_color());
  glClearColor(bg.x, bg.y, bg.z, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  // The backend sets its viewport to the bottom-left w x h of the canvas.
  view.with_contexts([&] { ImGui_ImplOpenGL3_RenderDrawData(&draw_data); });
  *width = w;
  *height = h;
}

} // namespace bdg::nymph::web
