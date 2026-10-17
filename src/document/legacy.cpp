// MIT License © 2026 Binary Dice Games
/// @file legacy.cpp
/// @brief is_wish_source() / convert_wish_source(). The splitting rule is
///        wish's `parse_document()`: only the first two separator lines
///        count, so the CSV part may contain more.
#include "document/legacy.hpp"
#include <vector>
#include "core/error.hpp"
#include "document/yaml_doc.hpp"

namespace bdg::nymph {

namespace {

bool is_separator(std::string_view line) {
  while (!line.empty() && (line.back() == ' ' || line.back() == '\t' || line.back() == '\r')) {
    line.remove_suffix(1);
  }
  return line == "--";
}

/// The three parts, if `text` has two separator lines.
struct parts {
  std::string_view description;
  std::string_view format;
  std::string_view data;
};

std::optional<parts> split(std::string_view text) {
  size_t begin[2] = {0, 0};
  size_t end[2] = {0, 0};
  int found = 0;
  size_t pos = 0;
  while (pos < text.size() && found < 2) {
    const size_t eol = text.find('\n', pos);
    const size_t next = eol == std::string_view::npos ? text.size() : eol + 1;
    const size_t content_end = eol == std::string_view::npos ? text.size() : eol;
    if (is_separator(text.substr(pos, content_end - pos))) {
      begin[found] = pos;
      end[found] = next;
      ++found;
    }
    pos = next;
  }
  if (found < 2) {
    return std::nullopt;
  }
  return parts{text.substr(0, begin[0]), text.substr(end[0], begin[1] - end[0]), text.substr(end[1])};
}

std::vector<std::string_view> lines_of(std::string_view text) {
  std::vector<std::string_view> out;
  size_t pos = 0;
  while (pos < text.size()) {
    const size_t eol = text.find('\n', pos);
    std::string_view line = text.substr(pos, eol == std::string_view::npos ? std::string_view::npos : eol - pos);
    if (!line.empty() && line.back() == '\r') {
      line.remove_suffix(1);
    }
    out.push_back(line);
    if (eol == std::string_view::npos) {
      break;
    }
    pos = eol + 1;
  }
  return out;
}

bool blank(std::string_view s) {
  return s.find_first_not_of(" \t\r\n") == std::string_view::npos;
}

/// `key: |...` followed by `text` indented by two spaces. `strip` drops the
/// final line break (`|-`). An indentation indicator is added when the
/// first non-blank line starts with a space, which YAML would otherwise take
/// as the block's indentation.
std::string block(std::string_view key, std::string_view text, bool strip) {
  std::vector<std::string_view> lines = lines_of(text);
  while (!lines.empty() && blank(lines.back())) {
    lines.pop_back();
  }
  while (!lines.empty() && blank(lines.front())) {
    lines.erase(lines.begin());
  }
  std::string out(key);
  out += ": |";
  if (!lines.empty() && lines.front().front() == ' ') {
    out += '2';
  }
  out += strip ? "-\n" : "\n";
  for (std::string_view line : lines) {
    out += line.empty() ? "" : "  ";
    out += line;
    out += '\n';
  }
  return out;
}

} // namespace

bool is_wish_source(std::string_view text) {
  if (!split(text)) {
    return false;
  }
  try {
    const yaml_node doc = parse_yaml(text);
    return !(doc.is_mapping() && doc.find("root") != nullptr);
  } catch (const error&) {
    return true; // not YAML at all: the three-part form
  }
}

std::optional<std::string> convert_wish_source(std::string_view text) {
  if (!is_wish_source(text)) {
    return std::nullopt;
  }
  const parts p = *split(text);
  std::string out = "nymph: 1\n";
  if (!blank(p.description)) {
    out += block("description", p.description, true);
  }
  out += p.format;
  if (!out.empty() && out.back() != '\n') {
    out += '\n';
  }
  if (!blank(p.data)) {
    out += block("data", p.data, false);
  }
  return out;
}

} // namespace bdg::nymph
