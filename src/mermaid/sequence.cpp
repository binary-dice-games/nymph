// MIT License © 2026 Binary Dice Games
/// @file sequence.cpp
/// @brief mermaid `sequenceDiagram` → `SequenceDiagram`
///        (spec/16_mermaid_import/DESIGN.md section 4.2).
#include <algorithm>
#include <cctype>
#include <map>
#include "core/limits.hpp"
#include "mermaid/mermaid_internal.hpp"

namespace bdg::nymph::mermaid {

namespace {

struct participant {
  std::string id;
  std::string name;
  std::string kind = "Participant";
  bool created = false;
  pos at;
};

struct frame {
  bool box = false; ///< a `box` block: no element
  size_t index = 0; ///< the Fragment's index in its parent's children
  pos at;
};

struct sequence {
  context& ctx;
  std::vector<participant> participants;
  std::map<std::string, size_t> index;
  element root;
  std::vector<frame> open;
  std::optional<std::vector<size_t>> last_message; ///< path of child indices from root
  std::string pending_create; ///< the next message to it is a Create
  pos pending_create_at;
  std::string pending_destroy;
  pos pending_destroy_at;

  explicit sequence(context& c) : ctx(c) {}

  participant& mention(const std::string& id, pos at) {
    auto it = index.find(id);
    if (it == index.end()) {
      participant p;
      p.id = id;
      p.at = at;
      index[id] = participants.size();
      participants.push_back(p);
      check_limit(participants.size(), kMaxUmlNodes, "participants", at, ctx);
      it = index.find(id);
    }
    return participants[it->second];
  }

  /// The innermost open Fragment (or root) and its path.
  element& container(std::vector<size_t>* path = nullptr) {
    element* e = &root;
    for (const frame& f : open) {
      if (!f.box) {
        e = &e->children[f.index];
        if (path != nullptr) {
          path->push_back(f.index);
        }
      }
    }
    return *e;
  }
  std::vector<element>& children() {
    return container().children;
  }
  element* resolve(const std::vector<size_t>& path) {
    element* e = &root;
    for (size_t i : path) {
      e = &e->children[i];
    }
    return e;
  }
  bool in_fragment() const {
    return std::any_of(open.begin(), open.end(), [](const frame& f) { return !f.box; });
  }
};

std::string lower(std::string_view text) {
  std::string out(text);
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return char(std::tolower(c)); });
  return out;
}

/// Splits `word rest` at the first space.
std::pair<std::string, std::string> word_and_rest(std::string_view text) {
  text = trim(text);
  const size_t space = text.find_first_of(" \t");
  if (space == std::string_view::npos) {
    return {std::string(text), {}};
  }
  return {std::string(text.substr(0, space)), std::string(trim(text.substr(space)))};
}

/// `X`, `X as Name`, `X@{ "type": "database" }`.
void read_participant(std::string_view rest, pos at, const std::string& default_kind, bool created, sequence& s) {
  rest = trim(rest);
  std::string id;
  std::string kind = default_kind;
  std::string name;
  const size_t attr = rest.find("@{");
  std::string_view head = rest;
  if (attr != std::string_view::npos) {
    head = rest.substr(0, attr);
    const size_t close = rest.find('}', attr);
    if (close == std::string_view::npos) {
      s.ctx.fail(at, "expected '}' to close @{");
    }
    const std::string_view body = rest.substr(attr + 2, close - attr - 2);
    for (const std::string& part : split_outside_quotes(body, ',')) {
      const size_t colon = part.find(':');
      if (colon == std::string::npos) {
        continue;
      }
      std::string key = label_text(trim(std::string_view(part).substr(0, colon)));
      std::string value = label_text(trim(std::string_view(part).substr(colon + 1)));
      if (key == "type") {
        static const std::pair<std::string_view, std::string_view> kinds[] = {
            {"participant", "Participant"}, {"actor", "Actor"},           {"boundary", "Boundary"},
            {"control", "Control"},         {"entity", "Entity"},         {"database", "Database"},
            {"collections", "Participant"}, {"queue", "Participant"}};
        bool found = false;
        for (const auto& [m, n] : kinds) {
          if (value == m) {
            kind = std::string(n);
            found = true;
          }
        }
        if (!found) {
          s.ctx.warn(at, "participant type '" + value + "' is drawn as a participant box");
        }
      } else if (key == "alias") {
        name = value;
      } else {
        s.ctx.warn(at, "participant attribute '" + key + "' is ignored");
      }
    }
    std::string_view after = trim(rest.substr(close + 1));
    if (starts_with(after, "as ")) {
      name = label_text(after.substr(3));
    }
  }
  const size_t as = head.find(" as ");
  if (as != std::string_view::npos) {
    id = std::string(trim(head.substr(0, as)));
    name = label_text(head.substr(as + 4));
  } else {
    id = std::string(trim(head));
  }
  if (id.empty()) {
    s.ctx.fail(at, "participant needs an id");
  }
  participant& p = s.mention(id, at);
  p.kind = kind;
  if (!name.empty()) {
    p.name = name;
  }
  if (created) {
    p.created = true;
    s.pending_create = id;
    s.pending_create_at = at;
  }
}

struct arrow {
  std::string_view token;
  std::string_view kind;
};

constexpr arrow kArrows[] = {
    {"<<-->>", "Reply"}, {"<<->>", "Sync"}, {"-->>", "Reply"}, {"->>", "Sync"}, {"-->", "Reply"},
    {"->", "Sync"},      {"--x", "Reply"},  {"-x", "Sync"},    {"--)", "Async"}, {"-)", "Async"},
};

/// The first arrow in `text` at or after `from`: its position and entry.
std::optional<std::pair<size_t, const arrow*>> find_arrow(std::string_view text) {
  for (size_t i = 0; i < text.size(); ++i) {
    if (text[i] != '-' && text[i] != '<') {
      continue;
    }
    for (const arrow& a : kArrows) {
      if (text.substr(i, a.token.size()) == a.token) {
        return std::pair{i, &a};
      }
    }
  }
  return std::nullopt;
}

bool read_message(const line& l, std::string_view text, size_t offset, sequence& s) {
  const std::string_view body = text.substr(0, text.find(':'));
  const auto found = find_arrow(body);
  if (!found) {
    return false;
  }
  const pos at = l.at(offset);
  const auto [i, a] = *found;
  const std::string from(trim(text.substr(0, i)));
  std::string_view rest = text.substr(i + a->token.size());
  bool activate = false;
  bool deactivate = false;
  rest = trim(rest);
  if (starts_with(rest, "+")) {
    activate = true;
    rest = trim(rest.substr(1));
  } else if (starts_with(rest, "-")) {
    deactivate = true;
    rest = trim(rest.substr(1));
  }
  const size_t colon = rest.find(':');
  const std::string to(trim(rest.substr(0, colon)));
  const std::string message = colon == std::string_view::npos ? "" : label_text(rest.substr(colon + 1));
  if (from.empty() || to.empty()) {
    s.ctx.fail(at, "a message needs a sender and a receiver: A->>B: text");
  }
  s.mention(from, at);
  s.mention(to, at);
  element e("Message", at);
  e.text("from", from).text("to", to).text("text", message);
  std::string kind(a->kind);
  if (s.pending_create == to) {
    kind = "Create";
    s.pending_create.clear();
  }
  if (!s.pending_destroy.empty() && (s.pending_destroy == to || s.pending_destroy == from)) {
    if (s.pending_destroy == to) {
      kind = "Destroy";
    } else {
      s.ctx.warn(s.pending_destroy_at, "destroy " + from + " is drawn without a cross (nymph ends the lifeline that receives the destroying message)");
    }
    s.pending_destroy.clear();
  }
  if (kind != "Sync") {
    e.name("kind", kind);
  }
  if (activate) {
    e.flag("activate", true);
  }
  if (deactivate) {
    e.flag("deactivate", true);
  }
  std::vector<size_t> path;
  element& parent = s.container(&path);
  parent.children.push_back(e);
  path.push_back(parent.children.size() - 1);
  s.last_message = path;
  return true;
}

void read_note(std::string_view rest, pos at, sequence& s) {
  const size_t colon = rest.find(':');
  if (colon == std::string_view::npos) {
    s.ctx.fail(at, "a note needs ':' and its text");
  }
  const std::string where = lower(trim(rest.substr(0, colon)));
  const std::string text = label_text(rest.substr(colon + 1));
  const std::string_view original = trim(rest.substr(0, colon));
  element e("Note", at);
  e.text("text", text);
  auto ids = [&](size_t skip) {
    std::vector<std::string> out;
    for (const std::string& id : split_outside_quotes(original.substr(skip), ',')) {
      if (!id.empty()) {
        s.mention(id, at);
        out.push_back(id);
      }
    }
    return out;
  };
  if (starts_with(where, "left of ")) {
    e.texts("attach", ids(8)).name("side", "Left");
  } else if (starts_with(where, "right of ")) {
    e.texts("attach", ids(9));
  } else if (starts_with(where, "over ")) {
    const std::vector<std::string> over = ids(5);
    if (over.size() > 2) {
      s.ctx.fail(at, "a note goes over one or two participants");
    }
    e.texts("over", over);
  } else {
    s.ctx.fail(at, "a note is 'left of', 'right of' or 'over' participants");
  }
  s.children().push_back(e);
  s.last_message.reset();
}

void read_line(const line& l, size_t begin, size_t end, sequence& s) {
  const std::string_view whole = std::string_view(l.text).substr(begin, end - begin);
  const size_t first = whole.find_first_not_of(" \t");
  if (first == std::string_view::npos) {
    return;
  }
  const std::string_view text = trim(whole);
  const size_t offset = begin + first;
  const pos at = l.at(offset);
  const auto [word, rest] = word_and_rest(text);
  const std::string w = lower(word);
  if (word == "participant" || word == "actor") {
    read_participant(rest, at, word == "actor" ? "Actor" : "Participant", false, s);
    return;
  }
  if (word == "create") {
    const auto [what, who] = word_and_rest(rest);
    if (what != "participant" && what != "actor") {
      s.ctx.fail(at, "create is followed by participant or actor");
    }
    read_participant(who, at, what == "actor" ? "Actor" : "Participant", true, s);
    return;
  }
  if (word == "destroy") {
    s.mention(rest, at);
    s.pending_destroy = rest;
    s.pending_destroy_at = at;
    return;
  }
  if (word == "autonumber") {
    s.root.flag("autonumber", true);
    if (!rest.empty()) {
      s.ctx.warn(at, "autonumber's start and step are ignored (nymph numbers from 1)");
    }
    return;
  }
  if (word == "title") {
    s.root.text("title", label_text(rest));
    return;
  }
  if (word == "activate" || word == "deactivate") {
    const bool on = word == "activate";
    element* m = s.last_message ? s.resolve(*s.last_message) : nullptr;
    const std::string* other = m == nullptr ? nullptr : m->get(on ? "to" : "from");
    if (other == nullptr || *other != yaml_text(rest)) {
      s.ctx.fail(at, on ? "activate " + rest + " must follow a message to " + rest + " (or write it as ->>+)"
                        : "deactivate " + rest + " must follow a message from " + rest + " (or write it as -->>-)");
    }
    m->flag(on ? "activate" : "deactivate", true);
    return;
  }
  if (w == "note") {
    read_note(rest, at, s);
    return;
  }
  static const std::pair<std::string_view, std::string_view> fragments[] = {
      {"loop", "Loop"}, {"alt", "Alt"}, {"opt", "Opt"}, {"par", "Par"}, {"critical", "Critical"}, {"break", "Break"},
      {"rect", "Group"}};
  for (const auto& [m, kind] : fragments) {
    if (word != m) {
      continue;
    }
    if (s.open.size() >= kMaxUmlNesting) {
      s.ctx.fail(at, "fragments nest more than " + std::to_string(kMaxUmlNesting) + " deep");
    }
    element e("Fragment", at);
    e.name("kind", kind);
    if (kind == "Group") {
      if (const std::optional<std::string> c = css_color(rest)) {
        e.raw("fill_color", "\"" + *c + "\"");
      } else if (!rest.empty()) {
        s.ctx.warn(at, "rect colour '" + rest + "' is ignored");
      }
    } else {
      e.text("label", label_text(rest));
    }
    std::vector<element>& list = s.children();
    list.push_back(e);
    s.open.push_back({false, list.size() - 1, at});
    s.last_message.reset();
    return;
  }
  if (word == "else" || word == "and" || word == "option") {
    if (s.open.empty() || s.open.back().box) {
      s.ctx.fail(at, word + " outside a fragment");
    }
    element e("Else", at);
    e.text("label", label_text(rest));
    s.children().push_back(e);
    s.last_message.reset();
    return;
  }
  if (word == "box") {
    s.ctx.warn(at, "box is dropped (its participants are kept)");
    s.open.push_back({true, 0, at});
    return;
  }
  if (word == "end") {
    if (s.open.empty()) {
      s.ctx.fail(at, "'end' without a block");
    }
    s.open.pop_back();
    s.last_message.reset();
    return;
  }
  if (word == "link" || word == "links" || word == "properties" || word == "details") {
    s.ctx.warn(at, word + " is ignored (a nymph figure has no menus or links)");
    return;
  }
  if (!read_message(l, text, offset, s)) {
    s.ctx.fail(at, "expected a message (A->>B: text), a participant, a note or a block, found '" + std::string(text) + "'");
  }
}

} // namespace

document convert_sequence(const source_text& src, context& ctx) {
  sequence s(ctx);
  s.root = element("SequenceDiagram", src.header_pos);
  s.root.text("title", src.title);
  for (const line& l : src.body) {
    // Statements may be separated by ';' (outside the text after ':').
    const size_t colon = l.text.find(':');
    size_t begin = 0;
    for (size_t i = 0; i <= l.text.size(); ++i) {
      if (i == l.text.size() || (l.text[i] == ';' && (colon == std::string::npos || i < colon))) {
        read_line(l, begin, i, s);
        begin = i + 1;
      }
    }
  }
  if (!s.open.empty()) {
    ctx.fail(s.open.back().at, "this block is not closed by 'end'");
  }
  if (s.participants.empty()) {
    ctx.fail(src.header_pos, "the sequence diagram has no participants");
  }
  std::vector<element> children;
  for (const participant& p : s.participants) {
    element e("Participant", p.at);
    e.text("id", p.id);
    if (!p.name.empty() && p.name != p.id) {
      e.text("name", p.name);
    }
    if (p.kind != "Participant") {
      e.name("kind", p.kind);
    }
    if (p.created) {
      e.flag("created", true);
    }
    children.push_back(e);
  }
  for (element& e : s.root.children) {
    children.push_back(std::move(e));
  }
  s.root.children = std::move(children);
  document doc;
  doc.root = s.root;
  return doc;
}

} // namespace bdg::nymph::mermaid
