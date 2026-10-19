// MIT License © 2026 Binary Dice Games
/// @file table.cpp
/// @brief CSV and column-mapping data to a table. The CSV reader is a port
///        of wish nymph's `nymph_csv.cpp`, with a position for every cell.
#include "document/table.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
#include "core/charconv.hpp"
#include "core/error.hpp"
#include "core/limits.hpp"

namespace bdg::nymph {

namespace {

bool is_blank(char c) {
  return c == ' ' || c == '\t' || c == '\r';
}

std::string trimmed(std::string_view s) {
  while (!s.empty() && is_blank(s.front())) {
    s.remove_prefix(1);
  }
  while (!s.empty() && is_blank(s.back())) {
    s.remove_suffix(1);
  }
  return std::string(s);
}

/// Number of UTF-8 characters in `s` (continuation bytes not counted).
int char_count(std::string_view s) {
  int n = 0;
  for (char c : s) {
    n += (static_cast<unsigned char>(c) & 0xC0) != 0x80 ? 1 : 0;
  }
  return n;
}

/// One record (a header or data row) and where it started.
struct record {
  std::vector<cell> cells;
  int line = 1;
  int column = 1;
  bool blank = true; ///< nothing but whitespace: skipped by the caller
};

/// Reads CSV records one at a time, tracking the source line and column.
class reader {
 public:
  reader(std::string_view text, int first_line, int first_column)
      : text_(text), line_(first_line), first_column_(first_column) {
    if (text_.substr(0, 3) == "\xEF\xBB\xBF") { // UTF-8 byte order mark
      pos_ = 3;
      line_start_ = 3;
    }
  }

  bool at_end() const {
    return pos_ >= text_.size();
  }

  record next() {
    record rec;
    rec.line = line_;
    rec.column = column_at(pos_);
    for (;;) {
      bool quoted = false;
      rec.cells.push_back(read_cell(quoted));
      if (quoted || !rec.cells.back().text.empty()) {
        rec.blank = false;
      }
      if (pos_ < text_.size() && text_[pos_] == ',') {
        ++pos_;
        rec.blank = false;
        continue;
      }
      if (pos_ < text_.size() && text_[pos_] == '\n') {
        new_line(pos_ + 1);
      }
      return rec;
    }
  }

 private:
  int column_at(size_t pos) const {
    return first_column_ + char_count(text_.substr(line_start_, pos - line_start_));
  }

  void new_line(size_t next_pos) {
    pos_ = next_pos;
    line_start_ = next_pos;
    ++line_;
  }

  /// Reads up to (not past) the next unquoted ',' or '\n'.
  cell read_cell(bool& quoted) {
    size_t probe = pos_;
    while (probe < text_.size() && is_blank(text_[probe])) {
      ++probe;
    }
    cell out;
    out.line = line_;
    out.column = column_at(probe);
    if (probe < text_.size() && text_[probe] == '"') {
      quoted = true;
      out.text = read_quoted(probe + 1, out);
      return out;
    }
    const size_t start = pos_;
    while (pos_ < text_.size() && text_[pos_] != ',' && text_[pos_] != '\n') {
      ++pos_;
    }
    out.text = trimmed(text_.substr(start, pos_ - start));
    return out;
  }

  std::string read_quoted(size_t from, const cell& where) {
    std::string out;
    pos_ = from;
    for (;;) {
      if (pos_ >= text_.size()) {
        throw error(where.line, where.column, "unterminated quoted CSV cell");
      }
      const char c = text_[pos_++];
      if (c == '"') {
        if (pos_ < text_.size() && text_[pos_] == '"') {
          out.push_back('"');
          ++pos_;
          continue;
        }
        break;
      }
      if (c == '\n') {
        line_start_ = pos_;
        ++line_;
      }
      out.push_back(c);
    }
    while (pos_ < text_.size() && is_blank(text_[pos_])) {
      ++pos_;
    }
    if (pos_ < text_.size() && text_[pos_] != ',' && text_[pos_] != '\n') {
      throw error(line_, column_at(pos_), "unexpected text after a quoted CSV cell");
    }
    return out;
  }

  std::string_view text_;
  size_t pos_ = 0;
  size_t line_start_ = 0;
  int line_;
  int first_column_;
};

bool parse_number(const std::string& text, double& out) {
  const char* first = text.data();
  const char* last = first + text.size();
  if (first != last && *first == '+') {
    ++first;
  }
  const auto result = from_chars_double(first, last, out);
  return result.ec == std::errc{} && result.ptr == last;
}

[[noreturn]] void too_many_rows(int line, int column) {
  throw error(line, column, "data has more than " + std::to_string(kMaxDataRows) + " rows");
}

/// A YAML scalar as a table cell: plain empty, `~` and `null` are empty.
cell to_cell(const yaml_node& n) {
  cell c{n.text, n.line, n.column};
  if (!n.quoted() && (n.text == "~" || n.text == "null" || n.text == "Null" || n.text == "NULL")) {
    c.text.clear();
  }
  return c;
}

} // namespace

int table::find_column(std::string_view reference) const {
  for (size_t i = 0; i < headers.size(); ++i) {
    if (headers[i].text == reference) {
      return static_cast<int>(i);
    }
  }
  if (reference.empty() ||
      !std::all_of(reference.begin(), reference.end(), [](char c) { return c >= '0' && c <= '9'; })) {
    return -1;
  }
  size_t position = 0;
  const auto result = std::from_chars(reference.data(), reference.data() + reference.size(), position);
  if (result.ec != std::errc{} || position < 1 || position > headers.size()) {
    return -1;
  }
  return static_cast<int>(position - 1);
}

std::string table::header_list() const {
  std::string out;
  for (const cell& h : headers) {
    out += (out.empty() ? "" : ", ") + h.text;
  }
  return out;
}

std::vector<double> table::numbers(int column) const {
  const std::vector<cell>& cells = columns.at(static_cast<size_t>(column));
  std::vector<double> out(cells.size());
  for (size_t r = 0; r < cells.size(); ++r) {
    if (cells[r].text.empty()) {
      out[r] = std::numeric_limits<double>::quiet_NaN();
      continue;
    }
    if (!parse_number(cells[r].text, out[r])) {
      throw error(
          cells[r].line,
          cells[r].column,
          "column '" + headers[static_cast<size_t>(column)].text + "', row " + std::to_string(r + 1) + ": '" +
              cells[r].text + "' is not a number");
    }
  }
  return out;
}

table parse_csv(std::string_view text, int first_line, int first_column) {
  table out;
  reader in(text, first_line, first_column);
  bool have_header = false;
  while (!in.at_end()) {
    record rec = in.next();
    if (rec.blank) {
      continue;
    }
    if (!have_header) {
      have_header = true;
      for (size_t i = 0; i < rec.cells.size(); ++i) {
        const cell& h = rec.cells[i];
        if (h.text.empty()) {
          throw error(h.line, h.column, "CSV header " + std::to_string(i + 1) + " is empty");
        }
        for (const cell& earlier : out.headers) {
          if (earlier.text == h.text) {
            throw error(h.line, h.column, "duplicate CSV header '" + h.text + "'");
          }
        }
        out.headers.push_back(h);
      }
      out.columns.resize(out.headers.size());
      continue;
    }
    if (rec.cells.size() != out.headers.size()) {
      throw error(
          rec.line,
          rec.column,
          "CSV row has " + std::to_string(rec.cells.size()) + " cells, the header has " +
              std::to_string(out.headers.size()));
    }
    if (out.row_lines.size() >= kMaxDataRows) {
      too_many_rows(rec.line, rec.column);
    }
    for (size_t i = 0; i < rec.cells.size(); ++i) {
      out.columns[i].push_back(std::move(rec.cells[i]));
    }
    out.row_lines.push_back(rec.line);
  }
  return out;
}

table table_from_mapping(const yaml_node& data) {
  table out;
  for (size_t c = 0; c < data.keys.size(); ++c) {
    const yaml_node& key = data.keys[c];
    const yaml_node& values = data.children[c];
    if (key.text.empty()) {
      throw error(key.line, key.column, "a data column needs a name");
    }
    if (!values.is_sequence()) {
      throw error(
          values.line, values.column, "data column '" + key.text + "' must be a list of values, like [1, 2, 3]");
    }
    if (values.children.size() > kMaxDataRows) {
      too_many_rows(values.line, values.column);
    }
    if (c > 0 && values.children.size() != out.columns[0].size()) {
      throw error(
          key.line,
          key.column,
          "data column '" + key.text + "' has " + std::to_string(values.children.size()) + " values, column '" +
              out.headers[0].text + "' has " + std::to_string(out.columns[0].size()));
    }
    std::vector<cell> column;
    column.reserve(values.children.size());
    for (size_t r = 0; r < values.children.size(); ++r) {
      const yaml_node& item = values.children[r];
      if (!item.is_scalar()) {
        throw error(
            item.line,
            item.column,
            "data column '" + key.text + "', value " + std::to_string(r + 1) +
                " must be a single value, not a list or a mapping");
      }
      column.push_back(to_cell(item));
    }
    out.headers.push_back(cell{key.text, key.line, key.column});
    out.columns.push_back(std::move(column));
  }
  if (!out.columns.empty()) {
    for (const cell& first : out.columns[0]) {
      out.row_lines.push_back(first.line);
    }
  }
  return out;
}

table parse_data(const yaml_node& data) {
  if (data.is_mapping()) {
    return table_from_mapping(data);
  }
  if (data.is_sequence()) {
    throw error(
        data.line,
        data.column,
        "data must be CSV text (data: |) or a mapping of columns (data: {x: [1, 2], y: [3, 4]})");
  }
  switch (data.style) {
    case scalar_style::literal:
      return parse_csv(data.text, data.content_line, std::max(1, data.content_column));
    case scalar_style::folded:
      throw error(
          data.line, data.column, "'>' joins the CSV lines into one; write the data as a literal block (data: |)");
    case scalar_style::single_quoted:
    case scalar_style::double_quoted:
      return parse_csv(data.text, data.line, data.column + 1);
    case scalar_style::plain:
      return parse_csv(data.text, data.line, data.column);
  }
  return {};
}

// ── Named tables ─────────────────────────────────────────────────────────────

int data_set::find_table(std::string_view name) const {
  for (size_t i = 0; i < names.size(); ++i) {
    if (names[i].text == name) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

std::string data_set::name_list() const {
  std::string out;
  for (const cell& n : names) {
    out += (out.empty() ? "" : ", ") + n.text;
  }
  return out;
}

data_set parse_data_set(const yaml_node& data) {
  data_set out;
  // Named tables: a mapping whose values are tables (text or mappings)
  // rather than columns (lists). The first value decides; a value of the
  // other kind is an error.
  const bool named = data.is_mapping() && !data.children.empty() && !data.children[0].is_sequence();
  for (size_t i = 1; data.is_mapping() && i < data.children.size(); ++i) {
    const yaml_node& value = data.children[i];
    if (value.is_sequence() == named) {
      const std::string& column = named ? data.keys[i].text : data.keys[0].text;
      const std::string& table_name = named ? data.keys[0].text : data.keys[i].text;
      throw error(
          value.line,
          value.column,
          "data mixes columns and named tables: '" + column + "' is a list (a column), '" + table_name +
              "' a table; use either columns ({x: [1, 2]}) or named tables ({sales: |, ...})");
    }
  }
  if (!named) {
    out.names.push_back(cell{});
    out.tables.push_back(parse_data(data));
    return out;
  }
  size_t total_rows = 0;
  for (size_t i = 0; i < data.keys.size(); ++i) {
    const yaml_node& key = data.keys[i];
    const yaml_node& value = data.children[i];
    if (key.text.empty()) {
      throw error(key.line, key.column, "a data table needs a name");
    }
    if (key.text.find(':') != std::string::npos) {
      throw error(
          key.line, key.column, "data table name '" + key.text + "' must not contain ':' (it separates $table:column)");
    }
    table t = parse_data(value);
    total_rows += t.row_count();
    if (total_rows > kMaxDataRows) {
      too_many_rows(value.line, value.column);
    }
    out.names.push_back(cell{key.text, key.line, key.column});
    out.tables.push_back(std::move(t));
  }
  return out;
}

} // namespace bdg::nymph
