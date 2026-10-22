// MIT License © 2026 Binary Dice Games
/// @file tree_edit.cpp
/// @brief Structural text patches to a nymph YAML source (see tree_edit.hpp).
#include "edit/tree_edit.hpp"
#include <algorithm>
#include <charconv>
#include <map>
#include "core/error.hpp"
#include "document/bind.hpp"
#include "document/view.hpp"
#include "edit/field_value.hpp"
#include "edit/source_edit.hpp"
#include "edit/yaml_text.hpp"
#include "schema/element_class.hpp"

namespace bdg::nymph {

using namespace yaml_text;

namespace {

bool is_path_or_below(std::string_view path, std::string_view ancestor) {
  return path == ancestor ||
      (path.size() > ancestor.size() && path.compare(0, ancestor.size(), ancestor) == 0 && path[ancestor.size()] == '/');
}

std::string child_path(std::string_view parent, size_t index) {
  return std::string(parent) + "/children/" + std::to_string(index);
}

/// "P/children/N" as (P, N).
/// @throws nymph::error for `root` or a path that is not a child's.
std::pair<std::string, size_t> split_child(std::string_view path) {
  const size_t slash = path.rfind('/');
  constexpr std::string_view kChildren = "/children";
  if (slash == std::string_view::npos || slash < kChildren.size() ||
      path.substr(slash - kChildren.size(), kChildren.size()) != kChildren) {
    throw error(0, 0, path == "root" ? "the root element cannot be removed or moved" : "no element '" + std::string(path) + "'");
  }
  size_t index = 0;
  const std::string_view number = path.substr(slash + 1);
  if (std::from_chars(number.data(), number.data() + number.size(), index).ec != std::errc()) {
    throw error(0, 0, "no element '" + std::string(path) + "'");
  }
  return {std::string(path.substr(0, slash - kChildren.size())), index};
}

int key_index(const yaml_node& mapping, std::string_view key) {
  for (size_t i = 0; i < mapping.keys.size(); ++i) {
    if (mapping.keys[i].text == key) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

void check_one_line(std::string_view yaml) {
  if (yaml.find_first_of("\r\n") != std::string_view::npos) {
    throw error(0, 0, "a new element is written on one line");
  }
}

/// The `children` list of the element at `parent`, or nullptr when it has
/// none (no key, or a key with no value).
/// @throws nymph::error for a `children` value that is not a list.
const yaml_node* children_of(const yaml_node& parent) {
  const yaml_node* list = parent.find("children");
  if (list == nullptr || (list->is_scalar() && list->style == scalar_style::plain && list->text.empty())) {
    return nullptr;
  }
  if (!list->is_sequence()) {
    fail(*list, "'children' is not a list");
  }
  return list;
}

/// Offset of the dash of block list item `item`.
size_t dash_offset(const yaml_node& item, const text_index& t, std::string_view text) {
  size_t at = t.offset(item.line, item.column);
  while (at > 0) {
    const char c = text[at - 1];
    if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
      break;
    }
    --at;
  }
  if (at == 0 || text[at - 1] != '-') {
    fail(item, "cannot find the '-' of this list item");
  }
  return at - 1;
}

/// The lines a block list item occupies: from its dash's line to its last.
struct item_lines {
  int first = 0;
  int last = 0;
};

item_lines lines_of(const yaml_node& item, const text_index& t, std::string_view text) {
  return {t.line_of(dash_offset(item, t, text)), last_line(item, t)};
}

/// The spaces before the dash of a block list's items.
std::string item_indent(const yaml_node& list, const text_index& t, std::string_view text) {
  const size_t dash = dash_offset(list.children.front(), t, text);
  return std::string(dash - t.line_start(t.line_of(dash)), ' ');
}

/// The text of list item `i`: the item itself in a flow list, its whole
/// lines (indentation and dash included) in a block list.
std::string item_text(const yaml_node& list, size_t i, const text_index& t, std::string_view text) {
  const yaml_node& item = list.children[i];
  if (is_flow(list, t)) {
    const size_t from = t.offset(item.line, item.column);
    return std::string(text.substr(from, value_end(item, t) - from));
  }
  const item_lines l = lines_of(item, t, text);
  const size_t from = t.line_start(l.first);
  return std::string(text.substr(from, t.line_end(l.last) - from));
}

/// Inserts `piece` as item `index` of `list`: an item's text in a flow list,
/// whole lines (indented, with their dash) in a block list.
void insert_item(std::string& out, const yaml_node& list, size_t index, const std::string& piece, const text_index& t, std::string_view text) {
  const size_t n = list.children.size();
  if (is_flow(list, t)) {
    if (n == 0) {
      out.insert(t.offset(list.line, list.column) + 1, piece);
    } else if (index < n) {
      out.insert(t.offset(list.children[index].line, list.children[index].column), piece + ", ");
    } else {
      out.insert(value_end(list.children.back(), t), ", " + piece);
    }
    return;
  }
  if (index < n) {
    out.insert(t.line_start(lines_of(list.children[index], t, text).first), piece + t.eol());
  } else {
    out.insert(t.line_end(last_line(list.children.back(), t)), t.eol() + piece);
  }
}

/// Offset just after the ':' following mapping key `key`.
size_t after_colon(const yaml_node& key, const text_index& t) {
  size_t at = t.offset(key.end_line, key.end_column);
  while (t.at(at) == ' ' || t.at(at) == '\t') {
    ++at;
  }
  if (t.at(at) != ':') {
    fail(key, "cannot find the ':' after '" + key.text + "'");
  }
  return at + 1;
}

/// Removes item `i` of `list` (whose key in `parent` is `children`).
void erase_item(std::string& out, const yaml_node& parent, const yaml_node& list, size_t i, const text_index& t, std::string_view text) {
  const size_t n = list.children.size();
  if (is_flow(list, t)) {
    const auto start = [&](size_t k) { return t.offset(list.children[k].line, list.children[k].column); };
    if (i + 1 < n) {
      out.erase(start(i), start(i + 1) - start(i));
    } else if (i > 0) {
      const size_t from = value_end(list.children[i - 1], t);
      out.erase(from, value_end(list.children[i], t) - from);
    } else {
      out.erase(start(i), value_end(list.children[i], t) - start(i));
    }
    return;
  }
  if (n == 1) {
    // The last child: an empty flow list keeps `children` a list.
    const yaml_node& key = parent.keys[static_cast<size_t>(key_index(parent, "children"))];
    const size_t from = after_colon(key, t);
    out.replace(from, value_end(list, t) - from, " []");
    return;
  }
  const item_lines l = lines_of(list.children[i], t, text);
  size_t from = t.line_start(l.first);
  const size_t to = l.last < static_cast<int>(t.line_count()) ? t.line_start(l.last + 1) : text.size();
  if (to == text.size() && l.first > 1 && text.back() != '\n') {
    from = t.line_end(l.first - 1); // the last line: take the break before it
  }
  out.erase(from, to - from);
}

/// The element mapping at `path`, with its parent's and list's nodes.
struct child_at {
  std::string parent_path;
  size_t index = 0;
  const yaml_node* parent = nullptr;
  const yaml_node* list = nullptr;
};

child_at locate_child(const yaml_node& doc, std::string_view path) {
  child_at c;
  std::tie(c.parent_path, c.index) = split_child(path);
  c.parent = &element_at(doc, c.parent_path);
  c.list = children_of(*c.parent);
  if (c.list == nullptr || c.index >= c.list->children.size()) {
    fail(*c.parent, "no element '" + std::string(path) + "'");
  }
  return c;
}

/// Rules moving children `from` .. `to` - 1 of `parent` by `delta` places.
void shift(path_map& map, std::string_view parent, size_t from, size_t to, int delta) {
  for (size_t j = from; j < to; ++j) {
    map.add(child_path(parent, j), child_path(parent, static_cast<size_t>(static_cast<long long>(j) + delta)));
  }
}

/// Every element mapping in the subtree at `path` of `doc`, by path.
void collect_elements(const yaml_node& element, const std::string& path, std::vector<std::pair<std::string, const yaml_node*>>& out) {
  out.emplace_back(path, &element);
  const yaml_node* list = element.find("children");
  if (list != nullptr && list->is_sequence()) {
    for (size_t i = 0; i < list->children.size(); ++i) {
      if (list->children[i].is_mapping()) {
        collect_elements(list->children[i], child_path(path, i), out);
      }
    }
  }
}

std::vector<std::string_view> segments(std::string_view path) {
  return split_path(path);
}

/// `n` as one flow value (`{k: v, ...}`, `[a, b]`), each scalar and flow
/// value kept as written; block layout and comments are lost.
std::string flat_text(const yaml_node& n, const text_index& t, std::string_view text) {
  if (n.is_scalar() && is_block_scalar(n)) {
    return encode_text(n.text);
  }
  if (n.is_scalar() || is_flow(n, t)) {
    const size_t from = t.offset(n.line, n.column);
    return std::string(text.substr(from, value_end(n, t) - from));
  }
  std::string out;
  if (n.is_mapping()) {
    out = "{";
    for (size_t i = 0; i < n.keys.size(); ++i) {
      out += (i ? ", " : "") + n.keys[i].text + ": " + flat_text(n.children[i], t, text);
    }
    return out + "}";
  }
  out = "[";
  for (size_t i = 0; i < n.children.size(); ++i) {
    out += (i ? ", " : "") + flat_text(n.children[i], t, text);
  }
  return out + "]";
}

/// `lines` (whole lines, `eol`-separated) with `delta` spaces added to (or,
/// when negative, taken off) each non-blank line's indentation.
std::string reindent(std::string_view lines, int delta, std::string_view eol) {
  std::string out;
  size_t at = 0;
  while (at <= lines.size()) {
    size_t end = lines.find('\n', at);
    const bool last = end == std::string_view::npos;
    std::string_view line = lines.substr(at, last ? std::string_view::npos : end - at);
    const bool cr = !line.empty() && line.back() == '\r';
    if (cr) {
      line.remove_suffix(1);
    }
    const size_t spaces = std::min(line.find_first_not_of(' '), line.size());
    if (spaces == line.size()) {
      out += line;
    } else if (delta >= 0) {
      out += std::string(static_cast<size_t>(delta), ' ') + std::string(line);
    } else {
      out += line.substr(std::min(spaces, static_cast<size_t>(-delta)));
    }
    if (last) {
      break;
    }
    out += eol;
    at = end + 1;
  }
  return out;
}

} // namespace

// ── path_map ─────────────────────────────────────────────────────────────────

void path_map::add(std::string from, std::optional<std::string> to) {
  if (stages_.empty()) {
    stages_.emplace_back();
  }
  stages_.back().push_back({std::move(from), std::move(to)});
}

void path_map::then(const path_map& next) {
  stages_.insert(stages_.end(), next.stages_.begin(), next.stages_.end());
}

std::optional<std::string> path_map::apply(std::string_view path) const {
  std::string current(path);
  for (const std::vector<rule>& stage : stages_) {
    for (const rule& r : stage) {
      if (is_path_or_below(current, r.from)) {
        if (!r.to) {
          return std::nullopt;
        }
        current = *r.to + current.substr(r.from.size());
        break;
      }
    }
  }
  return current;
}

// ── Structural changes ───────────────────────────────────────────────────────

/// insert_child() for an element text that may span lines (a moved element).
static tree_edit_result
insert_child_text(std::string_view source, std::string_view parent_path, size_t index, std::string_view element_yaml) {
  const yaml_node doc = parse_yaml(source);
  const yaml_node& parent = element_at(doc, parent_path);
  const text_index t(source);
  tree_edit_result r;
  r.source = std::string(source);
  const yaml_node* list = children_of(parent);
  if (list == nullptr) {
    if (index != 0 && index != kAppend) {
      fail(parent, "'" + std::string(parent_path) + "' has no child " + std::to_string(index));
    }
    if (is_flow(parent, t) || parent.find("children") != nullptr) {
      r.source = set_field(source, parent_path, "children", "[" + std::string(element_yaml) + "]");
    } else if (parent.keys.empty()) {
      fail(parent, "an element is a mapping with a 'type'");
    } else {
      const std::string indent(static_cast<size_t>(parent.keys.front().column) - 1, ' ');
      r.source.insert(
          t.line_end(last_line(parent.children.back(), t)),
          std::string(t.eol()) + indent + "children:" + t.eol() + indent + "  - " + std::string(element_yaml));
    }
    r.path = child_path(parent_path, 0);
    return r;
  }
  const size_t n = list->children.size();
  const size_t at = index == kAppend ? n : index;
  if (at > n) {
    fail(*list, "'" + std::string(parent_path) + "' has " + std::to_string(n) + " children; no place " + std::to_string(index));
  }
  const std::string piece = is_flow(*list, t) || n == 0
      ? std::string(element_yaml)
      : item_indent(*list, t, source) + "- " + std::string(element_yaml);
  insert_item(r.source, *list, at, piece, t, source);
  // Later siblings move one place down, the last first.
  for (size_t j = n; j-- > at;) {
    r.paths.add(child_path(parent_path, j), child_path(parent_path, j + 1));
  }
  r.path = child_path(parent_path, at);
  return r;
}

tree_edit_result
insert_child(std::string_view source, std::string_view parent_path, size_t index, std::string_view element_yaml) {
  check_one_line(element_yaml);
  return insert_child_text(source, parent_path, index, element_yaml);
}

tree_edit_result remove_element(std::string_view source, std::string_view path) {
  const yaml_node doc = parse_yaml(source);
  const child_at c = locate_child(doc, path);
  const text_index t(source);
  tree_edit_result r;
  r.source = std::string(source);
  erase_item(r.source, *c.parent, *c.list, c.index, t, source);
  r.paths.add(std::string(path), std::nullopt);
  shift(r.paths, c.parent_path, c.index + 1, c.list->children.size(), -1);
  return r;
}

tree_edit_result move_element(std::string_view source, std::string_view path, size_t index) {
  const yaml_node doc = parse_yaml(source);
  const child_at c = locate_child(doc, path);
  const size_t n = c.list->children.size();
  const size_t to = std::min(index, n - 1);
  tree_edit_result r;
  r.source = std::string(source);
  r.path = child_path(c.parent_path, to);
  if (to == c.index) {
    return r;
  }
  const text_index t(source);
  const std::string piece = item_text(*c.list, c.index, t, source);
  erase_item(r.source, *c.parent, *c.list, c.index, t, source);
  const yaml_node doc2 = parse_yaml(r.source);
  const yaml_node* list2 = children_of(element_at(doc2, c.parent_path));
  const text_index t2(r.source);
  const std::string after_erase = r.source;
  insert_item(r.source, *list2, to, piece, t2, after_erase);
  r.paths.add(std::string(path), r.path);
  if (to > c.index) {
    shift(r.paths, c.parent_path, c.index + 1, to + 1, -1);
  } else {
    shift(r.paths, c.parent_path, to, c.index, +1);
  }
  return r;
}

tree_edit_result reparent_element(std::string_view source, std::string_view path, std::string_view new_parent, size_t index) {
  const yaml_node doc = parse_yaml(source);
  const child_at c = locate_child(doc, path);
  (void)element_at(doc, new_parent); // an unknown parent throws
  if (is_path_or_below(new_parent, path)) {
    throw error(0, 0, "an element cannot be moved into itself");
  }
  if (new_parent == c.parent_path) {
    return move_element(source, path, index);
  }
  const text_index t(source);

  // Stage 1: the element leaves; its place is held by a marker path that its
  // descendants carry, and its later siblings move up.
  const std::string marker = "moved";
  tree_edit_result r;
  r.paths.add(std::string(path), marker);
  shift(r.paths, c.parent_path, c.index + 1, c.list->children.size(), -1);
  std::string removed(source);
  erase_item(removed, *c.parent, *c.list, c.index, t, source);

  // The new parent, as the removal left it.
  const std::string target = *r.paths.apply(new_parent);
  const yaml_node doc2 = parse_yaml(removed);
  const yaml_node* list = children_of(element_at(doc2, target));
  const size_t n = list == nullptr ? 0 : list->children.size();
  const size_t at = index == kAppend ? n : index;
  if (at > n) {
    throw error(0, 0, "'" + target + "' has " + std::to_string(n) + " children; no place " + std::to_string(index));
  }

  // Stage 2: the element arrives, in block lines when both lists are blocks,
  // else as one flow value.
  const yaml_node& item = c.list->children[c.index];
  const text_index t2(removed);
  path_map arrival;
  if (list != nullptr && n > 0 && !is_flow(*list, t2) && !is_flow(*c.list, t)) {
    const int delta = static_cast<int>(item_indent(*list, t2, removed).size()) -
        static_cast<int>(item_indent(*c.list, t, source).size());
    r.source = removed;
    insert_item(r.source, *list, at, reindent(item_text(*c.list, c.index, t, source), delta, t.eol()), t2, removed);
    for (size_t j = n; j-- > at;) {
      arrival.add(child_path(target, j), child_path(target, j + 1));
    }
    arrival.add(marker, child_path(target, at));
    r.path = child_path(target, at);
  } else {
    tree_edit_result inserted = insert_child_text(removed, target, at, flat_text(item, t, source));
    r.source = std::move(inserted.source);
    arrival = std::move(inserted.paths);
    arrival.add(marker, inserted.path);
    r.path = inserted.path;
  }
  r.paths.then(arrival);
  return r;
}

tree_edit_result duplicate_element(std::string_view source, std::string_view path, const std::set<std::string>& used_ids) {
  const yaml_node doc = parse_yaml(source);
  const child_at c = locate_child(doc, path);
  const text_index t(source);
  const std::string piece = item_text(*c.list, c.index, t, source);
  tree_edit_result r;
  r.source = std::string(source);
  insert_item(r.source, *c.list, c.index + 1, piece, t, source);
  for (size_t j = c.list->children.size(); j-- > c.index + 1;) {
    r.paths.add(child_path(c.parent_path, j), child_path(c.parent_path, j + 1));
  }
  r.path = child_path(c.parent_path, c.index + 1);

  // Fresh ids for the copy, and its own references to them.
  const yaml_node copy_doc = parse_yaml(r.source);
  std::vector<std::pair<std::string, const yaml_node*>> elements;
  collect_elements(element_at(copy_doc, r.path), r.path, elements);
  std::set<std::string> used = used_ids;
  std::map<std::string, std::string> renamed;
  for (const auto& [element_path, element] : elements) {
    const yaml_node* id = element->find("id");
    if (id != nullptr && id->is_scalar() && !id->text.empty() && !renamed.contains(id->text)) {
      const std::string fresh = fresh_id(id->text, used);
      used.insert(fresh);
      renamed[id->text] = fresh;
    }
  }
  if (renamed.empty()) {
    return r;
  }
  struct patch {
    std::string element;
    std::string key;
    std::string value;
  };
  std::vector<patch> patches;
  for (const auto& [element_path, element] : elements) {
    for (size_t k = 0; k < element->keys.size(); ++k) {
      const std::string& key = element->keys[k].text;
      const yaml_node& value = element->children[k];
      if (key == "type" || key == "children") {
        continue;
      }
      if (value.is_scalar() && !is_block_scalar(value)) {
        if (const auto it = renamed.find(value.text); it != renamed.end()) {
          patches.push_back({element_path, key, encode_text(it->second)});
        }
      } else if (value.is_sequence()) {
        bool changed = false;
        std::string list = "[";
        for (size_t i = 0; i < value.children.size(); ++i) {
          const yaml_node& item = value.children[i];
          if (!item.is_scalar()) {
            changed = false;
            break;
          }
          const auto it = renamed.find(item.text);
          changed = changed || it != renamed.end();
          list += (i ? ", " : "") + encode_text(it != renamed.end() ? it->second : item.text);
        }
        if (changed) {
          patches.push_back({element_path, key, list + "]"});
        }
      }
    }
  }
  for (const patch& p : patches) {
    r.source = set_field(r.source, p.element, p.key, p.value);
  }
  return r;
}

tree_edit_result wrap_root(std::string_view source, std::string_view layout_type) {
  const yaml_node doc = parse_yaml(source);
  const yaml_node* key = doc.find_key("root");
  const yaml_node* root = doc.find("root");
  if (!doc.is_mapping() || key == nullptr || root == nullptr || !root->is_mapping()) {
    fail(doc, "the source has no root element");
  }
  if (key->column != 1) {
    fail(*key, "the source's top level is not 'key: value' lines");
  }
  const text_index t(source);
  const std::string eol = t.eol();
  std::string text;
  if (is_flow(*root, t)) {
    const size_t from = t.offset(root->line, root->column);
    const std::string flow(source.substr(from, value_end(*root, t) - from));
    for (size_t i = 0; i < flow.size(); ++i) {
      text += flow[i];
      if (flow[i] == '\n' && i + 1 < flow.size()) {
        text += "    "; // continuation lines stay inside the new item
      }
    }
  } else {
    const size_t k = static_cast<size_t>(root->column) - 1;
    const size_t from = t.offset(root->line, root->column);
    text = std::string(source.substr(from, t.line_end(root->line) - from));
    const int last = last_line(*root, t);
    for (int line = root->line + 1; line <= last; ++line) {
      const std::string_view l = t.line_text(line);
      const size_t spaces = std::min(l.find_first_not_of(' '), l.size());
      text += eol;
      if (spaces < l.size()) {
        text += "      " + std::string(l.substr(std::min(spaces, k)));
      }
    }
  }
  const size_t colon = after_colon(*key, t);
  tree_edit_result r;
  r.source = std::string(source);
  r.source.replace(
      colon, value_end(*root, t) - colon,
      eol + "  type: " + std::string(layout_type) + eol + "  children:" + eol + "    - " + text);
  r.paths.add("root", std::string("root/children/0"));
  r.path = "root";
  return r;
}

std::string rename_view_paths(std::string_view source, const path_map& map) {
  if (map.empty()) {
    return std::string(source);
  }
  const yaml_node doc = parse_yaml(source);
  const yaml_node* view = doc.find("view");
  if (view == nullptr || !view->is_mapping() || view->keys.empty()) {
    return std::string(source);
  }
  std::vector<std::optional<std::string>> renamed;
  bool changed = false;
  bool any_left = false;
  for (const yaml_node& key : view->keys) {
    renamed.push_back(map.apply(key.text));
    changed = changed || renamed.back() != std::optional<std::string>(key.text);
    any_left = any_left || renamed.back().has_value();
  }
  if (!changed) {
    return std::string(source);
  }
  if (!any_left) {
    return replace_top_level(source, "view", "").source;
  }
  const text_index t(source);
  std::string out(source);
  if (is_flow(*view, t)) {
    std::string flow = "{";
    bool first = true;
    for (size_t i = 0; i < view->keys.size(); ++i) {
      if (!renamed[i]) {
        continue;
      }
      const yaml_node& value = view->children[i];
      const size_t from = t.offset(value.line, value.column);
      flow += (first ? "" : ", ") + *renamed[i] + ": " + std::string(source.substr(from, value_end(value, t) - from));
      first = false;
    }
    const size_t from = t.offset(view->line, view->column);
    out.replace(from, value_end(*view, t) - from, flow + "}");
    return out;
  }
  // Block mapping: each entry on its own lines, changed from the last up.
  for (size_t i = view->keys.size(); i-- > 0;) {
    const yaml_node& key = view->keys[i];
    if (renamed[i] == std::optional<std::string>(key.text)) {
      continue;
    }
    if (renamed[i]) {
      const size_t from = t.offset(key.line, key.column);
      out.replace(from, t.offset(key.end_line, key.end_column) - from, *renamed[i]);
      continue;
    }
    const int first = key.line;
    const int last = last_line(view->children[i], t);
    size_t from = t.line_start(first);
    const size_t to = last < static_cast<int>(t.line_count()) ? t.line_start(last + 1) : source.size();
    if (to == source.size() && first > 1 && source.back() != '\n') {
      from = t.line_end(first - 1);
    }
    out.erase(from, to - from);
  }
  return out;
}

// ── Ids and references ───────────────────────────────────────────────────────

std::string fresh_id(std::string_view base, const std::set<std::string>& used) {
  std::string stem(base);
  while (!stem.empty() && stem.back() >= '0' && stem.back() <= '9') {
    stem.pop_back();
  }
  if (stem.empty()) {
    stem = "id";
  }
  for (int n = 1;; ++n) {
    std::string candidate = stem + std::to_string(n);
    if (!used.contains(candidate)) {
      return candidate;
    }
  }
}

namespace {

void collect_ids(const node& n, std::set<std::string>& out) {
  const int id = n.cls != nullptr ? n.cls->field_index("id") : -1;
  if (id >= 0) {
    if (const auto* text = std::get_if<std::string>(&n.values[static_cast<size_t>(id)]); text != nullptr && !text->empty()) {
      out.insert(*text);
    }
  }
  for (const node& child : n.children) {
    collect_ids(child, out);
  }
}

} // namespace

std::set<std::string> ids_in(const node& root) {
  std::set<std::string> out;
  collect_ids(root, out);
  return out;
}

bool path_before(std::string_view a, std::string_view b) {
  const std::vector<std::string_view> x = segments(a);
  const std::vector<std::string_view> y = segments(b);
  for (size_t i = 0; i < x.size() && i < y.size(); ++i) {
    if (x[i] == y[i]) {
      continue;
    }
    size_t m = 0;
    size_t n = 0;
    const bool x_number = std::from_chars(x[i].data(), x[i].data() + x[i].size(), m).ec == std::errc();
    const bool y_number = std::from_chars(y[i].data(), y[i].data() + y[i].size(), n).ec == std::errc();
    if (x_number && y_number) {
      return m < n;
    }
    return x[i] < y[i];
  }
  return x.size() < y.size();
}

std::vector<std::string> find_references(const node& root, std::string_view path) {
  const auto paths = element_paths(root);
  const auto target = paths.find(path);
  if (target == paths.end()) {
    return {};
  }
  const std::set<std::string> ids = ids_in(*target->second);
  if (ids.empty()) {
    return {};
  }
  // The diagram: the nearest ancestor that is a figure.
  std::string scope = "root";
  for (std::string up(path); up != "root";) {
    up = split_child(up).first;
    const auto it = paths.find(up);
    if (it != paths.end() && it->second->cls->fills_slot(kFigureSlot)) {
      scope = up;
      break;
    }
  }
  std::vector<std::string> out;
  for (const auto& [p, n] : paths) {
    if (p == scope || !is_path_or_below(p, scope) || is_path_or_below(p, path)) {
      continue;
    }
    bool refers = false;
    for (size_t i = 0; i < n->values.size() && !refers; ++i) {
      if (n->cls->fields()[i].name == "id") {
        continue;
      }
      if (const auto* text = std::get_if<std::string>(&n->values[i])) {
        refers = ids.contains(*text);
      } else if (const auto* texts = std::get_if<std::vector<std::string>>(&n->values[i])) {
        refers = std::any_of(texts->begin(), texts->end(), [&](const std::string& s) { return ids.contains(s); });
      }
    }
    if (refers) {
      out.push_back(p);
    }
  }
  std::sort(out.begin(), out.end(), [](const std::string& a, const std::string& b) { return path_before(a, b); });
  return out;
}

} // namespace bdg::nymph
