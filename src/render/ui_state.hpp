// MIT License © 2026 Binary Dice Games
/// @file ui_state.hpp
/// @brief The state of user-interface elements (open, selected, values,
///        window geometry), kept by element path across the frames of a
///        live view (spec/09_ui_mockups/DESIGN.md section 5, `ui_state`).
#pragma once

#include <imgui.h>
#include <array>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
#include "schema/value.hpp"

namespace bdg::nymph {

/// @brief What ImGui edits through pointers for one element, and what its
///        state started from. Each widget uses the members it needs.
struct ui_entry {
  std::string type; ///< Class name when the entry was made.
  value source; ///< The source value(s) the state was initialised from.
  bool touched = false; ///< Changed by the user since it was initialised.

  bool flag = false; ///< Checkbox value, open, selected, active, checked.
  std::array<float, 4> floats{}; ///< Float sliders, drags, inputs; colours (RGBA).
  std::array<int, 4> ints{}; ///< Integer sliders, drags, inputs.
  std::array<double, 4> doubles{}; ///< InputDouble.
  std::string text; ///< InputText buffer; a colour widget's value as "#RRGGBB[AA]".
  int index = -1; ///< Combo / ListBox selection.
  std::vector<double> numbers; ///< A numeric widget's value as shown (degrees for SliderAngle).

  // Windows and child regions.
  ImVec2 pos{0.0f, 0.0f}; ///< Relative to the Screen.
  ImVec2 size{0.0f, 0.0f}; ///< As drawn (a collapsed window: its title bar).
  ImVec2 size_full{0.0f, 0.0f}; ///< When expanded (what a resize changes and the view saves).
  ImVec2 scroll{0.0f, 0.0f};
  bool collapsed = false;
  bool closed = false;
  // What the user changed of a window (saved view): only these are saved.
  bool moved = false;
  bool resized = false;
  bool scrolled = false;
  bool collapse_changed = false;
};

/// @brief One frame's facts about the Screen being drawn and the windows
///        and popups in it.
struct ui_frame {
  bool in_screen = false; ///< Inside a Screen's draw().
  ImVec2 screen_min{0.0f, 0.0f}; ///< The Screen's rectangle, logical pixels.
  ImVec2 screen_max{0.0f, 0.0f};
  int cascade = 0; ///< Windows placed by cascade so far.
  std::vector<std::string> popups_to_open; ///< Popup `id`s that `opens` fields asked for this frame.
  /// The last widget drawn this frame that `opens` each popup id: its item
  /// rectangle, where the popup opens.
  std::unordered_map<std::string, std::pair<ImVec2, ImVec2>> openers;
  ImU32 screen_background = 0; ///< The Screen's background colour (WindowTransition label panels).
  std::unordered_map<std::string, std::string> window_paths; ///< Window `id` -> element path, this Screen.
  /// WindowTransition path -> {its lane, lanes between the same two windows}.
  std::unordered_map<std::string, std::pair<int, int>> lanes;
};

/// @brief UI element state by element path, plus the per-frame checks.
class ui_state {
 public:
  /// @brief The entry of `path`, created on first use.
  /// @param type    The element's class name; a different type replaces the
  ///                entry.
  /// @param source  The source value(s) the state starts from.
  /// @param settle  Whether this frame applies the source unconditionally
  ///                (PNG rendering, and a live view's settling frames).
  /// @param[out] apply True when the caller must (re)initialise the entry
  ///                from the source: `settle`, a new entry, a new type, or
  ///                a source value that changed since it was applied.
  ui_entry& entry(const std::string& path, std::string_view type, const value& source, bool settle, bool& apply) {
    auto [it, made] = entries_.try_emplace(path);
    ui_entry& e = it->second;
    apply = settle || made || e.type != type || !(e.source == source);
    if (apply) {
      if (e.type != type) {
        e = ui_entry{};
      }
      e.type = std::string(type);
      e.source = source;
      e.touched = e.moved = e.resized = e.scrolled = e.collapse_changed = false;
    }
    return e;
  }

  /// @brief The entry of `path`, or nullptr.
  const ui_entry* find(const std::string& path) const {
    const auto it = entries_.find(path);
    return it == entries_.end() ? nullptr : &it->second;
  }

  /// @brief Every entry, by path.
  const std::unordered_map<std::string, ui_entry>& entries() const {
    return entries_;
  }

  /// @brief Every entry, to change several at once (a RadioButton clears
  ///        its siblings).
  std::unordered_map<std::string, ui_entry>& mutable_entries() {
    return entries_;
  }

  /// @brief Forgets every entry (fresh contexts).
  void clear() {
    entries_.clear();
  }

  /// @brief Called at the start of every frame.
  void begin_frame() {
    frame_ = ui_frame{};
    ids_.clear();
  }

  ui_frame& frame() {
    return frame_;
  }
  const ui_frame& frame() const {
    return frame_;
  }

  /// @brief Turns the unique-ID check on (tests): note_id() then throws on
  ///        a repeat (spec/09_ui_mockups/DESIGN.md section 3, "Unique IDs").
  void set_check_ids(bool on) {
    check_ids_ = on;
  }
  bool check_ids() const {
    return check_ids_;
  }

  /// @brief Records that the element at `path` submitted ImGui item `id`
  ///        this frame (0 is ignored: items without an ID).
  /// @throws std::logic_error when the check is on and another element
  ///         submitted the same ID this frame.
  void note_id(ImGuiID id, const std::string& path) {
    if (!check_ids_ || id == 0) {
      return;
    }
    const auto [it, added] = ids_.try_emplace(id, path);
    if (!added) {
      throw std::logic_error("ImGui ID conflict: " + it->second + " and " + path + " have the same ID");
    }
  }

 private:
  std::unordered_map<std::string, ui_entry> entries_;
  ui_frame frame_;
  bool check_ids_ = false;
  std::unordered_map<ImGuiID, std::string> ids_;
};

} // namespace bdg::nymph
