// MIT License © 2026 Binary Dice Games
/// @file view.hpp
/// @brief The `view` key of a source: zoom, pan, rotation and hidden
///        series per element, as a user left them in the web viewer
///        (spec/04_web_viewer/DESIGN.md section 3, "Saved view").
#pragma once

#include <array>
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
  gantt, ///< `x`.
  plot3d, ///< `x`, `y`, `z`, `rotation`, `hidden`.
  diagram, ///< `zoom`, `center`.
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

  bool empty() const {
    return !x && !y && !y2 && !z && hidden.empty() && !zoom && !center && !rotation;
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
std::string write_view(const view_map& views);

} // namespace bdg::nymph
