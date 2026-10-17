// MIT License © 2026 Binary Dice Games
/// @file draw_context.hpp
/// @brief What a drawing function may use while a figure frame is open.
#pragma once

#include <imgui.h>
#include "render/image_options.hpp"

namespace bdg::nymph {

/// @brief An axis-aligned rectangle in logical pixels (ImGui screen space).
struct rect {
  ImVec2 min;
  ImVec2 max;
  float width() const {
    return max.x - min.x;
  }
  float height() const {
    return max.y - min.y;
  }
};

/// @brief Handed to the drawing function once per frame, inside the
///        figure's full-display ImGui window, with the private ImGui, ImPlot
///        and ImPlot3D contexts current.
///
/// Element drawing (`draw_children`, per-node rectangles) is added by the
/// element and layout steps of the plan; this is the frame-level part.
class draw_context {
 public:
  explicit draw_context(const image_options& options) : options_(options) {}

  /// @brief The options the frame is rendered with.
  const image_options& options() const {
    return options_;
  }

  /// @brief Output pixels per logical pixel (`image.scale`).
  float scale() const {
    return static_cast<float>(options_.scale);
  }

  /// @brief The image minus `image.padding` on every side: where the root
  ///        element goes.
  rect content_rect() const {
    const float pad = static_cast<float>(options_.padding);
    return {
        ImVec2(pad, pad), ImVec2(static_cast<float>(options_.width) - pad, static_cast<float>(options_.height) - pad)};
  }

 private:
  const image_options& options_;
};

} // namespace bdg::nymph
