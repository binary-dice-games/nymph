// MIT License © 2026 Binary Dice Games
/// @file test_panel_style.cpp
/// @brief The editor panel's look (workspace/panel_style.hpp): light and dark
///        palettes, the accent, the figure theme followed until the host
///        pins the look; headless.
#include <gtest/gtest.h>
#include <imgui.h>
#include <string>
#include "workspace/editor_panel.hpp"
#include "workspace/panel_style.hpp"

namespace bdg::nymph {
namespace {

constexpr const char* kSource = "image: {width: 200, height: 100}\nroot: {type: Label, text: hi}\n";

ImU32 u32(const ImVec4& c) {
  return ImGui::ColorConvertFloat4ToU32(c);
}

/// A colour of the panel's own ImGui style.
ImVec4 panel_color(const editor_panel& p, ImGuiCol which) {
  ImGuiContext* previous = ImGui::GetCurrentContext();
  ImGui::SetCurrentContext(p.imgui_context());
  const ImVec4 c = ImGui::GetStyle().Colors[which];
  ImGui::SetCurrentContext(previous);
  return c;
}

TEST(PanelStyle, PalettesMatchThePage) {
  const panel_palette light = palette_of(panel_appearance{});
  const panel_palette dark = palette_of(panel_appearance{.dark = true});
  EXPECT_EQ(u32(light.accent), u32(ImVec4(0x0f / 255.0f, 0x9f / 255.0f, 0x86 / 255.0f, 1.0f)));
  EXPECT_EQ(u32(dark.accent), u32(ImVec4(0x2b / 255.0f, 0xc6 / 255.0f, 0xa8 / 255.0f, 1.0f)));
  EXPECT_GT(light.bg.x, 0.9f);
  EXPECT_LT(dark.bg.x, 0.2f);
  EXPECT_LT(light.text.x, 0.3f); // dark text on light
  EXPECT_GT(dark.text.x, 0.8f);
}

TEST(PanelStyle, StyleUsesTheAccentAndFlatSizes) {
  ImGuiContext* context = ImGui::CreateContext();
  ImGuiStyle& style = ImGui::GetStyle();
  apply_panel_style(style, panel_appearance{.accent = 0x112233});
  EXPECT_EQ(u32(style.Colors[ImGuiCol_CheckMark]), u32(ImVec4(0x11 / 255.0f, 0x22 / 255.0f, 0x33 / 255.0f, 1.0f)));
  EXPECT_EQ(u32(style.Colors[ImGuiCol_SliderGrab]), u32(style.Colors[ImGuiCol_CheckMark]));
  EXPECT_EQ(style.Colors[ImGuiCol_ScrollbarBg].w, 0.0f);
  EXPECT_GT(style.FrameRounding, 4.0f);
  EXPECT_EQ(style.WindowRounding, 0.0f);
  ImGui::DestroyContext(context);
}

TEST(PanelStyle, PanelFollowsTheFigureThemeUntilPinned) {
  editor_panel panel(kSource, "t.yaml", "");
  EXPECT_FALSE(panel.appearance().dark);
  panel.set_theme("dark");
  EXPECT_TRUE(panel.appearance().dark);
  EXPECT_LT(panel_color(panel, ImGuiCol_WindowBg).x, 0.2f);
  panel.set_theme("light");
  EXPECT_FALSE(panel.appearance().dark);

  panel.set_appearance(panel_appearance{.dark = true, .accent_dark = 0x445566});
  panel.set_theme("classic");
  EXPECT_TRUE(panel.appearance().dark); // pinned: the figure's theme no longer decides
  EXPECT_EQ(
      u32(panel_color(panel, ImGuiCol_CheckMark)), u32(ImVec4(0x44 / 255.0f, 0x55 / 255.0f, 0x66 / 255.0f, 1.0f)));
}

} // namespace
} // namespace bdg::nymph
