// MIT License © 2026 Binary Dice Games
/// @file csv_file.cpp
/// @brief CSV files and pasted rows (see csv_file.hpp).
#include "edit/csv_file.hpp"
#include <fstream>
#include <sstream>
#include <system_error>
#include "core/error.hpp"

namespace bdg::nymph {

table_text read_csv_file(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    throw error(0, 0, "cannot read " + path.string());
  }
  std::stringstream ss;
  ss << in.rdbuf();
  try {
    return table_from_csv(ss.str());
  } catch (const error& e) {
    throw error(e.line(), e.column(), path.filename().string() + ": " + e.what());
  }
}

void write_csv_file(const std::filesystem::path& path, const table_text& t) {
  std::filesystem::path temp = path;
  temp += ".nymph-tmp";
  {
    std::ofstream out(temp, std::ios::binary | std::ios::trunc);
    const std::string text = csv_text(t);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    out.close();
    if (!out) {
      std::error_code ignored;
      std::filesystem::remove(temp, ignored);
      throw error(0, 0, "cannot write " + path.string());
    }
  }
  std::error_code ec;
  std::filesystem::rename(temp, path, ec);
  if (ec) {
    std::error_code ignored;
    std::filesystem::remove(temp, ignored);
    throw error(0, 0, "cannot write " + path.string() + " (" + ec.message() + ")");
  }
}

std::vector<std::vector<std::string>> parse_rows(std::string_view text) {
  std::vector<std::vector<std::string>> rows;
  const size_t first_break = text.find('\n');
  const bool tabs = text.substr(0, first_break).find('\t') != std::string_view::npos;
  std::vector<std::string> row;
  std::string cell;
  bool quoted = false;
  bool any = false; // something on this line
  const auto end_cell = [&] {
    if (!tabs) {
      // CSV: spaces around an unquoted cell do not count.
      const size_t a = cell.find_first_not_of(" \t");
      const size_t b = cell.find_last_not_of(" \t");
      cell = a == std::string::npos ? std::string() : cell.substr(a, b - a + 1);
    }
    row.push_back(std::move(cell));
    cell.clear();
  };
  const auto end_row = [&] {
    end_cell();
    rows.push_back(std::move(row));
    row.clear();
    any = false;
  };
  for (size_t i = 0; i < text.size(); ++i) {
    const char c = text[i];
    if (quoted) {
      if (c == '"' && i + 1 < text.size() && text[i + 1] == '"') {
        cell += '"';
        ++i;
      } else if (c == '"') {
        quoted = false;
      } else {
        cell += c;
      }
      continue;
    }
    if (c == '"' && !tabs && cell.find_first_not_of(" \t") == std::string::npos) {
      quoted = true;
      cell.clear();
      any = true;
    } else if (c == (tabs ? '\t' : ',')) {
      end_cell();
      any = true;
    } else if (c == '\r') {
      continue;
    } else if (c == '\n') {
      end_row();
    } else {
      cell += c;
      any = true;
    }
  }
  if (any || !cell.empty() || !row.empty()) {
    end_row();
  }
  return rows;
}

} // namespace bdg::nymph
