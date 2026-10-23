// MIT License © 2026 Binary Dice Games
/// @file flowchart.cpp
/// @brief mermaid `flowchart` / `graph` → `Flowchart`
///        (spec/16_mermaid_import/DESIGN.md section 4.1).
#include <algorithm>
#include <map>
#include "core/limits.hpp"
#include "mermaid/mermaid_internal.hpp"

namespace bdg::nymph::mermaid {

namespace {

// ── Shapes ──────────────────────────────────────────────────────────────────

/// A nymph node look: a `shape` name, or a library `symbol`.
struct look {
  std::string shape; ///< FlowNode shape, or empty
  std::string symbol; ///< library symbol, or empty
};

/// mermaid 11 `@{shape: ...}` names and aliases → nymph.
const std::vector<std::pair<std::string, look>>& shape_names() {
  static const std::vector<std::pair<std::string, look>> names = {
      {"rect", {"Box", ""}},
      {"rectangle", {"Box", ""}},
      {"process", {"Box", ""}},
      {"proc", {"Box", ""}},
      {"text", {"Box", ""}},
      {"rounded", {"Rounded", ""}},
      {"event", {"Rounded", ""}},
      {"stadium", {"Stadium", ""}},
      {"pill", {"Stadium", ""}},
      {"terminal", {"Stadium", ""}},
      {"subproc", {"Subroutine", ""}},
      {"subprocess", {"Subroutine", ""}},
      {"subroutine", {"Subroutine", ""}},
      {"fr-rect", {"Subroutine", ""}},
      {"framed-rectangle", {"Subroutine", ""}},
      {"cyl", {"Cylinder", ""}},
      {"cylinder", {"Cylinder", ""}},
      {"database", {"Cylinder", ""}},
      {"db", {"Cylinder", ""}},
      {"h-cyl", {"Cylinder", ""}},
      {"das", {"Cylinder", ""}},
      {"horizontal-cylinder", {"Cylinder", ""}},
      {"lin-cyl", {"Cylinder", ""}},
      {"disk", {"Cylinder", ""}},
      {"lined-cylinder", {"Cylinder", ""}},
      {"circle", {"Circle", ""}},
      {"circ", {"Circle", ""}},
      {"dbl-circ", {"Circle", ""}},
      {"double-circle", {"Circle", ""}},
      {"sm-circ", {"Circle", ""}},
      {"small-circle", {"Circle", ""}},
      {"start", {"Circle", ""}},
      {"fr-circ", {"Circle", ""}},
      {"framed-circle", {"Circle", ""}},
      {"stop", {"Circle", ""}},
      {"f-circ", {"Circle", ""}},
      {"filled-circle", {"Circle", ""}},
      {"junction", {"Circle", ""}},
      {"diam", {"Diamond", ""}},
      {"diamond", {"Diamond", ""}},
      {"decision", {"Diamond", ""}},
      {"question", {"Diamond", ""}},
      {"hex", {"Hexagon", ""}},
      {"hexagon", {"Hexagon", ""}},
      {"prepare", {"Hexagon", ""}},
      {"lean-r", {"Parallelogram", ""}},
      {"lean-right", {"Parallelogram", ""}},
      {"in-out", {"Parallelogram", ""}},
      {"lean-l", {"Parallelogram", ""}},
      {"lean-left", {"Parallelogram", ""}},
      {"out-in", {"Parallelogram", ""}},
      {"trap-b", {"", "trapezoid"}},
      {"trapezoid", {"", "trapezoid"}},
      {"priority", {"", "trapezoid"}},
      {"trapezoid-bottom", {"", "trapezoid"}},
      {"trap-t", {"", "trapezoid"}},
      {"inv-trapezoid", {"", "trapezoid"}},
      {"manual", {"", "trapezoid"}},
      {"trapezoid-top", {"", "trapezoid"}},
      {"doc", {"", "document"}},
      {"document", {"", "document"}},
      {"lin-doc", {"", "document"}},
      {"lined-document", {"", "document"}},
      {"tag-doc", {"", "document"}},
      {"tagged-document", {"", "document"}},
      {"docs", {"", "multi_document"}},
      {"documents", {"", "multi_document"}},
      {"st-doc", {"", "multi_document"}},
      {"stacked-document", {"", "multi_document"}},
      {"delay", {"", "delay"}},
      {"half-rounded-rectangle", {"", "delay"}},
      {"manual-input", {"", "manual_input"}},
      {"sl-rect", {"", "manual_input"}},
      {"sloped-rectangle", {"", "manual_input"}},
      {"cloud", {"", "cloud"}},
      {"notch-rect", {"", "card"}},
      {"card", {"", "card"}},
      {"notched-rectangle", {"", "card"}},
      {"tri", {"", "triangle"}},
      {"triangle", {"", "triangle"}},
      {"extract", {"", "extract"}},
      {"flip-tri", {"", "merge"}},
      {"flipped-triangle", {"", "merge"}},
      {"manual-file", {"", "merge"}},
      {"fork", {"", "fork_bar"}},
      {"join", {"", "fork_bar"}},
      {"hourglass", {"", "collate"}},
      {"collate", {"", "collate"}},
      {"cross-circ", {"", "summing_junction"}},
      {"summary", {"", "summing_junction"}},
      {"crossed-circle", {"", "summing_junction"}},
      {"brace", {"", "brace_left"}},
      {"brace-l", {"", "brace_left"}},
      {"brace-r", {"", "brace_left"}},
      {"braces", {"", "brace_left"}},
      {"comment", {"", "brace_left"}},
      {"win-pane", {"", "internal_storage"}},
      {"internal-storage", {"", "internal_storage"}},
      {"window-pane", {"", "internal_storage"}},
      {"lin-rect", {"", "process"}},
      {"lined-process", {"", "process"}},
      {"lined-rectangle", {"", "process"}},
      {"shaded-process", {"", "process"}},
      {"div-rect", {"", "process"}},
      {"divided-process", {"", "process"}},
      {"divided-rectangle", {"", "process"}},
      {"tag-rect", {"", "process"}},
      {"tagged-process", {"", "process"}},
      {"tagged-rectangle", {"", "process"}},
      {"curv-trap", {"", "display"}},
      {"display", {"", "display"}},
      {"curved-trapezoid", {"", "display"}},
      {"bow-rect", {"", "stored_data"}},
      {"stored-data", {"", "stored_data"}},
      {"bow-tie-rectangle", {"", "stored_data"}},
      {"odd", {"", "banner"}},
      {"flag", {"", "banner"}},
      {"paper-tape", {"", "banner"}},
      {"notch-pent", {"", "pentagon"}},
      {"loop-limit", {"", "pentagon"}},
      {"notched-pentagon", {"", "pentagon"}},
      {"bolt", {"Box", ""}},
      {"com-link", {"Box", ""}},
      {"lightning-bolt", {"Box", ""}},
  };
  return names;
}

// ── The model ───────────────────────────────────────────────────────────────

struct node_info {
  std::string id;
  std::string text;
  bool has_text = false;
  look shape{"Box", ""};
  std::vector<std::string> classes;
  style own; ///< `style id ...`
  int owner = -1; ///< group index, -1 for the diagram
  int seq = 0;
  pos at;
};

struct group_info {
  std::string id;
  std::string title;
  int parent = -1;
  int seq = 0;
  pos at;
  std::vector<std::string> mentions; ///< node ids named directly inside
  std::vector<std::string> classes;
  style own;
};

struct link_info {
  std::string from, to, label;
  std::string line = "Solid";
  std::string arrow = "End";
  int length = 1;
  bool invisible = false;
  style own;
  pos at;
};

struct flow {
  context& ctx;
  std::vector<node_info> nodes;
  std::map<std::string, size_t> node_index;
  std::vector<group_info> groups;
  std::vector<int> open; ///< stack of open groups
  std::vector<link_info> links;
  std::map<std::string, style> class_defs;
  std::vector<std::pair<std::string, std::pair<std::string, pos>>> late_classes; ///< id, class, where
  std::vector<std::pair<std::string, std::pair<style, pos>>> late_styles;       ///< id, style, where
  int seq = 0;
  std::string direction = "TopDown";

  explicit flow(context& c) : ctx(c) {}

  node_info& mention(const std::string& id, pos at) {
    auto it = node_index.find(id);
    if (it == node_index.end()) {
      node_info n;
      n.id = id;
      n.seq = seq++;
      n.at = at;
      node_index[id] = nodes.size();
      nodes.push_back(n);
      check_limit(nodes.size(), kMaxUmlNodes, "nodes", at, ctx);
      it = node_index.find(id);
    }
    if (!open.empty()) {
      groups[static_cast<size_t>(open.back())].mentions.push_back(id);
    }
    return nodes[it->second];
  }
};

// ── Statement scanning ──────────────────────────────────────────────────────

/// One statement: a slice [begin, end) of a line.
struct statement {
  const line* ln;
  size_t begin;
  size_t end;
};

/// Splits a line on `;` outside quotes, brackets and |labels|.
std::vector<statement> statements_of(const line& l) {
  std::vector<statement> out;
  int depth = 0;
  bool quoted = false;
  bool bar = false;
  size_t begin = 0;
  const std::string& t = l.text;
  for (size_t i = 0; i < t.size(); ++i) {
    const char c = t[i];
    if (c == '"') {
      quoted = !quoted;
    } else if (!quoted) {
      if (c == '[' || c == '(' || c == '{') {
        ++depth;
      } else if ((c == ']' || c == ')' || c == '}') && depth > 0) {
        --depth;
      } else if (c == '|' && depth == 0) {
        bar = !bar;
      } else if (c == ';' && depth == 0 && !bar) {
        out.push_back({&l, begin, i});
        begin = i + 1;
      }
    }
  }
  out.push_back({&l, begin, t.size()});
  std::vector<statement> kept;
  for (const statement& s : out) {
    if (!trim(std::string_view(t).substr(s.begin, s.end - s.begin)).empty()) {
      kept.push_back(s);
    }
  }
  return kept;
}

/// A cursor over one statement.
struct cursor {
  const line& ln;
  size_t i;
  size_t end;
  context& ctx;

  const std::string& t() const {
    return ln.text;
  }
  bool done() {
    skip();
    return i >= end;
  }
  void skip() {
    while (i < end && (t()[i] == ' ' || t()[i] == '\t')) {
      ++i;
    }
  }
  char peek(size_t ahead = 0) const {
    return i + ahead < end ? t()[i + ahead] : '\0';
  }
  bool at(std::string_view s) const {
    return i + s.size() <= end && t().compare(i, s.size(), s) == 0;
  }
  pos here() const {
    return ln.at(i);
  }
  std::string_view rest() const {
    return std::string_view(t()).substr(i, end - i);
  }
  [[noreturn]] void fail(const std::string& message) const {
    ctx.fail(here(), message);
  }
};

bool is_link_char(char c) {
  return c == '-' || c == '.' || c == '=' || c == '~';
}

/// An id: id characters; `-` and `.` only when they do not start a link.
std::string read_id(cursor& c) {
  const size_t start = c.i;
  while (c.i < c.end) {
    const char ch = c.peek();
    if (ch == '-' || ch == '.') {
      const char next = c.peek(1);
      if (next == '-' || next == '.' || next == '=' || next == '>' || next == '\0' || next == ' ' || next == '\t') {
        break;
      }
      if (ch == '-' && (next == 'x' || next == 'o') && (c.peek(2) == ' ' || c.peek(2) == '\0')) {
        break;
      }
      ++c.i;
      continue;
    }
    if (!is_id_char(ch) || ch == '=' ) {
      break;
    }
    ++c.i;
  }
  return c.t().substr(start, c.i - start);
}

/// Text up to `close` (quoted text may hold anything).
std::string read_until(cursor& c, std::string_view close, const std::string& what) {
  c.skip();
  if (c.peek() == '"') {
    const size_t q = c.t().find('"', c.i + 1);
    if (q == std::string::npos || q >= c.end) {
      c.fail("the quote of " + what + " is not closed");
    }
    std::string text = c.t().substr(c.i, q + 1 - c.i);
    c.i = q + 1;
    c.skip();
    if (!c.at(close)) {
      c.fail("expected '" + std::string(close) + "' to close " + what);
    }
    c.i += close.size();
    return label_text(text);
  }
  const size_t found = c.t().find(close, c.i);
  if (found == std::string::npos || found + close.size() > c.end) {
    c.fail("expected '" + std::string(close) + "' to close " + what);
  }
  std::string text = c.t().substr(c.i, found - c.i);
  c.i = found + close.size();
  return label_text(text);
}

/// `@{ shape: x, label: "y" }` (the cursor is on '@').
void read_attributes(cursor& c, node_info& n, flow& f) {
  const pos at = c.here();
  c.i += 2; // "@{"
  const size_t close = c.t().find('}', c.i);
  if (close == std::string::npos || close >= c.end) {
    c.fail("expected '}' to close @{");
  }
  const std::string body = c.t().substr(c.i, close - c.i);
  c.i = close + 1;
  for (const std::string& part : split_outside_quotes(body, ',')) {
    if (part.empty()) {
      continue;
    }
    const size_t colon = part.find(':');
    const std::string key(trim(std::string_view(part).substr(0, colon)));
    const std::string value = colon == std::string::npos ? "" : std::string(trim(std::string_view(part).substr(colon + 1)));
    if (key == "shape") {
      std::string name = value;
      if (name.size() >= 2 && (name.front() == '"' || name.front() == '\'')) {
        name = name.substr(1, name.size() - 2);
      }
      const auto& names = shape_names();
      const auto it = std::find_if(names.begin(), names.end(), [&](const auto& p) { return p.first == name; });
      if (it == names.end()) {
        std::vector<std::string> all;
        for (const auto& p : names) {
          all.push_back(p.first);
        }
        f.ctx.fail(at, "unknown mermaid shape '" + name + "'" + suggestion(name, all));
      }
      if (name == "bolt" || name == "com-link" || name == "lightning-bolt") {
        f.ctx.warn(at, "shape '" + name + "' has no nymph equivalent; drawn as a box");
      }
      n.shape = it->second;
    } else if (key == "label") {
      n.text = label_text(value);
      n.has_text = true;
    } else {
      f.ctx.warn(at, "node attribute '" + key + "' is ignored");
    }
  }
}

/// A node reference with its optional shape, @{} and :::class.
std::string read_node(cursor& c, flow& f) {
  c.skip();
  const pos at = c.here();
  const std::string id = read_id(c);
  if (id.empty()) {
    c.fail(std::string("expected a node id, found '") + (c.i < c.end ? std::string(1, c.peek()) : "end of line") + "'");
  }
  node_info& n = f.mention(id, at);
  struct opener {
    std::string_view open, close;
    look shape;
  };
  static const opener openers[] = {
      {"(((", ")))", {"Circle", ""}},      {"([", "])", {"Stadium", ""}},       {"[[", "]]", {"Subroutine", ""}},
      {"[(", ")]", {"Cylinder", ""}},      {"((", "))", {"Circle", ""}},        {"{{", "}}", {"Hexagon", ""}},
      {"[/", "", {"Parallelogram", ""}},   {"[\\", "", {"Parallelogram", ""}}, {">", "]", {"", "banner"}},
      {"[", "]", {"Box", ""}},             {"(", ")", {"Rounded", ""}},         {"{", "}", {"Diamond", ""}},
  };
  if (c.at("@{")) {
    read_attributes(c, n, f);
  } else {
    for (const opener& o : openers) {
      if (!c.at(o.open)) {
        continue;
      }
      c.i += o.open.size();
      if (o.open == "[/" || o.open == "[\\") {
        // [/t/] and [\t\] are parallelograms; [/t\] and [\t/] trapezoids.
        const size_t slash = c.t().find("/]", c.i);
        const size_t back = c.t().find("\\]", c.i);
        const bool quoted = trim(c.rest()).substr(0, 1) == "\"";
        size_t close = std::min(slash, back);
        if (quoted) {
          const size_t q = c.t().find('"', c.t().find('"', c.i) + 1);
          close = std::min(c.t().find("/]", q), c.t().find("\\]", q));
        }
        if (close == std::string::npos || close >= c.end) {
          c.fail("expected /] or \\] to close the node's text");
        }
        const char last = c.t()[close];
        const char first = o.open[1];
        n.shape = first == last ? look{"Parallelogram", ""} : look{"", "trapezoid"};
        n.text = label_text(c.t().substr(c.i, close - c.i));
        c.i = close + 2;
      } else {
        n.shape = o.shape;
        n.text = read_until(c, o.close, "the node's text");
      }
      n.has_text = true;
      break;
    }
  }
  if (c.at(":::")) {
    c.i += 3;
    const std::string cls = read_id(c);
    n.classes.push_back(cls);
  }
  return id;
}

/// A link token and its label; nullopt if the cursor is not on a link.
std::optional<link_info> read_link(cursor& c) {
  c.skip();
  const size_t save = c.i;
  const pos at = c.here();
  // A link id: `e1@-->`.
  {
    const size_t start = c.i;
    while (c.i < c.end && is_id_char(c.peek()) && c.peek() != '-' && c.peek() != '.') {
      ++c.i;
    }
    if (c.i > start && c.peek() == '@' && is_link_char(c.peek(1))) {
      c.ctx.warn(at, "link id '" + c.t().substr(start, c.i - start) + "' is ignored");
      ++c.i;
    } else {
      c.i = start;
    }
  }
  link_info l;
  l.at = at;
  bool start_arrow = false;
  if (c.peek() == '<') {
    start_arrow = true;
    ++c.i;
  } else if ((c.peek() == 'o' || c.peek() == 'x') && (c.peek(1) == '-' || c.peek(1) == '=' || c.peek(1) == '.')) {
    start_arrow = true; // o--o, x--x
    ++c.i;
  }
  const size_t body_start = c.i;
  while (c.i < c.end && is_link_char(c.peek())) {
    ++c.i;
  }
  std::string body = c.t().substr(body_start, c.i - body_start);
  if (body.size() < 2 && !(body == "-" && c.peek() == '>')) {
    c.i = save;
    return std::nullopt;
  }
  if (body.find('~') != std::string::npos) {
    l.invisible = true;
    return l;
  }
  // A text link: `-- text -->`, `-. text .->`, `== text ==>`.
  if ((body == "--" || body == "-." || body == "==") && (c.peek() == ' ' || c.peek() == '\t')) {
    // Find the closing run, after a space.
    size_t p = c.i;
    size_t close = std::string::npos;
    for (; p < c.end; ++p) {
      const char ch = c.t()[p];
      if ((ch == '-' || ch == '.' || ch == '=') && (c.t()[p - 1] == ' ' || c.t()[p - 1] == '\t')) {
        size_t q = p;
        while (q < c.end && is_link_char(c.t()[q])) {
          ++q;
        }
        if (q - p >= 2 || (q < c.end && c.t()[q] == '>' && q - p >= 1)) {
          close = p;
          break;
        }
      }
    }
    if (close != std::string::npos) {
      l.label = label_text(c.t().substr(c.i, close - c.i));
      c.i = close;
      const size_t run = c.i;
      while (c.i < c.end && is_link_char(c.peek())) {
        ++c.i;
      }
      body += c.t().substr(run, c.i - run);
    }
  }
  char head = '\0';
  if (c.peek() == '>') {
    head = '>';
    ++c.i;
  } else if ((c.peek() == 'o' || c.peek() == 'x') && !is_id_char(c.peek(1))) {
    head = c.peek();
    ++c.i;
  }
  // Style and length.
  const bool thick = body.find('=') != std::string::npos;
  const bool dotted = !thick && body.find('.') != std::string::npos;
  l.line = thick ? "Thick" : dotted ? "Dotted" : "Solid";
  int length = 1;
  if (dotted) {
    // A text link (`-. text .->`) counts its closing run only.
    length = static_cast<int>(std::count(body.begin(), body.end(), '.')) - (l.label.empty() ? 0 : 1);
  } else {
    const char ch = thick ? '=' : '-';
    const int n = static_cast<int>(std::count(body.begin(), body.end(), ch));
    // A text link counts its closing run only.
    const int counted = l.label.empty() ? n : n - 2;
    length = head != '\0' ? counted - 1 : counted - 2;
  }
  l.length = std::max(1, length);
  if (head != '\0' && start_arrow) {
    l.arrow = "Both";
  } else if (head != '\0') {
    l.arrow = "End";
  } else if (start_arrow) {
    l.arrow = "End"; // `<--`: swapped by the caller
    l.from = "<";
  } else {
    l.arrow = "None";
  }
  // `|label|`
  c.skip();
  if (c.peek() == '|') {
    const size_t close = c.t().find('|', c.i + 1);
    if (close == std::string::npos || close >= c.end) {
      c.fail("the link's |text| is not closed");
    }
    l.label = label_text(c.t().substr(c.i + 1, close - c.i - 1));
    c.i = close + 1;
  }
  return l;
}

/// `A & B --> C & D --> E`
void read_chain(cursor& c, flow& f) {
  std::vector<std::string> left;
  left.push_back(read_node(c, f));
  while (true) {
    c.skip();
    if (c.peek() == '&') {
      ++c.i;
      left.push_back(read_node(c, f));
      continue;
    }
    break;
  }
  while (!c.done()) {
    if (c.at(":::")) {
      c.fail("':::' must follow a node id directly");
    }
    std::optional<link_info> l = read_link(c);
    if (!l) {
      c.fail("expected a link such as --> or the end of the statement, found '" + std::string(c.rest()) + "'");
    }
    std::vector<std::string> right;
    right.push_back(read_node(c, f));
    while (true) {
      c.skip();
      if (c.peek() == '&') {
        ++c.i;
        right.push_back(read_node(c, f));
        continue;
      }
      break;
    }
    const bool swap = l->from == "<";
    for (const std::string& a : left) {
      for (const std::string& b : right) {
        link_info copy = *l;
        copy.from = swap ? b : a;
        copy.to = swap ? a : b;
        f.links.push_back(copy);
        check_limit(f.links.size(), kMaxUmlEdges, "links", copy.at, f.ctx);
      }
    }
    left = right;
  }
}

std::string direction_name(std::string_view d, pos at, context& ctx) {
  if (d == "TB" || d == "TD") {
    return "TopDown";
  }
  if (d == "BT") {
    return "BottomUp";
  }
  if (d == "LR") {
    return "LeftRight";
  }
  if (d == "RL") {
    return "RightLeft";
  }
  ctx.fail(at, "unknown direction '" + std::string(d) + "' (directions: TB, TD, BT, LR, RL)");
}

/// The ids of a comma-separated list.
std::vector<std::string> id_list(std::string_view text) {
  std::vector<std::string> ids;
  for (const std::string& part : split_outside_quotes(text, ',')) {
    if (!part.empty()) {
      ids.push_back(part);
    }
  }
  return ids;
}

/// Splits `word rest` at the first space.
std::pair<std::string_view, std::string_view> word_and_rest(std::string_view text) {
  text = trim(text);
  const size_t space = text.find_first_of(" \t");
  if (space == std::string_view::npos) {
    return {text, {}};
  }
  return {text.substr(0, space), trim(text.substr(space))};
}

void read_statement(const statement& s, flow& f) {
  cursor c{*s.ln, s.begin, s.end, f.ctx};
  c.skip();
  const pos at = c.here();
  const std::string_view text = trim(c.rest());
  const auto [word, rest] = word_and_rest(text);
  if (word == "subgraph") {
    if (f.open.size() >= kMaxUmlNesting) {
      f.ctx.fail(at, "subgraphs nest more than " + std::to_string(kMaxUmlNesting) + " deep");
    }
    group_info g;
    g.at = at;
    g.seq = f.seq++;
    g.parent = f.open.empty() ? -1 : f.open.back();
    std::string_view r = rest;
    if (r.empty()) {
      f.ctx.fail(at, "subgraph needs an id or a title");
    }
    const size_t bracket = r.find('[');
    if (r.front() == '"') {
      g.id = label_text(r);
    } else if (bracket != std::string_view::npos && r.back() == ']') {
      g.id = std::string(trim(r.substr(0, bracket)));
      g.title = label_text(r.substr(bracket + 1, r.size() - bracket - 2));
    } else {
      g.id = label_text(r);
    }
    for (const group_info& other : f.groups) {
      if (other.id == g.id) {
        f.ctx.fail(at, "subgraph '" + g.id + "' is declared twice");
      }
    }
    f.groups.push_back(g);
    f.open.push_back(static_cast<int>(f.groups.size() - 1));
    return;
  }
  if (word == "end" && rest.empty()) {
    if (f.open.empty()) {
      f.ctx.fail(at, "'end' without a subgraph");
    }
    // Close: claim the nodes named directly in it (an inner subgraph keeps its nodes).
    group_info& g = f.groups[static_cast<size_t>(f.open.back())];
    for (const std::string& id : g.mentions) {
      node_info& n = f.nodes[f.node_index.at(id)];
      if (n.owner == -1) {
        n.owner = f.open.back();
      }
    }
    f.open.pop_back();
    return;
  }
  if (word == "direction") {
    if (f.open.empty()) {
      f.direction = direction_name(rest, at, f.ctx);
    } else {
      f.ctx.warn(at, "direction inside a subgraph is ignored (nymph lays out a diagram in one direction)");
    }
    return;
  }
  if (word == "classDef") {
    const auto [names, styles] = word_and_rest(rest);
    const style st = parse_style(styles, at, f.ctx);
    for (const std::string& name : id_list(names)) {
      merge_style(f.class_defs[name], st);
    }
    return;
  }
  if (word == "class") {
    const size_t space = rest.find_last_of(" \t");
    if (space == std::string_view::npos) {
      f.ctx.fail(at, "class needs node ids and a class name: class a,b name");
    }
    const std::vector<std::string> ids = id_list(rest.substr(0, space));
    const std::vector<std::string> classes = id_list(rest.substr(space + 1));
    for (const std::string& id : ids) {
      for (const std::string& cls : classes) {
        f.late_classes.push_back({id, {cls, at}});
      }
    }
    return;
  }
  if (word == "style") {
    const auto [id, styles] = word_and_rest(rest);
    f.late_styles.push_back({std::string(id), {parse_style(styles, at, f.ctx), at}});
    return;
  }
  if (word == "linkStyle") {
    const auto [which, styles] = word_and_rest(rest);
    style st = parse_style(styles, at, f.ctx);
    st.fill.clear();
    if (which == "default") {
      f.late_styles.push_back({std::string(1, '\x01') + "default", {st, at}});
      return;
    }
    for (const std::string& index : id_list(which)) {
      f.late_styles.push_back({std::string(1, '\x01') + index, {st, at}});
    }
    return;
  }
  if (word == "click" || word == "callback" || word == "href") {
    f.ctx.warn(at, "click is ignored (a nymph figure has no links or callbacks)");
    return;
  }
  if (starts_with(text, "title ")) {
    return; // handled by the reader's front matter title; a stray title line
  }
  // An `id@{ animate: true }` on an edge id is a separate statement.
  if (text.find("@{") != std::string_view::npos && text.find("animat") != std::string_view::npos) {
    f.ctx.warn(at, "link animation is ignored");
    return;
  }
  read_chain(c, f);
}

} // namespace

document convert_flowchart(const source_text& src, context& ctx) {
  flow f(ctx);
  if (!src.header_rest.empty()) {
    f.direction = direction_name(word_and_rest(src.header_rest).first, src.header_rest_pos, ctx);
  }
  for (const line& l : src.body) {
    for (const statement& s : statements_of(l)) {
      read_statement(s, f);
    }
  }
  if (!f.open.empty()) {
    const group_info& g = f.groups[static_cast<size_t>(f.open.back())];
    ctx.fail(g.at, "subgraph '" + g.id + "' is not closed by 'end'");
  }
  // Classes and styles named after the nodes they apply to.
  std::map<std::string, size_t> group_index;
  for (size_t i = 0; i < f.groups.size(); ++i) {
    group_index[f.groups[i].id] = i;
  }
  for (const auto& [id, cls] : f.late_classes) {
    if (auto it = f.node_index.find(id); it != f.node_index.end()) {
      f.nodes[it->second].classes.push_back(cls.first);
    } else if (auto g = group_index.find(id); g != group_index.end()) {
      f.groups[g->second].classes.push_back(cls.first);
    } else {
      ctx.warn(cls.second, "class names unknown node '" + id + "'; ignored");
    }
  }
  // `linkStyle default` first, so a link's own linkStyle wins.
  for (const auto& [id, st] : f.late_styles) {
    if (id == std::string(1, '\x01') + "default") {
      for (link_info& l : f.links) {
        merge_style(l.own, st.first);
      }
    }
  }
  for (const auto& [id, st] : f.late_styles) {
    if (!id.empty() && id[0] == '\x01') {
      const std::string which = id.substr(1);
      if (which == "default") {
        continue;
      }
      const std::optional<double> index = parse_number(which);
      if (!index || *index < 0 || static_cast<size_t>(*index) >= f.links.size()) {
        ctx.fail(st.second, "linkStyle " + which + ": there are " + std::to_string(f.links.size()) + " links (counted from 0)");
      }
      merge_style(f.links[static_cast<size_t>(*index)].own, st.first);
    } else if (auto it = f.node_index.find(id); it != f.node_index.end()) {
      merge_style(f.nodes[it->second].own, st.first);
    } else if (auto g = group_index.find(id); g != group_index.end()) {
      merge_style(f.groups[g->second].own, st.first);
    } else {
      ctx.warn(st.second, "style names unknown node '" + id + "'; ignored");
    }
  }
  auto class_style = [&](const std::vector<std::string>& classes, const style& own, pos at) {
    style s;
    if (auto d = f.class_defs.find("default"); d != f.class_defs.end()) {
      merge_style(s, d->second);
    }
    for (const std::string& cls : classes) {
      auto it = f.class_defs.find(cls);
      if (it == f.class_defs.end()) {
        ctx.warn(at, "class '" + cls + "' has no classDef; ignored");
        continue;
      }
      merge_style(s, it->second);
    }
    merge_style(s, own);
    return s;
  };

  // Elements: nodes and groups in order of appearance, in their containers.
  struct item {
    int seq;
    bool is_group;
    size_t index;
  };
  std::vector<std::vector<item>> members(f.groups.size() + 1); // [0] is the diagram
  for (size_t i = 0; i < f.nodes.size(); ++i) {
    if (group_index.count(f.nodes[i].id) != 0) {
      continue; // a link to a subgraph, not a node
    }
    members[static_cast<size_t>(f.nodes[i].owner + 1)].push_back({f.nodes[i].seq, false, i});
  }
  for (size_t i = 0; i < f.groups.size(); ++i) {
    members[static_cast<size_t>(f.groups[i].parent + 1)].push_back({f.groups[i].seq, true, i});
  }
  for (auto& m : members) {
    std::stable_sort(m.begin(), m.end(), [](const item& a, const item& b) { return a.seq < b.seq; });
  }
  std::vector<element> group_elements(f.groups.size());
  auto node_element = [&](const node_info& n) {
    element e("FlowNode", n.at);
    e.text("id", n.id);
    if (n.has_text && n.text != n.id) {
      e.text("text", n.text);
    }
    if (!n.shape.symbol.empty()) {
      e.name("symbol", n.shape.symbol);
    } else if (n.shape.shape != "Box") {
      e.name("shape", n.shape.shape);
    }
    apply_style(e, class_style(n.classes, n.own, n.at));
    return e;
  };
  // Build groups innermost first (a child group has a higher index than its parent).
  for (size_t gi = f.groups.size(); gi-- > 0;) {
    const group_info& g = f.groups[gi];
    element e("FlowGroup", g.at);
    e.text("id", g.id);
    e.text("title", g.title);
    apply_style(e, class_style(g.classes, g.own, g.at));
    for (const item& it : members[gi + 1]) {
      e.children.push_back(it.is_group ? group_elements[it.index] : node_element(f.nodes[it.index]));
    }
    group_elements[gi] = e;
  }
  element root("Flowchart", src.header_pos);
  root.text("title", src.title);
  if (f.direction != "TopDown") {
    root.name("direction", f.direction);
  }
  for (const item& it : members[0]) {
    root.children.push_back(it.is_group ? group_elements[it.index] : node_element(f.nodes[it.index]));
  }
  for (const link_info& l : f.links) {
    if (l.invisible) {
      ctx.warn(l.at, "invisible link ~~~ is dropped (nymph has no invisible links)");
      continue;
    }
    element e("FlowLink", l.at);
    e.text("from", l.from).text("to", l.to).text("label", l.label);
    if (l.line != "Solid") {
      e.name("line", l.line);
    }
    if (l.arrow != "End") {
      e.name("arrow", l.arrow);
    }
    if (l.length > 1) {
      e.number("min_length", l.length);
    }
    apply_style(e, l.own);
    root.children.push_back(e);
  }
  if (root.children.empty()) {
    ctx.fail(src.header_pos, "the flowchart has no nodes");
  }
  document doc;
  doc.root = root;
  return doc;
}

} // namespace bdg::nymph::mermaid
