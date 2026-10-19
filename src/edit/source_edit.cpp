// MIT License © 2026 Binary Dice Games
/// @file source_edit.cpp
/// @brief Text patches to a nymph YAML source (see source_edit.hpp).
#include "edit/source_edit.hpp"
#include <charconv>
#include <cstddef>
#include <vector>
#include "core/error.hpp"

namespace bdg::nymph {

namespace {

/// Byte offsets of the source's lines, to turn libyaml's (line, column in
/// characters) positions into byte offsets.
class text_index {
 public:
  explicit text_index(std::string_view text) : text_(text) {
    starts_.push_back(0);
    for (size_t i = 0; i < text.size(); ++i) {
      if (text[i] == '\n') {
        starts_.push_back(i + 1);
      }
    }
    // The text after a final line break is not a line.
    if (starts_.size() > 1 && starts_.back() == text.size()) {
      starts_.pop_back();
    }
    crlf_ = text.find("\r\n") != std::string_view::npos;
  }

  size_t line_count() const {
    return starts_.size();
  }

  /// Offset of the first byte of 1-based line `line`; the text's end past
  /// the last line.
  size_t line_start(int line) const {
    return line >= 1 && static_cast<size_t>(line) <= starts_.size() ? starts_[static_cast<size_t>(line) - 1]
                                                                    : text_.size();
  }

  /// Offset just after the last character of 1-based line `line`, before
  /// its line break.
  size_t line_end(int line) const {
    const bool last = line >= 1 && static_cast<size_t>(line) >= starts_.size();
    size_t end = last ? text_.size() : starts_[static_cast<size_t>(line)] - 1;
    if (last && end > line_start(line) && text_[end - 1] == '\n') {
      --end;
    }
    if (end > line_start(line) && text_[end - 1] == '\r') {
      --end;
    }
    return end;
  }

  std::string_view line_text(int line) const {
    return text_.substr(line_start(line), line_end(line) - line_start(line));
  }

  /// Offset of 1-based (line, column in characters).
  size_t offset(int line, int column) const {
    size_t at = line_start(line);
    const size_t end = line_end(line);
    for (int chars = 1; chars < column && at < end; ++chars) {
      ++at;
      while (at < end && (static_cast<unsigned char>(text_[at]) & 0xC0) == 0x80) {
        ++at;
      }
    }
    return at;
  }

  char at(size_t offset) const {
    return offset < text_.size() ? text_[offset] : '\0';
  }

  const char* eol() const {
    return crlf_ ? "\r\n" : "\n";
  }

 private:
  std::string_view text_;
  std::vector<size_t> starts_;
  bool crlf_ = false;
};

[[noreturn]] void fail(const yaml_node& at, const std::string& message) {
  throw error(at.line, at.column, message);
}

bool is_block_scalar(const yaml_node& n) {
  return n.is_scalar() && (n.style == scalar_style::literal || n.style == scalar_style::folded);
}

/// Whether `n` is written `[...]` or `{...}`.
bool is_flow(const yaml_node& n, const text_index& t) {
  if (n.is_scalar()) {
    return false;
  }
  const char c = t.at(t.offset(n.line, n.column));
  return c == '[' || c == '{';
}

bool blank_or_comment(std::string_view line) {
  const size_t i = line.find_first_not_of(" \t");
  return i == std::string_view::npos || line[i] == '#';
}

/// The 1-based last line `n` occupies.
int last_line(const yaml_node& n, const text_index& t) {
  if (is_block_scalar(n)) {
    int last = n.end_line - (n.end_column == 1 ? 1 : 0);
    while (last > n.line && blank_or_comment(t.line_text(last))) {
      --last;
    }
    return last;
  }
  if (n.is_scalar() || is_flow(n, t) || n.children.empty()) {
    return n.end_line > n.line ? n.end_line : n.line;
  }
  return last_line(n.children.back(), t);
}

/// Offset just after the value `n`.
size_t value_end(const yaml_node& n, const text_index& t) {
  if ((n.is_scalar() && !is_block_scalar(n)) || is_flow(n, t)) {
    return t.offset(n.end_line, n.end_column);
  }
  return t.line_end(last_line(n, t));
}

std::vector<std::string_view> split_path(std::string_view path) {
  std::vector<std::string_view> out;
  while (true) {
    const size_t slash = path.find('/');
    out.push_back(path.substr(0, slash));
    if (slash == std::string_view::npos) {
      return out;
    }
    path.remove_prefix(slash + 1);
  }
}

/// The element mapping at `path`, or an error naming the first step that
/// does not exist.
const yaml_node& element_at(const yaml_node& doc, std::string_view path) {
  if (!doc.is_mapping()) {
    fail(doc, "a nymph source is a mapping with a 'root' key");
  }
  const std::vector<std::string_view> steps = split_path(path);
  const yaml_node* node = steps[0] == "root" ? doc.find("root") : nullptr;
  if (node == nullptr) {
    fail(doc, "the source has no element '" + std::string(path) + "'");
  }
  for (size_t i = 1; i < steps.size(); ++i) {
    const yaml_node* list = steps[i] == "children" ? node->find("children") : nullptr;
    size_t index = 0;
    const std::string_view number = i + 1 < steps.size() ? steps[i + 1] : std::string_view();
    const bool numeric = !number.empty() &&
        std::from_chars(number.data(), number.data() + number.size(), index).ec == std::errc() &&
        list != nullptr && index < list->children.size();
    if (!numeric) {
      fail(*node, "the source has no element '" + std::string(path) + "'");
    }
    node = &list->children[index];
    ++i; // the index step
  }
  if (!node->is_mapping()) {
    fail(*node, "the element '" + std::string(path) + "' is not a mapping");
  }
  return *node;
}

int key_index(const yaml_node& element, std::string_view field) {
  for (size_t i = 0; i < element.keys.size(); ++i) {
    if (element.keys[i].text == field) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

std::string key_text(std::string_view field, std::string_view value) {
  std::string out(field);
  out += ": ";
  out += value;
  return out;
}

void check_value(std::string_view value) {
  if (value.find_first_of("\r\n") != std::string_view::npos) {
    throw error(0, 0, "a field value is written on one line");
  }
}

} // namespace

const yaml_node* find_element(const yaml_node& doc, std::string_view path) {
  try {
    return &element_at(doc, path);
  } catch (const error&) {
    return nullptr;
  }
}

std::optional<std::string>
field_reference(const yaml_node& doc, std::string_view element_path, std::string_view field) {
  const yaml_node* element = find_element(doc, element_path);
  const yaml_node* value = element != nullptr ? element->find(field) : nullptr;
  if (value != nullptr && value->is_scalar() && !value->quoted() && value->text.size() > 1 && value->text[0] == '$' &&
      value->text[1] != '$') {
    return value->text;
  }
  return std::nullopt;
}

std::string
set_field(std::string_view source, std::string_view element_path, std::string_view field, std::string_view yaml_value) {
  check_value(yaml_value);
  const yaml_node doc = parse_yaml(source);
  const yaml_node& element = element_at(doc, element_path);
  const text_index t(source);
  std::string out(source);

  const int existing = key_index(element, field);
  if (existing >= 0) {
    const yaml_node& key = element.keys[static_cast<size_t>(existing)];
    const yaml_node& value = element.children[static_cast<size_t>(existing)];
    if (value.is_scalar() && value.style == scalar_style::plain && value.text.empty()) {
      // `title:` with nothing after it: insert after the colon.
      size_t at = t.offset(key.end_line, key.end_column);
      while (t.at(at) == ' ') {
        ++at;
      }
      if (t.at(at) == ':') {
        out.insert(at + 1, " " + std::string(yaml_value));
        return out;
      }
    }
    const size_t from = t.offset(value.line, value.column);
    out.replace(from, value_end(value, t) - from, yaml_value);
    return out;
  }

  if (is_flow(element, t)) {
    if (element.children.empty()) {
      out.insert(t.offset(element.line, element.column) + 1, key_text(field, yaml_value));
    } else {
      out.insert(value_end(element.children.back(), t), ", " + key_text(field, yaml_value));
    }
    return out;
  }
  // Block mapping: a new line after the last entry, aligned with the keys.
  const std::string indent(static_cast<size_t>(element.keys.front().column) - 1, ' ');
  out.insert(t.line_end(last_line(element.children.back(), t)), t.eol() + indent + key_text(field, yaml_value));
  return out;
}

std::string remove_field(std::string_view source, std::string_view element_path, std::string_view field) {
  const yaml_node doc = parse_yaml(source);
  const yaml_node& element = element_at(doc, element_path);
  const int found = key_index(element, field);
  std::string out(source);
  if (found < 0) {
    return out;
  }
  const text_index t(source);
  const size_t i = static_cast<size_t>(found);
  if (is_flow(element, t)) {
    const size_t key_start = t.offset(element.keys[i].line, element.keys[i].column);
    if (i + 1 < element.keys.size()) {
      const size_t next = t.offset(element.keys[i + 1].line, element.keys[i + 1].column);
      out.erase(key_start, next - key_start);
    } else if (i > 0) {
      const size_t from = value_end(element.children[i - 1], t);
      out.erase(from, value_end(element.children[i], t) - from);
    } else {
      out.erase(key_start, value_end(element.children[i], t) - key_start);
    }
    return out;
  }
  const int first = element.keys[i].line;
  const int last = last_line(element.children[i], t);
  const std::string_view before = t.line_text(first).substr(0, t.offset(first, element.keys[i].column) - t.line_start(first));
  if (before.find_first_not_of(' ') != std::string_view::npos) {
    fail(element.keys[i], "the first key of a list item cannot be removed here");
  }
  size_t from = t.line_start(first);
  size_t to = last < static_cast<int>(t.line_count()) ? t.line_start(last + 1) : source.size();
  if (to == source.size() && first > 1 && source.back() != '\n') {
    from = t.line_end(first - 1); // the last line: take the break before it
  }
  out.erase(from, to - from);
  return out;
}

} // namespace bdg::nymph
