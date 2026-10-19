// MIT License © 2026 Binary Dice Games
/// @file view.hpp
/// @brief The `view` key of a source: zoom, pan, rotation and hidden
///        series per element, as a user left them in the web viewer
///        (spec/04_web_viewer/DESIGN.md section 3, "Saved view").
#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bdg::nymph {

struct yaml_node;
struct node;

/// @brief What view an element can have, which decides the keys of its
///        `view` entry.
enum class view_kind {
  none, ///< Nothing to save (layouts, labels, series).
  plot2d, ///< `x`, `y`, `y2`, `hidden`.
  gantt, ///< `x`, `hidden` (sections).
  plot3d, ///< `x`, `y`, `z`, `rotation`, `hidden`.
  diagram, ///< `zoom`, `center`.
  // User-interface elements (spec/09_ui_mockups/DESIGN.md section 4, "Saved view"):
  ui_window, ///< Window: `x`, `y`, `width`, `height`, `collapsed`, `closed`, `scroll`.
  ui_scroll, ///< Child: `scroll`.
  ui_open, ///< TreeNode, CollapsingHeader, Menu, Popup, PopupModal: `open`.
  ui_selected, ///< Selectable, TabItem: `selected` (true / false); TabItem also `closed`.
  ui_index, ///< Combo, ListBox: `selected` (an index).
  ui_flag, ///< Checkbox: `value` (true / false).
  ui_active, ///< RadioButton: `active`.
  ui_checked, ///< MenuItem: `checked`.
  ui_number, ///< Sliders, drags, numeric inputs: `value` (a number or a list).
  ui_text, ///< Text inputs: `value` (text).
  ui_color, ///< Colour editors: `value` ("#RRGGBB" / "#RRGGBBAA").
};

/// @brief An axis range, `min < max`.
struct view_range {
  double min = 0.0;
  double max = 0.0;
  bool operator==(const view_range&) const = default;
};

/// @brief The saved view of one element; unset parts keep the source's.
struct element_view {
  std::optional<view_range> x, y, y2, z;
  std::vector<std::string> hidden; ///< Legend entries switched off, by label.
  std::optional<double> zoom; ///< Diagrams: on top of the fit, 0.25 to 8.
  std::optional<std::array<double, 2>> center; ///< Diagrams: content point at the centre, at zoom 1.
  std::optional<std::array<double, 4>> rotation; ///< 3D plots: quaternion x, y, z, w.

  // User-interface elements; which of them an entry may have depends on
  // its view_kind.
  std::optional<std::array<double, 2>> position; ///< Window `x`, `y` (relative to the Screen).
  std::optional<std::array<double, 2>> size; ///< Window `width`, `height`.
  std::optional<std::array<double, 2>> scroll; ///< Window, Child.
  std::optional<bool> collapsed; ///< Window.
  std::optional<bool> closed; ///< Window, TabItem.
  std::optional<bool> flag; ///< open / selected / value / active / checked, by kind.
  std::optional<int64_t> index; ///< Combo, ListBox `selected`.
  std::optional<std::vector<double>> numbers; ///< Numeric widgets' `value`.
  std::optional<std::string> text; ///< Text and colour widgets' `value`.

  bool empty() const {
    return !x && !y && !y2 && !z && hidden.empty() && !zoom && !center && !rotation && !position && !size &&
        !scroll && !collapsed && !closed && !flag && !index && !numbers && !text;
  }
  bool operator==(const element_view&) const = default;
};

/// @brief Saved views by element path ("root/children/1").
using view_map = std::map<std::string, element_view, std::less<>>;

/// @brief Every element under `root` by path: "root", "root/children/0", ...
std::map<std::string, const node*, std::less<>> element_paths(const node& root);

/// @brief Reads the `view` key.
/// @param paths element_paths() of the bound root.
/// @return The entries whose path names an element that has a view kind;
///         others are ignored (the source may have changed since the view
///         was saved).
/// @throws nymph::error, positioned, for a malformed entry of such an
///         element: an unknown key (with a suggestion), a range that is not
///         two numbers min < max, a zoom outside 0.25 to 8, ...
view_map bind_view(const yaml_node& view, const std::map<std::string, const node*, std::less<>>& paths);

/// @brief The value of a `view` key for `views`, one flow mapping per line
///        ("root/children/1: {x: [0.5, 3.2], hidden: [cpu]}"); empty when
///        there is nothing to save.
/// @param kinds The view kind of each path; needed for user-interface
///        entries, whose keys depend on it (plots' are found without it).
std::string write_view(const view_map& views, const std::map<std::string, view_kind, std::less<>>& kinds = {});

} // namespace bdg::nymph
