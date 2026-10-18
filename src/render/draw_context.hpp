// MIT License © 2026 Binary Dice Games
/// @file draw_context.hpp
/// @brief What a drawing function may use while a figure frame is open.
#pragma once

#include <imgui.h>
#include <implot.h>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
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

/// @brief What a live view reads back about one 2D plot after a frame: its
///        plot area and axis ranges (spec/04_web_viewer/DESIGN.md
///        section 4, `view_state()`).
struct plot_view_record {
  const node* element = nullptr;
  rect area; ///< The plot area (inside the axes), logical pixels.
  double x_min = 0, x_max = 0;
  double y_min = 0, y_max = 0;
  bool has_y2 = false;
  double y2_min = 0, y2_max = 0;
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

  /// @brief Whether the frame belongs to a live view that takes pointer
  ///        input (spec/04_web_viewer/DESIGN.md section 4). False for
  ///        PNG rendering, where nothing interactive may change the image.
  bool interactive() const {
    return interactive_;
  }

  /// @brief Set by the live view; never by the PNG frame driver.
  void set_interactive(bool interactive) {
    interactive_ = interactive;
  }

  /// @brief How elements apply axis limits written in the source:
  ///        `ImPlotCond_Always` (fixed) for PNG rendering and for a live
  ///        view's first frames, `ImPlotCond_Once` afterwards so the user
  ///        can pan and zoom away from them. ImPlot treats an `Always` axis
  ///        as locked, which changes how it settles (an `Equal` plot then
  ///        adjusts its other axis), so the first frames must match the
  ///        PNG path exactly.
  ImPlotCond limits_cond() const {
    return (!interactive_ || lock_limits_) ? ImPlotCond_Always : ImPlotCond_Once;
  }

  /// @brief Set by the live view for the frames that settle fresh contexts.
  void set_lock_limits(bool lock) {
    lock_limits_ = lock;
  }

  /// @brief Called by the live view before each frame: forgets every
  ///        rectangle (the figure, and its node addresses, may have
  ///        changed) and keeps the last frame's plot views as previous.
  void begin_frame() {
    rects_.clear();
    previous_views_ = std::move(views_);
    views_.clear();
  }

  /// @brief In a live view, records the current ImPlot plot's area and
  ///        limits for `n`; call between BeginPlot and EndPlot, after its
  ///        items. Does nothing when not interactive.
  void record_current_plot(const node& n, bool with_y2);

  /// @brief The plot views recorded in this frame.
  const std::vector<plot_view_record>& plot_views() const {
    return views_;
  }

  /// @brief `n`'s plot view recorded in the previous frame, or nullptr.
  const plot_view_record* previous_plot_view(const node& n) const {
    for (const plot_view_record& v : previous_views_) {
      if (v.element == &n) {
        return &v;
      }
    }
    return nullptr;
  }

  /// @brief Output pixels per logical pixel (`image.scale`).
  float scale() const {
    return static_cast<float>(options_.scale);
  }

  /// @brief Draws each child of `n`, in order, through draw_node(), each
  ///        under its own ImGui ID (its index), so two series or plots with
  ///        the same label do not collide.
  void draw_children(const node& n);

  /// @brief Draws `n` through its class, with its text style applied
  ///        (render/text_style.hpp), so its children inherit it.
  void draw_node(const node& n);

  /// @brief Its class's measure(), with `n`'s text style applied.
  ImVec2 measure_node(const node& n, float width);

  /// @brief Its class's arrange(), with `n`'s text style applied.
  void arrange_node(const node& n, const rect& r);

  /// @brief Registers the loaded font called `name` (an embedded font file
  ///        name, or "default").
  void add_font(std::string name, ImFont* font) {
    fonts_[std::move(name)] = font;
  }

  /// @brief The font called `name`, or nullptr (keep the current font) if
  ///        none was registered.
  ImFont* font(std::string_view name) const {
    const auto it = fonts_.find(name);
    return it == fonts_.end() ? nullptr : it->second;
  }

  /// @brief The name `font` was registered under, or "" if it was not.
  std::string_view font_name(const ImFont* font) const {
    for (const auto& [name, f] : fonts_) {
      if (f == font) {
        return name;
      }
    }
    return {};
  }

  /// @brief Sets the rectangle `n` draws in.
  void set_rect(const node& n, const rect& r) {
    rects_[&n] = r;
  }

  /// @brief Every rectangle set so far.
  const std::unordered_map<const node*, rect>& rects() const {
    return rects_;
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
  bool interactive_ = false;
  bool lock_limits_ = false;
  std::vector<plot_view_record> views_;
  std::vector<plot_view_record> previous_views_;
  std::unordered_map<const node*, rect> rects_;
  std::map<std::string, ImFont*, std::less<>> fonts_;
};

} // namespace bdg::nymph
