// MIT License © 2026 Binary Dice Games
/// @file yaml_text.hpp
/// @brief Internal helpers of the source patchers (source_edit, tree_edit,
///        data_edit): byte offsets of parsed YAML positions, where a node's
///        text ends, and the element mapping at a path. Not a public API.
#pragma once

#include <charconv>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include "core/error.hpp"
#include "document/yaml_doc.hpp"

namespace bdg::nymph::yaml_text {

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

  /// 1-based line holding byte `offset`.
  int line_of(size_t offset) const {
    size_t lo = 0;
    size_t hi = starts_.size();
    while (hi - lo > 1) {
      const size_t mid = (lo + hi) / 2;
      if (starts_[mid] <= offset) {
        lo = mid;
      } else {
        hi = mid;
      }
    }
    return static_cast<int>(lo) + 1;
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

[[noreturn]] inline void fail(const yaml_node& at, const std::string& message) {
  throw error(at.line, at.column, message);
}

inline bool is_block_scalar(const yaml_node& n) {
  return n.is_scalar() && (n.style == scalar_style::literal || n.style == scalar_style::folded);
}

/// Whether `n` is written `[...]` or `{...}`.
inline bool is_flow(const yaml_node& n, const text_index& t) {
  if (n.is_scalar()) {
    return false;
  }
  const char c = t.at(t.offset(n.line, n.column));
  return c == '[' || c == '{';
}

inline bool blank_or_comment(std::string_view line) {
  const size_t i = line.find_first_not_of(" \t");
  return i == std::string_view::npos || line[i] == '#';
}

/// The 1-based last line `n` occupies.
inline int last_line(const yaml_node& n, const text_index& t) {
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
inline size_t value_end(const yaml_node& n, const text_index& t) {
  if ((n.is_scalar() && !is_block_scalar(n)) || is_flow(n, t)) {
    return t.offset(n.end_line, n.end_column);
  }
  return t.line_end(last_line(n, t));
}

inline std::vector<std::string_view> split_path(std::string_view path) {
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
inline const yaml_node& element_at(const yaml_node& doc, std::string_view path) {
  if (!doc.is_mapping()) {
    fail(doc, "a nymph source is a mapping with a 'root' key");
  }
  if (path == "image") { // the image options: a mapping, not an element
    const yaml_node* image = doc.find("image");
    if (image == nullptr || !image->is_mapping()) {
      fail(doc, "the source has no image options");
    }
    return *image;
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


} // namespace bdg::nymph::yaml_text
