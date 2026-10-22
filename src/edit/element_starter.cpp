// MIT License © 2026 Binary Dice Games
/// @file element_starter.cpp
/// @brief Starting fields of new elements (see element_starter.hpp). One
///        table rather than a virtual on every class: what a new element
///        starts with is the editor's concern, and test_starters checks that
///        every registered type binds in every parent that takes it.
#include "edit/element_starter.hpp"
#include <algorithm>
#include <cctype>
#include <map>
#include "edit/tree_edit.hpp"

namespace bdg::nymph {

namespace {

constexpr std::string_view kLink = "from: ?, to: ?";
constexpr std::string_view kSeries = "ys: [1, 3, 2]";
constexpr std::string_view k3dLine = "xs: [0, 1, 2], ys: [0, 1, 0], zs: [0, 1, 2]";

const std::map<std::string_view, std::string_view, std::less<>>& table() {
  static const std::map<std::string_view, std::string_view, std::less<>> t = {
      // Links name two nodes of their diagram.
      {"ClassRelation", kLink},
      {"ComponentLink", kLink},
      {"DataFlow", kLink},
      {"DeploymentLink", kLink},
      {"Flow", kLink},
      {"FlowLink", kLink},
      {"Link", kLink},
      {"Message", kLink},
      {"MindLink", kLink},
      {"ObjectLink", kLink},
      {"OrgLink", kLink},
      {"PackageRelation", kLink},
      {"Relationship", kLink},
      {"Transition", kLink},
      {"UseCaseRelation", kLink},
      {"WindowTransition", kLink},
      {"SankeyFlow", "from: A, to: B, value: 1"},
      // Diagrams that need a first item.
      {"GanttChart", "children: [{type: GanttTask, name: Task, start: 0, duration: 1}]"},
      {"GanttSection", "name: Section"},
      {"GanttTask", "name: Task, start: 0, duration: 1"},
      {"SankeyDiagram", "children: [{type: SankeyFlow, from: A, to: B, value: 1}]"},
      {"MindMap", "children: [{type: MindNode, text: Idea}]"},
      {"OrgChart", "children: [{type: Person, name: Name}]"},
      {"TreeDiagram", "children: [{type: TreeItem, text: Item}]"},
      {"Note", "text: Note"},
      {"SequenceDiagram/Note", "text: Note, over: [?]"},
      // Plot series need data.
      {"PlotBars", kSeries},
      {"PlotBarsH", kSeries},
      {"PlotDigital", "ys: [0, 1, 0]"},
      {"PlotLine", kSeries},
      {"PlotScatter", kSeries},
      {"PlotShaded", kSeries},
      {"PlotStairs", kSeries},
      {"PlotStems", kSeries},
      {"PlotErrorBars", "ys: [1, 3, 2], err: [0.5, 0.5, 0.5]"},
      {"PlotHistogram", "values: [1, 2, 2, 3, 3, 3]"},
      {"PlotHistogram2D", "xs: [1, 2, 3], ys: [1, 3, 2]"},
      {"PlotInfLines", "values: [1]"},
      {"PlotPieChart", "values: [1, 2, 3], labels: \"a\\nb\\nc\""},
      {"PlotBarGroups", "labels: [a, b], values: [1, 2, 3, 4]"},
      {"PlotHeatmap", "values: [1]"},
      {"PlotText", "text: Text"},
      {"Plot3DLine", k3dLine},
      {"Plot3DScatter", k3dLine},
      {"Plot3DMesh", "xs: [0, 1, 0], ys: [0, 0, 1], zs: [0, 0, 0], indices: [0, 1, 2]"},
      {"Plot3DTriangle", "xs: [0, 1, 0], ys: [0, 0, 1], zs: [0, 0, 0]"},
      {"Plot3DQuad", "xs: [0, 1, 1, 0], ys: [0, 0, 1, 1], zs: [0, 0, 0, 0]"},
      {"Plot3DSurface", "xs: [0, 1, 0, 1], ys: [0, 0, 1, 1], zs: [0, 1, 1, 0]"},
      {"Plot3DText", "text: Text"},
      // Widgets holding several numbers.
      {"DragFloat2", "value: [0, 0]"},
      {"DragFloat3", "value: [0, 0, 0]"},
      {"DragFloat4", "value: [0, 0, 0, 0]"},
      {"DragInt2", "value: [0, 0]"},
      {"DragInt3", "value: [0, 0, 0]"},
      {"DragInt4", "value: [0, 0, 0, 0]"},
      {"DragFloatRange2", "value: [0, 1]"},
      {"DragIntRange2", "value: [0, 10]"},
      {"InputFloat2", "value: [0, 0]"},
      {"InputFloat3", "value: [0, 0, 0]"},
      {"InputFloat4", "value: [0, 0, 0, 0]"},
      {"InputInt2", "value: [0, 0]"},
      {"InputInt3", "value: [0, 0, 0]"},
      {"InputInt4", "value: [0, 0, 0, 0]"},
      {"SliderFloat2", "value: [0, 0]"},
      {"SliderFloat3", "value: [0, 0, 0]"},
      {"SliderFloat4", "value: [0, 0, 0, 0]"},
      {"SliderInt2", "value: [0, 0]"},
      {"SliderInt3", "value: [0, 0, 0]"},
      {"SliderInt4", "value: [0, 0, 0, 0]"},
  };
  return t;
}

} // namespace

std::string_view starter_fields(std::string_view type, std::string_view diagram) {
  if (!diagram.empty()) {
    const auto it = table().find(std::string(diagram) + "/" + std::string(type));
    if (it != table().end()) {
      return it->second;
    }
  }
  const auto it = table().find(type);
  return it == table().end() ? std::string_view() : it->second;
}

std::string id_base(std::string_view type) {
  std::string lower;
  for (const char c : type) {
    lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return lower.find("node") != std::string::npos ? std::string("node") : lower;
}

new_element make_element(
    const element_class& cls,
    const std::set<std::string>& used_ids,
    const std::vector<std::string>& candidate_ids,
    std::string_view diagram) {
  new_element e;
  e.yaml = "{type: " + std::string(cls.name());
  if (cls.field_index("id") >= 0) {
    e.id = fresh_id(id_base(cls.name()), used_ids);
    e.yaml += ", id: " + e.id;
  }
  const std::string_view fields = starter_fields(cls.name(), diagram);
  if (!fields.empty()) {
    // Each `?` takes the next candidate id; the key it belongs to is the
    // last one before it.
    std::string filled;
    size_t next = 0;
    for (size_t i = 0; i < fields.size(); ++i) {
      if (fields[i] != '?') {
        filled += fields[i];
        continue;
      }
      if (!candidate_ids.empty()) {
        filled += candidate_ids[std::min(next++, candidate_ids.size() - 1)];
        continue;
      }
      filled += "\"\""; // parses; the element binds once the field is given
      const size_t colon = fields.rfind(':', i);
      size_t key_start = colon;
      while (key_start > 0 && fields[key_start - 1] != ' ' && fields[key_start - 1] != '{') {
        --key_start;
      }
      const std::string key(fields.substr(key_start, colon - key_start));
      if (std::find(e.unfilled.begin(), e.unfilled.end(), key) == e.unfilled.end()) {
        e.unfilled.push_back(key);
      }
    }
    e.yaml += ", " + filled;
  }
  e.yaml += "}";
  return e;
}

} // namespace bdg::nymph
