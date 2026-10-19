// MIT License © 2026 Binary Dice Games
/// @file placeholders.cpp
/// @brief The placeholder catalog and its drawing.
#include "placeholders/placeholders.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <mutex>

namespace bdg::nymph {

namespace {

/// Every placeholder, parsed on first use.
const std::map<std::string, svg_image, std::less<>>& catalog() {
  static std::once_flag once;
  static std::map<std::string, svg_image, std::less<>> images;
  std::call_once(once, [] {
    for (const embedded_file& f : embedded_placeholders()) {
      images.emplace(f.name, parse_svg(f.text()));
    }
  });
  return images;
}

ImU32 with_alpha_scaled(ImU32 color, float k) {
  const ImU32 a = (color >> IM_COL32_A_SHIFT) & 0xFF;
  const ImU32 scaled = static_cast<ImU32>(std::lround(static_cast<float>(a) * k));
  return (color & ~IM_COL32_A_MASK) | (scaled << IM_COL32_A_SHIFT);
}

bool transparent(ImU32 color) {
  return (color & IM_COL32_A_MASK) == 0;
}

/// Fills the current path as a concave polygon. ImGui's concave fill
/// expects one winding order (a positive shoelace area with y down);
/// the other gives the convex hull, so a path wound the other way is
/// reversed first.
void fill_concave(ImDrawList* dl, ImU32 color) {
  ImVector<ImVec2>& path = dl->_Path;
  if (path.Size < 3) {
    dl->PathClear();
    return;
  }
  double area = 0.0;
  for (int i = 0; i < path.Size; ++i) {
    const ImVec2& a = path[i];
    const ImVec2& b = path[(i + 1) % path.Size];
    area += static_cast<double>(a.x) * b.y - static_cast<double>(b.x) * a.y;
  }
  if (area < 0.0) {
    std::reverse(path.begin(), path.end());
  }
  dl->PathFillConcave(color);
}

/// Paints the four corners of [min, max) outside a rounding of `r` in `color`.
void mask_corners(ImDrawList* dl, ImVec2 min, ImVec2 max, float r, ImU32 color) {
  constexpr float kPi = 3.14159265358979f;
  const struct {
    ImVec2 corner, centre;
    float from;
  } corners[] = {
      {min, ImVec2(min.x + r, min.y + r), kPi},
      {ImVec2(max.x, min.y), ImVec2(max.x - r, min.y + r), 1.5f * kPi},
      {max, ImVec2(max.x - r, max.y - r), 0.0f},
      {ImVec2(min.x, max.y), ImVec2(min.x + r, max.y - r), 0.5f * kPi},
  };
  for (const auto& c : corners) {
    dl->PathClear();
    dl->PathLineTo(c.corner);
    dl->PathArcTo(c.centre, r, c.from, c.from + 0.5f * kPi);
    fill_concave(dl, color);
  }
}

} // namespace

std::vector<std::string> placeholder_names() {
  std::vector<std::string> out;
  for (const embedded_file& f : embedded_placeholders()) {
    out.emplace_back(f.name);
  }
  std::sort(out.begin(), out.end());
  return out;
}

const svg_image* find_placeholder(std::string_view name) {
  const auto& images = catalog();
  const auto it = images.find(name);
  return it == images.end() ? nullptr : &it->second;
}

void draw_placeholder(ImDrawList* dl, const svg_image& image, ImVec2 min, ImVec2 max, const placeholder_style& style) {
  const ImVec2 size(max.x - min.x, max.y - min.y);
  if (size.x <= 0.0f || size.y <= 0.0f) {
    return;
  }
  const float rounding = std::min(style.rounding, std::min(size.x, size.y) * 0.5f);
  if (!transparent(style.background)) {
    dl->AddRectFilled(min, max, style.background, rounding);
  }

  // viewBox -> box.
  float sx = size.x / image.view_size.x, sy = size.y / image.view_size.y;
  if (!image.stretch && style.fit != placeholder_fit::stretch) {
    const float s = style.fit == placeholder_fit::contain ? std::min(sx, sy) : std::max(sx, sy);
    sx = sy = s;
  }
  const ImVec2 offset(
      min.x + (size.x - image.view_size.x * sx) * 0.5f - image.view_min.x * sx,
      min.y + (size.y - image.view_size.y * sy) * 0.5f - image.view_min.y * sy);
  const auto at = [&](ImVec2 p) { return ImVec2(offset.x + p.x * sx, offset.y + p.y * sy); };
  const float stroke_scale = std::sqrt(sx * sy);
  const ImU32 cutout = transparent(style.background) ? style.behind : style.background;

  dl->PushClipRect(min, max, true);
  for (const svg_shape& shape : image.shapes) {
    const ImU32 color = shape.role == svg_role::tint ? style.tint
        : shape.role == svg_role::half              ? with_alpha_scaled(style.tint, 0.5f)
                                                    : cutout;
    for (const svg_subpath& sub : shape.subpaths) {
      dl->PathClear();
      dl->PathLineTo(at(sub.start));
      for (const svg_segment& seg : sub.segments) {
        if (seg.cubic) {
          dl->PathBezierCubicCurveTo(at(seg.c1), at(seg.c2), at(seg.to));
        } else {
          dl->PathLineTo(at(seg.to));
        }
      }
      if (shape.stroke) {
        dl->PathStroke(color, sub.closed ? ImDrawFlags_Closed : ImDrawFlags_None, shape.width * stroke_scale);
      } else {
        fill_concave(dl, color);
      }
    }
  }
  dl->PopClipRect();
  if (rounding > 0.0f) {
    mask_corners(dl, min, max, rounding, style.behind);
  }
  if (style.border) {
    dl->AddRect(min, max, ImGui::GetColorU32(ImGuiCol_Border), rounding);
  }
}

} // namespace bdg::nymph
