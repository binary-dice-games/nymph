// MIT License © 2026 Binary Dice Games
/// @file table_grid.cpp
/// @brief The data table grid and its CSV dialog (see table_grid.hpp).
#include "edit/table_grid.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <system_error>
#include "core/error.hpp"
#include "edit/csv_file.hpp"
#include "edit/inspector.hpp"

namespace bdg::nymph {

namespace {

constexpr const char* kImportDialog = "Import CSV";
constexpr const char* kExportDialog = "Export CSV";

void report(const commit_result& r, inspector_events& events) {
  if (r.ok) {
    events.source = r.source;
    events.error.clear();
  } else {
    events.error = r.line > 0 ? "line " + std::to_string(r.line) + ": " + r.error : r.error;
  }
}

void copy_to(char* buffer, size_t size, const std::string& text) {
  std::strncpy(buffer, text.c_str(), size - 1);
  buffer[size - 1] = '\0';
}

const char* table_title(const std::string& name) {
  return name.empty() ? "data" : name.c_str();
}

/// The elements that use a column, one line each, for its tooltip.
std::string uses_of(const std::vector<column_use>& uses, const std::string& table, size_t column, const std::string& header) {
  std::string out;
  for (const column_use& u : uses) {
    if (refers_to(u.reference, table, column, header)) {
      out += (out.empty() ? "" : "\n") + u.element + "  " + u.field + ": $" + u.reference;
    }
  }
  return out;
}

} // namespace

// ── Files ────────────────────────────────────────────────────────────────────

commit_result table_grid::import_file(editor_model& model, const std::string& name, const std::filesystem::path& path) {
  try {
    return model.set_table(name, read_csv_file(path));
  } catch (const error& e) {
    commit_result r;
    r.source = model.source();
    r.error = e.what();
    return r;
  }
}

std::string table_grid::export_file(const editor_model& model, const std::string& name, const std::filesystem::path& path) const {
  for (const data_table& t : model.tables()) {
    if (t.name == name) {
      try {
        write_csv_file(path, t.cells);
        return {};
      } catch (const error& e) {
        return e.what();
      }
    }
  }
  return "there is no table '" + name + "'";
}

std::vector<std::string> table_grid::folder_entries(const std::filesystem::path& folder) {
  std::vector<std::string> folders;
  std::vector<std::string> files;
  std::error_code ec;
  for (std::filesystem::directory_iterator it(folder, ec), end; !ec && it != end; it.increment(ec)) {
    const std::string name = it->path().filename().string();
    if (name.empty() || name[0] == '.') {
      continue;
    }
    std::error_code type_ec;
    if (it->is_directory(type_ec)) {
      folders.push_back(name + "/");
    } else {
      std::string ext = it->path().extension().string();
      std::transform(ext.begin(), ext.end(), ext.begin(), [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
      if (ext == ".csv") {
        files.push_back(name);
      }
    }
  }
  std::sort(folders.begin(), folders.end());
  std::sort(files.begin(), files.end());
  folders.insert(folders.end(), files.begin(), files.end());
  return folders;
}

void table_grid::ask_import(const std::string& name) {
  if (host_files_) {
    requests_.push_back({true, name});
    return;
  }
  dialog_ = dialog::import_csv;
  open_dialog_ = true;
  dialog_table_ = name;
  file_name_[0] = '\0';
  dialog_error_.clear();
  confirm_replace_ = false;
}

void table_grid::ask_export(const std::string& name) {
  if (host_files_) {
    requests_.push_back({false, name});
    return;
  }
  dialog_ = dialog::export_csv;
  open_dialog_ = true;
  dialog_table_ = name;
  copy_to(file_name_, sizeof(file_name_), std::string(table_title(name)) + ".csv");
  dialog_error_.clear();
  confirm_replace_ = false;
}

void table_grid::show_column(const std::string& name, const std::string& column) {
  highlight_ = std::make_pair(name, column);
  scroll_to_highlight_ = true;
}

// ── Cells ────────────────────────────────────────────────────────────────────

commit_result table_grid::commit_cell(editor_model& model, const std::string& name, int row, size_t column, const std::string& text) {
  if (row < 0) {
    for (const data_table& t : model.tables()) {
      if (t.name == name && column < t.cells.width()) {
        return model.rename_column(name, t.cells.headers[column], text);
      }
    }
    commit_result r;
    r.source = model.source();
    r.error = "there is no such column";
    return r;
  }
  return model.set_cell(name, static_cast<size_t>(row), column, text);
}

bool table_grid::cell(editor_model& model, const cell_id& id, const std::string& value, float width, inspector_events& events) {
  const bool is_editing = editing_ && *editing_ == id;
  char scratch[1024];
  char* buffer = is_editing ? edit_ : scratch;
  if (!is_editing) {
    copy_to(scratch, sizeof(scratch), value);
  }
  if (focus_ && *focus_ == id) {
    ImGui::SetKeyboardFocusHere();
    focus_.reset();
  }
  ImGui::SetNextItemWidth(width);
  ImGui::PushID(id.row);
  ImGui::PushID(static_cast<int>(id.column));
  ImGui::InputText("##cell", buffer, sizeof(edit_));
  const bool activated = ImGui::IsItemActivated();
  const bool active = ImGui::IsItemActive();
  const bool committed = ImGui::IsItemDeactivatedAfterEdit();
  const bool deactivated = ImGui::IsItemDeactivated();
  ImGui::PopID();
  ImGui::PopID();
  if (activated) {
    editing_ = id;
    copy_to(edit_, sizeof(edit_), value);
    discard_edit_ = false;
  }
  // Ctrl+V of several cells (lines or tabs) pastes a block from here.
  if (active && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V, false) && id.row >= 0) {
    const char* clip = ImGui::GetClipboardText();
    const std::string text = clip != nullptr ? clip : "";
    if (text.find_first_of("\n\t") != std::string::npos) {
      report(model.paste_cells(id.table, static_cast<size_t>(id.row), id.column, text), events);
      discard_edit_ = true;
      ImGui::ClearActiveID();
      editing_.reset();
      return true;
    }
  }
  if (committed && !discard_edit_ && editing_ && *editing_ == id) {
    const std::string typed = edit_;
    if (typed != value) {
      report(commit_cell(model, id.table, id.row, id.column, typed), events);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)) {
      focus_ = cell_id{id.table, id.row + 1, id.column}; // Enter moves down
    }
  }
  if (deactivated && editing_ && *editing_ == id) {
    editing_.reset();
    discard_edit_ = false;
  }
  return committed;
}

void table_grid::toolbar(editor_model& model, const std::string& name, const table_text& t, inspector_events& events) {
  if (ImGui::Button("Import CSV")) {
    ask_import(name);
  }
  ImGui::SameLine();
  if (ImGui::Button("Export CSV")) {
    ask_export(name);
  }
  ImGui::SameLine();
  if (ImGui::Button("Paste CSV")) {
    const char* clip = ImGui::GetClipboardText();
    try {
      report(model.set_table(name, table_from_csv(clip != nullptr ? clip : "")), events);
    } catch (const error& e) {
      events.error = std::string("the clipboard is not a CSV table: ") + e.what();
    }
  }
  ImGui::SetItemTooltip("Replaces the table with the CSV text on the clipboard (header first).");
  ImGui::SameLine();
  if (ImGui::Button("Add row")) {
    report(model.insert_rows(name, t.rows.size()), events);
    focus_ = cell_id{name, static_cast<int>(t.rows.size()), 0};
  }
  ImGui::SameLine();
  if (ImGui::Button("Add column")) {
    std::string column = "column" + std::to_string(t.width() + 1);
    for (int n = static_cast<int>(t.width()) + 2; std::find(t.headers.begin(), t.headers.end(), column) != t.headers.end(); ++n) {
      column = "column" + std::to_string(n);
    }
    report(model.insert_column(name, t.width(), column), events);
  }
  ImGui::SameLine();
  ImGui::TextDisabled("%d rows", static_cast<int>(t.rows.size()));
}

void table_grid::grid(editor_model& model, const std::string& name, const table_text& t, inspector_events& events) {
  const ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX |
      ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;
  const int columns = static_cast<int>(t.width()) + 1;
  if (columns > 512 || !ImGui::BeginTable("##grid", columns, flags, ImGui::GetContentRegionAvail())) {
    return;
  }
  const float cell_width = 110.0f;
  ImGui::TableSetupScrollFreeze(1, 1);
  ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("00000").x);
  for (size_t c = 0; c < t.width(); ++c) {
    ImGui::TableSetupColumn(t.headers[c].c_str(), ImGuiTableColumnFlags_WidthFixed, cell_width);
  }
  const std::vector<column_use> uses = find_column_uses(model.source());

  // The header row: column names are text fields too.
  ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
  ImGui::TableSetColumnIndex(0);
  ImGui::TextDisabled("#");
  for (size_t c = 0; c < t.width(); ++c) {
    ImGui::TableSetColumnIndex(static_cast<int>(c) + 1);
    const bool shown = highlight_ && highlight_->first == name && highlight_->second == t.headers[c];
    if (shown) {
      ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, ImGui::GetColorU32(ImGuiCol_HeaderActive));
      if (scroll_to_highlight_) {
        ImGui::SetScrollHereX(0.5f);
        scroll_to_highlight_ = false;
      }
    }
    const std::string used = uses_of(uses, name, c, t.headers[c]);
    cell(model, cell_id{name, -1, c}, t.headers[c], used.empty() ? cell_width : cell_width - 18.0f, events);
    if (!used.empty()) {
      ImGui::SameLine(0.0f, 2.0f);
      ImGui::TextDisabled("$");
      ImGui::SetItemTooltip("Used by:\n%s", used.c_str());
    }
    ImGui::PushID(static_cast<int>(c));
    if (ImGui::BeginPopupContextItem("##column")) {
      if (ImGui::MenuItem("Insert column left")) {
        report(model.insert_column(name, c, "column" + std::to_string(t.width() + 1)), events);
      }
      if (ImGui::MenuItem("Insert column right")) {
        report(model.insert_column(name, c + 1, "column" + std::to_string(t.width() + 1)), events);
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Delete column", nullptr, false, t.width() > 1)) {
        report(model.remove_column(name, c), events);
      }
      ImGui::EndPopup();
    }
    ImGui::PopID();
  }

  // The rows: only the visible ones are drawn.
  ImGuiListClipper clipper;
  clipper.Begin(static_cast<int>(t.rows.size()));
  if (focus_ && focus_->table == name && focus_->row >= 0 && focus_->row < static_cast<int>(t.rows.size())) {
    clipper.IncludeItemByIndex(focus_->row);
  }
  while (clipper.Step()) {
    for (int r = clipper.DisplayStart; r < clipper.DisplayEnd; ++r) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::PushID(r);
      ImGui::AlignTextToFramePadding();
      ImGui::TextDisabled("%d", r + 1);
      if (ImGui::BeginPopupContextItem("##row")) {
        if (ImGui::MenuItem("Insert row above")) {
          report(model.insert_rows(name, static_cast<size_t>(r)), events);
        }
        if (ImGui::MenuItem("Insert row below")) {
          report(model.insert_rows(name, static_cast<size_t>(r) + 1), events);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Delete row")) {
          report(model.remove_rows(name, static_cast<size_t>(r)), events);
        }
        ImGui::EndPopup();
      }
      ImGui::PopID();
      const std::vector<std::string>& row = t.rows[static_cast<size_t>(r)];
      for (size_t c = 0; c < t.width(); ++c) {
        ImGui::TableSetColumnIndex(static_cast<int>(c) + 1);
        if (cell(model, cell_id{name, r, c}, row[c], cell_width, events) && events.source) {
          // The table changed under the loop: draw the rest next frame.
          clipper.End();
          ImGui::EndTable();
          return;
        }
      }
    }
  }
  ImGui::EndTable();
}

void table_grid::draw(editor_model& model, const std::string& name, inspector_events& events) {
  const std::vector<data_table> tables = model.tables();
  const auto it = std::find_if(tables.begin(), tables.end(), [&](const data_table& t) { return t.name == name; });
  if (it == tables.end()) {
    ImGui::TextDisabled("This table is no longer in the source.");
    return;
  }
  ImGui::PushID(name.c_str());
  toolbar(model, name, it->cells, events);
  if (!events.source) {
    grid(model, name, it->cells, events);
  }
  ImGui::PopID();
}

// ── The CSV dialog ───────────────────────────────────────────────────────────

void table_grid::dialogs(editor_model& model, inspector_events& events) {
  const char* title = dialog_ == dialog::import_csv ? kImportDialog : kExportDialog;
  if (open_dialog_) {
    ImGui::OpenPopup(title);
    open_dialog_ = false;
  }
  if (dialog_ == dialog::none) {
    return;
  }
  const ImGuiViewport* vp = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(460, 420), ImGuiCond_Appearing);
  if (!ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_NoSavedSettings)) {
    return;
  }
  const bool importing = dialog_ == dialog::import_csv;
  if (folder_.empty()) {
    std::error_code ec;
    folder_ = std::filesystem::current_path(ec);
  }
  ImGui::Text("%s table '%s'", importing ? "Replace the" : "Write the", table_title(dialog_table_));
  // The folder: a path field and its entries.
  char folder[1024];
  copy_to(folder, sizeof(folder), folder_.string());
  ImGui::SetNextItemWidth(-1.0f);
  if (ImGui::InputText("##folder", folder, sizeof(folder), ImGuiInputTextFlags_EnterReturnsTrue)) {
    folder_ = folder;
  }
  const float list_height = ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() * 3.0f;
  if (ImGui::BeginListBox("##entries", ImVec2(-1.0f, std::max(list_height, 80.0f)))) {
    if (ImGui::Selectable("../")) {
      folder_ = folder_.parent_path();
    }
    for (const std::string& entry : folder_entries(folder_)) {
      const bool is_folder = entry.back() == '/';
      if (ImGui::Selectable(entry.c_str(), !is_folder && entry == file_name_, ImGuiSelectableFlags_AllowDoubleClick)) {
        if (is_folder) {
          folder_ /= entry.substr(0, entry.size() - 1);
        } else {
          copy_to(file_name_, sizeof(file_name_), entry);
          confirm_replace_ = false;
          if (importing && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            const commit_result r = import_file(model, dialog_table_, folder_ / file_name_);
            if (r.ok) {
              report(r, events);
              dialog_ = dialog::none;
              ImGui::CloseCurrentPopup();
            } else {
              dialog_error_ = r.error;
            }
          }
        }
      }
    }
    ImGui::EndListBox();
  }
  ImGui::SetNextItemWidth(-1.0f);
  if (ImGui::InputText("##file", file_name_, sizeof(file_name_))) {
    confirm_replace_ = false;
  }
  if (!dialog_error_.empty()) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.2f, 0.2f, 1.0f));
    ImGui::TextWrapped("%s", dialog_error_.c_str());
    ImGui::PopStyleColor();
  }
  const std::filesystem::path target = folder_ / file_name_;
  ImGui::BeginDisabled(file_name_[0] == '\0');
  const char* action = importing ? "Import" : confirm_replace_ ? "Replace" : "Export";
  if (ImGui::Button(action, ImVec2(110, 0))) {
    if (importing) {
      const commit_result r = import_file(model, dialog_table_, target);
      if (r.ok) {
        report(r, events);
        dialog_ = dialog::none;
        ImGui::CloseCurrentPopup();
      } else {
        dialog_error_ = r.error;
      }
    } else {
      std::error_code ec;
      if (std::filesystem::exists(target, ec) && !confirm_replace_) {
        confirm_replace_ = true; // ask once: the button now says Replace
        dialog_error_ = target.filename().string() + " exists; Replace overwrites it.";
      } else {
        dialog_error_ = export_file(model, dialog_table_, target);
        if (dialog_error_.empty()) {
          dialog_ = dialog::none;
          ImGui::CloseCurrentPopup();
        }
      }
    }
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  if (ImGui::Button("Cancel", ImVec2(110, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
    dialog_ = dialog::none;
    ImGui::CloseCurrentPopup();
  }
  ImGui::EndPopup();
}

} // namespace bdg::nymph
