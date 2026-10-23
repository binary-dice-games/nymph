// MIT License © 2026 Binary Dice Games
/// @file test_figure_renderer.cpp
/// @brief Plan step 3: render_frames() draws real ImGui / ImPlot / ImPlot3D
///        frames into an image with no window, GPU or display.
#include <gtest/gtest.h>
#include <imgui.h>
#include <implot.h>
#include <implot3d.h>
#include <array>
#include <cstdint>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#include "render/figure_renderer.hpp"

namespace bdg::nymph {
namespace {

using rgba = std::array<uint8_t, 4>;

rgba pixel(const image& img, int x, int y) {
  const uint8_t* p = img.at(x, y);
  return {p[0], p[1], p[2], p[3]};
}

/// Number of distinct colours in [x0, x1) x [y0, y1).
size_t distinct_colors(const image& img, int x0, int y0, int x1, int y1) {
  std::set<uint32_t> colors;
  for (int y = y0; y < y1; ++y) {
    for (int x = x0; x < x1; ++x) {
      const uint8_t* p = img.at(x, y);
      colors.insert(static_cast<uint32_t>(p[0]) | p[1] << 8 | p[2] << 16 | static_cast<uint32_t>(p[3]) << 24);
    }
  }
  return colors.size();
}

/// Pixels that differ from `background`, and pixels exactly equal to `ink`.
struct ink_count {
  int touched = 0;
  int solid = 0;
};

ink_count count_ink(const image& img, rgba background, rgba ink) {
  ink_count c;
  for (int y = 0; y < img.height; ++y) {
    for (int x = 0; x < img.width; ++x) {
      const rgba p = pixel(img, x, y);
      c.touched += p != background ? 1 : 0;
      c.solid += p == ink ? 1 : 0;
    }
  }
  return c;
}

const float kXs[] = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f};
const float kYs[] = {1.0f, 3.0f, 2.0f, 4.0f, 3.5f};
const float kZs[] = {0.0f, 1.0f, 0.5f, 2.0f, 1.0f};

/// A Plot with one PlotLine filling the content rectangle.
void draw_line_plot(draw_context& ctx) {
  const rect r = ctx.content_rect();
  ImGui::SetCursorScreenPos(r.min);
  if (ImPlot::BeginPlot("line", ImVec2(r.width(), r.height()))) {
    ImPlot::PlotLine("y", kXs, kYs, 5);
    ImPlot::EndPlot();
  }
}

image_options options(int width, int height, int scale = 1, const char* theme = "light") {
  image_options o;
  o.width = width;
  o.height = height;
  o.scale = scale;
  o.theme = theme;
  o.base = theme; // render_frames() takes bound options: the base colours draw
  return o;
}

// ── Output ───────────────────────────────────────────────────────────────────

TEST(figure_renderer, LinePlotRendersInsideAWindowBackground) {
  const image img = render_frames(options(400, 300), &draw_line_plot);
  ASSERT_EQ(img.width, 400);
  ASSERT_EQ(img.height, 300);
  // Plot area: frame, grid, line, axis labels and the title all differ.
  EXPECT_GT(distinct_colors(img, 50, 50, 350, 250), 2u);
  // The 8 px padding shows the light theme's window background (240).
  EXPECT_EQ(pixel(img, 0, 0), (rgba{240, 240, 240, 255}));
  EXPECT_EQ(pixel(img, 399, 299), (rgba{240, 240, 240, 255}));
  // Every pixel is opaque.
  for (size_t i = 3; i < img.rgba.size(); i += 4) {
    ASSERT_EQ(img.rgba[i], 255);
  }
}

TEST(figure_renderer, Plot3DRenders) {
  const image img = render_frames(options(300, 300), [](draw_context& ctx) {
    const rect r = ctx.content_rect();
    ImGui::SetCursorScreenPos(r.min);
    if (ImPlot3D::BeginPlot("3d", ImVec2(r.width(), r.height()))) {
      ImPlot3D::PlotLine("line", kXs, kYs, kZs, 5);
      ImPlot3D::EndPlot();
    }
  });
  EXPECT_GT(distinct_colors(img, 20, 20, 280, 280), 2u);
}

TEST(figure_renderer, ThemesChangeTheBackground) {
  const image light = render_frames(options(100, 100, 1, "light"), &draw_line_plot);
  const image dark = render_frames(options(100, 100, 1, "dark"), &draw_line_plot);
  const image classic = render_frames(options(100, 100, 1, "classic"), &draw_line_plot);
  EXPECT_NE(pixel(light, 0, 0), pixel(dark, 0, 0));
  EXPECT_NE(pixel(dark, 0, 0), pixel(classic, 0, 0));
}

// ── Scale and text ───────────────────────────────────────────────────────────

TEST(figure_renderer, ScaleTwoDoublesTheImageAndRebakesText) {
  auto draw_text = [](draw_context& ctx) {
    ImGui::SetCursorScreenPos(ctx.content_rect().min);
    ImGui::TextUnformatted("The quick brown fox jumps over 0123456789");
  };
  const image x1 = render_frames(options(400, 60, 1), draw_text);
  const image x2 = render_frames(options(400, 60, 2), draw_text);
  ASSERT_EQ(x2.width, 800);
  ASSERT_EQ(x2.height, 120);

  const rgba bg{240, 240, 240, 255};
  const rgba black{0, 0, 0, 255};
  const ink_count c1 = count_ink(x1, bg, black);
  const ink_count c2 = count_ink(x2, bg, black);
  ASSERT_GT(c1.touched, 200);
  // Text covers roughly four times the pixels at twice the scale (3.1x
  // when written: anti-aliased edges are a larger share at 1x)...
  const double area_ratio = static_cast<double>(c2.touched) / c1.touched;
  EXPECT_GT(area_ratio, 2.5);
  EXPECT_LT(area_ratio, 5.0);
  // ...and is baked at the higher density, not stretched. Fully inked
  // pixels were 5.6% of text pixels at 1x and 37% at 2x when written; a
  // glyph baked at 1x and magnified would stay near the 1x share.
  const double solid_share_x1 = static_cast<double>(c1.solid) / c1.touched;
  const double solid_share_x2 = static_cast<double>(c2.solid) / c2.touched;
  EXPECT_GT(solid_share_x2, solid_share_x1 * 3.0);
}

// ── Determinism and contexts ─────────────────────────────────────────────────

TEST(figure_renderer, IsRepeatableAndLeavesNoContextCurrent) {
  ASSERT_EQ(ImGui::GetCurrentContext(), nullptr);
  const image a = render_frames(options(320, 200), &draw_line_plot);
  EXPECT_EQ(ImGui::GetCurrentContext(), nullptr);
  EXPECT_EQ(ImPlot::GetCurrentContext(), nullptr);
  EXPECT_EQ(ImPlot3D::GetCurrentContext(), nullptr);
  const image b = render_frames(options(320, 200), &draw_line_plot);
  EXPECT_TRUE(a.rgba == b.rgba);
}

TEST(figure_renderer, RunsInsideAnotherOpenFrameAndRestoresItsContexts) {
  ImGuiContext* outer = ImGui::CreateContext();
  ImPlotContext* outer_plot = ImPlot::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.DisplaySize = ImVec2(100, 100);
  io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
  ImGui::NewFrame();

  const image img = render_frames(options(120, 80), &draw_line_plot);
  EXPECT_EQ(img.width, 120);
  EXPECT_EQ(ImGui::GetCurrentContext(), outer);
  EXPECT_EQ(ImPlot::GetCurrentContext(), outer_plot);

  ImGui::EndFrame();
  ImPlot::DestroyContext(outer_plot);
  ImGui::DestroyContext(outer);
}

TEST(figure_renderer, ContextsAreRestoredWhenDrawingThrows) {
  EXPECT_THROW(
      render_frames(options(100, 100), [](draw_context&) { throw std::runtime_error("boom"); }), std::runtime_error);
  EXPECT_EQ(ImGui::GetCurrentContext(), nullptr);
  EXPECT_EQ(ImPlot::GetCurrentContext(), nullptr);
}

// ── Options ──────────────────────────────────────────────────────────────────

TEST(figure_renderer, RejectsOutOfRangeOptions) {
  auto nothing = [](draw_context&) {};
  EXPECT_THROW(render_frames(options(15, 100), nothing), std::invalid_argument);
  EXPECT_THROW(render_frames(options(100, 4097), nothing), std::invalid_argument);
  EXPECT_THROW(render_frames(options(100, 100, 5), nothing), std::invalid_argument);
  EXPECT_THROW(render_frames(options(3000, 100, 3), nothing), std::invalid_argument);
  EXPECT_THROW(render_frames(options(100, 100, 1, "solarized"), nothing), std::invalid_argument);
  image_options padded = options(100, 100);
  padded.padding = 50;
  EXPECT_THROW(render_frames(padded, nothing), std::invalid_argument);
  EXPECT_TRUE(is_known_theme("classic"));
  EXPECT_FALSE(is_known_theme("wish"));
}

// ── Raster backends (spec/02_opengl_rasterizer, plan step 1) ─────────────────

/// Records what render_frames() hands it and returns a marker image.
class recording_backend final : public raster_backend {
 public:
  const char* name() const noexcept override {
    return "nymph_test_backend";
  }
  image rasterize(const ImDrawData& data, const texture_store& textures, ImU32 clear) override {
    ++calls;
    width = static_cast<int>(data.DisplaySize.x * data.FramebufferScale.x + 0.5f);
    height = static_cast<int>(data.DisplaySize.y * data.FramebufferScale.y + 0.5f);
    clear_color = clear;
    texture_count = textures.size();
    if (fail) {
      throw raster_unavailable("test backend unavailable");
    }
    image img;
    img.width = 1;
    img.height = 1;
    img.rgba = {1, 2, 3, 4};
    return img;
  }
  int calls = 0;
  int width = 0;
  int height = 0;
  ImU32 clear_color = 0;
  size_t texture_count = 0;
  bool fail = false;
};

TEST(figure_renderer, BackendIsCalledOnceWithTheLastFrame) {
  recording_backend backend;
  const image img = render_frames(options(120, 80, 2), &draw_line_plot, backend);
  EXPECT_EQ(backend.calls, 1);
  EXPECT_EQ(backend.width, 240);
  EXPECT_EQ(backend.height, 160);
  EXPECT_EQ(backend.clear_color, IM_COL32(240, 240, 240, 255)); // light WindowBg, opaque
  EXPECT_GE(backend.texture_count, 1u); // the font atlas
  EXPECT_EQ(img.rgba, (std::vector<uint8_t>{1, 2, 3, 4}));
}

TEST(figure_renderer, BackendNameIsTheRendererNameWhileDrawing) {
  recording_backend backend;
  std::string seen;
  render_frames(options(64, 64), [&](draw_context&) { seen = ImGui::GetIO().BackendRendererName; }, backend);
  EXPECT_EQ(seen, "nymph_test_backend");
  render_frames(options(64, 64), [&](draw_context&) { seen = ImGui::GetIO().BackendRendererName; });
  EXPECT_EQ(seen, "nymph_soft_rasterizer");
}

TEST(figure_renderer, RasterUnavailablePropagatesAndContextsAreRestored) {
  recording_backend backend;
  backend.fail = true;
  EXPECT_THROW(render_frames(options(64, 64), &draw_line_plot, backend), raster_unavailable);
  EXPECT_EQ(ImGui::GetCurrentContext(), nullptr);
  EXPECT_EQ(ImPlot::GetCurrentContext(), nullptr);
  EXPECT_EQ(ImPlot3D::GetCurrentContext(), nullptr);
}

TEST(figure_renderer, SoftwareBackendGivesTheDefaultBytes) {
  software_backend software;
  const image a = render_frames(options(160, 120), &draw_line_plot, software);
  const image b = render_frames(options(160, 120), &draw_line_plot);
  EXPECT_TRUE(a.rgba == b.rgba);
}

} // namespace
} // namespace bdg::nymph
