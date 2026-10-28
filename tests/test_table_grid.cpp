// MIT License © 2026 Binary Dice Games
/// @file test_table_grid.cpp
/// @brief spec/14_editor_workspace plan step 7: the table grid's commits,
///        CSV import and export, the file dialog's entries, and a long table
///        drawn headless with only its visible rows.
#include <gtest/gtest.h>
#include <imgui.h>
#include <filesystem>
#include <fstream>
#include <string>
#include "edit/editor_model.hpp"
#include "edit/inspector.hpp"
#include "edit/table_grid.hpp"
#include "render/draw_context.hpp"
#include "render/frame_setup.hpp"

namespace bdg::nymph {
namespace {

const char* const kData =
    "root:\n"
    "  type: Plot\n"
    "  children:\n"
    "    - {type: PlotLine, label: r, xs: $x, ys: $y}\n"
    "data: |\n"
    "  x, y\n"
    "  1, 5\n"
    "  2, 6\n";

std::filesystem::path test_dir() {
  const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
  const std::filesystem::path dir =
      std::filesystem::temp_directory_path() / (std::string("nymph_grid_") + info->test_suite_name() + "_" + info->name());
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  return dir;
}

TEST(table_grid, ACellCommitSetsTheCellAndAHeaderCommitRenamesTheColumn) {
  editor_model m(kData);
  table_grid g;
  ASSERT_TRUE(g.commit_cell(m, "", 1, 1, "9").ok);
  EXPECT_EQ(m.tables()[0].cells.rows[1][1], "9");
  ASSERT_TRUE(g.commit_cell(m, "", -1, 1, "sales").ok);
  EXPECT_EQ(m.tables()[0].cells.headers[1], "sales");
  EXPECT_NE(m.source().find("ys: $sales"), std::string::npos);
  EXPECT_FALSE(g.commit_cell(m, "", 0, 1, "not a number").ok); // the line plots it
}

TEST(table_grid, ImportsAndExportsCsvFiles) {
  const std::filesystem::path dir = test_dir();
  std::ofstream(dir / "in.csv") << "x, y\n1, 10\n2, 20\n3, 30\n";
  editor_model m(kData);
  table_grid g;
  ASSERT_TRUE(g.import_file(m, "", dir / "in.csv").ok);
  EXPECT_EQ(m.tables()[0].cells.rows.size(), 3u);
  EXPECT_FALSE(g.import_file(m, "", dir / "missing.csv").ok);
  EXPECT_EQ(g.export_file(m, "", dir / "out.csv"), "");
  std::ifstream in(dir / "out.csv");
  EXPECT_EQ(std::string(std::istreambuf_iterator<char>(in), {}), "x, y\n1, 10\n2, 20\n3, 30\n");
  EXPECT_NE(g.export_file(m, "nope", dir / "x.csv"), "");
  std::filesystem::remove_all(dir);
}

TEST(table_grid, TheFileDialogListsFoldersThenCsvFiles) {
  const std::filesystem::path dir = test_dir();
  std::filesystem::create_directories(dir / "sub");
  std::filesystem::create_directories(dir / ".hidden");
  std::ofstream(dir / "b.csv") << "a\n";
  std::ofstream(dir / "A.CSV") << "a\n";
  std::ofstream(dir / "notes.txt") << "a\n";
  EXPECT_EQ(table_grid::folder_entries(dir), (std::vector<std::string>{"sub/", "A.CSV", "b.csv"}));
  std::filesystem::remove_all(dir);
}

/// An ImGui context that runs frames without a rendering backend.
class headless_imgui {
 public:
  headless_imgui() {
    const context_switch on(contexts_);
    configure(*find_theme("light"), "test", ImGuiBackendFlags_RendererHasTextures);
    load_fonts(dc_);
  }
  template <typename Fn>
  void frame(Fn f) {
    const context_switch on(contexts_);
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(600, 400);
    io.DeltaTime = 1.0f / 60.0f;
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(600, 400));
    ImGui::Begin("table");
    f();
    ImGui::End();
    ImGui::Render();
    vertices_ = ImGui::GetDrawData()->TotalVtxCount;
  }
  int vertices() const {
    return vertices_;
  }

 private:
  imgui_contexts contexts_;
  image_options options_;
  draw_context dc_{options_};
  int vertices_ = 0;
};

TEST(table_grid, ALongTableDrawsOnlyItsVisibleRows) {
  std::string source = "root: {type: Label, text: hi}\ndata: |\n  n, square\n";
  for (int i = 0; i < 10000; ++i) {
    source += "  " + std::to_string(i) + ", " + std::to_string(i * i) + "\n";
  }
  editor_model m(source);
  table_grid g;
  headless_imgui ui;
  inspector_events events;
  for (int i = 0; i < 3; ++i) {
    ui.frame([&] { g.draw(m, "", events); });
  }
  EXPECT_FALSE(events.source);
  // A few dozen rows of two cells, not ten thousand.
  EXPECT_LT(ui.vertices(), 40000);
  EXPECT_GT(ui.vertices(), 100);
}

TEST(table_grid, TheDialogsDrawHeadless) {
  editor_model m(kData);
  table_grid g;
  g.set_folder(test_dir());
  headless_imgui ui;
  inspector_events events;
  g.ask_export("");
  bool open = false;
  for (int i = 0; i < 3; ++i) {
    ui.frame([&] {
      g.draw(m, "", events);
      g.dialogs(m, events);
      open = open || ImGui::IsPopupOpen("Export CSV");
    });
  }
  EXPECT_TRUE(open);
}

} // namespace
} // namespace bdg::nymph
