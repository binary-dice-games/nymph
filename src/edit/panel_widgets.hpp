// MIT License © 2026 Binary Dice Games
/// @file panel_widgets.hpp
/// @brief Two widgets of the editor's web-app look (see
///        workspace/panel_style.hpp): a primary button in the accent colour
///        and a section header that reads as a title, not a filled bar.
///        They take their colours from the current ImGui style.
#pragma once

#include <imgui.h>

namespace bdg::nymph {

/// @brief A button in the accent colour (the style's CheckMark colour, which
///        apply_panel_style() sets to the accent); ImGui::Button() otherwise.
inline bool primary_button(const char* label, const ImVec2& size = ImVec2(0, 0)) {
  const ImGuiStyle& s = ImGui::GetStyle();
  const ImVec4 accent = s.Colors[ImGuiCol_CheckMark];
  const bool dark = s.Colors[ImGuiCol_WindowBg].x < 0.5f;
  const auto shade = [&](float t) {
    const float to = dark ? 1.0f : 0.0f;
    return ImVec4(accent.x + (to - accent.x) * t, accent.y + (to - accent.y) * t, accent.z + (to - accent.z) * t, 1.0f);
  };
  ImGui::PushStyleColor(ImGuiCol_Button, accent);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, shade(0.1f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, shade(0.2f));
  ImGui::PushStyleColor(ImGuiCol_Border, accent);
  ImGui::PushStyleColor(ImGuiCol_Text, dark ? ImVec4(0.02f, 0.14f, 0.11f, 1.0f) : ImVec4(1, 1, 1, 1));
  const bool clicked = ImGui::Button(label, size);
  ImGui::PopStyleColor(5);
  return clicked;
}

/// @brief A collapsing section header without the filled bar; returns
///        whether it is open, as ImGui::CollapsingHeader().
inline bool section_header(const char* label, ImGuiTreeNodeFlags flags = 0) {
  ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImGui::GetStyleColorVec4(ImGuiCol_HeaderHovered));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 6.0f));
  const bool open = ImGui::CollapsingHeader(label, flags | ImGuiTreeNodeFlags_SpanAvailWidth);
  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor(2);
  return open;
}

} // namespace bdg::nymph
