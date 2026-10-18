// MIT License © 2026 Binary Dice Games
/// @file live_view.cpp
/// @brief live_view: persistent contexts, sizing, and frames on demand
///        (spec/04_web_viewer/DESIGN.md section 4).
#include "view/live_view.hpp"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <optional>
#include <unordered_map>
#include <utility>
#include "core/error.hpp"
#include "document/bind.hpp"
#include "elements/register_builtin.hpp"
#include "fonts/fonts_used.hpp"
#include "layout/layout.hpp"
#include "nymph.hpp"
#include "render/draw_context.hpp"
#include "render/figure_renderer.hpp"
#include "render/frame_setup.hpp"

namespace bdg::nymph {

namespace {

/// Largest device pixel ratio used (DESIGN.md section 3, "Sizing").
constexpr float kMaxPixelRatio = 3.0f;
/// Largest side of the drawn image in device pixels (as render_frames()).
constexpr float kMaxPixels = 8192.0f;
/// Frames run after an input: ImPlot sees the hover one frame after the
/// pointer moves, ImGui hides a new tooltip window on its first frame, and
/// ImPlot applies a fit one frame late.
constexpr int kInputFrames = 3;

/// Paths of every node under `n` ("root/children/1/children/0").
void index_paths(const node& n, const std::string& path, std::unordered_map<const node*, std::string>& out) {
  out[&n] = path;
  for (size_t i = 0; i < n.children.size(); ++i) {
    index_paths(n.children[i], path + "/children/" + std::to_string(i), out);
  }
}

std::string join(const std::vector<std::string>& names) {
  std::string out;
  for (const std::string& n : names) {
    out += (out.empty() ? "" : ", ") + n;
  }
  return out;
}

} // namespace

fonts_needed::fonts_needed(std::vector<std::string> fonts)
    : std::runtime_error("fonts needed: " + join(fonts)), fonts_(std::move(fonts)) {}

// ── impl ─────────────────────────────────────────────────────────────────────

struct live_view::impl {
  view_options options;
  view_size size;
  std::string theme_override;

  std::unique_ptr<figure> fig;
  std::string source;
  std::unordered_map<const node*, std::string> paths;
  std::vector<plot_view> views; ///< Of the last frame.
  std::optional<view_pick> hovered;
  std::optional<view_pick> click;

  // The ImGui side; recreated by reset_contexts().
  std::unique_ptr<imgui_contexts> contexts;
  image_options frame_options; ///< What draw_context sees; updated in place.
  std::unique_ptr<draw_context> ctx;
  texture_store textures;
  std::string applied_theme; ///< Theme the contexts are configured with.
  ImU32 clear = IM_COL32(255, 255, 255, 255);

  view_geometry geom;
  bool overflowed = false; ///< The fit overflowed at the current size; laid out at the source size.
  int pending_frames = 0;
  int fresh_frames = 0; ///< Frames run since the contexts were created.
  std::optional<double> last_time;

  explicit impl(view_options o) : options(o) {
    reset_contexts();
  }

  const std::string& effective_theme() const {
    static const std::string kLight = "light";
    if (!theme_override.empty()) {
      return theme_override;
    }
    return fig ? fig->image.theme : kLight;
  }

  ImGuiBackendFlags backend_flags() const {
    return options.cpu_textures ? (ImGuiBackendFlags_RendererHasTextures | ImGuiBackendFlags_RendererHasVtxOffset) : 0;
  }

  /// Fresh contexts: drops every view state.
  void reset_contexts() {
    ctx.reset();
    contexts = std::make_unique<imgui_contexts>();
    textures = texture_store();
    ctx = std::make_unique<draw_context>(frame_options);
    ctx->set_interactive(true);
    ctx->set_tooltips(options.tooltips);
    const context_switch on(*contexts);
    applied_theme = effective_theme();
    configure(*find_theme(applied_theme), "nymph_live_view", backend_flags());
    load_fonts(*ctx);
    update_clear_color();
    last_time.reset();
    fresh_frames = 0;
  }

  /// Re-applies the theme's styles if it changed; call with the contexts
  /// current.
  void apply_theme() {
    if (applied_theme == effective_theme()) {
      return;
    }
    applied_theme = effective_theme();
    configure(*find_theme(applied_theme), "nymph_live_view", backend_flags());
    update_clear_color();
  }

  void update_clear_color() {
    ImVec4 bg = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
    bg.w = 1.0f; // the image is opaque
    clear = ImGui::ColorConvertFloat4ToU32(bg);
  }

  /// The geometry at the source size, shrunk with CSS to the box width.
  view_geometry source_geometry() const {
    view_geometry g;
    g.layout_width = static_cast<float>(fig->image.width);
    g.layout_height = static_cast<float>(fig->image.height);
    g.css_scale = size.width > 0.0f ? std::min(1.0f, size.width / g.layout_width) : 1.0f;
    return g;
  }

  /// Layout size and scales for the current figure, box and fit; sets
  /// frame_options to match.
  void update_geometry() {
    if (!fig) {
      return;
    }
    view_geometry g = source_geometry();
    bool fallback = overflowed;
    if (!overflowed && options.fit != view_fit::source && size.width > 0.0f) {
      view_geometry fit;
      fit.layout_width = std::round(size.width);
      if (options.fit == view_fit::width) {
        fit.layout_height = std::round(g.layout_height * size.width / g.layout_width);
      } else {
        fit.layout_height = size.height > 0.0f ? std::round(size.height) : g.layout_height;
      }
      image_options candidate = fig->image;
      candidate.width = static_cast<int>(fit.layout_width);
      candidate.height = static_cast<int>(fit.layout_height);
      candidate.scale = 1;
      try {
        check_options(candidate);
        g = fit;
      } catch (const std::invalid_argument&) {
        fallback = true; // a box the image options cannot describe
      }
    }
    const float ratio = std::clamp(size.pixel_ratio, 0.25f, kMaxPixelRatio);
    g.framebuffer_scale = std::min({g.css_scale * ratio, kMaxPixels / g.layout_width, kMaxPixels / g.layout_height});
    g.pixel_width = static_cast<int>(std::lround(g.layout_width * g.framebuffer_scale));
    g.pixel_height = static_cast<int>(std::lround(g.layout_height * g.framebuffer_scale));
    g.fit_fallback = fallback;
    geom = g;

    frame_options = fig->image;
    frame_options.width = static_cast<int>(g.layout_width);
    frame_options.height = static_cast<int>(g.layout_height);
    frame_options.scale = 1; // the framebuffer scale goes to ImGui directly
    frame_options.theme = effective_theme();
  }

  /// One frame in the current contexts.
  void run_frame(double now) {
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(geom.layout_width, geom.layout_height);
    io.DisplayFramebufferScale = ImVec2(geom.framebuffer_scale, geom.framebuffer_scale);
    const double step = last_time ? now - *last_time : 1.0 / 60.0;
    io.DeltaTime = static_cast<float>(std::clamp(step, 1e-4, 0.25));
    last_time = now;
    ctx->begin_frame();
    // Source limits stay fixed while fresh contexts settle (static parity
    // with render_figure()); then they let go for pan and zoom.
    ctx->set_lock_limits(fresh_frames < kFigureFrames);
    ++fresh_frames;
    const node& root = fig->root;
    run_figure_frame(frame_options, *ctx, [&](draw_context& dc) {
      layout_root(root, dc);
      dc.draw_node(root);
    });
    if (options.cpu_textures) {
      textures.apply_all(*ImGui::GetDrawData());
    }
    hovered.reset();
    if (const std::optional<pick>& p = ctx->hovered()) {
      view_pick v;
      const auto it = paths.find(p->element);
      v.element = it == paths.end() ? std::string() : it->second;
      v.type = p->type;
      v.label = p->label;
      v.index = p->index;
      v.values = p->values;
      hovered = std::move(v);
    }
    // A click: the primary button released where it was pressed.
    const float slop = io.MouseDragThreshold;
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) &&
        io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Left] <= slop * slop && hovered) {
      click = hovered;
    }
    views.clear();
    const float s = geom.css_scale;
    for (const plot_view_record& r : ctx->plot_views()) {
      plot_view v;
      const auto it = paths.find(r.element);
      v.element = it == paths.end() ? std::string() : it->second;
      v.x = r.area.min.x * s;
      v.y = r.area.min.y * s;
      v.width = r.area.width() * s;
      v.height = r.area.height() * s;
      v.x_min = r.x_min;
      v.x_max = r.x_max;
      v.y_min = r.y_min;
      v.y_max = r.y_max;
      v.has_y2 = r.has_y2;
      v.y2_min = r.y2_min;
      v.y2_max = r.y2_max;
      views.push_back(std::move(v));
    }
  }
};

// ── live_view ────────────────────────────────────────────────────────────────

live_view::live_view(view_options options) : impl_(std::make_unique<impl>(options)) {}
live_view::~live_view() = default;
live_view::live_view(live_view&&) noexcept = default;
live_view& live_view::operator=(live_view&&) noexcept = default;

void live_view::set_source(std::string_view source, bool reset_view) {
  impl& m = *impl_;
  figure f = bind_source(source, builtin_registry());
  if (std::vector<std::string> missing = fonts_missing(f); !missing.empty()) {
    throw fonts_needed(std::move(missing));
  }
  // A layout that does not fit the source's own size is an error of the
  // source; checked in separate contexts so the view state survives it.
  (void)layout_figure(f);

  m.fig = std::make_unique<figure>(std::move(f));
  m.source = std::string(source);
  m.paths.clear();
  index_paths(m.fig->root, "root", m.paths);
  m.views.clear();
  m.hovered.reset();
  m.click.reset();
  m.overflowed = false;
  if (reset_view) {
    m.reset_contexts();
  } else {
    const context_switch on(*m.contexts);
    load_fonts(*m.ctx); // fonts registered since the last update
    m.apply_theme();
  }
  m.update_geometry();
  m.pending_frames = kFigureFrames;
}

void live_view::set_data(std::string_view data_yaml) {
  impl& m = *impl_;
  if (!m.fig) {
    throw std::logic_error("set_data: no figure to replace the data of");
  }
  const data_replacement r = replace_data(m.source, data_yaml);
  try {
    set_source(r.source);
  } catch (const error& e) {
    if (e.line() >= r.first_line && e.line() < r.first_line + r.line_count) {
      // In the new data: say where in what the caller passed.
      throw error(e.line() - r.first_line + 1, std::max(1, e.column() - 2), std::string("data: ") + e.what());
    }
    throw;
  }
}

bool live_view::set_png(std::span<const uint8_t> png, bool reset_view) {
  impl& m = *impl_;
  const std::optional<std::string> source = extract_source(png);
  if (!source) {
    m.fig.reset();
    m.source.clear();
    m.paths.clear();
    m.views.clear();
    m.hovered.reset();
    m.click.reset();
    m.pending_frames = 0;
    return false;
  }
  set_source(*source, reset_view);
  return true;
}

void live_view::set_theme(std::string theme) {
  impl& m = *impl_;
  if (!theme.empty() && find_theme(theme) == nullptr) {
    throw std::invalid_argument("unknown theme '" + theme + "' (themes: light, dark, classic)");
  }
  m.theme_override = std::move(theme);
  {
    const context_switch on(*m.contexts);
    m.apply_theme();
  }
  m.update_geometry();
  m.pending_frames = kFigureFrames;
}

void live_view::resize(view_size size) {
  impl& m = *impl_;
  m.size = size;
  m.overflowed = false;
  m.update_geometry();
  m.pending_frames = kFigureFrames;
}

void live_view::input(const pointer_event& e) {
  impl& m = *impl_;
  const context_switch on(*m.contexts);
  ImGuiIO& io = ImGui::GetIO();
  io.AddMouseSourceEvent(e.touch ? ImGuiMouseSource_TouchScreen : ImGuiMouseSource_Mouse);
  const float s = m.geom.css_scale > 0.0f ? m.geom.css_scale : 1.0f;
  if (e.type == pointer_event::kind::leave) {
    io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
  } else {
    io.AddMousePosEvent(e.x / s, e.y / s);
  }
  if (e.type == pointer_event::kind::down || e.type == pointer_event::kind::up) {
    if (e.button >= 0 && e.button < ImGuiMouseButton_COUNT) {
      io.AddMouseButtonEvent(e.button, e.type == pointer_event::kind::down);
    }
  } else if (e.type == pointer_event::kind::wheel) {
    io.AddMouseWheelEvent(e.wheel_x, e.wheel_y);
  }
  m.pending_frames = std::max(m.pending_frames, kInputFrames);
}

void live_view::reset_view() {
  impl& m = *impl_;
  m.reset_contexts();
  m.views.clear();
  m.update_geometry();
  m.pending_frames = kFigureFrames;
}

std::vector<plot_view> live_view::view_state() const {
  return impl_->views;
}

const std::optional<view_pick>& live_view::hovered() const {
  return impl_->hovered;
}

std::optional<view_pick> live_view::take_click() {
  std::optional<view_pick> out = std::move(impl_->click);
  impl_->click.reset();
  return out;
}

const ImDrawData* live_view::frame(double now_seconds) {
  impl& m = *impl_;
  if (!m.fig || m.pending_frames <= 0) {
    return nullptr;
  }
  try {
    const context_switch on(*m.contexts);
    m.run_frame(now_seconds);
    --m.pending_frames;
    return ImGui::GetDrawData();
  } catch (const error&) {
    // The frame is left open: these contexts cannot be used again.
    m.reset_contexts();
    if (m.geom.layout_width == static_cast<float>(m.fig->image.width) &&
        m.geom.layout_height == static_cast<float>(m.fig->image.height)) {
      throw; // fails at the source size too (set_source() checked it, so a bug)
    }
    // The fit overflowed at this box size: lay out at the source size.
    m.overflowed = true;
    m.update_geometry();
    m.pending_frames = kFigureFrames;
  }
  const context_switch on(*m.contexts);
  m.run_frame(now_seconds);
  --m.pending_frames;
  return ImGui::GetDrawData();
}

bool live_view::wants_frame() const {
  return impl_->fig != nullptr && impl_->pending_frames > 0;
}

ImU32 live_view::clear_color() const {
  return impl_->clear;
}

view_geometry live_view::geometry() const {
  return impl_->geom;
}

bool live_view::has_figure() const {
  return impl_->fig != nullptr;
}

const std::string& live_view::description() const {
  static const std::string kEmpty;
  return impl_->fig ? impl_->fig->description : kEmpty;
}

std::string_view live_view::source() const {
  return impl_->source;
}

const texture_store& live_view::textures() const {
  return impl_->textures;
}

} // namespace bdg::nymph
