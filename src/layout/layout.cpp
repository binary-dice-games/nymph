// MIT License © 2026 Binary Dice Games
/// @file layout.cpp
/// @brief distribute(), arrange_linear(), layout_root().
#include "layout/layout.hpp"
#include <algorithm>
#include <cmath>
#include <string>
#include "core/error.hpp"
#include "schema/element_class.hpp"

namespace bdg::nymph {

namespace {

/// Overflow up to this much is rounding, not an error.
constexpr float kTolerance = 0.5f;

std::string px(float v) {
  return std::to_string(static_cast<long>(std::lround(v)));
}

[[noreturn]] void overflow(const node& n, const char* axis, float needed, float available) {
  throw error(
      n.line,
      n.column,
      std::string(n.cls->name()) + " needs " + px(needed) + " px of " + axis + ", has " + px(available) +
          "; reduce fixed sizes or spacing, give children negative (stretch) sizes, or enlarge the image");
}

/// The size of a child across the layout's axis: a positive hint, clamped
/// to the layout; anything else fills.
float cross_size(float hint, float available) {
  return hint > 0.0f ? std::min(hint, available) : available;
}

} // namespace

axis_plan distribute(std::span<const axis_item> items, float available, float spacing) {
  axis_plan plan;
  plan.sizes.resize(items.size(), 0.0f);
  float weights = 0.0f;
  for (size_t i = 0; i < items.size(); ++i) {
    const axis_item& it = items[i];
    if (it.hint > 0.0f) {
      plan.sizes[i] = it.hint;
    } else if (it.hint == 0.0f) {
      plan.sizes[i] = it.natural;
    } else {
      weights += -it.hint;
    }
    plan.needed += plan.sizes[i];
  }
  if (items.size() > 1) {
    plan.needed += spacing * static_cast<float>(items.size() - 1);
  }
  const float left = std::max(0.0f, available - plan.needed);
  if (weights > 0.0f) {
    for (size_t i = 0; i < items.size(); ++i) {
      if (items[i].hint < 0.0f) {
        plan.sizes[i] = left * (-items[i].hint) / weights;
      }
    }
  }
  return plan;
}

ImVec2 measure_linear(const node& n, draw_context& dc, float width, const linear_layout& how) {
  float along = 0.0f;
  float across = 0.0f;
  size_t count = 0;
  for (const node& child : n.children) {
    const size_hints h = child.cls->hints(child);
    const float axis_hint = how.vertical ? h.height : h.width;
    const float cross_hint = how.vertical ? h.width : h.height;
    const float child_width = how.vertical ? cross_size(cross_hint, width) : width;
    const ImVec2 natural = child.cls->measure(child, dc, child_width);
    const float natural_along = how.vertical ? natural.y : natural.x;
    const float natural_across = how.vertical ? natural.x : natural.y;
    along += axis_hint > 0.0f ? axis_hint : (axis_hint == 0.0f ? natural_along : 0.0f);
    across = std::max(across, cross_hint > 0.0f ? cross_hint : natural_across);
    ++count;
  }
  if (count > 1) {
    along += how.spacing * static_cast<float>(count - 1);
  }
  return how.vertical ? ImVec2(across, along) : ImVec2(along, across);
}

void arrange_linear(const node& n, draw_context& dc, const rect& r, const linear_layout& how) {
  const float available = how.vertical ? r.height() : r.width();
  const float across = how.vertical ? r.width() : r.height();
  std::vector<axis_item> items;
  std::vector<float> cross;
  bool any_stretch = false;
  for (const node& child : n.children) {
    const size_hints h = child.cls->hints(child);
    const float axis_hint = how.vertical ? h.height : h.width;
    const float c = cross_size(how.vertical ? h.width : h.height, across);
    float natural = 0.0f;
    if (axis_hint == 0.0f) {
      // Measured with the width the child will get: wrapped text in a
      // column depends on it; a row's child may take the whole row.
      const ImVec2 m = child.cls->measure(child, dc, how.vertical ? c : r.width());
      natural = how.vertical ? m.y : m.x;
    }
    any_stretch = any_stretch || axis_hint < 0.0f;
    items.push_back({axis_hint, natural});
    cross.push_back(c);
  }
  const axis_plan plan = distribute(items, available, how.spacing);
  if (plan.needed > available + kTolerance) {
    overflow(n, how.vertical ? "height" : "width", plan.needed, available);
  }

  float pos = how.vertical ? r.min.y : r.min.x;
  if (how.align_right && !how.vertical && !any_stretch) {
    pos += available - plan.needed;
  }
  for (size_t i = 0; i < n.children.size(); ++i) {
    const node& child = n.children[i];
    // Boundaries are rounded to whole pixels so edges stay crisp.
    const float start = std::round(pos);
    const float end = std::round(pos + plan.sizes[i]);
    rect cr;
    if (how.vertical) {
      cr = {ImVec2(r.min.x, start), ImVec2(r.min.x + std::round(cross[i]), end)};
    } else {
      cr = {ImVec2(start, r.min.y), ImVec2(end, r.min.y + std::round(cross[i]))};
    }
    dc.set_rect(child, cr);
    child.cls->arrange(child, dc, cr);
    pos += plan.sizes[i] + how.spacing;
  }
}

void layout_root(const node& root, draw_context& dc) {
  const rect content = dc.content_rect();
  const size_hints h = root.cls->hints(root);
  if (h.width > content.width() + kTolerance) {
    overflow(root, "width", h.width, content.width());
  }
  if (h.height > content.height() + kTolerance) {
    overflow(root, "height", h.height, content.height());
  }
  const float w = h.width > 0.0f ? h.width : content.width();
  const float ht = h.height > 0.0f ? h.height : content.height();
  const rect r{content.min, ImVec2(content.min.x + w, content.min.y + ht)};
  dc.set_rect(root, r);
  root.cls->arrange(root, dc, r);
}

} // namespace bdg::nymph
