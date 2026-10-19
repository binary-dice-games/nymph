// MIT License © 2026 Binary Dice Games
/// @file viewer_window.cpp
/// @brief show_window(): a live_view in a native_window, drawn with Dear
///        ImGui's OpenGL3 backend (spec/04_web_viewer/DESIGN.md section 10),
///        with the editor panel docked to its right edge when asked for
///        (spec/08_view_editor/DESIGN.md). Platform-neutral; the window
///        itself is window_win32.cpp or window_x11.cpp.
#include "window/viewer_window.hpp"
#include <imgui_impl_opengl3.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include "cli/cli_support.hpp"
#include "core/error.hpp"
#include "nymph.hpp"
#include "view/live_view.hpp"
#include "window/editor_panel.hpp"
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
    window_->make_current(); // the backends' shutdown (panel_ and view_'s destruction) needs the context
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
    saved_source_ = request_.source;
    open_at_layout_size();
    set_title("");
    w.show();
    if (request_.args.edit) {
      set_panel(true);
    }
    while (true) {
      const bool panel_wants = panel_ != nullptr && panel_->wants_frame();
      if (view_.wants_frame() || panel_wants) {
        if (!w.pump(false)) {
          return;
        }
        present(true);
        if (panel_ != nullptr && panel_->panel().pending()) {
          // A typed text waits for its pause: poll instead of sleeping in the event queue.
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
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

  bool on_close_request() override {
    // Unsaved changes: ask first (the dialog closes the window if the user says so).
    if (!ready_) {
      return true;
    }
    if (panel_ == nullptr && std::string(view_.source()) != saved_source_) {
      set_panel(true); // changes made earlier with the panel hidden: show it to ask
    }
    return panel_ == nullptr || !panel_->ask_close();
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
    if (panel_ == nullptr) {
      view_.input(e);
      return;
    }
    // The strip takes the pointer while it is over it, and a drag that began in it.
    const bool over_panel = e.x >= panel_->strip_x() || panel_->popup_open();
    if (type == pointer_event::kind::down) {
      panel_drag_ = over_panel;
    }
    const bool to_panel = panel_drag_ || over_panel;
    if (to_panel) {
      if (!pointer_in_panel_) { // the figure sees the pointer leave
        view_.input({pointer_event::kind::leave, 0.0f, 0.0f, 0});
      }
      pointer_in_panel_ = true;
      panel_->pointer(type, e.x, e.y, button, wheel_x, wheel_y);
    } else {
      if (pointer_in_panel_) {
        panel_->pointer(pointer_event::kind::leave, 0.0f, 0.0f, 0, 0.0f, 0.0f);
      }
      pointer_in_panel_ = false;
      view_.input(e);
    }
    if (type == pointer_event::kind::up) {
      panel_drag_ = false;
    }
  }

  void on_key(window_key key, key_mods mods, bool down, bool repeat) override {
    if (!ready_) {
      return;
    }
    if (panel_ != nullptr && (panel_->wants_keyboard() || panel_->modal_open())) {
      panel_->key(key, mods, down, repeat);
      return;
    }
    if (!down || repeat) {
      return;
    }
    if (panel_ != nullptr && mods.ctrl) {
      if (key == window_key::z && !mods.shift) {
        apply(panel_events(panel_->model().undo()));
      } else if (key == window_key::y || (key == window_key::z && mods.shift)) {
        apply(panel_events(panel_->model().redo()));
      }
      return;
    }
    switch (key) {
      case window_key::escape:
      case window_key::q:
        if (on_close_request()) {
          window_->close();
        }
        break;
      case window_key::r:
        reset_view();
        break;
      case window_key::s:
        save("png");
        break;
      case window_key::y:
        save("yaml");
        break;
      case window_key::e:
        set_panel(panel_ == nullptr);
        break;
      default:
        break;
    }
  }

  void on_text(char32_t codepoint) override {
    if (ready_ && panel_ != nullptr && panel_->wants_keyboard()) {
      panel_->text(codepoint);
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

  /// Width of the editor strip in device pixels; 0 without the panel.
  int strip_pixels() const {
    return panel_ != nullptr ? static_cast<int>(std::lround(editor_panel::kWidth * scale_)) : 0;
  }

  /// Sizes the window to the layout (CSS pixels times the display scale),
  /// within the screen's usable area, plus the editor strip when it is shown.
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

  /// Gives the view its box: the client area in CSS pixels (less the editor
  /// strip), the display scale as its pixel ratio.
  void sync_size() {
    scale_ = window_->scale();
    window_->client_size(client_w_, client_h_);
    if (client_w_ <= 0 || client_h_ <= 0) {
      return; // minimised
    }
    const float css_w = static_cast<float>(client_w_) / scale_;
    const float css_h = static_cast<float>(client_h_) / scale_;
    const float figure_w = panel_ != nullptr ? std::max(css_w - editor_panel::kWidth, 1.0f) : css_w;
    view_.resize(view_size{figure_w, css_h, scale_});
    if (panel_ != nullptr) {
      panel_->resize(css_w, css_h, scale_);
    }
  }

  /// Shows or hides the editor panel; the window grows (or shrinks again)
  /// by the strip when the screen has room.
  void set_panel(bool on) {
    if ((panel_ != nullptr) == on) {
      return;
    }
    window_->make_current();
    if (on) {
      try {
        panel_ = std::make_unique<editor_panel>(std::string(view_.source()), name_, window_->glsl_version());
      } catch (const std::exception& e) {
        set_title(std::string("cannot edit: ") + e.what());
        return;
      }
      panel_->model().set_original(saved_source_);
      view_.set_edit_mode(true);
      int avail_w = 0, avail_h = 0;
      window_->available_client_size(avail_w, avail_h);
      const int strip = strip_pixels();
      grown_ = client_w_ + strip <= avail_w;
      if (grown_) {
        resize_client(client_w_ + strip, client_h_);
      } else {
        sync_size();
      }
    } else {
      const int strip = strip_pixels();
      panel_.reset();
      view_.set_edit_mode(false);
      view_.set_selection(std::nullopt);
      if (grown_) {
        resize_client(std::max(client_w_ - strip, 1), client_h_);
      } else {
        sync_size();
      }
      grown_ = false;
      pointer_in_panel_ = false;
      panel_drag_ = false;
    }
    last_ = nullptr;
    present(true);
  }

  /// The events of an undo or redo.
  static inspector_events panel_events(const std::optional<std::string>& source) {
    inspector_events events;
    events.source = source;
    return events;
  }

  /// Applies what the panel did: shows a changed source, selects, saves.
  void apply(const inspector_events& events) {
    if (panel_ == nullptr) {
      return;
    }
    if (events.source) {
      try {
        view_.set_source(*events.source);
      } catch (const std::exception& e) {
        status_ = std::string("cannot show: ") + e.what();
      }
    }
    if (events.select) {
      panel_->model().select(*events.select);
    }
    if (events.reset_view) {
      reset_view();
    }
    // The model decides what is selected (an undo may remove the element).
    view_.set_selection(panel_->model().selection());
    if (events.save) {
      panel_->ask_save();
    }
    panel_->refresh();
  }

  /// Does what a confirmation dialog decided.
  void act_on(editor_panel::decision decision) {
    using decision_t = editor_panel::decision;
    if (decision == decision_t::save || decision == decision_t::save_and_close) {
      const bool saved = save_input();
      if (saved && decision == decision_t::save_and_close) {
        window_->close();
      }
    } else if (decision == decision_t::discard_and_close) {
      window_->close();
    }
  }

  /// Overwrites the input with the edited chart (with the view on screen
  /// saved in its `view` key): a PNG input becomes a PNG again, a YAML input
  /// stays YAML.
  bool save_input() {
    const std::string& path = request_.args.input;
    try {
      const std::string source = view_.source_with_view();
      std::ostringstream unused;
      if (is_png_file(path)) {
        const std::vector<uint8_t> png = render_png(source);
        write_output(path, as_text(png), unused);
      } else {
        write_output(path, source, unused);
      }
      panel_->model().mark_saved();
      saved_source_ = std::string(view_.source());
      status_ = "saved " + name_;
      set_title(status_);
      return true;
    } catch (const std::exception& e) {
      status_ = std::string("cannot save: ") + e.what();
      set_title(status_);
      return false;
    }
  }

  /// Whether the file starts with the PNG signature.
  static bool is_png_file(const std::string& path) {
    std::ifstream in(std::filesystem::path(path), std::ios::binary);
    char head[8] = {};
    in.read(head, sizeof(head));
    static constexpr char kSignature[8] = {char(0x89), 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
    return in.gcount() == 8 && std::memcmp(head, kSignature, 8) == 0;
  }

  void reset_view() {
    view_.reset_view();
    last_ = nullptr; // its draw data went with the old contexts
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
    if (panel_ != nullptr && fresh) {
      if (const std::optional<edit_target> clicked = view_.take_edit_click()) {
        panel_->model().select(edit_target_id{clicked->element, clicked->field});
        view_.set_selection(panel_->model().selection());
        panel_->refresh();
      }
      panel_->set_theme(view_.theme());
      if (panel_->wants_frame()) {
        const inspector_events events = panel_->frame(now_seconds(), view_.element_list(), status_);
        apply(events);
        act_on(panel_->take_decision());
        if (panel_ == nullptr) {
          return;
        }
      }
    }
    if (last_ == nullptr && panel_ == nullptr) {
      return;
    }
    viewport_(0, 0, client_w_, client_h_);
    const ImVec4 bg = ImGui::ColorConvertU32ToFloat4(view_.clear_color());
    clear_color_(bg.x, bg.y, bg.z, 1.0f);
    clear_(kColorBufferBit);
    if (last_ != nullptr) {
      view_.with_contexts([&] { ImGui_ImplOpenGL3_RenderDrawData(const_cast<ImDrawData*>(last_)); });
    }
    if (panel_ != nullptr) {
      panel_->render();
    }
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
    const int want_w = static_cast<int>(std::lround(g.layout_width * scale_)) + strip_pixels();
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
      status_ = "saved " + std::filesystem::path(path).filename().string();
      set_title(status_);
    } catch (const std::exception& e) {
      status_ = std::string("cannot save: ") + e.what();
      set_title(status_);
    }
  }

  void set_title(const std::string& status) {
    window_->set_title("nymph - " + name_ + (status.empty() ? "" : " - " + status));
  }

  const view_request& request_;
  std::string name_;
  bool ready_ = false; ///< Events sent while the window is created find no view yet.
  std::unique_ptr<native_window> window_;
  live_view view_; ///< After window_: destroyed after panel_, before the window.
  std::unique_ptr<editor_panel> panel_; ///< After view_: destroyed first, while the context exists.
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
  bool grown_ = false; ///< The window was made wider for the panel.
  bool panel_drag_ = false; ///< A button went down over the panel and is still held.
  bool pointer_in_panel_ = false;
  std::string status_; ///< What was saved or refused, for the panel.
  std::string saved_source_; ///< The source the input file holds, to tell unsaved changes.
};

} // namespace

void show_window(const view_request& request) {
  viewer v(request);
  v.run();
}

} // namespace bdg::nymph
