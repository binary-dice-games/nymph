// MIT License © 2026 Binary Dice Games
/// @file yaml_language.cpp
/// @brief yaml_language() (see yaml_language.hpp).
#include "workspace/yaml_language.hpp"
#include "elements/register_builtin.hpp"
#include "schema/element_class.hpp"
#include "schema/registry.hpp"

namespace bdg::nymph {

namespace {

using iterator = TextEditor::Iterator;

bool is_name_start(ImWchar c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool is_name(ImWchar c) {
  return is_name_start(c) || (c >= '0' && c <= '9') || c == '-' || c == '.';
}

/// A mapping key (`name:` followed by a space or the end), a `$` reference
/// or nothing (the colorizer's identifiers, numbers and punctuation then
/// apply).
iterator tokenize(iterator start, iterator end, TextEditor::Color& color) {
  iterator i = start;
  if (i < end && *i == '$') {
    ++i;
    while (i < end && (is_name(*i) || *i == ':')) {
      ++i;
    }
    if (i - start > 1) {
      color = TextEditor::Color::preprocessor;
      return i;
    }
    return start;
  }
  if (i < end && is_name_start(*i)) {
    while (i < end && is_name(*i)) {
      ++i;
    }
    iterator after = i;
    if (after < end && *after == ':') {
      iterator next = after;
      ++next;
      if (next == end || *next == ' ' || *next == '\t') {
        color = TextEditor::Color::keyword;
        return i; // the key; ':' is punctuation
      }
    }
  }
  return start;
}

iterator identifier(iterator start, iterator end) {
  iterator i = start;
  if (i < end && is_name_start(*i)) {
    while (i < end && is_name(*i)) {
      ++i;
    }
  }
  return i;
}

iterator number(iterator start, iterator end) {
  iterator i = start;
  if (i < end && (*i == '-' || *i == '+')) {
    ++i;
  }
  const iterator digits = i;
  while (i < end && ((*i >= '0' && *i <= '9') || *i == '.' || *i == 'e' || *i == 'E')) {
    ++i;
  }
  if (i == digits || !(*digits >= '0' && *digits <= '9') && !(*digits == '.')) {
    return start;
  }
  return i;
}

} // namespace

const TextEditor::Language* yaml_language() {
  static TextEditor::Language language = [] {
    TextEditor::Language l;
    l.name = "nymph YAML";
    l.singleLineComment = "#";
    l.hasSingleQuotedStrings = true;
    l.hasDoubleQuotedStrings = true;
    l.stringEscape = '\\';
    l.indentationForBlocks = true;
    for (const char* k : {"true", "false", "null", "yes", "no", "on", "off"}) {
      l.declarations.insert(k);
    }
    for (const element_class* cls : builtin_registry().classes()) {
      l.identifiers.insert(std::string(cls->name()));
    }
    l.isPunctuation = [](ImWchar c) {
      return c == ':' || c == '-' || c == '[' || c == ']' || c == '{' || c == '}' || c == ',' || c == '|' || c == '>';
    };
    l.customTokenizer = tokenize;
    l.getIdentifier = identifier;
    l.getNumber = number;
    return l;
  }();
  return &language;
}

std::vector<yaml_token> yaml_tokens(std::string_view line) {
  // The editor's colorizer, in the order it tries things (ASCII lines):
  // whitespace, comments, strings, the custom tokenizer, identifiers,
  // numbers, punctuation.
  const TextEditor::Language& l = *yaml_language();
  std::vector<TextEditor::Glyph> glyphs;
  for (const char c : line) {
    glyphs.emplace_back(static_cast<ImWchar>(static_cast<unsigned char>(c)));
  }
  std::vector<yaml_token> out;
  TextEditor::Glyph* const first = glyphs.data();
  const iterator end(first + glyphs.size());
  size_t at = 0;
  const auto push = [&](size_t from, size_t to, TextEditor::Color color) {
    out.push_back({std::string(line.substr(from, to - from)), color});
    at = to;
  };
  while (at < glyphs.size()) {
    const char c = line[at];
    if (c == ' ' || c == '\t') {
      ++at;
    } else if (c == '#') {
      push(at, line.size(), TextEditor::Color::comment);
    } else if (c == '"' || c == '\'') {
      size_t to = at + 1;
      while (to < line.size() && line[to] != c) {
        to += line[to] == '\\' ? 2 : 1;
      }
      push(at, std::min(to + 1, line.size()), TextEditor::Color::string);
    } else {
      const iterator start(first + at);
      TextEditor::Color color = TextEditor::Color::text;
      iterator stop = l.customTokenizer(start, end, color);
      if (stop != start) {
        push(at, at + (stop - start), color);
        continue;
      }
      stop = l.getIdentifier(start, end);
      if (stop != start) {
        const std::string word(line.substr(at, stop - start));
        color = l.keywords.contains(word) ? TextEditor::Color::keyword
            : l.declarations.contains(word) ? TextEditor::Color::declaration
            : l.identifiers.contains(word)  ? TextEditor::Color::knownIdentifier
                                            : TextEditor::Color::identifier;
        push(at, at + word.size(), color);
        continue;
      }
      stop = l.getNumber(start, end);
      if (stop != start) {
        push(at, at + (stop - start), TextEditor::Color::number);
        continue;
      }
      push(at, at + 1, l.isPunctuation(static_cast<ImWchar>(c)) ? TextEditor::Color::punctuation : TextEditor::Color::text);
    }
  }
  return out;
}

} // namespace bdg::nymph
