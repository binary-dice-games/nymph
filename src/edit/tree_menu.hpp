// MIT License © 2026 Binary Dice Games
/// @file tree_menu.hpp
/// @brief The element tree's right-click menu and its dialogs: add, insert,
///        duplicate, move and delete elements, the New element dialog and
///        the Delete dialog for referenced elements
///        (spec/14_editor_workspace/DESIGN.md section 5). The choices are
///        computed apart from the drawing (child_types(), element_draft), so
///        they are tested without simulated clicks.
#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "edit/editor_model.hpp"
#include "schema/element_class.hpp"
#include "view/live_view.hpp"

namespace bdg::nymph {

struct inspector_events;

/// @brief Types offered by an "Add" menu, by the slot they fill.
struct type_group {
  std::string slot; ///< The parent's accepted slot ("flowchart.item").
  std::vector<const element_class*> types; ///< Sorted by name.
};

/// @brief The types `parent` takes, grouped by the slots it accepts, in its
///        slots' order; a type filling several of them is in the first.
std::vector<type_group> child_types(const element_class& parent);

/// @brief The chart types a root can get next to it (the `figure` slot).
std::vector<const element_class*> chart_types();

/// @brief Where a dragged tree row would land: `index` is the place in
///        `parent`'s children before the dragged element leaves it.
struct drop_place {
  std::string parent;
  size_t index = 0;
};

/// @brief Where dropping `dragged` on `row` puts it, by how far down the row
///        the pointer is (`fraction`, 0 top to 1 bottom): before it (top),
///        into it as the last child (middle), or after it (bottom; not for
///        a row `open` with visible children). Falls back to the other
///        places when the first does not take the element.
/// @return nullopt when no place takes it or nothing would change: the
///         parent does not accept the element's type, the place is the
///         element or inside it, the root is dragged or dropped beside.
std::optional<drop_place>
plan_drop(const editor_model& model, const std::string& dragged, const edit_element& row, float fraction, bool open);

/// @brief A new element being written in the New element dialog: its type,
///        where it goes, and each field's YAML text.
struct element_draft {
  std::string parent;
  size_t index = 0;
  std::string type;
  /// Field name and one-line YAML value, the starter's first (id, link
  /// ends, data), then the type's other fields, empty.
  std::vector<std::pair<std::string, std::string>> fields;
  size_t starter_count = 0; ///< The first fields, from the starter (shown first).
  /// The loader's message for the source with the element; "" when it binds.
  std::string error;

  /// @brief The draft of a `type` element in `parent` at `index`, its fields
  ///        from the starter as the model would make it.
  static element_draft make(const editor_model& model, std::string parent, size_t index, std::string type);

  /// @brief Checks the source with the element as the fields say: sets
  ///        `error`. @return Whether it binds.
  bool check(const editor_model& model);

  /// @brief The fields written (non-empty), for editor_model::add_element().
  std::vector<std::pair<std::string, std::string>> written() const;
};

/// @brief The tree's menu, its keyboard shortcuts and dialogs. Pure ImGui.
class tree_menu {
 public:
  /// @brief After a tree row's item: opens the row's menu on a right-click
  ///        (selecting the row) and draws it while open.
  void row(editor_model& model, const edit_element& e, inspector_events& events);

  /// @brief Opens the element's menu at (`x`, `y`) (window pixels): a
  ///        right-click on it in the figure.
  void open_at(std::string path, float x, float y);

  /// @brief Draws the dialogs; call once per frame, outside any row.
  void dialogs(editor_model& model, inspector_events& events);

  // ── Requests (menu items, keyboard shortcuts, tests) ─────────────────

  /// @brief Adds a `type` element to `parent` at `index`; when it does not
  ///        bind as started, opens the New element dialog instead.
  void add(editor_model& model, const std::string& parent, size_t index, const std::string& type, inspector_events& events);

  /// @brief Deletes the element at `path`; when other elements refer to it,
  ///        asks first (with or without them).
  void remove(editor_model& model, const std::string& path, inspector_events& events);

  /// @brief Copies the element at `path` after itself, selecting the copy.
  void duplicate(editor_model& model, const std::string& path, inspector_events& events);

  /// @brief Moves the element at `path` one place up (`delta` -1) or down (+1).
  void move(editor_model& model, const std::string& path, int delta, inspector_events& events);

  /// @brief Moves the element at `path` to `place` (see plan_drop()), the
  ///        selection following it.
  void drop(editor_model& model, const std::string& path, const drop_place& place, inspector_events& events);

  /// @brief The New element dialog's draft while it is open.
  const std::optional<element_draft>& draft() const {
    return draft_;
  }
  element_draft* draft() {
    return draft_ ? &*draft_ : nullptr;
  }
  /// @brief Inserts the draft (the dialog's Insert button).
  /// @return Whether it was inserted (the dialog closes).
  bool insert_draft(editor_model& model, inspector_events& events);

  /// @brief The element the Delete dialog asks about, and its referrers.
  const std::optional<std::pair<std::string, std::vector<std::string>>>& pending_delete() const {
    return pending_delete_;
  }
  /// @brief Answers the Delete dialog: with the referrers or alone.
  void confirm_delete(editor_model& model, bool with_references, inspector_events& events);

 private:
  void items(editor_model& model, const edit_element& e, inspector_events& events);
  void type_menu(
      editor_model& model,
      const char* label,
      const std::vector<type_group>& groups,
      const std::string& parent,
      size_t index,
      inspector_events& events);

  std::optional<element_draft> draft_;
  std::string figure_menu_; ///< The element whose menu a right-click in the figure opened.
  float figure_x_ = 0.0f;
  float figure_y_ = 0.0f;
  bool open_figure_menu_ = false;
  bool open_draft_ = false; ///< Open the New element dialog at the next dialogs().
  std::optional<std::pair<std::string, std::vector<std::string>>> pending_delete_;
  bool open_delete_ = false;
};

} // namespace bdg::nymph
