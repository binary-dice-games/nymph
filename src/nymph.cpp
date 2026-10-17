// MIT License © 2026 Binary Dice Games
/// @file nymph.cpp
/// @brief Library-wide entry points and the renderer's fallback rules
///        (spec/02_opengl_rasterizer/DESIGN.md section 3, `renderer`).
#include "nymph.hpp"
#include "document/bind.hpp"
#include "elements/register_builtin.hpp"
#include "png/png_meta.hpp"
#include "render/figure_renderer.hpp"
#include "render/raster_backend.hpp"
#include "render/renderer_options.hpp"
#include "version.hpp"
#if NYMPH_HAS_OPENGL
#include "render/gl/gl_device.hpp"
#include "render/gl_rasterizer.hpp"
#endif

namespace bdg::nymph {

namespace {

constexpr const char* kNoOpenGLBuild = "nymph was built without OpenGL support";

} // namespace

std::string_view version() noexcept {
  return NYMPH_VERSION_STRING;
}

std::string_view rasterizer_name(rasterizer r) noexcept {
  return r == rasterizer::opengl ? "opengl" : "software";
}

// ── renderer ─────────────────────────────────────────────────────────────────

struct renderer::impl {
  rasterizer requested = rasterizer::software;
  rasterizer active = rasterizer::software;
  rasterizer last_used = rasterizer::software;
  std::string reason;
  software_backend software;
#if NYMPH_HAS_OPENGL
  std::unique_ptr<gl_device> device; // declared first: outlives the backend
  std::unique_ptr<gl_backend> backend;
#endif

  impl(rasterizer r, const renderer_test_options& options) : requested(r) {
    if (r != rasterizer::opengl) {
      return;
    }
#if NYMPH_HAS_OPENGL
    gl_device_options device_options;
    device_options.allow_software_gl = options.allow_software_gl;
    device_options.library_names = options.library_names;
    gl_device::result created = gl_device::create(device_options);
    if (!created.device) {
      reason = created.reason;
      return;
    }
    device = std::move(created.device);
    backend = std::make_unique<gl_backend>(*device);
    backend->limit_size(options.max_size);
    if (options.fail_first_render) {
      backend->fail_next_rasterize();
    }
    active = last_used = rasterizer::opengl;
#else
    (void)options;
    reason = kNoOpenGLBuild;
#endif
  }

  /// Drops OpenGL for good after a driver failure.
  void drop_opengl() {
#if NYMPH_HAS_OPENGL
    backend.reset();
    device.reset();
#endif
    active = rasterizer::software;
  }

  image render(const figure& f) {
#if NYMPH_HAS_OPENGL
    if (active == rasterizer::opengl) {
      try {
        image img = render_figure(f, *backend);
        last_used = rasterizer::opengl;
        return img;
      } catch (const raster_too_large& e) {
        reason = e.what();
      } catch (const raster_unavailable& e) {
        reason = e.what();
        drop_opengl();
      }
    }
#endif
    last_used = rasterizer::software;
    return render_figure(f, software);
  }
};

renderer::renderer(rasterizer requested) : impl_(std::make_unique<impl>(requested, renderer_test_options{})) {}

renderer::renderer(std::unique_ptr<impl> p) : impl_(std::move(p)) {}

renderer::~renderer() = default;
renderer::renderer(renderer&&) noexcept = default;
renderer& renderer::operator=(renderer&&) noexcept = default;

std::vector<uint8_t> renderer::render_png(std::string_view source) {
  const figure f = bind_source(source, builtin_registry());
  return encode_png(impl_->render(f), source, f.description);
}

rasterizer renderer::requested() const noexcept {
  return impl_->requested;
}

rasterizer renderer::active() const noexcept {
  return impl_->active;
}

rasterizer renderer::last_used() const noexcept {
  return impl_->last_used;
}

const std::string& renderer::fallback_reason() const noexcept {
  return impl_->reason;
}

std::string renderer::device_name() const {
#if NYMPH_HAS_OPENGL
  if (impl_->active == rasterizer::opengl && impl_->device) {
    return impl_->device->renderer_name();
  }
#endif
  return {};
}

renderer make_renderer(rasterizer requested, const renderer_test_options& options) {
  return renderer(std::make_unique<renderer::impl>(requested, options));
}

std::pair<bool, std::string> probe_opengl() {
#if NYMPH_HAS_OPENGL
  gl_device::result r = gl_device::create({});
  if (!r.device) {
    return {false, r.reason};
  }
  return {true, r.device->renderer_name()};
#else
  return {false, kNoOpenGLBuild};
#endif
}

// ── Free functions ───────────────────────────────────────────────────────────

std::vector<uint8_t> render_png(std::string_view source) {
  const figure f = bind_source(source, builtin_registry());
  return encode_png(render_figure(f), source, f.description);
}

void check_source(std::string_view source) {
  // Binding checks the content; the layout pass (it needs font metrics)
  // finds the one remaining error, a layout that does not fit.
  layout_figure(bind_source(source, builtin_registry()));
}

std::optional<std::string> extract_source(std::span<const uint8_t> png) {
  return read_source(png);
}

} // namespace bdg::nymph
