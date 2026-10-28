// MIT License © 2026 Binary Dice Games
/// @file number_box.cpp
/// @brief The editor's number box (see number_box.hpp).
#include "edit/number_box.hpp"
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string_view>

namespace bdg::nymph {

double number_box::drag_step(double start, const number_box_options& options) {
  const double span = options.min && options.max ? *options.max - *options.min : 0.0;
  const double basis = span > 0.0 && span <= 1e5 ? span / 200.0 : std::max(std::fabs(start), 1.0) / 200.0;
  double step = std::pow(10.0, std::round(std::log10(basis)));
  if (span > 0.0 && span <= 1e5) {
    // A wide range (a width of -1000..8192) must not jump by tens per
    // pixel: at most one unit per pixel (Alt drags ten times faster).
    step = std::min(step, 1.0);
  }
  if (options.integer) {
    step = std::max(step, 0.1); // ten pixels per unit at the finest
  }
  return step;
}

double number_box::tidy(double value, const number_box_options& options, double step) {
  if (!std::isfinite(value)) {
    value = 0.0;
  }
  if (options.integer) {
    value = std::round(value);
  } else if (step > 0.0) {
    // A multiple of the step, written with the step's decimals so that
    // 0.1 * 3 reads 0.3, not 0.30000000000000004.
    const int decimals = std::clamp(static_cast<int>(-std::floor(std::log10(step) + 1e-9)), 0, 15);
    char text[64];
    std::snprintf(text, sizeof(text), "%.*f", decimals, std::round(value / step) * step);
    value = std::strtod(text, nullptr);
  }
  if (options.min) {
    value = std::max(value, *options.min);
  }
  if (options.max) {
    value = std::min(value, *options.max);
  }
  return value == 0.0 ? 0.0 : value; // no "-0"
}

number_box_change number_box::draw(const char* id, double& value, const number_box_options& options, float width) {
  ImGui::PushID(id);
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigDragClickToInputText = true; // a click without dragging types the value
  if (typing_id_ != 0 && ImGui::GetFrameCount() - typing_frame_ > 1) {
    typing_id_ = 0; // its box was not drawn last frame: gone
  }

  // A value of 0 that shows as text ("auto") is dragged from, and typed
  // over, its inherited value.
  const bool zero_shown = value == 0.0 && options.zero_text != nullptr;
  double v = zero_shown ? options.zero_start : value;
  const double before = v;
  const double step = options.step > 0.0 ? options.step : drag_step(v, options);
  std::string format = "%.0f";
  if (zero_shown) {
    format.clear();
    for (const char c : std::string_view(options.zero_text)) {
      format += c == '%' ? std::string("%%") : std::string(1, c);
    }
  } else if (!options.integer) {
    format = "%.10g"; // the shortest decimal, 0.3 not 0.30000000000000004
  }
  const double lowest = options.min.value_or(std::numeric_limits<double>::lowest());
  const double highest = options.max.value_or(std::numeric_limits<double>::max());
  if (width > 0.0f) {
    ImGui::SetNextItemWidth(width);
  }
  ImGui::DragScalar("##v", ImGuiDataType_Double, &v, static_cast<float>(step), &lowest, &highest, format.c_str(),
                    ImGuiSliderFlags_AlwaysClamp);
  const ImGuiID item = ImGui::GetItemID();
  if (ImGui::TempInputIsActive(item)) {
    typing_id_ = item;
    typing_frame_ = ImGui::GetFrameCount();
  } else if (typing_id_ == item) {
    typing_id_ = 0;
  }

  number_box_change change = number_box_change::none;
  if (v != before) {
    const bool dragged = io.MouseDown[0];
    // A drag lands on a multiple of its step; a typed value is kept as typed.
    const double tidy_value = tidy(v, options, dragged ? step * (io.KeyShift ? 0.1 : 1.0) : 0.0);
    if (tidy_value != value) {
      value = tidy_value;
      change = dragged ? number_box_change::dragged : number_box_change::typed;
    }
  }
  ImGui::PopID();
  return change;
}

} // namespace bdg::nymph
