// MIT License © 2026 Binary Dice Games
/// @file imuml_render.cpp
/// @brief Drawing primitives of ImUml: paths, dashes, markers and shapes,
///        on an ImDrawList, in screen coordinates.
///
/// Curves and circles use explicit segment counts, computed from their
/// size, so the geometry does not depend on ImGui's adaptive tessellation
/// settings.
#include "imuml_internal.h"

int ImUml::CircleSegments(float r) {
  return ImClamp((int)(r * 1.5f) + 12, 12, 64);
}

void ImUml::FlattenPath(const ImVec2* points, int count, bool bezier, ImVector<ImVec2>* out) {
  out->resize(0);
  if (count <= 0) {
    return;
  }
  out->push_back(points[0]);
  if (!bezier) {
    for (int i = 1; i < count; ++i) {
      out->push_back(points[i]);
    }
    return;
  }
  for (int i = 0; i + 3 < count; i += 3) {
    const ImVec2 p0 = points[i], c1 = points[i + 1], c2 = points[i + 2], p1 = points[i + 3];
    const float len = ImSqrt(ImLengthSqr(c1 - p0)) + ImSqrt(ImLengthSqr(c2 - c1)) + ImSqrt(ImLengthSqr(p1 - c2));
    const int segments = ImClamp((int)(len / 6.0f), 4, 48);
    for (int s = 1; s <= segments; ++s) {
      out->push_back(ImBezierCubicCalc(p0, c1, c2, p1, (float)s / (float)segments));
    }
  }
}

void ImUml::RenderPath(ImDrawList* dl, const ImVec2* pts, int count, ImU32 col, float lw, bool dashed, float dash,
                       float gap) {
  if (count < 2) {
    return;
  }
  if (!dashed) {
    dl->AddPolyline(pts, count, col, ImDrawFlags_None, lw);
    return;
  }
  // Dashes by arc length, continuing across the path's corners.
  const float period = ImMax(dash + gap, 1.0f);
  float phase = 0.0f; // distance into the current period
  ImVector<ImVec2> run;
  for (int i = 0; i + 1 < count; ++i) {
    const ImVec2 a = pts[i], b = pts[i + 1];
    const float len = ImSqrt(ImLengthSqr(b - a));
    if (len <= 0.0f) {
      continue;
    }
    const ImVec2 dir = (b - a) * (1.0f / len);
    float t = 0.0f;
    while (t < len) {
      const bool on = phase < dash;
      const float left = on ? dash - phase : period - phase;
      const float step = ImMin(left, len - t);
      if (on) {
        if (run.empty()) {
          run.push_back(a + dir * t);
        }
        run.push_back(a + dir * (t + step));
      }
      t += step;
      phase += step;
      if (phase >= period - 1e-4f) {
        phase = 0.0f;
      }
      if (!(phase < dash) && !run.empty()) {
        dl->AddPolyline(run.Data, run.Size, col, ImDrawFlags_None, lw);
        run.resize(0);
      }
    }
  }
  if (run.Size >= 2) {
    dl->AddPolyline(run.Data, run.Size, col, ImDrawFlags_None, lw);
  }
}

float ImUml::MarkerLength(ImUmlMarker marker, float size) {
  switch (marker) {
    case ImUmlMarker_Filled:
    case ImUmlMarker_Hollow:
      return size;
    case ImUmlMarker_HollowDiamond:
    case ImUmlMarker_FilledDiamond:
      return size * 1.6f;
    default:
      return 0.0f;
  }
}

void ImUml::RenderMarker(ImDrawList* dl, ImUmlMarker marker, const ImVec2& tip, const ImVec2& dir, float size,
                         ImU32 line, ImU32 bg, float lw) {
  const ImVec2 n(-dir.y, dir.x);
  const ImVec2 back = tip - dir * size;
  const float w = size * 0.5f;
  switch (marker) {
    case ImUmlMarker_Open:
      dl->AddLine(tip, back + n * w, line, lw);
      dl->AddLine(tip, back - n * w, line, lw);
      break;
    case ImUmlMarker_Filled:
      dl->AddTriangleFilled(tip, back + n * w, back - n * w, line);
      break;
    case ImUmlMarker_Hollow:
      dl->AddTriangleFilled(tip, back + n * w, back - n * w, bg);
      dl->AddTriangle(tip, back + n * w, back - n * w, line, lw);
      break;
    case ImUmlMarker_HollowDiamond:
    case ImUmlMarker_FilledDiamond: {
      const float len = size * 1.6f;
      const ImVec2 mid = tip - dir * (len * 0.5f);
      const ImVec2 end = tip - dir * len;
      const float half = size * 0.45f;
      const ImVec2 l = mid + n * half, r = mid - n * half;
      dl->AddQuadFilled(tip, l, end, r, marker == ImUmlMarker_FilledDiamond ? line : bg);
      dl->AddQuad(tip, l, end, r, line, lw);
      break;
    }
    case ImUmlMarker_Cross: {
      const float h = size * 0.6f;
      dl->AddLine(tip + ImVec2(-h, -h), tip + ImVec2(h, h), line, lw * 1.5f);
      dl->AddLine(tip + ImVec2(-h, h), tip + ImVec2(h, -h), line, lw * 1.5f);
      break;
    }
    default:
      break;
  }
}

void ImUml::RenderDashedRect(ImDrawList* dl, const ImVec2& a, const ImVec2& b, ImU32 col, float lw, float dash,
                             float gap) {
  const ImVec2 pts[5] = {a, ImVec2(b.x, a.y), b, ImVec2(a.x, b.y), a};
  RenderPath(dl, pts, 5, col, lw, true, dash, gap);
}

void ImUml::RenderNoteShape(ImDrawList* dl, const ImVec2& a, const ImVec2& b, float fold, ImU32 fill, ImU32 border,
                            float lw) {
  fold = ImMin(fold, ImMin(b.x - a.x, b.y - a.y) * 0.5f);
  const ImVec2 pts[5] = {a, ImVec2(b.x - fold, a.y), ImVec2(b.x, a.y + fold), b, ImVec2(a.x, b.y)};
  dl->AddConvexPolyFilled(pts, 5, fill);
  dl->AddPolyline(pts, 5, border, ImDrawFlags_Closed, lw);
  const ImVec2 corner[3] = {ImVec2(b.x - fold, a.y), ImVec2(b.x - fold, a.y + fold), ImVec2(b.x, a.y + fold)};
  dl->AddPolyline(corner, 3, border, ImDrawFlags_None, lw);
}

void ImUml::RenderActorFigure(ImDrawList* dl, const ImVec2& a, const ImVec2& b, ImU32 col, float lw) {
  const float w = b.x - a.x, h = b.y - a.y;
  const float cx = (a.x + b.x) * 0.5f;
  const float head = ImMin(w * 0.5f, h * 0.2f);
  const ImVec2 head_c(cx, a.y + head);
  dl->AddCircle(head_c, head, col, CircleSegments(head), lw);
  const ImVec2 neck(cx, a.y + head * 2.0f);
  const ImVec2 hip(cx, a.y + h * 0.65f);
  dl->AddLine(neck, hip, col, lw);
  const float arm_y = a.y + head * 2.0f + (hip.y - neck.y) * 0.3f;
  dl->AddLine(ImVec2(a.x, arm_y), ImVec2(b.x, arm_y), col, lw);
  dl->AddLine(hip, ImVec2(a.x, b.y), col, lw);
  dl->AddLine(hip, ImVec2(b.x, b.y), col, lw);
}

void ImUml::RenderDiamond(ImDrawList* dl, const ImVec2& c, const ImVec2& half, ImU32 fill, ImU32 border, float lw) {
  const ImVec2 t(c.x, c.y - half.y), r(c.x + half.x, c.y), b(c.x, c.y + half.y), l(c.x - half.x, c.y);
  dl->AddQuadFilled(t, r, b, l, fill);
  dl->AddQuad(t, r, b, l, border, lw);
}

void ImUml::RenderFragmentFrame(ImDrawList* dl, const ImVec2& a, const ImVec2& b, const ImVec2& tab, ImU32 border,
                                ImU32 tab_fill, float lw) {
  const float cut = ImMin(tab.y * 0.5f, 8.0f);
  const ImVec2 pts[5] = {
      a, ImVec2(a.x + tab.x, a.y), ImVec2(a.x + tab.x, a.y + tab.y - cut), ImVec2(a.x + tab.x - cut, a.y + tab.y),
      ImVec2(a.x, a.y + tab.y)};
  dl->AddConvexPolyFilled(pts, 5, tab_fill);
  dl->AddPolyline(pts + 1, 4, border, ImDrawFlags_None, lw);
  dl->AddRect(a, b, border, 0.0f, ImDrawFlags_None, lw);
}

void ImUml::RenderCylinder(ImDrawList* dl, const ImVec2& a, const ImVec2& b, ImU32 fill, ImU32 border, float lw) {
  const float rx = (b.x - a.x) * 0.5f;
  const float ry = ImMin((b.y - a.y) * 0.15f, 8.0f);
  const float cx = (a.x + b.x) * 0.5f;
  const int seg = CircleSegments(rx);
  dl->AddRectFilled(ImVec2(a.x, a.y + ry), ImVec2(b.x, b.y - ry), fill);
  dl->AddEllipseFilled(ImVec2(cx, b.y - ry), ImVec2(rx, ry), fill, 0.0f, seg);
  dl->AddEllipseFilled(ImVec2(cx, a.y + ry), ImVec2(rx, ry), fill, 0.0f, seg);
  dl->AddEllipse(ImVec2(cx, a.y + ry), ImVec2(rx, ry), border, 0.0f, seg, lw);
  // The lower rim: the front half of an ellipse.
  ImVector<ImVec2> rim;
  for (int i = 0; i <= seg / 2; ++i) {
    const float t = IM_PI * (float)i / (float)(seg / 2);
    rim.push_back(ImVec2(cx + ImCos(t) * rx, b.y - ry + ImSin(t) * ry));
  }
  dl->AddPolyline(rim.Data, rim.Size, border, ImDrawFlags_None, lw);
  dl->AddLine(ImVec2(a.x, a.y + ry), ImVec2(a.x, b.y - ry), border, lw);
  dl->AddLine(ImVec2(b.x, a.y + ry), ImVec2(b.x, b.y - ry), border, lw);
}

void ImUml::RenderCardinality(ImDrawList* dl, ImUmlCardinality cardinality, const ImVec2& tip, const ImVec2& dir,
                              float size, ImU32 line, ImU32 bg, float lw) {
  // Measured back from the tip along the line: the crow's foot spreads at
  // the tip; bars and circles sit behind it, the "many" / "one" mark
  // nearest the entity and the "zero / one" minimum farther out.
  const ImVec2 n(-dir.y, dir.x);
  const float half = size * 0.55f;
  auto at = [&](float back) { return tip - dir * back; };
  auto bar = [&](float back) { dl->AddLine(at(back) + n * half, at(back) - n * half, line, lw); };
  auto circle = [&](float back) {
    const float r = size * 0.32f;
    dl->AddCircleFilled(at(back), r, bg, CircleSegments(r));
    dl->AddCircle(at(back), r, line, CircleSegments(r), lw);
  };
  const bool many = cardinality == ImUmlCardinality_ZeroOrMore || cardinality == ImUmlCardinality_OneOrMore;
  const bool zero = cardinality == ImUmlCardinality_ZeroOrOne || cardinality == ImUmlCardinality_ZeroOrMore;
  if (many) {
    const ImVec2 apex = at(size);
    dl->AddLine(apex, tip + n * half, line, lw);
    dl->AddLine(apex, tip - n * half, line, lw);
  } else {
    bar(size * 0.5f);
  }
  if (zero) {
    circle(size * 1.45f);
  } else {
    bar(size * 1.1f);
  }
}
