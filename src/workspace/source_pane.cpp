// MIT License © 2026 Binary Dice Games
/// @file source_pane.cpp
/// @brief source_pane (see source_pane.hpp).
#include "workspace/source_pane.hpp"
#include <algorithm>
#include "workspace/yaml_language.hpp"

namespace bdg::nymph {

namespace {

/// The editor's position (0-based line, index in code points) of byte
/// `offset` of UTF-8 `text`.
TextEditor::DocPos position_of(const std::string& text, size_t offset) {
  size_t line = 0;
  size_t index = 0;
  for (size_t i = 0; i < offset && i < text.size(); ++i) {
    const auto b = static_cast<unsigned char>(text[i]);
    if (b == '\n') {
      ++line;
      index = 0;
    } else if ((b & 0xC0) != 0x80) { // not a continuation byte
      ++index;
    }
  }
  return TextEditor::DocPos(line, index);
}

/// Moves `offset` back to the start of a UTF-8 sequence.
size_t char_start(const std::string& text, size_t offset) {
  while (offset > 0 && offset < text.size() && (static_cast<unsigned char>(text[offset]) & 0xC0) == 0x80) {
    --offset;
  }
  return offset;
}

} // namespace

source_pane::source_pane() {
  editor_.SetLanguage(yaml_language());
  editor_.SetTabSize(2);
  editor_.SetInsertSpacesOnTabs(true);
  editor_.SetAutoIndentEnabled(true);
  editor_.SetShowLineNumbersEnabled(true);
  editor_.SetShowMatchingBrackets(true);
  editor_.SetLineFoldingEnabled(true);
  editor_.SetCompletePairedGlyphs(false);
  editor_.SetShowWhitespacesEnabled(false);
  editor_.SetPalette(TextEditor::GetLightPalette());
}

void source_pane::reset(const std::string& text) {
  editor_.SetText(text);
  editor_.ClearMarkers();
  editor_.ClearSquiggles();
  synced_ = text;
  undo_index_ = editor_.GetUndoIndex();
  dirty_ = false;
  ahead_ = false;
  error_.clear();
}

void source_pane::follow(const std::string& source) {
  if (ahead_ || dirty_ || source == synced_) {
    return;
  }
  const std::string current = editor_.GetText();
  if (current == source) {
    synced_ = source;
    return;
  }
  // The span that differs: common prefix and suffix kept.
  size_t prefix = 0;
  const size_t limit = std::min(current.size(), source.size());
  while (prefix < limit && current[prefix] == source[prefix]) {
    ++prefix;
  }
  prefix = char_start(current, prefix);
  size_t suffix = 0;
  while (suffix < limit - prefix && current[current.size() - 1 - suffix] == source[source.size() - 1 - suffix]) {
    ++suffix;
  }
  size_t old_end = current.size() - suffix;
  size_t new_end = source.size() - suffix;
  // Keep both ends on character boundaries.
  while (old_end < current.size() && (static_cast<unsigned char>(current[old_end]) & 0xC0) == 0x80) {
    ++old_end;
    ++new_end;
  }
  // The cursor stays where it was relative to the text around it.
  const TextEditor::DocPos cursor = editor_.GetMainCursorPosition();
  const TextEditor::DocPos from = position_of(current, prefix);
  const TextEditor::DocPos to = position_of(current, old_end);
  editor_.ReplaceSectionText(from, to, std::string_view(source).substr(prefix, new_end - prefix));
  const TextEditor::DocPos new_to = position_of(source, new_end);
  TextEditor::DocPos keep = cursor;
  if (cursor > to) {
    keep.line = cursor.line + new_to.line - to.line;
    if (cursor.line == to.line) {
      keep.index = cursor.index + new_to.index - to.index;
    }
  } else if (cursor > from) {
    keep = new_to;
  }
  placing_cursor_ = true;
  editor_.SetCursor(keep);
  cursor_line_ = keep.line;
  placing_cursor_ = false;
  synced_ = source;
  undo_index_ = editor_.GetUndoIndex();
}

void source_pane::set_palette(const TextEditor::Palette& palette) {
  editor_.SetPalette(palette);
}

std::optional<std::string> source_pane::draw(double now_seconds, ImFont* font, float font_size) {
  if (font != nullptr) {
    ImGui::PushFont(font, font_size);
  }
  editor_.Render("##source_text", ImVec2(0.0f, 0.0f));
  focused_ = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
  if (font != nullptr) {
    ImGui::PopFont();
  }
  if (editor_.GetUndoIndex() != undo_index_) {
    undo_index_ = editor_.GetUndoIndex();
    changed_at_ = now_seconds;
    dirty_ = true;
  }
  // A cursor the pane did not place itself was moved by the user.
  const size_t line = editor_.GetMainCursorPosition().line;
  if (line != cursor_line_) {
    cursor_line_ = line;
    cursor_moved_ = true;
    cursor_at_ = now_seconds;
  }
  if (dirty_ && now_seconds - changed_at_ >= kApplyDelay) {
    dirty_ = false;
    std::string text = editor_.GetText();
    if (text != synced_ || ahead_) {
      return text;
    }
  }
  return std::nullopt;
}

void source_pane::refused(int line, int column, const std::string& message) {
  ahead_ = true;
  error_ = message;
  editor_.ClearMarkers();
  editor_.ClearSquiggles();
  const size_t l = line > 0 ? static_cast<size_t>(line - 1) : 0;
  if (l < editor_.GetLineCount()) {
    const ImU32 red = IM_COL32(220, 50, 47, 255);
    editor_.AddMarker(l, red, IM_COL32(220, 50, 47, 40), message, message);
    const size_t c = column > 0 ? static_cast<size_t>(column - 1) : 0;
    const size_t len = editor_.GetLineText(l).size();
    const size_t from = std::min(c, len);
    editor_.AddSquiggle(TextEditor::DocPos(l, from), TextEditor::DocPos(l, std::max(from + 1, len)), 1, red, message);
  }
}

void source_pane::accepted(const std::string& source) {
  ahead_ = false;
  error_.clear();
  editor_.ClearMarkers();
  editor_.ClearSquiggles();
  synced_ = source;
  if (editor_.GetText() != source) {
    follow(source);
  }
}

void source_pane::go_to_line(int line) {
  if (line <= 0) {
    return;
  }
  const size_t l = std::min(static_cast<size_t>(line - 1), editor_.GetLineCount() > 0 ? editor_.GetLineCount() - 1 : 0);
  if (l == editor_.GetMainCursorPosition().line) {
    return;
  }
  editor_.SetCursor(TextEditor::DocPos(l, 0));
  cursor_line_ = l; // placed by the pane: not a move of the user's
  cursor_moved_ = false;
}

std::optional<int> source_pane::take_cursor_line(double now_seconds) {
  if (!cursor_moved_ || dirty_ || now_seconds - cursor_at_ < kApplyDelay) {
    return std::nullopt;
  }
  cursor_moved_ = false;
  return static_cast<int>(cursor_line_) + 1;
}

} // namespace bdg::nymph
