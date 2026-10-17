// MIT License © 2026 Binary Dice Games
/// @file register_builtin.cpp
/// @brief register_builtin_elements(): the one list of element families.
#include "elements/register_builtin.hpp"
#include "elements/layout_elements.hpp"
#include "elements/plot2d.hpp"
#include "elements/plot3d.hpp"
#include "elements/uml/uml.hpp"

namespace bdg::nymph {

void register_builtin_elements(registry& reg) {
  register_plot2d(reg);
  register_plot3d(reg);
  register_layout(reg);
  register_uml(reg);
}

const registry& builtin_registry() {
  static const registry reg = [] {
    registry r;
    register_builtin_elements(r);
    return r;
  }();
  return reg;
}

} // namespace bdg::nymph
