// MIT License © 2026 Binary Dice Games
/// @file viewer_window.cpp
/// @brief show_window(): a live_view in a native_window, drawn with Dear
///        ImGui's OpenGL3 backend (spec/04_web_viewer/DESIGN.md section 10).
///        Platform-neutral; the window itself is window_win32.cpp or
///        window_x11.cpp.
#include "window/viewer_window.hpp"
#include <imgui_impl_opengl3.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include "cli/cli_support.hpp"
#include "nymph.hpp"
#include "view/live_view.hpp"
#include "window/window_host.hpp"

namespace bdg::nymph {

namespace {

constexpr unsigned kColorBufferBit = 0x00004000;

/// OpenGL's calling convention (APIENTRY): __stdcall on Windows, which
/// matters on 32-bit builds; the default elsewhere.
#if defined(_WIN32)
#define NYMPH_GL_CALL __stdcall
#else
#define NYMPH_GL_CALL
#endif

/// Seconds on a monotonic clock.
double now_seconds() {
  using clock = std::chrono::steady_clock;
  return std::chrono::duration<double>(clock::now().time_since_epoch()).count();
}

/// A live_view in a native_window: events in, frames out.
class viewer : public window_events {
 public:
  explicit viewer(const view_request& request)
      : request_(request),
        name_(std::filesystem::path(request.args.input).filename().string()),
        window_(create_native_window(*this)),
        view_(view_options{request.args.fit, /*cpu_textures=*/false, /*tooltips=*/true}) {
    ready_ = true;
  }

  ~viewer() override {
    ready_ = false;
    window_->make_current(); // the backend's shutdown (view_'s destruction) needs its context
  }

  /// Opens the window at the layout's size and runs until it is closed.
  void run() {
    load_gl();
    native_window& w = *window_;
    view_.set_context_hooks(
        [&w] {
          if (!ImGui_ImplOpenGL3_Init(w.glsl_version())) {
            throw window_error("cannot open a window: the OpenGL driver cannot run the viewer");
          }
          // Normally made by ImGui_ImplOpenGL3_NewFrame(), which nymph's own
          // frame loop does not call.
          ImGui_ImplOpenGL3_CreateDeviceObjects();
        },
        [] { ImGui_ImplOpenGL3_Shutdown(); });
    view_.set_source(request_.source);
    view_.set_theme(request_.args.theme);
    open_at_layout_size();
    set_title("");
    w.show();
    while (true) {
      if (view_.wants_frame()) {
        if (!w.pump(false)) {
          return;
        }
        present(true);
      } else if (!w.pump(true)) { // idle: wait for input
        return;
      }
    }
  }

  void on_resized() override {
    if (!ready_ || opening_) {
      return;
    }
    sync_size();
    present(true);
  }

  void on_expose() override {
    if (ready_) {
      present(false);
    }
  }

  void on_pointer(pointer_event::kind type, float x, float y, int button, float wheel_x, float wheel_y) override {
    if (!ready_) {
      return;
    }
    pointer_event e;
    e.type = type;
    e.x = x / scale_;
    e.y = y / scale_;
    e.button = button; // left, right, middle are ImGui's 0, 1, 2
    e.wheel_x = wheel_x;
    e.wheel_y = wheel_y;
    view_.input(e);
  }

  void on_key(window_key key) override {
    if (!ready_) {
      return;
    }
    switch (key) {
      case window_key::escape:
      case window_key::q:
        window_->close();
        break;
      case window_key::r:
        view_.reset_view();
        last_ = nullptr; // its draw data went with the old contexts
        break;
      case window_key::s:
        save("png");
        break;
      case window_key::y:
        save("yaml");
        break;
      case window_key::other:
        break;
    }
  }

 private:
  void load_gl() {
    assign(viewport_, window_->gl_proc("glViewport"));
    assign(clear_color_, window_->gl_proc("glClearColor"));
    assign(clear_, window_->gl_proc("glClear"));
    if (!viewport_ || !clear_color_ || !clear_) {
      throw window_error("cannot open a window: the OpenGL driver lacks glViewport / glClear");
    }
  }

  template <typename Fn>
  static void assign(Fn& fn, void* address) {
    static_assert(sizeof(Fn) == sizeof(void*), "function and data pointers differ in size");
    std::memcpy(&fn, &address, sizeof(fn));
  }

  /// Sizes the window to the layout (CSS pixels times the display scale),
  /// within the screen's usable area.
  void open_at_layout_size() {
    scale_ = window_->scale();
    int avail_w = 0, avail_h = 0;
    window_->available_client_size(avail_w, avail_h);
    const window_size client = initial_window_size(
        static_cast<int>(std::lround(static_cast<float>(request_.width) * scale_)),
        static_cast<int>(std::lround(static_cast<float>(request_.height) * scale_)),
        avail_w,
        avail_h);
    resize_client(client.width, client.height);
  }

  /// Asks for a client area of `w` x `h` pixels.
  void resize_client(int w, int h) {
    opening_ = true; // the resize event this causes is handled here
    window_->resize_client(w, h);
    opening_ = false;
    sync_size();
  }

  /// Gives the view its box: the client area in CSS pixels, the display
  /// scale as its pixel ratio.
  void sync_size() {
    scale_ = window_->scale();
    window_->client_size(client_w_, client_h_);
    if (client_w_ <= 0 || client_h_ <= 0) {
      return; // minimised
    }
    view_.resize(view_size{static_cast<float>(client_w_) / scale_, static_cast<float>(client_h_) / scale_, scale_});
  }

  /// Draws a new frame if one is due (`fresh`), or the last one again (an
  /// exposed window), and shows it.
  void present(bool fresh) {
    if (client_w_ <= 0 || client_h_ <= 0) {
      return;
    }
    if (fresh && view_.wants_frame()) {
      if (const ImDrawData* data = view_.frame(now_seconds())) {
        last_ = data;
      }
      enlarge_if_too_small();
    }
    if (last_ == nullptr) {
      return;
    }
    viewport_(0, 0, client_w_, client_h_);
    const ImVec4 bg = ImGui::ColorConvertU32ToFloat4(view_.clear_color());
    clear_color_(bg.x, bg.y, bg.z, 1.0f);
    clear_(kColorBufferBit);
    view_.with_contexts([&] { ImGui_ImplOpenGL3_RenderDrawData(const_cast<ImDrawData*>(last_)); });
    window_->swap();
  }

  /// A window too small for the layout is drawn shrunk (the view falls back
  /// to the source size); ask for a window of the layout's size once.
  void enlarge_if_too_small() {
    const view_geometry g = view_.geometry();
    if (!g.fit_fallback) {
      asked_w_ = asked_h_ = 0;
      return;
    }
    const int want_w = static_cast<int>(std::lround(g.layout_width * scale_));
    const int want_h = static_cast<int>(std::lround(g.layout_height * scale_));
    if (want_w == asked_w_ && want_h == asked_h_) {
      return;
    }
    asked_w_ = want_w;
    asked_h_ = want_h;
    resize_client(want_w, want_h);
  }

  /// Writes the image or the YAML on screen next to the input.
  void save(const char* extension) {
    const std::string path = view_save_path(request_.args.input, extension);
    try {
      const std::string source = view_.source_with_view();
      std::ostringstream unused;
      if (std::string_view(extension) == "png") {
        const std::vector<uint8_t> png = render_png(source);
        write_output(path, as_text(png), unused);
      } else {
        write_output(path, source, unused);
      }
      set_title("saved " + std::filesystem::path(path).filename().string());
    } catch (const std::exception& e) {
      set_title(std::string("cannot save: ") + e.what());
    }
  }

  void set_title(const std::string& status) {
    window_->set_title("nymph - " + name_ + (status.empty() ? "" : " - " + status));
  }

  const view_request& request_;
  std::string name_;
  bool ready_ = false; ///< Events sent while the window is created find no view yet.
  std::unique_ptr<native_window> window_;
  live_view view_; ///< After window_: destroyed first, while the context exists.
  void(NYMPH_GL_CALL* viewport_)(int, int, int, int) = nullptr;
  void(NYMPH_GL_CALL* clear_color_)(float, float, float, float) = nullptr;
  void(NYMPH_GL_CALL* clear_)(unsigned) = nullptr;
  const ImDrawData* last_ = nullptr; ///< The last frame, valid until the next one.
  float scale_ = 1.0f; ///< Device pixels per CSS pixel.
  int client_w_ = 0;
  int client_h_ = 0;
  bool opening_ = false;
  int asked_w_ = 0; ///< The size last asked of the window for a layout that did not fit.
  int asked_h_ = 0;
};

} // namespace

void show_window(const view_request& request) {
  viewer v(request);
  v.run();
}

} // namespace bdg::nymph
