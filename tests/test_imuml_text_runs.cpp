// MIT License © 2026 Binary Dice Games
/// @file test_imuml_text_runs.cpp
/// @brief spec/08_view_editor plan step 6: ImUmlDiagramFlags_CaptureText
///        reports every text a diagram draws with the item it belongs to,
///        and changes nothing that is drawn.
#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "imuml_internal.h"
#include "render/draw_context.hpp"
#include "render/frame_setup.hpp"

namespace bdg::nymph {
namespace {

/// One frame of a small flowchart (A -> B labelled "go", then a note-free
/// sequence below is not drawn).
struct captured {
  std::vector<ImUmlTextRun> runs;
  std::vector<std::string> texts;
  int draw_commands = 0;
  int vertices = 0;
  int nodes = 0, edges = 0, events = 0;
  int before_nodes = 0;
};

captured draw_flowchart(ImUmlDiagramFlags flags) {
  image_options options;
  options.width = 400;
  options.height = 300;
  options.padding = 0;
  draw_context dc(options);
  imgui_contexts contexts;
  const context_switch on(contexts);
  configure(*find_theme("light"), "test", ImGuiBackendFlags_RendererHasTextures);
  load_fonts(dc);
  captured out;
  for (int frame = 0; frame < 3; ++frame) { // the first frames settle ImGui
    ImGui::GetIO().DisplaySize = ImVec2(400, 300);
    ImGui::GetIO().DeltaTime = 1.0f / 60.0f;
    run_figure_frame(options, dc, [&](draw_context&) {
      ImGui::SetCursorScreenPos(ImVec2(0, 0));
      if (ImUml::BeginFlowchart("Flow##d", ImVec2(400, 300), flags)) {
        ImUml::GetItemCounts(&out.before_nodes, &out.edges, &out.events);
        ImUml::FlowNode("a", "Alpha", ImUmlFlowShape_Box);
        ImUml::FlowNode("b", "Beta\nsecond line", ImUmlFlowShape_Box);
        ImUml::FlowLink("a", "b", "go");
        ImUml::GetItemCounts(&out.nodes, &out.edges, &out.events);
        ImUml::EndFlowchart();
      }
    });
    const ImUmlDiagramInfo& info = ImUml::GetLastDiagramInfo();
    out.runs.assign(info.TextRuns, info.TextRuns + info.TextRunCount);
    out.texts.clear();
    for (const ImUmlTextRun& r : out.runs) {
      out.texts.push_back(r.Text);
    }
    out.draw_commands = 0;
    out.vertices = 0;
    const ImDrawData* data = ImGui::GetDrawData();
    for (int i = 0; i < data->CmdListsCount; ++i) {
      out.draw_commands += data->CmdLists[i]->CmdBuffer.Size;
      out.vertices += data->CmdLists[i]->VtxBuffer.Size;
    }
  }
  return out;
}

TEST(imuml_text_runs, AreOnlyReportedWithTheFlag) {
  const captured plain = draw_flowchart(ImUmlDiagramFlags_None);
  EXPECT_TRUE(plain.runs.empty());
  const captured c = draw_flowchart(ImUmlDiagramFlags_CaptureText);
  EXPECT_FALSE(c.runs.empty());
}

TEST(imuml_text_runs, ReportEachDrawnTextWithItsItem) {
  const captured c = draw_flowchart(ImUmlDiagramFlags_CaptureText);
  ASSERT_EQ(c.runs.size(), 4u);
  // The title comes first; edges are drawn before nodes.
  const auto find = [&](int kind, int item) -> int {
    for (size_t i = 0; i < c.runs.size(); ++i) {
      if (c.runs[i].Kind == kind && c.runs[i].Item == item) {
        return static_cast<int>(i);
      }
    }
    return -1;
  };
  EXPECT_EQ(c.runs[0].Kind, ImUmlTextRunKind_Title);
  EXPECT_EQ(c.texts[0], "Flow");
  const int alpha = find(ImUmlTextRunKind_Node, 0), beta = find(ImUmlTextRunKind_Node, 1);
  const int go = find(ImUmlTextRunKind_Edge, 0);
  ASSERT_GE(alpha, 0);
  ASSERT_GE(beta, 0);
  ASSERT_GE(go, 0);
  EXPECT_EQ(c.texts[static_cast<size_t>(alpha)], "Alpha");
  EXPECT_EQ(c.texts[static_cast<size_t>(beta)], "Beta\nsecond line");
  EXPECT_EQ(c.texts[static_cast<size_t>(go)], "go");
  for (const ImUmlTextRun& r : c.runs) {
    EXPECT_LT(r.Min.x, r.Max.x);
    EXPECT_LT(r.Min.y, r.Max.y);
    EXPECT_GE(r.Min.x, 0.0f);
    EXPECT_LE(r.Max.x, 400.0f);
  }
  // The two-line node text is taller than the one-line one.
  const ImUmlTextRun& one = c.runs[static_cast<size_t>(alpha)];
  const ImUmlTextRun& two = c.runs[static_cast<size_t>(beta)];
  EXPECT_GT(two.Max.y - two.Min.y, 1.5f * (one.Max.y - one.Min.y));
}

TEST(imuml_text_runs, DrawingIsUnchangedByCapturing) {
  const captured plain = draw_flowchart(ImUmlDiagramFlags_None);
  const captured c = draw_flowchart(ImUmlDiagramFlags_CaptureText);
  EXPECT_EQ(plain.draw_commands, c.draw_commands);
  EXPECT_EQ(plain.vertices, c.vertices);
}

TEST(imuml_text_runs, ItemCountsSayWhatWasSubmitted) {
  const captured c = draw_flowchart(ImUmlDiagramFlags_None);
  EXPECT_EQ(c.before_nodes, 0);
  EXPECT_EQ(c.nodes, 2);
  EXPECT_EQ(c.edges, 1);
  EXPECT_EQ(c.events, 0);
  int n = -1, e = -1, v = -1;
  EXPECT_FALSE(ImUml::GetItemCounts(&n, &e, &v)); // no diagram open here
  EXPECT_EQ(n, -1);
}

} // namespace
} // namespace bdg::nymph
