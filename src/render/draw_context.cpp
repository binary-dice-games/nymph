// MIT License © 2026 Binary Dice Games
/// @file draw_context.cpp
/// @brief draw_context: element drawing helpers.
#include "render/draw_context.hpp"
#include "schema/element_class.hpp"

namespace bdg::nymph {

void draw_context::draw_children(const node& n) {
  for (const node& child : n.children) {
    child.cls->draw(child, *this);
  }
}

} // namespace bdg::nymph
