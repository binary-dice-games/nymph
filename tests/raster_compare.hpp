// MIT License © 2026 Binary Dice Games
/// @file raster_compare.hpp
/// @brief Tolerance comparison of two rasterizers' images
///        (spec/02_opengl_rasterizer/DESIGN.md section 6).
#pragma once

#include <gtest/gtest.h>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include "png/png_meta.hpp"
#include "render/image.hpp"

namespace bdg::nymph {

/// @brief How far apart two images of the same size are.
struct raster_difference {
  double share_within_2 = 1.0; ///< pixels whose four channels differ by <= 2
  int max_channel = 0; ///< largest difference of any channel
  double mean_abs = 0.0; ///< mean absolute difference per channel
};

inline raster_difference compare_images(const image& a, const image& b) {
  raster_difference d;
  const size_t pixels = static_cast<size_t>(a.width) * a.height;
  if (pixels == 0) {
    return d;
  }
  size_t within = 0;
  double sum = 0.0;
  for (size_t i = 0; i < pixels; ++i) {
    int worst = 0;
    for (size_t c = 0; c < 4; ++c) {
      const int diff = std::abs(int(a.rgba[i * 4 + c]) - int(b.rgba[i * 4 + c]));
      worst = std::max(worst, diff);
      sum += diff;
    }
    within += worst <= 2 ? 1 : 0;
    d.max_channel = std::max(d.max_channel, worst);
  }
  d.share_within_2 = static_cast<double>(within) / pixels;
  d.mean_abs = sum / (pixels * 4.0);
  return d;
}

/// @brief The DESIGN.md section 6 thresholds.
inline constexpr double kMinShareWithin2 = 0.995;
inline constexpr int kMaxChannelDifference = 64;
inline constexpr double kMaxMeanDifference = 0.5;

/// @brief The share-within-2 rule for an image of `pixels` pixels: in tiny
///        images one pixel is more than 0.5 %, so allow at least one.
inline double min_share_for(size_t pixels) {
  return std::min(kMinShareWithin2, pixels <= 1 ? 0.0 : 1.0 - 1.0 / static_cast<double>(pixels));
}

/// @brief Writes `img` as a PNG next to the test binary, for a failure.
inline void dump_image(const image& img, const std::string& suffix) {
  const std::string name =
      std::string("nymph_") + ::testing::UnitTest::GetInstance()->current_test_info()->name() + "_" + suffix + ".png";
  const std::vector<uint8_t> png = encode_png(img, "", "");
  std::ofstream(name, std::ios::binary).write(reinterpret_cast<const char*>(png.data()), png.size());
}

/// @brief Asserts `actual` is within the DESIGN.md tolerance of `expected`
///        and returns the difference. On failure both images and their
///        difference (x8) are written to the working directory.
inline raster_difference expect_close(const image& expected, const image& actual, const std::string& label) {
  EXPECT_EQ(expected.width, actual.width) << label;
  EXPECT_EQ(expected.height, actual.height) << label;
  if (expected.width != actual.width || expected.height != actual.height) {
    return {0.0, 255, 255.0};
  }
  const raster_difference d = compare_images(expected, actual);
  const size_t pixels = static_cast<size_t>(expected.width) * expected.height;
  const bool ok = d.share_within_2 >= min_share_for(pixels) && d.max_channel <= kMaxChannelDifference &&
      d.mean_abs <= kMaxMeanDifference;
  EXPECT_TRUE(ok) << label << ": " << d.share_within_2 * 100.0 << "% within 2, max " << d.max_channel << ", mean "
                  << d.mean_abs;
  if (!ok) {
    image diff = expected;
    for (size_t i = 0; i < diff.rgba.size(); ++i) {
      diff.rgba[i] = (i % 4 == 3)
          ? 255
          : static_cast<uint8_t>(std::min(255, 8 * std::abs(int(expected.rgba[i]) - int(actual.rgba[i]))));
    }
    dump_image(expected, label + "_expected");
    dump_image(actual, label + "_actual");
    dump_image(diff, label + "_diff");
  }
  return d;
}

} // namespace bdg::nymph
