// MIT License © 2026 Binary Dice Games
/// @file state.cpp
/// @brief mermaid `stateDiagram` / `stateDiagram-v2` → `StateDiagram`
///        (spec/16_mermaid_import/DESIGN.md section 4.4).
#include <algorithm>
#include <map>
#include "core/limits.hpp"
#include "mermaid/mermaid_internal.hpp"

namespace bdg::nymph::mermaid {

namespace {

struct state_info {
  std::string id;
  std::vector<std::string> names; ///< description lines
  std::string kind = "Simple";
  int parent = -1; ///< composite index in states, -1 for the diagram
  int seq = 0;
  pos at;
  std::string base; ///< a [*] pseudo-state: the id it is named after at the end
};

struct transition {
  int from = 0, to = 0;
  std::string label;
  pos at;
};

struct note {
  int target = 0;
  std::string text;
  pos at;
};

/// One scope: the diagram or a composite state.
struct scope {
  int composite = -1;     ///< index in states, -1 for the diagram
  int initial = -1;       ///< index of its [*] start, created on use
  int final = -1;         ///< index of its [*] end
  std::vector<transition> transitions;
  std::vector<note> notes;
};

struct diagram {
  context& ctx;
  std::vector<state_info> states;
  std::map<std::string, size_t> index;
  std::map<int, scope> scopes; ///< by composite index
  std::vector<int> open;       ///< open composites
  style_sheet styles;
  std::string direction = "TopDown";
  int seq = 0;

  explicit diagram(context& c) : ctx(c) {
    scopes[-1].composite = -1;
  }

  int current() const {
    return open.empty() ? -1 : open.back();
  }

  int mention(const std::string& id, pos at) {
    auto it = index.find(id);
    if (it == index.end()) {
      state_info s;
      s.id = id;
      s.at = at;
      s.parent = current();
      s.seq = seq++;
      index[id] = states.size();
      states.push_back(s);
      check_limit(states.size(), kMaxUmlNodes, "states", at, ctx);
      return static_cast<int>(states.size() - 1);
    }
    return static_cast<int>(it->second);
  }

  /// A free id for a pseudo-state: `base`, or `base_2`, ...
  std::string fresh(const std::string& base) const {
    std::string id = base;
    for (int n = 2; index.count(id) != 0; ++n) {
      id = base + "_" + std::to_string(n);
    }
    return id;
  }

  /// The [*] of the current scope: its start (as a source) or end. Its id
  /// is chosen at the end, so it never takes the id of a state written later.
  int pseudo(bool start, pos at) {
    scope& s = scopes[current()];
    int& slot = start ? s.initial : s.final;
    if (slot < 0) {
      state_info st;
      st.base = (current() < 0 ? "" : states[static_cast<size_t>(current())].id + "_") + (start ? "start" : "end");
      st.kind = start ? "Initial" : "Final";
      st.at = at;
      st.parent = current();
      st.seq = seq++;
      states.push_back(st);
      slot = static_cast<int>(states.size() - 1);
    }
    return slot;
  }

  /// Names the pseudo-states, avoiding every written id.
  void name_pseudo_states() {
    for (state_info& st : states) {
      if (!st.base.empty()) {
        st.id = fresh(st.base);
        index[st.id] = static_cast<size_t>(&st - states.data());
      }
    }
  }
};

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

/// A state reference with an optional `:::class`.
std::string state_ref(std::string_view text, pos at, diagram& d) {
  text = trim(text);
  const size_t colons = text.find(":::");
  if (colons != std::string_view::npos) {
    const std::string id(trim(text.substr(0, colons)));
    d.styles.assign(id, std::string(trim(text.substr(colons + 3))), at);
    return id;
  }
  return std::string(text);
}

/// `state ...`: a declaration, an alias, a pseudo-state or a composite.
void read_state(std::string_view rest, pos at, diagram& d) {
  bool opens = false;
  if (!rest.empty() && rest.back() == '{') {
    opens = true;
    rest = trim(rest.substr(0, rest.size() - 1));
  }
  std::string id;
  std::string name;
  if (!rest.empty() && rest.front() == '"') {
    const size_t close = rest.find('"', 1);
    const size_t as = rest.find(" as ", close == std::string_view::npos ? 0 : close);
    if (close == std::string_view::npos || as == std::string_view::npos) {
      d.ctx.fail(at, "expected state \"Description\" as id");
    }
    name = label_text(rest.substr(0, close + 1));
    id = state_ref(rest.substr(as + 4), at, d);
  } else {
    const size_t stereo = rest.find("<<");
    std::string_view head = stereo == std::string_view::npos ? rest : trim(rest.substr(0, stereo));
    const size_t colon = head.find(':');
    if (colon != std::string_view::npos && head.find(":::") != colon) {
      name = label_text(head.substr(colon + 1));
      head = trim(head.substr(0, colon));
    }
    id = state_ref(head, at, d);
    if (stereo != std::string_view::npos) {
      const std::string_view s = trim(rest.substr(stereo));
      const int i = d.mention(id, at);
      if (s == "<<choice>>") {
        d.states[static_cast<size_t>(i)].kind = "Choice";
      } else if (s == "<<fork>>") {
        d.states[static_cast<size_t>(i)].kind = "Fork";
      } else if (s == "<<join>>") {
        d.states[static_cast<size_t>(i)].kind = "Join";
      } else {
        d.ctx.fail(at, "unknown state stereotype " + std::string(s) + " (choice, fork, join)");
      }
    }
  }
  if (id.empty()) {
    d.ctx.fail(at, "state needs an id");
  }
  const int i = d.mention(id, at);
  if (!name.empty()) {
    d.states[static_cast<size_t>(i)].names.push_back(name);
  }
  if (opens) {
    if (d.open.size() >= kMaxUmlNesting) {
      d.ctx.fail(at, "composite states nest more than " + std::to_string(kMaxUmlNesting) + " deep");
    }
    d.open.push_back(i);
    d.scopes[i].composite = i;
  }
}

void read_transition(std::string_view text, size_t arrow, pos at, diagram& d) {
  std::string label;
  std::string_view right = text.substr(arrow + 3);
  const size_t colon = right.find(':');
  if (colon != std::string_view::npos && right.find(":::") != colon) {
    label = label_text(right.substr(colon + 1));
    right = right.substr(0, colon);
  }
  const std::string a = state_ref(text.substr(0, arrow), at, d);
  const std::string b = state_ref(right, at, d);
  if (a.empty() || b.empty()) {
    d.ctx.fail(at, "a transition needs a state on each side: a --> b");
  }
  const int from = a == "[*]" ? d.pseudo(true, at) : d.mention(a, at);
  const int to = b == "[*]" ? d.pseudo(false, at) : d.mention(b, at);
  d.scopes[d.current()].transitions.push_back({from, to, label, at});
}

} // namespace

document convert_state(const source_text& src, context& ctx) {
  diagram d(ctx);
  std::string title = src.title;
  for (size_t li = 0; li < src.body.size(); ++li) {
    const line& l = src.body[li];
    const pos at = l.start();
    const std::string_view text = trim(l.text);
    const auto [word, rest] = word_and_rest(text);
    if (text == "}") {
      if (d.open.empty()) {
        ctx.fail(at, "'}' without a composite state");
      }
      d.open.pop_back();
      continue;
    }
    if (text == "--") {
      ctx.fail(at, "concurrent regions (--) are not supported by nymph; split them into separate composite states");
    }
    if (word == "direction") {
      d.direction = direction_name(rest, at, ctx);
      continue;
    }
    if (word == "title") {
      title = label_text(rest);
      continue;
    }
    if (word == "hide" || word == "scale") {
      continue;
    }
    if (word == "note") {
      // `note right of s : text` or a block up to `end note`.
      std::string_view r = rest;
      const bool left = starts_with(r, "left of ");
      if (!left && !starts_with(r, "right of ")) {
        ctx.fail(at, "a note is 'left of' or 'right of' a state");
      }
      r = trim(r.substr(left ? 8 : 9));
      std::string text_of_note;
      const size_t colon = r.find(':');
      std::string id(trim(r.substr(0, colon)));
      if (colon != std::string_view::npos) {
        text_of_note = label_text(r.substr(colon + 1));
      } else {
        size_t j = li + 1;
        for (; j < src.body.size() && trim(src.body[j].text) != "end note"; ++j) {
          text_of_note += (text_of_note.empty() ? "" : "\n") + std::string(trim(src.body[j].text));
        }
        if (j >= src.body.size()) {
          ctx.fail(at, "the note is not closed by 'end note'");
        }
        li = j;
      }
      d.scopes[d.current()].notes.push_back({d.mention(id, at), text_of_note, at});
      continue;
    }
    if (word == "click") {
      ctx.warn(at, "click is ignored (a nymph figure has no links or callbacks)");
      continue;
    }
    if (d.styles.read(word, rest, at, true, ctx)) {
      continue;
    }
    if (word == "state") {
      read_state(rest, at, d);
      continue;
    }
    if (const size_t arrow = text.find("-->"); arrow != std::string_view::npos) {
      read_transition(text, arrow, at, d);
      continue;
    }
    // `s1 : description`, `s1:::cls` or a bare state.
    const size_t colon = text.find(':');
    if (colon != std::string_view::npos && text.find(":::") != colon) {
      const int i = d.mention(state_ref(text.substr(0, colon), at, d), at);
      d.states[static_cast<size_t>(i)].names.push_back(label_text(text.substr(colon + 1)));
      continue;
    }
    if (text.find(' ') == std::string_view::npos) {
      d.mention(state_ref(text, at, d), at);
      continue;
    }
    ctx.fail(at, "expected a transition (a --> b), a state or a note, found '" + std::string(text) + "'");
  }
  if (!d.open.empty()) {
    ctx.fail(d.states[static_cast<size_t>(d.open.back())].at, "the composite state is not closed by '}'");
  }
  if (d.states.empty()) {
    ctx.fail(src.header_pos, "the state diagram has no states");
  }
  d.name_pseudo_states();
  auto transition_element = [&](const transition& t) {
    element e("Transition", t.at);
    e.text("from", d.states[static_cast<size_t>(t.from)].id).text("to", d.states[static_cast<size_t>(t.to)].id);
    e.text("label", t.label);
    return e;
  };
  auto note_element = [&](const note& n) {
    element e("Note", n.at);
    e.text("text", n.text).texts("attach", {d.states[static_cast<size_t>(n.target)].id});
    return e;
  };
  // Elements, innermost composites first (a child is created after its parent).
  std::vector<std::string> ids;
  std::vector<std::vector<size_t>> members(d.states.size() + 1);
  for (size_t i = 0; i < d.states.size(); ++i) {
    ids.push_back(d.states[i].id);
    members[static_cast<size_t>(d.states[i].parent + 1)].push_back(i);
  }
  d.styles.check_ids(ids, ctx);
  auto order = [&](std::vector<size_t>& list, int composite) {
    const scope& s = d.scopes[composite];
    std::stable_sort(list.begin(), list.end(), [&](size_t a, size_t b) {
      auto rank = [&](size_t i) { return static_cast<int>(i) == s.initial ? 0 : static_cast<int>(i) == s.final ? 2 : 1; };
      return rank(a) != rank(b) ? rank(a) < rank(b) : d.states[a].seq < d.states[b].seq;
    });
  };
  std::vector<element> built(d.states.size());
  for (size_t i = d.states.size(); i-- > 0;) {
    const state_info& s = d.states[i];
    element e("State", s.at);
    e.text("id", s.id);
    std::string name;
    for (const std::string& n : s.names) {
      name += (name.empty() ? "" : "\n") + n;
    }
    if (!name.empty() && name != s.id) {
      e.text("name", name);
    }
    if (s.kind != "Simple") {
      e.name("kind", s.kind);
    }
    apply_style(e, d.styles.of(s.id, ctx));
    std::vector<size_t>& inner = members[i + 1];
    order(inner, static_cast<int>(i));
    for (size_t c : inner) {
      e.children.push_back(built[c]);
    }
    if (auto sc = d.scopes.find(static_cast<int>(i)); sc != d.scopes.end()) {
      for (const transition& t : sc->second.transitions) {
        e.children.push_back(transition_element(t));
      }
      for (const note& n : sc->second.notes) {
        e.children.push_back(note_element(n));
      }
    }
    built[i] = e;
  }
  element root("StateDiagram", src.header_pos);
  root.text("title", title);
  if (d.direction != "TopDown") {
    root.name("direction", d.direction);
  }
  order(members[0], -1);
  for (size_t c : members[0]) {
    root.children.push_back(built[c]);
  }
  for (const transition& t : d.scopes[-1].transitions) {
    root.children.push_back(transition_element(t));
  }
  for (const note& n : d.scopes[-1].notes) {
    root.children.push_back(note_element(n));
  }
  document doc;
  doc.root = root;
  return doc;
}

} // namespace bdg::nymph::mermaid
