// MIT License © 2026 Binary Dice Games
/// @file er.cpp
/// @brief mermaid `erDiagram` → `ERDiagram`
///        (spec/16_mermaid_import/DESIGN.md section 4.5).
#include <algorithm>
#include <cctype>
#include <map>
#include "core/limits.hpp"
#include "mermaid/mermaid_internal.hpp"

namespace bdg::nymph::mermaid {

namespace {

struct entity {
  std::string id;
  std::string name;
  std::vector<std::string> attributes;
  pos at;
};

struct diagram {
  context& ctx;
  std::vector<entity> entities;
  std::map<std::string, size_t> index;
  std::vector<element> relationships;
  style_sheet styles;
  std::string direction = "TopDown";

  explicit diagram(context& c) : ctx(c) {}

  /// `NAME`, `"NAME"`, `p[Person]`, `p["Person"]`, with `:::cls`.
  size_t mention(std::string_view text, pos at) {
    text = trim(text);
    std::string cls;
    const size_t colons = text.find(":::");
    if (colons != std::string_view::npos) {
      cls = std::string(trim(text.substr(colons + 3)));
      text = trim(text.substr(0, colons));
    }
    std::string id;
    std::string name;
    const size_t bracket = text.find('[');
    if (bracket != std::string_view::npos && !text.empty() && text.back() == ']') {
      id = std::string(trim(text.substr(0, bracket)));
      name = label_text(text.substr(bracket + 1, text.size() - bracket - 2));
    } else {
      if (!text.empty() && text.front() != '"' && text.find_first_of(" \t") != std::string_view::npos) {
        ctx.fail(at, "entity name '" + std::string(text) + "' has spaces: quote it (\"" + std::string(text) + "\")");
      }
      id = label_text(text);
    }
    if (id.empty()) {
      ctx.fail(at, "expected an entity name");
    }
    auto it = index.find(id);
    if (it == index.end()) {
      entity e;
      e.id = id;
      e.at = at;
      index[id] = entities.size();
      entities.push_back(e);
      check_limit(entities.size(), kMaxUmlNodes, "entities", at, ctx);
      it = index.find(id);
    }
    if (!name.empty()) {
      entities[it->second].name = name;
    }
    if (!cls.empty()) {
      styles.assign(id, cls, at);
    }
    return it->second;
  }
};

std::string lower(std::string_view text) {
  std::string out(text);
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return char(std::tolower(c)); });
  return out;
}

/// A symbol cardinality (`|o`, `||`, `}o`, `}|` on the left; mirrored on the right).
std::optional<std::string> symbol_cardinality(std::string_view s) {
  if (s == "|o" || s == "o|") {
    return "ZeroOrOne";
  }
  if (s == "||") {
    return "ExactlyOne";
  }
  if (s == "}o" || s == "o{") {
    return "ZeroOrMore";
  }
  if (s == "}|" || s == "|{") {
    return "OneOrMore";
  }
  return std::nullopt;
}

/// A word cardinality (`zero or more`, `only one`, `many(0)`, `1+`, ...).
std::optional<std::string> word_cardinality(std::string_view s) {
  const std::string w = lower(trim(s));
  if (w == "one or zero" || w == "zero or one") {
    return "ZeroOrOne";
  }
  if (w == "only one" || w == "1" || w == "exactly one") {
    return "ExactlyOne";
  }
  if (w == "zero or more" || w == "zero or many" || w == "many(0)" || w == "0+") {
    return "ZeroOrMore";
  }
  if (w == "one or more" || w == "one or many" || w == "many(1)" || w == "1+") {
    return "OneOrMore";
  }
  return std::nullopt;
}

struct parsed {
  std::string a, b, from_card, to_card;
  bool identifying = true;
};

/// `A ||--o{ B` (symbols) or `A one or more to zero or many B` (words).
std::optional<parsed> parse_relationship(std::string_view text) {
  // Symbols: find `--` or `..` with two characters on each side.
  for (size_t i = 2; i + 4 <= text.size(); ++i) {
    const std::string_view mid = text.substr(i, 2);
    if (mid != "--" && mid != "..") {
      continue;
    }
    const std::optional<std::string> l = symbol_cardinality(text.substr(i - 2, 2));
    const std::optional<std::string> r = symbol_cardinality(text.substr(i + 2, 2));
    if (!l || !r) {
      continue;
    }
    return parsed{std::string(trim(text.substr(0, i - 2))), std::string(trim(text.substr(i + 4))), *l, *r, mid == "--"};
  }
  // Words: `<A> <card> [optionally ]to <card> <B>`.
  const std::string low = lower(text);
  for (const std::string_view sep : {std::string_view(" optionally to "), std::string_view(" to ")}) {
    const size_t at = low.find(sep);
    if (at == std::string::npos) {
      continue;
    }
    const std::string_view left = trim(text.substr(0, at));
    const std::string_view right = trim(text.substr(at + sep.size()));
    // The longest cardinality phrase at the end of left / start of right.
    for (size_t split = 0; split < left.size(); ++split) {
      if (left[split] != ' ') {
        continue;
      }
      const std::optional<std::string> l = word_cardinality(left.substr(split + 1));
      if (!l) {
        continue;
      }
      for (size_t rs = right.size(); rs > 0; --rs) {
        if (rs < right.size() && right[rs] != ' ') {
          continue;
        }
        const std::optional<std::string> r = word_cardinality(right.substr(0, rs));
        if (r && rs < right.size()) {
          return parsed{std::string(trim(left.substr(0, split))), std::string(trim(right.substr(rs))), *l, *r,
                        sep == " to "};
        }
      }
    }
  }
  return std::nullopt;
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

document convert_er(const source_text& src, context& ctx) {
  diagram d(ctx);
  std::string title = src.title;
  int block = -1; ///< the entity whose { } block is open
  for (const line& l : src.body) {
    const pos at = l.start();
    std::string_view text = trim(l.text);
    if (block >= 0) {
      if (text == "}") {
        block = -1;
        continue;
      }
      bool closes = false;
      if (!text.empty() && text.back() == '}') {
        text = trim(text.substr(0, text.size() - 1));
        closes = true;
      }
      if (!text.empty()) {
        d.entities[static_cast<size_t>(block)].attributes.emplace_back(text);
      }
      if (closes) {
        block = -1;
      }
      continue;
    }
    const auto [word, rest] = word_and_rest(text);
    if (word == "direction") {
      d.direction = direction_name(rest, at, ctx);
      continue;
    }
    if (word == "title") {
      title = label_text(rest);
      continue;
    }
    if (d.styles.read(word, rest, at, true, ctx)) {
      continue;
    }
    // A relationship: `A ||--o{ B : label`.
    std::string_view head = text;
    std::string label;
    {
      bool quoted = false;
      for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '"') {
          quoted = !quoted;
        } else if (text[i] == ':' && !quoted && (i + 2 >= text.size() || text.substr(i, 3) != ":::") &&
                   (i == 0 || text[i - 1] != ':')) {
          head = text.substr(0, i);
          label = label_text(text.substr(i + 1));
          break;
        }
      }
    }
    if (const std::optional<parsed> r = parse_relationship(head)) {
      const size_t a = d.mention(r->a, at);
      const size_t b = d.mention(r->b, at);
      element e("Relationship", at);
      e.text("from", d.entities[a].id).text("to", d.entities[b].id);
      e.name("from_cardinality", r->from_card).name("to_cardinality", r->to_card);
      e.text("label", label);
      if (!r->identifying) {
        e.flag("identifying", false);
      }
      d.relationships.push_back(e);
      continue;
    }
    // An entity, with or without an attribute block.
    std::string_view name = text;
    bool opens = false;
    if (!name.empty() && name.back() == '{') {
      opens = true;
      name = trim(name.substr(0, name.size() - 1));
    } else if (const size_t brace = name.find('{'); brace != std::string_view::npos && name.back() == '}') {
      // `A { int id PK }` on one line.
      const size_t e = d.mention(name.substr(0, brace), at);
      const std::string_view body = trim(name.substr(brace + 1, name.size() - brace - 2));
      if (!body.empty()) {
        d.entities[e].attributes.emplace_back(body);
      }
      continue;
    }
    if (name.find(' ') != std::string_view::npos && name.front() != '"' && name.find('[') == std::string_view::npos) {
      ctx.fail(at, "expected a relationship (A ||--o{ B : label) or an entity, found '" + std::string(text) + "'");
    }
    const size_t e = d.mention(name, at);
    if (opens) {
      block = static_cast<int>(e);
    }
  }
  if (block >= 0) {
    ctx.fail(d.entities[static_cast<size_t>(block)].at, "the { block of " + d.entities[static_cast<size_t>(block)].id + " is not closed by }");
  }
  if (d.entities.empty()) {
    ctx.fail(src.header_pos, "the ER diagram has no entities");
  }
  element root("ERDiagram", src.header_pos);
  root.text("title", title);
  if (d.direction != "TopDown") {
    root.name("direction", d.direction);
  }
  std::vector<std::string> ids;
  for (const entity& e : d.entities) {
    ids.push_back(e.id);
    element x("Entity", e.at);
    x.text("id", e.id);
    if (!e.name.empty() && e.name != e.id) {
      x.text("name", e.name);
    }
    x.texts("attributes", e.attributes);
    apply_style(x, d.styles.of(e.id, ctx));
    root.children.push_back(x);
  }
  d.styles.check_ids(ids, ctx);
  for (element& r : d.relationships) {
    root.children.push_back(std::move(r));
  }
  document doc;
  doc.root = root;
  return doc;
}

} // namespace bdg::nymph::mermaid
