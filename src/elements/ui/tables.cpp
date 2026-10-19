// MIT License © 2026 Binary Dice Games
/// @file tables.cpp
/// @brief Table, TableColumn and TableRow (spec/09_ui_mockups/DESIGN.md
///        section 3, "Widgets"): columns of data cells (often `$column`s)
///        and rows of widgets.
#include <imgui.h>
#include <algorithm>
#include <memory>
#include "core/error.hpp"
#include "elements/ui/ui_common.hpp"
#include "elements/ui/ui_flags.hpp"
#include "schema/registry.hpp"

namespace bdg::nymph::ui {

namespace {

/// ImGui's column limit (IMGUI_TABLE_MAX_COLUMNS).
constexpr size_t kMaxColumns = 512;

namespace column_field {
constexpr size_t label = 0, width = 1, flags = 2, cells = 3;
}

namespace table_field {
constexpr size_t label = 0, flags = 1, freeze = 2, width = 3, height = 4;
}

bool is_column(const node& n) {
  return n.cls->name() == "TableColumn";
}

// ── TableColumn ──────────────────────────────────────────────────────────────

class table_column_class : public widget_class {
 public:
  table_column_class()
      : widget_class(
            "TableColumn",
            {},
            {text_field("label", "", "Header of the column; a table shows a header row when any column has one."),
             number_field(
                 "width",
                 0.0,
                 "Width in pixels (WidthFixed) or a stretch weight (WidthStretch); 0 is the table's sizing.",
                 value_range{0, 4096}),
             flags_field("flags", 0, table_column_flag_names(), "ImGuiTableColumnFlags without the prefix."),
             texts_field("cells", "The column's text, one cell per row, or a $column whose cells are the rows.")},
            "One column of a Table (ImGui::TableSetupColumn): its header, width and text cells.",
            false) {
    set_primary_slot(std::string(kTableSlot));
  }

  void check(const node& n) const override {
    const auto flags = n.get<int64_t>(column_field::flags);
    // ImGui asserts on these combinations.
    if ((flags & ImGuiTableColumnFlags_WidthStretch) && (flags & ImGuiTableColumnFlags_WidthFixed)) {
      fail_at(n, column_field::flags, "may not have both WidthStretch and WidthFixed");
    }
    if ((flags & ImGuiTableColumnFlags_IndentEnable) && (flags & ImGuiTableColumnFlags_IndentDisable)) {
      fail_at(n, column_field::flags, "may not have both IndentEnable and IndentDisable");
    }
  }

 protected:
  item_result draw_item(const node&, draw_context&) const override {
    item_result r;
    r.finished = true; // set up and filled by its Table
    return r;
  }
};

// ── TableRow ─────────────────────────────────────────────────────────────────

class table_row_class : public widget_class {
 public:
  table_row_class()
      : widget_class(
            "TableRow",
            widget_container_slots(),
            {number_field("height", 0.0, "Smallest height of the row in pixels; 0 fits its cells.", value_range{0, 4096}),
             bool_field("header", false, "Draws the row as a header row (ImGuiTableRowFlags_Headers).")},
            "One row of a Table: each child is one cell, left to right (ImGui::TableNextRow, TableNextColumn).",
            false) {
    set_primary_slot(std::string(kTableSlot));
  }

 protected:
  item_result draw_item(const node& n, draw_context& dc) const override {
    ImGui::TableNextRow(
        n.get<bool>(1) ? ImGuiTableRowFlags_Headers : ImGuiTableRowFlags_None, static_cast<float>(n.get<double>(0)));
    for (size_t i = 0; i < n.children.size(); ++i) {
      const node& cell = n.children[i];
      ImGui::TableNextColumn();
      ImGui::PushID(static_cast<int>(i));
      if (cell.cls->placed_as(cell) == kFigureSlot) {
        draw_figure_in_flow(cell, dc);
      } else {
        dc.draw_node(cell);
      }
      ImGui::PopID();
    }
    item_result r;
    r.finished = true;
    return r;
  }
};

// ── Table ────────────────────────────────────────────────────────────────────

class table_class : public widget_class {
 public:
  table_class()
      : widget_class(
            "Table",
            {std::string(kTableSlot)},
            {text_field("label", "", "ID of the table (not shown)."),
             flags_field("flags", 0, table_flag_names(), "ImGuiTableFlags without the prefix, like Borders|RowBg."),
             integers_field("freeze", "Columns and rows [columns, rows] that stay in view while scrolling; empty for none."),
             number_field("width", 0.0, "Outer width in pixels; 0 fills the row.", value_range{0, 8192}),
             number_field(
                 "height",
                 0.0,
                 "Outer height in pixels; 0 fits the rows (with ScrollY: fills the space left).",
                 value_range{0, 8192})},
            "ImGui::BeginTable: TableColumn children define the columns (and their text cells, often $columns); "
            "TableRow children add rows of widgets after them.",
            false) {}

  void check(const node& n) const override {
    const auto flags = n.get<int64_t>(table_field::flags);
    const int64_t sizing = flags & ImGuiTableFlags_SizingMask_;
    if (sizing != 0 && sizing != ImGuiTableFlags_SizingFixedFit && sizing != ImGuiTableFlags_SizingFixedSame &&
        sizing != ImGuiTableFlags_SizingStretchProp && sizing != ImGuiTableFlags_SizingStretchSame) {
      fail_at(n, table_field::flags, "may have only one Sizing policy");
    }
    const auto& freeze = n.get<std::vector<int64_t>>(table_field::freeze);
    if (!freeze.empty() && (freeze.size() != 2 || freeze[0] < 0 || freeze[1] < 0 || freeze[0] > 64 || freeze[1] > 64)) {
      fail_at(n, table_field::freeze, "must be [columns, rows], each 0 to 64");
    }
    size_t columns = 0, rows = 0;
    const node* first_cells = nullptr;
    for (const node& child : n.children) {
      if (is_column(child)) {
        ++columns;
        const auto& cells = child.get<std::vector<std::string>>(column_field::cells);
        if (cells.empty()) {
          continue;
        }
        if (first_cells == nullptr) {
          first_cells = &child;
          rows = cells.size();
        } else if (cells.size() != rows) {
          child.cls->fail_at(
              child,
              column_field::cells,
              "has " + std::to_string(cells.size()) + " cells, but an earlier column has " + std::to_string(rows));
        }
      }
    }
    for (const node& child : n.children) {
      if (!is_column(child) && columns > 0 && child.children.size() > columns) {
        throw error(
            child.line,
            child.column,
            "TableRow has " + std::to_string(child.children.size()) + " cells, but the Table has " +
                std::to_string(columns) + " columns");
      }
    }
    if (column_count(n) > kMaxColumns) {
      throw error(n.line, n.column, "Table has more than " + std::to_string(kMaxColumns) + " columns");
    }
  }

 protected:
  item_result draw_item(const node& n, draw_context& dc) const override {
    const size_t columns = std::max<size_t>(1, column_count(n));
    const std::string& label = n.get<std::string>(table_field::label);
    const ImVec2 outer(
        static_cast<float>(n.get<double>(table_field::width)), static_cast<float>(n.get<double>(table_field::height)));
    item_result r;
    r.finished = true;
    if (!ImGui::BeginTable(
            label.empty() ? "##table" : label.c_str(),
            static_cast<int>(columns),
            static_cast<ImGuiTableFlags>(n.get<int64_t>(table_field::flags)),
            outer)) {
      return r;
    }
    const auto& freeze = n.get<std::vector<int64_t>>(table_field::freeze);
    if (freeze.size() == 2) {
      ImGui::TableSetupScrollFreeze(static_cast<int>(freeze[0]), static_cast<int>(freeze[1]));
    }
    bool headers = false;
    size_t data_rows = 0;
    std::vector<const node*> column_nodes;
    for (const node& child : n.children) {
      if (is_column(child)) {
        column_nodes.push_back(&child);
        const std::string& header = child.get<std::string>(column_field::label);
        headers = headers || !header.empty();
        ImGui::TableSetupColumn(
            header.c_str(),
            static_cast<ImGuiTableColumnFlags>(child.get<int64_t>(column_field::flags)),
            static_cast<float>(child.get<double>(column_field::width)));
        data_rows = std::max(data_rows, child.get<std::vector<std::string>>(column_field::cells).size());
      }
    }
    if (headers) {
      ImGui::TableHeadersRow();
    }
    // The text cells of the columns, row by row.
    for (size_t row = 0; row < data_rows; ++row) {
      ImGui::TableNextRow();
      for (const node* column : column_nodes) {
        ImGui::TableNextColumn();
        const auto& cells = column->get<std::vector<std::string>>(column_field::cells);
        if (row < cells.size()) {
          ImGui::TextUnformatted(cells[row].c_str());
          dc.record_target(*column, "cells", {ImGui::GetItemRectMin(), ImGui::GetItemRectMax()}, cells[row]);
        }
      }
    }
    // Then the rows of widgets, each under its index as ID.
    for (size_t i = 0; i < n.children.size(); ++i) {
      if (!is_column(n.children[i])) {
        ImGui::PushID(static_cast<int>(i));
        dc.draw_node(n.children[i]);
        ImGui::PopID();
      }
    }
    ImGui::EndTable();
    if (dc.edit_mode()) {
      dc.record_target(n, {}, {ImGui::GetItemRectMin(), ImGui::GetItemRectMax()}, label);
    }
    return r;
  }

 private:
  /// The declared columns, or the widest row when none are declared.
  static size_t column_count(const node& n) {
    size_t columns = 0, widest = 0;
    for (const node& child : n.children) {
      if (is_column(child)) {
        ++columns;
      } else {
        widest = std::max(widest, child.children.size());
      }
    }
    return columns > 0 ? columns : widest;
  }
};

} // namespace

void register_tables(registry& reg) {
  reg.add(std::make_unique<table_class>());
  reg.add(std::make_unique<table_column_class>());
  reg.add(std::make_unique<table_row_class>());
}

} // namespace bdg::nymph::ui
