// MIT License © 2026 Binary Dice Games
/// @file source_editor.cpp
/// @brief source_editor (see source_editor.hpp).
#include "workspace/source_editor.hpp"
#include <algorithm>
#include "workspace/yaml_language.hpp"

namespace bdg::nymph {

source_editor::source_editor() {
  editor_.SetLanguage(yaml_language());
  editor_.SetTabSize(2);
  editor_.SetInsertSpacesOnTabs(true);
  editor_.SetAutoIndentEnabled(true);
  editor_.SetShowLineNumbersEnabled(true);
  editor_.SetShowMatchingBrackets(true);
  editor_.SetLineFoldingEnabled(false);
  editor_.SetCompletePairedGlyphs(false);
  editor_.SetShowWhitespacesEnabled(false);
  editor_.SetPalette(TextEditor::GetLightPalette());
}

void source_editor::reset(const std::string& text) {
  editor_.SetText(text);
  editor_.ClearMarkers();
  editor_.ClearSquiggles();
  undo_index_ = editor_.GetUndoIndex();
  dirty_ = false;
  error_.clear();
  error_line_ = 0;
  error_column_ = 0;
}

void source_editor::set_palette(const TextEditor::Palette& palette) {
  editor_.SetPalette(palette);
}

std::optional<std::string> source_editor::draw(double now_seconds, ImVec2 size, ImFont* font, float font_size) {
  if (font != nullptr) {
    ImGui::PushFont(font, font_size);
  }
  if (focus_) {
    editor_.SetFocus();
    focus_ = false;
  }
  // In a child of its own, so its focus is the editor's alone (not the
  // log's beside it).
  ImGui::BeginChild(
      "##source_box", size, ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  editor_.Render("##source_text", ImVec2(0.0f, 0.0f));
  focused_ = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
  ImGui::EndChild();
  if (font != nullptr) {
    ImGui::PopFont();
  }
  if (editor_.GetUndoIndex() != undo_index_) {
    undo_index_ = editor_.GetUndoIndex();
    changed_at_ = now_seconds;
    dirty_ = true;
  }
  if (dirty_ && now_seconds - changed_at_ >= kParseDelay) {
    dirty_ = false;
    return editor_.GetText();
  }
  return std::nullopt;
}

std::optional<std::string> source_editor::flush() {
  if (editor_.GetUndoIndex() != undo_index_) {
    undo_index_ = editor_.GetUndoIndex();
    dirty_ = true;
  }
  if (!dirty_) {
    return std::nullopt;
  }
  dirty_ = false;
  return editor_.GetText();
}

void source_editor::refused(int line, int column, const std::string& message) {
  error_ = message;
  error_line_ = line;
  error_column_ = column;
  editor_.ClearMarkers();
  editor_.ClearSquiggles();
  // Marks only: they never touch the text or the cursor.
  const size_t l = line > 0 ? static_cast<size_t>(line - 1) : 0;
  if (line > 0 && l < editor_.GetLineCount()) {
    const ImU32 red = IM_COL32(220, 50, 47, 255);
    editor_.AddMarker(l, red, IM_COL32(220, 50, 47, 40), message, message);
    const size_t len = editor_.GetLineText(l).size();
    if (len > 0) { // from the column to the line's end, inside the line
      const size_t from = std::min(column > 0 ? static_cast<size_t>(column - 1) : 0, len - 1);
      editor_.AddSquiggle(TextEditor::DocPos(l, from), TextEditor::DocPos(l, len), 1, red, message);
    }
  }
}

void source_editor::accepted() {
  error_.clear();
  error_line_ = 0;
  error_column_ = 0;
  editor_.ClearMarkers();
  editor_.ClearSquiggles();
}

} // namespace bdg::nymph
