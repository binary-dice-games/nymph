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
#include "edit/workspace_layout.hpp"
#include "window/editor_panel.hpp"
#include "window/window_host.hpp"
#if defined(NYMPH_EDITOR_MCP)
#include "ipc/editor_registry.hpp"
#include "mcp/editor_server.hpp"
#include "png/png_meta.hpp"
#endif

namespace bdg::nymph {

namespace {

constexpr unsigned kColorBufferBit = 0x00004000;
constexpr unsigned kRgba = 0x1908; // GL_RGBA
constexpr unsigned kUnsignedByte = 0x1401; // GL_UNSIGNED_BYTE
constexpr unsigned kPackAlignment = 0x0D05; // GL_PACK_ALIGNMENT

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
#if defined(NYMPH_EDITOR_MCP)
    server_.reset(); // agents' calls fail before the editor goes
    record_.reset();
#endif
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
      run_agent_jobs();
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
    if (divider(type, e.x, button)) {
      return;
    }
    // The panel (strip, tab bar, a data table) takes the pointer everywhere
    // but over the figure, and a drag that began in it. The figure starts
    // under the tab bar.
    const layout_box figure = panel_->figure_box();
    const bool over_panel = !figure.contains(e.x, e.y) || panel_->popup_open();
    e.y -= figure.y;
    if (type == pointer_event::kind::down) {
      panel_drag_ = over_panel;
    }
    const bool to_panel = panel_drag_ || over_panel;
    if (to_panel) {
      if (!pointer_in_panel_) { // the figure sees the pointer leave
        view_.input({pointer_event::kind::leave, 0.0f, 0.0f, 0});
      }
      pointer_in_panel_ = true;
      panel_->pointer(type, e.x, e.y + figure.y, button, wheel_x, wheel_y);
    } else {
      if (pointer_in_panel_) {
        panel_->pointer(pointer_event::kind::leave, 0.0f, 0.0f, 0, 0.0f, 0.0f);
      }
      pointer_in_panel_ = false;
      view_.input(e);
      figure_right_click(type, e.x, e.y, button, figure);
    }
    if (type == pointer_event::kind::up) {
      panel_drag_ = false;
    }
  }

  void on_key(window_key key, key_mods mods, bool down, bool repeat) override {
    if (!ready_) {
      return;
    }
    const key_event figure_key{imgui_key(key), down, mods.ctrl, mods.shift, mods.alt, false};
    // A popup of the panel (a menu, a colour picker) takes the keys too:
    // Escape closes it, not the window.
    const bool panel_keys = panel_ != nullptr && (panel_->wants_keyboard() || panel_->modal_open() || panel_->popup_open());
    switch (route_key(panel_keys, view_.wants_keyboard())) {
      case key_route::panel:
        panel_->key(key, mods, down, repeat);
        return;
      case key_route::figure:
        view_.input_key(figure_key);
        return;
      case key_route::viewer:
        break;
    }
    if (!down) {
      view_.input_key(figure_key); // a key pressed in a field may be released after it
      return;
    }
    if (repeat) {
      return;
    }
    if (panel_ != nullptr) {
      const inspector_events tree = panel_->shortcut(key, mods);
      if (tree.source || tree.select) {
        apply(tree);
        return;
      }
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

  void on_focus(bool focused) override {
#if defined(NYMPH_EDITOR_MCP)
    if (focused && record_ != nullptr) {
      record_->set_focused(ipc::now_epoch()); // agents pick the editor used last
    }
#else
    (void)focused;
#endif
  }

  void on_text(char32_t codepoint) override {
    if (!ready_) {
      return;
    }
    switch (route_key(panel_ != nullptr && panel_->wants_keyboard(), view_.wants_keyboard())) {
      case key_route::panel:
        panel_->text(codepoint);
        break;
      case key_route::figure:
        view_.input_text(std::u32string_view(&codepoint, 1));
        break;
      case key_route::viewer:
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
    assign(read_pixels_, window_->gl_proc("glReadPixels")); // the figure for agents; optional
    assign(pixel_store_, window_->gl_proc("glPixelStorei"));
  }

  template <typename Fn>
  static void assign(Fn& fn, void* address) {
    static_assert(sizeof(Fn) == sizeof(void*), "function and data pointers differ in size");
    std::memcpy(&fn, &address, sizeof(fn));
  }

  /// Width of the editor strip in device pixels; 0 without the panel.
  int strip_pixels() const {
    return panel_ != nullptr ? static_cast<int>(std::lround(panel_->strip_width() * scale_)) : 0;
  }

  /// Height of the tab bar over the figure in device pixels; 0 without the panel.
  int tab_pixels() const {
    return panel_ != nullptr ? static_cast<int>(std::lround(editor_panel::kTabBarHeight * scale_)) : 0;
  }

  /// The divider between the figure and the strip: its cursor, dragging it,
  /// and a double-click putting it back. x in CSS pixels.
  /// @return true when the event was the divider's (nothing else sees it).
  bool divider(pointer_event::kind type, float x, int button) {
    const float css_w = static_cast<float>(client_w_) / scale_;
    if (divider_drag_) {
      if (type == pointer_event::kind::move) {
        panel_->set_strip_width(css_w - x);
        strip_wanted_ = panel_->wanted_strip_width();
        sync_size();
      } else if (type == pointer_event::kind::up) {
        divider_drag_ = false;
      }
      return true;
    }
    const bool over = !panel_drag_ && !panel_->popup_open() && !panel_->modal_open() && divider_hit(x, panel_->strip_x());
    set_cursor(over ? cursor_shape::resize_horizontal : cursor_shape::arrow);
    if (!over || type != pointer_event::kind::down || button != 0) {
      return false;
    }
    const double now = now_seconds();
    if (now - divider_clicked_at_ < kDoubleClick) {
      panel_->set_strip_width(kDefaultStripWidth); // double-click: back to the default width
      strip_wanted_ = kDefaultStripWidth;
      divider_clicked_at_ = -1.0;
      sync_size();
    } else {
      divider_clicked_at_ = now;
      divider_drag_ = true;
    }
    // The figure and the strip see the pointer leave.
    view_.input({pointer_event::kind::leave, 0.0f, 0.0f, 0});
    panel_->pointer(pointer_event::kind::leave, 0.0f, 0.0f, 0, 0.0f, 0.0f);
    pointer_in_panel_ = false;
    return true;
  }

  /// The line under the panel's toolbar: what was saved or refused, and
  /// whether an agent is attached.
  std::string status_line() const {
#if defined(NYMPH_EDITOR_MCP)
    if (server_ != nullptr && server_->connections() > 0) {
      return status_.empty() ? std::string("Agent connected") : status_ + "  -  agent connected";
    }
#endif
    return status_;
  }

  // ── Agents (spec/14_editor_workspace/DESIGN.md sections 6 and 7) ─────────

  /// Starts the MCP server agents reach through `nymph mcp`, once, unless
  /// --no-mcp. A failure is a status line; the editor works without it.
  void start_agent_server() {
#if defined(NYMPH_EDITOR_MCP)
    if (request_.args.no_mcp || server_ != nullptr) {
      return;
    }
    try {
      const std::filesystem::path dir = ipc::editors_dir();
      const int64_t pid = ipc::current_pid();
      const std::filesystem::path socket = ipc::socket_path(dir, pid);
      native_window* w = window_.get();
      server_ = std::make_unique<mcp::editor_server>(socket, [w] { w->wake(); });
      std::error_code ec;
      const std::filesystem::path file = std::filesystem::absolute(request_.args.input, ec);
      record_ = std::make_unique<ipc::editor_record>(
          dir, ipc::editor_entry{pid, socket.string(), (ec ? std::filesystem::path(request_.args.input) : file).string(),
                                 std::string(version()), ipc::now_epoch(), {}});
      host_ = std::make_unique<agent_host>(*this);
    } catch (const std::exception& e) {
      server_.reset();
      record_.reset();
      status_ = std::string("agents cannot connect: ") + e.what();
    }
#endif
  }

  /// Runs what agents asked for since the last time (UI thread).
  void run_agent_jobs() {
#if defined(NYMPH_EDITOR_MCP)
    if (server_ == nullptr) {
      return;
    }
    const int connections = server_->connections();
    if (connections != last_connections_ && panel_ != nullptr) {
      last_connections_ = connections;
      panel_->refresh(); // "Agent connected" comes or goes
    }
    if (panel_ != nullptr && panel_->panel().pending()) {
      apply(panel_->panel().flush(panel_->model())); // what the user typed goes first
    }
    server_->run_pending(*host_);
#endif
  }

#if defined(NYMPH_EDITOR_MCP)
  /// The editor as the agents' tools see it.
  class agent_host : public mcp::editor_host {
   public:
    explicit agent_host(viewer& v) : v_(v) {}
    editor_model* model() override {
      return v_.panel_ != nullptr ? &v_.panel_->model() : nullptr;
    }
    std::string file() const override {
      return v_.record_ != nullptr ? v_.record_->entry().file : v_.request_.args.input;
    }
    std::optional<std::string> data_tab() const override {
      return v_.panel_ != nullptr ? v_.panel_->data_tab() : std::nullopt;
    }
    void show_tab(std::optional<std::string> table) override {
      if (v_.panel_ != nullptr) {
        v_.panel_->show_tab(std::move(table));
      }
    }
    std::vector<edit_element> tree() const override {
      return v_.view_.element_list();
    }
    std::string view_source() const override {
      try {
        return v_.view_.source_with_view();
      } catch (const std::exception&) {
        return std::string(v_.view_.source());
      }
    }
    std::string panel_error() const override {
      return v_.panel_ != nullptr ? v_.panel_->panel().error() : std::string();
    }
    void changed(const commit_result& r, const std::string& tool) override {
      inspector_events events;
      events.source = r.source;
      events.paths = r.paths;
      v_.apply(events);
      v_.status_ = "changed by the agent: " + tool;
      if (v_.panel_ != nullptr) {
        v_.panel_->panel().set_error("");
        v_.panel_->refresh();
      }
    }
    void select(const edit_target_id& target) override {
      if (v_.panel_ == nullptr) {
        return;
      }
      v_.panel_->model().select(target);
      v_.view_.set_selection(v_.panel_->model().selection());
      v_.panel_->refresh();
    }
    std::vector<uint8_t> view_image(int max_width, int& width, int& height) override {
      return v_.capture_figure(max_width, width, height);
    }

   private:
    viewer& v_;
  };

  /// The figure as on screen, read back from the frame buffer; drawn from the
  /// source when a data table hides it.
  std::vector<uint8_t> capture_figure(int max_width, int& width, int& height) {
    const bool shown = panel_ == nullptr || !panel_->figure_box().empty();
    if (!shown || read_pixels_ == nullptr || client_w_ <= 0 || client_h_ <= 0) {
      const std::vector<uint8_t> png = render_png(view_.source_with_view());
      width = png.size() > 24 ? (png[16] << 24 | png[17] << 16 | png[18] << 8 | png[19]) : 0;
      height = png.size() > 24 ? (png[20] << 24 | png[21] << 16 | png[22] << 8 | png[23]) : 0;
      return png;
    }
    window_->make_current();
    if (view_.wants_frame()) {
      if (const ImDrawData* data = view_.frame(now_seconds())) {
        last_ = data;
      }
    }
    if (last_ == nullptr) {
      return {};
    }
    // The figure alone, in the bottom-left of the back buffer (as present()
    // draws it), read before anything else is drawn over it.
    const layout_box box = panel_ != nullptr ? panel_->figure_box() : layout_box{0, 0, client_w_ / scale_, client_h_ / scale_};
    const int w = std::max(1, static_cast<int>(std::lround(box.width * scale_)));
    const int h = std::max(1, static_cast<int>(std::lround(box.height * scale_)));
    viewport_(0, 0, client_w_, client_h_);
    const ImVec4 bg = ImGui::ColorConvertU32ToFloat4(view_.clear_color());
    clear_color_(bg.x, bg.y, bg.z, 1.0f);
    clear_(kColorBufferBit);
    view_.with_contexts([&] { ImGui_ImplOpenGL3_RenderDrawData(const_cast<ImDrawData*>(last_)); });
    image img;
    img.width = w;
    img.height = h;
    img.rgba.resize(static_cast<size_t>(w) * h * 4);
    if (pixel_store_ != nullptr) {
      pixel_store_(kPackAlignment, 1);
    }
    std::vector<uint8_t> flipped(img.rgba.size());
    read_pixels_(0, 0, w, h, kRgba, kUnsignedByte, flipped.data());
    for (int y = 0; y < h; ++y) { // OpenGL's rows go up
      std::memcpy(img.at(0, y), flipped.data() + static_cast<size_t>(h - 1 - y) * w * 4, static_cast<size_t>(w) * 4);
    }
    for (size_t i = 3; i < img.rgba.size(); i += 4) {
      img.rgba[i] = 255;
    }
    present(false); // the window as it was
    // Smaller when asked: whole-pixel steps, each the average of its block.
    const int step = max_width > 0 && w > max_width ? (w + max_width - 1) / max_width : 1;
    if (step > 1) {
      image small;
      small.width = w / step;
      small.height = h / step;
      small.rgba.resize(static_cast<size_t>(small.width) * small.height * 4);
      for (int y = 0; y < small.height; ++y) {
        for (int x = 0; x < small.width; ++x) {
          for (int c = 0; c < 4; ++c) {
            int sum = 0;
            for (int dy = 0; dy < step; ++dy) {
              for (int dx = 0; dx < step; ++dx) {
                sum += img.at(x * step + dx, y * step + dy)[c];
              }
            }
            small.at(x, y)[c] = static_cast<uint8_t>(sum / (step * step));
          }
        }
      }
      img = std::move(small);
    }
    width = img.width;
    height = img.height;
    return encode_png(img, "", "");
  }
#endif

  /// A right-click (press and release in place) on an element of the figure
  /// selects it and opens its menu, as on its row in the tree. x, y in the
  /// figure's CSS pixels.
  void figure_right_click(pointer_event::kind type, float x, float y, int button, const layout_box& figure) {
    if (button != 1) {
      return;
    }
    if (type == pointer_event::kind::down) {
      right_down_ = std::make_pair(x, y);
      return;
    }
    if (type != pointer_event::kind::up || !right_down_) {
      return;
    }
    const auto [x0, y0] = *right_down_;
    right_down_.reset();
    if (std::abs(x - x0) > 4.0f || std::abs(y - y0) > 4.0f) {
      return; // a drag, not a click
    }
    const std::vector<edit_target> targets = view_.targets_at(x, y);
    if (targets.empty() || targets.front().element == "image") {
      return; // the image options have no menu
    }
    const std::string path = targets.front().element;
    panel_->model().select(edit_target_id{path, ""});
    view_.set_selection(panel_->model().selection());
    panel_->open_element_menu(path, x + figure.x, y + figure.y);
  }

  void set_cursor(cursor_shape shape) {
    if (shape != cursor_) {
      cursor_ = shape;
      window_->set_cursor(shape);
    }
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
    if (panel_ != nullptr) {
      panel_->resize(css_w, css_h, scale_);
      const layout_box left = panel_->layout().left;
      view_.resize(view_size{std::max(left.width, 1.0f), std::max(left.height, 1.0f), scale_});
    } else {
      view_.resize(view_size{css_w, css_h, scale_});
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
      panel_->set_strip_width(strip_wanted_);
      image_w_ = panel_->model().image().width;
      image_h_ = panel_->model().image().height;
      view_.set_edit_mode(true);
      int avail_w = 0, avail_h = 0;
      window_->available_client_size(avail_w, avail_h);
      const int strip = strip_pixels();
      const int tabs = tab_pixels();
      grown_ = client_w_ + strip <= avail_w;
      grown_tabs_ = client_h_ + tabs <= avail_h;
      if (grown_ || grown_tabs_) {
        resize_client(client_w_ + (grown_ ? strip : 0), client_h_ + (grown_tabs_ ? tabs : 0));
      } else {
        sync_size();
      }
      panel_->set_folder(std::filesystem::path(request_.args.input).parent_path());
      start_agent_server();
    } else {
      const int strip = strip_pixels();
      const int tabs = tab_pixels();
      panel_.reset();
      view_.set_edit_mode(false);
      view_.set_selection(std::nullopt);
      if (grown_ || grown_tabs_) {
        resize_client(std::max(client_w_ - (grown_ ? strip : 0), 1), std::max(client_h_ - (grown_tabs_ ? tabs : 0), 1));
      } else {
        sync_size();
      }
      grown_ = false;
      grown_tabs_ = false;
      pointer_in_panel_ = false;
      panel_drag_ = false;
      divider_drag_ = false;
      set_cursor(cursor_shape::arrow);
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
        if (events.paths.empty()) {
          view_.set_source(*events.source);
        } else {
          view_.set_source_moved(*events.source, [&](std::string_view p) { return events.paths.apply(p); });
          last_ = nullptr; // the contexts were recreated
        }
      } catch (const std::exception& e) {
        status_ = std::string("cannot show: ") + e.what();
      }
      fit_window_to_image();
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

  /// A changed `image.width` / `height` sizes the window to the new layout,
  /// as opening the file would.
  void fit_window_to_image() {
    const image_options& image = panel_->model().image();
    if (image.width == image_w_ && image.height == image_h_) {
      return;
    }
    image_w_ = image.width;
    image_h_ = image.height;
    int avail_w = 0, avail_h = 0;
    window_->available_client_size(avail_w, avail_h);
    const window_size client = initial_window_size(
        static_cast<int>(std::lround(static_cast<float>(image.width) * scale_)) + strip_pixels(),
        static_cast<int>(std::lround(static_cast<float>(image.height) * scale_)) + tab_pixels(),
        avail_w,
        avail_h);
    resize_client(client.width, client.height);
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
        const inspector_events events = panel_->frame(now_seconds(), view_.element_list(), status_line());
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
    // The figure's draw data fills the bottom-left of the window (OpenGL
    // counts from the bottom), which is the left side under the tab bar.
    // A data tab hides it.
    if (last_ != nullptr && (panel_ == nullptr || !panel_->figure_box().empty())) {
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
    const int want_h = static_cast<int>(std::lround(g.layout_height * scale_)) + tab_pixels();
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
  bool grown_tabs_ = false; ///< The window was made taller for the tab bar.
  bool panel_drag_ = false; ///< A button went down over the panel and is still held.
  bool divider_drag_ = false; ///< The divider is being dragged.
  std::optional<std::pair<float, float>> right_down_; ///< Where the right button went down in the figure.
  double divider_clicked_at_ = -1.0; ///< When the divider was last pressed, for double-clicks.
  float strip_wanted_ = kDefaultStripWidth; ///< The strip width asked for, kept while the panel is hidden.
  cursor_shape cursor_ = cursor_shape::arrow;
  static constexpr double kDoubleClick = 0.4; ///< Seconds between the presses of a double-click.
  bool pointer_in_panel_ = false;
  std::string status_; ///< What was saved or refused, for the panel.
  int image_w_ = 0; ///< The image size last shown, to see when the source changes it.
  int image_h_ = 0;
  std::string saved_source_; ///< The source the input file holds, to tell unsaved changes.
  void(NYMPH_GL_CALL* read_pixels_)(int, int, int, int, unsigned, unsigned, void*) = nullptr;
  void(NYMPH_GL_CALL* pixel_store_)(unsigned, int) = nullptr;
#if defined(NYMPH_EDITOR_MCP)
  // Last: destroyed first (the destructor stops the server even before).
  std::unique_ptr<ipc::editor_record> record_;
  std::unique_ptr<agent_host> host_;
  std::unique_ptr<mcp::editor_server> server_;
  int last_connections_ = 0;
#endif
};

} // namespace

void show_window(const view_request& request) {
  viewer v(request);
  v.run();
}

} // namespace bdg::nymph
