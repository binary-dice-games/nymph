// MIT License © 2026 Binary Dice Games
/// @file source_editor.hpp
/// @brief The advanced edit mode's YAML editor (ImGuiColorTextEdit): the
///        user writes the chart's source by hand
///        (spec/14_editor_workspace/DESIGN.md section 13). Pure ImGui: drawn
///        in the panel's context by editor_panel.
#pragma once

#include <TextEditor.h>
#include <imgui.h>
#include <optional>
#include <string>

namespace bdg::nymph {

/// @brief The text editor and what is known about its text.
///
/// The text is the user's alone: nothing but reset() ever writes into it,
/// so typing is never raced by the model (the reason the earlier source
/// pane was removed, spec/14_editor_workspace/DESIGN.md section 13). After a
/// pause of kParseDelay the panel tries the text (draw() returns it); the
/// outcome comes back through accepted() or refused(), which only change
/// the error marks.
class source_editor {
 public:
  /// @brief Seconds without a change before the text is tried.
  static constexpr double kParseDelay = 0.3;

  source_editor();

  /// @brief Replaces the text, the undo history and the marks (entering
  ///        the advanced mode).
  void reset(const std::string& text);

  /// @brief The editor's colours (panel_style.hpp source_palette()).
  void set_palette(const TextEditor::Palette& palette);

  /// @brief Draws the editor into the current window, `size` CSS pixels
  ///        (0: the space left), in `font` (nullptr: the current font).
  /// @return The text to try, once, when the pause after a change is over.
  std::optional<std::string> draw(double now_seconds, ImVec2 size, ImFont* font, float font_size);

  /// @brief The text changed since it was last tried: returns it now (a
  ///        save, leaving the mode), else nullopt.
  std::optional<std::string> flush();

  /// @brief The text does not load: marks the loader's position (1-based
  ///        line and column; 0 when it has none) with `message`.
  void refused(int line, int column, const std::string& message);
  /// @brief The text loads: the marks go.
  void accepted();

  /// @brief Why the text last tried does not load; "" when it loads.
  const std::string& error() const {
    return error_;
  }
  /// @brief The line and column of error() (1-based; 0 when unknown).
  int error_line() const {
    return error_line_;
  }
  int error_column() const {
    return error_column_;
  }

  /// @brief A change waits for its pause: frames must keep coming.
  bool pending() const {
    return dirty_;
  }
  /// @brief The editor had the keyboard in the last frame.
  bool focused() const {
    return focused_;
  }
  /// @brief Gives the editor the keyboard at the next draw().
  void focus() {
    focus_ = true;
  }

  std::string text() const {
    return editor_.GetText();
  }

  TextEditor& editor() {
    return editor_;
  }

 private:
  TextEditor editor_;
  size_t undo_index_ = 0; ///< The editor's undo index when last seen: a change moves it.
  double changed_at_ = 0.0;
  bool dirty_ = false;
  bool focused_ = false;
  bool focus_ = false;
  std::string error_;
  int error_line_ = 0;
  int error_column_ = 0;
};

} // namespace bdg::nymph
