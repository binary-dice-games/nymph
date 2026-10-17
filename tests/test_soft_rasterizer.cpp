// MIT License © 2026 Binary Dice Games
/// @file test_soft_rasterizer.cpp
/// @brief Plan step 2: soft_rasterizer and texture_store on hand-built draw
///        lists (no ImGui frame), so every vertex, index and clip rectangle
///        is known exactly.
#include <gtest/gtest.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>
#include "raster_scenes.hpp"
#include "render/soft_rasterizer.hpp"
#include "render/texture_store.hpp"

namespace bdg::nymph {
namespace {

/// Rasterizes with fast paths on and off, checks both give the same bytes,
/// and returns the image.
image render_both(const ImDrawData& data, const texture_store& store, ImU32 clear = kBlack) {
  const image fast = rasterize(data, store, clear, raster_options{true});
  const image slow = rasterize(data, store, clear, raster_options{false});
  EXPECT_EQ(fast.width, slow.width);
  EXPECT_EQ(fast.height, slow.height);
  EXPECT_TRUE(fast.rgba == slow.rgba) << "fast paths changed the output";
  return fast;
}

class soft_rasterizer : public ::testing::Test {
 protected:
  void SetUp() override {
    white_id_ = store_.add(moved_white_pixel_texture());
  }
  ImVec2 white_uv() const {
    return texel_center(2, 2, 4, 4);
  }
  texture_store store_;
  ImTextureID white_id_ = ImTextureID_Invalid;
};

// ── Coverage ─────────────────────────────────────────────────────────────────

TEST_F(soft_rasterizer, OpaqueRectangleCoversExactlyItsPixels) {
  draw_builder b(10, 10);
  b.command(white_id_);
  b.solid(ImVec2(2, 3), ImVec2(6, 8), white_uv(), kRed);
  const image img = render_both(b.data(), store_);

  ASSERT_EQ(img.width, 10);
  ASSERT_EQ(img.height, 10);
  for (int y = 0; y < 10; ++y) {
    for (int x = 0; x < 10; ++x) {
      const bool inside = x >= 2 && x < 6 && y >= 3 && y < 8;
      EXPECT_EQ(pixel(img, x, y), (inside ? rgba{255, 0, 0, 255} : rgba{0, 0, 0, 255})) << x << "," << y;
    }
  }
}

TEST_F(soft_rasterizer, SharedDiagonalIsBlendedOnce) {
  // Same square, split along the other diagonal, so it never matches the
  // rectangle fast path's index pattern: the triangle path alone.
  draw_builder b(8, 8);
  b.command(white_id_);
  const ImU32 half_red = IM_COL32(255, 0, 0, 128);
  const ImDrawIdx a = b.vtx(ImVec2(1, 1), white_uv(), half_red);
  const ImDrawIdx c1 = b.vtx(ImVec2(7, 1), white_uv(), half_red);
  const ImDrawIdx c2 = b.vtx(ImVec2(7, 7), white_uv(), half_red);
  const ImDrawIdx d = b.vtx(ImVec2(1, 7), white_uv(), half_red);
  b.tri(a, c1, d);
  b.tri(c1, c2, d);
  const image img = render_both(b.data(), store_);

  const rgba once = pixel(img, 1, 1);
  EXPECT_EQ(once, (rgba{128, 0, 0, 255}));
  for (int y = 0; y < 8; ++y) {
    for (int x = 0; x < 8; ++x) {
      const bool inside = x >= 1 && x < 7 && y >= 1 && y < 7;
      EXPECT_EQ(pixel(img, x, y), (inside ? once : rgba{0, 0, 0, 255})) << x << "," << y;
    }
  }
}

TEST_F(soft_rasterizer, TriangleFanCoversEachPixelOnce) {
  // Eight triangles around a centre at a non-integer position, on an
  // octagon with non-integer corners: every interior edge is shared.
  draw_builder b(16, 16);
  b.command(white_id_);
  const ImU32 half_red = IM_COL32(255, 0, 0, 128);
  const ImVec2 centre(8.3f, 7.6f);
  const ImDrawIdx hub = b.vtx(centre, white_uv(), half_red);
  ImDrawIdx rim[8];
  for (int i = 0; i < 8; ++i) {
    const float angle = 0.4f + i * 3.14159265f / 4.0f;
    rim[i] = b.vtx(ImVec2(centre.x + 6.1f * std::cos(angle), centre.y + 6.1f * std::sin(angle)), white_uv(), half_red);
  }
  for (int i = 0; i < 8; ++i) {
    b.tri(hub, rim[i], rim[(i + 1) % 8]);
  }
  const image img = render_both(b.data(), store_);

  int covered = 0;
  for (int y = 0; y < 16; ++y) {
    for (int x = 0; x < 16; ++x) {
      const rgba p = pixel(img, x, y);
      ASSERT_TRUE(p == (rgba{0, 0, 0, 255}) || p == (rgba{128, 0, 0, 255}))
          << "pixel blended twice at " << x << "," << y;
      covered += p[0] == 128 ? 1 : 0;
    }
  }
  // The octagon's area is about 2 * sqrt(2) * 6.1^2 = 105 px.
  EXPECT_GT(covered, 95);
  EXPECT_LT(covered, 115);
}

TEST_F(soft_rasterizer, ClipRectangleBoundsTheTriangle) {
  draw_builder b(10, 10);
  b.command(white_id_, ImVec4(2, 3, 5, 7));
  const ImDrawIdx a = b.vtx(ImVec2(-20, -20), white_uv(), kRed);
  const ImDrawIdx c = b.vtx(ImVec2(40, -20), white_uv(), kRed);
  const ImDrawIdx d = b.vtx(ImVec2(-20, 40), white_uv(), kRed);
  b.tri(a, c, d);
  const image img = render_both(b.data(), store_);

  for (int y = 0; y < 10; ++y) {
    for (int x = 0; x < 10; ++x) {
      const bool inside = x >= 2 && x < 5 && y >= 3 && y < 7;
      EXPECT_EQ(pixel(img, x, y)[0], inside ? 255 : 0) << x << "," << y;
    }
  }
}

TEST_F(soft_rasterizer, FramebufferScaleScalesTheOutput) {
  draw_builder b(10, 10);
  b.set_framebuffer_scale(2.0f);
  b.command(white_id_);
  b.solid(ImVec2(1, 2), ImVec2(4, 5), white_uv(), kRed);
  const image img = render_both(b.data(), store_);

  ASSERT_EQ(img.width, 20);
  ASSERT_EQ(img.height, 20);
  int covered = 0;
  for (int y = 0; y < 20; ++y) {
    for (int x = 0; x < 20; ++x) {
      const bool inside = x >= 2 && x < 8 && y >= 4 && y < 10;
      EXPECT_EQ(pixel(img, x, y)[0], inside ? 255 : 0) << x << "," << y;
      covered += pixel(img, x, y)[0] == 255 ? 1 : 0;
    }
  }
  EXPECT_EQ(covered, 36);
}

// ── Blending ─────────────────────────────────────────────────────────────────

TEST_F(soft_rasterizer, BlendGivesHandComputedBytes) {
  struct blend_case {
    ImU32 background;
    ImU32 source;
    rgba expected;
  };
  // out.rgb = (s * sa + d * (255 - sa) + 127) / 255
  // out.a   = sa + (da * (255 - sa) + 127) / 255
  const blend_case cases[] = {
      // r: (200*128 + 10*127 + 127) / 255 = 26997 / 255 = 105
      // g: (100*128 + 20*127 + 127) / 255 = 15467 / 255 = 60
      // b: ( 50*128 + 30*127 + 127) / 255 = 10337 / 255 = 40
      // a: 128 + (255*127 + 127) / 255 = 128 + 127 = 255
      {IM_COL32(10, 20, 30, 255), IM_COL32(200, 100, 50, 128), {105, 60, 40, 255}},
      // Over transparent: r = (255*64 + 127) / 255 = 64, a = 64.
      {IM_COL32(0, 0, 0, 0), IM_COL32(255, 0, 0, 64), {64, 0, 0, 64}},
      // Opaque source replaces.
      {IM_COL32(10, 20, 30, 255), IM_COL32(1, 2, 3, 255), {1, 2, 3, 255}},
      // Fully transparent source leaves the background.
      {IM_COL32(10, 20, 30, 40), IM_COL32(200, 100, 50, 0), {10, 20, 30, 40}},
  };
  for (const blend_case& bc : cases) {
    draw_builder b(2, 2);
    b.command(white_id_);
    b.solid(ImVec2(0, 0), ImVec2(2, 2), white_uv(), bc.source);
    const image img = render_both(b.data(), store_, bc.background);
    EXPECT_EQ(pixel(img, 1, 1), bc.expected);
  }
}

// ── Textures ─────────────────────────────────────────────────────────────────

TEST_F(soft_rasterizer, OneToOneTexturedQuadReproducesTexels) {
  const ImTextureID tex = store_.add(gradient_texture());
  draw_builder b(6, 6);
  b.command(tex);
  b.rect(ImVec2(1, 1), ImVec2(5, 5), ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE);
  const image img = render_both(b.data(), store_);

  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      EXPECT_EQ(
          pixel(img, 1 + x, 1 + y),
          (rgba{static_cast<uint8_t>(10 + 40 * x), static_cast<uint8_t>(10 + 40 * y), 7, 255}))
          << x << "," << y;
    }
  }
  EXPECT_EQ(pixel(img, 0, 0), (rgba{0, 0, 0, 255}));
  EXPECT_EQ(pixel(img, 5, 5), (rgba{0, 0, 0, 255}));
}

TEST_F(soft_rasterizer, SampleBetweenTwoTexelsIsTheirAverage) {
  const ImTextureID tex = store_.add(gradient_texture());
  draw_builder b(1, 1);
  b.command(tex);
  // Across the one pixel, u runs from texel 0's centre to texel 1's: the
  // pixel centre samples halfway, on row 0.
  b.rect(ImVec2(0, 0), ImVec2(1, 1), texel_center(0, 0, 4, 4), ImVec2(1.5f / 4, 0.5f / 4), IM_COL32_WHITE);
  const image img = render_both(b.data(), store_);
  // Texels (10, 10, 7) and (50, 10, 7): the average is (30, 10, 7).
  EXPECT_EQ(pixel(img, 0, 0), (rgba{30, 10, 7, 255}));
}

TEST_F(soft_rasterizer, SolidFillSamplesTheMovedWhitePixel) {
  draw_builder b(4, 2);
  b.command(white_id_);
  b.solid(ImVec2(0, 0), ImVec2(2, 2), white_uv(), kRed);
  b.solid(ImVec2(2, 0), ImVec2(4, 2), texel_center(0, 0, 4, 4), kRed); // alpha 0 there
  const image img = render_both(b.data(), store_);
  EXPECT_EQ(pixel(img, 1, 1), (rgba{255, 0, 0, 255}));
  EXPECT_EQ(pixel(img, 3, 1), (rgba{0, 0, 0, 255}));
}

TEST_F(soft_rasterizer, EachCommandSamplesItsOwnTexture) {
  const ImTextureID gradient = store_.add(gradient_texture());
  draw_builder b(4, 2);
  b.command(white_id_);
  b.solid(ImVec2(0, 0), ImVec2(2, 2), white_uv(), kRed);
  b.command(gradient);
  b.solid(ImVec2(2, 0), ImVec2(4, 2), texel_center(3, 1, 4, 4), IM_COL32_WHITE);
  const image img = render_both(b.data(), store_);
  EXPECT_EQ(pixel(img, 0, 0), (rgba{255, 0, 0, 255}));
  EXPECT_EQ(pixel(img, 3, 1), (rgba{130, 50, 7, 255}));
}

TEST_F(soft_rasterizer, UnknownTextureThrows) {
  draw_builder b(2, 2);
  b.command(static_cast<ImTextureID>(999));
  b.solid(ImVec2(0, 0), ImVec2(2, 2), ImVec2(0, 0), kRed);
  EXPECT_THROW(rasterize(b.data(), store_, kBlack), std::runtime_error);
}

// ── Large meshes and several lists ───────────────────────────────────────────

TEST_F(soft_rasterizer, VtxOffsetDrawsMeshesOver64KVertices) {
  // 16,900 one-pixel quads (67,600 vertices) of distinct colours, first in
  // one list split into commands with VtxOffset, then one small list each.
  constexpr int kSide = 130;
  auto colour = [](int i) { return IM_COL32(i & 0xFF, (i >> 8) & 0xFF, 100, 255); };

  draw_builder big(kSide, kSide);
  int in_command = 0;
  for (int i = 0; i < kSide * kSide; ++i) {
    if (i == 0 || in_command == 10000) { // 40,000 vertices per command
      big.command(white_id_, ImVec4(-1e6f, -1e6f, 1e6f, 1e6f), true);
      in_command = 0;
    }
    const float x = static_cast<float>(i % kSide);
    const float y = static_cast<float>(i / kSide);
    big.solid(ImVec2(x, y), ImVec2(x + 1, y + 1), white_uv(), colour(i));
    ++in_command;
  }
  ASSERT_GT(big.data().TotalVtxCount, 65536);

  draw_builder split(kSide, kSide);
  for (int i = 0; i < kSide * kSide; ++i) {
    if (i % 1000 == 0) {
      if (i > 0) {
        split.new_list();
      }
      split.command(white_id_);
    }
    const float x = static_cast<float>(i % kSide);
    const float y = static_cast<float>(i / kSide);
    split.solid(ImVec2(x, y), ImVec2(x + 1, y + 1), white_uv(), colour(i));
  }

  const image a = render_both(big.data(), store_);
  const image b = render_both(split.data(), store_);
  EXPECT_TRUE(a.rgba == b.rgba);
  EXPECT_EQ(pixel(a, 129, 129), (rgba{(16899 & 0xFF), (16899 >> 8) & 0xFF, 100, 255}));
}

// ── Callbacks ────────────────────────────────────────────────────────────────

void some_user_callback(const ImDrawList*, const ImDrawCmd*) {}

TEST_F(soft_rasterizer, ResetRenderStateIsIgnoredOtherCallbacksThrow) {
  draw_builder ok(2, 2);
  ok.add_callback(&reset_render_state_callback);
  ok.add_callback(ImDrawCallback_ResetRenderState);
  ok.command(white_id_);
  ok.solid(ImVec2(0, 0), ImVec2(2, 2), white_uv(), kRed);
  EXPECT_EQ(pixel(rasterize(ok.data(), store_, kBlack), 0, 0), (rgba{255, 0, 0, 255}));

  draw_builder bad(2, 2);
  bad.add_callback(&some_user_callback);
  EXPECT_THROW(rasterize(bad.data(), store_, kBlack), std::runtime_error);
}

// ── Fast paths and determinism ───────────────────────────────────────────────

TEST_F(soft_rasterizer, MixedSceneIsIdenticalWithAndWithoutFastPathsAndRepeatable) {
  const ImTextureID gradient = store_.add(gradient_texture());
  draw_builder b(64, 48);
  b.command(white_id_);
  b.solid(ImVec2(0, 0), ImVec2(64, 48), white_uv(), IM_COL32(240, 240, 240, 255));
  b.solid(ImVec2(3.25f, 4.75f), ImVec2(40.5f, 30.125f), white_uv(), IM_COL32(20, 90, 200, 180));
  // A gradient (non-uniform colour) quad and a slanted triangle.
  const ImDrawIdx g0 = b.vtx(ImVec2(10, 10), white_uv(), IM_COL32(255, 0, 0, 255));
  const ImDrawIdx g1 = b.vtx(ImVec2(50, 10), white_uv(), IM_COL32(0, 255, 0, 200));
  const ImDrawIdx g2 = b.vtx(ImVec2(50, 40), white_uv(), IM_COL32(0, 0, 255, 100));
  const ImDrawIdx g3 = b.vtx(ImVec2(10, 40), white_uv(), IM_COL32(255, 255, 0, 255));
  b.tri(g0, g1, g2);
  b.tri(g0, g2, g3);
  const ImDrawIdx s0 = b.vtx(ImVec2(5.5f, 44.2f), white_uv(), IM_COL32(0, 0, 0, 90));
  const ImDrawIdx s1 = b.vtx(ImVec2(60.1f, 2.3f), white_uv(), IM_COL32(0, 0, 0, 0));
  const ImDrawIdx s2 = b.vtx(ImVec2(61.7f, 5.9f), white_uv(), IM_COL32(0, 0, 0, 90));
  b.tri(s0, s1, s2);
  // Glyph-like 1:1 textured quads at integer positions, tinted.
  b.command(gradient);
  for (int i = 0; i < 6; ++i) {
    const float x = 2.0f + 9.0f * i;
    b.rect(ImVec2(x, 33), ImVec2(x + 4, 37), ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 200, 100, 220));
  }
  // A scaled (not 1:1) textured quad: bilinear in both paths.
  b.rect(ImVec2(40, 2), ImVec2(62, 20), ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE);

  const image first = render_both(b.data(), store_, IM_COL32(255, 255, 255, 255));
  const image second = rasterize(b.data(), store_, IM_COL32(255, 255, 255, 255));
  EXPECT_TRUE(first.rgba == second.rgba);
}

// ── texture_store ────────────────────────────────────────────────────────────

TEST(texture_store, FollowsTheCreateUpdateDestroyProtocol) {
  texture_store store;
  ImTextureData tex;
  tex.Create(ImTextureFormat_RGBA32, 4, 4);
  for (int i = 0; i < 4 * 4 * 4; ++i) {
    tex.Pixels[i] = 1;
  }
  ASSERT_EQ(tex.Status, ImTextureStatus_WantCreate);

  store.apply(tex);
  EXPECT_EQ(tex.Status, ImTextureStatus_OK);
  ASSERT_NE(tex.TexID, ImTextureID_Invalid);
  const texture* copy = store.find(tex.TexID);
  ASSERT_NE(copy, nullptr);
  EXPECT_EQ(copy->width, 4);
  EXPECT_EQ(copy->pixels[0], 1);

  // Change every texel, but report only a 2x1 rectangle at (1, 2).
  for (int i = 0; i < 4 * 4 * 4; ++i) {
    tex.Pixels[i] = 9;
  }
  ImTextureRect r;
  r.x = 1;
  r.y = 2;
  r.w = 2;
  r.h = 1;
  tex.Updates.push_back(r);
  tex.SetStatus(ImTextureStatus_WantUpdates);
  store.apply(tex);
  EXPECT_EQ(tex.Status, ImTextureStatus_OK);
  copy = store.find(tex.TexID);
  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      const bool updated = y == 2 && (x == 1 || x == 2);
      EXPECT_EQ(copy->pixels[(y * 4 + x) * 4], updated ? 9 : 1) << x << "," << y;
    }
  }

  const ImTextureID id = tex.TexID;
  tex.SetStatus(ImTextureStatus_WantDestroy);
  store.apply(tex);
  EXPECT_EQ(store.find(id), nullptr);
  EXPECT_EQ(tex.TexID, ImTextureID_Invalid);
  EXPECT_EQ(store.size(), 0u);
}

TEST(texture_store, UpdateForAnUnknownTextureThrows) {
  texture_store store;
  ImTextureData tex;
  tex.Create(ImTextureFormat_Alpha8, 2, 2);
  tex.SetTexID(static_cast<ImTextureID>(42));
  tex.SetStatus(ImTextureStatus_WantUpdates);
  EXPECT_THROW(store.apply(tex), std::runtime_error);
}

} // namespace
} // namespace bdg::nymph
