// MIT License © 2026 Binary Dice Games
/// @file test_imuml_interactive.cpp
/// @brief spec/04_web_viewer plan step 7: ImUmlDiagramFlags_Interactive
///        (wheel zoom around the mouse, drag pan, double-click fit, hovered
///        node or edge), driven with real ImGui mouse events.
#include <gtest/gtest.h>
#include <cfloat>
#include <optional>
#include "imuml_internal.h"
#include "render/draw_context.hpp"
#include "render/frame_setup.hpp"

namespace bdg::nymph {
namespace {

/// Runs frames of one class diagram (A above B, B -> A) filling 400 x 300,
/// with mouse input.
class diagram_driver {
 public:
  explicit diagram_driver(ImUmlDiagramFlags flags = ImUmlDiagramFlags_Interactive) : flags_(flags), dc_(options_) {
    options_.width = 400;
    options_.height = 300;
    options_.padding = 0;
    const context_switch on(contexts_);
    configure(*find_theme("light"), "test", ImGuiBackendFlags_RendererHasTextures);
    load_fonts(dc_);
    frame();
  }

  /// One frame; the events go in first.
  ImUmlDiagramInfo frame(std::optional<ImVec2> mouse = std::nullopt, int button_down = -1, float wheel = 0.0f) {
    const context_switch on(contexts_);
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(400, 300);
    io.DeltaTime = 1.0f / 60.0f;
    if (mouse) {
      io.AddMousePosEvent(mouse->x, mouse->y);
    }
    if (button_down >= 0) {
      io.AddMouseButtonEvent(0, button_down == 1);
    }
    if (wheel != 0.0f) {
      io.AddMouseWheelEvent(0.0f, wheel);
    }
    ImUmlDiagramInfo info;
    run_figure_frame(options_, dc_, [&](draw_context&) {
      ImGui::SetCursorScreenPos(ImVec2(0, 0));
      if (ImUml::BeginClassDiagram("##d", ImVec2(400, 300), flags_)) {
        ImUml::Class("A");
        ImUml::Class("B");
        ImUml::Relation("B", "A", ImUmlRelationKind_Inheritance);
        ImUml::EndClassDiagram();
      }
      info = ImUml::GetLastDiagramInfo();
    });
    return info;
  }

  /// Settles a mouse position over three frames (hover lags a frame).
  ImUmlDiagramInfo hover(ImVec2 p) {
    frame(p);
    frame(p);
    return frame(p);
  }

  /// The screen rectangle of node `id`, found by sweeping the diagram.
  std::optional<ImRect> find_node(const char* id) {
    for (float y = 2; y < 300; y += 4) {
      for (float x = 2; x < 400; x += 4) {
        const ImUmlDiagramInfo info = hover(ImVec2(x, y));
        if (info.HoveredNode >= 0 && info.HoveredNodeId == ImHashStr(id)) {
          return ImRect(info.HoveredMin, info.HoveredMax);
        }
      }
    }
    return std::nullopt;
  }

 private:
  ImUmlDiagramFlags flags_;
  image_options options_;
  imgui_contexts contexts_;
  draw_context dc_;
};

TEST(imuml_interactive, WithoutTheFlagNothingIsHoveredOrMoved) {
  diagram_driver d(ImUmlDiagramFlags_None);
  for (float y = 10; y < 300; y += 20) {
    const ImUmlDiagramInfo info = d.hover(ImVec2(200, y));
    EXPECT_EQ(info.HoveredNode, -1);
    EXPECT_EQ(info.HoveredEdge, -1);
  }
  const float zoom = d.frame().Zoom;
  d.frame(ImVec2(200, 150), -1, 3.0f);
  EXPECT_EQ(d.frame().Zoom, zoom);
}

TEST(imuml_interactive, TheNodeUnderTheMouseIsReported) {
  diagram_driver d;
  const std::optional<ImRect> a = d.find_node("A");
  const std::optional<ImRect> b = d.find_node("B");
  ASSERT_TRUE(a && b);
  EXPECT_LT(a->Max.y, b->Min.y); // the parent above its child
  const ImUmlDiagramInfo info = d.hover(b->GetCenter());
  EXPECT_EQ(info.HoveredNode, 1);
  EXPECT_EQ(info.HoveredNodeId, ImHashStr("B"));
}

TEST(imuml_interactive, TheEdgeBetweenTwoNodesIsReported) {
  diagram_driver d;
  const std::optional<ImRect> a = d.find_node("A");
  const std::optional<ImRect> b = d.find_node("B");
  ASSERT_TRUE(a && b);
  bool found = false;
  // Between the two boxes, across the width: the edge crosses this band.
  const float y = (a->Max.y + b->Min.y) * 0.5f;
  for (float x = 2; x < 400 && !found; x += 2) {
    const ImUmlDiagramInfo info = d.hover(ImVec2(x, y));
    if (info.HoveredEdge >= 0) {
      found = true;
      EXPECT_EQ(info.HoveredEdge, 0);
      EXPECT_EQ(info.HoveredEdgeFrom, ImHashStr("B"));
      EXPECT_EQ(info.HoveredEdgeTo, ImHashStr("A"));
    }
  }
  EXPECT_TRUE(found);
}

TEST(imuml_interactive, TheWheelZoomsAroundTheMouse) {
  diagram_driver d;
  const std::optional<ImRect> a = d.find_node("A");
  ASSERT_TRUE(a);
  const ImVec2 m(a->Min.x + 3, a->Min.y + 3); // near A's top-left corner
  const float before = d.hover(m).Zoom;
  d.frame(m, -1, 1.0f);
  const ImUmlDiagramInfo after = d.hover(m);
  EXPECT_NEAR(after.ViewZoom, 1.2f, 1e-5f);
  EXPECT_NEAR(after.Zoom, before * 1.2f, 1e-4f);
  // A grew about the mouse: its corner moved away from m by the zoom.
  ASSERT_EQ(after.HoveredNodeId, ImHashStr("A"));
  EXPECT_NEAR(after.HoveredMin.x, m.x - 3 * 1.2f, 1.5f);
  EXPECT_NEAR(after.HoveredMin.y, m.y - 3 * 1.2f, 1.5f);
  EXPECT_NEAR(after.HoveredMax.x - after.HoveredMin.x, a->GetWidth() * 1.2f, 1.5f);
}

TEST(imuml_interactive, TheZoomIsClamped) {
  diagram_driver d;
  d.frame(ImVec2(200, 150), -1, 50.0f);
  EXPECT_FLOAT_EQ(d.frame().ViewZoom, 8.0f);
  d.frame(ImVec2(200, 150), -1, -100.0f);
  EXPECT_FLOAT_EQ(d.frame().ViewZoom, 0.25f);
}

TEST(imuml_interactive, DraggingPansAndADoubleClickFitsAgain) {
  diagram_driver d;
  const std::optional<ImRect> a = d.find_node("A");
  ASSERT_TRUE(a);
  const ImVec2 start(a->GetCenter().x, a->GetCenter().y);
  d.frame(start);
  d.frame(start, 1);
  for (int k = 1; k <= 4; ++k) {
    d.frame(ImVec2(start.x + 10.0f * k, start.y + 5.0f * k));
  }
  d.frame(ImVec2(start.x + 40, start.y + 20), 0);
  const ImUmlDiagramInfo moved = d.hover(ImVec2(start.x + 40, start.y + 20));
  ASSERT_EQ(moved.HoveredNodeId, ImHashStr("A"));
  EXPECT_NEAR(moved.HoveredMin.x, a->Min.x + 40, 1.0f);
  EXPECT_NEAR(moved.HoveredMin.y, a->Min.y + 20, 1.0f);
  // A double-click (two clicks within ImGui's double-click time).
  const ImVec2 p(start.x + 40, start.y + 20);
  d.frame(p, 1);
  d.frame(p, 0);
  d.frame(p, 1);
  d.frame(p, 0);
  const ImUmlDiagramInfo fitted = d.hover(start);
  EXPECT_FLOAT_EQ(fitted.ViewZoom, 1.0f);
  ASSERT_EQ(fitted.HoveredNodeId, ImHashStr("A"));
  EXPECT_NEAR(fitted.HoveredMin.x, a->Min.x, 0.5f);
}

} // namespace
} // namespace bdg::nymph
