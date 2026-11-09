// MIT License © 2026 Binary Dice Games
/// @file panel_style.cpp
/// @brief The editor's palette and sizes (see panel_style.hpp). The colours
///        are the web app's (web/app/app.css), so the editor and its page
///        read as one design.
#include "workspace/panel_style.hpp"

namespace bdg::nymph {

namespace {

ImVec4 rgb(unsigned hex, float alpha = 1.0f) {
  return ImVec4(
      static_cast<float>((hex >> 16) & 0xff) / 255.0f,
      static_cast<float>((hex >> 8) & 0xff) / 255.0f,
      static_cast<float>(hex & 0xff) / 255.0f,
      alpha);
}

ImVec4 mix(const ImVec4& a, const ImVec4& b, float t) {
  return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
}

ImVec4 alpha(ImVec4 c, float a) {
  c.w = a;
  return c;
}

ImU32 u32(const ImVec4& c) {
  return ImGui::ColorConvertFloat4ToU32(c);
}

} // namespace

panel_palette palette_of(const panel_appearance& a) {
  panel_palette p;
  if (a.dark) {
    p.bg = rgb(0x171b22);
    p.surface = rgb(0x1f242d);
    p.surface_2 = rgb(0x262c36);
    p.text = rgb(0xe6e9ef);
    p.muted = rgb(0x9aa4b5);
    p.border = rgb(0x323946);
    p.accent = rgb(a.accent_dark);
    p.accent_text = rgb(0x06231d);
    p.accent_soft = mix(p.bg, p.accent, 0.22f);
    p.danger = rgb(0xf07167);
  } else {
    p.bg = rgb(0xffffff);
    p.surface = rgb(0xffffff);
    p.surface_2 = rgb(0xf2f4f7);
    p.text = rgb(0x1b2230);
    p.muted = rgb(0x5f6b7d);
    p.border = rgb(0xe1e5eb);
    p.accent = rgb(a.accent);
    p.accent_text = rgb(0xffffff);
    p.accent_soft = mix(p.bg, p.accent, 0.13f);
    p.danger = rgb(0xc4352b);
  }
  return p;
}

void apply_panel_style(ImGuiStyle& style, const panel_appearance& a) {
  // Sizes: roomy, rounded, thin borders, no 3D effects.
  style.WindowRounding = 0.0f; // the panel's windows fill their regions; the toolbar rounds its own
  style.ChildRounding = 8.0f;
  style.FrameRounding = 7.0f;
  style.PopupRounding = 10.0f;
  style.GrabRounding = 6.0f;
  style.ScrollbarRounding = 8.0f;
  style.TabRounding = 6.0f;
  style.WindowPadding = ImVec2(14.0f, 12.0f);
  style.FramePadding = ImVec2(10.0f, 6.0f);
  style.CellPadding = ImVec2(6.0f, 4.0f);
  style.ItemSpacing = ImVec2(8.0f, 8.0f);
  style.ItemInnerSpacing = ImVec2(6.0f, 5.0f);
  style.IndentSpacing = 16.0f;
  style.ScrollbarSize = 10.0f;
  style.GrabMinSize = 12.0f;
  style.WindowBorderSize = 0.0f;
  style.ChildBorderSize = 1.0f;
  style.PopupBorderSize = 1.0f;
  style.FrameBorderSize = 1.0f;
  style.TabBorderSize = 0.0f;
  style.TabBarBorderSize = 1.0f;
  style.TabBarOverlineSize = 2.0f;
  style.SeparatorTextBorderSize = 1.0f;
  style.SeparatorTextPadding = ImVec2(12.0f, 4.0f);
  style.SeparatorTextAlign = ImVec2(0.0f, 0.5f);
  style.WindowMenuButtonPosition = ImGuiDir_None;
  style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
  style.SelectableTextAlign = ImVec2(0.0f, 0.5f);
  style.DisabledAlpha = 0.45f;
  style.AntiAliasedLines = true;
  style.AntiAliasedFill = true;

  const panel_palette p = palette_of(a);
  ImVec4* c = style.Colors;
  const ImVec4 clear(0, 0, 0, 0);
  const ImVec4 hover = mix(p.surface_2, p.text, a.dark ? 0.08f : 0.05f);
  const ImVec4 press = mix(p.surface_2, p.text, a.dark ? 0.14f : 0.10f);
  c[ImGuiCol_Text] = p.text;
  c[ImGuiCol_TextDisabled] = p.muted;
  c[ImGuiCol_WindowBg] = p.bg;
  c[ImGuiCol_ChildBg] = clear;
  c[ImGuiCol_PopupBg] = p.surface;
  c[ImGuiCol_Border] = p.border;
  c[ImGuiCol_BorderShadow] = clear;
  c[ImGuiCol_FrameBg] = a.dark ? p.surface_2 : p.bg;
  c[ImGuiCol_FrameBgHovered] = a.dark ? hover : p.surface_2;
  c[ImGuiCol_FrameBgActive] = a.dark ? press : p.surface_2;
  c[ImGuiCol_TitleBg] = p.surface;
  c[ImGuiCol_TitleBgActive] = p.surface;
  c[ImGuiCol_TitleBgCollapsed] = p.surface;
  c[ImGuiCol_MenuBarBg] = p.bg;
  c[ImGuiCol_ScrollbarBg] = clear;
  c[ImGuiCol_ScrollbarGrab] = alpha(p.muted, 0.35f);
  c[ImGuiCol_ScrollbarGrabHovered] = alpha(p.muted, 0.55f);
  c[ImGuiCol_ScrollbarGrabActive] = alpha(p.muted, 0.7f);
  c[ImGuiCol_CheckMark] = p.accent;
  c[ImGuiCol_CheckboxSelectedBg] = p.accent_soft;
  c[ImGuiCol_SliderGrab] = p.accent;
  c[ImGuiCol_SliderGrabActive] = mix(p.accent, p.text, 0.15f);
  // Buttons are flat until hovered, as the toolbar's tools.
  c[ImGuiCol_Button] = p.surface_2;
  c[ImGuiCol_ButtonHovered] = hover;
  c[ImGuiCol_ButtonActive] = p.accent_soft;
  // A selected row, a hovered row, a pressed row.
  c[ImGuiCol_Header] = p.accent_soft;
  c[ImGuiCol_HeaderHovered] = p.surface_2;
  c[ImGuiCol_HeaderActive] = p.accent_soft;
  c[ImGuiCol_Separator] = p.border;
  c[ImGuiCol_SeparatorHovered] = p.accent;
  c[ImGuiCol_SeparatorActive] = p.accent;
  c[ImGuiCol_ResizeGrip] = clear;
  c[ImGuiCol_ResizeGripHovered] = alpha(p.accent, 0.5f);
  c[ImGuiCol_ResizeGripActive] = p.accent;
  c[ImGuiCol_InputTextCursor] = p.accent;
  c[ImGuiCol_Tab] = clear;
  c[ImGuiCol_TabHovered] = p.surface_2;
  c[ImGuiCol_TabSelected] = p.bg;
  c[ImGuiCol_TabSelectedOverline] = p.accent;
  c[ImGuiCol_TabDimmed] = clear;
  c[ImGuiCol_TabDimmedSelected] = p.bg;
  c[ImGuiCol_TabDimmedSelectedOverline] = p.accent;
  c[ImGuiCol_PlotLines] = p.accent;
  c[ImGuiCol_PlotLinesHovered] = p.accent;
  c[ImGuiCol_PlotHistogram] = p.accent;
  c[ImGuiCol_PlotHistogramHovered] = p.accent;
  c[ImGuiCol_TableHeaderBg] = p.surface_2;
  c[ImGuiCol_TableBorderStrong] = p.border;
  c[ImGuiCol_TableBorderLight] = alpha(p.border, 0.7f);
  c[ImGuiCol_TableRowBg] = clear;
  c[ImGuiCol_TableRowBgAlt] = alpha(p.surface_2, 0.6f);
  c[ImGuiCol_TextLink] = p.accent;
  c[ImGuiCol_TextSelectedBg] = alpha(p.accent, 0.28f);
  c[ImGuiCol_TreeLines] = p.border;
  c[ImGuiCol_DragDropTarget] = p.accent;
  c[ImGuiCol_DragDropTargetBg] = alpha(p.accent, 0.12f);
  c[ImGuiCol_UnsavedMarker] = p.accent;
  c[ImGuiCol_NavCursor] = p.accent;
  c[ImGuiCol_NavWindowingHighlight] = p.accent;
  c[ImGuiCol_NavWindowingDimBg] = ImVec4(0.04f, 0.05f, 0.08f, 0.45f);
  c[ImGuiCol_ModalWindowDimBg] = ImVec4(0.04f, 0.05f, 0.08f, 0.45f);
}

TextEditor::Palette source_palette(const panel_appearance& a) {
  TextEditor::Palette t = a.dark ? TextEditor::GetDarkPalette() : TextEditor::GetLightPalette();
  const panel_palette p = palette_of(a);
  const auto set = [&](TextEditor::Color which, const ImVec4& c) { t[static_cast<size_t>(which)] = u32(c); };
  set(TextEditor::Color::background, a.dark ? p.bg : rgb(0xfbfcfd));
  set(TextEditor::Color::text, p.text);
  set(TextEditor::Color::identifier, p.text);
  set(TextEditor::Color::punctuation, p.muted);
  set(TextEditor::Color::comment, alpha(p.muted, 0.9f));
  set(TextEditor::Color::keyword, a.dark ? rgb(0x7cc7ff) : rgb(0x1f5fbf)); // mapping keys
  set(TextEditor::Color::knownIdentifier, p.accent); // element types
  set(TextEditor::Color::preprocessor, a.dark ? rgb(0xd6a2ff) : rgb(0x8a3fc7)); // $column
  set(TextEditor::Color::string, a.dark ? rgb(0xf2b37a) : rgb(0xa1561b));
  set(TextEditor::Color::number, a.dark ? rgb(0xa5d6a7) : rgb(0x2e7d32));
  set(TextEditor::Color::declaration, a.dark ? rgb(0xf48fb1) : rgb(0xb5306a)); // true / false / null
  set(TextEditor::Color::lineNumber, alpha(p.muted, 0.7f));
  set(TextEditor::Color::currentLineNumber, p.text);
  set(TextEditor::Color::currentLineHighlight, alpha(p.surface_2, a.dark ? 0.6f : 0.9f));
  set(TextEditor::Color::currentLineHighlightBorder, ImVec4(0, 0, 0, 0));
  set(TextEditor::Color::selection, alpha(p.accent, 0.25f));
  set(TextEditor::Color::cursor, p.accent);
  set(TextEditor::Color::whitespace, alpha(p.muted, 0.35f));
  return t;
}

} // namespace bdg::nymph
