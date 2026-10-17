// MIT License © 2026 Binary Dice Games
/// @file test_gl_rasterizer.cpp
/// @brief spec/02_opengl_rasterizer plan step 4: the OpenGL rasterizer
///        against the software one, on hand-built scenes and the examples.
///        Skips without an OpenGL device.
#include <gtest/gtest.h>
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include "document/bind.hpp"
#include "elements/register_builtin.hpp"
#include "gl_test_device.hpp"
#include "raster_compare.hpp"
#include "raster_scenes.hpp"
#include "render/figure_renderer.hpp"
#include "render/gl_rasterizer.hpp"

namespace bdg::nymph {
namespace {

namespace fs = std::filesystem;

/// Worst values over a test, printed so a run records them.
struct worst_case {
  double share = 1.0;
  int max_channel = 0;
  double mean = 0.0;
  void add(const raster_difference& d) {
    share = std::min(share, d.share_within_2);
    max_channel = std::max(max_channel, d.max_channel);
    mean = std::max(mean, d.mean_abs);
  }
  void print(const char* what) const {
    std::printf(
        "%s on %s: worst %.3f%% within 2, max %d, mean %.4f\n",
        what,
        test_device()->renderer_name().c_str(),
        share * 100.0,
        max_channel,
        mean);
  }
};

// ── Hand-built scenes ────────────────────────────────────────────────────────

TEST(gl_rasterizer, EverySceneMatchesTheSoftwareRasterizer) {
  NYMPH_REQUIRE_GL();
  gl_backend gl(*test_device());
  worst_case worst;
  for (const raster_scene& s : raster_scenes()) {
    const image soft = rasterize(s.data(), s.store, s.clear);
    const image hard = gl.rasterize(s.data(), s.store, s.clear);
    worst.add(expect_close(soft, hard, s.name));
  }
  worst.print("scenes");
}

TEST(gl_rasterizer, OpaqueRectangleAndClearAreExact) {
  NYMPH_REQUIRE_GL();
  gl_backend gl(*test_device());
  const std::vector<raster_scene> scenes = raster_scenes();
  const raster_scene& rect = scenes.front();
  ASSERT_EQ(rect.name, "rectangle");
  const image img = gl.rasterize(rect.data(), rect.store, rect.clear);
  ASSERT_EQ(img.width, 10);
  for (int y = 0; y < 10; ++y) {
    for (int x = 0; x < 10; ++x) {
      const bool inside = x >= 2 && x < 6 && y >= 3 && y < 8;
      EXPECT_EQ(pixel(img, x, y), (inside ? rgba{255, 0, 0, 255} : rgba{0, 0, 0, 255})) << x << "," << y;
    }
  }

  draw_builder empty(5, 3);
  texture_store store;
  const image cleared = gl.rasterize(empty.data(), store, IM_COL32(12, 34, 56, 255));
  ASSERT_EQ(cleared.width, 5);
  ASSERT_EQ(cleared.height, 3);
  for (int y = 0; y < 3; ++y) {
    for (int x = 0; x < 5; ++x) {
      EXPECT_EQ(pixel(cleared, x, y), (rgba{12, 34, 56, 255}));
    }
  }
}

void some_user_callback(const ImDrawList*, const ImDrawCmd*) {}

TEST(gl_rasterizer, CallbacksAndUnknownTexturesAreErrorsNotFallbacks) {
  NYMPH_REQUIRE_GL();
  gl_backend gl(*test_device());
  texture_store store;
  const ImTextureID white = store.add(moved_white_pixel_texture());

  draw_builder ok(2, 2);
  ok.add_callback(&reset_render_state_callback);
  ok.add_callback(ImDrawCallback_ResetRenderState);
  ok.command(white);
  ok.solid(ImVec2(0, 0), ImVec2(2, 2), texel_center(2, 2, 4, 4), kRed);
  EXPECT_EQ(pixel(gl.rasterize(ok.data(), store, kBlack), 0, 0), (rgba{255, 0, 0, 255}));

  draw_builder bad(2, 2);
  bad.add_callback(&some_user_callback);
  try {
    gl.rasterize(bad.data(), store, kBlack);
    ADD_FAILURE() << "no exception";
  } catch (const raster_unavailable&) {
    ADD_FAILURE() << "a bad callback must not be a fallback";
  } catch (const std::runtime_error&) {
  }

  draw_builder unknown(2, 2);
  unknown.command(static_cast<ImTextureID>(999));
  unknown.solid(ImVec2(0, 0), ImVec2(2, 2), ImVec2(0, 0), kRed);
  try {
    gl.rasterize(unknown.data(), store, kBlack);
    ADD_FAILURE() << "no exception";
  } catch (const raster_unavailable&) {
    ADD_FAILURE() << "an unknown texture must not be a fallback";
  } catch (const std::runtime_error&) {
  }

  // The backend still works after the errors.
  EXPECT_EQ(pixel(gl.rasterize(ok.data(), store, kBlack), 1, 1), (rgba{255, 0, 0, 255}));
}

TEST(gl_rasterizer, ImageLargerThanTheDeviceIsUnavailable) {
  NYMPH_REQUIRE_GL();
  gl_backend gl(*test_device());
  draw_builder b(1, 1);
  b.set_framebuffer_scale(static_cast<float>(test_device()->max_size()) + 1.0f);
  texture_store store;
  EXPECT_THROW(gl.rasterize(b.data(), store, kBlack), raster_too_large);
  // The test hook lowers the limit the same way.
  draw_builder small(20, 20);
  gl.limit_size(16);
  EXPECT_THROW(gl.rasterize(small.data(), store, kBlack), raster_too_large);
  gl.limit_size(0);
  EXPECT_NO_THROW(gl.rasterize(small.data(), store, kBlack));
}

TEST(gl_rasterizer, IsRepeatableOnOneDevice) {
  NYMPH_REQUIRE_GL();
  gl_backend gl(*test_device());
  const std::vector<raster_scene> scenes = raster_scenes();
  const raster_scene& mixed = scenes.back();
  ASSERT_EQ(mixed.name, "mixed");
  const image a = gl.rasterize(mixed.data(), mixed.store, mixed.clear);
  const image b = gl.rasterize(mixed.data(), mixed.store, mixed.clear);
  EXPECT_TRUE(a.rgba == b.rgba);
}

TEST(gl_rasterizer, InjectedFailureIsUnavailableOnce) {
  NYMPH_REQUIRE_GL();
  gl_backend gl(*test_device());
  draw_builder b(2, 2);
  texture_store store;
  gl.fail_next_rasterize();
  EXPECT_THROW(gl.rasterize(b.data(), store, kBlack), raster_unavailable);
  EXPECT_NO_THROW(gl.rasterize(b.data(), store, kBlack));
}

// ── Whole figures ────────────────────────────────────────────────────────────

std::string read_file(const fs::path& path) {
  std::ifstream f(path, std::ios::binary);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

std::vector<fs::path> example_sources() {
  std::vector<fs::path> paths;
  for (const auto& e : fs::directory_iterator(fs::path(NYMPH_SOURCE_DIR) / "examples")) {
    if (e.path().extension() == ".yaml") {
      paths.push_back(e.path());
    }
  }
  std::sort(paths.begin(), paths.end());
  return paths;
}

/// `source` with its `image:` mapping given extra keys (flow style only).
std::string with_image_options(std::string source, const std::string& extra) {
  const size_t at = source.find("\nimage: {");
  if (at == std::string::npos) {
    return {};
  }
  source.insert(at + 9, extra + ", ");
  return source;
}

TEST(gl_rasterizer, EveryExampleMatchesTheSoftwareRasterizer) {
  NYMPH_REQUIRE_GL();
  gl_backend gl(*test_device());
  const std::vector<fs::path> sources = example_sources();
  ASSERT_GE(sources.size(), 8u);
  worst_case worst;
  int variants = 0;
  for (const fs::path& path : sources) {
    const std::string source = read_file(path);
    std::vector<std::pair<std::string, std::string>> cases{{path.stem().string(), source}};
    if (path.stem() == "line" || path.stem() == "surface") {
      for (const char* extra : {"scale: 2", "theme: dark"}) {
        const std::string variant = with_image_options(source, extra);
        if (!variant.empty()) {
          cases.emplace_back(path.stem().string() + "_" + (extra[0] == 's' ? "scale2" : "dark"), variant);
          ++variants;
        }
      }
    }
    for (const auto& [label, text] : cases) {
      const figure f = bind_source(text, builtin_registry());
      const image soft = render_figure(f);
      const image hard = render_figure(f, gl);
      worst.add(expect_close(soft, hard, label));
    }
  }
  worst.print("examples");
  std::printf("%d scale / theme variants\n", variants);
}

} // namespace
} // namespace bdg::nymph
