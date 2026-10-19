// MIT License © 2026 Binary Dice Games
/// @file ui_flags.cpp
/// @brief ImGui's flag names without their prefix, for the `flags` fields
///        of the user-interface elements. Single bits come before
///        composites (name_table prefers the first match when formatting).
#include <imgui.h>
#include "elements/ui/ui_flags.hpp"

namespace bdg::nymph::ui {

const name_table& window_flag_names() {
  static const name_table t{
      {"None", ImGuiWindowFlags_None},
      {"NoTitleBar", ImGuiWindowFlags_NoTitleBar},
      {"NoResize", ImGuiWindowFlags_NoResize},
      {"NoMove", ImGuiWindowFlags_NoMove},
      {"NoScrollbar", ImGuiWindowFlags_NoScrollbar},
      {"NoScrollWithMouse", ImGuiWindowFlags_NoScrollWithMouse},
      {"NoCollapse", ImGuiWindowFlags_NoCollapse},
      {"AlwaysAutoResize", ImGuiWindowFlags_AlwaysAutoResize},
      {"NoBackground", ImGuiWindowFlags_NoBackground},
      {"NoMouseInputs", ImGuiWindowFlags_NoMouseInputs},
      {"MenuBar", ImGuiWindowFlags_MenuBar},
      {"HorizontalScrollbar", ImGuiWindowFlags_HorizontalScrollbar},
      {"NoFocusOnAppearing", ImGuiWindowFlags_NoFocusOnAppearing},
      {"NoBringToFrontOnFocus", ImGuiWindowFlags_NoBringToFrontOnFocus},
      {"AlwaysVerticalScrollbar", ImGuiWindowFlags_AlwaysVerticalScrollbar},
      {"AlwaysHorizontalScrollbar", ImGuiWindowFlags_AlwaysHorizontalScrollbar},
      {"NoNavInputs", ImGuiWindowFlags_NoNavInputs},
      {"NoNavFocus", ImGuiWindowFlags_NoNavFocus},
      {"UnsavedDocument", ImGuiWindowFlags_UnsavedDocument},
      {"NoNav", ImGuiWindowFlags_NoNav},
      {"NoDecoration", ImGuiWindowFlags_NoDecoration},
      {"NoInputs", ImGuiWindowFlags_NoInputs},
  };
  return t;
}

const name_table& slider_flag_names() {
  static const name_table t{
      {"None", ImGuiSliderFlags_None},
      {"Logarithmic", ImGuiSliderFlags_Logarithmic},
      {"NoRoundToFormat", ImGuiSliderFlags_NoRoundToFormat},
      {"NoInput", ImGuiSliderFlags_NoInput},
      {"WrapAround", ImGuiSliderFlags_WrapAround},
      {"ClampOnInput", ImGuiSliderFlags_ClampOnInput},
      {"ClampZeroRange", ImGuiSliderFlags_ClampZeroRange},
      {"NoSpeedTweaks", ImGuiSliderFlags_NoSpeedTweaks},
      {"AlwaysClamp", ImGuiSliderFlags_AlwaysClamp},
  };
  return t;
}

const name_table& input_text_flag_names() {
  // No callbacks: they call code, and the source names none.
  static const name_table t{
      {"None", ImGuiInputTextFlags_None},
      {"CharsDecimal", ImGuiInputTextFlags_CharsDecimal},
      {"CharsHexadecimal", ImGuiInputTextFlags_CharsHexadecimal},
      {"CharsScientific", ImGuiInputTextFlags_CharsScientific},
      {"CharsUppercase", ImGuiInputTextFlags_CharsUppercase},
      {"CharsNoBlank", ImGuiInputTextFlags_CharsNoBlank},
      {"AllowTabInput", ImGuiInputTextFlags_AllowTabInput},
      {"EnterReturnsTrue", ImGuiInputTextFlags_EnterReturnsTrue},
      {"EscapeClearsAll", ImGuiInputTextFlags_EscapeClearsAll},
      {"CtrlEnterForNewLine", ImGuiInputTextFlags_CtrlEnterForNewLine},
      {"ReadOnly", ImGuiInputTextFlags_ReadOnly},
      {"Password", ImGuiInputTextFlags_Password},
      {"AlwaysOverwrite", ImGuiInputTextFlags_AlwaysOverwrite},
      {"AutoSelectAll", ImGuiInputTextFlags_AutoSelectAll},
      {"ParseEmptyRefVal", ImGuiInputTextFlags_ParseEmptyRefVal},
      {"DisplayEmptyRefVal", ImGuiInputTextFlags_DisplayEmptyRefVal},
      {"NoHorizontalScroll", ImGuiInputTextFlags_NoHorizontalScroll},
      {"NoUndoRedo", ImGuiInputTextFlags_NoUndoRedo},
  };
  return t;
}

const name_table& color_edit_flag_names() {
  static const name_table t{
      {"None", ImGuiColorEditFlags_None},
      {"NoAlpha", ImGuiColorEditFlags_NoAlpha},
      {"NoPicker", ImGuiColorEditFlags_NoPicker},
      {"NoOptions", ImGuiColorEditFlags_NoOptions},
      {"NoSmallPreview", ImGuiColorEditFlags_NoSmallPreview},
      {"NoInputs", ImGuiColorEditFlags_NoInputs},
      {"NoTooltip", ImGuiColorEditFlags_NoTooltip},
      {"NoLabel", ImGuiColorEditFlags_NoLabel},
      {"NoSidePreview", ImGuiColorEditFlags_NoSidePreview},
      {"NoDragDrop", ImGuiColorEditFlags_NoDragDrop},
      {"NoBorder", ImGuiColorEditFlags_NoBorder},
      {"AlphaOpaque", ImGuiColorEditFlags_AlphaOpaque},
      {"AlphaNoBg", ImGuiColorEditFlags_AlphaNoBg},
      {"AlphaPreviewHalf", ImGuiColorEditFlags_AlphaPreviewHalf},
      {"AlphaBar", ImGuiColorEditFlags_AlphaBar},
      {"HDR", ImGuiColorEditFlags_HDR},
      {"DisplayRGB", ImGuiColorEditFlags_DisplayRGB},
      {"DisplayHSV", ImGuiColorEditFlags_DisplayHSV},
      {"DisplayHex", ImGuiColorEditFlags_DisplayHex},
      {"Uint8", ImGuiColorEditFlags_Uint8},
      {"Float", ImGuiColorEditFlags_Float},
      {"PickerHueBar", ImGuiColorEditFlags_PickerHueBar},
      {"PickerHueWheel", ImGuiColorEditFlags_PickerHueWheel},
      {"InputRGB", ImGuiColorEditFlags_InputRGB},
      {"InputHSV", ImGuiColorEditFlags_InputHSV},
  };
  return t;
}

const name_table& combo_flag_names() {
  static const name_table t{
      {"None", ImGuiComboFlags_None},
      {"PopupAlignLeft", ImGuiComboFlags_PopupAlignLeft},
      {"HeightSmall", ImGuiComboFlags_HeightSmall},
      {"HeightRegular", ImGuiComboFlags_HeightRegular},
      {"HeightLarge", ImGuiComboFlags_HeightLarge},
      {"HeightLargest", ImGuiComboFlags_HeightLargest},
      {"NoArrowButton", ImGuiComboFlags_NoArrowButton},
      {"NoPreview", ImGuiComboFlags_NoPreview},
      {"WidthFitPreview", ImGuiComboFlags_WidthFitPreview},
  };
  return t;
}

const name_table& selectable_flag_names() {
  static const name_table t{
      {"None", ImGuiSelectableFlags_None},
      {"NoAutoClosePopups", ImGuiSelectableFlags_NoAutoClosePopups},
      {"SpanAllColumns", ImGuiSelectableFlags_SpanAllColumns},
      {"AllowDoubleClick", ImGuiSelectableFlags_AllowDoubleClick},
      {"Disabled", ImGuiSelectableFlags_Disabled},
      {"AllowOverlap", ImGuiSelectableFlags_AllowOverlap},
      {"Highlight", ImGuiSelectableFlags_Highlight},
  };
  return t;
}

const name_table& direction_names() {
  static const name_table t{
      {"Left", ImGuiDir_Left},
      {"Right", ImGuiDir_Right},
      {"Up", ImGuiDir_Up},
      {"Down", ImGuiDir_Down},
  };
  return t;
}

const name_table& tree_node_flag_names() {
  static const name_table t{
      {"None", ImGuiTreeNodeFlags_None},
      {"Selected", ImGuiTreeNodeFlags_Selected},
      {"Framed", ImGuiTreeNodeFlags_Framed},
      {"DefaultOpen", ImGuiTreeNodeFlags_DefaultOpen},
      {"AllowOverlap", ImGuiTreeNodeFlags_AllowOverlap},
      {"NoTreePushOnOpen", ImGuiTreeNodeFlags_NoTreePushOnOpen},
      {"NoAutoOpenOnLog", ImGuiTreeNodeFlags_NoAutoOpenOnLog},
      {"OpenOnDoubleClick", ImGuiTreeNodeFlags_OpenOnDoubleClick},
      {"OpenOnArrow", ImGuiTreeNodeFlags_OpenOnArrow},
      {"Leaf", ImGuiTreeNodeFlags_Leaf},
      {"Bullet", ImGuiTreeNodeFlags_Bullet},
      {"FramePadding", ImGuiTreeNodeFlags_FramePadding},
      {"SpanAvailWidth", ImGuiTreeNodeFlags_SpanAvailWidth},
      {"SpanFullWidth", ImGuiTreeNodeFlags_SpanFullWidth},
      {"SpanLabelWidth", ImGuiTreeNodeFlags_SpanLabelWidth},
      {"SpanAllColumns", ImGuiTreeNodeFlags_SpanAllColumns},
      {"LabelSpanAllColumns", ImGuiTreeNodeFlags_LabelSpanAllColumns},
      {"DrawLinesNone", ImGuiTreeNodeFlags_DrawLinesNone},
      {"DrawLinesFull", ImGuiTreeNodeFlags_DrawLinesFull},
      {"DrawLinesToNodes", ImGuiTreeNodeFlags_DrawLinesToNodes},
      {"CollapsingHeader", ImGuiTreeNodeFlags_CollapsingHeader},
  };
  return t;
}

const name_table& tab_bar_flag_names() {
  static const name_table t{
      {"None", ImGuiTabBarFlags_None},
      {"Reorderable", ImGuiTabBarFlags_Reorderable},
      {"AutoSelectNewTabs", ImGuiTabBarFlags_AutoSelectNewTabs},
      {"TabListPopupButton", ImGuiTabBarFlags_TabListPopupButton},
      {"NoCloseWithMiddleMouseButton", ImGuiTabBarFlags_NoCloseWithMiddleMouseButton},
      {"NoTabListScrollingButtons", ImGuiTabBarFlags_NoTabListScrollingButtons},
      {"NoTooltip", ImGuiTabBarFlags_NoTooltip},
      {"DrawSelectedOverline", ImGuiTabBarFlags_DrawSelectedOverline},
      {"FittingPolicyMixed", ImGuiTabBarFlags_FittingPolicyMixed},
      {"FittingPolicyShrink", ImGuiTabBarFlags_FittingPolicyShrink},
      {"FittingPolicyScroll", ImGuiTabBarFlags_FittingPolicyScroll},
  };
  return t;
}

const name_table& tab_item_flag_names() {
  // SetSelected is the `selected` field.
  static const name_table t{
      {"None", ImGuiTabItemFlags_None},
      {"UnsavedDocument", ImGuiTabItemFlags_UnsavedDocument},
      {"NoCloseWithMiddleMouseButton", ImGuiTabItemFlags_NoCloseWithMiddleMouseButton},
      {"NoPushId", ImGuiTabItemFlags_NoPushId},
      {"NoTooltip", ImGuiTabItemFlags_NoTooltip},
      {"NoReorder", ImGuiTabItemFlags_NoReorder},
      {"Leading", ImGuiTabItemFlags_Leading},
      {"Trailing", ImGuiTabItemFlags_Trailing},
      {"NoAssumedClosure", ImGuiTabItemFlags_NoAssumedClosure},
  };
  return t;
}

const name_table& child_flag_names() {
  static const name_table t{
      {"None", ImGuiChildFlags_None},
      {"Borders", ImGuiChildFlags_Borders},
      {"AlwaysUseWindowPadding", ImGuiChildFlags_AlwaysUseWindowPadding},
      {"ResizeX", ImGuiChildFlags_ResizeX},
      {"ResizeY", ImGuiChildFlags_ResizeY},
      {"AutoResizeX", ImGuiChildFlags_AutoResizeX},
      {"AutoResizeY", ImGuiChildFlags_AutoResizeY},
      {"AlwaysAutoResize", ImGuiChildFlags_AlwaysAutoResize},
      {"FrameStyle", ImGuiChildFlags_FrameStyle},
      {"NavFlattened", ImGuiChildFlags_NavFlattened},
  };
  return t;
}

const name_table& table_flag_names() {
  static const name_table t{
      {"None", ImGuiTableFlags_None},
      {"Resizable", ImGuiTableFlags_Resizable},
      {"Reorderable", ImGuiTableFlags_Reorderable},
      {"Hideable", ImGuiTableFlags_Hideable},
      {"Sortable", ImGuiTableFlags_Sortable},
      {"ContextMenuInBody", ImGuiTableFlags_ContextMenuInBody},
      {"RowBg", ImGuiTableFlags_RowBg},
      {"BordersInnerH", ImGuiTableFlags_BordersInnerH},
      {"BordersOuterH", ImGuiTableFlags_BordersOuterH},
      {"BordersInnerV", ImGuiTableFlags_BordersInnerV},
      {"BordersOuterV", ImGuiTableFlags_BordersOuterV},
      {"NoBordersInBody", ImGuiTableFlags_NoBordersInBody},
      {"NoBordersInBodyUntilResize", ImGuiTableFlags_NoBordersInBodyUntilResize},
      {"SizingFixedFit", ImGuiTableFlags_SizingFixedFit},
      {"SizingFixedSame", ImGuiTableFlags_SizingFixedSame},
      {"SizingStretchProp", ImGuiTableFlags_SizingStretchProp},
      {"SizingStretchSame", ImGuiTableFlags_SizingStretchSame},
      {"NoHostExtendX", ImGuiTableFlags_NoHostExtendX},
      {"NoHostExtendY", ImGuiTableFlags_NoHostExtendY},
      {"NoKeepColumnsVisible", ImGuiTableFlags_NoKeepColumnsVisible},
      {"PreciseWidths", ImGuiTableFlags_PreciseWidths},
      {"NoClip", ImGuiTableFlags_NoClip},
      {"PadOuterX", ImGuiTableFlags_PadOuterX},
      {"NoPadOuterX", ImGuiTableFlags_NoPadOuterX},
      {"NoPadInnerX", ImGuiTableFlags_NoPadInnerX},
      {"ScrollX", ImGuiTableFlags_ScrollX},
      {"ScrollY", ImGuiTableFlags_ScrollY},
      {"SortMulti", ImGuiTableFlags_SortMulti},
      {"SortTristate", ImGuiTableFlags_SortTristate},
      {"HighlightHoveredColumn", ImGuiTableFlags_HighlightHoveredColumn},
      {"BordersH", ImGuiTableFlags_BordersH},
      {"BordersV", ImGuiTableFlags_BordersV},
      {"BordersInner", ImGuiTableFlags_BordersInner},
      {"BordersOuter", ImGuiTableFlags_BordersOuter},
      {"Borders", ImGuiTableFlags_Borders},
  };
  return t;
}

const name_table& table_column_flag_names() {
  static const name_table t{
      {"None", ImGuiTableColumnFlags_None},
      {"Disabled", ImGuiTableColumnFlags_Disabled},
      {"DefaultHide", ImGuiTableColumnFlags_DefaultHide},
      {"DefaultSort", ImGuiTableColumnFlags_DefaultSort},
      {"WidthStretch", ImGuiTableColumnFlags_WidthStretch},
      {"WidthFixed", ImGuiTableColumnFlags_WidthFixed},
      {"NoResize", ImGuiTableColumnFlags_NoResize},
      {"NoReorder", ImGuiTableColumnFlags_NoReorder},
      {"NoHide", ImGuiTableColumnFlags_NoHide},
      {"NoClip", ImGuiTableColumnFlags_NoClip},
      {"NoSort", ImGuiTableColumnFlags_NoSort},
      {"NoSortAscending", ImGuiTableColumnFlags_NoSortAscending},
      {"NoSortDescending", ImGuiTableColumnFlags_NoSortDescending},
      {"NoHeaderLabel", ImGuiTableColumnFlags_NoHeaderLabel},
      {"NoHeaderWidth", ImGuiTableColumnFlags_NoHeaderWidth},
      {"PreferSortAscending", ImGuiTableColumnFlags_PreferSortAscending},
      {"PreferSortDescending", ImGuiTableColumnFlags_PreferSortDescending},
      {"IndentEnable", ImGuiTableColumnFlags_IndentEnable},
      {"IndentDisable", ImGuiTableColumnFlags_IndentDisable},
      {"AngledHeader", ImGuiTableColumnFlags_AngledHeader},
  };
  return t;
}

} // namespace bdg::nymph::ui
