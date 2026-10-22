// MIT License © 2026 Binary Dice Games
/// @file workspace_layout.hpp
/// @brief Where the editor's regions go in the `nymph view --edit` window:
///        the figure (or a data table) on the left under a tab bar, the
///        editor strip on the right, the divider between them
///        (spec/14_editor_workspace/DESIGN.md sections 3 and 4). Pure
///        arithmetic in CSS pixels, so it is tested without a window.
#pragma once

namespace bdg::nymph {

/// @brief An axis-aligned box, CSS pixels from the window's top-left corner.
struct layout_box {
  float x = 0.0f;
  float y = 0.0f;
  float width = 0.0f;
  float height = 0.0f;

  bool empty() const {
    return width <= 0.0f || height <= 0.0f;
  }
  bool contains(float px, float py) const {
    return !empty() && px >= x && px < x + width && py >= y && py < y + height;
  }
};

/// @brief The editor strip's width when nothing else is asked for.
inline constexpr float kDefaultStripWidth = 320.0f;
/// @brief The narrowest the strip can be dragged.
inline constexpr float kMinStripWidth = 240.0f;
/// @brief The narrowest the figure can become by dragging the divider.
inline constexpr float kMinFigureWidth = 240.0f;
/// @brief Half the width of the divider's grab zone.
inline constexpr float kDividerGrab = 3.0f;

/// @brief The strip width to use in a window `window_width` wide when
///        `wanted` was asked for: at least kMinStripWidth, leaving the figure
///        at least kMinFigureWidth. A window too narrow for both keeps the
///        figure's minimum and gives the strip what is left (never negative).
inline float clamp_strip_width(float window_width, float wanted) {
  const float upper = window_width - kMinFigureWidth;
  float width = wanted < kMinStripWidth ? kMinStripWidth : wanted;
  if (width > upper) {
    width = upper;
  }
  return width < 0.0f ? 0.0f : width;
}

/// @brief Whether `x` is on the divider at `strip_x` (the strip's left edge).
inline bool divider_hit(float x, float strip_x) {
  const float d = x - strip_x;
  return d >= -kDividerGrab && d <= kDividerGrab;
}

/// @brief The regions of the window.
struct workspace_layout {
  layout_box tabs; ///< The tab bar over the left side; empty without tabs.
  layout_box left; ///< The left side under the tab bar: the figure or a table.
  layout_box strip; ///< The editor strip on the right.
};

/// @brief Splits a `width` x `height` window with a strip of
///        clamp_strip_width(width, wanted_strip) and a tab bar `tab_height`
///        high (0 for none) over the left side.
inline workspace_layout layout_workspace(float width, float height, float wanted_strip, float tab_height) {
  workspace_layout l;
  const float strip = clamp_strip_width(width, wanted_strip);
  const float left_w = width - strip > 1.0f ? width - strip : 1.0f;
  l.strip = {width - strip, 0.0f, strip, height};
  l.tabs = {0.0f, 0.0f, tab_height > 0.0f ? left_w : 0.0f, tab_height};
  const float top = tab_height > height ? height : tab_height;
  l.left = {0.0f, top, left_w, height - top};
  return l;
}

} // namespace bdg::nymph
