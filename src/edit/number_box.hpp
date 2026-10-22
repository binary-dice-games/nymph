// MIT License © 2026 Binary Dice Games
/// @file number_box.hpp
/// @brief The editor's widget for a number: a box showing the value that
///        is typed into after a click, or changed by dragging the mouse
///        sideways over it (spec/08_view_editor/DESIGN.md section 3, field
///        widgets). Pure ImGui (public API only), so it runs headless.
#pragma once

#include <imgui.h>
#include <optional>
#include <string>

namespace bdg::nymph {

/// @brief How a number box reads, steps and limits its value.
struct number_box_options {
  bool integer = false; ///< Whole numbers only.
  std::optional<double> min; ///< Lowest value; none when unset.
  std::optional<double> max; ///< Highest value; none when unset.
  /// Change per pixel dragged; 0 picks one from the limits or the value
  /// (number_box::drag_step()).
  double step = 0.0;
  /// Shown instead of 0 when set ("auto" for a size that 0 inherits); the
  /// text field then starts empty, and an empty text changes nothing.
  const char* zero_text = nullptr;
  /// With zero_text: the value a drag from 0 starts at (the inherited one).
  double zero_start = 0.0;
};

/// @brief What a number box did this frame.
enum class number_box_change {
  none,
  dragged, ///< The value moves with a drag; more changes follow until the button is released.
  typed, ///< A typed value was accepted (Enter, Tab or a click elsewhere).
};

/// @brief A number box. One object serves any number of boxes (one is typed
///        into at a time); keep it alive across frames.
///
/// Click (press and release without moving) to type a value: Enter, Tab or
/// a click elsewhere accepts it, Escape cancels it. Drag sideways to change
/// the value; Shift drags ten times finer, Ctrl ten times coarser.
/// Values outside the limits are clamped.
class number_box {
 public:
  /// @brief Draws a box `width` pixels wide (<= 0: ImGui's next item width,
  ///        then the available width) with ID `id`.
  /// @param value The value shown; changed in place.
  /// @return What changed `value` this frame.
  number_box_change draw(const char* id, double& value, const number_box_options& options, float width = 0.0f);

  /// @brief Whether a box is being typed into (its text has the keyboard).
  bool typing() const {
    return typing_id_ != 0;
  }

  /// @brief The change per pixel a drag starting at `start` makes: a power
  ///        of ten near 1/200 of the limits' span (when the span is known
  ///        and below 1e5) or of the value's size, at least 0.1 for whole
  ///        numbers.
  static double drag_step(double start, const number_box_options& options);

  /// @brief `value` clamped to the limits, whole for integers, and rounded
  ///        to a multiple of `step` when `step` > 0.
  static double tidy(double value, const number_box_options& options, double step = 0.0);

  /// @brief The value as the box shows it: the shortest decimal for a
  ///        number, no fraction for an integer.
  static std::string format(double value, bool integer);

  /// @brief Reads a typed value ("12", " -1.5e3 ", "2,5" with a comma);
  ///        nullopt for anything else.
  static std::optional<double> parse(const std::string& text);

 private:
  ImGuiID typing_id_ = 0; ///< The box being typed into.
  int typing_frame_ = 0; ///< The last frame it was drawn in.
  bool focus_ = false; ///< Give the keyboard to the typed box this frame.
  char buffer_[64] = {}; ///< What is typed.
  ImGuiID drag_id_ = 0; ///< The box being pressed or dragged.
  bool dragged_ = false; ///< The press moved past the drag threshold.
  double drag_start_ = 0.0; ///< The value when the press began.
  double drag_pixels_ = 0.0; ///< Pixels moved since, scaled by the modifiers.
};

} // namespace bdg::nymph
