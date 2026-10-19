// MIT License © 2026 Binary Dice Games
/// @file draw_context.cpp
/// @brief draw_context: element drawing helpers.
#include "render/draw_context.hpp"
#include <implot.h>
#include <imuml.h>
#include "render/text_style.hpp"
#include "schema/element_class.hpp"

namespace bdg::nymph {

void draw_context::draw_children(const node& n) {
  // Without an ImGui context (unit tests of drawing order) there is no ID
  // stack to push to.
  const bool imgui = ImGui::GetCurrentContext() != nullptr;
  for (size_t i = 0; i < n.children.size(); ++i) {
    if (imgui) {
      ImGui::PushID(static_cast<int>(i));
    }
    draw_node(n.children[i]);
    if (imgui) {
      ImGui::PopID();
    }
  }
}

void draw_context::record_current_plot(const node& n, bool with_y2, view_kind kind) {
  if (!interactive_) {
    return;
  }
  plot_view_record v;
  v.element = &n;
  v.kind = kind;
  const ImVec2 pos = ImPlot::GetPlotPos();
  const ImVec2 size = ImPlot::GetPlotSize();
  v.area = {pos, ImVec2(pos.x + size.x, pos.y + size.y)};
  const ImPlotRect r = ImPlot::GetPlotLimits(ImAxis_X1, ImAxis_Y1);
  v.x_min = r.X.Min;
  v.x_max = r.X.Max;
  v.y_min = r.Y.Min;
  v.y_max = r.Y.Max;
  if (with_y2) {
    const ImPlotRect r2 = ImPlot::GetPlotLimits(ImAxis_X1, ImAxis_Y2);
    v.has_y2 = true;
    v.y2_min = r2.Y.Min;
    v.y2_max = r2.Y.Max;
  }
  v.hidden = pending_hidden_;
  views_.push_back(v);
}

void draw_context::draw_node(const node& n) {
  const text_style_scope style(text_style_of(n), *this);
  if (edit_mode_ && n.cls->placed_as(n) == kFigureSlot) {
    record_target(n, {}, rect_of(n)); // before its children, so they are on top
  }
  item_range range;
  const bool in_diagram = edit_mode_ && ImUml::GetCurrentContext() != nullptr &&
      ImUml::GetItemCounts(&range.first[0], &range.first[1], &range.first[2]);
  n.cls->draw(n, *this);
  if (in_diagram && ImUml::GetItemCounts(&range.last[0], &range.last[1], &range.last[2])) {
    range.element = &n;
    item_ranges_.push_back(range);
  }
}

ImVec2 draw_context::measure_node(const node& n, float width) {
  const text_style_scope style(text_style_of(n), *this);
  return n.cls->measure(n, *this, width);
}

void draw_context::arrange_node(const node& n, const rect& r) {
  const text_style_scope style(text_style_of(n), *this);
  n.cls->arrange(n, *this, r);
}

} // namespace bdg::nymph
