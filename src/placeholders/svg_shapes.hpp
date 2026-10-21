// MIT License © 2026 Binary Dice Games
/// @file svg_shapes.hpp
/// @brief A parser for the small SVG subset placeholder images are written
///        in (spec/09_ui_mockups/DESIGN.md section 3, "Placeholder
///        images"): shapes as paths of lines and cubic curves, each in one
///        of three colour roles.
#pragma once

#include <imgui.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace bdg::nymph {

/// @brief What colour a shape takes when drawn.
enum class svg_role {
  tint, ///< `#000000`: the placeholder's `tint`.
  half, ///< `#808080`: `tint` at half its alpha.
  cutout, ///< `#FFFFFF`: the background, to cut shapes out.
};

/// @brief One piece of a subpath: a line or a cubic Bezier curve to `to`.
struct svg_segment {
  bool cubic = false;
  ImVec2 c1{0.0f, 0.0f}; ///< Control points (cubic only).
  ImVec2 c2{0.0f, 0.0f};
  ImVec2 to{0.0f, 0.0f};
};

/// @brief A connected run of segments from `start`.
struct svg_subpath {
  ImVec2 start{0.0f, 0.0f};
  std::vector<svg_segment> segments;
  bool closed = false;
};

/// @brief One filled or stroked shape, in viewBox units.
struct svg_shape {
  svg_role role = svg_role::tint;
  bool stroke = false; ///< Stroked with `width`, else filled.
  float width = 1.0f;
  float dash = 0.0f; ///< `stroke-dasharray` (shapes only): dash and gap, viewBox units; 0 is solid.
  float gap = 0.0f;
  std::vector<svg_subpath> subpaths;
};

/// @brief A rectangle in viewBox units (the `text` and `content` rects of a
///        shape).
struct svg_rect {
  bool present = false;
  ImVec2 min{0.0f, 0.0f};
  ImVec2 max{0.0f, 0.0f};
};

/// @brief A parsed placeholder or shape.
struct svg_image {
  ImVec2 view_min{0.0f, 0.0f}; ///< The viewBox.
  ImVec2 view_size{0.0f, 0.0f};
  bool stretch = false; ///< `preserveAspectRatio="none"`: always fills its box.
  std::vector<svg_shape> shapes; ///< In painter's order.

  // Shape files only (spec/13_more_diagrams_and_shapes/DESIGN.md section 3).
  float slice[4] = {0, 0, 0, 0}; ///< `data-slice`: left, top, right, bottom margins that keep their size.
  bool grow_proportional = false; ///< `data-grow="proportional"` (else `margins`).
  svg_rect text; ///< `id="text"`: where the label goes.
  svg_rect content; ///< `id="content"`: where a container's children go.
  svg_subpath outline; ///< `id="outline"`: the border for clipping (empty: the viewBox).
  bool has_outline = false;
};

/// @brief Which files are accepted: placeholders (square-ish, no arcs, no
///        metadata) or shapes (any viewBox, arcs, `data-*` and the `text` /
///        `content` / `outline` ids).
enum class svg_dialect { placeholder, shape };

/// @brief A file outside the subset; the message names the rule.
class svg_error : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

/// @brief Parses a placeholder file: one `<svg>` with a `viewBox`, holding
///        `path` (commands M L H V C S Q T Z, absolute or relative; no
///        arcs), `rect` (with `rx` / `ry`), `circle`, `ellipse`, `polygon`
///        and `line` elements whose `fill` / `stroke` are `#000000`,
///        `#808080`, `#FFFFFF` or `none`. Comments and the XML declaration
///        are skipped. With `svg_dialect::shape`, also arcs (`A`, converted to
///        cubics), `id` attributes, `data-slice` / `data-grow` on `<svg>` and
///        the reserved ids `text`, `content` and `outline`.
/// @throws svg_error for anything else (an element, attribute, colour or
///         command outside the subset), naming it.
svg_image parse_svg(std::string_view text, svg_dialect dialect = svg_dialect::placeholder);

/// @brief Points of `subpath` with its curves flattened into `segments`
///        lines each (for checks: drawing lets ImGui flatten them).
std::vector<ImVec2> flatten(const svg_subpath& subpath, int segments = 16);

} // namespace bdg::nymph
