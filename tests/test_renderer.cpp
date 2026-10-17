// MIT License © 2026 Binary Dice Games
/// @file test_renderer.cpp
/// @brief spec/02_opengl_rasterizer plan step 5: the renderer API and its
///        fallback rules. Tests marked "always" need no OpenGL.
#include <gtest/gtest.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "core/error.hpp"
#include "nymph.hpp"
#include "raster_compare.hpp"
#include "render/renderer_options.hpp"

// Decodes pixels with its own copy of stb_image (nymph keeps its copy
// internal).
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include <stb_image.h>

namespace bdg::nymph {
namespace {

namespace fs = std::filesystem;

#if defined(_WIN32)
const char* const kMissing = "nymph-no-such-opengl32.dll";
#else
const char* const kMissing = "nymph-no-such-libEGL.so";
#endif

const char* const kSource =
    "description: A small line chart.\n"
    "image: {width: 200, height: 150}\n"
    "root:\n"
    "  type: Plot\n"
    "  children:\n"
    "    - {type: PlotLine, label: v, ys: $v}\n"
    "data: {v: [1, 3, 2]}\n";

/// The reason a renderer gives for the forced missing library: that name,
/// or, in a build without OpenGL, that.
bool names_missing_library(const std::string& reason) {
  return reason.find(kMissing) != std::string::npos || reason.find("built without OpenGL support") != std::string::npos;
}

std::string read_file(const fs::path& path) {
  std::ifstream f(path, std::ios::binary);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

std::vector<std::string> example_sources() {
  std::vector<fs::path> paths;
  for (const auto& e : fs::directory_iterator(fs::path(NYMPH_SOURCE_DIR) / "examples")) {
    if (e.path().extension() == ".yaml") {
      paths.push_back(e.path());
    }
  }
  std::sort(paths.begin(), paths.end());
  std::vector<std::string> sources;
  for (const fs::path& p : paths) {
    sources.push_back(read_file(p));
  }
  return sources;
}

/// Decodes a PNG's pixels as RGBA.
image decode(const std::vector<uint8_t>& png) {
  int w = 0;
  int h = 0;
  int n = 0;
  stbi_uc* data = stbi_load_from_memory(png.data(), static_cast<int>(png.size()), &w, &h, &n, 4);
  image img;
  if (data != nullptr) {
    img.width = w;
    img.height = h;
    img.rgba.assign(data, data + static_cast<size_t>(w) * h * 4);
    stbi_image_free(data);
  }
  return img;
}

/// A renderer on the OpenGL path with software GL allowed, or null.
std::unique_ptr<renderer> gl_renderer(renderer_test_options o = {}) {
  o.allow_software_gl = true;
  auto r = std::make_unique<renderer>(make_renderer(rasterizer::opengl, o));
  if (r->active() != rasterizer::opengl) {
    return nullptr;
  }
  return r;
}

#define REQUIRE_GL_RENDERER(r)                                                                                         \
  if (!(r)) {                                                                                                          \
    GTEST_SKIP() << "no OpenGL device: " << make_renderer(rasterizer::opengl, {true, {}, false, 0}).fallback_reason(); \
  }

// ── Always ───────────────────────────────────────────────────────────────────

TEST(renderer, SoftwareGivesTheBytesOfRenderPng) {
  renderer default_renderer;
  renderer software(rasterizer::software);
  EXPECT_EQ(default_renderer.requested(), rasterizer::software);
  for (const std::string& source : example_sources()) {
    const std::vector<uint8_t> expected = render_png(source);
    EXPECT_EQ(default_renderer.render_png(source), expected);
    EXPECT_EQ(software.render_png(source), expected);
  }
  EXPECT_EQ(software.last_used(), rasterizer::software);
  EXPECT_TRUE(software.fallback_reason().empty());
  EXPECT_TRUE(software.device_name().empty());
}

TEST(renderer, MissingOpenGLLibraryFallsBackToSoftware) {
  renderer_test_options o;
  o.library_names = {kMissing};
  renderer r = make_renderer(rasterizer::opengl, o);
  EXPECT_EQ(r.requested(), rasterizer::opengl);
  EXPECT_EQ(r.active(), rasterizer::software);
  EXPECT_TRUE(names_missing_library(r.fallback_reason())) << r.fallback_reason();
  EXPECT_EQ(r.render_png(kSource), render_png(kSource));
  EXPECT_EQ(r.last_used(), rasterizer::software);
  EXPECT_TRUE(r.device_name().empty());
}

TEST(renderer, SourceErrorsThrowBeforeAnyRasterizer) {
  const std::string bad = "root: {type: Plot, nonsense: 1}\n";
  renderer software;
  EXPECT_THROW(software.render_png(bad), error);
  renderer_test_options o;
  o.allow_software_gl = true;
  renderer gl = make_renderer(rasterizer::opengl, o);
  const rasterizer before = gl.active();
  try {
    gl.render_png(bad);
    ADD_FAILURE() << "no error";
  } catch (const error& e) {
    EXPECT_GT(e.line(), 0);
  }
  EXPECT_EQ(gl.active(), before); // a source error is not an OpenGL failure
}

TEST(renderer, NamesAndProbe) {
  EXPECT_EQ(rasterizer_name(rasterizer::software), "software");
  EXPECT_EQ(rasterizer_name(rasterizer::opengl), "opengl");
  const auto [available, text] = probe_opengl();
  EXPECT_FALSE(text.empty());
}

TEST(renderer, MovedFromRendererCanBeDestroyed) {
  renderer a(rasterizer::software);
  renderer b = std::move(a);
  EXPECT_EQ(b.render_png(kSource), render_png(kSource));
  renderer c;
  c = std::move(b);
  EXPECT_EQ(c.active(), rasterizer::software);
}

// ── With OpenGL (software GL allowed; skipped without a device) ──────────────

TEST(renderer, OpenGLRendersCloseToSoftwareAndKeepsTheSource) {
  auto r = gl_renderer();
  REQUIRE_GL_RENDERER(r);
  EXPECT_FALSE(r->device_name().empty());
  EXPECT_TRUE(r->fallback_reason().empty());
  for (const std::string& source : example_sources()) {
    const std::vector<uint8_t> png = r->render_png(source);
    EXPECT_EQ(r->last_used(), rasterizer::opengl);
    EXPECT_EQ(extract_source(png).value_or(""), source);
    expect_close(decode(render_png(source)), decode(png), "example");
  }
  EXPECT_EQ(r->active(), rasterizer::opengl);
}

TEST(renderer, OversizeImageFallsBackForThatImageOnly) {
  renderer_test_options o;
  o.max_size = 300;
  auto r = gl_renderer(o);
  REQUIRE_GL_RENDERER(r);
  const std::string big =
      "image: {width: 400, height: 150}\n"
      "root: {type: Plot, children: [{type: PlotLine, label: v, ys: $v}]}\n"
      "data: {v: [1, 3, 2]}\n";
  EXPECT_EQ(r->render_png(big), render_png(big));
  EXPECT_EQ(r->last_used(), rasterizer::software);
  EXPECT_EQ(r->active(), rasterizer::opengl);
  EXPECT_NE(r->fallback_reason().find("larger"), std::string::npos) << r->fallback_reason();
  r->render_png(kSource);
  EXPECT_EQ(r->last_used(), rasterizer::opengl);
}

TEST(renderer, DriverFailureFallsBackAndDropsOpenGL) {
  renderer_test_options o;
  o.fail_first_render = true;
  auto r = gl_renderer(o);
  REQUIRE_GL_RENDERER(r);
  EXPECT_EQ(r->render_png(kSource), render_png(kSource));
  EXPECT_EQ(r->last_used(), rasterizer::software);
  EXPECT_EQ(r->active(), rasterizer::software);
  EXPECT_FALSE(r->fallback_reason().empty());
  r->render_png(kSource);
  EXPECT_EQ(r->last_used(), rasterizer::software);
  EXPECT_TRUE(r->device_name().empty());
}

} // namespace
} // namespace bdg::nymph
