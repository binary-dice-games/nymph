// MIT License © 2026 Binary Dice Games
/// @file test_imuml_context.cpp
/// @brief UML plan steps 2-3: the ImUml context, style, diagram scopes,
///        sizing and zoom, and the drawing primitives.
#include <gtest/gtest.h>
#include <stdexcept>
#include "imuml_internal.h"
#include "render/figure_renderer.hpp"

namespace bdg::nymph {
namespace {

image_options options(int w = 300, int h = 200) {
  image_options o;
  o.width = w;
  o.height = h;
  o.padding = 0;
  return o;
}

ImU32 pixel(const image& img, int x, int y) {
  const uint8_t* p = img.at(x, y);
  return IM_COL32(p[0], p[1], p[2], p[3]);
}

// ── Context ──────────────────────────────────────────────────────────────────

TEST(imuml_context, CreateDestroyAndRestore) {
  ImUmlContext* outer = ImUml::CreateContext();
  ImUml::SetCurrentContext(outer);
  render_frames(options(), [](draw_context&) { EXPECT_NE(ImUml::GetCurrentContext(), nullptr); });
  EXPECT_EQ(ImUml::GetCurrentContext(), outer);
  EXPECT_THROW(
      render_frames(options(), [](draw_context&) { throw std::runtime_error("boom"); }), std::runtime_error);
  EXPECT_EQ(ImUml::GetCurrentContext(), outer);
  ImUml::DestroyContext(outer);
  EXPECT_EQ(ImUml::GetCurrentContext(), nullptr);
}

TEST(imuml_context, AutoColoursFollowTheImGuiTheme) {
  for (const char* theme : {"light", "dark"}) {
    image_options o = options();
    o.theme = theme;
    render_frames(o, [](draw_context&) {
      ImUml::StyleColorsAuto();
      EXPECT_EQ(ImUml::GetColorU32(ImUmlCol_Text), ImGui::GetColorU32(ImGuiCol_Text));
      ImUml::PushStyleColor(ImUmlCol_Line, IM_COL32(255, 0, 0, 255));
      EXPECT_EQ(ImUml::GetColorU32(ImUmlCol_Line), IM_COL32(255, 0, 0, 255));
      ImUml::PopStyleColor();
      ImUml::PushStyleVar(ImUmlStyleVar_ArrowSize, 21.0f);
      EXPECT_EQ(ImUml::GetStyle().ArrowSize, 21.0f);
      ImUml::PopStyleVar();
      EXPECT_EQ(ImUml::GetStyle().ArrowSize, ImUmlStyle().ArrowSize);
    });
  }
}

TEST(imuml_context, ScopesReserveSpaceAndMeasureOnlyDoesNot) {
  render_frames(options(), [](draw_context&) {
    ImGui::SetCursorScreenPos(ImVec2(0, 0));
    ASSERT_TRUE(ImUml::BeginClassDiagram("##m", ImVec2(0, 0), ImUmlDiagramFlags_MeasureOnly));
    ImUml::Class("A");
    ImUml::EndClassDiagram();
    const ImUmlDiagramInfo measured = ImUml::GetLastDiagramInfo();
    EXPECT_EQ(measured.Size.x, 0.0f);
    EXPECT_GT(measured.NaturalSize.x, 0.0f);
    EXPECT_EQ(ImGui::GetCursorScreenPos().y, 0.0f); // nothing reserved

    ASSERT_TRUE(ImUml::BeginClassDiagram("Title", ImVec2(-1, 150)));
    ImUml::Class("A");
    ImUml::EndClassDiagram();
    const ImUmlDiagramInfo drawn = ImUml::GetLastDiagramInfo();
    EXPECT_EQ(drawn.Size.x, 299.0f); // -1: the available width minus 1, as ImGui's item sizes
    EXPECT_EQ(drawn.Size.y, 150.0f);
    EXPECT_GT(drawn.TitleHeight, 0.0f);
    EXPECT_EQ(drawn.NaturalSize.y, drawn.ContentSize.y + drawn.TitleHeight);
    EXPECT_GE(ImGui::GetCursorScreenPos().y, 150.0f);
    EXPECT_EQ(drawn.NodeCount, 1);
  });
}

TEST(imuml_context, ZoomFitsAndIsCappedByMaxZoom) {
  render_frames(options(600, 400), [](draw_context&) {
    auto zoom_for = [](ImVec2 size, float max_zoom) {
      ImUml::PushStyleVar(ImUmlStyleVar_MaxZoom, max_zoom);
      ImGui::SetCursorScreenPos(ImVec2(0, 0));
      ImUml::BeginClassDiagram("##z", size);
      ImUml::Class("A");
      ImUml::Class("B");
      ImUml::Relation("A", "B");
      ImUml::EndClassDiagram();
      ImUml::PopStyleVar();
      return ImUml::GetLastDiagramInfo();
    };
    const ImUmlDiagramInfo natural = zoom_for(ImVec2(0, 0), 1.0f);
    EXPECT_FLOAT_EQ(natural.Zoom, 1.0f);
    const ImVec2 c = natural.ContentSize;
    const ImUmlDiagramInfo half = zoom_for(ImVec2(c.x * 0.5f, c.y), 1.0f);
    EXPECT_NEAR(half.Zoom, 0.5f, 0.01f);
    EXPECT_FLOAT_EQ(zoom_for(ImVec2(c.x * 3, c.y * 3), 1.0f).Zoom, 1.0f);
    EXPECT_NEAR(zoom_for(ImVec2(c.x * 3, c.y * 3), 2.0f).Zoom, 2.0f, 0.01f);
  });
}

TEST(imuml_context, UnknownIdsAreSkippedAndCounted) {
  render_frames(options(), [](draw_context&) {
    ImUml::BeginClassDiagram("##s");
    ImUml::Class("A");
    ImUml::Relation("A", "Nope");
    ImUml::EndClassDiagram();
    EXPECT_EQ(ImUml::GetLastDiagramInfo().SkippedItems, 1);
  });
}

// ── Primitives ───────────────────────────────────────────────────────────────

const ImU32 kLine = IM_COL32(200, 0, 0, 255);
const ImU32 kBg = IM_COL32(0, 0, 200, 255);

image draw(const std::function<void(ImDrawList*)>& f) {
  return render_frames(options(), [&](draw_context&) { f(ImGui::GetWindowDrawList()); });
}

TEST(imuml_render, HollowMarkersKeepTheirInsideFilledOnesDoNot) {
  const image img = draw([](ImDrawList* dl) {
    ImUml::RenderMarker(dl, ImUmlMarker_Hollow, ImVec2(50, 20), ImVec2(0, -1), 30, kLine, kBg, 1);
    ImUml::RenderMarker(dl, ImUmlMarker_Filled, ImVec2(120, 20), ImVec2(0, -1), 30, kLine, kBg, 1);
    ImUml::RenderMarker(dl, ImUmlMarker_HollowDiamond, ImVec2(200, 10), ImVec2(0, -1), 30, kLine, kBg, 1);
    ImUml::RenderMarker(dl, ImUmlMarker_FilledDiamond, ImVec2(260, 10), ImVec2(0, -1), 30, kLine, kBg, 1);
  });
  EXPECT_EQ(pixel(img, 50, 40), kBg); // inside the hollow triangle
  EXPECT_EQ(pixel(img, 120, 40), kLine);
  EXPECT_EQ(pixel(img, 200, 34), kBg);
  EXPECT_EQ(pixel(img, 260, 34), kLine);
}

TEST(imuml_render, DashedLinesHaveGapsSolidOnesDoNot) {
  const image img = draw([](ImDrawList* dl) {
    const ImVec2 solid[2] = {ImVec2(10, 50.5f), ImVec2(290, 50.5f)};
    const ImVec2 dashed[2] = {ImVec2(10, 100.5f), ImVec2(290, 100.5f)};
    ImUml::RenderPath(dl, solid, 2, kLine, 1, false, 5, 3);
    ImUml::RenderPath(dl, dashed, 2, kLine, 1, true, 5, 3);
  });
  int solid_on = 0, dashed_on = 0, runs = 0;
  bool prev = false;
  for (int x = 12; x < 288; ++x) {
    solid_on += pixel(img, x, 50) == kLine ? 1 : 0;
    const bool on = pixel(img, x, 100) == kLine;
    dashed_on += on ? 1 : 0;
    runs += on && !prev ? 1 : 0;
    prev = on;
  }
  EXPECT_EQ(solid_on, 276);
  EXPECT_NEAR(dashed_on, 276 * 5 / 8, 20);
  EXPECT_NEAR(runs, 276 / 8, 3);
}

TEST(imuml_render, ShapesStayInsideTheirRectangle) {
  const image img = draw([](ImDrawList* dl) {
    ImUml::RenderNoteShape(dl, ImVec2(20, 20), ImVec2(120, 80), 10, kBg, kLine, 1);
    ImUml::RenderActorFigure(dl, ImVec2(150, 20), ImVec2(180, 80), kLine, 1);
    ImUml::RenderCylinder(dl, ImVec2(200, 20), ImVec2(260, 80), kBg, kLine, 1);
    ImUml::RenderDiamond(dl, ImVec2(60, 150), ImVec2(30, 20), kBg, kLine, 1);
  });
  for (int y = 0; y < img.height; ++y) {
    for (int x = 0; x < img.width; ++x) {
      const ImU32 p = pixel(img, x, y);
      if (p == pixel(img, 0, 0)) {
        continue;
      }
      const bool inside = (x >= 19 && x <= 121 && y >= 19 && y <= 81) || (x >= 149 && x <= 181 && y >= 19 && y <= 81) ||
                          (x >= 199 && x <= 261 && y >= 19 && y <= 81) || (x >= 29 && x <= 91 && y >= 129 && y <= 171);
      EXPECT_TRUE(inside) << "pixel at " << x << ", " << y;
      if (!inside) {
        return;
      }
    }
  }
  EXPECT_EQ(pixel(img, 60, 150), kBg); // inside of the diamond
}

TEST(imuml_render, DrawingIsDeterministic) {
  auto scene = [](ImDrawList* dl) {
    const ImVec2 pts[4] = {ImVec2(10, 10), ImVec2(100, 10), ImVec2(100, 150), ImVec2(250, 150)};
    ImVector<ImVec2> flat;
    ImUml::FlattenPath(pts, 4, true, &flat);
    ImUml::RenderPath(dl, flat.Data, flat.Size, kLine, 1.5f, true, 6, 3);
    ImUml::RenderMarker(dl, ImUmlMarker_Open, ImVec2(250, 150), ImVec2(1, 0), 12, kLine, kBg, 1.5f);
  };
  const image a = draw(scene);
  const image b = draw(scene);
  EXPECT_EQ(a.rgba, b.rgba);
}

} // namespace
} // namespace bdg::nymph
