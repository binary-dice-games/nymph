// MIT License © 2026 Binary Dice Games
/// @file window_icon.hpp
/// @brief The icon of the `nymph view` window: `resources/icon.png`,
///        compiled into the executable, and its smaller sizes for the
///        platform window layers (internal; spec/04_web_viewer/DESIGN.md
///        section 10).
#pragma once

#include "render/image.hpp"

namespace bdg::nymph {

/// @brief nymph's icon at its full size (square, straight alpha), decoded
///        once from the embedded `resources/icon.png`.
/// @return An empty image if the embedded file cannot be decoded; the
///         window then keeps the system's default icon.
const image& window_icon();

/// @brief `icon` scaled to `size` x `size` pixels with an area-averaging
///        filter weighted by alpha (transparent pixels do not darken the
///        edges).
/// @return An empty image if `icon` is empty or `size` is not positive.
image scale_icon(const image& icon, int size);

} // namespace bdg::nymph
