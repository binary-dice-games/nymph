// MIT License © 2026 Binary Dice Games
/// @file source_edit.cpp
/// @brief Text patches to a nymph YAML source (see source_edit.hpp).
#include "edit/source_edit.hpp"
#include <charconv>
#include <cstddef>
#include <vector>
#include "core/error.hpp"
#include "document/bind.hpp"
#include "edit/yaml_text.hpp"

namespace bdg::nymph {

using namespace yaml_text;

namespace {

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
  if (element_path == "image" && doc.is_mapping() && doc.find("image") == nullptr) {
    // The first image option: the section is new.
    return replace_top_level(source, "image", "{" + std::string(field) + ": " + std::string(yaml_value) + "}").source;
  }
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
  if (element_path == "image" && doc.is_mapping() && doc.find("image") == nullptr) {
    return std::string(source); // nothing written, nothing to remove
  }
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
