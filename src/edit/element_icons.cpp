// MIT License © 2026 Binary Dice Games
/// @file element_icons.cpp
/// @brief Element type icons (see element_icons.hpp).
#include "edit/element_icons.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace bdg::nymph {

namespace {

using icon_entry = std::pair<std::string_view, element_icon>;

/// Types named one by one; the rest go by their name's ending or start.
constexpr std::array kByName = {
    icon_entry{"Image", element_icon::image},
    icon_entry{"HorizontalLayout", element_icon::layout_h},
    icon_entry{"VerticalLayout", element_icon::layout_v},
    icon_entry{"Spring", element_icon::spring},
    icon_entry{"Spacing", element_icon::spring},
    icon_entry{"Dummy", element_icon::spring},
    icon_entry{"NewLine", element_icon::spring},
    icon_entry{"SameLine", element_icon::spring},
    icon_entry{"Label", element_icon::text},
    icon_entry{"Text", element_icon::text},
    icon_entry{"TextColored", element_icon::text},
    icon_entry{"TextDisabled", element_icon::text},
    icon_entry{"TextWrapped", element_icon::text},
    icon_entry{"TextLink", element_icon::text},
    icon_entry{"BulletText", element_icon::text},
    icon_entry{"Bullet", element_icon::text},
    icon_entry{"LabelText", element_icon::text},
    icon_entry{"PlotText", element_icon::text},
    icon_entry{"Plot3DText", element_icon::text},
    icon_entry{"Separator", element_icon::separator},
    icon_entry{"SeparatorText", element_icon::separator},
    icon_entry{"Plot", element_icon::plot},
    icon_entry{"Plot3D", element_icon::plot3d},
    icon_entry{"PlotLine", element_icon::line},
    icon_entry{"PlotStairs", element_icon::line},
    icon_entry{"PlotShaded", element_icon::line},
    icon_entry{"PlotDigital", element_icon::line},
    icon_entry{"Plot3DLine", element_icon::line},
    icon_entry{"PlotScatter", element_icon::scatter},
    icon_entry{"Plot3DScatter", element_icon::scatter},
    icon_entry{"PlotBars", element_icon::bars},
    icon_entry{"PlotBarGroups", element_icon::bars},
    icon_entry{"PlotHistogram", element_icon::bars},
    icon_entry{"PlotStems", element_icon::bars},
    icon_entry{"PlotErrorBars", element_icon::bars},
    icon_entry{"PlotBarsH", element_icon::bars_h},
    icon_entry{"GanttChart", element_icon::bars_h},
    icon_entry{"GanttSection", element_icon::bars_h},
    icon_entry{"GanttTask", element_icon::bars_h},
    icon_entry{"PlotPieChart", element_icon::pie},
    icon_entry{"PlotHeatmap", element_icon::heatmap},
    icon_entry{"PlotHistogram2D", element_icon::heatmap},
    icon_entry{"ColorScale", element_icon::heatmap},
    icon_entry{"Plot3DSurface", element_icon::heatmap},
    icon_entry{"Plot3DMesh", element_icon::heatmap},
    icon_entry{"Plot3DQuad", element_icon::heatmap},
    icon_entry{"Plot3DTriangle", element_icon::heatmap},
    icon_entry{"PlotAnnotation", element_icon::tag},
    icon_entry{"PlotTag", element_icon::tag},
    icon_entry{"PlotInfLines", element_icon::tag},
    icon_entry{"SankeyDiagram", element_icon::sankey},
    icon_entry{"SankeyNode", element_icon::sankey},
    icon_entry{"SankeyFlow", element_icon::sankey},
    icon_entry{"SequenceDiagram", element_icon::sequence},
    icon_entry{"TreeDiagram", element_icon::tree},
    icon_entry{"MindMap", element_icon::tree},
    icon_entry{"OrgChart", element_icon::tree},
    icon_entry{"Flowchart", element_icon::diagram},
    icon_entry{"Message", element_icon::link},
    icon_entry{"UseCase", element_icon::ellipse},
    icon_entry{"Actor", element_icon::actor},
    icon_entry{"Person", element_icon::actor},
    icon_entry{"Participant", element_icon::participant},
    icon_entry{"FlowGroup", element_icon::group},
    icon_entry{"Zone", element_icon::group},
    icon_entry{"TrustBoundary", element_icon::group},
    icon_entry{"Fragment", element_icon::group},
    icon_entry{"Else", element_icon::group},
    icon_entry{"Package", element_icon::group},
    icon_entry{"Note", element_icon::note},
    icon_entry{"Shape", element_icon::shape},
    icon_entry{"ActivityControl", element_icon::shape},
    icon_entry{"Screen", element_icon::window},
    icon_entry{"Window", element_icon::window},
    icon_entry{"Child", element_icon::window},
    icon_entry{"Popup", element_icon::window},
    icon_entry{"PopupModal", element_icon::window},
    icon_entry{"Tooltip", element_icon::window},
    icon_entry{"Button", element_icon::button},
    icon_entry{"SmallButton", element_icon::button},
    icon_entry{"ArrowButton", element_icon::button},
    icon_entry{"ImageButton", element_icon::button},
    icon_entry{"TabItemButton", element_icon::button},
    icon_entry{"ColorButton", element_icon::button},
    icon_entry{"Selectable", element_icon::button},
    icon_entry{"Checkbox", element_icon::checkbox},
    icon_entry{"RadioButton", element_icon::radio},
    icon_entry{"ProgressBar", element_icon::slider},
    icon_entry{"Combo", element_icon::input},
    icon_entry{"ListBox", element_icon::input},
    icon_entry{"Menu", element_icon::menu},
    icon_entry{"MenuItem", element_icon::menu},
    icon_entry{"MenuBar", element_icon::menu},
    icon_entry{"MainMenuBar", element_icon::menu},
    icon_entry{"Table", element_icon::table},
    icon_entry{"TableColumn", element_icon::table},
    icon_entry{"TableRow", element_icon::table},
    icon_entry{"TabBar", element_icon::tabs},
    icon_entry{"TabItem", element_icon::tabs},
    icon_entry{"CollapsingHeader", element_icon::tabs},
    icon_entry{"TreeNode", element_icon::tabs},
    icon_entry{"Group", element_icon::container},
    icon_entry{"Indent", element_icon::container},
    icon_entry{"Disabled", element_icon::container},
    // Diagram nodes.
    icon_entry{"Class", element_icon::node},
    icon_entry{"State", element_icon::node},
    icon_entry{"Action", element_icon::node},
    icon_entry{"FlowNode", element_icon::node},
    icon_entry{"Entity", element_icon::node},
    icon_entry{"Component", element_icon::node},
    icon_entry{"Process", element_icon::node},
    icon_entry{"DataStore", element_icon::node},
    icon_entry{"ExternalEntity", element_icon::node},
    icon_entry{"Object", element_icon::node},
    icon_entry{"Artifact", element_icon::node},
    icon_entry{"DeploymentNode", element_icon::node},
    icon_entry{"Device", element_icon::node},
    icon_entry{"MindNode", element_icon::node},
    icon_entry{"TreeItem", element_icon::node},
    icon_entry{"PackageItem", element_icon::node},
};

bool starts_with(std::string_view s, std::string_view prefix) {
  return s.substr(0, prefix.size()) == prefix;
}

bool ends_with(std::string_view s, std::string_view suffix) {
  return s.size() >= suffix.size() && s.substr(s.size() - suffix.size()) == suffix;
}

/// The family colour: plots blue, diagrams green, links orange, user
/// interface purple, layout and text grey. Mid tones, legible on light and
/// dark panels.
ImU32 color_of(element_icon icon) {
  switch (icon) {
    case element_icon::plot:
    case element_icon::plot3d:
    case element_icon::line:
    case element_icon::scatter:
    case element_icon::bars:
    case element_icon::bars_h:
    case element_icon::pie:
    case element_icon::heatmap:
    case element_icon::tag:
    case element_icon::sankey:
      return IM_COL32(66, 133, 230, 255);
    case element_icon::diagram:
    case element_icon::sequence:
    case element_icon::tree:
    case element_icon::node:
    case element_icon::ellipse:
    case element_icon::actor:
    case element_icon::participant:
    case element_icon::group:
    case element_icon::note:
    case element_icon::shape:
      return IM_COL32(52, 168, 83, 255);
    case element_icon::link:
      return IM_COL32(236, 133, 30, 255);
    case element_icon::window:
    case element_icon::button:
    case element_icon::checkbox:
    case element_icon::radio:
    case element_icon::slider:
    case element_icon::input:
    case element_icon::menu:
    case element_icon::table:
    case element_icon::tabs:
    case element_icon::container:
      return IM_COL32(150, 96, 210, 255);
    default:
      return IM_COL32(128, 128, 128, 255);
  }
}

/// A dashed line from `a` to `b`, axis-aligned.
void dashed(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col, float dash, float thickness) {
  const ImVec2 d(b.x - a.x, b.y - a.y);
  const float length = std::max(std::abs(d.x), std::abs(d.y));
  for (float t = 0.0f; t < length; t += dash * 2.0f) {
    const float u = std::min(t + dash, length);
    dl->AddLine(
        ImVec2(a.x + d.x * t / length, a.y + d.y * t / length),
        ImVec2(a.x + d.x * u / length, a.y + d.y * u / length),
        col,
        thickness);
  }
}

} // namespace

element_icon icon_for(std::string_view type) {
  for (const icon_entry& e : kByName) {
    if (e.first == type) {
      return e.second;
    }
  }
  if (ends_with(type, "Diagram")) {
    return element_icon::diagram;
  }
  if (ends_with(type, "Link") || ends_with(type, "Relation") || ends_with(type, "Relationship") ||
      ends_with(type, "Transition") || ends_with(type, "Flow")) {
    return element_icon::link;
  }
  if (starts_with(type, "Slider") || starts_with(type, "VSlider") || starts_with(type, "Drag")) {
    return element_icon::slider;
  }
  if (starts_with(type, "Input") || starts_with(type, "ColorEdit") || starts_with(type, "ColorPicker")) {
    return element_icon::input;
  }
  return element_icon::generic;
}

void draw_element_icon(ImDrawList* dl, ImVec2 min, float size, std::string_view type) {
  const element_icon icon = icon_for(type);
  const ImU32 col = color_of(icon);
  const ImU32 soft = (col & 0x00FFFFFF) | (static_cast<ImU32>(90) << IM_COL32_A_SHIFT);
  const float s = size;
  const float w = std::max(1.0f, s / 12.0f); // line thickness
  // Point at fractions of the square.
  const auto p = [&](float fx, float fy) { return ImVec2(min.x + fx * s, min.y + fy * s); };
  switch (icon) {
    case element_icon::image:
      dl->AddRect(p(0.1f, 0.2f), p(0.9f, 0.8f), col, 0.0f, 0, w);
      dl->AddTriangleFilled(p(0.2f, 0.72f), p(0.42f, 0.4f), p(0.62f, 0.72f), col);
      dl->AddTriangleFilled(p(0.5f, 0.72f), p(0.65f, 0.52f), p(0.8f, 0.72f), soft);
      dl->AddCircleFilled(p(0.7f, 0.35f), s * 0.07f, col);
      break;
    case element_icon::layout_h:
      dl->AddRect(p(0.1f, 0.2f), p(0.46f, 0.8f), col, 0.0f, 0, w);
      dl->AddRect(p(0.54f, 0.2f), p(0.9f, 0.8f), col, 0.0f, 0, w);
      break;
    case element_icon::layout_v:
      dl->AddRect(p(0.15f, 0.1f), p(0.85f, 0.46f), col, 0.0f, 0, w);
      dl->AddRect(p(0.15f, 0.54f), p(0.85f, 0.9f), col, 0.0f, 0, w);
      break;
    case element_icon::spring: {
      const ImVec2 pts[] = {p(0.1f, 0.5f), p(0.22f, 0.5f), p(0.3f, 0.25f), p(0.42f, 0.75f), p(0.54f, 0.25f),
                            p(0.66f, 0.75f), p(0.74f, 0.5f), p(0.9f, 0.5f)};
      dl->AddPolyline(pts, 8, col, 0, w);
      break;
    }
    case element_icon::text:
      dl->AddLine(p(0.2f, 0.2f), p(0.8f, 0.2f), col, w * 2.0f);
      dl->AddLine(p(0.5f, 0.2f), p(0.5f, 0.85f), col, w * 2.0f);
      break;
    case element_icon::separator:
      dl->AddLine(p(0.1f, 0.5f), p(0.9f, 0.5f), col, w * 1.5f);
      dl->AddLine(p(0.25f, 0.3f), p(0.75f, 0.3f), soft, w);
      dl->AddLine(p(0.25f, 0.7f), p(0.75f, 0.7f), soft, w);
      break;
    case element_icon::plot: {
      dl->AddLine(p(0.15f, 0.1f), p(0.15f, 0.85f), col, w);
      dl->AddLine(p(0.15f, 0.85f), p(0.9f, 0.85f), col, w);
      const ImVec2 pts[] = {p(0.22f, 0.7f), p(0.42f, 0.45f), p(0.6f, 0.58f), p(0.85f, 0.2f)};
      dl->AddPolyline(pts, 4, col, 0, w * 1.5f);
      break;
    }
    case element_icon::plot3d:
      dl->AddLine(p(0.3f, 0.65f), p(0.3f, 0.1f), col, w);
      dl->AddLine(p(0.3f, 0.65f), p(0.9f, 0.65f), col, w);
      dl->AddLine(p(0.3f, 0.65f), p(0.08f, 0.9f), col, w);
      dl->AddQuadFilled(p(0.45f, 0.3f), p(0.75f, 0.3f), p(0.65f, 0.5f), p(0.35f, 0.5f), soft);
      break;
    case element_icon::line: {
      const ImVec2 pts[] = {p(0.1f, 0.75f), p(0.35f, 0.4f), p(0.6f, 0.6f), p(0.9f, 0.2f)};
      dl->AddPolyline(pts, 4, col, 0, w * 1.5f);
      break;
    }
    case element_icon::scatter:
      for (const ImVec2& c : {p(0.22f, 0.72f), p(0.4f, 0.45f), p(0.55f, 0.65f), p(0.72f, 0.3f), p(0.82f, 0.55f)}) {
        dl->AddCircleFilled(c, s * 0.08f, col);
      }
      break;
    case element_icon::bars:
      dl->AddRectFilled(p(0.12f, 0.5f), p(0.32f, 0.88f), col);
      dl->AddRectFilled(p(0.4f, 0.15f), p(0.6f, 0.88f), col);
      dl->AddRectFilled(p(0.68f, 0.35f), p(0.88f, 0.88f), col);
      break;
    case element_icon::bars_h:
      dl->AddRectFilled(p(0.1f, 0.12f), p(0.55f, 0.3f), col);
      dl->AddRectFilled(p(0.3f, 0.41f), p(0.9f, 0.59f), col);
      dl->AddRectFilled(p(0.2f, 0.7f), p(0.7f, 0.88f), col);
      break;
    case element_icon::pie:
      dl->AddCircle(p(0.5f, 0.5f), s * 0.38f, col, 0, w);
      dl->PathLineTo(p(0.5f, 0.5f));
      dl->PathArcTo(p(0.5f, 0.5f), s * 0.38f, -1.5708f, 0.3f, 10);
      dl->PathFillConvex(col);
      break;
    case element_icon::heatmap:
      dl->AddRectFilled(p(0.12f, 0.12f), p(0.5f, 0.5f), col);
      dl->AddRectFilled(p(0.5f, 0.12f), p(0.88f, 0.5f), soft);
      dl->AddRectFilled(p(0.12f, 0.5f), p(0.5f, 0.88f), soft);
      dl->AddRectFilled(p(0.5f, 0.5f), p(0.88f, 0.88f), col);
      break;
    case element_icon::tag:
      dl->AddQuadFilled(p(0.1f, 0.3f), p(0.65f, 0.3f), p(0.65f, 0.7f), p(0.1f, 0.7f), col);
      dl->AddTriangleFilled(p(0.65f, 0.3f), p(0.9f, 0.5f), p(0.65f, 0.7f), col);
      break;
    case element_icon::sankey:
      dl->AddRectFilled(p(0.1f, 0.15f), p(0.22f, 0.85f), col);
      dl->AddRectFilled(p(0.78f, 0.1f), p(0.9f, 0.45f), col);
      dl->AddRectFilled(p(0.78f, 0.55f), p(0.9f, 0.9f), col);
      dl->AddQuadFilled(p(0.22f, 0.15f), p(0.78f, 0.1f), p(0.78f, 0.45f), p(0.22f, 0.5f), soft);
      dl->AddQuadFilled(p(0.22f, 0.5f), p(0.78f, 0.55f), p(0.78f, 0.9f), p(0.22f, 0.85f), soft);
      break;
    case element_icon::diagram:
      dl->AddLine(p(0.3f, 0.3f), p(0.7f, 0.3f), col, w);
      dl->AddLine(p(0.3f, 0.3f), p(0.5f, 0.75f), col, w);
      dl->AddRectFilled(p(0.08f, 0.15f), p(0.38f, 0.42f), col, s * 0.05f);
      dl->AddRectFilled(p(0.62f, 0.15f), p(0.92f, 0.42f), col, s * 0.05f);
      dl->AddRectFilled(p(0.35f, 0.62f), p(0.65f, 0.89f), col, s * 0.05f);
      break;
    case element_icon::sequence:
      dl->AddRectFilled(p(0.08f, 0.1f), p(0.38f, 0.3f), col);
      dl->AddRectFilled(p(0.62f, 0.1f), p(0.92f, 0.3f), col);
      dashed(dl, p(0.23f, 0.3f), p(0.23f, 0.92f), col, s * 0.08f, w);
      dashed(dl, p(0.77f, 0.3f), p(0.77f, 0.92f), col, s * 0.08f, w);
      dl->AddLine(p(0.23f, 0.6f), p(0.72f, 0.6f), col, w);
      dl->AddTriangleFilled(p(0.77f, 0.6f), p(0.67f, 0.53f), p(0.67f, 0.67f), col);
      break;
    case element_icon::tree:
      dl->AddLine(p(0.5f, 0.3f), p(0.5f, 0.5f), col, w);
      dl->AddLine(p(0.2f, 0.5f), p(0.8f, 0.5f), col, w);
      dl->AddLine(p(0.2f, 0.5f), p(0.2f, 0.65f), col, w);
      dl->AddLine(p(0.8f, 0.5f), p(0.8f, 0.65f), col, w);
      dl->AddRectFilled(p(0.35f, 0.08f), p(0.65f, 0.3f), col);
      dl->AddRectFilled(p(0.05f, 0.65f), p(0.35f, 0.9f), col);
      dl->AddRectFilled(p(0.65f, 0.65f), p(0.95f, 0.9f), col);
      break;
    case element_icon::node:
      dl->AddRectFilled(p(0.1f, 0.22f), p(0.9f, 0.78f), soft, s * 0.1f);
      dl->AddRect(p(0.1f, 0.22f), p(0.9f, 0.78f), col, s * 0.1f, 0, w);
      break;
    case element_icon::ellipse: {
      constexpr int kPoints = 24;
      ImVec2 pts[kPoints];
      for (int i = 0; i < kPoints; ++i) {
        const float a = 6.2831853f * static_cast<float>(i) / kPoints;
        pts[i] = p(0.5f + 0.42f * std::cos(a), 0.5f + 0.26f * std::sin(a));
      }
      dl->AddConvexPolyFilled(pts, kPoints, soft);
      dl->AddPolyline(pts, kPoints, col, ImDrawFlags_Closed, w);
      break;
    }
    case element_icon::actor:
      dl->AddCircle(p(0.5f, 0.2f), s * 0.12f, col, 0, w);
      dl->AddLine(p(0.5f, 0.32f), p(0.5f, 0.62f), col, w);
      dl->AddLine(p(0.25f, 0.44f), p(0.75f, 0.44f), col, w);
      dl->AddLine(p(0.5f, 0.62f), p(0.3f, 0.9f), col, w);
      dl->AddLine(p(0.5f, 0.62f), p(0.7f, 0.9f), col, w);
      break;
    case element_icon::participant:
      dl->AddRectFilled(p(0.15f, 0.08f), p(0.85f, 0.38f), soft);
      dl->AddRect(p(0.15f, 0.08f), p(0.85f, 0.38f), col, 0.0f, 0, w);
      dashed(dl, p(0.5f, 0.38f), p(0.5f, 0.95f), col, s * 0.08f, w);
      break;
    case element_icon::link:
      dl->AddLine(p(0.1f, 0.5f), p(0.72f, 0.5f), col, w * 1.5f);
      dl->AddTriangleFilled(p(0.92f, 0.5f), p(0.66f, 0.32f), p(0.66f, 0.68f), col);
      break;
    case element_icon::group:
      dashed(dl, p(0.1f, 0.15f), p(0.9f, 0.15f), col, s * 0.09f, w);
      dashed(dl, p(0.1f, 0.85f), p(0.9f, 0.85f), col, s * 0.09f, w);
      dashed(dl, p(0.1f, 0.15f), p(0.1f, 0.85f), col, s * 0.09f, w);
      dashed(dl, p(0.9f, 0.15f), p(0.9f, 0.85f), col, s * 0.09f, w);
      dl->AddRectFilled(p(0.1f, 0.15f), p(0.45f, 0.35f), col);
      break;
    case element_icon::note: {
      const ImVec2 pts[] = {p(0.15f, 0.1f), p(0.65f, 0.1f), p(0.85f, 0.3f), p(0.85f, 0.9f), p(0.15f, 0.9f)};
      dl->AddConvexPolyFilled(pts, 5, soft);
      dl->AddPolyline(pts, 5, col, ImDrawFlags_Closed, w);
      dl->AddLine(p(0.65f, 0.1f), p(0.65f, 0.3f), col, w);
      dl->AddLine(p(0.65f, 0.3f), p(0.85f, 0.3f), col, w);
      break;
    }
    case element_icon::shape:
      dl->AddQuadFilled(p(0.5f, 0.1f), p(0.9f, 0.5f), p(0.5f, 0.9f), p(0.1f, 0.5f), soft);
      dl->AddQuad(p(0.5f, 0.1f), p(0.9f, 0.5f), p(0.5f, 0.9f), p(0.1f, 0.5f), col, w);
      break;
    case element_icon::window:
      dl->AddRect(p(0.08f, 0.15f), p(0.92f, 0.85f), col, s * 0.06f, 0, w);
      dl->AddRectFilled(p(0.08f, 0.15f), p(0.92f, 0.33f), col, s * 0.06f, ImDrawFlags_RoundCornersTop);
      break;
    case element_icon::button:
      dl->AddRectFilled(p(0.08f, 0.28f), p(0.92f, 0.72f), col, s * 0.12f);
      dl->AddLine(p(0.3f, 0.5f), p(0.7f, 0.5f), IM_COL32_WHITE, w);
      break;
    case element_icon::checkbox:
      dl->AddRect(p(0.15f, 0.15f), p(0.85f, 0.85f), col, s * 0.08f, 0, w);
      dl->AddLine(p(0.28f, 0.5f), p(0.45f, 0.68f), col, w * 1.8f);
      dl->AddLine(p(0.45f, 0.68f), p(0.74f, 0.3f), col, w * 1.8f);
      break;
    case element_icon::radio:
      dl->AddCircle(p(0.5f, 0.5f), s * 0.36f, col, 0, w);
      dl->AddCircleFilled(p(0.5f, 0.5f), s * 0.17f, col);
      break;
    case element_icon::slider:
      dl->AddLine(p(0.08f, 0.5f), p(0.92f, 0.5f), col, w * 1.5f);
      dl->AddRectFilled(p(0.52f, 0.25f), p(0.68f, 0.75f), col, s * 0.04f);
      break;
    case element_icon::input:
      dl->AddRect(p(0.06f, 0.28f), p(0.94f, 0.72f), col, s * 0.06f, 0, w);
      dl->AddLine(p(0.22f, 0.38f), p(0.22f, 0.62f), col, w);
      break;
    case element_icon::menu:
      for (const float y : {0.25f, 0.5f, 0.75f}) {
        dl->AddLine(p(0.15f, y), p(0.85f, y), col, w * 1.5f);
      }
      break;
    case element_icon::table:
      dl->AddRect(p(0.1f, 0.15f), p(0.9f, 0.85f), col, 0.0f, 0, w);
      dl->AddRectFilled(p(0.1f, 0.15f), p(0.9f, 0.38f), col);
      dl->AddLine(p(0.1f, 0.62f), p(0.9f, 0.62f), col, w);
      dl->AddLine(p(0.5f, 0.15f), p(0.5f, 0.85f), col, w);
      break;
    case element_icon::tabs:
      dl->AddRectFilled(p(0.1f, 0.2f), p(0.42f, 0.42f), col);
      dl->AddRect(p(0.46f, 0.2f), p(0.78f, 0.42f), col, 0.0f, 0, w);
      dl->AddRect(p(0.1f, 0.42f), p(0.9f, 0.85f), col, 0.0f, 0, w);
      break;
    case element_icon::container:
      dl->AddLine(p(0.3f, 0.12f), p(0.15f, 0.12f), col, w);
      dl->AddLine(p(0.15f, 0.12f), p(0.15f, 0.88f), col, w);
      dl->AddLine(p(0.15f, 0.88f), p(0.3f, 0.88f), col, w);
      dl->AddLine(p(0.7f, 0.12f), p(0.85f, 0.12f), col, w);
      dl->AddLine(p(0.85f, 0.12f), p(0.85f, 0.88f), col, w);
      dl->AddLine(p(0.85f, 0.88f), p(0.7f, 0.88f), col, w);
      break;
    case element_icon::generic:
      dl->AddRect(p(0.25f, 0.25f), p(0.75f, 0.75f), col, 0.0f, 0, w);
      break;
  }
}

} // namespace bdg::nymph
