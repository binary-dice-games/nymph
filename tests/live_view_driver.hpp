// MIT License © 2026 Binary Dice Games
/// @file live_view_driver.hpp
/// @brief Test helper: a live_view driven with synthetic pointer events,
///        settling after each one.
#pragma once

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "view/live_view.hpp"

namespace bdg::nymph {

/// A view of `source` at its own size, settled.
struct driven_view {
  live_view v;
  double clock = 0.0;
  const ImDrawData* last = nullptr; ///< Draw data of the last frame run.

  explicit driven_view(const std::string& source, float width = 400, float height = 300, view_options options = {})
      : v(options) {
    v.set_source(source);
    v.resize({width, height, 1});
    run();
  }

  /// Runs frames until the view settles.
  void run() {
    for (int i = 0; i < 20 && v.wants_frame(); ++i) {
      last = v.frame(clock);
      clock += 1.0 / 60.0;
    }
  }

  /// Sends one event and runs its frames.
  void send(pointer_event e) {
    v.input(e);
    run();
  }

  void move(float x, float y) {
    send({pointer_event::kind::move, x, y});
  }

  void drag(float x0, float y0, float x1, float y1, int button = 0) {
    move(x0, y0);
    send({pointer_event::kind::down, x0, y0, button});
    for (int k = 1; k <= 4; ++k) {
      move(x0 + (x1 - x0) * k / 4.0f, y0 + (y1 - y0) * k / 4.0f);
    }
    send({pointer_event::kind::up, x1, y1, button});
  }

  void wheel(float x, float y, float notches) {
    move(x, y);
    pointer_event e{pointer_event::kind::wheel, x, y};
    e.wheel_y = notches;
    send(e);
  }

  /// CSS position of data point (x, y) in plot `p` (Y1 axis).
  static ImVec2 at(const plot_view& p, double x, double y) {
    return ImVec2(
        p.x + static_cast<float>((x - p.x_min) / (p.x_max - p.x_min)) * p.width,
        p.y + static_cast<float>((p.y_max - y) / (p.y_max - p.y_min)) * p.height);
  }

  void click(float x, float y) {
    move(x, y);
    send({pointer_event::kind::down, x, y, 0});
    send({pointer_event::kind::up, x, y, 0});
  }

  plot_view plot(size_t i = 0) const {
    const std::vector<plot_view> views = v.view_state();
    EXPECT_GT(views.size(), i);
    return views.size() > i ? views[i] : plot_view{};
  }
};

} // namespace bdg::nymph
