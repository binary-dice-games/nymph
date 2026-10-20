// MIT License © 2026 Binary Dice Games
/// @file test_window_icon.cpp
/// @brief The `nymph view` window's icon: resources/icon.png compiled in,
///        and its smaller sizes. Setting it on a real window needs a
///        display and is checked by hand.
#include <gtest/gtest.h>
#include <cstdint>
#include "render/image.hpp"
#include "window/window_icon.hpp"

namespace bdg::nymph {
namespace {

image solid(int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  image img;
  img.width = w;
  img.height = h;
  img.rgba.resize(static_cast<size_t>(w) * h * 4);
  for (size_t i = 0; i < img.rgba.size(); i += 4) {
    img.rgba[i + 0] = r;
    img.rgba[i + 1] = g;
    img.rgba[i + 2] = b;
    img.rgba[i + 3] = a;
  }
  return img;
}

TEST(window_icon, IsTheEmbeddedSquareIconWithTransparency) {
  const image& icon = window_icon();
  ASSERT_FALSE(icon.rgba.empty());
  EXPECT_EQ(icon.width, 512);
  EXPECT_EQ(icon.height, 512);
  EXPECT_EQ(icon.at(0, 0)[3], 0) << "the corner is transparent";
  EXPECT_EQ(icon.at(256, 256)[3], 255) << "the dragonfly's body is opaque";
  EXPECT_EQ(&window_icon(), &icon) << "decoded once";
}

TEST(window_icon, ScalesToEverySize) {
  for (const int size : {16, 20, 24, 32, 48, 64, 128, 256}) {
    const image scaled = scale_icon(window_icon(), size);
    EXPECT_EQ(scaled.width, size);
    EXPECT_EQ(scaled.height, size);
    EXPECT_EQ(scaled.rgba.size(), static_cast<size_t>(size) * size * 4);
    EXPECT_LT(scaled.at(0, 0)[3], 8) << "the corner is (nearly) transparent";
  }
}

TEST(window_icon, ScalingKeepsAUniformColour) {
  const image scaled = scale_icon(solid(100, 100, 10, 200, 30, 128), 7);
  for (int y = 0; y < 7; ++y) {
    for (int x = 0; x < 7; ++x) {
      const uint8_t* p = scaled.at(x, y);
      EXPECT_EQ(p[0], 10);
      EXPECT_EQ(p[1], 200);
      EXPECT_EQ(p[2], 30);
      EXPECT_EQ(p[3], 128);
    }
  }
}

TEST(window_icon, TransparentPixelsDoNotDarkenTheEdges) {
  // Left half opaque red, right half transparent black: the averaged pixel
  // stays pure red, at half coverage.
  image img = solid(2, 1, 255, 0, 0, 255);
  img.at(1, 0)[0] = 0;
  img.at(1, 0)[3] = 0;
  const image scaled = scale_icon(img, 1);
  EXPECT_EQ(scaled.at(0, 0)[0], 255);
  EXPECT_EQ(scaled.at(0, 0)[3], 128);
}

TEST(window_icon, UpscalingRepeatsPixels) {
  const image scaled = scale_icon(solid(1, 1, 1, 2, 3, 4), 3);
  ASSERT_EQ(scaled.width, 3);
  EXPECT_EQ(scaled.at(2, 2)[2], 3);
  EXPECT_EQ(scaled.at(2, 2)[3], 4);
}

TEST(window_icon, EmptyInputOrSizeGivesAnEmptyImage) {
  EXPECT_TRUE(scale_icon(image{}, 32).rgba.empty());
  EXPECT_TRUE(scale_icon(window_icon(), 0).rgba.empty());
  EXPECT_TRUE(scale_icon(window_icon(), -4).rgba.empty());
}

} // namespace
} // namespace bdg::nymph
