// MIT License © 2026 Binary Dice Games
/// @file viewer_window.cpp
/// @brief show_window(): an editor_workspace (the live_view and, when asked
///        for, the editor panel docked to its right edge) in a native_window,
///        drawn with Dear ImGui's OpenGL3 backend (spec/04_web_viewer/DESIGN.md
///        section 10, spec/08_view_editor/DESIGN.md,
///        spec/15_web_app/DESIGN.md section 5.1). What is left here is what a
///        window does: its size, files, close dialog, keys of its own, and the
///        editor's MCP server. Platform-neutral; the window itself is
///        window_win32.cpp or window_x11.cpp.
#include "window/viewer_window.hpp"
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
#include "window/window_host.hpp"
#include "workspace/editor_workspace.hpp"
#if defined(NYMPH_EDITOR_MCP)
#include "ipc/editor_registry.hpp"
#include "mcp/editor_server.hpp"
#include "workspace/workspace_tools.hpp"
#endif

namespace bdg::nymph {

namespace {

/// An editor_workspace in a native_window: events in, frames out.
class viewer : public window_events, public workspace_host {
 public:
  explicit viewer(const view_request& request)
      : request_(request),
        name_(std::filesystem::path(request.args.input).filename().string()),
        save_path_(edit_save_path(request.args.input, !request.mermaid_kind.empty())),
        from_mermaid_(!request.mermaid_kind.empty()),
        window_(create_native_window(*this)) {
    ready_ = true;
  }

  ~viewer() override {
    ready_ = false;
#if defined(NYMPH_EDITOR_MCP)
    server_.reset(); // agents' calls fail before the editor goes
    record_.reset();
#endif
    window_->make_current(); // the backends' shutdown (the workspace's destruction) needs the context
    ws_.reset();
  }

  /// Opens the window at the layout's size and runs until it is closed.
  void run() {
    native_window& w = *window_;
    workspace_gl gl;
    if (!gl.load([&w](const char* name) { return w.gl_proc(name); })) {
      throw window_error("cannot open a window: the OpenGL driver lacks glViewport / glClear");
    }
    ws_ = std::make_unique<editor_workspace>(
        *this, view_options{request_.args.fit, /*cpu_textures=*/false, /*tooltips=*/true}, w.glsl_version(), gl);
    ws_->init_view();
    ws_->set_file_name(name_);
    ws_->view().set_theme(request_.args.theme);
    if (request_.repair.empty()) {
      ws_->view().set_source(request_.source);
      ws_->set_saved_source(request_.source);
    }
    open_at_layout_size();
    set_title(request_.repair.empty() ? std::string() : "does not load: " + request_.repair);
    w.show();
    if (!request_.repair.empty()) {
      window_->make_current();
      ws_->open(request_.source, true, true); // repair state: the source pane at the error
      sync_size();
      ws_->panel()->set_folder(std::filesystem::path(request_.args.input).parent_path());
      start_agent_server();
    } else if (request_.args.edit) {
      set_panel(true);
    }
    while (true) {
      run_agent_jobs();
      if (ws_->wants_frame()) {
        if (!w.pump(false)) {
          return;
        }
        present(true);
        if (ws_->pending()) {
          // A typed text waits for its pause: poll instead of sleeping in the event queue.
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
      } else if (!w.pump(true)) { // idle: wait for input
        return;
      }
    }
  }

  // ── window_events ─────────────────────────────────────────────────────

  void on_resized() override {
    if (!ready_ || opening_ || ws_ == nullptr) {
      return;
    }
    sync_size();
    present(true);
  }

  void on_expose() override {
    if (ready_ && ws_ != nullptr) {
      present(false);
    }
  }

  bool on_close_request() override {
    // Unsaved changes: ask first (the dialog closes the window if the user says so).
    if (!ready_ || ws_ == nullptr) {
      return true;
    }
    if (ws_->panel() == nullptr && ws_->modified()) {
      set_panel(true); // changes made earlier with the panel hidden: show it to ask
    }
    return ws_->panel() == nullptr || !ws_->panel()->ask_close();
  }

  void on_pointer(pointer_event::kind type, float x, float y, int button, float wheel_x, float wheel_y) override {
    if (!ready_ || ws_ == nullptr) {
      return;
    }
    pointer_event e;
    e.type = type;
    e.x = x / scale_;
    e.y = y / scale_;
    e.button = button; // left, right, middle are ImGui's 0, 1, 2
    e.wheel_x = wheel_x;
    e.wheel_y = wheel_y;
    ws_->pointer(e);
  }

  void on_key(window_key key, key_mods mods, bool down, bool repeat) override {
    if (!ready_ || ws_ == nullptr) {
      return;
    }
    if (ws_->key(key, mods, down, repeat) == key_outcome::consumed || !down || repeat) {
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
        ws_->reset_view();
        break;
      case window_key::s:
        save("png");
        break;
      case window_key::y:
        save("yaml");
        break;
      case window_key::e:
        set_panel(ws_->panel() == nullptr);
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
    if (ready_ && ws_ != nullptr) {
      ws_->text(codepoint);
    }
  }

  // ── workspace_host ────────────────────────────────────────────────────

  void set_cursor(cursor_shape shape) override {
    window_->set_cursor(shape);
  }

  void request_save() override {
    if (ws_->panel() != nullptr) {
      ws_->panel()->ask_save();
    }
  }

  void request_size(float css_width, float css_height) override {
    int avail_w = 0, avail_h = 0;
    window_->available_client_size(avail_w, avail_h);
    const window_size client = initial_window_size(
        static_cast<int>(std::lround(css_width * scale_)), static_cast<int>(std::lround(css_height * scale_)), avail_w,
        avail_h);
    resize_client(client.width, client.height);
  }

  void decided(editor_panel::decision decision) override {
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

  std::string status_line(const std::string& status) const override {
#if defined(NYMPH_EDITOR_MCP)
    if (server_ != nullptr && server_->connections() > 0) {
      return status.empty() ? std::string("Agent connected") : status + "  -  agent connected";
    }
#endif
    return status;
  }

  void make_current() override {
    window_->make_current();
  }

 private:
  /// Width of the editor strip in device pixels; 0 without the panel.
  int strip_pixels() const {
    return static_cast<int>(std::lround(ws_->strip_width() * scale_));
  }

  /// Height of the tab bar over the figure in device pixels; 0 without the panel.
  int tab_pixels() const {
    return static_cast<int>(std::lround(ws_->tab_height() * scale_));
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
      host_ = std::make_unique<workspace_tools>(*ws_, [this] {
        return record_ != nullptr ? record_->entry().file : request_.args.input;
      });
    } catch (const std::exception& e) {
      server_.reset();
      record_.reset();
      ws_->set_status(std::string("agents cannot connect: ") + e.what());
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
    if (connections != last_connections_ && ws_->panel() != nullptr) {
      last_connections_ = connections;
      ws_->panel()->refresh(); // "Agent connected" comes or goes
    }
    ws_->flush_pending(); // what the user typed goes first
    server_->run_pending(*host_);
#endif
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

  /// Gives the workspace the client area in CSS pixels and the display scale.
  void sync_size() {
    scale_ = window_->scale();
    window_->client_size(client_w_, client_h_);
    if (client_w_ <= 0 || client_h_ <= 0) {
      return; // minimised
    }
    ws_->resize(static_cast<float>(client_w_) / scale_, static_cast<float>(client_h_) / scale_, scale_);
  }

  /// Shows or hides the editor panel; the window grows (or shrinks again)
  /// by the strip and the tab bar when the screen has room.
  void set_panel(bool on) {
    if (ws_->panel_shown() == on) {
      return;
    }
    window_->make_current();
    if (on) {
      const std::string refused = ws_->show_panel(true);
      if (!refused.empty()) {
        set_title("cannot edit: " + refused);
        return;
      }
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
      ws_->panel()->set_folder(std::filesystem::path(request_.args.input).parent_path());
      if (from_mermaid_) {
        // A mermaid input has no YAML file yet: Save (the conversion) is
        // offered at once, and saves next to it (spec/16_mermaid_import).
        ws_->set_saved_source(std::string());
        set_mermaid_save_target();
      }
      start_agent_server();
    } else {
      const int strip = strip_pixels();
      const int tabs = tab_pixels();
      ws_->show_panel(false);
      if (grown_ || grown_tabs_) {
        resize_client(std::max(client_w_ - (grown_ ? strip : 0), 1), std::max(client_h_ - (grown_tabs_ ? tabs : 0), 1));
      } else {
        sync_size();
      }
      grown_ = false;
      grown_tabs_ = false;
    }
    present(true);
  }

  /// Overwrites the input with the edited chart (with the view on screen
  /// saved in its `view` key): a PNG input becomes a PNG again, a YAML input
  /// stays YAML.
  bool save_input() {
    const std::string path = save_path_;
    try {
      const std::string source = ws_->view().source_with_view();
      std::ostringstream unused;
      if (is_png_file(path)) {
        const std::vector<uint8_t> png = render_png(source);
        write_output(path, as_text(png), unused);
      } else {
        write_output(path, source, unused);
      }
      ws_->mark_saved();
      if (from_mermaid_) {
        // From now on the window edits the YAML file it wrote.
        from_mermaid_ = false;
        name_ = std::filesystem::path(path).filename().string();
        ws_->set_file_name(name_);
        if (ws_->panel() != nullptr) {
          ws_->panel()->set_save_target(name_);
        }
      }
      ws_->set_status("saved " + name_);
      set_title(ws_->status());
      return true;
    } catch (const std::exception& e) {
      ws_->set_status(std::string("cannot save: ") + e.what());
      set_title(ws_->status());
      return false;
    }
  }

  /// The Save dialogs of a mermaid input: it is saved as YAML next to it.
  void set_mermaid_save_target() {
    const std::string target = std::filesystem::path(save_path_).filename().string();
    std::error_code ec;
    const bool exists = std::filesystem::exists(save_path_, ec);
    ws_->panel()->set_save_target(
        target,
        "Save the converted chart as " + target + "?",
        exists ? target + " exists and is replaced; " + name_ + " is not changed."
               : name_ + " (mermaid) is not changed; nymph saves YAML.");
  }

  /// Whether the file starts with the PNG signature.
  static bool is_png_file(const std::string& path) {
    std::ifstream in(std::filesystem::path(path), std::ios::binary);
    char head[8] = {};
    in.read(head, sizeof(head));
    static constexpr char kSignature[8] = {char(0x89), 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
    return in.gcount() == 8 && std::memcmp(head, kSignature, 8) == 0;
  }

  /// Draws new frames if due (`fresh`), or the last ones again (an exposed
  /// window), and shows them.
  void present(bool fresh) {
    if (client_w_ <= 0 || client_h_ <= 0) {
      return;
    }
    if (fresh) {
      ws_->frame(workspace_now());
    }
    if (!ws_->has_frame()) {
      return;
    }
    ws_->render(client_w_, client_h_);
    window_->swap();
  }

  /// Writes the image or the YAML on screen next to the input.
  void save(const char* extension) {
    const std::string path = view_save_path(request_.args.input, extension);
    try {
      const std::string source = ws_->view().source_with_view();
      std::ostringstream unused;
      if (std::string_view(extension) == "png") {
        const std::vector<uint8_t> png = render_png(source);
        write_output(path, as_text(png), unused);
      } else {
        write_output(path, source, unused);
      }
      ws_->set_status("saved " + std::filesystem::path(path).filename().string());
    } catch (const std::exception& e) {
      ws_->set_status(std::string("cannot save: ") + e.what());
    }
    set_title(ws_->status());
  }

  void set_title(const std::string& status) {
    window_->set_title("nymph - " + name_ + (status.empty() ? "" : " - " + status));
  }

  const view_request& request_;
  std::string name_;
  std::string save_path_; ///< Where the editor's Save writes (edit_save_path()).
  bool from_mermaid_ = false; ///< The input is mermaid and has not been saved as YAML yet.
  bool ready_ = false; ///< Events sent while the window is created find no workspace yet.
  std::unique_ptr<native_window> window_;
  std::unique_ptr<editor_workspace> ws_; ///< After window_: destroyed first, with the context current.
  float scale_ = 1.0f; ///< Device pixels per CSS pixel.
  int client_w_ = 0;
  int client_h_ = 0;
  bool opening_ = false;
  bool grown_ = false; ///< The window was made wider for the panel.
  bool grown_tabs_ = false; ///< The window was made taller for the tab bar.
#if defined(NYMPH_EDITOR_MCP)
  // Last: destroyed first (the destructor stops the server even before).
  std::unique_ptr<ipc::editor_record> record_;
  std::unique_ptr<workspace_tools> host_;
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
