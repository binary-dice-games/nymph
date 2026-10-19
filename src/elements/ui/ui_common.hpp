// MIT License © 2026 Binary Dice Games
/// @file ui_common.hpp
/// @brief What the user-interface elements share: their slots, the widget
///        base class with the common fields, and helpers
///        (spec/09_ui_mockups/DESIGN.md sections 3 to 5).
#pragma once

#include <imgui.h>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "render/draw_context.hpp"
#include "schema/element_class.hpp"
#include "schema/field_desc.hpp"

namespace bdg::nymph::ui {

// ── Slots ────────────────────────────────────────────────────────────────────

inline constexpr std::string_view kWindowSlot = "ui.window"; ///< Children of a Screen.
inline constexpr std::string_view kFlowSlot = "ui.flow"; ///< WindowTransition, a child of a Screen.
inline constexpr std::string_view kWidgetSlot = "ui.widget"; ///< Children of windows and containers.
inline constexpr std::string_view kMenuBarSlot = "ui.menubar"; ///< MenuBar.
inline constexpr std::string_view kMenuSlot = "ui.menu"; ///< Menu, MenuItem.
inline constexpr std::string_view kTabSlot = "ui.tab"; ///< TabItem, TabItemButton.
inline constexpr std::string_view kTableSlot = "ui.table"; ///< TableColumn, TableRow.

/// @brief What a container of widgets accepts: widgets first, then figure
///        elements (drawn in the flow, DESIGN section 4).
std::vector<std::string> widget_container_slots();

/// @brief Most UI elements in one figure.
inline constexpr size_t kMaxUiElements = 10000;

/// @brief Largest InputText buffer, bytes.
inline constexpr size_t kMaxTextBuffer = 64 * 1024;

// ── Field helpers ────────────────────────────────────────────────────────────

/// @brief A text field holding a printf format for one integer: literal
///        text, `%%`, and at most one `%d`, `%i`, `%u`, `%x` or `%X`
///        conversion, width up to 99.
field_desc int_format_field(std::string name, std::string def, std::string description);

/// @brief The `"#RRGGBB[AA]"` colour `text` as ImGui float RGBA.
ImVec4 color_of(const std::string& text);

/// @brief Checks that every `opens` field under `n` names one of
///        `popup_ids` (the popups of its Screen; none outside a Screen).
/// @throws nymph::error at the first `opens` that names no popup.
void check_opens(const node& n, const std::vector<std::string>& popup_ids);

/// @brief Whether field `index` of `n` was written in the source.
inline bool written(const node& n, size_t index) {
  return index < n.field_pos.size() && n.field_pos[index].line > 0;
}

/// @brief Pushes ImGui's stock window style back (the figure window zeroes
///        padding, border and rounding, frame_setup.cpp), so windows look
///        as in an ImGui application. Pops on destruction.
class stock_window_style {
 public:
  stock_window_style();
  ~stock_window_style();
  stock_window_style(const stock_window_style&) = delete;
  stock_window_style& operator=(const stock_window_style&) = delete;
};

// ── Children ─────────────────────────────────────────────────────────────────

/// @brief Draws the children of a UI container in order, each under its
///        index as ID (as draw_context::draw_children()); widgets draw
///        themselves, figure elements are placed in ImGui's flow
///        (draw_figure_in_flow()).
void draw_children(const node& n, draw_context& dc);

/// @brief Draws a figure element (a Plot, a layout, a Label) at the cursor
///        of the current window (DESIGN section 4, "Figures in windows"):
///        a positive hint is pixels, a negative height fills the height
///        left, anything else across fills the width; never smaller than
///        its natural size (the window scrolls instead). Then advances the
///        cursor past it.
void draw_figure_in_flow(const node& n, draw_context& dc);

// ── Widgets ──────────────────────────────────────────────────────────────────

/// @brief What a widget reports after drawing its item.
struct item_result {
  bool clicked = false; ///< Pressed and released (opens popups, reports a click).
  /// Set by containers that called finish_item() themselves, right after
  /// their own item and before their children, and by widgets that submit
  /// no item of their own (SameLine, Spacing).
  bool finished = false;
  /// Whether the item has an ImGui ID of its own to check (a Group's last
  /// item ID may be a child's).
  bool has_id = true;
  /// The values shown in its pick ({"value", "0.8"}); empty for none.
  std::vector<std::pair<std::string, std::string>> values;
};

/// @brief Base of every widget (slot `ui.widget`): its own fields, then
///        the common ones (`width`, `disabled`, `tooltip`, `opens`) and the
///        text style. draw() wraps draw_item() with them, offers the pick,
///        records the edit target and checks the item's ID.
class widget_class : public element_class {
 public:
  /// @param accepts Slots of its children (containers), usually none.
  /// @param fields  Its own fields; the common ones are appended.
  /// @param item_width Whether the class takes the `width` field (items
  ///        with a frame); classes with their own size leave it out.
  widget_class(
      std::string name,
      std::vector<std::string> accepts,
      std::vector<field_desc> fields,
      std::string description,
      bool item_width = true);

  void draw(const node& n, draw_context& dc) const final;

  /// @brief The label shown in picks and the editor ("" for none).
  virtual std::string label_of(const node& n) const;

  view_kind viewable() const override {
    return view_kind_;
  }

 protected:
  /// @brief Draws the ImGui item(s) of `n`.
  virtual item_result draw_item(const node& n, draw_context& dc) const = 0;

  /// @brief After the item: notes its ID, sets its tooltip, offers its pick,
  ///        opens the popup it `opens` when clicked, records its edit
  ///        targets. Called by draw() unless draw_item() did it.
  void finish_item(const node& n, draw_context& dc, const item_result& r) const;

  /// @brief Index of common field `name` ("width", "disabled", ...).
  size_t common(std::string_view name) const;

  /// @brief The state entry of `n`, (re)initialised from `source` when
  ///        `apply` comes back true (see ui_state::entry()).
  static ui_entry& state(const node& n, draw_context& dc, const value& source, bool& apply);

  /// @brief `n`'s entry of the `view` key while the source applies
  ///        (dc.apply_view()), else nullptr: what the user left, saved.
  static const element_view* saved(const node& n, const draw_context& dc) {
    return dc.apply_view() ? dc.view_of(n) : nullptr;
  }

  /// @brief What the element saves in the `view` key (DESIGN section 4).
  void set_view_kind(view_kind kind) {
    view_kind_ = kind;
  }

 private:
  bool item_width_;
  view_kind view_kind_ = view_kind::none;
};

/// @brief "0.8", "3", "true": a short text for a pick value.
std::string pick_number(double v);

} // namespace bdg::nymph::ui

namespace bdg::nymph {

class registry;

/// @brief Registers every user-interface element (spec/09_ui_mockups).
void register_ui(registry& reg);

} // namespace bdg::nymph
