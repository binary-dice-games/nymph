// MIT License © 2026 Binary Dice Games
/// @file panel_style.hpp
/// @brief The editor's look: one flat, web-app style palette (light or
///        dark, one accent colour) for the panel, the tabs, the tables, the
///        dialogs, the floating toolbar and the source pane, in
///        `nymph view --edit` and the web editor alike. The web editor takes
///        light / dark and the accent from its page, so the editor looks like
///        part of it (spec/15_web_app/DESIGN.md section 5.5).
#pragma once

#include <TextEditor.h>
#include <imgui.h>

namespace bdg::nymph {

/// @brief What the host decides about the editor's look.
struct panel_appearance {
  bool dark = false;
  /// The accent (selection, focus, the primary button, checks, sliders):
  /// nymph's green unless the page says otherwise. 0xRRGGBB.
  unsigned accent = 0x0f9f86;
  /// The accent in the dark palette (lighter, as the page's CSS has it).
  unsigned accent_dark = 0x2bc6a8;

  bool operator==(const panel_appearance&) const = default;
};

/// @brief The palette's named colours, for code that draws its own (the
///        toolbar's shadow, the primary button).
struct panel_palette {
  ImVec4 bg; ///< The panel's background.
  ImVec4 surface; ///< Popups, the toolbar, menus, cards.
  ImVec4 surface_2; ///< Fields, neutral buttons.
  ImVec4 text;
  ImVec4 muted;
  ImVec4 border;
  ImVec4 accent;
  ImVec4 accent_text; ///< Text on the accent.
  ImVec4 accent_soft; ///< A selected row, a pressed tool.
  ImVec4 danger;
};

/// @brief The palette of `a`.
panel_palette palette_of(const panel_appearance& a);

/// @brief Sets every size, rounding and colour of `style` for `a`.
void apply_panel_style(ImGuiStyle& style, const panel_appearance& a);

/// @brief The source pane's colours for `a` (background and gutter as the
///        panel's, syntax colours tuned to the palette).
TextEditor::Palette source_palette(const panel_appearance& a);

} // namespace bdg::nymph
