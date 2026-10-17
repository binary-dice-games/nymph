// MIT License © 2026 Binary Dice Games
/// @file table.hpp
/// @brief The `data` of a nymph source: named columns of cells kept as text,
///        each with its source position, converted to numbers per column on
///        demand.
///
/// Two source forms give the same table (spec/01_core_features/DESIGN.md
/// section 3, `data`): CSV text, usually a `|` block scalar, and a YAML
/// mapping of column name to a list of values. `data` may also map table names to tables in either
/// form: a `data_set`.
#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include "document/yaml_doc.hpp"

namespace bdg::nymph {

/// @brief One CSV cell or YAML value: its text and where it is in the file.
struct cell {
  std::string text;
  int line = 0; ///< 1-based source line.
  int column = 0; ///< 1-based source column, in characters.
};

/// @brief Column-major table: `columns[c][r]` is row `r` of column `c`.
struct table {
  std::vector<cell> headers; ///< One per column, in order.
  std::vector<std::vector<cell>> columns; ///< Same size as `headers`; all the same length.
  std::vector<int> row_lines; ///< Source line each data row starts on.

  /// @brief Number of data rows (a CSV header is not one).
  size_t row_count() const {
    return row_lines.size();
  }

  /// @brief Resolves a column reference: a header name, else a 1-based
  ///        position written in digits (`"2"`).
  /// @return The column index, or -1 if `reference` matches neither.
  int find_column(std::string_view reference) const;

  /// @brief Header names joined as "a, b, c", for error messages.
  std::string header_list() const;

  /// @brief Column `column` as numbers; an empty cell is NaN (a gap).
  /// @throws nymph::error at the cell for one that is neither empty nor a
  ///         number.
  std::vector<double> numbers(int column) const;
};

/// @brief Parses CSV text: comma-separated, header first, RFC 4180 quoting,
///        whitespace around unquoted cells trimmed, blank lines skipped,
///        every row as wide as the header, an optional UTF-8 BOM.
/// @param text         The CSV. Empty (or blank) gives an empty table.
/// @param first_line   1-based source line of `text`'s first line.
/// @param first_column 1-based source column of the start of every line of
///                     `text` (a literal block scalar's indentation + 1).
/// @throws nymph::error for a ragged row, an unterminated quote, text after
///         a closing quote, an empty or duplicate header, or more than
///         kMaxDataRows rows.
table parse_csv(std::string_view text, int first_line = 1, int first_column = 1);

/// @brief Builds a table from a YAML mapping of column name to a list of
///        scalars. A plain empty value, `~` or `null` is an empty cell.
/// @throws nymph::error for a value that is not a list, a list item that is
///         not a scalar, columns of different lengths, an empty column
///         name, or more than kMaxDataRows rows.
table table_from_mapping(const yaml_node& data);

/// @brief The `data` value of a source in either form: a literal (`|`) or
///        single-line scalar is CSV, a mapping is columns.
/// @throws nymph::error for a sequence, a folded (`>`) scalar (folding
///         joins CSV lines), or any error of the two parsers.
table parse_data(const yaml_node& data);

/// @brief Every table of a source's `data`.
///
/// The single-table forms give one table with an empty name; named tables
/// keep their key order. A source without `data` has no tables.
struct data_set {
  std::vector<cell> names; ///< Table names with their key positions.
  std::vector<table> tables; ///< Same size as `names`.

  /// @brief True when `data` used the named-tables form.
  bool named() const {
    return !names.empty() && !names[0].text.empty();
  }

  /// @brief The index of the table called `name`, or -1.
  int find_table(std::string_view name) const;

  /// @brief Table names joined as "a, b, c", for error messages.
  std::string name_list() const;
};

/// @brief The `data` value of a source: one table (parse_data()), or named
///        tables when every value of a `data` mapping is text or a mapping.
/// @throws nymph::error for a mapping mixing lists with tables (at the
///         first value whose kind differs from the first one's), a table
///         name that is empty or contains ':', or any parse_data() error.
data_set parse_data_set(const yaml_node& data);

} // namespace bdg::nymph
