// MIT License © 2026 Binary Dice Games
/// @file editor_model.hpp
/// @brief The state of the view editor: the source being edited, what is
///        selected, the editable fields of the selection, and undo / redo
///        (spec/08_view_editor/DESIGN.md section 3, `editor_model`).
///        Platform-neutral: no window, no OpenGL, no ImGui.
#pragma once

#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "document/bind.hpp"
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
};

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

  std::string source_;
  std::string original_;
  std::unique_ptr<figure> figure_;
  std::optional<edit_target_id> selection_;
  std::deque<std::string> undo_;
  std::deque<std::string> redo_;
  std::string last_key_; ///< Element, field and undo generation of the last change, for merging.
};

} // namespace bdg::nymph
