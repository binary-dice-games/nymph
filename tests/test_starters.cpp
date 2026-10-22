// MIT License © 2026 Binary Dice Games
/// @file test_starters.cpp
/// @brief spec/14_editor_workspace plan step 3: a new element of every
///        registered type binds in every parent that takes it.
#include <gtest/gtest.h>
#include <string>
#include "document/bind.hpp"
#include "edit/element_starter.hpp"
#include "edit/tree_edit.hpp"
#include "elements/register_builtin.hpp"
#include "schema/element_class.hpp"
#include "schema/registry.hpp"

namespace bdg::nymph {
namespace {

bool takes(const element_class& parent, const element_class& child) {
  return parent.placement_of(child) >= 0;
}

std::set<std::string> ids_of(const std::string& source) {
  return ids_in(bind_source(source, builtin_registry()).root);
}

/// The type of the source's root.
std::string diagram_of(const std::string& source) {
  return std::string(bind_source(source, builtin_registry()).root.cls->name());
}

/// A valid source with an element of `parent` at the returned path, or
/// nullopt when no figure takes it directly.
std::optional<std::pair<std::string, std::string>> host_for(const element_class& parent) {
  const registry& reg = builtin_registry();
  if (parent.fills_slot(kFigureSlot)) {
    return std::make_pair("root: " + make_element(parent, {}, {}).yaml + "\n", std::string("root"));
  }
  for (const element_class* g : reg.classes()) {
    if (!g->fills_slot(kFigureSlot) || !takes(*g, parent)) {
      continue;
    }
    // Directly in the figure, or else in one of the children its starter
    // made (a MindMap takes one root MindNode; more go under it).
    const std::string root = "root: " + make_element(*g, {}, {}).yaml + "\n";
    std::vector<std::string> places{"root"};
    const figure f = bind_source(root, reg);
    for (size_t i = 0; i < f.root.children.size(); ++i) {
      if (takes(*f.root.children[i].cls, parent)) {
        places.push_back("root/children/" + std::to_string(i));
      }
    }
    for (const std::string& place : places) {
      const tree_edit_result r = insert_child(root, place, kAppend, make_element(parent, ids_of(root), {}).yaml);
      try {
        (void)bind_source(r.source, reg);
        return std::make_pair(r.source, r.path);
      } catch (const std::exception&) {
      }
    }
  }
  return std::nullopt;
}

/// The loader's message for `source`, or "" when it binds.
std::string problem(const std::string& source) {
  try {
    (void)bind_source(source, builtin_registry());
    return {};
  } catch (const std::exception& e) {
    return e.what();
  }
}

/// The sources made by adding two nodes to `parent` (so links have ids to
/// name), one of each of two types it takes that have an id and no `?`,
/// every pair of such types; with the two ids.
std::vector<std::pair<std::string, std::vector<std::string>>>
with_nodes(const std::string& source, const std::string& parent_path, const element_class& parent) {
  std::vector<const element_class*> nodes;
  for (const element_class* c : builtin_registry().classes()) {
    if (takes(parent, *c) && c->field_index("id") >= 0 && starter_fields(c->name()).find('?') == std::string_view::npos) {
      nodes.push_back(c);
    }
  }
  std::vector<std::pair<std::string, std::vector<std::string>>> out;
  for (const element_class* a : nodes) {
    for (const element_class* b : nodes) {
      std::string s = source;
      std::vector<std::string> ids;
      for (const element_class* c : {a, b}) {
        if (!problem(s).empty()) {
          break;
        }
        const new_element e = make_element(*c, ids_of(s), {});
        s = insert_child(s, parent_path, kAppend, e.yaml).source;
        ids.push_back(e.id);
      }
      out.emplace_back(std::move(s), std::move(ids));
    }
  }
  return out;
}

TEST(starters, EveryTypeBindsInEveryParentThatTakesIt) {
  const registry& reg = builtin_registry();
  int checked = 0;
  for (const element_class* x : reg.classes()) {
    if (x->fills_slot(kFigureSlot)) {
      const std::string source = "root: " + make_element(*x, {}, {}).yaml + "\n";
      EXPECT_NO_THROW((void)bind_source(source, reg)) << source;
      ++checked;
    }
    for (const element_class* p : reg.classes()) {
      if (!takes(*p, *x)) {
        continue;
      }
      std::optional<std::pair<std::string, std::string>> host = host_for(*p);
      if (!host) {
        continue;
      }
      auto& [source, path] = *host;
      ASSERT_NO_THROW((void)bind_source(source, reg)) << p->name() << "\n" << source;
      // A link is tried with nodes of each type its parent takes: one must do.
      std::vector<std::pair<std::string, std::vector<std::string>>> tries;
      const std::string diagram = diagram_of(source);
      if (starter_fields(x->name(), diagram).find('?') != std::string_view::npos) {
        tries = with_nodes(source, path, *p);
      } else {
        tries.emplace_back(source, std::vector<std::string>{});
      }
      std::string message = "no node type to link";
      std::string result;
      for (const auto& [with, candidates] : tries) {
        if (const std::string bad = problem(with); !bad.empty()) {
          message = bad; // the nodes themselves cannot go here
          continue;
        }
        const new_element e = make_element(*x, ids_of(with), candidates, diagram_of(with));
        result = insert_child(with, path, kAppend, e.yaml).source;
        message = problem(result);
        if (message.empty()) {
          break;
        }
      }
      // Placement rules beyond slots are refused with the loader's message,
      // which the New element dialog shows.
      const bool placement = message.find("cannot go here") != std::string::npos ||
          message.find("must be a direct child") != std::string::npos ||
          message.find("this is a second one") != std::string::npos ||
          // A Sankey node is named by the flows: its id must be one of their
          // ends, which the New element dialog asks for.
          (std::string_view(x->name()) == "SankeyNode" && message.find("is in no flow") != std::string::npos);
      EXPECT_TRUE(message.empty() || placement) << x->name() << " in " << p->name() << ": " << message << "\n" << result;
      ++checked;
    }
  }
  EXPECT_GT(checked, 100);
}

TEST(starters, FillsLinkEndsFromTheCandidates) {
  const element_class* link = builtin_registry().find("FlowLink");
  ASSERT_NE(link, nullptr);
  EXPECT_EQ(make_element(*link, {}, {"a", "b"}).yaml, "{type: FlowLink, from: a, to: b}");
  EXPECT_EQ(make_element(*link, {}, {"a"}).yaml, "{type: FlowLink, from: a, to: a}");
  const new_element none = make_element(*link, {}, {});
  EXPECT_EQ(none.unfilled, (std::vector<std::string>{"from", "to"}));
  const element_class* note = builtin_registry().find("Note");
  ASSERT_NE(note, nullptr);
  EXPECT_EQ(make_element(*note, {}, {"alice"}, "SequenceDiagram").yaml, "{type: Note, text: Note, over: [alice]}");
  EXPECT_EQ(make_element(*note, {}, {}, "SequenceDiagram").unfilled, (std::vector<std::string>{"over"}));
  EXPECT_EQ(make_element(*note, {}, {"a"}, "Flowchart").yaml, "{type: Note, text: Note}");
}

TEST(starters, GivesFreshIds) {
  const element_class* node = builtin_registry().find("FlowNode");
  ASSERT_NE(node, nullptr);
  const new_element e = make_element(*node, {"node1"}, {});
  EXPECT_EQ(e.id, "node2");
  EXPECT_EQ(e.yaml, "{type: FlowNode, id: node2}");
  EXPECT_EQ(id_base("Participant"), "participant");
}

} // namespace
} // namespace bdg::nymph
