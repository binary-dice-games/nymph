// MIT License © 2026 Binary Dice Games
/// @file inspector.hpp
/// @brief The editor's contents drawn with Dear ImGui: toolbar, element
///        tree, the fields of the selection with a widget per field type,
///        and an error panel at the bottom
///        (spec/08_view_editor/DESIGN.md section 3). Pure ImGui, no OpenGL,
///        so it runs against any ImGui context.
#pragma once

#include <optional>
#include <string>
#include <vector>
#include "edit/editor_model.hpp"
#include "edit/number_box.hpp"
#include "edit/tree_menu.hpp"
#include "schema/field_desc.hpp"
#include "view/live_view.hpp"

namespace bdg::nymph {

/// @brief The widget the inspector uses for a field.
enum class widget_kind {
  checkbox, ///< boolean
  integer, ///< integer
  number, ///< number
  text, ///< single-line text
  multiline_text, ///< a text that has line breaks, or takes them
  font, ///< a font file name: a combo of the embedded fonts
  color, ///< colour picker with an "automatic" state
  choice, ///< enumeration: a combo of its names
  flags, ///< a checkbox per flag
  list, ///< lists: the YAML text (`[1, 2]`, `$table:column`) and a column picker
  read_only, ///< anything else: shown as text
};

/// @brief The widget for `f`; independent of any value.
widget_kind widget_for(const field_desc& f);

/// @brief What the user did in one frame of the inspector.
struct inspector_events {
  /// A changed source, to show (the model already holds it).
  std::optional<std::string> source;
  /// With a structural change: where element paths went, so the view state
  /// follows them (live_view::set_source_moved()).
  path_map paths;
  /// A tree item was chosen.
  std::optional<edit_target_id> select;
  bool undo = false; ///< The undo or redo button; `source` has the result.
  bool save = false; ///< The Save button: overwrite the input (after the viewer asks).
  bool reset_view = false;
  /// A change from the tree's menu was refused: the message for the error
  /// panel.
  std::string error;
  /// "Go to data": the table ("" for the single unnamed one) and column a
  /// field takes its values from.
  std::optional<std::pair<std::string, std::string>> show_data;
};

/// @brief Draws the editor's panel and applies its changes to a model.
class inspector {
 public:
  /// @brief Draws into the current ImGui window.
  /// @param tree The figure's elements, from live_view::element_list().
  /// @param status A line shown under the toolbar (what was saved, say).
  inspector_events draw(editor_model& model, const std::vector<edit_element>& tree, const std::string& status);

  /// @brief What typing `text` into the text widget of `field` does, at
  ///        once (the widget does it after a short pause). Returns the
  ///        events as draw() would.
  inspector_events edit_text(editor_model& model, const std::string& field, const std::string& text);

  /// @brief What writing `yaml` (one line: `[1, 2]`, `$monthly:cost`) into
  ///        list field `field` does, as the list widget and its column picker
  ///        do: a column reference may replace a list or another reference;
  ///        empty text clears the field.
  inspector_events edit_list(editor_model& model, const std::string& field, const std::string& yaml);

  /// @brief Commits a text typed a moment ago, if any (also done by draw()
  ///        when the pause is over or the widget loses focus).
  inspector_events flush(editor_model& model);

  /// @brief The last change refused, with its position; "" when the last
  ///        change was accepted.
  const std::string& error() const {
    return error_;
  }

  /// @brief Whether a typed text waits for its pause: frames must keep
  ///        coming until it is committed.
  bool pending() const {
    return pending_.has_value();
  }

  /// @brief Whether a number field is being typed into: frames must keep
  ///        coming (the field is drawn the frame after the click, and its
  ///        caret blinks).
  bool number_typing() const {
    return numbers_.typing();
  }

  /// @brief Shows `message` in the error panel ("" hides it): a change made
  ///        elsewhere in the window (a table) was refused or accepted.
  void set_error(std::string message) {
    error_ = std::move(message);
  }

  /// @brief The tree's menu and dialogs (keyboard shortcuts reach it here).
  tree_menu& menu() {
    return menu_;
  }

  /// @brief Pause after the last keystroke before a text is committed.
  static constexpr double kTypingPause = 0.15;

 private:
  struct pending_text {
    std::string element;
    std::string field;
    std::string text;
    double changed_at = 0.0;
    bool merge = false;
  };

  void note(const commit_result& r, inspector_events& events);
  void draw_field(editor_model& model, const editable_field& f, bool focused, inspector_events& events);
  commit_result commit_pending(editor_model& model);
  void draw_tree(editor_model& model, const std::vector<edit_element>& tree, inspector_events& events);
  /// Drag source and drop target of the tree row just drawn (`open`: its
  /// children are listed below it): reorders and reparents elements.
  void drag_drop(editor_model& model, const edit_element& e, bool open, inspector_events& events);

  /// The Description window: the figure's `description` and `source` keys.
  void draw_info(editor_model& model, inspector_events& events);
  /// The Theme popup: every chart theme with a swatch; a click sets
  /// `image.theme` (docs/themes.md).
  void draw_themes(editor_model& model, inspector_events& events);

  bool info_open_ = false;
  std::string info_key_; ///< The key being typed into, "" when none.
  char info_buffer_[8192] = {}; ///< Its text.
  float info_split_ = 0.5f; ///< Share of the dialog's height the description box takes.
  float elements_height_ = 220.0f; ///< Height of the elements panel, dragged by the divider.
  float log_height_ = 0.0f;        ///< Height of the log panel once dragged; 0: fits its text.
  std::string error_;
  std::optional<pending_text> pending_;
  pending_text last_attempt_; ///< The text commit_pending() tried last.
  std::optional<pending_text> rejected_; ///< A refused text, still shown in its widget.
  std::string last_selection_;
  std::string typing_key_; ///< Element and field being typed into, "" when none.
  std::string drag_key_; ///< Widget being dragged, "" when none.
  /// Colour field whose picker opens next frame (it was automatic and just
  /// got its first colour), "" when none.
  std::string open_picker_key_;
  std::string revealed_; ///< The element the tree last scrolled to.
  std::string list_key_; ///< Element and field of the list being typed into, "" when none.
  char list_buffer_[4096] = {}; ///< Its text.
  tree_menu menu_;
  number_box numbers_; ///< The number fields' boxes.
};

} // namespace bdg::nymph
