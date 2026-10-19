// MIT License © 2026 Binary Dice Games
/// @file editor_panel.hpp
/// @brief The editor panel docked to the right edge of the `nymph view`
///        window: its own Dear ImGui context drawing the inspector with the
///        OpenGL3 backend into the window's context
///        (spec/08_view_editor/DESIGN.md sections 3 and 8).
#pragma once

#include <imgui.h>
#include <algorithm>
#include <optional>
#include <string>
#include <vector>
#include "edit/editor_model.hpp"
#include "edit/inspector.hpp"
#include "view/live_view.hpp"
#include "window/window_host.hpp"

namespace bdg::nymph {

/// @brief The editor's strip. Create and destroy it with the window's
///        OpenGL context current; call every other function with it current
///        too where it says so.
class editor_panel {
 public:
  /// Width of the strip, CSS pixels.
  static constexpr float kWidth = 320.0f;

  /// What the user decided in a confirmation dialog.
  enum class decision {
    none,
    save, ///< Overwrite the input.
    save_and_close, ///< Overwrite the input, then close the window.
    discard_and_close, ///< Close the window, dropping the changes.
  };

  /// @param glsl_version The window's GLSL version line.
  /// @param file_name The input's name, shown in the dialogs.
  /// @throws nymph::error if `source` is not a valid source.
  editor_panel(std::string source, std::string file_name, const char* glsl_version);
  /// OpenGL context current.
  ~editor_panel();
  editor_panel(const editor_panel&) = delete;
  editor_panel& operator=(const editor_panel&) = delete;

  /// @brief Colours the strip like the figure: "light", "dark" or
  ///        "classic". Does nothing when it is the theme already in use.
  void set_theme(const std::string& theme);

  /// @brief The window's client area (CSS pixels) and display scale: the
  ///        strip is its right-hand kWidth.
  void resize(float width, float height, float scale);

  /// @brief Where the strip starts, CSS pixels from the left.
  float strip_x() const {
    return width_ - kWidth;
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
  /// @brief Whether a popup (the colour picker) is open: it may reach over
  ///        the figure, so the strip takes the pointer wherever it is.
  bool popup_open() const {
    return popup_open_;
  }
  /// @brief Opens the "overwrite the input?" dialog (the Save button).
  void ask_save();
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
  inspector& panel() {
    return inspector_;
  }

  /// @brief Asks for frames (a change from outside the strip).
  void refresh() {
    frames_left_ = std::max(frames_left_, 3);
  }

 private:
  void draw_dialogs();

  editor_model model_;
  inspector inspector_;
  ImGuiContext* context_ = nullptr;
  float width_ = 0.0f;
  float height_ = 0.0f;
  float scale_ = 1.0f;
  int frames_left_ = 3;
  bool wants_keyboard_ = false;
  bool popup_open_ = false;
  bool modal_open_ = false;
  std::string file_name_;
  enum class ask { none, save, close } asked_ = ask::none; ///< A dialog to open at the next frame.
  decision decision_ = decision::none;
  std::string theme_;
  std::optional<double> last_time_;
};

} // namespace bdg::nymph
