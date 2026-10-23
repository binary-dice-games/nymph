// MIT License © 2026 Binary Dice Games
/// @file mindmap.cpp
/// @brief mermaid `mindmap` → `MindMap` (spec/16_mermaid_import/DESIGN.md
///        section 4.8).
#include "core/limits.hpp"
#include "mermaid/mermaid_internal.hpp"

namespace bdg::nymph::mermaid {

namespace {

/// Leading white space, a tab counting as four spaces.
size_t indent_of(const std::string& text) {
  size_t n = 0;
  for (char c : text) {
    if (c == ' ') {
      ++n;
    } else if (c == '\t') {
      n += 4;
    } else {
      break;
    }
  }
  return n;
}

/// The text of a node: `id[t]`, `id(t)`, `id((t))`, `id))t((`, `id)t(`,
/// `id{{t}}`, or plain text.
std::string node_text(std::string_view text) {
  struct shape {
    std::string_view open, close;
  };
  static constexpr shape shapes[] = {{"((", "))"}, {"))", "(("}, {"{{", "}}"}, {"[", "]"}, {"(", ")"}, {")", "("}};
  for (const shape& s : shapes) {
    const size_t open = text.find(s.open);
    if (open == std::string_view::npos) {
      continue;
    }
    if (text.size() >= open + s.open.size() + s.close.size() &&
        text.substr(text.size() - s.close.size()) == s.close) {
      // The id before the shape must be an id (no spaces).
      const std::string_view id = text.substr(0, open);
      if (id.find(' ') != std::string_view::npos) {
        continue;
      }
      return label_text(text.substr(open + s.open.size(), text.size() - open - s.open.size() - s.close.size()));
    }
  }
  return label_text(text);
}

} // namespace

document convert_mindmap(const source_text& src, context& ctx) {
  element root("MindMap", src.header_pos);
  root.text("title", src.title);
  // Built as a tree of indices to keep pointers stable.
  struct item {
    std::string text;
    size_t indent;
    int parent;
    pos at;
  };
  std::vector<item> items;
  std::vector<size_t> stack; ///< open items, by indentation
  for (const line& l : src.body) {
    const pos at = l.start();
    const std::string_view text = trim(l.text);
    if (starts_with(text, "::icon(")) {
      ctx.warn(at, "icons are dropped");
      continue;
    }
    std::string_view node = text;
    const size_t colons = node.find(":::");
    if (colons != std::string_view::npos) {
      ctx.warn(at, "classes (:::) are dropped");
      node = trim(node.substr(0, colons));
    }
    const size_t indent = indent_of(l.text);
    while (!stack.empty() && items[stack.back()].indent >= indent) {
      stack.pop_back();
    }
    if (stack.empty() && !items.empty()) {
      ctx.fail(at, "a mind map has one root; indent this node under it");
    }
    if (stack.size() >= kMaxTreeDepth) {
      ctx.fail(at, "the mind map is deeper than " + std::to_string(kMaxTreeDepth) + " levels");
    }
    items.push_back({node_text(node), indent, stack.empty() ? -1 : static_cast<int>(stack.back()), at});
    stack.push_back(items.size() - 1);
  }
  if (items.empty()) {
    ctx.fail(src.header_pos, "the mind map has no nodes");
  }
  // Elements, deepest first (children always follow their parent).
  std::vector<element> built(items.size());
  std::vector<std::vector<size_t>> children(items.size());
  for (size_t i = 1; i < items.size(); ++i) {
    children[static_cast<size_t>(items[i].parent)].push_back(i);
  }
  for (size_t i = items.size(); i-- > 0;) {
    element e("MindNode", items[i].at);
    e.text("text", items[i].text);
    for (size_t c : children[i]) {
      e.children.push_back(std::move(built[c]));
    }
    built[i] = std::move(e);
  }
  root.children.push_back(std::move(built[0]));
  document doc;
  doc.root = root;
  return doc;
}

} // namespace bdg::nymph::mermaid
