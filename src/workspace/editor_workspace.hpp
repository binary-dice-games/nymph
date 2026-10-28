// MIT License © 2026 Binary Dice Games
/// @file editor_workspace.hpp
/// @brief The editor as both hosts drive it: the figure (a live_view), the
///        editor panel and, later, the source pane, with the pointer and key
///        routing between them, drawn with Dear ImGui's OpenGL3 backend into
///        the current GL context (spec/15_web_app/DESIGN.md section 5.1).
///        `nymph view` (viewer_window.cpp) and the web editor
///        (web/editor/nymph_editor_web.cpp) only add their window, files and
///        clipboard.
#pragma once

#include <imgui.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "edit/editor_model.hpp"
#include "edit/inspector.hpp"
#include "edit/workspace_layout.hpp"
#include "view/live_view.hpp"
#include "workspace/editor_panel.hpp"
#include "workspace/workspace_input.hpp"

namespace bdg::nymph {

/// @brief What the workspace asks of its host. Every call comes on the UI
///        thread, from inside a workspace call.
class workspace_host {
 public:
  virtual ~workspace_host() = default;
  /// @brief Shows `shape` as the pointer.
  virtual void set_cursor(cursor_shape shape) {
    (void)shape;
  }
  /// @brief The Save button or Ctrl+S. The native host opens the panel's
  ///        "overwrite?" dialog (editor_panel::ask_save()); the web host
  ///        tells the page.
  virtual void request_save() {}
  /// @brief Import CSV into `table`: answer with
  ///        editor_workspace::import_csv(). Only with host files on.
  virtual void request_csv_import(const std::string& table) {
    (void)table;
  }
  /// @brief Export CSV: `csv` is the table's text, `file_name` a suggestion.
  virtual void request_csv_export(const std::string& table, const std::string& file_name, const std::string& csv) {
    (void)table;
    (void)file_name;
    (void)csv;
  }
  /// @brief The workspace would like to be `css_width` x `css_height` CSS
  ///        pixels: the layout does not fit, or the image size changed. A
  ///        window grows; a page ignores it.
  virtual void request_size(float css_width, float css_height) {
    (void)css_width;
    (void)css_height;
  }
  /// @brief What a confirmation dialog of the panel decided.
  virtual void decided(editor_panel::decision decision) {
    (void)decision;
  }
  /// @brief An accepted change (panel, toolbar, tables, undo, an agent).
  virtual void changed(const commit_result& r) {
    (void)r;
  }
  /// @brief The status line under the panel's toolbar, from the
  ///        workspace's own status (what was saved, refused, changed): the
  ///        native host adds "agent connected".
  virtual std::string status_line(const std::string& status) const {
    return status;
  }
  /// @brief The figure needs fonts it does not have (the web build fetches
  ///        them, then calls editor_workspace::retry_view()).
  virtual void need_fonts(const std::vector<std::string>& fonts) {
    (void)fonts;
  }
  /// @brief Makes the GL context current (before a read-back).
  virtual void make_current() {}
  /// @brief Whether the host gives the panel its clipboard (the page's);
  ///        else ImGui's own is used.
  virtual bool has_clipboard() const {
    return false;
  }
  virtual void set_clipboard(std::string_view text) {
    (void)text;
  }
  virtual std::string clipboard() {
    return {};
  }
};

/// @brief The OpenGL calls the workspace makes itself (the backend loads
///        its own). Native: looked up at run time; web: Emscripten's.
struct workspace_gl {
#if defined(_WIN32)
#define NYMPH_WORKSPACE_GL_CALL __stdcall
#else
#define NYMPH_WORKSPACE_GL_CALL
#endif
  void(NYMPH_WORKSPACE_GL_CALL* viewport)(int, int, int, int) = nullptr;
  void(NYMPH_WORKSPACE_GL_CALL* clear_color)(float, float, float, float) = nullptr;
  void(NYMPH_WORKSPACE_GL_CALL* clear)(unsigned) = nullptr;
  void(NYMPH_WORKSPACE_GL_CALL* read_pixels)(int, int, int, int, unsigned, unsigned, void*) = nullptr; ///< optional
  void(NYMPH_WORKSPACE_GL_CALL* pixel_store)(unsigned, int) = nullptr; ///< optional
#undef NYMPH_WORKSPACE_GL_CALL

  /// @brief Fills the table from `load` (a name to its address).
  /// @return false when a required call is missing.
  bool load(const std::function<void*(const char*)>& load);
};

/// @brief What key() did with a key.
enum class key_outcome {
  consumed, ///< Routed to the panel or the figure, or an editor shortcut.
  unhandled, ///< Not the workspace's: the host's own shortcuts may use it.
};

/// @brief The figure and the editor of one document. Create, use and destroy
///        it on one thread with the GL context current where it says so.
class editor_workspace {
 public:
  /// @param glsl_version The GL context's GLSL version line; "" for a
  ///        headless workspace (no GL: frames run, nothing is drawn; tests
  ///        and the web editor's Node tests).
  /// @param gl The calls the workspace makes (workspace_gl::load()).
  editor_workspace(workspace_host& host, view_options options, std::string glsl_version, workspace_gl gl);
  /// GL context current.
  ~editor_workspace();
  editor_workspace(const editor_workspace&) = delete;
  editor_workspace& operator=(const editor_workspace&) = delete;

  /// @brief Installs ImGui's OpenGL3 backend in the view's contexts. Call
  ///        once, GL context current, before the first source.
  void init_view();

  live_view& view() {
    return view_;
  }
  const live_view& view() const {
    return view_;
  }
  /// @brief Pins the panel's look (light or dark, the accent), now and for
  ///        a panel shown later; std::nullopt: it follows the figure's theme
  ///        (editor_panel::set_theme()).
  void set_appearance(std::optional<panel_appearance> appearance) {
    appearance_ = appearance;
    if (panel_ != nullptr && appearance_) {
      panel_->set_appearance(*appearance_);
    }
  }

  /// @brief The panel, or nullptr while it is hidden.
  editor_panel* panel() {
    return panel_.get();
  }
  const editor_panel* panel() const {
    return panel_.get();
  }

  /// @brief The file's name, for the panel's dialogs.
  void set_file_name(std::string name) {
    file_name_ = std::move(name);
  }
  /// @brief The source the file holds: what "modified" compares with.
  void set_saved_source(std::string source);
  const std::string& saved_source() const {
    return saved_source_;
  }
  /// @brief Whether the CSV import / export go to the host
  ///        (request_csv_import / _export) instead of the ImGui dialog.
  void set_host_files(bool on);

  /// @brief Shows or hides the editor panel (GL context current).
  /// @param repair With a source that does not bind, open the panel in
  ///        repair state (source pane only) instead of refusing.
  /// @return "" when done, else why the source cannot be edited (the panel
  ///         stays hidden).
  std::string show_panel(bool on, bool repair = false);

  /// @brief Opens `source` as the document, with the panel when `edit`:
  ///        a source that does not bind opens in repair state (the source
  ///        pane at the error, the figure empty) when `edit`, and throws
  ///        nymph::error otherwise. Throws fonts_needed as live_view does.
  void open(const std::string& source, bool reset_view, bool edit);
  /// @brief The panel is in repair state: its source does not bind yet.
  bool repairing() const {
    return panel_ != nullptr && panel_->model().repairing();
  }
  /// @brief Shows or hides the source pane (with the panel; Ctrl+U).
  void show_source(bool on, std::optional<int> line = std::nullopt);
  /// @brief Shows the model's source in the figure again after the fonts it
  ///        needed were registered (workspace_host::need_fonts()).
  void retry_view();
  /// @brief Text typed in the panel or the source pane waits for its pause.
  bool pending() const {
    return panel_ != nullptr && (panel_->panel().pending() || panel_->source_pending());
  }
  bool panel_shown() const {
    return panel_ != nullptr;
  }

  /// @brief The workspace's size: CSS pixels and device pixels per CSS pixel.
  void resize(float css_width, float css_height, float scale);
  float css_width() const {
    return css_w_;
  }
  float css_height() const {
    return css_h_;
  }
  float scale() const {
    return scale_;
  }

  /// @brief The strip's width and the tab bar's height, CSS pixels; 0
  ///        without the panel.
  float strip_width() const;
  float tab_height() const;
  /// @brief The strip width asked for (dragging the divider), kept while the
  ///        panel is hidden.
  float wanted_strip_width() const {
    return strip_wanted_;
  }

  // ── Input (CSS pixels from the workspace's top-left corner) ──────────

  void pointer(const pointer_event& e);
  /// @return Whether the workspace used the key (the host then does not).
  key_outcome key(window_key key, key_mods mods, bool down, bool repeat);
  void text(char32_t codepoint);
  /// @brief Whether a text field (panel or figure) or a dialog has the
  ///        keyboard.
  bool wants_keyboard() const;

  // ── Frames ───────────────────────────────────────────────────────────

  /// @brief Whether frame() has something to do.
  bool wants_frame() const;
  /// @brief Runs the figure's and the panel's frames when due.
  void frame(double now_seconds);
  /// @brief Draws the last frames into the back buffer (GL context
  ///        current; the host swaps).
  /// @param pixel_width, pixel_height The drawable's size in device pixels.
  void render(int pixel_width, int pixel_height);
  /// @brief Whether render() has anything to draw.
  bool has_frame() const {
    return last_ != nullptr || panel_ != nullptr;
  }

  // ── Changes ──────────────────────────────────────────────────────────

  /// @brief Applies what the panel (or a tool) did: shows a changed source,
  ///        selects, resets the view, asks to save.
  void apply(const inspector_events& events);
  /// @brief Shows an accepted change made outside the panel (an agent's
  ///        tool), with a status line naming it.
  void apply_change(const commit_result& r, const std::string& status);
  /// @brief Commits text the panel is waiting to commit (before another
  ///        change).
  void flush_pending();
  /// @brief Replaces table `table` with CSV text (the answer to
  ///        request_csv_import()).
  /// @return "" when done, else why not (also shown in the panel).
  std::string import_csv(const std::string& table, std::string_view csv);
  /// @brief Undo / redo; does nothing without the panel.
  void undo();
  void redo();
  void reset_view();
  /// @brief The status line under the panel's toolbar.
  void set_status(std::string status) {
    status_ = std::move(status);
    if (panel_ != nullptr) {
      panel_->refresh();
    }
  }
  const std::string& status() const {
    return status_;
  }
  /// @brief After a save: the source is the file's again.
  void mark_saved();
  /// @brief Whether the source differs from the saved one.
  bool modified() const;

  /// @brief A PNG of the figure as on screen (with the selection outline),
  ///        scaled down to at most `max_width` pixels (0: as it is); drawn
  ///        from the source when the figure is hidden by a data tab. Leaves
  ///        the back buffer as render() would.
  std::vector<uint8_t> capture_figure(int max_width, int& width, int& height, double now_seconds);

  /// @brief Where the layout is drawn inside the figure box (CSS px): (0, 0)
  ///        when it fills the box, else centred (view_fit::natural).
  ImVec2 figure_offset() const;

 private:
  bool divider(pointer_event::kind type, float x, int button);
  void figure_right_click(pointer_event::kind type, float x, float y, int button, const layout_box& figure);
  void set_cursor(cursor_shape shape);
  void relayout();
  void fit_to_image();
  void ask_size_if_too_small();
  std::optional<layout_box> selection_box() const;
  void draw_figure(int pixel_width, int pixel_height);
  void take_file_requests();
  static inspector_events events_of(const std::optional<std::string>& source);

  workspace_host& host_;
  std::string glsl_version_;
  workspace_gl gl_;
  live_view view_; ///< Before panel_: destroyed after it.
  std::unique_ptr<editor_panel> panel_;
  std::optional<panel_appearance> appearance_; ///< set_appearance()
  const ImDrawData* last_ = nullptr; ///< The figure's last frame, valid until the next one.
  std::string file_name_;
  std::string saved_source_;
  std::string status_;
  std::optional<std::string> repair_source_; ///< The text show_panel() opens in repair state.
  bool host_files_ = false;
  float css_w_ = 0.0f;
  float css_h_ = 0.0f;
  float scale_ = 1.0f;
  int pixel_w_ = 0;
  int pixel_h_ = 0;
  float strip_wanted_ = kDefaultStripWidth;
  bool panel_drag_ = false; ///< A button went down over the panel and is still held.
  bool figure_drag_ = false; ///< A button went down over the figure and is still held.
  enum class drag { none, strip, source } divider_drag_ = drag::none; ///< The divider being dragged.
  bool pointer_in_panel_ = false;
  std::optional<std::pair<float, float>> right_down_; ///< Where the right button went down in the figure.
  double divider_clicked_at_ = -1.0;
  cursor_shape cursor_ = cursor_shape::arrow;
  int image_w_ = 0; ///< The image size last shown, to see when the source changes it.
  int image_h_ = 0;
  float asked_w_ = 0.0f; ///< The size last asked for a layout that did not fit.
  float asked_h_ = 0.0f;
  static constexpr double kDoubleClick = 0.4; ///< Seconds between the presses of a double-click.
};

/// @brief Seconds on a monotonic clock (the frames' time).
double workspace_now();

} // namespace bdg::nymph
