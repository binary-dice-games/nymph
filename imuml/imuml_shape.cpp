// MIT License © 2026 Binary Dice Games
/// @file imuml_shape.cpp
/// @brief Shapes: vector artwork registered by name, drawn into any
///        rectangle (spec/13_more_diagrams_and_shapes/DESIGN.md sections 3, 4).
///
/// A resizable shape is resized by a piecewise-linear map of x and y applied
/// to every point (the slice margins keep their size, the middle stretches),
/// so sliced corners and caps stay round. An icon keeps its aspect ratio.
#include <string.h>
#include "imuml_internal.h"

extern ImUmlContext* GImUml;

namespace {

ImUmlContext& Ctx() {
  IM_ASSERT(GImUml != nullptr && "No current ImUml context: call ImUml::CreateContext()");
  return *GImUml;
}

/// Index of the first registered shape whose name is not below `name`.
int LowerBound(const ImUmlContext& ctx, const char* name) {
  int lo = 0, hi = ctx.Shapes.Size;
  while (lo < hi) {
    const int mid = (lo + hi) / 2;
    if (strcmp(ctx.Shapes[mid]->Name.Data, name) < 0) {
      lo = mid + 1;
    } else {
      hi = mid;
    }
  }
  return lo;
}

const ImUmlShapeArt* Find(const char* name) {
  if (GImUml == nullptr || name == nullptr) {
    return nullptr;
  }
  const int i = LowerBound(*GImUml, name);
  if (i < GImUml->Shapes.Size && strcmp(GImUml->Shapes[i]->Name.Data, name) == 0) {
    return GImUml->Shapes[i];
  }
  return nullptr;
}

/// One axis of the resize map from a shape's viewBox length `view` to the
/// drawn length `size`. `m0` and `m1` are the fixed margins in viewBox
/// units, `zoom` the scale they are drawn at.
struct AxisMap {
  float View, Size, M0, M1, Zoom;
  float Lo, Hi; ///< Drawn margins.
  float Scale; ///< Scale of the stretching middle.

  AxisMap(float view, float size, float m0, float m1, float zoom) : View(view), Size(size), M0(m0), M1(m1), Zoom(zoom) {
    Lo = m0 * zoom;
    Hi = m1 * zoom;
    if (Lo + Hi > size) { // too small for the margins: shrink them evenly
      const float k = (Lo + Hi) > 0.0f ? size / (Lo + Hi) : 1.0f;
      Lo *= k;
      Hi *= k;
    }
    const float middle = view - m0 - m1;
    Scale = middle > 0.0f ? (size - Lo - Hi) / middle : 1.0f;
  }

  float operator()(float v) const {
    if (v <= M0) {
      return M0 > 0.0f ? v * (Lo / M0) : 0.0f;
    }
    if (v >= View - M1) {
      return M1 > 0.0f ? Size - (View - v) * (Hi / M1) : Size;
    }
    return Lo + (v - M0) * Scale;
  }
};

/// Maps viewBox points into [`min`, `max`) for shape `s`.
struct ShapeMap {
  const ImUmlShapeArt& S;
  ImVec2 Origin;
  AxisMap X, Y;
  float IconScale;
  ImVec2 IconOffset;

  ShapeMap(const ImUmlShapeArt& s, const ImVec2& min, const ImVec2& max, float zoom)
      : S(s),
        Origin(min),
        X(s.ViewSize.x, max.x - min.x, s.Slice[0], s.Slice[2], zoom),
        Y(s.ViewSize.y, max.y - min.y, s.Slice[1], s.Slice[3], zoom) {
    const float w = max.x - min.x, h = max.y - min.y;
    IconScale = ImMin(w / s.ViewSize.x, h / s.ViewSize.y);
    IconOffset = ImVec2((w - s.ViewSize.x * IconScale) * 0.5f, (h - s.ViewSize.y * IconScale) * 0.5f);
  }

  ImVec2 operator()(const ImVec2& p) const {
    if (S.Resizable) {
      return ImVec2(Origin.x + X(p.x), Origin.y + Y(p.y));
    }
    return ImVec2(Origin.x + IconOffset.x + p.x * IconScale, Origin.y + IconOffset.y + p.y * IconScale);
  }
};

ImU32 WithAlpha(ImU32 col, float k) {
  const ImU32 a = (col >> IM_COL32_A_SHIFT) & 0xFF;
  return (col & ~IM_COL32_A_MASK) | ((ImU32)(a * k + 0.5f) << IM_COL32_A_SHIFT);
}

} // namespace

void ImUml::AddShape(const ImUmlShapeDesc& d) {
  ImUmlContext& ctx = Ctx();
  const int at = LowerBound(ctx, d.Name);
  ImUmlShapeArt* s = nullptr;
  if (at < ctx.Shapes.Size && strcmp(ctx.Shapes[at]->Name.Data, d.Name) == 0) {
    s = ctx.Shapes[at];
  } else {
    s = IM_NEW(ImUmlShapeArt)();
    ctx.Shapes.insert(ctx.Shapes.Data + at, s);
  }
  const int n = (int)strlen(d.Name);
  s->Name.resize(n + 1);
  memcpy(s->Name.Data, d.Name, (size_t)n + 1);
  s->ViewSize = d.ViewSize;
  s->Resizable = d.Resizable;
  for (int i = 0; i < 4; ++i) {
    s->Slice[i] = d.Slice[i];
  }
  s->GrowProportional = d.GrowProportional;
  s->HasText = d.HasText;
  s->TextMin = d.TextMin;
  s->TextMax = d.TextMax;
  s->HasContent = d.HasContent;
  s->ContentMin = d.ContentMin;
  s->ContentMax = d.ContentMax;
  s->Points.resize(d.PointCount);
  for (int i = 0; i < d.PointCount; ++i) {
    s->Points[i] = d.Points[i];
  }
  s->Paths.resize(d.PathCount);
  for (int i = 0; i < d.PathCount; ++i) {
    s->Paths[i] = d.Paths[i];
  }
  s->Outline.resize(d.OutlineCount);
  for (int i = 0; i < d.OutlineCount; ++i) {
    s->Outline[i] = d.Outline[i];
  }
}

const ImUmlShapeArt* ImUml::FindShapeArt(const char* name) {
  return Find(name);
}

bool ImUml::HasShape(const char* name) {
  return Find(name) != nullptr;
}

bool ImUml::IsShapeIcon(const char* name) {
  const ImUmlShapeArt* s = Find(name);
  return s != nullptr && !s->Resizable;
}

ImVec2 ImUml::GetShapeSize(const char* name) {
  const ImUmlShapeArt* s = Find(name);
  return s != nullptr ? s->ViewSize : ImVec2(0, 0);
}

ImVec2 ImUml::GetShapeNodeSize(const char* name, const ImVec2& label) {
  const ImUmlShapeArt* s = Find(name);
  if (s == nullptr) {
    return label;
  }
  if (!s->Resizable) {
    return s->ViewSize;
  }
  const ImVec2 text_min = s->HasText ? s->TextMin : ImVec2(0, 0);
  const ImVec2 text_max = s->HasText ? s->TextMax : s->ViewSize;
  const ImVec2 text_size = text_max - text_min;
  ImVec2 size;
  if (s->GrowProportional) {
    size = ImVec2(label.x * s->ViewSize.x / text_size.x, label.y * s->ViewSize.y / text_size.y);
  } else {
    size = ImVec2(label.x + s->ViewSize.x - text_size.x, label.y + s->ViewSize.y - text_size.y);
  }
  return ImVec2(ImMax(size.x, s->ViewSize.x), ImMax(size.y, s->ViewSize.y));
}

bool ImUml::GetShapeTextRect(
    const char* name, const ImVec2& min, const ImVec2& max, float zoom, ImVec2* text_min, ImVec2* text_max) {
  const ImUmlShapeArt* s = Find(name);
  if (s == nullptr) {
    return false;
  }
  if (!s->HasText) {
    *text_min = min;
    *text_max = max;
    return true;
  }
  const ShapeMap map(*s, min, max, zoom);
  *text_min = map(s->TextMin);
  *text_max = map(s->TextMax);
  return true;
}

bool ImUml::RenderShape(
    ImDrawList* dl, const char* name, const ImVec2& min, const ImVec2& max, ImU32 line, ImU32 fill, float line_weight,
    float zoom) {
  const ImUmlShapeArt* s = Find(name);
  if (s == nullptr || max.x <= min.x || max.y <= min.y) {
    return false;
  }
  const ShapeMap map(*s, min, max, zoom);
  ImVector<ImVec2> pts;
  for (int p = 0; p < s->Paths.Size; ++p) {
    const ImUmlShapePath& path = s->Paths[p];
    pts.resize(0);
    for (int i = 0; i < path.PointCount; ++i) {
      const ImVec2 q = map(s->Points[path.PointStart + i]);
      if (pts.Size == 0 || q.x != pts.back().x || q.y != pts.back().y) { // no zero-length segments
        pts.push_back(q);
      }
    }
    if (path.Closed && pts.Size > 1 && pts.back().x == pts[0].x && pts.back().y == pts[0].y) {
      pts.pop_back(); // the closing point repeats the start
    }
    if (pts.Size < 2) {
      continue;
    }
    const ImU32 col = path.Role == ImUmlShapeRole_Fill ? fill : path.Role == ImUmlShapeRole_Half ? WithAlpha(line, 0.5f) : line;
    if (path.Stroke && path.Dash > 0.0f) {
      if (path.Closed) {
        pts.push_back(pts[0]);
      }
      ImUml::RenderPath(dl, pts.Data, pts.Size, col, path.Width * line_weight, true, path.Dash * zoom, path.Gap * zoom);
    } else if (path.Stroke) {
      dl->AddPolyline(pts.Data, pts.Size, col, path.Closed ? ImDrawFlags_Closed : ImDrawFlags_None, path.Width * line_weight);
    } else if (pts.Size >= 3 && (col >> IM_COL32_A_SHIFT) != 0) {
      // ImGui's concave fill expects one winding order (positive shoelace
      // area with y down); the other gives the convex hull.
      double area = 0.0;
      for (int i = 0; i < pts.Size; ++i) {
        const ImVec2& a = pts[i];
        const ImVec2& b = pts[(i + 1) % pts.Size];
        area += (double)a.x * b.y - (double)b.x * a.y;
      }
      for (int i = 0; i < pts.Size; ++i) {
        dl->PathLineTo(pts[area < 0.0 ? pts.Size - 1 - i : i]);
      }
      dl->PathFillConcave(col);
    }
  }
  return true;
}
