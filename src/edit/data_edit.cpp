// MIT License © 2026 Binary Dice Games
/// @file data_edit.cpp
/// @brief Text patches to the `data` tables (see data_edit.hpp).
#include "edit/data_edit.hpp"
#include <algorithm>
#include "core/error.hpp"
#include "document/bind.hpp"
#include "edit/field_value.hpp"
#include "edit/source_edit.hpp"
#include "edit/yaml_text.hpp"

namespace bdg::nymph {

using namespace yaml_text;

namespace {

/// Where table `name` is written.
struct located {
  const yaml_node* data = nullptr; ///< The `data` value.
  const yaml_node* key = nullptr; ///< The table's key (`data` itself when unnamed).
  const yaml_node* value = nullptr; ///< The table's value.
  bool named = false;
};

located locate(const yaml_node& doc, std::string_view name) {
  located l;
  l.data = doc.is_mapping() ? doc.find("data") : nullptr;
  if (l.data == nullptr) {
    throw error(0, 0, "the source has no data");
  }
  const data_set ds = parse_data_set(*l.data);
  l.named = ds.named();
  if (!l.named) {
    if (!name.empty()) {
      throw error(l.data->line, l.data->column, "the data has no table '" + std::string(name) + "' (it is one unnamed table)");
    }
    l.key = doc.find_key("data");
    l.value = l.data;
    return l;
  }
  l.key = l.data->find_key(name);
  l.value = l.data->find(name);
  if (l.value == nullptr) {
    throw error(l.data->line, l.data->column, "the data has no table '" + std::string(name) + "'; it has " + ds.name_list());
  }
  return l;
}

void check_name(std::string_view name) {
  if (name.empty()) {
    throw error(0, 0, "a table needs a name");
  }
  if (name.find(':') != std::string_view::npos) {
    throw error(0, 0, "a table name cannot contain ':'");
  }
}

/// A table or column name as a mapping key: plain when it is a simple
/// name (`y`, `unit price`), else as encode_text() writes it.
std::string key_text(std::string_view name) {
  const auto simple = [](char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
  };
  bool plain = !name.empty() && simple(name.front()) && simple(name.back());
  for (const char c : name) {
    plain = plain && (simple(c) || c == ' ' || c == '-' || c == '.');
  }
  return plain ? std::string(name) : encode_text(name);
}

std::string column_cell(const std::string& text) {
  return text.empty() ? std::string("~") : encode_text(text);
}

/// The columns form's lines: "name: [a, b, ~]".
std::vector<std::string> column_lines(const table_text& t) {
  std::vector<std::string> out;
  for (size_t c = 0; c < t.width(); ++c) {
    std::string line = key_text(t.headers[c]) + ": [";
    for (size_t r = 0; r < t.rows.size(); ++r) {
      line += (r ? ", " : "") + column_cell(t.rows[r][c]);
    }
    out.push_back(line + "]");
  }
  return out;
}

std::vector<std::string> csv_lines(const table_text& t) {
  std::vector<std::string> out;
  const std::string text = csv_text(t);
  for (size_t at = 0; at < text.size();) {
    const size_t nl = text.find('\n', at);
    out.push_back(text.substr(at, nl - at));
    at = nl + 1;
  }
  return out;
}

/// `lines` as one text, each line after the first prefixed with `indent`.
std::string join(const std::vector<std::string>& lines, const std::string& indent, const char* eol) {
  std::string out;
  for (size_t i = 0; i < lines.size(); ++i) {
    out += (i ? eol + indent : std::string()) + lines[i];
  }
  return out;
}

/// The value text replacing a table written as `form`, at an indentation of
/// `indent` (the table's key column, 0-based) in block context.
std::string block_value(const table_text& t, table_form form, size_t indent, const char* eol) {
  const std::string inner(indent + 2, ' ');
  if (form == table_form::csv) {
    const std::vector<std::string> lines = csv_lines(t);
    return lines.empty() ? std::string("|") : "|" + std::string(eol) + inner + join(lines, inner, eol);
  }
  const std::vector<std::string> lines = column_lines(t);
  return lines.empty() ? std::string("{}") : std::string(eol) + inner + join(lines, inner, eol);
}

/// The source with its top-level `data` entry replaced by `lines` (the key
/// line included), or appended.
std::string replace_data_entry(std::string_view source, const std::vector<std::string>& lines) {
  const data_replacement r = replace_top_level(source, "data", "x");
  std::vector<std::string> all;
  for (size_t at = 0; at < r.source.size();) {
    const size_t nl = r.source.find('\n', at);
    const size_t end = nl == std::string::npos ? r.source.size() : nl;
    all.push_back(r.source.substr(at, end - at));
    at = end + 1;
  }
  const size_t key_line = static_cast<size_t>(r.first_line) - 2; // 0-based: "data:" then "  x"
  all.erase(all.begin() + static_cast<long>(key_line), all.begin() + static_cast<long>(key_line) + 2);
  all.insert(all.begin() + static_cast<long>(key_line), lines.begin(), lines.end());
  std::string out;
  for (const std::string& l : all) {
    out += l + "\n";
  }
  return out;
}

/// The whole `data` entry for `tables`, in block style.
std::vector<std::string> data_lines(const std::vector<data_table>& tables) {
  std::vector<std::string> out;
  const auto add = [&](const std::string& head, const data_table& t, const std::string& indent) {
    if (t.form == table_form::csv) {
      out.push_back(head + " |");
      for (const std::string& l : csv_lines(t.cells)) {
        out.push_back(indent + l);
      }
    } else if (t.cells.width() == 0) {
      out.push_back(head + " {}");
    } else {
      out.push_back(head);
      for (const std::string& l : column_lines(t.cells)) {
        out.push_back(indent + l);
      }
    }
  };
  if (tables.size() == 1 && tables[0].name.empty()) {
    add("data:", tables[0], "  ");
    return out;
  }
  out.push_back("data:");
  for (const data_table& t : tables) {
    add("  " + key_text(t.name) + ":", t, "    ");
  }
  return out;
}

/// Every element mapping under `root` by path.
void collect_elements(const yaml_node& element, const std::string& path, std::vector<std::pair<std::string, const yaml_node*>>& out) {
  out.emplace_back(path, &element);
  const yaml_node* list = element.find("children");
  if (list != nullptr && list->is_sequence()) {
    for (size_t i = 0; i < list->children.size(); ++i) {
      if (list->children[i].is_mapping()) {
        collect_elements(list->children[i], path + "/children/" + std::to_string(i), out);
      }
    }
  }
}

/// A reference's new text, or nullopt to keep it.
std::optional<std::string> rewritten(
    const yaml_node& value,
    const std::function<std::optional<std::string>(std::string_view)>& rewrite) {
  if (!value.is_scalar() || value.text.size() < 2 || value.text[0] != '$' || value.text[1] == '$') {
    return std::nullopt;
  }
  const std::optional<std::string> to = rewrite(std::string_view(value.text).substr(1));
  return to ? std::optional<std::string>("$" + *to) : std::nullopt;
}

} // namespace

// ── Text ─────────────────────────────────────────────────────────────────────

table_text to_text(const table& t) {
  table_text out;
  for (const cell& h : t.headers) {
    out.headers.push_back(h.text);
  }
  out.rows.assign(t.row_count(), std::vector<std::string>(t.headers.size()));
  for (size_t c = 0; c < t.columns.size(); ++c) {
    for (size_t r = 0; r < t.columns[c].size(); ++r) {
      out.rows[r][c] = t.columns[c][r].text;
    }
  }
  return out;
}

table_text table_from_csv(std::string_view csv) {
  return to_text(parse_csv(csv));
}

std::string csv_cell(std::string_view text) {
  const bool quote = text.find_first_of(",\"\r\n") != std::string_view::npos ||
      (!text.empty() && (text.front() == ' ' || text.front() == '\t' || text.back() == ' ' || text.back() == '\t'));
  if (!quote) {
    return std::string(text);
  }
  std::string out = "\"";
  for (const char c : text) {
    out += c;
    if (c == '"') {
      out += '"';
    }
  }
  return out + "\"";
}

std::string csv_text(const table_text& t) {
  std::string out;
  const auto line = [&](const std::vector<std::string>& cells) {
    const size_t start = out.size();
    for (size_t i = 0; i < cells.size(); ++i) {
      out += (i ? ", " : "") + csv_cell(cells[i]);
    }
    if (out.size() == start) {
      out += "\"\""; // one empty cell: a blank line would be skipped
    }
    out += "\n";
  };
  if (t.width() == 0) {
    return out;
  }
  line(t.headers);
  for (const std::vector<std::string>& row : t.rows) {
    line(row);
  }
  return out;
}

// ── Tables ───────────────────────────────────────────────────────────────────

std::vector<data_table> read_tables(std::string_view source) {
  const yaml_node doc = parse_yaml(source);
  const yaml_node* data = doc.is_mapping() ? doc.find("data") : nullptr;
  std::vector<data_table> out;
  if (data == nullptr) {
    return out;
  }
  const data_set ds = parse_data_set(*data);
  for (size_t i = 0; i < ds.tables.size(); ++i) {
    data_table t;
    t.name = ds.names[i].text;
    const yaml_node* value = ds.named() ? data->find(t.name) : data;
    t.form = value != nullptr && value->is_mapping() ? table_form::columns : table_form::csv;
    t.cells = to_text(ds.tables[i]);
    out.push_back(std::move(t));
  }
  return out;
}

std::string set_table(std::string_view source, std::string_view name, const table_text& t) {
  const yaml_node doc = parse_yaml(source);
  const located l = locate(doc, name);
  const table_form form = l.value->is_mapping() ? table_form::columns : table_form::csv;
  const text_index text(source);
  const bool flow_parent = l.named && is_flow(*l.data, text);
  if (flow_parent || l.key == nullptr) {
    // A block scalar cannot go inside a flow mapping: write `data` anew.
    std::vector<data_table> tables = read_tables(source);
    for (data_table& d : tables) {
      if (d.name == name) {
        d.cells = t;
      }
    }
    return replace_data_entry(source, data_lines(tables));
  }
  std::string out(source);
  const size_t from = text.offset(l.value->line, l.value->column);
  const size_t to = value_end(*l.value, text);
  std::string value;
  if (form == table_form::columns && is_flow(*l.value, text)) {
    const std::vector<std::string> lines = column_lines(t);
    value = "{" + join(lines, "", ", ") + "}";
  } else if (form == table_form::columns) {
    // A block mapping's text starts at its first key, already indented.
    const std::vector<std::string> lines = column_lines(t);
    const std::string indent(static_cast<size_t>(l.value->column) - 1, ' ');
    value = lines.empty() ? std::string("{}") : join(lines, indent, text.eol());
  } else {
    value = block_value(t, form, static_cast<size_t>(l.key->column) - 1, text.eol());
  }
  out.replace(from, to - from, value);
  return out;
}

std::string add_table(std::string_view source, std::string_view name, const table_text& t, std::string_view first_name) {
  const yaml_node doc = parse_yaml(source);
  const yaml_node* data = doc.is_mapping() ? doc.find("data") : nullptr;
  if (data == nullptr) {
    if (!name.empty()) {
      check_name(name);
    }
    return replace_data_entry(source, data_lines({data_table{std::string(name), table_form::csv, t}}));
  }
  check_name(name);
  std::vector<data_table> tables = read_tables(source);
  if (std::any_of(tables.begin(), tables.end(), [&](const data_table& d) { return d.name == name; })) {
    throw error(data->line, data->column, "the data already has a table '" + std::string(name) + "'");
  }
  const bool named = !tables.empty() && !tables.front().name.empty();
  if (!named) {
    // One unnamed table: it becomes `first_name`, and its references say so.
    check_name(first_name);
    if (first_name == name) {
      throw error(0, 0, "the two tables need different names");
    }
    if (!tables.empty()) {
      tables.front().name = std::string(first_name);
    }
    tables.push_back(data_table{std::string(name), table_form::csv, t});
    const std::string converted = replace_data_entry(source, data_lines(tables));
    const std::string prefix(first_name);
    return rewrite_references(converted, [&](std::string_view ref) -> std::optional<std::string> {
      if (ref.find(':') != std::string_view::npos) {
        return std::nullopt;
      }
      return prefix + ":" + std::string(ref);
    });
  }
  const text_index text(source);
  if (is_flow(*data, text) || data->keys.empty()) {
    tables.push_back(data_table{std::string(name), table_form::csv, t});
    return replace_data_entry(source, data_lines(tables));
  }
  const size_t indent = static_cast<size_t>(data->keys.front().column) - 1;
  std::string out(source);
  out.insert(
      text.line_end(last_line(*data, text)),
      std::string(text.eol()) + std::string(indent, ' ') + key_text(name) + ": " +
          block_value(t, table_form::csv, indent, text.eol()));
  return out;
}

std::string remove_table(std::string_view source, std::string_view name) {
  const yaml_node doc = parse_yaml(source);
  const located l = locate(doc, name);
  if (!l.named || l.data->keys.size() == 1) {
    return replace_top_level(source, "data", "").source;
  }
  const text_index text(source);
  if (is_flow(*l.data, text)) {
    std::vector<data_table> tables = read_tables(source);
    tables.erase(std::remove_if(tables.begin(), tables.end(), [&](const data_table& d) { return d.name == name; }), tables.end());
    return replace_data_entry(source, data_lines(tables));
  }
  const int first = l.key->line;
  const int last = last_line(*l.value, text);
  std::string out(source);
  size_t from = text.line_start(first);
  const size_t to = last < static_cast<int>(text.line_count()) ? text.line_start(last + 1) : source.size();
  if (to == source.size() && first > 1 && source.back() != '\n') {
    from = text.line_end(first - 1);
  }
  out.erase(from, to - from);
  return out;
}

std::string rename_table(std::string_view source, std::string_view from, std::string_view to) {
  check_name(to);
  const yaml_node doc = parse_yaml(source);
  const located l = locate(doc, from);
  if (!l.named) {
    throw error(0, 0, "the single table has no name to change; add a second table to name both");
  }
  if (from == to) {
    return std::string(source);
  }
  if (l.data->find(to) != nullptr) {
    throw error(l.data->line, l.data->column, "the data already has a table '" + std::string(to) + "'");
  }
  const text_index text(source);
  std::string out(source);
  const size_t key_from = text.offset(l.key->line, l.key->column);
  out.replace(key_from, text.offset(l.key->end_line, l.key->end_column) - key_from, key_text(to));
  const std::string old_prefix = std::string(from) + ":";
  const std::string new_prefix = std::string(to) + ":";
  return rewrite_references(out, [&](std::string_view ref) -> std::optional<std::string> {
    if (ref.substr(0, old_prefix.size()) != old_prefix) {
      return std::nullopt;
    }
    return new_prefix + std::string(ref.substr(old_prefix.size()));
  });
}

std::string rename_column(std::string_view source, std::string_view table_name, std::string_view from, std::string_view to) {
  if (to.empty()) {
    throw error(0, 0, "a column needs a name");
  }
  std::vector<data_table> tables = read_tables(source);
  const auto it = std::find_if(tables.begin(), tables.end(), [&](const data_table& d) { return d.name == table_name; });
  if (it == tables.end()) {
    throw error(0, 0, "the data has no table '" + std::string(table_name) + "'");
  }
  std::vector<std::string>& headers = it->cells.headers;
  const auto column = std::find(headers.begin(), headers.end(), from);
  if (column == headers.end()) {
    throw error(0, 0, "the table has no column '" + std::string(from) + "'");
  }
  if (from == to) {
    return std::string(source);
  }
  if (std::find(headers.begin(), headers.end(), to) != headers.end()) {
    throw error(0, 0, "the table already has a column '" + std::string(to) + "'");
  }
  *column = std::string(to);
  const std::string changed = set_table(source, table_name, it->cells);
  const std::string old_ref = table_name.empty() ? std::string(from) : std::string(table_name) + ":" + std::string(from);
  const std::string new_ref = table_name.empty() ? std::string(to) : std::string(table_name) + ":" + std::string(to);
  return rewrite_references(changed, [&](std::string_view ref) -> std::optional<std::string> {
    return ref == old_ref ? std::optional<std::string>(new_ref) : std::nullopt;
  });
}

std::vector<column_use> find_column_uses(std::string_view source) {
  std::vector<column_use> out;
  const yaml_node doc = parse_yaml(source);
  const yaml_node* root = doc.is_mapping() ? doc.find("root") : nullptr;
  if (root == nullptr || !root->is_mapping()) {
    return out;
  }
  std::vector<std::pair<std::string, const yaml_node*>> elements;
  collect_elements(*root, "root", elements);
  const auto note = [&](const std::string& path, const std::string& key, const yaml_node& value) {
    if (value.is_scalar() && value.text.size() > 1 && value.text[0] == '$' && value.text[1] != '$') {
      out.push_back({path, key, value.text.substr(1)});
    }
  };
  for (const auto& [path, element] : elements) {
    for (size_t k = 0; k < element->keys.size(); ++k) {
      const std::string& key = element->keys[k].text;
      if (key == "type" || key == "children") {
        continue;
      }
      const yaml_node& value = element->children[k];
      note(path, key, value);
      if (value.is_sequence()) {
        for (const yaml_node& item : value.children) {
          note(path, key, item);
        }
      }
    }
  }
  return out;
}

bool refers_to(std::string_view reference, std::string_view table, size_t column, std::string_view header) {
  std::string_view name = reference;
  if (const size_t colon = reference.find(':'); colon != std::string_view::npos) {
    if (reference.substr(0, colon) != table) {
      return false;
    }
    name = reference.substr(colon + 1);
  } else if (!table.empty()) {
    return false;
  }
  if (name == header) {
    return true;
  }
  size_t position = 0;
  const bool digits = !name.empty() && name.find_first_not_of("0123456789") == std::string_view::npos;
  for (const char c : name) {
    position = position * 10 + static_cast<size_t>(c - '0');
  }
  return digits && position == column + 1;
}

std::string rewrite_references(
    std::string_view source,
    const std::function<std::optional<std::string>(std::string_view reference)>& rewrite) {
  const yaml_node doc = parse_yaml(source);
  const yaml_node* root = doc.is_mapping() ? doc.find("root") : nullptr;
  if (root == nullptr || !root->is_mapping()) {
    return std::string(source);
  }
  std::vector<std::pair<std::string, const yaml_node*>> elements;
  collect_elements(*root, "root", elements);
  struct patch {
    std::string element;
    std::string key;
    std::string value;
  };
  std::vector<patch> patches;
  for (const auto& [path, element] : elements) {
    for (size_t k = 0; k < element->keys.size(); ++k) {
      const std::string& key = element->keys[k].text;
      const yaml_node& value = element->children[k];
      if (key == "type" || key == "children") {
        continue;
      }
      if (value.is_scalar()) {
        if (const std::optional<std::string> to = rewritten(value, rewrite)) {
          patches.push_back({path, key, encode_text(*to)});
        }
      } else if (value.is_sequence()) {
        bool changed = false;
        std::string list = "[";
        for (size_t i = 0; i < value.children.size(); ++i) {
          const yaml_node& item = value.children[i];
          if (!item.is_scalar()) {
            changed = false;
            break;
          }
          const std::optional<std::string> to = rewritten(item, rewrite);
          changed = changed || to.has_value();
          list += (i ? ", " : "") + encode_text(to ? *to : item.text);
        }
        if (changed) {
          patches.push_back({path, key, list + "]"});
        }
      }
    }
  }
  std::string out(source);
  for (const patch& p : patches) {
    out = set_field(out, p.element, p.key, p.value);
  }
  return out;
}

} // namespace bdg::nymph
