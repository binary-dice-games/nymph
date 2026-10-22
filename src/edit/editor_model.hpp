// MIT License © 2026 Binary Dice Games
/// @file editor_model.hpp
/// @brief The state of the view editor: the source being edited, what is
///        selected, the editable fields of the selection, and undo / redo
///        (spec/08_view_editor/DESIGN.md section 3, `editor_model`).
///        Platform-neutral: no window, no OpenGL, no ImGui.
#pragma once

#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "document/bind.hpp"
#include "edit/data_edit.hpp"
#include "edit/tree_edit.hpp"
#include "schema/field_desc.hpp"
#include "schema/value.hpp"
#include "view/live_view.hpp"

namespace bdg::nymph {

/// @brief One field of the selected element as the inspector shows it.
struct editable_field {
  const field_desc* desc = nullptr; ///< Name, type, description, limits, names.
  value current; ///< What the element has (the default when not written).
  bool written = false; ///< The source writes the field (else it is at its default).
  /// Non-empty when the field's value comes from the data (`$names`): shown,
  /// not edited.
  std::string bound_to;
  /// Non-empty for a text style field of an element that passes it on:
  /// "applies to all text of this Plot".
  std::string scope_note;
};

/// @brief The outcome of a change.
struct commit_result {
  bool ok = false;
  std::string source; ///< The new source when ok; the unchanged one otherwise.
  std::string error; ///< When not ok: what is wrong, with its position if the source has one.
  int line = 0;
  int column = 0;
  /// A structural change: where element paths went (empty otherwise), for
  /// the view state and anything else keyed by path.
  path_map paths;
  /// A structural change: the element it made, moved or duplicated.
  std::string path;
};

/// @brief Where a chart added at the root goes when the root is not a
///        layout that takes it (spec/14_editor_workspace/DESIGN.md section 5).
enum class chart_placement { below, right };

/// @brief The editor's state. Every change is a patch of the YAML text
///        (source_edit.hpp), checked by reading the whole result like any
///        source; a change that makes it invalid is refused and changes
///        nothing.
class editor_model {
 public:
  /// @throws nymph::error if `source` is not a valid nymph source.
  explicit editor_model(std::string source);
  ~editor_model();
  editor_model(const editor_model&) = delete;
  editor_model& operator=(const editor_model&) = delete;

  /// @brief The source as edited.
  const std::string& source() const {
    return source_;
  }

  // ── Selection ────────────────────────────────────────────────────────

  /// @brief Selects an element (and optionally one of its fields). A path
  ///        that is not in the source selects nothing.
  void select(std::optional<edit_target_id> selection);
  const std::optional<edit_target_id>& selection() const {
    return selection_;
  }

  /// @brief The image options of the source as it is.
  const image_options& image() const {
    return figure_->image;
  }

  /// @brief The type name of the selected element ("Image" for the image
  ///        options), or "".
  std::string selected_type() const;

  /// @brief The fields of the selected element, in schema order; empty
  ///        without a selection.
  std::vector<editable_field> fields() const;

  /// @brief The fields of the selected element that are written in the
  ///        source or were asked for by name: for "Selection" lists.
  const field_desc* find_field(std::string_view name) const;

  // ── Changes ──────────────────────────────────────────────────────────

  /// @brief Sets field `field` of the selected element to `yaml_value` (one
  ///        line of YAML, from encode_*()).
  /// @param merge Continue the previous change of the same field as one
  ///        undo step (a drag, typing): no new undo entry.
  /// @param replace_binding Allow replacing a `$column` reference by the
  ///        literal.
  commit_result commit(std::string_view field, std::string_view yaml_value, bool merge = false, bool replace_binding = false);

  /// @brief Removes the field from the source: back to its default.
  commit_result clear(std::string_view field);

  // ── Structure (spec/14_editor_workspace/DESIGN.md sections 3 and 5) ───

  /// @brief Inserts a new element of `type` into `parent` at `index`
  ///        (kAppend: last), from its starter (element_starter.hpp): a fresh
  ///        `id`, link ends from the selected node and the diagram's other
  ///        ids. `fields` (name, one-line YAML value) are set on it before it
  ///        is checked. Selects it.
  commit_result add_element(
      std::string_view parent,
      size_t index,
      std::string_view type,
      const std::vector<std::pair<std::string, std::string>>& fields = {});

  /// @brief Adds a chart of `type` at the root: appended when the root is a
  ///        layout that takes it, else next to the root in a new
  ///        VerticalLayout (`below`) or HorizontalLayout (`right`).
  commit_result add_chart(std::string_view type, chart_placement placement);

  /// @brief Removes the element at `path` (not `root`); with
  ///        `with_references` also the elements that refer to it by id
  ///        (references()). The selection moves to the next sibling, else the
  ///        previous one, else the parent.
  commit_result remove_element(std::string_view path, bool with_references = false);

  /// @brief Moves the element at `path` to place `index` among its siblings.
  commit_result move_element(std::string_view path, size_t index);

  /// @brief Copies the element at `path` after itself, with fresh ids.
  commit_result duplicate_element(std::string_view path);

  /// @brief Sets (a value) or clears (nullopt) several fields of the element
  ///        at `path` (or `image`) as one undo step, whatever is selected; a
  ///        value may be a column reference (`$x`). For an agent's edits.
  commit_result set_fields(
      std::string_view path,
      const std::vector<std::pair<std::string, std::optional<std::string>>>& fields);

  /// @brief Replaces the whole source (an agent's rewrite); paths may change.
  commit_result set_source(std::string source);

  /// @brief The elements that refer to the element at `path` by id
  ///        (find_references()).
  std::vector<std::string> references(std::string_view path) const;

  /// @brief The ids of the diagram `parent` belongs to (the nearest figure
  ///        at or above it), in document order, the selected element's
  ///        first: what a new link's ends are filled from.
  std::vector<std::string> candidate_ids(std::string_view parent) const;

  /// @brief The figure as bound from source().
  const figure& bound() const {
    return *figure_;
  }

  /// @brief Counts accepted changes, undo and redo included: a change from
  ///        somewhere else since a number was read shows as a new number.
  uint64_t version() const {
    return version_;
  }

  // ── Data (spec/14_editor_workspace/DESIGN.md sections 3 and 4) ──────────
  // Each change rewrites one table in its form (data_edit.hpp) and is one
  // undo step; a table is named by its name, "" for the single unnamed one.

  /// @brief The tables of the source, in order.
  std::vector<data_table> tables() const;

  /// @brief Sets one cell (the row and column must exist).
  commit_result set_cell(std::string_view table, size_t row, size_t column, std::string_view text);

  /// @brief Replaces a table's header and rows (an import, a paste).
  commit_result set_table(std::string_view table, const table_text& cells);

  /// @brief Writes the rows of `text` (parse_rows(): CSV or tab-separated)
  ///        into the table from (`row`, `column`) on, adding rows and
  ///        columns as needed.
  commit_result paste_cells(std::string_view table, size_t row, size_t column, std::string_view text);

  /// @brief Inserts `count` empty rows before `index` (the row count: last).
  commit_result insert_rows(std::string_view table, size_t index, size_t count = 1);
  /// @brief Removes `count` rows from `index`.
  commit_result remove_rows(std::string_view table, size_t index, size_t count = 1);
  /// @brief Inserts an empty column called `name` before `index`.
  commit_result insert_column(std::string_view table, size_t index, std::string_view name);
  /// @brief Removes column `index` (refused while elements refer to it).
  commit_result remove_column(std::string_view table, size_t index);
  /// @brief Renames a column and the references to it (data_edit.hpp).
  commit_result rename_column(std::string_view table, std::string_view from, std::string_view to);

  /// @brief Adds table `name` (data_edit.hpp add_table(): a second table
  ///        names the first `first_name`).
  commit_result add_table(std::string_view name, const table_text& cells, std::string_view first_name = "table1");
  /// @brief Removes a table (refused while elements refer to it).
  commit_result remove_table(std::string_view name);
  /// @brief Renames a table and the references to it.
  commit_result rename_table(std::string_view from, std::string_view to);

  /// @brief A name no table has: "table1", "table2", ...
  std::string fresh_table_name() const;

  /// @brief What a new table starts with: columns x and y, three rows.
  static table_text starter_table();

  // ── Undo ─────────────────────────────────────────────────────────────

  bool can_undo() const {
    return !undo_.empty();
  }
  bool can_redo() const {
    return !redo_.empty();
  }
  /// @return The restored source, or nullopt when there is nothing to undo.
  std::optional<std::string> undo();
  std::optional<std::string> redo();

  /// @brief Whether the source differs from the one the model started with.
  bool modified() const {
    return source_ != original_;
  }

  /// @brief Notes that the source as it is now was saved: modified() is
  ///        false again; the undo history stays.
  void mark_saved() {
    original_ = source_;
  }

  /// @brief Sets the source the file holds, which modified() compares with
  ///        (a panel opened after changes were made already).
  void set_original(std::string source) {
    original_ = std::move(source);
  }

  /// @brief Replaces the source from outside (the file changed); drops the
  ///        undo history.
  /// @throws nymph::error as the constructor; the model is unchanged.
  void reset(std::string source);

  /// @brief Most undo steps kept.
  static constexpr size_t kMaxUndo = 200;

 private:
  void refresh(); ///< Binds source_.
  commit_result fail(const std::string& message, int line = 0, int column = 0) const;
  commit_result apply(std::string next, const std::string& key, bool merge);
  /// apply() for a structural change: renames the `view` key's paths and the
  /// selection by `r.paths`, then selects `to_select` (if not empty).
  commit_result apply_structure(tree_edit_result r, const std::string& key, const std::string& to_select);
  /// Runs a data patch (`patch` returns the new source) as one undo step.
  template <typename Patch>
  commit_result apply_data(const std::string& key, Patch patch);
  /// Changes a copy of a table's cells with `change` and writes it back.
  template <typename Change>
  commit_result change_table(std::string_view table, const std::string& key, Change change);

  std::string source_;
  std::string original_;
  std::unique_ptr<figure> figure_;
  std::optional<edit_target_id> selection_;
  std::deque<std::string> undo_;
  std::deque<std::string> redo_;
  std::string last_key_; ///< Element, field and undo generation of the last change, for merging.
  uint64_t version_ = 0;
};

} // namespace bdg::nymph
