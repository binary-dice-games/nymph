// MIT License © 2026 Binary Dice Games
/// @file editor_panel.hpp
/// @brief The editor panel docked to the right edge of the `nymph view`
///        window: its own Dear ImGui context drawing the inspector with the
///        OpenGL3 backend into the window's context
///        (spec/08_view_editor/DESIGN.md sections 3 and 8).
#pragma once

#include <imgui.h>
#include <algorithm>
#include <filesystem>
#include <functional>
#include <string_view>
#include <optional>
#include <string>
#include <vector>
#include "edit/edit_toolbar.hpp"
#include "edit/editor_model.hpp"
#include "edit/inspector.hpp"
#include "edit/table_grid.hpp"
#include "edit/workspace_layout.hpp"
#include "view/live_view.hpp"
#include "workspace/panel_style.hpp"
#include "workspace/workspace_input.hpp"

namespace bdg::nymph {

/// @brief The editor's strip. Create and destroy it with the window's
///        OpenGL context current; call every other function with it current
///        too where it says so.
class editor_panel {
 public:

  /// What the user decided in a confirmation dialog.
  enum class decision {
    none,
    save, ///< Overwrite the input.
    save_and_close, ///< Overwrite the input, then close the window.
    discard_and_close, ///< Close the window, dropping the changes.
  };

  /// @param glsl_version The window's GLSL version line; nullptr or "" for
  ///        a panel that runs its frames without drawing them (tests, the
  ///        web editor's headless mode).
  /// @param file_name The input's name, shown in the dialogs.
  /// @throws nymph::error if `source` is not a valid source.
  editor_panel(std::string source, std::string file_name, const char* glsl_version);
  /// OpenGL context current.
  ~editor_panel();
  editor_panel(const editor_panel&) = delete;
  editor_panel& operator=(const editor_panel&) = delete;

  /// @brief Uses the host's clipboard instead of ImGui's own (the web
  ///        editor: the page's clipboard). `get` returns the text to paste.
  void set_clipboard(std::function<void(std::string_view)> set, std::function<std::string()> get);

  /// @brief The figure's theme ("light", "dark", "classic"): the panel is
  ///        dark for "dark", light otherwise, until set_appearance() pins
  ///        its look.
  void set_theme(const std::string& theme);

  /// @brief Asks for a frame when a font the panel waits for (its sans
  ///        text font) has become available since the last frame (the web
  ///        page fetches them).
  void check_fonts();

  /// @brief Pins the panel's look (light or dark, the accent) whatever the
  ///        figure's theme: the web editor takes its page's.
  void set_appearance(const panel_appearance& appearance);
  /// @brief The look in use.
  const panel_appearance& appearance() const {
    return appearance_;
  }

  /// @brief The window's client area (CSS pixels) and display scale: the
  ///        strip is its right-hand strip_width().
  void resize(float width, float height, float scale);

  /// @brief The strip's width in the current window, CSS pixels: the width
  ///        asked for, clamped (clamp_strip_width()).
  float strip_width() const {
    return width_ > 0.0f ? clamp_strip_width(width_, wanted_strip_) : wanted_strip_; // before the first resize()
  }
  /// @brief The width asked for (by dragging the divider), kept while the
  ///        window is too narrow for it.
  float wanted_strip_width() const {
    return wanted_strip_;
  }
  /// @brief Asks for a strip `width` CSS pixels wide (the divider was dragged).
  void set_strip_width(float width) {
    wanted_strip_ = width;
    refresh();
  }

  /// @brief Where the strip starts, CSS pixels from the left.
  float strip_x() const {
    return width_ - strip_width();
  }

  /// @brief Height of the tab bar over the left side, CSS pixels.
  static constexpr float kTabBarHeight = 28.0f;

  /// @brief The regions of the window (workspace_layout.hpp).
  workspace_layout layout() const {
    return layout_workspace(width_, height_, wanted_strip_, kTabBarHeight);
  }

  /// @brief Where the figure is shown: the left side under the tab bar
  ///        while "Chart" is the tab, empty while a data table is.
  layout_box figure_box() const {
    return tab_ ? layout_box{} : layout().left;
  }

  /// @brief The data table shown instead of the figure ("" for the single
  ///        unnamed one), or nullopt for "Chart".
  const std::optional<std::string>& data_tab() const {
    return tab_;
  }
  /// @brief Shows table `name`'s tab, or "Chart" for nullopt.
  void show_tab(std::optional<std::string> name);

  /// @brief The folder of the input file: where the CSV dialogs start.
  void set_folder(std::filesystem::path folder) {
    grid_.set_folder(std::move(folder));
  }

  // ── Input (x, y in CSS pixels from the window's top-left corner) ──────
  void pointer(pointer_event::kind type, float x, float y, int button, float wheel_x, float wheel_y);
  void key(window_key key, key_mods mods, bool down, bool repeat);
  void text(char32_t codepoint);

  /// @brief Whether a text field has the keyboard (so the viewer's own key
  ///        shortcuts must not fire).
  bool wants_keyboard() const {
    return wants_keyboard_;
  }
  /// @brief Where the selection is drawn (CSS pixels in the window), for
  ///        the floating toolbar to sit next to; nullopt when it is not
  ///        drawn.
  void set_anchor(std::optional<layout_box> anchor) {
    const bool same = anchor.has_value() == anchor_.has_value() &&
        (!anchor || (anchor->x == anchor_->x && anchor->y == anchor_->y && anchor->width == anchor_->width &&
                     anchor->height == anchor_->height));
    if (!same) {
      anchor_ = anchor;
      refresh();
    }
  }
  /// @brief Whether the floating toolbar covers (`x`, `y`), CSS pixels in
  ///        the window: the pointer there goes to the panel.
  bool over_toolbar(float x, float y) const {
    return toolbar_.contains(x, y);
  }
  edit_toolbar& toolbar() {
    return toolbar_;
  }

  /// @brief Whether a popup (the colour picker) is open: it may reach over
  ///        the figure, so the strip takes the pointer wherever it is.
  bool popup_open() const {
    return popup_open_;
  }
  /// @brief A tree shortcut on the selected element, outside text fields:
  ///        Delete, Ctrl+D (duplicate), Alt+Up / Alt+Down (move).
  /// @return What changed, as frame() reports it; nothing for other keys.
  inspector_events shortcut(window_key key, key_mods mods);

  /// @brief Opens the menu of the element at `path` (add, insert, move,
  ///        delete) at (`x`, `y`), CSS pixels in the window: a right-click on
  ///        it in the figure.
  void open_element_menu(const std::string& path, float x, float y) {
    inspector_.menu().open_at(path, x, y);
    refresh();
  }

  /// @brief Opens the "overwrite the input?" dialog (the Save button).
  void ask_save();
  /// @brief The file the Save dialogs name, and an optional line under the
  ///        question; empty `question` restores "Overwrite <file>?" (a
  ///        mermaid input saves to a new YAML file:
  ///        spec/16_mermaid_import/DESIGN.md section 5.3).
  void set_save_target(std::string file_name, std::string question = {}, std::string note = {}) {
    file_name_ = std::move(file_name);
    save_question_ = std::move(question);
    save_note_ = std::move(note);
  }
  /// @brief Opens the "unsaved changes" dialog (the window is being closed).
  /// @return false, and no dialog, when there is nothing to lose.
  bool ask_close();
  /// @brief Whether a dialog is open; it takes the keyboard too.
  bool modal_open() const {
    return modal_open_;
  }
  /// @brief What a dialog decided since the last call, once.
  decision take_decision() {
    const decision d = decision_;
    decision_ = decision::none;
    return d;
  }

  /// @brief Whether the strip needs another frame().
  bool wants_frame() const;

  /// @brief Runs one frame of the strip: draw data for render().
  /// @param tree live_view::element_list().
  /// @param status A line under the toolbar.
  inspector_events frame(double now_seconds, const std::vector<edit_element>& tree, const std::string& status);

  /// @brief Draws the last frame. OpenGL context current.
  void render();

  editor_model& model() {
    return model_;
  }
  table_grid& grid() {
    return grid_;
  }
  /// @brief The panel's ImGui context (tests, diagnostics).
  ImGuiContext* imgui_context() const {
    return context_;
  }
  inspector& panel() {
    return inspector_;
  }
  /// @brief A number field (the strip's or the toolbar's) is being typed into.
  bool number_typing() const {
    return inspector_.number_typing() || toolbar_.number_typing();
  }
  /// @brief No frames are owed and no number field is typed into: what the
  ///        last input started has played out (a click's text field takes a
  ///        few frames to appear).
  bool settled() const {
    return frames_left_ <= 0 && !number_typing();
  }

  /// @brief Asks for frames (a change from outside the strip).
  void refresh() {
    frames_left_ = std::max(frames_left_, 3);
  }

 private:
  void draw_dialogs();
  void draw_tabs(inspector_events& events);
  void draw_table_dialogs(inspector_events& events);

  editor_model model_;
  inspector inspector_;
  edit_toolbar toolbar_;
  std::optional<layout_box> anchor_; ///< The selection in the figure, for the toolbar.

  ImGuiContext* context_ = nullptr;
  ImFont* ui_font_ = nullptr; ///< FiraSans, once its bytes are there.
  panel_appearance appearance_;
  bool appearance_pinned_ = false; ///< set_appearance() was called.
  void apply_appearance(const panel_appearance& appearance);
  static constexpr float kUiFontSize = 14.5f;
  std::function<void(std::string_view)> clipboard_set_;
  std::function<std::string()> clipboard_get_;
  std::string clipboard_text_; ///< What clipboard_get_ returned last, kept for ImGui.
  bool headless_ = false; ///< No OpenGL backend: frames run, nothing is drawn.
  float width_ = 0.0f;
  float height_ = 0.0f;
  float scale_ = 1.0f;
  float wanted_strip_ = kDefaultStripWidth;
  int frames_left_ = 3;
  bool wants_keyboard_ = false;
  bool popup_open_ = false;
  bool modal_open_ = false;
  std::string file_name_;
  std::string save_question_; ///< replaces "Overwrite <file> with your changes?" when set
  std::string save_note_;     ///< replaces "The file is replaced; ..." when set
  enum class ask { none, save, close } asked_ = ask::none; ///< A dialog to open at the next frame.
  decision decision_ = decision::none;
  std::string theme_;
  std::optional<double> last_time_;
  table_grid grid_;
  std::optional<std::string> tab_; ///< The data table shown; nullopt: "Chart".
  bool select_tab_ = false; ///< tab_ was set from outside the tab bar: select it there.
  enum class table_ask { none, name_both, rename } table_ask_ = table_ask::none;
  bool open_table_dialog_ = false;
  std::string renaming_; ///< The table being renamed.
  char first_name_[64] = {}; ///< Name dialogs' fields.
  char second_name_[64] = {};
};

} // namespace bdg::nymph
