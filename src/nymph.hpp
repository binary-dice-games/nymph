// MIT License © 2026 Binary Dice Games
/// @file nymph.hpp
/// @brief nymph's public library API: a YAML chart source to a PNG that
///        carries it, and back.
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "core/error.hpp"

namespace bdg::nymph {

/// @brief Returns the nymph library version as "MAJOR.MINOR.PATCH".
std::string_view version() noexcept;

/// @brief Validates and renders a source with the built-in elements.
/// @return PNG bytes with `source` embedded verbatim (and its description).
///         The same source and build give the same bytes.
/// @throws nymph::error for a problem in the source (positioned);
///         std::runtime_error for a rendering or encoding failure.
std::vector<uint8_t> render_png(std::string_view source);

// ── Rasterizers (spec/02_opengl_rasterizer/DESIGN.md) ────────────────────────

/// @brief The rasterizers nymph can draw with.
enum class rasterizer {
  software, ///< nymph's CPU rasterizer: same bytes for the same source and build
  opengl, ///< OpenGL 3.3 on a GPU, loaded at run time; close to software, not byte-equal
};

/// @brief The name used on the command line: "software" or "opengl".
std::string_view rasterizer_name(rasterizer r) noexcept;

struct renderer_test_options;

/// @brief Renders sources with a chosen rasterizer, keeping an OpenGL
///        context between calls.
///
/// OpenGL problems never fail a render: the image is drawn by the software
/// rasterizer instead. Device creation failing, or a driver error during a
/// render, switches the renderer to software for good; an image larger
/// than the GPU allows falls back for that image only. Used by one thread
/// at a time (ImGui's current context is process-global). A moved-from
/// renderer may only be destroyed or assigned to.
class renderer {
 public:
  /// @param requested `opengl` opens OpenGL and creates its context now; on
  ///        failure the renderer uses `software` and fallback_reason() says
  ///        why. Never throws for an OpenGL problem.
  explicit renderer(rasterizer requested = rasterizer::software);
  ~renderer();
  renderer(renderer&&) noexcept;
  renderer& operator=(renderer&&) noexcept;
  renderer(const renderer&) = delete;
  renderer& operator=(const renderer&) = delete;

  /// @brief As the free render_png(), with this renderer's rasterizer.
  /// @throws as the free render_png(); never for an OpenGL problem.
  std::vector<uint8_t> render_png(std::string_view source);

  /// @brief The rasterizer asked for at construction.
  rasterizer requested() const noexcept;
  /// @brief The rasterizer the next render_png() will try.
  rasterizer active() const noexcept;
  /// @brief The rasterizer that drew the last image (active() before the
  ///        first render).
  rasterizer last_used() const noexcept;
  /// @brief Why OpenGL was not used the last time it was requested and not
  ///        used; empty when it never fell back.
  const std::string& fallback_reason() const noexcept;
  /// @brief "<vendor> <renderer>, OpenGL <version>" while active() is
  ///        opengl; empty otherwise.
  std::string device_name() const;

 private:
  struct impl;
  explicit renderer(std::unique_ptr<impl> p);
  friend renderer make_renderer(rasterizer requested, const renderer_test_options& options);
  std::unique_ptr<impl> impl_;
};

/// @brief Whether OpenGL can be used on this machine.
/// @return {true, device name} or {false, the reason it cannot}. Creates and
///         destroys an OpenGL device; never throws for an OpenGL problem.
std::pair<bool, std::string> probe_opengl();

/// @brief Validates a source without rendering it.
/// @throws nymph::error for the first problem found.
void check_source(std::string_view source);

/// @brief The source a nymph PNG carries, byte for byte.
/// @return nullopt if `png` is not a PNG or has no nymph source.
/// @throws std::runtime_error for a corrupt or truncated PNG.
std::optional<std::string> extract_source(std::span<const uint8_t> png);

} // namespace bdg::nymph
