// MIT License © 2026 Binary Dice Games
/// @file draw_context.cpp
/// @brief draw_context: element drawing helpers.
#include "render/draw_context.hpp"
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
    n.children[i].cls->draw(n.children[i], *this);
    if (imgui) {
      ImGui::PopID();
    }
  }
}

} // namespace bdg::nymph
