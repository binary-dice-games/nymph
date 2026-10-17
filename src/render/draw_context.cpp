// MIT License © 2026 Binary Dice Games
/// @file draw_context.cpp
/// @brief draw_context: element drawing helpers.
#include "render/draw_context.hpp"
#include "render/text_style.hpp"
#include "schema/element_class.hpp"

namespace bdg::nymph {

void draw_context::draw_children(const node& n) {
  // Without an ImGui context (unit tests of drawing order) there is no ID
  // stack to push to.
  const bool imgui = ImGui::GetCurrentContext() != nullptr;
  for (size_t i = 0; i < n.children.size(); ++i) {
    if (imgui) {
      ImGui::PushID(static_cast<int>(i));
    }
    draw_node(n.children[i]);
    if (imgui) {
      ImGui::PopID();
    }
  }
}

void draw_context::draw_node(const node& n) {
  const text_style_scope style(text_style_of(n), *this);
  n.cls->draw(n, *this);
}

ImVec2 draw_context::measure_node(const node& n, float width) {
  const text_style_scope style(text_style_of(n), *this);
  return n.cls->measure(n, *this, width);
}

void draw_context::arrange_node(const node& n, const rect& r) {
  const text_style_scope style(text_style_of(n), *this);
  n.cls->arrange(n, *this, r);
}

} // namespace bdg::nymph
