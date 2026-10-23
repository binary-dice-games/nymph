// MIT License © 2026 Binary Dice Games
/// @file reader.cpp
/// @brief The mermaid reader: detection, the fence and front matter,
///        comments and directives, the header, accessibility statements,
///        and the text helpers the converters share
///        (spec/16_mermaid_import/DESIGN.md section 3).
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "core/charconv.hpp"
#include "core/error.hpp"
#include "core/suggest.hpp"
#include "edit/field_value.hpp"
#include "mermaid/mermaid_internal.hpp"

namespace bdg::nymph::mermaid {

namespace {

/// The keywords that open a mermaid diagram. Supported ones first.
constexpr std::array kSupported = {
    "flowchart",    "flowchart-elk", "graph",   "sequenceDiagram", "classDiagram", "classDiagram-v2",
    "stateDiagram", "stateDiagram-v2", "erDiagram", "gantt",       "pie",          "mindmap",
    "sankey-beta",  "sankey",        "xychart-beta", "xychart"};
constexpr std::array kUnsupported = {
    "journey",      "timeline",     "quadrantChart",     "gitGraph",          "C4Context",    "C4Container",
    "C4Component",  "C4Dynamic",    "C4Deployment",      "requirementDiagram", "requirement",  "block-beta",
    "block",        "architecture-beta", "architecture", "packet-beta",       "packet",       "kanban",
    "radar-beta",   "radar",        "treemap-beta",      "treemap",           "zenuml",       "info"};

struct raw_line {
  std::string text;
  int number = 0;
};

std::vector<raw_line> split_lines(std::string_view text) {
  if (starts_with(text, "\xEF\xBB\xBF")) {
    text.remove_prefix(3);
  }
  std::vector<raw_line> lines;
  int number = 1;
  size_t at = 0;
  while (at <= text.size()) {
    size_t end = text.find('\n', at);
    if (end == std::string_view::npos) {
      end = text.size();
    }
    std::string_view l = text.substr(at, end - at);
    if (!l.empty() && l.back() == '\r') {
      l.remove_suffix(1);
    }
    lines.push_back({std::string(l), number++});
    if (end == text.size()) {
      break;
    }
    at = end + 1;
  }
  return lines;
}

bool is_blank(std::string_view text) {
  return trim(text).empty();
}

/// The first word of `text` (up to a space, tab or `;`).
std::string_view first_word(std::string_view text) {
  text = trim(text);
  size_t end = 0;
  while (end < text.size() && text[end] != ' ' && text[end] != '\t' && text[end] != ';') {
    ++end;
  }
  return text.substr(0, end);
}

bool is_keyword(std::string_view word) {
  return std::find(kSupported.begin(), kSupported.end(), word) != kSupported.end() ||
      std::find(kUnsupported.begin(), kUnsupported.end(), word) != kUnsupported.end();
}

/// Where the diagram is in the text: the range of lines after the fence
/// and front matter, the front matter range, and the header line index.
struct layout_of_text {
  size_t first = 0; ///< first line of the diagram (after the fence opener)
  size_t end = 0;   ///< one past the last line
  size_t front_begin = 0, front_end = 0; ///< front matter lines, without the --- lines
  bool front = false;
  bool fenced = false;
  size_t fence_end = 0; ///< the closing fence line, when fenced
  size_t header = 0;    ///< index of the header line
  bool found = false;
  bool unclosed_front = false;
};

bool is_fence(std::string_view text, std::string_view& marker) {
  const std::string_view t = trim(text);
  for (std::string_view m : {std::string_view("```"), std::string_view("~~~")}) {
    if (starts_with(t, m) && trim(t.substr(3)) == "mermaid") {
      marker = m;
      return true;
    }
  }
  return false;
}

layout_of_text locate(const std::vector<raw_line>& lines) {
  layout_of_text l;
  l.end = lines.size();
  size_t i = 0;
  while (i < lines.size() && is_blank(lines[i].text)) {
    ++i;
  }
  std::string_view marker;
  if (i < lines.size() && is_fence(lines[i].text, marker)) {
    l.fenced = true;
    l.first = i + 1;
    size_t close = l.first;
    while (close < lines.size() && trim(lines[close].text) != marker) {
      ++close;
    }
    l.end = close;
    l.fence_end = close;
    i = l.first;
  }
  while (i < l.end && is_blank(lines[i].text)) {
    ++i;
  }
  if (i < l.end && trim(lines[i].text) == "---") {
    size_t close = i + 1;
    while (close < l.end && trim(lines[close].text) != "---") {
      ++close;
    }
    if (close >= l.end) {
      l.unclosed_front = true;
      return l;
    }
    l.front = true;
    l.front_begin = i + 1;
    l.front_end = close;
    i = close + 1;
  }
  bool in_directive = false;
  for (; i < l.end; ++i) {
    const std::string_view t = trim(lines[i].text);
    if (in_directive) {
      in_directive = t.find("}%%") == std::string_view::npos;
      continue;
    }
    if (t.empty()) {
      continue;
    }
    if (starts_with(t, "%%{")) {
      in_directive = t.find("}%%") == std::string_view::npos;
      continue;
    }
    if (starts_with(t, "%%")) {
      continue;
    }
    l.header = i;
    l.found = is_keyword(first_word(t));
    return l;
  }
  return l;
}

std::string unquote(std::string_view text) {
  text = trim(text);
  if (text.size() >= 2 && (text.front() == '"' || text.front() == '\'') && text.back() == text.front()) {
    return std::string(text.substr(1, text.size() - 2));
  }
  return std::string(text);
}

void append_utf8(std::string& out, unsigned long code) {
  if (code < 0x80) {
    out += static_cast<char>(code);
  } else if (code < 0x800) {
    out += static_cast<char>(0xC0 | (code >> 6));
    out += static_cast<char>(0x80 | (code & 0x3F));
  } else if (code < 0x10000) {
    out += static_cast<char>(0xE0 | (code >> 12));
    out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (code & 0x3F));
  } else if (code < 0x110000) {
    out += static_cast<char>(0xF0 | (code >> 18));
    out += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
    out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (code & 0x3F));
  }
}

std::string lower(std::string_view text) {
  std::string out(text);
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return char(std::tolower(c)); });
  return out;
}

std::string hex2(int v) {
  char buf[3];
  std::snprintf(buf, sizeof(buf), "%02X", std::clamp(v, 0, 255));
  return buf;
}

} // namespace

// ── line ────────────────────────────────────────────────────────────────────

pos line::at(size_t offset) const {
  int column = 1;
  for (size_t i = 0; i < offset && i < text.size(); ++i) {
    if ((static_cast<unsigned char>(text[i]) & 0xC0) != 0x80) {
      ++column;
    }
  }
  return {number, column};
}

pos line::start() const {
  const size_t first = text.find_first_not_of(" \t");
  return at(first == std::string::npos ? 0 : first);
}

// ── context ─────────────────────────────────────────────────────────────────

void context::fail(pos p, const std::string& message) const {
  throw error(p.line, p.column, message);
}

// ── Text helpers ────────────────────────────────────────────────────────────

std::string_view trim(std::string_view text) {
  const size_t first = text.find_first_not_of(" \t");
  if (first == std::string_view::npos) {
    return {};
  }
  const size_t last = text.find_last_not_of(" \t");
  return text.substr(first, last - first + 1);
}

std::string yaml_text(std::string_view text) {
  if (!text.empty() && text.front() == '$') {
    return encode_text("$" + std::string(text));
  }
  return encode_text(text);
}

std::string label_text(std::string_view raw) {
  std::string_view t = trim(raw);
  if (t.size() >= 2 && t.front() == '"' && t.back() == '"') {
    t = t.substr(1, t.size() - 2);
  }
  bool markdown = false;
  if (t.size() >= 2 && t.front() == '`' && t.back() == '`') {
    t = t.substr(1, t.size() - 2);
    markdown = true;
  }
  std::string out;
  for (size_t i = 0; i < t.size();) {
    const std::string_view rest = t.substr(i);
    if (rest[0] == '<') {
      const std::string l = lower(rest.substr(0, 6));
      if (starts_with(l, "<br>")) {
        out += '\n';
        i += 4;
        continue;
      }
      if (starts_with(l, "<br/>")) {
        out += '\n';
        i += 5;
        continue;
      }
      if (starts_with(l, "<br />")) {
        out += '\n';
        i += 6;
        continue;
      }
    }
    if (rest[0] == '#') {
      const size_t semi = rest.find(';');
      if (semi != std::string_view::npos && semi > 1 && semi < 10) {
        const std::string_view code = rest.substr(1, semi - 1);
        static const std::pair<std::string_view, std::string_view> named[] = {
            {"quot", "\""}, {"amp", "&"}, {"lt", "<"}, {"gt", ">"}, {"nbsp", " "}, {"apos", "'"}};
        bool done = false;
        for (const auto& [name, value] : named) {
          if (code == name) {
            out += value;
            done = true;
          }
        }
        if (!done && std::all_of(code.begin(), code.end(), [](char c) { return std::isdigit((unsigned char)c); })) {
          append_utf8(out, std::strtoul(std::string(code).c_str(), nullptr, 10));
          done = true;
        }
        if (done) {
          i += semi + 1;
          continue;
        }
      }
    }
    if (markdown && (starts_with(rest, "**") || starts_with(rest, "__"))) {
      i += 2;
      continue;
    }
    if (markdown && rest[0] == '*') {
      ++i;
      continue;
    }
    if (markdown && rest[0] == '\n') {
      out += '\n';
      ++i;
      continue;
    }
    out += rest[0];
    ++i;
  }
  // Lines of a markdown string keep their own indentation out.
  if (markdown) {
    std::string cleaned;
    size_t at = 0;
    while (at <= out.size()) {
      size_t end = out.find('\n', at);
      if (end == std::string::npos) {
        end = out.size();
      }
      cleaned += (cleaned.empty() && at == 0 ? "" : "\n") + std::string(trim(std::string_view(out).substr(at, end - at)));
      if (end == out.size()) {
        break;
      }
      at = end + 1;
    }
    out = cleaned;
  }
  return out;
}

bool is_id_char(char c) {
  const unsigned char u = static_cast<unsigned char>(c);
  return std::isalnum(u) != 0 || c == '_' || c == '-' || c == '.' || u >= 0x80;
}

std::optional<std::string> css_color(std::string_view value) {
  const std::string v = lower(trim(value));
  auto hex = [](char c) { return std::isxdigit(static_cast<unsigned char>(c)) != 0; };
  if (!v.empty() && v[0] == '#') {
    const std::string digits = v.substr(1);
    if (!std::all_of(digits.begin(), digits.end(), hex)) {
      return std::nullopt;
    }
    std::string up = digits;
    std::transform(up.begin(), up.end(), up.begin(), [](unsigned char c) { return char(std::toupper(c)); });
    if (up.size() == 3 || up.size() == 4) {
      std::string full;
      for (char c : up.substr(0, 3)) {
        full += std::string(2, c);
      }
      if (up.size() == 4) {
        full += std::string(2, up[3]);
      }
      return "#" + full;
    }
    if (up.size() == 6 || up.size() == 8) {
      return "#" + up;
    }
    return std::nullopt;
  }
  if (starts_with(v, "rgb")) {
    const size_t open = v.find('(');
    const size_t close = v.find(')');
    if (open == std::string::npos || close == std::string::npos || close < open) {
      return std::nullopt;
    }
    std::vector<double> parts;
    std::string token;
    for (char c : v.substr(open + 1, close - open - 1) + ",") {
      if (c == ',' || c == ' ' || c == '/') {
        if (!token.empty()) {
          char* end = nullptr;
          const double d = std::strtod(token.c_str(), &end);
          if (end == token.c_str()) {
            return std::nullopt;
          }
          parts.push_back(token.back() == '%' ? d * 2.55 : d);
          token.clear();
        }
      } else {
        token += c;
      }
    }
    if (parts.size() < 3) {
      return std::nullopt;
    }
    std::string out = "#" + hex2(int(parts[0] + 0.5)) + hex2(int(parts[1] + 0.5)) + hex2(int(parts[2] + 0.5));
    if (parts.size() >= 4 && parts[3] < 1.0) {
      out += hex2(int(parts[3] * 255.0 + 0.5));
    }
    return out;
  }
  static const std::pair<std::string_view, std::string_view> names[] = {
      {"black", "#000000"},     {"white", "#FFFFFF"},     {"red", "#FF0000"},       {"green", "#008000"},
      {"blue", "#0000FF"},      {"yellow", "#FFFF00"},    {"orange", "#FFA500"},    {"purple", "#800080"},
      {"pink", "#FFC0CB"},      {"brown", "#A52A2A"},     {"gray", "#808080"},      {"grey", "#808080"},
      {"cyan", "#00FFFF"},      {"magenta", "#FF00FF"},   {"lime", "#00FF00"},      {"navy", "#000080"},
      {"teal", "#008080"},      {"olive", "#808000"},     {"maroon", "#800000"},    {"silver", "#C0C0C0"},
      {"gold", "#FFD700"},      {"coral", "#FF7F50"},     {"salmon", "#FA8072"},    {"beige", "#F5F5DC"},
      {"lavender", "#E6E6FA"},  {"khaki", "#F0E68C"},     {"violet", "#EE82EE"},    {"indigo", "#4B0082"},
      {"tomato", "#FF6347"},    {"orchid", "#DA70D6"},    {"skyblue", "#87CEEB"},   {"steelblue", "#4682B4"},
      {"lightblue", "#ADD8E6"}, {"lightgreen", "#90EE90"}, {"lightgray", "#D3D3D3"}, {"lightgrey", "#D3D3D3"},
      {"darkgray", "#A9A9A9"},  {"darkgrey", "#A9A9A9"},  {"darkblue", "#00008B"},  {"darkgreen", "#006400"},
      {"darkred", "#8B0000"},   {"lightyellow", "#FFFFE0"}, {"lightpink", "#FFB6C1"}, {"aqua", "#00FFFF"},
      {"fuchsia", "#FF00FF"},   {"crimson", "#DC143C"},   {"turquoise", "#40E0D0"}, {"tan", "#D2B48C"},
      {"wheat", "#F5DEB3"},     {"ivory", "#FFFFF0"},     {"plum", "#DDA0DD"},      {"chocolate", "#D2691E"},
      {"firebrick", "#B22222"}, {"forestgreen", "#228B22"}, {"royalblue", "#4169E1"}, {"slategray", "#708090"},
      {"dodgerblue", "#1E90FF"}, {"mintcream", "#F5FFFA"}, {"honeydew", "#F0FFF0"}, {"aliceblue", "#F0F8FF"}};
  for (const auto& [name, hexcode] : names) {
    if (v == name) {
      return std::string(hexcode);
    }
  }
  return std::nullopt;
}

std::optional<double> parse_number(std::string_view text) {
  text = trim(text);
  if (!text.empty() && text.front() == '+') {
    text.remove_prefix(1);
  }
  double value = 0.0;
  if (text.empty()) {
    return std::nullopt;
  }
  const auto [end, ec] = from_chars_double(text.data(), text.data() + text.size(), value);
  if (ec != std::errc() || end != text.data() + text.size() || !std::isfinite(value)) {
    return std::nullopt;
  }
  return value;
}

std::vector<std::string> split_outside_quotes(std::string_view text, char sep) {
  std::vector<std::string> parts;
  std::string current;
  bool quoted = false;
  for (char c : text) {
    if (c == '"') {
      quoted = !quoted;
    }
    if (c == sep && !quoted) {
      parts.emplace_back(trim(current));
      current.clear();
    } else {
      current += c;
    }
  }
  parts.emplace_back(trim(current));
  return parts;
}

std::string suggestion(std::string_view word, const std::vector<std::string>& names) {
  return did_you_mean(word, names);
}

void check_limit(size_t count, size_t limit, const std::string& what, pos at, context& ctx) {
  if (count > limit) {
    ctx.fail(at, "more than " + std::to_string(limit) + " " + what + " (nymph's limit for one diagram)");
  }
}

// ── Styles ──────────────────────────────────────────────────────────────────

style parse_style(std::string_view list, pos at, context& ctx) {
  style s;
  // Split on commas outside parentheses: rgb(0,0,128) is one value.
  std::vector<std::string> parts;
  std::string current;
  int depth = 0;
  for (char c : list) {
    depth += c == '(' ? 1 : c == ')' && depth > 0 ? -1 : 0;
    if (c == ',' && depth == 0) {
      parts.emplace_back(trim(current));
      current.clear();
    } else {
      current += c;
    }
  }
  parts.emplace_back(trim(current));
  for (const std::string& part : parts) {
    if (part.empty()) {
      continue;
    }
    const size_t colon = part.find(':');
    if (colon == std::string::npos) {
      ctx.warn(at, "style '" + part + "' is ignored (expected property:value)");
      continue;
    }
    const std::string prop = lower(trim(std::string_view(part).substr(0, colon)));
    std::string value(trim(std::string_view(part).substr(colon + 1)));
    if (!value.empty() && value.back() == ';') {
      value.pop_back();
    }
    std::string* target = prop == "fill" ? &s.fill : prop == "stroke" ? &s.stroke : prop == "color" ? &s.color : nullptr;
    if (target == nullptr) {
      ctx.warn(at, "style property '" + prop + "' is ignored (nymph sets colours: fill, stroke, color)");
      continue;
    }
    const std::optional<std::string> c = css_color(value);
    if (!c) {
      ctx.warn(at, "colour '" + value + "' is ignored (use #rrggbb, rgb() or a CSS colour name)");
      continue;
    }
    *target = *c;
  }
  return s;
}

void merge_style(style& base, const style& over) {
  if (!over.fill.empty()) {
    base.fill = over.fill;
  }
  if (!over.stroke.empty()) {
    base.stroke = over.stroke;
  }
  if (!over.color.empty()) {
    base.color = over.color;
  }
}

void apply_style(element& e, const style& s, std::string_view stroke_key) {
  if (!s.stroke.empty()) {
    e.set(stroke_key, encode_text(s.stroke));
  }
  if (!s.fill.empty()) {
    e.set("fill_color", encode_text(s.fill));
  }
  if (!s.color.empty()) {
    e.set("text_color", encode_text(s.color));
  }
}

// ── The reader ──────────────────────────────────────────────────────────────

source_text read_source(std::string_view text, context& ctx) {
  const std::vector<raw_line> lines = split_lines(text);
  const layout_of_text l = locate(lines);
  if (l.unclosed_front) {
    ctx.fail({lines.empty() ? 1 : 1, 1}, "the front matter opened with --- is not closed by another ---");
  }
  if (!l.found) {
    const int at = l.header < lines.size() ? lines[l.header].number : 1;
    ctx.fail({at, 1}, "not a mermaid diagram (no diagram keyword such as flowchart or sequenceDiagram)");
  }
  source_text src;
  // Front matter: the title; anything else is a warning.
  if (l.front) {
    for (size_t i = l.front_begin; i < l.front_end; ++i) {
      const std::string& t = lines[i].text;
      if (t.empty() || t[0] == ' ' || t[0] == '\t' || trim(t).empty() || trim(t)[0] == '#') {
        continue;
      }
      const size_t colon = t.find(':');
      const std::string key(trim(std::string_view(t).substr(0, colon)));
      const line ln{t, lines[i].number};
      if (key == "title" && colon != std::string::npos) {
        src.title = label_text(unquote(std::string_view(t).substr(colon + 1)));
        src.title_pos = ln.start();
      } else {
        ctx.warn(ln.start(), "front matter '" + key + "' is ignored (nymph's own fields set the look)");
      }
    }
  }
  if (l.fenced && l.fence_end < lines.size()) {
    for (size_t i = l.fence_end + 1; i < lines.size(); ++i) {
      if (!is_blank(lines[i].text)) {
        ctx.warn({lines[i].number, 1}, "text after the closing fence is ignored");
        break;
      }
    }
  }
  // The header.
  {
    const line h{lines[l.header].text, lines[l.header].number};
    const size_t first = h.text.find_first_not_of(" \t");
    const std::string_view word = first_word(h.text);
    src.kind = std::string(word);
    src.header_pos = h.at(first);
    size_t rest = first + word.size();
    if (rest < h.text.size() && h.text[rest] == ';') {
      ++rest;
    }
    const std::string_view after = std::string_view(h.text).substr(std::min(rest, h.text.size()));
    src.header_rest = std::string(trim(after));
    const size_t rest_first = h.text.find_first_not_of(" \t;", rest);
    src.header_rest_pos = h.at(rest_first == std::string::npos ? h.text.size() : rest_first);
    if (std::find(kUnsupported.begin(), kUnsupported.end(), word) != kUnsupported.end()) {
      std::string list;
      for (std::string_view k : {"flowchart", "graph", "sequenceDiagram", "classDiagram", "stateDiagram", "erDiagram",
                                 "gantt", "pie", "mindmap", "sankey-beta", "xychart-beta"}) {
        list += (list.empty() ? "" : ", ") + std::string(k);
      }
      ctx.fail(src.header_pos, "mermaid " + src.kind + " diagrams are not supported (nymph reads: " + list + ")");
    }
  }
  // Directive warnings, before and after the header.
  bool warned_directive = false;
  for (size_t i = l.first; i < l.end; ++i) {
    if (starts_with(trim(lines[i].text), "%%{") && !warned_directive) {
      ctx.warn(line{lines[i].text, lines[i].number}.start(),
               "%%{init}%% directives are ignored (nymph's own fields set the look)");
      warned_directive = true;
    }
  }
  // The body.
  bool in_directive = false;
  for (size_t i = l.header + 1; i < l.end; ++i) {
    const std::string& t = lines[i].text;
    const std::string_view tt = trim(t);
    if (in_directive) {
      in_directive = tt.find("}%%") == std::string_view::npos;
      continue;
    }
    if (tt.empty()) {
      continue;
    }
    if (starts_with(tt, "%%{")) {
      in_directive = tt.find("}%%") == std::string_view::npos;
      continue;
    }
    if (starts_with(tt, "%%")) {
      continue;
    }
    if (starts_with(tt, "accTitle") && trim(tt.substr(8)).substr(0, 1) == ":") {
      src.acc_title = std::string(trim(trim(tt.substr(8)).substr(1)));
      continue;
    }
    if (starts_with(tt, "accDescr")) {
      const std::string_view rest = trim(tt.substr(8));
      if (starts_with(rest, ":")) {
        src.acc_descr = std::string(trim(rest.substr(1)));
        continue;
      }
      if (starts_with(rest, "{")) {
        std::string descr(trim(rest.substr(1)));
        size_t j = i;
        bool closed = descr.find('}') != std::string::npos;
        while (!closed && j + 1 < l.end) {
          ++j;
          const std::string_view part = trim(lines[j].text);
          closed = part.find('}') != std::string_view::npos;
          descr += (descr.empty() ? "" : " ") + std::string(part);
        }
        descr = descr.substr(0, descr.find('}'));
        src.acc_descr = std::string(trim(descr));
        i = j;
        continue;
      }
    }
    src.body.push_back({t, lines[i].number});
  }
  return src;
}

} // namespace bdg::nymph::mermaid

namespace bdg::nymph {

bool is_mermaid(std::string_view text) noexcept {
  try {
    const std::vector<mermaid::raw_line> lines = mermaid::split_lines(text.substr(0, std::min<size_t>(text.size(), 1u << 20)));
    return mermaid::locate(lines).found;
  } catch (...) {
    return false;
  }
}

} // namespace bdg::nymph
