// MIT License © 2026 Binary Dice Games
/// @file ui_flags.hpp
/// @brief ImGui flag and enumeration names for the user-interface elements
///        (ImGui's names without their prefix: `NoResize`, `Logarithmic`).
#pragma once

#include "schema/name_table.hpp"

namespace bdg::nymph::ui {

const name_table& window_flag_names(); ///< ImGuiWindowFlags_
const name_table& slider_flag_names(); ///< ImGuiSliderFlags_
const name_table& input_text_flag_names(); ///< ImGuiInputTextFlags_, without the callbacks
const name_table& color_edit_flag_names(); ///< ImGuiColorEditFlags_
const name_table& combo_flag_names(); ///< ImGuiComboFlags_
const name_table& selectable_flag_names(); ///< ImGuiSelectableFlags_
const name_table& direction_names(); ///< ImGuiDir_: Left, Right, Up, Down
const name_table& tree_node_flag_names(); ///< ImGuiTreeNodeFlags_
const name_table& tab_bar_flag_names(); ///< ImGuiTabBarFlags_
const name_table& tab_item_flag_names(); ///< ImGuiTabItemFlags_, without SetSelected (the `selected` field)
const name_table& child_flag_names(); ///< ImGuiChildFlags_
const name_table& table_flag_names(); ///< ImGuiTableFlags_
const name_table& table_column_flag_names(); ///< ImGuiTableColumnFlags_

} // namespace bdg::nymph::ui
