// MIT License © 2026 Binary Dice Games
/// @file table_grid.hpp
/// @brief A data table as an editable ImGui grid: a toolbar (Import CSV,
///        Export CSV, Paste CSV, Add row, Add column), editable column names,
///        row numbers, one text field per visible cell, menus on the row and
///        column headers, and the CSV file dialog
///        (spec/14_editor_workspace/DESIGN.md section 4). Pure ImGui.
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "edit/editor_model.hpp"

namespace bdg::nymph {

struct inspector_events;

/// @brief The grid of one table at a time (the active data tab).
class table_grid {
 public:
  /// @brief Draws table `name` ("" for the single unnamed one) of the model
  ///        into the current window, filling it.
  void draw(editor_model& model, const std::string& name, inspector_events& events);

  /// @brief Draws the CSV file dialog when one is open; call once per frame
  ///        outside the grid.
  void dialogs(editor_model& model, inspector_events& events);

  /// @brief The folder the file dialogs start in (the input file's).
  void set_folder(std::filesystem::path folder) {
    folder_ = std::move(folder);
  }

  /// @brief Opens the Import CSV / Export CSV dialog for table `name`, or
  ///        with set_host_files() records a request for the host instead.
  void ask_import(const std::string& name);
  void ask_export(const std::string& name);

  /// @brief A CSV import or export the host does with its own file picker
  ///        (the browser's), instead of the ImGui file dialog
  ///        (spec/15_web_app/DESIGN.md section 5.1).
  struct file_request {
    bool import_csv = true; ///< false: export.
    std::string table;
  };
  /// @brief Whether Import / Export CSV go to the host (take_requests())
  ///        rather than to the ImGui file dialog (the default).
  void set_host_files(bool on) {
    host_files_ = on;
  }
  /// @brief The requests made since the last call.
  std::vector<file_request> take_requests() {
    return std::exchange(requests_, {});
  }

  /// @brief Highlights `column` of table `name` and scrolls to it ("Go to data").
  void show_column(const std::string& name, const std::string& column);

  /// @brief Whether a cell or a column name is being typed into.
  bool editing() const {
    return editing_.has_value();
  }

  // ── What the dialog does, for its buttons and for tests ──────────────

  /// @brief Replaces table `name` with the CSV file at `path`.
  commit_result import_file(editor_model& model, const std::string& name, const std::filesystem::path& path);
  /// @brief Writes table `name` to `path`.
  /// @return "" when written, else what went wrong.
  std::string export_file(const editor_model& model, const std::string& name, const std::filesystem::path& path) const;

  /// @brief Commits the text typed into a cell (`row` -1: the column name).
  commit_result commit_cell(editor_model& model, const std::string& name, int row, size_t column, const std::string& text);

  /// @brief The CSV files and folders of `folder`: folders first (ending in
  ///        '/'), then `.csv` files, each sorted by name.
  static std::vector<std::string> folder_entries(const std::filesystem::path& folder);

 private:
  struct cell_id {
    std::string table;
    int row = 0; ///< -1: the header.
    size_t column = 0;
    bool operator==(const cell_id&) const = default;
  };
  enum class dialog { none, import_csv, export_csv };

  void toolbar(editor_model& model, const std::string& name, const table_text& t, inspector_events& events);
  void grid(editor_model& model, const std::string& name, const table_text& t, inspector_events& events);
  bool cell(editor_model& model, const cell_id& id, const std::string& value, float width, inspector_events& events);

  std::optional<cell_id> editing_; ///< The text field in use.
  char edit_[1024] = {}; ///< Its text.
  bool discard_edit_ = false; ///< A block paste replaced the edit: do not commit it.
  std::optional<cell_id> focus_; ///< A field to give the keyboard to (Enter moves down).
  std::optional<std::pair<std::string, std::string>> highlight_; ///< Table and column to show.
  bool scroll_to_highlight_ = false;

  bool host_files_ = false;
  std::vector<file_request> requests_;
  std::filesystem::path folder_;
  dialog dialog_ = dialog::none;
  bool open_dialog_ = false;
  std::string dialog_table_;
  char file_name_[512] = {};
  std::string dialog_error_;
  bool confirm_replace_ = false;
};

} // namespace bdg::nymph
