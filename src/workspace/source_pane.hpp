// MIT License © 2026 Binary Dice Games
/// @file source_pane.hpp
/// @brief The YAML text of the chart beside the figure, in an ImGui code
///        editor (ImGuiColorTextEdit), kept in step with the editor model
///        both ways (spec/15_web_app/DESIGN.md section 5.4). Pure ImGui: drawn
///        in the panel's context by editor_panel.
#pragma once

#include <imgui.h>
#include <TextEditor.h>
#include <optional>
#include <string>

namespace bdg::nymph {

/// @brief The text editor and its sync state.
///
/// Typing is applied (the panel hands it to editor_model::set_source())
/// after a pause of kApplyDelay; a refused text marks its error and leaves
/// the pane "ahead" of the model until a text binds again. A change of the
/// model made elsewhere is written into the pane as one replacement of the
/// span that differs, so the cursor and scroll outside it stay.
class source_pane {
 public:
  /// @brief Seconds without a keystroke before typed text is applied, and
  ///        before a moved cursor selects its element.
  static constexpr double kApplyDelay = 0.3;

  source_pane();

  /// @brief Replaces the text without an undo step (a new document).
  void reset(const std::string& text);
  /// @brief The model's source changed elsewhere (panel, tool, undo): write
  ///        the differing span into the pane. Does nothing while the pane is
  ///        ahead or has text waiting to be applied.
  void follow(const std::string& source);

  /// @brief The editor's dark or light palette.
  void set_dark(bool dark);

  /// @brief Draws the editor into the current window, filling the space
  ///        left (`font` the monospace font, or nullptr for the current one).
  /// @return The text to apply, once the typing pause is over.
  std::optional<std::string> draw(double now_seconds, ImFont* font, float font_size);

  /// @brief The text applied was refused: underline and mark the loader's
  ///        position (1-based line and column; 0: the first line).
  void refused(int line, int column, const std::string& message);
  /// @brief The text applied was accepted: marks cleared, in step again.
  void accepted(const std::string& source);

  /// @brief The text differs from the model's and does not bind.
  bool ahead() const {
    return ahead_;
  }
  /// @brief Typed text waits for the pause.
  bool pending() const {
    return dirty_;
  }
  /// @brief The editor had the keyboard in the last frame.
  bool focused() const {
    return focused_;
  }
  /// @brief The loader's message while ahead, else "".
  const std::string& error() const {
    return error_;
  }

  std::string text() const {
    return editor_.GetText();
  }
  /// @brief The text last in step with the model.
  const std::string& synced() const {
    return synced_;
  }

  /// @brief Puts the cursor on `line` (1-based) and scrolls to it: an
  ///        element selected elsewhere.
  void go_to_line(int line);
  /// @brief The line (1-based) the user moved the cursor to in the pane,
  ///        once, after the pause.
  std::optional<int> take_cursor_line(double now_seconds);
  /// @brief Typed text or a moved cursor waits for its pause: frames are
  ///        needed.
  bool waiting() const {
    return dirty_ || cursor_moved_;
  }

  TextEditor& editor() {
    return editor_;
  }

 private:
  TextEditor editor_;
  std::string synced_;
  size_t undo_index_ = 0;
  double changed_at_ = 0.0;
  bool dirty_ = false;
  bool ahead_ = false;
  bool focused_ = false;
  std::string error_;
  size_t cursor_line_ = 0; ///< 0-based, as last seen.
  bool cursor_moved_ = false;
  double cursor_at_ = 0.0;
  bool placing_cursor_ = false; ///< follow() is moving it, not the user.
};

} // namespace bdg::nymph
