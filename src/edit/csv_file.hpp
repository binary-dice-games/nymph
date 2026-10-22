// MIT License © 2026 Binary Dice Games
/// @file csv_file.hpp
/// @brief CSV files for the data tables' Import and Export, and the rows
///        of a pasted block (spec/14_editor_workspace/DESIGN.md sections 3
///        and 4).
#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
#include "edit/data_edit.hpp"

namespace bdg::nymph {

/// @brief Reads a CSV file (header first; parse_csv()'s rules, a UTF-8 BOM
///        dropped).
/// @throws nymph::error naming the file for one that cannot be read, or the
///         parser's error with the file's name.
table_text read_csv_file(const std::filesystem::path& path);

/// @brief Writes `t` as CSV (csv_text(), UTF-8, no BOM) to `path`, through a
///        temporary file renamed over it: a failure leaves no partial file.
/// @throws nymph::error naming the file when it cannot be written.
void write_csv_file(const std::filesystem::path& path, const table_text& t);

/// @brief The rows of a pasted block: tab-separated when its first line has a
///        tab (a spreadsheet's copy), else CSV with quotes; no header. Rows
///        may differ in width; a final line break adds no row.
std::vector<std::vector<std::string>> parse_rows(std::string_view text);

} // namespace bdg::nymph
