// MIT License © 2026 Binary Dice Games
/// @file test_dependencies.cpp
/// @brief Plan step 1: the vendored dependencies build, link and work the way
///        later steps rely on (headless ImGui 1.92 frames with ImPlot and
///        ImPlot3D, the texture-request protocol, libyaml positions, stb).
// A private copy of the stb implementation for this test only:
// STB_IMAGE_WRITE_STATIC keeps it from clashing with nymph's own (step 4).
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <gtest/gtest.h>
#include <imgui.h>
#include <implot.h>
#include <implot3d.h>
#include <stb_image_write.h>
#include <yaml.h>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace {

// ── Headless ImGui context ───────────────────────────────────────────────────

/// Creates an ImGui + ImPlot + ImPlot3D context trio configured the way
/// nymph's frame driver will be: no backend, fixed display size, the 1.92
/// texture protocol, an Alpha8 atlas, no ini or log files.
class headless_frame : public ::testing::Test {
 protected:
  void SetUp() override {
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImPlot3D::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.DisplaySize = ImVec2(400.0f, 300.0f);
    io.DeltaTime = 1.0f / 60.0f;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures | ImGuiBackendFlags_RendererHasVtxOffset;
    io.Fonts->TexDesiredFormat = ImTextureFormat_Alpha8;
    io.Fonts->AddFontDefaultVector();
  }

  void TearDown() override {
    ImPlot3D::DestroyContext();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
  }

  /// Plays the renderer's part of the texture protocol: marks every pending
  /// request as done, as a backend would after uploading the pixels.
  static void acknowledge_textures(ImDrawData* draw_data) {
    if (draw_data->Textures == nullptr) {
      return;
    }
    for (ImTextureData* tex : *draw_data->Textures) {
      if (tex->Status == ImTextureStatus_WantCreate) {
        tex->SetTexID(static_cast<ImTextureID>(1));
        tex->SetStatus(ImTextureStatus_OK);
      } else if (tex->Status == ImTextureStatus_WantUpdates) {
        tex->SetStatus(ImTextureStatus_OK);
      } else if (tex->Status == ImTextureStatus_WantDestroy) {
        tex->SetTexID(ImTextureID_Invalid);
        tex->SetStatus(ImTextureStatus_Destroyed);
      }
    }
  }
};

// ── ImGui / ImPlot / ImPlot3D ────────────────────────────────────────────────

TEST_F(headless_frame, AtlasIsRequestedAsAlpha8AndTextAppearsOnTheSecondFrame) {
  ImGui::NewFrame();
  ImGui::Begin("w");
  ImGui::TextUnformatted("hello");
  ImGui::End();
  ImGui::Render();

  ImDrawData* draw_data = ImGui::GetDrawData();
  ASSERT_NE(draw_data, nullptr);
  ASSERT_NE(draw_data->Textures, nullptr);
  ASSERT_GE(draw_data->Textures->Size, 1);
  ImTextureData* atlas = (*draw_data->Textures)[0];
  EXPECT_EQ(atlas->Status, ImTextureStatus_WantCreate);
  EXPECT_EQ(atlas->Format, ImTextureFormat_Alpha8);
  EXPECT_EQ(atlas->BytesPerPixel, 1);
  EXPECT_GT(atlas->Width, 0);
  EXPECT_GT(atlas->Height, 0);
  EXPECT_NE(atlas->GetPixels(), nullptr);

  // A new auto-sized window is hidden on its first frame (ImGui measures it
  // first), one reason nymph renders several frames and keeps the last.
  acknowledge_textures(draw_data);
  ImGui::NewFrame();
  ImGui::Begin("w");
  ImGui::TextUnformatted("hello");
  ImGui::End();
  ImGui::Render();
  EXPECT_GT(ImGui::GetDrawData()->TotalVtxCount, 0);
  EXPECT_EQ(atlas->Status, ImTextureStatus_OK);
}

TEST_F(headless_frame, PlotAndPlot3DProduceGeometryOverSeveralFrames) {
  const float xs[] = {0.0f, 1.0f, 2.0f, 3.0f};
  const float ys[] = {1.0f, 3.0f, 2.0f, 4.0f};
  const float zs[] = {0.0f, 1.0f, 0.5f, 2.0f};

  for (int frame = 0; frame < 3; ++frame) {
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("figure", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    if (ImPlot::BeginPlot("2d", ImVec2(-1.0f, 140.0f))) {
      ImPlot::PlotLine("line", xs, ys, 4);
      ImPlot::EndPlot();
    }
    if (ImPlot3D::BeginPlot("3d", ImVec2(-1.0f, 140.0f))) {
      ImPlot3D::PlotLine("line3d", xs, ys, zs, 4);
      ImPlot3D::EndPlot();
    }
    ImGui::End();
    ImGui::Render();
    acknowledge_textures(ImGui::GetDrawData());
  }

  const ImDrawData* draw_data = ImGui::GetDrawData();
  ASSERT_NE(draw_data, nullptr);
  EXPECT_GE(draw_data->CmdListsCount, 1);
  EXPECT_GT(draw_data->TotalIdxCount, 0);
  EXPECT_FLOAT_EQ(draw_data->DisplaySize.x, 400.0f);
  EXPECT_FLOAT_EQ(draw_data->DisplaySize.y, 300.0f);
}

// ── libyaml ──────────────────────────────────────────────────────────────────

TEST(Dependencies, LibyamlParsesADocumentWithNodePositions) {
  const std::string text = "root:\n  type: Plot\n  xs: [1, 2]\n";
  yaml_parser_t parser;
  ASSERT_EQ(yaml_parser_initialize(&parser), 1);
  yaml_parser_set_input_string(&parser, reinterpret_cast<const unsigned char*>(text.data()), text.size());
  yaml_document_t doc;
  ASSERT_EQ(yaml_parser_load(&parser, &doc), 1);

  const yaml_node_t* root = yaml_document_get_root_node(&doc);
  ASSERT_NE(root, nullptr);
  ASSERT_EQ(root->type, YAML_MAPPING_NODE);
  const yaml_node_pair_t& pair = root->data.mapping.pairs.start[0];
  const yaml_node_t* inner = yaml_document_get_node(&doc, pair.value);
  ASSERT_EQ(inner->type, YAML_MAPPING_NODE);

  // Second pair of the inner mapping: "xs: [1, 2]", 0-based line 2.
  const yaml_node_pair_t& xs_pair = inner->data.mapping.pairs.start[1];
  const yaml_node_t* xs_key = yaml_document_get_node(&doc, xs_pair.key);
  const yaml_node_t* xs_value = yaml_document_get_node(&doc, xs_pair.value);
  ASSERT_EQ(xs_key->type, YAML_SCALAR_NODE);
  EXPECT_EQ(std::string(reinterpret_cast<const char*>(xs_key->data.scalar.value)), "xs");
  EXPECT_EQ(xs_key->start_mark.line, 2u);
  EXPECT_EQ(xs_key->start_mark.column, 2u);
  EXPECT_EQ(xs_value->type, YAML_SEQUENCE_NODE);
  EXPECT_EQ(xs_value->start_mark.column, 6u);

  yaml_document_delete(&doc);
  yaml_parser_delete(&parser);
}

// ── stb ──────────────────────────────────────────────────────────────────────

TEST(Dependencies, StbWritesAPngToMemory) {
  const uint8_t rgba[2 * 2 * 4] = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255};
  std::vector<uint8_t> png;
  auto append = [](void* context, void* data, int size) {
    auto* out = static_cast<std::vector<uint8_t>*>(context);
    const auto* bytes = static_cast<const uint8_t*>(data);
    out->insert(out->end(), bytes, bytes + size);
  };
  ASSERT_NE(stbi_write_png_to_func(append, &png, 2, 2, 4, rgba, 2 * 4), 0);
  const uint8_t signature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
  ASSERT_GE(png.size(), sizeof(signature));
  EXPECT_EQ(std::memcmp(png.data(), signature, sizeof(signature)), 0);
}

} // namespace
