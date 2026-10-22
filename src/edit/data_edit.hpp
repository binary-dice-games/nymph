// MIT License © 2026 Binary Dice Games
/// @file data_edit.hpp
/// @brief Text patches to the `data` tables of a nymph YAML source: a whole
///        table rewritten in its own form (CSV text or columns), tables
///        added, removed and renamed, columns renamed, with the elements'
///        column references kept in step
///        (spec/14_editor_workspace/DESIGN.md section 3, `data_edit`). Cells
///        are text, written as they are; every byte outside the changed table
///        stays as written. The caller validates the result.
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "document/table.hpp"

namespace bdg::nymph {

/// @brief A table as text: its header and its rows (row-major), every row as
///        wide as the header.
struct table_text {
  std::vector<std::string> headers;
  std::vector<std::vector<std::string>> rows;

  size_t width() const {
    return headers.size();
  }
  bool operator==(const table_text&) const = default;
};

/// @brief How a table is written in the source.
enum class table_form {
  csv, ///< CSV text in a literal block (`|`).
  columns, ///< A mapping of column name to a list of values.
};

/// @brief `t`'s cells as text.
table_text to_text(const table& t);

/// @brief Parses CSV text (parse_csv()) into a table_text.
/// @throws nymph::error as parse_csv().
table_text table_from_csv(std::string_view csv);

/// @brief One CSV cell: quoted (RFC 4180) when it holds a comma, a quote, a
///        line break, or spaces at either end.
std::string csv_cell(std::string_view text);

/// @brief The table as CSV text: header then rows, cells joined by ", ",
///        each line ended by "\n".
std::string csv_text(const table_text& t);

/// @brief One table of a source's `data`, by name ("" for the single
///        unnamed table), with how it is written.
struct data_table {
  std::string name;
  table_form form = table_form::csv;
  table_text cells;
};

/// @brief The tables of a source's `data` (none without one).
/// @throws nymph::error for a source that does not parse or bad `data`.
std::vector<data_table> read_tables(std::string_view source);

/// @brief Replaces the contents of table `name` ("" for the single unnamed
///        one), keeping its form.
/// @throws nymph::error for an unknown table.
std::string set_table(std::string_view source, std::string_view name, const table_text& t);

/// @brief Adds table `name` (CSV form). Without `data`, an empty `name`
///        makes the single unnamed table. A source with a single unnamed
///        table first turns it into the named table `first_name`, and its
///        bare column references (`$x`, `$2`) into `$first_name:x`.
/// @throws nymph::error for an empty name or one with ':', a name in use,
///         or an empty `name` when `data` exists.
std::string add_table(std::string_view source, std::string_view name, const table_text& t, std::string_view first_name = "table1");

/// @brief Removes table `name` ("" for the single unnamed one); `data` goes
///        with the last table. References to it are left for the caller to
///        reject.
std::string remove_table(std::string_view source, std::string_view name);

/// @brief Renames a named table and the references to it (`$from:c`).
/// @throws nymph::error for the unnamed table, an unknown or bad name, or a
///         name in use.
std::string rename_table(std::string_view source, std::string_view from, std::string_view to);

/// @brief Renames column `from` of table `table` and the references to it
///        (`$from`, `$table:from`; positional `$2` are unchanged).
/// @throws nymph::error for an unknown table or column, or a name in use.
std::string rename_column(std::string_view source, std::string_view table, std::string_view from, std::string_view to);

/// @brief One column reference written in an element.
struct column_use {
  std::string element; ///< Element path.
  std::string field; ///< Field name.
  std::string reference; ///< Without its `$`: `x`, `t:x`, `2`.
};

/// @brief Every column reference in the elements, in document order.
std::vector<column_use> find_column_uses(std::string_view source);

/// @brief Whether `reference` (from find_column_uses()) names column
///        `column` (0-based, called `header`) of table `table` ("" for the
///        single unnamed one).
bool refers_to(std::string_view reference, std::string_view table, size_t column, std::string_view header);

/// @brief Rewrites every column reference in the elements: `rewrite` gets
///        a reference without its `$` (`x`, `t:x`, `2`) and returns its new
///        text, or nullopt to keep it.
std::string rewrite_references(
    std::string_view source,
    const std::function<std::optional<std::string>(std::string_view reference)>& rewrite);

} // namespace bdg::nymph
