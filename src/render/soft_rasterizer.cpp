// MIT License © 2026 Binary Dice Games
/// @file soft_rasterizer.cpp
/// @brief rasterize(): ImDrawData to RGBA8 pixels on the CPU.
///
/// Structure and fast paths follow emilk's imgui_software_renderer (public
/// domain); coverage, sampling and blending are integer-exact here so the
/// fast paths and the general path produce the same bytes.
#include "render/soft_rasterizer.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace bdg::nymph {

void reset_render_state_callback(const ImDrawList*, const ImDrawCmd*) {}

namespace {

// ── Fixed-point coordinates ──────────────────────────────────────────────────

constexpr int64_t kOne = 256; ///< Sub-pixel units per pixel (8 bits).
constexpr int64_t kHalf = kOne / 2;
/// Vertices are clamped to +-2^19 px (64x the largest image), which keeps
/// every edge-function product inside 64 bits.
constexpr float kGuardBand = 524288.0f;

int64_t floor_div(int64_t a, int64_t b) {
  int64_t q = a / b;
  if ((a % b != 0) && (a < 0)) {
    --q;
  }
  return q;
}

int64_t ceil_div(int64_t a, int64_t b) {
  return -floor_div(-a, b);
}

/// Index of the first pixel whose centre is at or after fixed coordinate `e`.
int64_t first_center_at_or_after(int64_t e) {
  return ceil_div(e - kHalf, kOne);
}

/// Index of the last pixel whose centre is at or before fixed coordinate `e`.
int64_t last_center_at_or_before(int64_t e) {
  return floor_div(e - kHalf, kOne);
}

int64_t to_fixed(float v) {
  v = std::clamp(v, -kGuardBand, kGuardBand);
  return static_cast<int64_t>(std::llround(static_cast<double>(v) * kOne));
}

/// A vertex in output pixels (fixed point), with its ImGui attributes.
struct fx_vertex {
  int64_t x = 0;
  int64_t y = 0;
  const ImDrawVert* v = nullptr;
};

/// Pixels [x0, x1) x [y0, y1) that may be written.
struct pixel_rect {
  int64_t x0 = 0;
  int64_t y0 = 0;
  int64_t x1 = 0;
  int64_t y1 = 0;
  bool empty() const {
    return x0 >= x1 || y0 >= y1;
  }
};

// ── Colour arithmetic ────────────────────────────────────────────────────────

struct color8 {
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
  uint8_t a = 0;
};

color8 unpack(ImU32 c) {
  return {
      static_cast<uint8_t>(c >> IM_COL32_R_SHIFT),
      static_cast<uint8_t>(c >> IM_COL32_G_SHIFT),
      static_cast<uint8_t>(c >> IM_COL32_B_SHIFT),
      static_cast<uint8_t>(c >> IM_COL32_A_SHIFT)};
}

/// x / 255 rounded to nearest, for x in [0, 255 * 255].
uint8_t div255(uint32_t x) {
  return static_cast<uint8_t>((x + 127) / 255);
}

/// Vertex colour times texel, as a GPU fragment shader does it.
color8 modulate(color8 c, color8 t) {
  return {div255(c.r * t.r), div255(c.g * t.g), div255(c.b * t.b), div255(c.a * t.a)};
}

/// Straight-alpha "over": colour SRC_ALPHA / ONE_MINUS_SRC_ALPHA, alpha
/// ONE / ONE_MINUS_SRC_ALPHA (ImGui's blend state), with a one-entry cache
/// of the last (destination, source) pair when fast paths are on.
class blender {
 public:
  explicit blender(bool use_cache) : use_cache_(use_cache) {}

  void blend(uint8_t* dst, color8 s) {
    if (s.a == 0) {
      return;
    }
    uint32_t d_bits;
    uint32_t s_bits;
    std::memcpy(&d_bits, dst, 4);
    std::memcpy(&s_bits, &s, 4);
    if (use_cache_ && valid_ && d_bits == last_dst_ && s_bits == last_src_) {
      std::memcpy(dst, &last_out_, 4);
      return;
    }
    const uint32_t inv = 255u - s.a;
    dst[0] = div255(s.r * s.a + dst[0] * inv);
    dst[1] = div255(s.g * s.a + dst[1] * inv);
    dst[2] = div255(s.b * s.a + dst[2] * inv);
    dst[3] = static_cast<uint8_t>(s.a + div255(dst[3] * inv));
    if (use_cache_) {
      last_dst_ = d_bits;
      last_src_ = s_bits;
      std::memcpy(&last_out_, dst, 4);
      valid_ = true;
    }
  }

 private:
  bool use_cache_;
  bool valid_ = false;
  uint32_t last_dst_ = 0;
  uint32_t last_src_ = 0;
  uint32_t last_out_ = 0;
};

// ── Texture sampling ─────────────────────────────────────────────────────────

/// Texel (x, y), clamped to the edge. Alpha8 texels are white with that
/// alpha, as ImGui's GPU backends treat them.
color8 texel(const texture& t, int64_t x, int64_t y) {
  x = std::clamp<int64_t>(x, 0, t.width - 1);
  y = std::clamp<int64_t>(y, 0, t.height - 1);
  const size_t i = static_cast<size_t>(y) * t.width + static_cast<size_t>(x);
  if (t.format == ImTextureFormat_Alpha8) {
    return {255, 255, 255, t.pixels[i]};
  }
  const uint8_t* p = t.pixels.data() + i * 4;
  return {p[0], p[1], p[2], p[3]};
}

/// Bilinear sample at (u, v), clamp to edge, texel centres at (i + 0.5) /
/// size. The position is rounded to 1/256 texel first, so a sample within
/// that distance of a texel centre returns the texel exactly.
color8 sample(const texture& t, double u, double v) {
  const double fx = std::clamp(u * t.width - 0.5, -2.0, t.width + 1.0);
  const double fy = std::clamp(v * t.height - 0.5, -2.0, t.height + 1.0);
  const int64_t sx = std::llround(fx * kOne);
  const int64_t sy = std::llround(fy * kOne);
  const int64_t x0 = floor_div(sx, kOne);
  const int64_t y0 = floor_div(sy, kOne);
  const uint32_t ax = static_cast<uint32_t>(sx - x0 * kOne);
  const uint32_t ay = static_cast<uint32_t>(sy - y0 * kOne);
  if (ax == 0 && ay == 0) {
    return texel(t, x0, y0);
  }
  const color8 t00 = texel(t, x0, y0);
  const color8 t10 = texel(t, x0 + 1, y0);
  const color8 t01 = texel(t, x0, y0 + 1);
  const color8 t11 = texel(t, x0 + 1, y0 + 1);
  const uint32_t w00 = (256 - ax) * (256 - ay);
  const uint32_t w10 = ax * (256 - ay);
  const uint32_t w01 = (256 - ax) * ay;
  const uint32_t w11 = ax * ay;
  auto mix = [&](uint8_t c00, uint8_t c10, uint8_t c01, uint8_t c11) {
    return static_cast<uint8_t>((c00 * w00 + c10 * w10 + c01 * w01 + c11 * w11 + 32768u) >> 16);
  };
  return {
      mix(t00.r, t10.r, t01.r, t11.r),
      mix(t00.g, t10.g, t01.g, t11.g),
      mix(t00.b, t10.b, t01.b, t11.b),
      mix(t00.a, t10.a, t01.a, t11.a)};
}

// ── General path: one triangle ───────────────────────────────────────────────

/// Edge function of the directed edge from -> to, evaluated at pixel centres:
/// w(p) = dx * (p.y - from.y) - dy * (p.x - from.x).
struct edge {
  int64_t dx = 0;
  int64_t dy = 0;
  int64_t fx = 0;
  int64_t fy = 0;
  int64_t bias = 0; ///< 0 for a top or left edge, -1 otherwise.

  edge(const fx_vertex& from, const fx_vertex& to) : dx(to.x - from.x), dy(to.y - from.y), fx(from.x), fy(from.y) {
    // With the triangle oriented to positive area in y-down coordinates, a
    // left edge runs upwards and a top edge runs rightwards.
    const bool top_left = dy < 0 || (dy == 0 && dx > 0);
    bias = top_left ? 0 : -1;
  }
  int64_t at(int64_t cx, int64_t cy) const {
    return dx * (cy - fy) - dy * (cx - fx);
  }
};

uint8_t lerp_channel(double l0, double l1, double l2, uint8_t c0, uint8_t c1, uint8_t c2) {
  const double v = l0 * c0 + l1 * c1 + l2 * c2;
  return static_cast<uint8_t>(std::clamp<long>(std::lround(v), 0, 255));
}

void fill_triangle(
    fx_vertex a,
    fx_vertex b,
    fx_vertex c,
    const texture& tex,
    const pixel_rect& clip,
    image& img,
    blender& bl) {
  int64_t area = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
  if (area == 0) {
    return;
  }
  if (area < 0) {
    std::swap(b, c);
    area = -area;
  }

  const int64_t px0 = std::max(clip.x0, first_center_at_or_after(std::min({a.x, b.x, c.x})));
  const int64_t px1 = std::min(clip.x1 - 1, last_center_at_or_before(std::max({a.x, b.x, c.x})));
  const int64_t py0 = std::max(clip.y0, first_center_at_or_after(std::min({a.y, b.y, c.y})));
  const int64_t py1 = std::min(clip.y1 - 1, last_center_at_or_before(std::max({a.y, b.y, c.y})));
  if (px0 > px1 || py0 > py1) {
    return;
  }

  // w0 weighs a (edge b->c), w1 weighs b (edge c->a), w2 weighs c (edge a->b).
  const edge e0(b, c);
  const edge e1(c, a);
  const edge e2(a, b);

  // Attributes constant across the triangle are never interpolated, so a
  // uniform triangle gives exactly the bytes of the rectangle fast path.
  const bool uniform_color = a.v->col == b.v->col && a.v->col == c.v->col;
  const bool uniform_uv =
      a.v->uv.x == b.v->uv.x && a.v->uv.x == c.v->uv.x && a.v->uv.y == b.v->uv.y && a.v->uv.y == c.v->uv.y;
  const color8 ca = unpack(a.v->col);
  const color8 cb = unpack(b.v->col);
  const color8 cc = unpack(c.v->col);
  const color8 uniform_texel = uniform_uv ? sample(tex, a.v->uv.x, a.v->uv.y) : color8{};
  const color8 uniform_src = modulate(ca, uniform_texel);
  const double inv_area = 1.0 / static_cast<double>(area);

  for (int64_t py = py0; py <= py1; ++py) {
    const int64_t cy = py * kOne + kHalf;
    const int64_t cx0 = px0 * kOne + kHalf;
    int64_t w0 = e0.at(cx0, cy);
    int64_t w1 = e1.at(cx0, cy);
    int64_t w2 = e2.at(cx0, cy);
    bool entered = false;
    for (int64_t px = px0; px <= px1; ++px) {
      if (w0 + e0.bias >= 0 && w1 + e1.bias >= 0 && w2 + e2.bias >= 0) {
        entered = true;
        color8 src;
        if (uniform_color && uniform_uv) {
          src = uniform_src;
        } else {
          const double l0 = static_cast<double>(w0) * inv_area;
          const double l1 = static_cast<double>(w1) * inv_area;
          const double l2 = static_cast<double>(w2) * inv_area;
          const color8 col = uniform_color ? ca
                                           : color8{
                                                 lerp_channel(l0, l1, l2, ca.r, cb.r, cc.r),
                                                 lerp_channel(l0, l1, l2, ca.g, cb.g, cc.g),
                                                 lerp_channel(l0, l1, l2, ca.b, cb.b, cc.b),
                                                 lerp_channel(l0, l1, l2, ca.a, cb.a, cc.a)};
          const color8 tx = uniform_uv ? uniform_texel
                                       : sample(
                                             tex,
                                             l0 * a.v->uv.x + l1 * b.v->uv.x + l2 * c.v->uv.x,
                                             l0 * a.v->uv.y + l1 * b.v->uv.y + l2 * c.v->uv.y);
          src = modulate(col, tx);
        }
        bl.blend(img.at(static_cast<int>(px), static_cast<int>(py)), src);
      } else if (entered) {
        break; // a triangle is convex: once out, the rest of the row is out
      }
      w0 -= e0.dy * kOne;
      w1 -= e1.dy * kOne;
      w2 -= e2.dy * kOne;
    }
  }
}

// ── Fast path: axis-aligned quads ────────────────────────────────────────────

/// Text-quad tolerance, in texels: 1/4096. Two such errors stay far below
/// the general path's 1/256-texel rounding (see sample()), so both paths
/// read the same texel.
constexpr double kTexelTolerance = 1.0 / 4096.0;

bool near_zero(double v) {
  return std::fabs(v) < kTexelTolerance;
}

bool near_integer(double v) {
  return near_zero(v - std::round(v));
}

/// Fills the quad (a, b, c), (a, c, d) — the order of ImGui's PrimRect — in
/// one pass when it is an axis-aligned rectangle of one colour that has
/// either one UV (a solid fill) or a 1:1, pixel-aligned texture mapping (a
/// glyph). Covers exactly the pixels the two triangles would: centres in
/// [x0, x1) x [y0, y1). Returns false, drawing nothing, for any other quad.
bool try_fill_quad(
    const fx_vertex& a,
    const fx_vertex& b,
    const fx_vertex& c,
    const fx_vertex& d,
    const texture& tex,
    const pixel_rect& clip,
    image& img,
    blender& bl) {
  const ImU32 col = a.v->col;
  if (b.v->col != col || c.v->col != col || d.v->col != col) {
    return false;
  }
  const bool rows_first = a.y == b.y && b.x == c.x && c.y == d.y && d.x == a.x;
  const bool cols_first = a.x == b.x && b.y == c.y && c.x == d.x && d.y == a.y;
  if (!(rows_first || cols_first) || a.x == c.x || a.y == c.y) {
    return false;
  }
  const int64_t x0 = std::min(a.x, c.x);
  const int64_t x1 = std::max(a.x, c.x);
  const int64_t y0 = std::min(a.y, c.y);
  const int64_t y1 = std::max(a.y, c.y);
  const pixel_rect r{
      std::max(clip.x0, first_center_at_or_after(x0)),
      std::max(clip.y0, first_center_at_or_after(y0)),
      std::min(clip.x1, first_center_at_or_after(x1)),
      std::min(clip.y1, first_center_at_or_after(y1))};

  const ImVec2 uv_a = a.v->uv;
  const bool uniform_uv = b.v->uv.x == uv_a.x && b.v->uv.y == uv_a.y && c.v->uv.x == uv_a.x && c.v->uv.y == uv_a.y &&
      d.v->uv.x == uv_a.x && d.v->uv.y == uv_a.y;
  const color8 vc = unpack(col);
  if (uniform_uv) {
    const color8 src = modulate(vc, sample(tex, uv_a.x, uv_a.y));
    for (int64_t py = r.y0; py < r.y1; ++py) {
      for (int64_t px = r.x0; px < r.x1; ++px) {
        bl.blend(img.at(static_cast<int>(px), static_cast<int>(py)), src);
      }
    }
    return true;
  }

  // Text: u must depend on x only and v on y only, 1:1 and texel-aligned.
  const bool separable = rows_first
      ? (a.v->uv.y == b.v->uv.y && c.v->uv.y == d.v->uv.y && b.v->uv.x == c.v->uv.x && d.v->uv.x == a.v->uv.x)
      : (a.v->uv.x == b.v->uv.x && c.v->uv.x == d.v->uv.x && b.v->uv.y == c.v->uv.y && d.v->uv.y == a.v->uv.y);
  if (!separable || floor_div(x0, kOne) * kOne != x0 || floor_div(y0, kOne) * kOne != y0) {
    return false;
  }
  // a and c are opposite corners, so each holds one end of each axis.
  const double u_left = (a.x == x0 ? a.v->uv.x : c.v->uv.x) * static_cast<double>(tex.width);
  const double u_right = (a.x == x0 ? c.v->uv.x : a.v->uv.x) * static_cast<double>(tex.width);
  const double v_top = (a.y == y0 ? a.v->uv.y : c.v->uv.y) * static_cast<double>(tex.height);
  const double v_bottom = (a.y == y0 ? c.v->uv.y : a.v->uv.y) * static_cast<double>(tex.height);
  const double width_px = static_cast<double>(x1 - x0) / kOne;
  const double height_px = static_cast<double>(y1 - y0) / kOne;
  if (!near_integer(u_left) || !near_integer(v_top) || !near_zero(u_right - u_left - width_px) ||
      !near_zero(v_bottom - v_top - height_px)) {
    return false;
  }
  const int64_t tx0 = std::llround(u_left) - x0 / kOne;
  const int64_t ty0 = std::llround(v_top) - y0 / kOne;
  for (int64_t py = r.y0; py < r.y1; ++py) {
    for (int64_t px = r.x0; px < r.x1; ++px) {
      bl.blend(img.at(static_cast<int>(px), static_cast<int>(py)), modulate(vc, texel(tex, tx0 + px, ty0 + py)));
    }
  }
  return true;
}

// ── Draw commands ────────────────────────────────────────────────────────────

bool to_fx(const ImDrawVert& v, ImVec2 origin, ImVec2 scale, fx_vertex& out) {
  const float x = (v.pos.x - origin.x) * scale.x;
  const float y = (v.pos.y - origin.y) * scale.y;
  if (!std::isfinite(x) || !std::isfinite(y)) {
    return false;
  }
  out = {to_fixed(x), to_fixed(y), &v};
  return true;
}

bool is_reset_render_state(ImDrawCallback cb) {
  if (cb == &reset_render_state_callback) {
    return true;
  }
#ifdef ImDrawCallback_ResetRenderState
  if (cb == ImDrawCallback_ResetRenderState) {
    return true;
  }
#endif
  return false;
}

void paint_command(
    const ImDrawList& list,
    const ImDrawCmd& cmd,
    const ImDrawData& data,
    const texture& tex,
    image& img,
    blender& bl,
    bool fast_paths) {
  const ImVec2 origin = data.DisplayPos;
  const ImVec2 scale = data.FramebufferScale;
  // Pixels whose centre lies inside the clip rectangle.
  const pixel_rect clip{
      std::max<int64_t>(0, first_center_at_or_after(to_fixed((cmd.ClipRect.x - origin.x) * scale.x))),
      std::max<int64_t>(0, first_center_at_or_after(to_fixed((cmd.ClipRect.y - origin.y) * scale.y))),
      std::min<int64_t>(img.width, first_center_at_or_after(to_fixed((cmd.ClipRect.z - origin.x) * scale.x))),
      std::min<int64_t>(img.height, first_center_at_or_after(to_fixed((cmd.ClipRect.w - origin.y) * scale.y)))};
  if (clip.empty()) {
    return;
  }

  const ImDrawIdx* idx = list.IdxBuffer.Data + cmd.IdxOffset;
  const ImDrawVert* vtx = list.VtxBuffer.Data + cmd.VtxOffset;
  const unsigned int count = cmd.ElemCount - cmd.ElemCount % 3;
  unsigned int i = 0;
  while (i < count) {
    if (fast_paths && i + 6 <= count && idx[i + 3] == idx[i] && idx[i + 4] == idx[i + 2]) {
      fx_vertex q[4];
      if (to_fx(vtx[idx[i]], origin, scale, q[0]) && to_fx(vtx[idx[i + 1]], origin, scale, q[1]) &&
          to_fx(vtx[idx[i + 2]], origin, scale, q[2]) && to_fx(vtx[idx[i + 5]], origin, scale, q[3]) &&
          try_fill_quad(q[0], q[1], q[2], q[3], tex, clip, img, bl)) {
        i += 6;
        continue;
      }
    }
    fx_vertex a;
    fx_vertex b;
    fx_vertex c;
    if (to_fx(vtx[idx[i]], origin, scale, a) && to_fx(vtx[idx[i + 1]], origin, scale, b) &&
        to_fx(vtx[idx[i + 2]], origin, scale, c)) {
      fill_triangle(a, b, c, tex, clip, img, bl);
    }
    i += 3;
  }
}

} // namespace

image rasterize(
    const ImDrawData& draw_data,
    const texture_store& textures,
    ImU32 clear_color,
    const raster_options& options) {
  image img;
  img.width = std::max(0, static_cast<int>(std::lround(draw_data.DisplaySize.x * draw_data.FramebufferScale.x)));
  img.height = std::max(0, static_cast<int>(std::lround(draw_data.DisplaySize.y * draw_data.FramebufferScale.y)));
  img.rgba.resize(static_cast<size_t>(img.width) * img.height * 4);
  const color8 clear = unpack(clear_color);
  for (size_t i = 0; i < img.rgba.size(); i += 4) {
    std::memcpy(img.rgba.data() + i, &clear, 4);
  }

  blender bl(options.fast_paths);
  for (const ImDrawList* list : draw_data.CmdLists) {
    for (const ImDrawCmd& cmd : list->CmdBuffer) {
      if (cmd.UserCallback != nullptr) {
        if (is_reset_render_state(cmd.UserCallback)) {
          continue;
        }
        throw std::runtime_error("unsupported ImGui draw callback");
      }
      if (cmd.ElemCount == 0) {
        continue;
      }
      const texture* tex = textures.find(cmd.GetTexID());
      if (tex == nullptr) {
        throw std::runtime_error("draw command refers to an unknown texture");
      }
      paint_command(*list, cmd, draw_data, *tex, img, bl, options.fast_paths);
    }
  }
  return img;
}

} // namespace bdg::nymph
