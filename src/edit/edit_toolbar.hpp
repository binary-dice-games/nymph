// MIT License © 2026 Binary Dice Games
/// @file edit_toolbar.hpp
/// @brief The editor's floating toolbar: a row of tools over the figure,
///        next to the selected element, for the fields changed most often
///        (shape, fill, line and text colours, font, text size, line style)
///        and for copying a style, clearing it, duplicating and deleting
///        (spec/08_view_editor/DESIGN.md section 3). Pure ImGui, no OpenGL,
///        so it runs against any ImGui context.
#pragma once

#include <imgui.h>
#include <array>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "edit/editor_model.hpp"
#include "edit/inspector.hpp"
#include "edit/number_box.hpp"
#include "edit/tree_menu.hpp"
#include "edit/workspace_layout.hpp"

struct ImUmlContext;
struct ImPlotContext;

namespace bdg::nymph {

/// @brief A tool of the toolbar.
enum class toolbar_tool {
  shape, ///< `shape`: a grid of shape pictures
  fill_color, ///< `fill_color`: a palette
  line_color, ///< `color`: a palette
  text_color, ///< `text_color`: a palette
  colormap, ///< `colormap` of the element, or of the plot a series without colours is in
  font, ///< `font`: a list of the embedded fonts
  font_size, ///< `font_size`: a number box
  line_weight, ///< `line_weight`: a number box
  line, ///< `line` (solid, dotted, thick): a list
  arrow, ///< `arrow` (which ends have heads): a list
  marker, ///< `marker`: a grid of marker pictures
  marker_size, ///< `marker_size`: a number box
  copy_style, ///< copy the style, then paste it on the next element selected
  clear_style, ///< remove the colours and the text style
  duplicate, ///< copy the element after itself
  remove, ///< delete the element
};

/// @brief The field a tool edits ("" for the actions).
const char* toolbar_field(toolbar_tool tool);

/// @brief The colours of the toolbar's palettes: three rows of six (greys,
///        strong colours, pale colours), as "#RRGGBB".
const std::array<const char*, 18>& toolbar_palette();

/// @brief The fields a copied style carries (colours, colour scheme, text
///        style, shape, line style, markers).
const std::vector<std::string>& style_fields();

/// @brief The floating toolbar. Keep one alive across frames.
class edit_toolbar {
 public:
  edit_toolbar();
  ~edit_toolbar();
  edit_toolbar(const edit_toolbar&) = delete;
  edit_toolbar& operator=(const edit_toolbar&) = delete;

  /// @brief The tools for the selected element, in toolbar order: those
  ///        whose field the element has and does not take from the data,
  ///        then the actions it allows. Empty without a selection.
  static std::vector<toolbar_tool> tools_for(const editor_model& model);

  /// @brief Where a `width` x `height` toolbar goes: centred over `anchor`
  ///        (the selection, nullopt when not drawn), above it when there is
  ///        room in `area`, else under it, else at the top of `area`; always
  ///        inside `area` when it fits.
  static layout_box place(const std::optional<layout_box>& anchor, const layout_box& area, float width, float height);

  /// @brief Draws the toolbar as its own ImGui window over `area` (the
  ///        figure, CSS pixels in the window) next to `anchor`, and applies
  ///        its changes to `model`. Draws nothing without a selection or
  ///        with an empty `area`.
  /// @param menu The tree's menu: duplicating and deleting go through it
  ///        (it asks before deleting an element others refer to).
  inspector_events
  draw(editor_model& model, tree_menu& menu, const layout_box& area, const std::optional<layout_box>& anchor);

  /// @brief Whether the toolbar drawn last covers (`x`, `y`): the pointer
  ///        there belongs to it, not to the figure.
  bool contains(float x, float y) const {
    return shown_ && rect_.contains(x, y);
  }
  /// @brief The toolbar's box in the last frame (empty when not drawn).
  const layout_box& rect() const {
    return rect_;
  }

  /// @brief The element whose `colormap` the colormap tool changes: the
  ///        selected one when it has the field, else the plot a series
  ///        without colours of its own is in (a pie chart); "" for none.
  static std::string colormap_owner(const editor_model& model);

  /// @brief Sets the colour scheme of colormap_owner() to `name` ("Deep",
  ///        "Default", ...), as one undo step.
  static commit_result set_colormap(editor_model& model, const std::string& name);

  /// @brief Copies the style of the selected element: the next element
  ///        selected takes it (paste_style()). False when nothing is
  ///        selected.
  bool copy_style(const editor_model& model);
  /// @brief Whether a copied style waits for an element to go to.
  bool style_copied() const {
    return clip_.has_value();
  }
  /// @brief Forgets the copied style.
  void cancel_copy() {
    clip_.reset();
  }
  /// @brief Gives the copied style to the selected element as one undo step
  ///        (the fields it has; a shape or line style only between elements
  ///        of one type) and forgets it.
  commit_result paste_style(editor_model& model);
  /// @brief Removes the colours and the text style of the selected element,
  ///        as one undo step.
  commit_result clear_style(editor_model& model);

 private:
  struct copied_style {
    std::string from; ///< The element copied from.
    std::string type; ///< Its type.
    /// Each style field it has: its one-line YAML, or nullopt when unwritten.
    std::vector<std::pair<std::string, std::optional<std::string>>> fields;
  };

  void note(const commit_result& r, inspector_events& events);
  void color_tool(editor_model& model, const editable_field& f, toolbar_tool tool, inspector_events& events);
  void shape_tool(editor_model& model, const editable_field& f, inspector_events& events);
  void choice_tool(editor_model& model, const editable_field& f, float width, inspector_events& events);
  void font_tool(editor_model& model, const editable_field& f, inspector_events& events);
  void number_tool(editor_model& model, const editable_field& f, inspector_events& events);
  void colormap_tool(editor_model& model, inspector_events& events);
  void marker_tool(editor_model& model, const editable_field& f, inspector_events& events);
  /// Draws colormap `value` (an ImPlot colormap, -1: the default) as
  /// stripes in [lo, hi).
  void draw_colormap(ImDrawList* dl, int64_t value, ImVec2 lo, ImVec2 hi);
  /// Draws shape `name` (an enumeration name) in [lo, hi); false when the
  /// library has no picture for it.
  bool draw_shape(ImDrawList* dl, const std::string& name, ImVec2 lo, ImVec2 hi, ImU32 line, ImU32 fill);

  layout_box rect_;
  bool shown_ = false;
  float width_ = 360.0f; ///< The toolbar's size in the last frame.
  float height_ = 44.0f;
  number_box numbers_;
  std::string drag_key_; ///< The field being dragged (a number, a custom colour): one undo step.
  std::optional<copied_style> clip_;
  ImUmlContext* shapes_ = nullptr; ///< Shape pictures, made on first use.
  ImPlotContext* colormaps_ = nullptr; ///< Colormap colours, made on first use.
  float custom_[4] = {0, 0, 0, 1}; ///< The custom colour being picked.
  std::string custom_key_; ///< Which field it is for.
};

} // namespace bdg::nymph
