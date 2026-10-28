// MIT License © 2026 Binary Dice Games
/// @file number_box.cpp
/// @brief The editor's number box (see number_box.hpp).
#include "edit/number_box.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "edit/field_value.hpp"

namespace bdg::nymph {

double number_box::drag_step(double start, const number_box_options& options) {
  const double span = options.min && options.max ? *options.max - *options.min : 0.0;
  const double basis = span > 0.0 && span <= 1e5 ? span / 200.0 : std::max(std::fabs(start), 1.0) / 200.0;
  double step = std::pow(10.0, std::round(std::log10(basis)));
  if (span > 0.0 && span <= 1e5) {
    // A wide range (a width of -1000..8192) must not jump by tens per
    // pixel: at most one unit per pixel; Ctrl drags ten times faster.
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

std::string number_box::format(double value, bool integer) {
  if (integer) {
    char text[32];
    std::snprintf(text, sizeof(text), "%lld", static_cast<long long>(std::llround(value)));
    return text;
  }
  return encode_number(value);
}

std::optional<double> number_box::parse(const std::string& text) {
  std::string s = text;
  std::replace(s.begin(), s.end(), ',', '.');
  const size_t a = s.find_first_not_of(" \t");
  if (a == std::string::npos) {
    return std::nullopt;
  }
  s = s.substr(a, s.find_last_not_of(" \t") - a + 1);
  char* end = nullptr;
  const double v = std::strtod(s.c_str(), &end);
  if (end != s.c_str() + s.size() || !std::isfinite(v)) {
    return std::nullopt;
  }
  return v;
}

number_box_change number_box::draw(const char* id, double& value, const number_box_options& options, float width) {
  number_box_change change = number_box_change::none;
  ImGui::PushID(id);
  const ImGuiID me = ImGui::GetID("##box");
  const float w = width > 0.0f ? width : ImGui::CalcItemWidth();
  const ImGuiStyle& style = ImGui::GetStyle();
  if (typing_id_ != 0 && ImGui::GetFrameCount() - typing_frame_ > 1) {
    typing_id_ = 0; // its box was not drawn last frame: gone
  }

  // The text field, once a click asked for typing: a text field until it
  // lets go of the keyboard. Drawn at `at`.
  const auto type_into = [&](const ImVec2& at) {
    ImGui::SetCursorScreenPos(at);
    typing_frame_ = ImGui::GetFrameCount();
    // Typing: a text field until it lets go of the keyboard.
    if (focus_) {
      ImGui::SetKeyboardFocusHere();
      focus_ = false;
    }
    ImGui::SetNextItemWidth(w);
    ImGui::InputText(
        "##box", buffer_, sizeof(buffer_), ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_CharsScientific);
    // Never stuck: a field that did not take the keyboard ends typing.
    const bool lost = !ImGui::IsItemActive() && ImGui::GetFrameCount() > typing_start_ + 2;
    if (ImGui::IsItemDeactivated() || lost) {
      typing_id_ = 0;
      if (!ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        if (const std::optional<double> typed = parse(buffer_)) {
          const double v = tidy(*typed, options);
          if (v != value) {
            value = v;
            change = number_box_change::typed;
          }
        }
      }
    }
  };

  if (typing_id_ == me) {
    type_into(ImGui::GetCursorScreenPos());
    ImGui::PopID();
    return change;
  }

  // The box: the value centred, arrows at the sides while hovered.
  const ImVec2 size(std::max(w, ImGui::GetFrameHeight()), ImGui::GetFrameHeight());
  const ImVec2 lo = ImGui::GetCursorScreenPos();
  const ImVec2 hi(lo.x + size.x, lo.y + size.y);
  ImGui::InvisibleButton("##box", size);
  const bool hovered = ImGui::IsItemHovered();
  const bool active = ImGui::IsItemActive();
  if (hovered || active) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
  }
  const ImGuiIO& io = ImGui::GetIO();
  if (ImGui::IsItemActivated()) {
    drag_id_ = me;
    dragged_ = false;
    drag_start_ = value == 0.0 && options.zero_text != nullptr ? options.zero_start : value;
    drag_pixels_ = 0.0;
  }
  if (active && drag_id_ == me) {
    const double scale = io.KeyShift ? 0.1 : io.KeyCtrl ? 10.0 : 1.0;
    if (!dragged_ && ImGui::IsMouseDragging(ImGuiMouseButton_Left, io.MouseDragThreshold)) {
      dragged_ = true;
      drag_pixels_ = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left).x * scale;
    } else if (dragged_) {
      drag_pixels_ += io.MouseDelta.x * scale;
    }
    if (dragged_) {
      const double step = options.step > 0.0 ? options.step : drag_step(drag_start_, options);
      const double v = tidy(drag_start_ + drag_pixels_ * step, options, step * std::min(scale, 1.0));
      if (v != value) {
        value = v;
        change = number_box_change::dragged;
      }
    }
  }
  if (ImGui::IsItemDeactivated() && drag_id_ == me) {
    drag_id_ = 0;
    if (!dragged_ && hovered) { // a click: type a value
      typing_id_ = me;
      typing_frame_ = ImGui::GetFrameCount();
      focus_ = true;
      const bool blank = value == 0.0 && options.zero_text != nullptr;
      std::snprintf(buffer_, sizeof(buffer_), "%s", blank ? "" : format(value, options.integer).c_str());
      typing_start_ = ImGui::GetFrameCount();
    }
  }

  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImGuiCol bg = active ? ImGuiCol_FrameBgActive : hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg;
  dl->AddRectFilled(lo, hi, ImGui::GetColorU32(bg), style.FrameRounding);
  if (style.FrameBorderSize > 0.0f) {
    dl->AddRect(lo, hi, ImGui::GetColorU32(ImGuiCol_Border), style.FrameRounding, 0, style.FrameBorderSize);
  }
  const bool zero_shown = value == 0.0 && options.zero_text != nullptr;
  const std::string text = zero_shown ? options.zero_text : format(value, options.integer);
  const ImVec2 text_size = ImGui::CalcTextSize(text.c_str());
  const float mid_y = (lo.y + hi.y) * 0.5f;
  dl->PushClipRect(lo, hi, true);
  dl->AddText(
      ImVec2(std::max(lo.x + style.FramePadding.x, (lo.x + hi.x - text_size.x) * 0.5f), mid_y - text_size.y * 0.5f),
      ImGui::GetColorU32(zero_shown ? ImGuiCol_TextDisabled : ImGuiCol_Text),
      text.c_str());
  if (hovered || active) {
    // Arrows at the sides: drag sideways.
    const float a = std::max(3.0f, size.y * 0.16f);
    const float inset = std::max(4.0f, style.FramePadding.x * 0.6f);
    const ImU32 c = ImGui::GetColorU32(ImGuiCol_TextDisabled);
    dl->AddTriangleFilled(
        ImVec2(lo.x + inset, mid_y), ImVec2(lo.x + inset + a, mid_y - a), ImVec2(lo.x + inset + a, mid_y + a), c);
    dl->AddTriangleFilled(
        ImVec2(hi.x - inset, mid_y), ImVec2(hi.x - inset - a, mid_y + a), ImVec2(hi.x - inset - a, mid_y - a), c);
  }
  dl->PopClipRect();
  if (hovered && !active) {
    ImGui::SetItemTooltip("Click to type a value, drag sideways to change it\n(Shift: finer, Ctrl: coarser)");
  }
  ImGui::PopID();
  return change;
}

} // namespace bdg::nymph
