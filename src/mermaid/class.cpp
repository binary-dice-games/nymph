// MIT License © 2026 Binary Dice Games
/// @file class.cpp
/// @brief mermaid `classDiagram` → `ClassDiagram`
///        (spec/16_mermaid_import/DESIGN.md section 4.3).
#include <algorithm>
#include <cctype>
#include <map>
#include "core/limits.hpp"
#include "mermaid/mermaid_internal.hpp"

namespace bdg::nymph::mermaid {

namespace {

struct class_info {
  std::string id;
  std::string name;
  std::string generic;
  std::string kind = "Class";
  std::vector<std::string> attributes;
  std::vector<std::string> methods;
  pos at;
};

struct diagram {
  context& ctx;
  std::vector<class_info> classes;
  std::map<std::string, size_t> index;
  std::vector<element> relations;
  std::vector<element> notes;
  style_sheet styles;
  std::string direction = "TopDown";

  explicit diagram(context& c) : ctx(c) {}

  class_info& mention(std::string id, pos at) {
    // `Square~Shape~` names class Square with a type parameter.
    std::string generic;
    const size_t tilde = id.find('~');
    if (tilde != std::string::npos) {
      generic = id.substr(tilde + 1);
      if (!generic.empty() && generic.back() == '~') {
        generic.pop_back();
      }
      id = id.substr(0, tilde);
    }
    if (id.size() >= 2 && id.front() == '`' && id.back() == '`') {
      id = id.substr(1, id.size() - 2);
    }
    auto it = index.find(id);
    if (it == index.end()) {
      class_info c;
      c.id = id;
      c.at = at;
      index[id] = classes.size();
      classes.push_back(c);
      check_limit(classes.size(), kMaxUmlNodes, "classes", at, ctx);
      it = index.find(id);
    }
    class_info& c = classes[it->second];
    if (!generic.empty()) {
      c.generic = generic;
    }
    return c;
  }
};

/// `List~int~` → `List<int>` (mermaid's generic markup in members).
std::string member_text(std::string_view text) {
  std::string out;
  bool open = false;
  for (char c : trim(text)) {
    if (c == '~') {
      out += open ? '>' : '<';
      open = !open;
    } else {
      out += c;
    }
  }
  return out;
}

void add_member(class_info& c, std::string_view text) {
  const std::string m = member_text(text);
  if (m.empty()) {
    return;
  }
  if (m.find('(') != std::string::npos) {
    c.methods.push_back(m);
  } else {
    c.attributes.push_back(m);
  }
}

bool set_stereotype(class_info& c, std::string_view text, pos at, context& ctx) {
  std::string s(trim(text));
  if (!starts_with(s, "<<") || s.size() < 4 || s.substr(s.size() - 2) != ">>") {
    return false;
  }
  s = std::string(trim(std::string_view(s).substr(2, s.size() - 4)));
  std::string l = s;
  std::transform(l.begin(), l.end(), l.begin(), [](unsigned char ch) { return char(std::tolower(ch)); });
  if (l == "interface") {
    c.kind = "Interface";
  } else if (l == "abstract") {
    c.kind = "Abstract";
  } else if (l == "enumeration" || l == "enum") {
    c.kind = "Enum";
  } else {
    ctx.warn(at, "stereotype <<" + s + ">> is ignored (nymph shows interface, abstract and enumeration)");
  }
  return true;
}

/// `class A`, `class A~T~`, `class A["Label"]`, `class A:::cls`, `class A {`.
/// Returns true when a member block opens.
bool read_class(std::string_view rest, pos at, diagram& d, class_info*& current) {
  rest = trim(rest);
  bool block = false;
  if (!rest.empty() && rest.back() == '{') {
    block = true;
    rest = trim(rest.substr(0, rest.size() - 1));
  }
  std::string cls;
  const size_t colons = rest.find(":::");
  if (colons != std::string_view::npos) {
    cls = std::string(trim(rest.substr(colons + 3)));
    rest = trim(rest.substr(0, colons));
  }
  std::string label;
  const size_t bracket = rest.find('[');
  if (bracket != std::string_view::npos && rest.back() == ']') {
    label = label_text(rest.substr(bracket + 1, rest.size() - bracket - 2));
    rest = trim(rest.substr(0, bracket));
  }
  if (rest.empty()) {
    d.ctx.fail(at, "class needs a name");
  }
  class_info& c = d.mention(std::string(rest), at);
  if (!label.empty()) {
    c.name = label;
  }
  if (!cls.empty()) {
    d.styles.assign(c.id, cls, at);
  }
  current = &c;
  return block;
}

struct relation_kind {
  std::string kind;
  bool reversed = false; ///< from is the right-hand class
};

/// The relation operator in `text`: its start, length and meaning.
struct found_relation {
  size_t begin = 0, end = 0;
  relation_kind kind;
  bool lollipop = false;
};

std::optional<found_relation> find_relation(std::string_view text) {
  bool quoted = false;
  for (size_t i = 0; i + 1 < text.size(); ++i) {
    if (text[i] == '"') {
      quoted = !quoted;
    }
    if (quoted || !((text[i] == '-' && text[i + 1] == '-') || (text[i] == '.' && text[i + 1] == '.'))) {
      continue;
    }
    const bool dashed = text[i] == '.';
    found_relation f;
    f.begin = i;
    f.end = i + 2;
    std::string left;
    std::string right;
    // Left head.
    if (i >= 2 && text.substr(i - 2, 2) == "<|") {
      left = "<|";
    } else if (i >= 2 && text.substr(i - 2, 2) == "()") {
      left = "()";
    } else if (i >= 1 && (text[i - 1] == '*' || text[i - 1] == '<')) {
      left = std::string(1, text[i - 1]);
    } else if (i >= 1 && text[i - 1] == 'o' && (i < 2 || text[i - 2] == ' ' || text[i - 2] == '"')) {
      left = "o";
    }
    f.begin -= left.size();
    // Right head.
    const std::string_view after = text.substr(f.end);
    if (starts_with(after, "|>")) {
      right = "|>";
    } else if (starts_with(after, "()")) {
      right = "()";
    } else if (!after.empty() && (after[0] == '*' || after[0] == '>')) {
      right = std::string(1, after[0]);
    } else if (!after.empty() && after[0] == 'o' && (after.size() == 1 || after[1] == ' ' || after[1] == '"')) {
      right = "o";
    }
    f.end += right.size();
    f.lollipop = left == "()" || right == "()";
    // The right head wins when both ends have one (A <|--|> B: approximation).
    if (right == "|>") {
      f.kind = {dashed ? "Realization" : "Inheritance", false};
    } else if (right == "*") {
      f.kind = {"Composition", true};
    } else if (right == "o") {
      f.kind = {"Aggregation", true};
    } else if (right == ">") {
      f.kind = {dashed ? "Dependency" : "DirectedAssociation", false};
    } else if (left == "<|") {
      f.kind = {dashed ? "Realization" : "Inheritance", true};
    } else if (left == "*") {
      f.kind = {"Composition", false};
    } else if (left == "o") {
      f.kind = {"Aggregation", false};
    } else if (left == "<") {
      f.kind = {dashed ? "Dependency" : "DirectedAssociation", true};
    } else {
      f.kind = {dashed ? "Dependency" : "Association", false};
    }
    return f;
  }
  return std::nullopt;
}

/// `A "1" --> "*" B : label`
bool read_relation(std::string_view text, pos at, diagram& d) {
  // The label after the last ':' outside quotes.
  std::string label;
  {
    bool quoted = false;
    size_t colon = std::string_view::npos;
    for (size_t i = 0; i < text.size(); ++i) {
      if (text[i] == '"') {
        quoted = !quoted;
      } else if (text[i] == ':' && !quoted && colon == std::string_view::npos) {
        colon = i;
      }
    }
    if (colon != std::string_view::npos) {
      label = label_text(text.substr(colon + 1));
      text = text.substr(0, colon);
    }
  }
  const std::optional<found_relation> r = find_relation(text);
  if (!r) {
    return false;
  }
  auto side = [](std::string_view part, bool left, std::string& card) {
    part = trim(part);
    // A cardinality in quotes, next to the operator.
    if (left && !part.empty() && part.back() == '"') {
      const size_t open = part.rfind('"', part.size() - 2);
      if (open != std::string_view::npos) {
        card = std::string(part.substr(open + 1, part.size() - open - 2));
        part = trim(part.substr(0, open));
      }
    } else if (!left && !part.empty() && part.front() == '"') {
      const size_t close = part.find('"', 1);
      if (close != std::string_view::npos) {
        card = std::string(part.substr(1, close - 1));
        part = trim(part.substr(close + 1));
      }
    }
    return std::string(part);
  };
  std::string left_card;
  std::string right_card;
  const std::string a = side(text.substr(0, r->begin), true, left_card);
  const std::string b = side(text.substr(r->end), false, right_card);
  if (a.empty() || b.empty()) {
    d.ctx.fail(at, "a relation needs a class on each side: A <|-- B");
  }
  if (r->lollipop) {
    d.mention(a, at);
    d.mention(b, at);
    d.ctx.warn(at, "lollipop interfaces are dropped (draw them with a ComponentDiagram after converting)");
    return true;
  }
  // Declared in the order written, then ordered as nymph's from / to.
  const std::string a_id = d.mention(a, at).id;
  const std::string b_id = d.mention(b, at).id;
  const std::string& from_id = r->kind.reversed ? b_id : a_id;
  const std::string& to_id = r->kind.reversed ? a_id : b_id;
  element e("ClassRelation", at);
  e.text("from", from_id).text("to", to_id);
  if (r->kind.kind != "Association") {
    e.name("kind", r->kind.kind);
  }
  e.text("label", label);
  e.text("from_label", r->kind.reversed ? right_card : left_card);
  e.text("to_label", r->kind.reversed ? left_card : right_card);
  d.relations.push_back(e);
  return true;
}

std::pair<std::string_view, std::string_view> word_and_rest(std::string_view text) {
  text = trim(text);
  const size_t space = text.find_first_of(" \t");
  if (space == std::string_view::npos) {
    return {text, {}};
  }
  return {text.substr(0, space), trim(text.substr(space))};
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

} // namespace

document convert_class(const source_text& src, context& ctx) {
  diagram d(ctx);
  std::string title = src.title;
  class_info* block = nullptr; ///< the class whose { } block is open
  size_t block_index = 0;
  int namespaces = 0;
  for (const line& l : src.body) {
    const pos at = l.start();
    const std::string_view text = trim(l.text);
    if (block != nullptr) {
      class_info& c = d.classes[block_index];
      if (text == "}") {
        block = nullptr;
        continue;
      }
      std::string_view member = text;
      bool closes = false;
      if (!member.empty() && member.back() == '}') {
        member = trim(member.substr(0, member.size() - 1));
        closes = true;
      }
      if (!set_stereotype(c, member, at, ctx)) {
        add_member(c, member);
      }
      if (closes) {
        block = nullptr;
      }
      continue;
    }
    const auto [word, rest] = word_and_rest(text);
    if (word == "class") {
      class_info* c = nullptr;
      const bool opens = read_class(rest, at, d, c);
      if (opens) {
        block = c;
        block_index = d.index.at(c->id);
      }
      continue;
    }
    if (word == "namespace") {
      ctx.warn(at, "namespace is dropped (its classes are kept)");
      if (!rest.empty() && rest.back() == '{') {
        ++namespaces;
      }
      continue;
    }
    if (text == "}" && namespaces > 0) {
      --namespaces;
      continue;
    }
    if (word == "direction") {
      d.direction = direction_name(rest, at, ctx);
      continue;
    }
    if (word == "title") {
      title = label_text(rest);
      continue;
    }
    if (word == "note") {
      element n("Note", at);
      std::string_view r = rest;
      if (starts_with(r, "for ")) {
        r = trim(r.substr(4));
        const size_t space = r.find_first_of(" \t");
        const std::string id(r.substr(0, space));
        n.texts("attach", {d.mention(id, at).id});
        r = space == std::string_view::npos ? std::string_view() : trim(r.substr(space));
      }
      n.text("text", label_text(r));
      d.notes.push_back(n);
      continue;
    }
    if (word == "click" || word == "link" || word == "callback") {
      ctx.warn(at, std::string(word) + " is ignored (a nymph figure has no links or callbacks)");
      continue;
    }
    if (d.styles.read(word, rest, at, false, ctx)) {
      continue;
    }
    if (starts_with(text, "<<")) {
      // `<<interface>> Animal`
      const size_t close = text.find(">>");
      if (close == std::string_view::npos) {
        ctx.fail(at, "expected '>>' to close the stereotype");
      }
      class_info& c = d.mention(std::string(trim(text.substr(close + 2))), at);
      set_stereotype(c, text.substr(0, close + 2), at, ctx);
      continue;
    }
    if (read_relation(text, at, d)) {
      continue;
    }
    // `Animal : +int age`
    const size_t colon = text.find(':');
    if (colon != std::string_view::npos) {
      std::string id(trim(text.substr(0, colon)));
      class_info& c = d.mention(id, at);
      const std::string_view member = text.substr(colon + 1);
      if (!set_stereotype(c, member, at, ctx)) {
        add_member(c, member);
      }
      continue;
    }
    // `Animal:::cls` or a bare class name.
    const size_t colons = text.find(":::");
    if (colons != std::string_view::npos) {
      class_info& c = d.mention(std::string(trim(text.substr(0, colons))), at);
      d.styles.assign(c.id, std::string(trim(text.substr(colons + 3))), at);
      continue;
    }
    if (std::all_of(text.begin(), text.end(), [](char ch) { return is_id_char(ch) || ch == '~'; })) {
      d.mention(std::string(text), at);
      continue;
    }
    ctx.fail(at, "expected a class, a relation (A <|-- B), a member (A : +name) or a note, found '" + std::string(text) + "'");
  }
  if (block != nullptr) {
    ctx.fail(d.classes[block_index].at, "the { block of class " + d.classes[block_index].id + " is not closed by }");
  }
  if (d.classes.empty()) {
    ctx.fail(src.header_pos, "the class diagram has no classes");
  }
  element root("ClassDiagram", src.header_pos);
  root.text("title", title);
  if (d.direction != "TopDown") {
    root.name("direction", d.direction);
  }
  std::vector<std::string> ids;
  for (const class_info& c : d.classes) {
    ids.push_back(c.id);
    element e("Class", c.at);
    e.text("id", c.id);
    if (!c.name.empty() && c.name != c.id) {
      e.text("name", c.name);
    }
    if (c.kind != "Class") {
      e.name("kind", c.kind);
    }
    e.text("generic", member_text(c.generic));
    e.texts("attributes", c.attributes).texts("methods", c.methods);
    apply_style(e, d.styles.of(c.id, ctx));
    root.children.push_back(e);
  }
  d.styles.check_ids(ids, ctx);
  for (element& r : d.relations) {
    root.children.push_back(std::move(r));
  }
  for (element& n : d.notes) {
    root.children.push_back(std::move(n));
  }
  document doc;
  doc.root = root;
  return doc;
}

} // namespace bdg::nymph::mermaid
