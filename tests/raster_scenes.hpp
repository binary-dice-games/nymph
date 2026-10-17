// MIT License © 2026 Binary Dice Games
/// @file raster_scenes.hpp
/// @brief Hand-built ImGui draw data shared by the software and OpenGL
///        rasterizer tests: a builder, test textures, and a list of scenes
///        that covers every rule of DESIGN.md's rasterizer section.
#pragma once

#include <imgui.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "render/image.hpp"
#include "render/soft_rasterizer.hpp"
#include "render/texture_store.hpp"

namespace bdg::nymph {
namespace {

using rgba = std::array<uint8_t, 4>;

rgba pixel(const image& img, int x, int y) {
  const uint8_t* p = img.at(x, y);
  return {p[0], p[1], p[2], p[3]};
}

// ── Draw-data builder ────────────────────────────────────────────────────────

/// Builds ImDrawLists by hand. Each `command()` starts a new draw command;
/// vertex indices returned by `vtx()` are relative to that command's
/// VtxOffset, as ImGui's own are.
class draw_builder {
 public:
  draw_builder(float width, float height) {
    data_.DisplayPos = ImVec2(0.0f, 0.0f);
    data_.DisplaySize = ImVec2(width, height);
    data_.FramebufferScale = ImVec2(1.0f, 1.0f);
    new_list();
  }

  /// Starts a new ImDrawList (a second "window").
  void new_list() {
    lists_.push_back(std::make_unique<ImDrawList>(nullptr));
  }

  /// Starts a draw command on the current list. `with_vtx_offset` makes its
  /// vertices start at VtxOffset = current vertex count.
  void command(ImTextureID tex, ImVec4 clip = ImVec4(-1e6f, -1e6f, 1e6f, 1e6f), bool with_vtx_offset = false) {
    ImDrawList& l = list();
    ImDrawCmd cmd;
    cmd.ClipRect = clip;
    cmd.TexRef = ImTextureRef(tex);
    cmd.IdxOffset = static_cast<unsigned int>(l.IdxBuffer.Size);
    cmd.VtxOffset = with_vtx_offset ? static_cast<unsigned int>(l.VtxBuffer.Size) : 0;
    cmd.ElemCount = 0;
    l.CmdBuffer.push_back(cmd);
  }

  ImDrawIdx vtx(ImVec2 pos, ImVec2 uv, ImU32 col) {
    ImDrawList& l = list();
    ImDrawVert v;
    v.pos = pos;
    v.uv = uv;
    v.col = col;
    l.VtxBuffer.push_back(v);
    return static_cast<ImDrawIdx>(l.VtxBuffer.Size - 1 - static_cast<int>(l.CmdBuffer.back().VtxOffset));
  }

  void tri(ImDrawIdx a, ImDrawIdx b, ImDrawIdx c) {
    ImDrawList& l = list();
    l.IdxBuffer.push_back(a);
    l.IdxBuffer.push_back(b);
    l.IdxBuffer.push_back(c);
    l.CmdBuffer.back().ElemCount += 3;
  }

  /// A rectangle in ImGui's PrimRect / PrimRectUV order: (a, b, c), (a, c, d).
  void rect(ImVec2 min, ImVec2 max, ImVec2 uv_min, ImVec2 uv_max, ImU32 col) {
    const ImDrawIdx a = vtx(min, uv_min, col);
    const ImDrawIdx b = vtx(ImVec2(max.x, min.y), ImVec2(uv_max.x, uv_min.y), col);
    const ImDrawIdx c = vtx(max, uv_max, col);
    const ImDrawIdx d = vtx(ImVec2(min.x, max.y), ImVec2(uv_min.x, uv_max.y), col);
    tri(a, b, c);
    tri(a, c, d);
  }

  /// A rectangle with one UV for every corner (a solid fill).
  void solid(ImVec2 min, ImVec2 max, ImVec2 uv, ImU32 col) {
    rect(min, max, uv, uv, col);
  }

  void add_callback(ImDrawCallback cb) {
    ImDrawCmd cmd;
    cmd.UserCallback = cb;
    list().CmdBuffer.push_back(cmd);
  }

  void set_framebuffer_scale(float s) {
    data_.FramebufferScale = ImVec2(s, s);
  }

  const ImDrawData& data() {
    data_.CmdLists.clear();
    data_.TotalVtxCount = data_.TotalIdxCount = 0;
    for (auto& l : lists_) {
      data_.CmdLists.push_back(l.get());
      data_.TotalVtxCount += l->VtxBuffer.Size;
      data_.TotalIdxCount += l->IdxBuffer.Size;
    }
    data_.CmdListsCount = data_.CmdLists.Size;
    data_.Valid = true;
    return data_;
  }

 private:
  ImDrawList& list() {
    return *lists_.back();
  }

  std::vector<std::unique_ptr<ImDrawList>> lists_;
  ImDrawData data_;
};

// ── Fixtures ─────────────────────────────────────────────────────────────────

constexpr ImU32 kBlack = IM_COL32(0, 0, 0, 255);
constexpr ImU32 kRed = IM_COL32(255, 0, 0, 255);

/// A 4x4 Alpha8 texture whose only opaque texel, the "white pixel", is at
/// (2, 2), not at (0, 0) as pre-1.92 atlases had it.
texture moved_white_pixel_texture() {
  texture t;
  t.format = ImTextureFormat_Alpha8;
  t.width = 4;
  t.height = 4;
  t.pixels.assign(16, 0);
  t.pixels[2 * 4 + 2] = 255;
  return t;
}

/// UV of the centre of texel (x, y) in a w x h texture.
ImVec2 texel_center(int x, int y, int w, int h) {
  return ImVec2((x + 0.5f) / w, (y + 0.5f) / h);
}

/// A 4x4 RGBA32 texture where texel (x, y) = (10 + 40x, 10 + 40y, 7, 255).
texture gradient_texture() {
  texture t;
  t.format = ImTextureFormat_RGBA32;
  t.width = 4;
  t.height = 4;
  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      t.pixels.insert(t.pixels.end(), {static_cast<uint8_t>(10 + 40 * x), static_cast<uint8_t>(10 + 40 * y), 7, 255});
    }
  }
  return t;
}

// ── Scenes ───────────────────────────────────────────────────────────────────

/// One draw-data scene with its textures and clear colour.
struct raster_scene {
  std::string name;
  std::unique_ptr<draw_builder> builder;
  texture_store store;
  ImU32 clear = kBlack;
  const ImDrawData& data() const {
    return builder->data();
  }
};

/// Every scene of test_soft_rasterizer.cpp, built the same way.
inline std::vector<raster_scene> raster_scenes() {
  std::vector<raster_scene> scenes;
  auto add = [&](std::string name, float w, float h) -> raster_scene& {
    raster_scene s;
    s.name = std::move(name);
    s.builder = std::make_unique<draw_builder>(w, h);
    scenes.push_back(std::move(s));
    return scenes.back();
  };
  const ImVec2 white_uv = texel_center(2, 2, 4, 4);
  const ImU32 half_red = IM_COL32(255, 0, 0, 128);
  {
    raster_scene& s = add("rectangle", 10, 10);
    const ImTextureID white = s.store.add(moved_white_pixel_texture());
    s.builder->command(white);
    s.builder->solid(ImVec2(2, 3), ImVec2(6, 8), white_uv, kRed);
  }
  {
    raster_scene& s = add("shared_diagonal", 8, 8);
    draw_builder& b = *s.builder;
    b.command(s.store.add(moved_white_pixel_texture()));
    const ImDrawIdx a = b.vtx(ImVec2(1, 1), white_uv, half_red);
    const ImDrawIdx c1 = b.vtx(ImVec2(7, 1), white_uv, half_red);
    const ImDrawIdx c2 = b.vtx(ImVec2(7, 7), white_uv, half_red);
    const ImDrawIdx d = b.vtx(ImVec2(1, 7), white_uv, half_red);
    b.tri(a, c1, d);
    b.tri(c1, c2, d);
  }
  {
    raster_scene& s = add("triangle_fan", 16, 16);
    draw_builder& b = *s.builder;
    b.command(s.store.add(moved_white_pixel_texture()));
    const ImVec2 centre(8.3f, 7.6f);
    const ImDrawIdx hub = b.vtx(centre, white_uv, half_red);
    ImDrawIdx rim[8];
    for (int i = 0; i < 8; ++i) {
      const float angle = 0.4f + i * 3.14159265f / 4.0f;
      rim[i] = b.vtx(ImVec2(centre.x + 6.1f * std::cos(angle), centre.y + 6.1f * std::sin(angle)), white_uv, half_red);
    }
    for (int i = 0; i < 8; ++i) {
      b.tri(hub, rim[i], rim[(i + 1) % 8]);
    }
  }
  {
    raster_scene& s = add("clip_rectangle", 10, 10);
    draw_builder& b = *s.builder;
    b.command(s.store.add(moved_white_pixel_texture()), ImVec4(2, 3, 5, 7));
    const ImDrawIdx a = b.vtx(ImVec2(-20, -20), white_uv, kRed);
    const ImDrawIdx c = b.vtx(ImVec2(40, -20), white_uv, kRed);
    const ImDrawIdx d = b.vtx(ImVec2(-20, 40), white_uv, kRed);
    b.tri(a, c, d);
  }
  {
    raster_scene& s = add("framebuffer_scale", 10, 10);
    s.builder->set_framebuffer_scale(2.0f);
    s.builder->command(s.store.add(moved_white_pixel_texture()));
    s.builder->solid(ImVec2(1, 2), ImVec2(4, 5), white_uv, kRed);
  }
  const ImU32 blends[][2] = {
      {IM_COL32(10, 20, 30, 255), IM_COL32(200, 100, 50, 128)},
      {IM_COL32(0, 0, 0, 0), IM_COL32(255, 0, 0, 64)},
      {IM_COL32(10, 20, 30, 255), IM_COL32(1, 2, 3, 255)},
      {IM_COL32(10, 20, 30, 40), IM_COL32(200, 100, 50, 0)},
  };
  for (int i = 0; i < 4; ++i) {
    raster_scene& s = add("blend_" + std::to_string(i), 2, 2);
    s.clear = blends[i][0];
    s.builder->command(s.store.add(moved_white_pixel_texture()));
    s.builder->solid(ImVec2(0, 0), ImVec2(2, 2), white_uv, blends[i][1]);
  }
  {
    raster_scene& s = add("textured_one_to_one", 6, 6);
    s.builder->command(s.store.add(gradient_texture()));
    s.builder->rect(ImVec2(1, 1), ImVec2(5, 5), ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE);
  }
  {
    raster_scene& s = add("half_texel", 1, 1);
    s.builder->command(s.store.add(gradient_texture()));
    s.builder->rect(ImVec2(0, 0), ImVec2(1, 1), texel_center(0, 0, 4, 4), ImVec2(1.5f / 4, 0.5f / 4), IM_COL32_WHITE);
  }
  {
    raster_scene& s = add("moved_white_pixel", 4, 2);
    s.builder->command(s.store.add(moved_white_pixel_texture()));
    s.builder->solid(ImVec2(0, 0), ImVec2(2, 2), white_uv, kRed);
    s.builder->solid(ImVec2(2, 0), ImVec2(4, 2), texel_center(0, 0, 4, 4), kRed);
  }
  {
    raster_scene& s = add("two_textures", 4, 2);
    const ImTextureID white = s.store.add(moved_white_pixel_texture());
    const ImTextureID gradient = s.store.add(gradient_texture());
    s.builder->command(white);
    s.builder->solid(ImVec2(0, 0), ImVec2(2, 2), white_uv, kRed);
    s.builder->command(gradient);
    s.builder->solid(ImVec2(2, 0), ImVec2(4, 2), texel_center(3, 1, 4, 4), IM_COL32_WHITE);
  }
  {
    // 16,900 one-pixel quads (67,600 vertices) through VtxOffset.
    constexpr int kSide = 130;
    raster_scene& s = add("vtx_offset_over_64k", kSide, kSide);
    const ImTextureID white = s.store.add(moved_white_pixel_texture());
    int in_command = 0;
    for (int i = 0; i < kSide * kSide; ++i) {
      if (i == 0 || in_command == 10000) {
        s.builder->command(white, ImVec4(-1e6f, -1e6f, 1e6f, 1e6f), true);
        in_command = 0;
      }
      const float x = static_cast<float>(i % kSide);
      const float y = static_cast<float>(i / kSide);
      s.builder->solid(ImVec2(x, y), ImVec2(x + 1, y + 1), white_uv, IM_COL32(i & 0xFF, (i >> 8) & 0xFF, 100, 255));
      ++in_command;
    }
  }
  {
    raster_scene& s = add("mixed", 64, 48);
    s.clear = IM_COL32(255, 255, 255, 255);
    draw_builder& b = *s.builder;
    const ImTextureID white = s.store.add(moved_white_pixel_texture());
    const ImTextureID gradient = s.store.add(gradient_texture());
    b.command(white);
    b.solid(ImVec2(0, 0), ImVec2(64, 48), white_uv, IM_COL32(240, 240, 240, 255));
    b.solid(ImVec2(3.25f, 4.75f), ImVec2(40.5f, 30.125f), white_uv, IM_COL32(20, 90, 200, 180));
    const ImDrawIdx g0 = b.vtx(ImVec2(10, 10), white_uv, IM_COL32(255, 0, 0, 255));
    const ImDrawIdx g1 = b.vtx(ImVec2(50, 10), white_uv, IM_COL32(0, 255, 0, 200));
    const ImDrawIdx g2 = b.vtx(ImVec2(50, 40), white_uv, IM_COL32(0, 0, 255, 100));
    const ImDrawIdx g3 = b.vtx(ImVec2(10, 40), white_uv, IM_COL32(255, 255, 0, 255));
    b.tri(g0, g1, g2);
    b.tri(g0, g2, g3);
    const ImDrawIdx s0 = b.vtx(ImVec2(5.5f, 44.2f), white_uv, IM_COL32(0, 0, 0, 90));
    const ImDrawIdx s1 = b.vtx(ImVec2(60.1f, 2.3f), white_uv, IM_COL32(0, 0, 0, 0));
    const ImDrawIdx s2 = b.vtx(ImVec2(61.7f, 5.9f), white_uv, IM_COL32(0, 0, 0, 90));
    b.tri(s0, s1, s2);
    b.command(gradient);
    for (int i = 0; i < 6; ++i) {
      const float x = 2.0f + 9.0f * i;
      b.rect(ImVec2(x, 33), ImVec2(x + 4, 37), ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 200, 100, 220));
    }
    b.rect(ImVec2(40, 2), ImVec2(62, 20), ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE);
  }
  return scenes;
}

} // namespace
} // namespace bdg::nymph
