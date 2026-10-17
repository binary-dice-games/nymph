// MIT License © 2026 Binary Dice Games
/// @file nymph.cpp
/// @brief Library-wide entry points.
#include "nymph.hpp"
#include "document/bind.hpp"
#include "document/legacy.hpp"
#include "elements/register_builtin.hpp"
#include "png/png_meta.hpp"
#include "render/figure_renderer.hpp"
#include "version.hpp"

namespace bdg::nymph {

std::string_view version() noexcept {
  return NYMPH_VERSION_STRING;
}

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
  std::optional<std::string> source = read_source(png);
  if (source) {
    // A wish nymph image: give back the equivalent nymph YAML.
    if (std::optional<std::string> converted = convert_wish_source(*source)) {
      return converted;
    }
  }
  return source;
}

} // namespace bdg::nymph
