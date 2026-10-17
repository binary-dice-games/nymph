// MIT License © 2026 Binary Dice Games
/// @file yaml_doc.cpp
/// @brief parse_yaml(): libyaml's document API to a positioned yaml_node tree.
#include "document/yaml_doc.hpp"
#include <yaml.h>
#include <string>
#include "core/error.hpp"

namespace bdg::nymph {

const yaml_node* yaml_node::find(std::string_view key) const {
  for (size_t i = 0; i < keys.size(); ++i) {
    if (keys[i].text == key) {
      return &children[i];
    }
  }
  return nullptr;
}

const yaml_node* yaml_node::find_key(std::string_view key) const {
  for (const yaml_node& k : keys) {
    if (k.text == key) {
      return &k;
    }
  }
  return nullptr;
}

namespace {

/// Nesting deeper than this is rejected, so the recursive conversion cannot
/// overflow the stack.
constexpr int kMaxDepth = 200;

// ── libyaml RAII ─────────────────────────────────────────────────────────────

struct parser_guard {
  yaml_parser_t parser;
  parser_guard() {
    yaml_parser_initialize(&parser);
  }
  ~parser_guard() {
    yaml_parser_delete(&parser);
  }
  parser_guard(const parser_guard&) = delete;
  parser_guard& operator=(const parser_guard&) = delete;
};

struct document_guard {
  yaml_document_t doc{};
  bool loaded = false;
  ~document_guard() {
    if (loaded) {
      yaml_document_delete(&doc);
    }
  }
};

// ── Source positions ─────────────────────────────────────────────────────────

/// Byte offsets of the start of every line, for mapping libyaml's
/// (line, column-in-characters) marks back to bytes.
class line_index {
 public:
  explicit line_index(std::string_view text) : text_(text) {
    starts_.push_back(0);
    for (size_t i = 0; i < text.size(); ++i) {
      if (text[i] == '\n') {
        starts_.push_back(i + 1);
      }
    }
  }

  /// The text of 0-based line `line` without its line break; empty past
  /// the end.
  std::string_view line(size_t line) const {
    if (line >= starts_.size()) {
      return {};
    }
    const size_t begin = starts_[line];
    size_t end = line + 1 < starts_.size() ? starts_[line + 1] - 1 : text_.size();
    if (end > begin && text_[end - 1] == '\r') {
      --end;
    }
    return text_.substr(begin, end - begin);
  }

  /// The byte at 0-based (line, column-in-characters), or '\0'.
  char char_at(size_t line_no, size_t column) const {
    const std::string_view l = line(line_no);
    size_t chars = 0;
    for (size_t i = 0; i < l.size(); ++i) {
      if ((static_cast<unsigned char>(l[i]) & 0xC0) == 0x80) {
        continue; // UTF-8 continuation byte
      }
      if (chars == column) {
        return l[i];
      }
      ++chars;
    }
    return '\0';
  }

  size_t line_count() const {
    return starts_.size();
  }

 private:
  std::string_view text_;
  std::vector<size_t> starts_;
};

int to_line(const yaml_mark_t& m) {
  return static_cast<int>(m.line) + 1;
}

int to_column(const yaml_mark_t& m) {
  return static_cast<int>(m.column) + 1;
}

[[noreturn]] void throw_parser_error(const yaml_parser_t& p, const line_index& lines) {
  const yaml_mark_t& mark = p.problem ? p.problem_mark : p.context_mark;
  if (lines.char_at(mark.line, mark.column) == '\t') {
    throw error(to_line(mark), to_column(mark), "tab character in indentation; YAML indents with spaces only");
  }
  std::string message = p.problem ? p.problem : "invalid YAML";
  if (p.context) {
    // libyaml notices a problem where it stops making sense, often after
    // the cause; where the enclosing construct began helps find it.
    message += std::string(" (") + p.context + " that started at line " + std::to_string(to_line(p.context_mark)) +
        ", column " + std::to_string(to_column(p.context_mark)) + ")";
  }
  throw error(to_line(mark), to_column(mark), message);
}

// ── Conversion ───────────────────────────────────────────────────────────────

scalar_style to_style(yaml_scalar_style_t s) {
  switch (s) {
    case YAML_SINGLE_QUOTED_SCALAR_STYLE:
      return scalar_style::single_quoted;
    case YAML_DOUBLE_QUOTED_SCALAR_STYLE:
      return scalar_style::double_quoted;
    case YAML_LITERAL_SCALAR_STYLE:
      return scalar_style::literal;
    case YAML_FOLDED_SCALAR_STYLE:
      return scalar_style::folded;
    default:
      return scalar_style::plain;
  }
}

class converter {
 public:
  converter(yaml_document_t& doc, const line_index& lines) : doc_(doc), lines_(lines) {}

  yaml_node convert(const yaml_node_t& in, int depth) {
    if (depth > kMaxDepth) {
      throw error(to_line(in.start_mark), to_column(in.start_mark), "nesting is too deep");
    }
    if (++count_ > kMaxYamlNodes) {
      throw error(
          to_line(in.start_mark), to_column(in.start_mark), "the document is too large (too many nodes after aliases)");
    }
    yaml_node out;
    out.line = to_line(in.start_mark);
    out.column = to_column(in.start_mark);
    switch (in.type) {
      case YAML_SCALAR_NODE:
        out.kind = yaml_node::kind_t::scalar;
        out.text.assign(reinterpret_cast<const char*>(in.data.scalar.value), in.data.scalar.length);
        out.style = to_style(in.data.scalar.style);
        if (out.style == scalar_style::literal || out.style == scalar_style::folded) {
          set_block_content(in, out);
        }
        break;
      case YAML_SEQUENCE_NODE:
        out.kind = yaml_node::kind_t::sequence;
        for (yaml_node_item_t* item = in.data.sequence.items.start; item < in.data.sequence.items.top; ++item) {
          out.children.push_back(convert(*node(*item), depth + 1));
        }
        break;
      case YAML_MAPPING_NODE:
        out.kind = yaml_node::kind_t::mapping;
        for (yaml_node_pair_t* pair = in.data.mapping.pairs.start; pair < in.data.mapping.pairs.top; ++pair) {
          yaml_node key = convert(*node(pair->key), depth + 1);
          check_key(out, key);
          out.keys.push_back(std::move(key));
          out.children.push_back(convert(*node(pair->value), depth + 1));
        }
        break;
      default:
        throw error(out.line, out.column, "unsupported YAML node");
    }
    return out;
  }

 private:
  const yaml_node_t* node(int id) const {
    return yaml_document_get_node(&doc_, id);
  }

  static void check_key(const yaml_node& mapping, const yaml_node& key) {
    if (!key.is_scalar()) {
      throw error(key.line, key.column, "a mapping key must be plain text, not a list or a mapping");
    }
    if (const yaml_node* first = mapping.find_key(key.text)) {
      throw error(
          key.line,
          key.column,
          "duplicate key '" + key.text + "' (first used at line " + std::to_string(first->line) + ")");
    }
  }

  /// The block scalar's text starts on the line after its `|` / `>`
  /// indicator; its column is the indentation of its first non-blank line.
  void set_block_content(const yaml_node_t& in, yaml_node& out) const {
    const size_t first = in.start_mark.line + 1; // 0-based line after the indicator
    out.content_line = static_cast<int>(first) + 1;
    for (size_t l = first; l <= in.end_mark.line && l < lines_.line_count(); ++l) {
      const std::string_view text = lines_.line(l);
      const size_t indent = text.find_first_not_of(' ');
      if (indent != std::string_view::npos) {
        out.content_column = static_cast<int>(indent) + 1;
        return;
      }
    }
  }

  yaml_document_t& doc_;
  const line_index& lines_;
  size_t count_ = 0;
};

} // namespace

yaml_node parse_yaml(std::string_view text) {
  const line_index lines(text);
  parser_guard p;
  yaml_parser_set_input_string(&p.parser, reinterpret_cast<const unsigned char*>(text.data()), text.size());

  document_guard first;
  if (!yaml_parser_load(&p.parser, &first.doc)) {
    throw_parser_error(p.parser, lines);
  }
  first.loaded = true;
  const yaml_node_t* root = yaml_document_get_root_node(&first.doc);
  if (root == nullptr) {
    throw error(1, 1, "the document is empty");
  }
  converter conv(first.doc, lines);
  yaml_node out = conv.convert(*root, 0);

  // Exactly one document: anything after it is an error.
  document_guard second;
  if (!yaml_parser_load(&p.parser, &second.doc)) {
    throw_parser_error(p.parser, lines);
  }
  second.loaded = true;
  if (yaml_document_get_root_node(&second.doc) != nullptr) {
    throw error(
        to_line(second.doc.start_mark),
        to_column(second.doc.start_mark),
        "a second YAML document; a nymph source is a single document");
  }
  return out;
}

} // namespace bdg::nymph
