// MIT License © 2026 Binary Dice Games
/// @file draw_context.hpp
/// @brief What a drawing function may use while a figure frame is open.
#pragma once

#include <imgui.h>
#include <unordered_map>
#include "render/image_options.hpp"
#include "schema/node.hpp"

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
/// Each `figure` element draws in its rectangle (set_rect() / rect()): the
/// root gets content_rect(); the layout engine (plan step 11) sets the
/// rectangles of layout children.
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

  /// @brief Draws each child of `n`, in order, through its class's draw(),
  ///        each under its own ImGui ID (its index), so two series or plots
  ///        with the same label do not collide.
  void draw_children(const node& n);

  /// @brief Sets the rectangle `n` draws in.
  void set_rect(const node& n, const rect& r) {
    rects_[&n] = r;
  }

  /// @brief The rectangle `n` draws in; content_rect() if none was set.
  rect rect_of(const node& n) const {
    const auto it = rects_.find(&n);
    return it == rects_.end() ? content_rect() : it->second;
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
  std::unordered_map<const node*, rect> rects_;
};

} // namespace bdg::nymph
