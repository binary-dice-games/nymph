// MIT License © 2026 Binary Dice Games
/// @file register_ui.cpp
/// @brief register_ui(): every user-interface element family.
#include "elements/ui/ui_common.hpp"
#include "schema/registry.hpp"

namespace bdg::nymph {

namespace ui {
void register_screen(registry& reg);
void register_widgets(registry& reg);
void register_value_widgets(registry& reg);
void register_image_widgets(registry& reg);
void register_containers(registry& reg);
void register_tables(registry& reg);
void register_menus(registry& reg);
} // namespace ui

void register_ui(registry& reg) {
  ui::register_screen(reg);
  ui::register_widgets(reg);
  ui::register_value_widgets(reg);
  ui::register_image_widgets(reg);
  ui::register_containers(reg);
  ui::register_tables(reg);
  ui::register_menus(reg);
}

} // namespace bdg::nymph
