// MIT License © 2026 Binary Dice Games
/// @file plot_pick.cpp
/// @brief Picks inside an open ImPlot plot.
#include "elements/plot_pick.hpp"
#include <implot3d_internal.h> // forget_plot_items(): the 3D plots' items
#include <implot_internal.h> // the current plot's axes, ticks and items
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <variant>
#include <vector>
#include "core/dates.hpp"
#include "schema/element_class.hpp"

namespace bdg::nymph {

std::string format_iso_time(double seconds) {
  const double day_seconds = 86400.0;
  const auto days = static_cast<int64_t>(std::floor(seconds / day_seconds));
  const civil_date d = civil_from_days(days);
  const auto in_day = static_cast<int64_t>(std::llround(seconds - static_cast<double>(days) * day_seconds));
  char buf[64];
  if (in_day == 0) {
    std::snprintf(buf, sizeof(buf), "%04lld-%02u-%02u", static_cast<long long>(d.year), d.month, d.day);
  } else {
    std::snprintf(
        buf,
        sizeof(buf),
        "%04lld-%02u-%02u %02lld:%02lld:%02lld",
        static_cast<long long>(d.year),
        d.month,
        d.day,
        static_cast<long long>(in_day / 3600),
        static_cast<long long>(in_day / 60 % 60),
        static_cast<long long>(in_day % 60));
  }
  return buf;
}

std::optional<ImVec2> plot_pointer(const draw_context& dc, const char* label) {
  if (!dc.interactive() || !ImPlot::IsPlotHovered()) {
    return std::nullopt;
  }
  if (label != nullptr) {
    if (const ImPlotItem* item = ImPlot::GetItem(label); item != nullptr && !item->Show) {
      return std::nullopt; // hidden from the legend
    }
  }
  return ImGui::GetIO().MousePos;
}

std::optional<ImPlotPoint> plot_pointer_value(const draw_context& dc, const char* label) {
  const std::optional<ImVec2> p = plot_pointer(dc, label);
  if (!p) {
    return std::nullopt;
  }
  return ImPlot::PixelsToPlot(*p);
}

std::string format_axis_value(ImAxis axis_id, double value) {
  ImPlotPlot* plot = ImPlot::GetCurrentPlot();
  if (plot == nullptr) {
    return {};
  }
  ImPlotAxis& axis = plot->Axes[axis_id];
  if (axis.ShowDefaultTicks == false) {
    for (const ImPlotTick& t : axis.Ticker.Ticks) {
      if (t.ShowLabel && t.PlotPos == value) {
        return axis.Ticker.GetText(t.Idx);
      }
    }
  }
  if (axis.Scale == ImPlotScale_Time) {
    return format_iso_time(value);
  }
  char buf[IMPLOT_LABEL_MAX_SIZE];
  if (axis.Formatter != nullptr) {
    axis.Formatter(value, buf, sizeof(buf), axis.FormatterData);
  } else {
    std::snprintf(buf, sizeof(buf), "%g", value);
  }
  return buf;
}

std::string display_label(const char* label) {
  const std::string s = label == nullptr ? std::string() : std::string(label);
  const size_t hash = s.find("##");
  return hash == std::string::npos ? s : s.substr(0, hash);
}

void offer_nearest_point(
    draw_context& dc,
    const node& n,
    const char* label,
    const double* xs,
    const double* ys,
    int count,
    const pick_extra& extra,
    float priority) {
  const std::optional<ImVec2> pointer = plot_pointer(dc, label);
  if (!pointer || count <= 0) {
    return;
  }
  int best = -1;
  float best_d2 = kPickRadius * kPickRadius;
  ImVec2 best_px;
  for (int i = 0; i < count; ++i) {
    if (!std::isfinite(xs[i]) || !std::isfinite(ys[i])) {
      continue;
    }
    const ImVec2 px = ImPlot::PlotToPixels(xs[i], ys[i]);
    const float dx = px.x - pointer->x, dy = px.y - pointer->y;
    const float d2 = dx * dx + dy * dy;
    if (d2 <= best_d2) {
      best = i;
      best_d2 = d2;
      best_px = px;
    }
  }
  if (best < 0) {
    return;
  }
  const ImPlotPlot* plot = ImPlot::GetCurrentPlot();
  pick_candidate c;
  c.distance = std::max(0.0f, std::sqrt(best_d2) - priority);
  c.anchor = best_px;
  c.color = ImGui::ColorConvertFloat4ToU32(ImPlot::GetLastItemColor());
  c.hit.element = &n;
  c.hit.type = std::string(n.cls->name());
  c.hit.label = display_label(label);
  c.hit.index = best;
  c.hit.values = {
      {"x", format_axis_value(plot->CurrentX, xs[best])}, {"y", format_axis_value(plot->CurrentY, ys[best])}};
  if (extra) {
    extra(best, c.hit);
  }
  dc.offer_pick(std::move(c));
}

void offer_rect_px(draw_context& dc, const ImVec2& pointer, pick hit, ImVec2 a, ImVec2 b, ImU32 color) {
  const ImVec2 lo(std::min(a.x, b.x), std::min(a.y, b.y));
  const ImVec2 hi(std::max(a.x, b.x), std::max(a.y, b.y));
  if (pointer.x < lo.x || pointer.x > hi.x || pointer.y < lo.y || pointer.y > hi.y) {
    return;
  }
  pick_candidate c;
  c.distance = 0.0f;
  c.hit = std::move(hit);
  c.anchor = ImVec2((lo.x + hi.x) * 0.5f, (lo.y + hi.y) * 0.5f);
  c.color = color;
  c.outline = true;
  c.outline_min = lo;
  c.outline_max = hi;
  dc.offer_pick(std::move(c));
}

void offer_bars(
    draw_context& dc,
    const node& n,
    const char* label,
    int count,
    double size,
    bool horizontal,
    const std::function<bar_shape(int)>& shape,
    const pick_extra& values,
    ImU32 color) {
  const std::optional<ImVec2> pointer = plot_pointer(dc, label);
  if (!pointer) {
    return;
  }
  if (color == 0) {
    color = ImGui::ColorConvertFloat4ToU32(ImPlot::GetLastItemColor());
  }
  // Later bars are drawn over earlier ones: the last hit wins.
  for (int i = count - 1; i >= 0; --i) {
    const bar_shape b = shape(i);
    if (!std::isfinite(b.pos) || !std::isfinite(b.lo) || !std::isfinite(b.hi)) {
      continue;
    }
    const double half = size * 0.5;
    const ImVec2 a = horizontal ? ImPlot::PlotToPixels(b.lo, b.pos - half) : ImPlot::PlotToPixels(b.pos - half, b.lo);
    const ImVec2 z = horizontal ? ImPlot::PlotToPixels(b.hi, b.pos + half) : ImPlot::PlotToPixels(b.pos + half, b.hi);
    const ImVec2 lo(std::min(a.x, z.x), std::min(a.y, z.y));
    const ImVec2 hi(std::max(a.x, z.x), std::max(a.y, z.y));
    if (pointer->x >= lo.x && pointer->x <= hi.x && pointer->y >= lo.y && pointer->y <= hi.y) {
      pick hit;
      hit.element = &n;
      hit.type = std::string(n.cls->name());
      hit.label = display_label(label);
      hit.index = i;
      if (values) {
        values(i, hit);
      }
      offer_rect_px(dc, *pointer, std::move(hit), lo, hi, color);
      return;
    }
  }
}

namespace {

/// The highlight and tooltip of `c`, on `dl` (clipping already set).
void draw_pick(const draw_context& dc, const pick_candidate& c, ImDrawList* dl) {
  if (c.outline) {
    dl->AddRect(c.outline_min, c.outline_max, c.color, 0.0f, ImDrawFlags_None, 2.0f);
  } else {
    dl->AddCircle(c.anchor, 6.0f, c.color, 0, 2.0f);
  }
  if (dc.tooltips()) {
    ImGui::BeginTooltip();
    if (!c.hit.label.empty()) {
      ImGui::TextUnformatted(c.hit.label.c_str());
    }
    for (const auto& [name, value] : c.hit.values) {
      ImGui::Text("%s: %s", name.c_str(), value.c_str());
    }
    ImGui::EndTooltip();
  }
}

} // namespace

void sync_hidden(draw_context& dc, const char* label) {
  ImPlotItem* item = label == nullptr ? nullptr : ImPlot::GetItem(label);
  if (item == nullptr) {
    return;
  }
  if (dc.take_rehidden(item->ID)) {
    item->Show = false;
  }
  if (const node* plot = dc.current_plot(); plot != nullptr && dc.apply_view()) {
    if (const element_view* v = dc.view_of(*plot);
        v != nullptr && std::find(v->hidden.begin(), v->hidden.end(), label) != v->hidden.end()) {
      item->Show = false;
    }
  }
  if (!item->Show) {
    dc.record_hidden(label);
  }
}

std::vector<ImGuiID> forget_plot_items() {
  std::vector<ImGuiID> hidden;
  if (ImPlotContext* gp = ImPlot::GetCurrentContext(); gp != nullptr) {
    for (int i = 0; i < gp->Plots.GetBufSize(); ++i) {
      ImPlotItemGroup& items = gp->Plots.GetByIndex(i)->Items;
      for (int k = 0; k < items.GetItemCount(); ++k) {
        if (const ImPlotItem* item = items.GetItemByIndex(k); !item->Show) {
          hidden.push_back(item->ID);
        }
      }
      items.Reset();
      items.MarkerIdx = 0;
    }
  }
  if (ImPlot3DContext* gp = ImPlot3D::GetCurrentContext(); gp != nullptr) {
    for (int i = 0; i < gp->Plots.GetBufSize(); ++i) {
      ImPlot3DItemGroup& items = gp->Plots.GetByIndex(i)->Items;
      for (int k = 0; k < items.GetItemCount(); ++k) {
        if (const ImPlot3DItem* item = items.GetItemByIndex(k); !item->Show) {
          hidden.push_back(item->ID);
        }
      }
      items.Reset();
    }
  }
  return hidden;
}

void finish_picks(draw_context& dc, ImDrawList* dl) {
  std::optional<pick_candidate> c = dc.take_pick_candidate();
  if (!c) {
    return;
  }
  draw_pick(dc, *c, dl);
  dc.set_hovered(std::move(c->hit));
}

void finish_plot_picks(draw_context& dc) {
  std::optional<pick_candidate> c = dc.take_pick_candidate();
  if (!c) {
    return;
  }
  ImPlot::PushPlotClipRect();
  draw_pick(dc, *c, ImPlot::GetPlotDrawList());
  ImPlot::PopPlotClipRect();
  dc.set_hovered(std::move(c->hit));
}

namespace {

/// The series of `plot` that legend entry `shown` belongs to: the one with
/// that `label`, else one whose `labels` (pie slices, one per line) holds
/// it; nullptr for none.
const node* legend_owner(const node& plot, const std::string& shown, bool children) {
  if (!children) {
    return nullptr;
  }
  for (const node& c : plot.children) {
    const int label = c.cls->field_index("label");
    if (label >= 0) {
      const auto* text = std::get_if<std::string>(&c.values[static_cast<size_t>(label)]);
      if (text != nullptr && display_label(text->c_str()) == shown) {
        return &c;
      }
    }
  }
  for (const node& c : plot.children) {
    const int labels = c.cls->field_index("labels");
    if (labels < 0) {
      continue;
    }
    const value& v = c.values[static_cast<size_t>(labels)];
    if (const auto* list = std::get_if<std::vector<std::string>>(&v)) {
      if (std::find(list->begin(), list->end(), shown) != list->end()) {
        return &c;
      }
    } else if (const auto* text = std::get_if<std::string>(&v)) {
      size_t start = 0;
      while (start <= text->size()) {
        const size_t end = std::min(text->find('\n', start), text->size());
        if (text->compare(start, end - start, shown) == 0) {
          return &c;
        }
        start = end + 1;
      }
    }
  }
  return nullptr;
}

} // namespace

void record_plot_texts(draw_context& dc, const plot_texts& texts, const ImPlotPlot* plot) {
  if (!dc.edit_mode() || plot == nullptr || texts.element == nullptr) {
    return;
  }
  const node& n = *texts.element;
  // The axes' hover areas reach out over the plot padding; labels sit inside it.
  const ImVec2 pad = ImPlot::GetStyle().PlotPadding;
  const auto text_size = [](const std::string& t) {
    return ImGui::CalcTextSize(t.c_str(), ImGui::FindRenderedTextEnd(t.c_str()), true);
  };
  // The title is centred over the plot area at the top of the canvas.
  if (!texts.title.empty() && texts.title.compare(0, 2, "##") != 0) {
    const ImVec2 size = text_size(texts.title);
    const float x = plot->PlotRect.GetCenter().x - size.x * 0.5f;
    dc.record_target(
        n, "title", {ImVec2(x, plot->CanvasRect.Min.y), ImVec2(x + size.x, plot->CanvasRect.Min.y + size.y)}, texts.title);
  }
  // X label: centred under the plot area, at the bottom of the axis.
  if (!texts.x_label.empty()) {
    const ImPlotAxis& axis = plot->Axes[ImAxis_X1];
    const ImVec2 size = text_size(texts.x_label);
    const float x = plot->PlotRect.GetCenter().x - size.x * 0.5f;
    dc.record_target(
        n, "x_label", {ImVec2(x, axis.HoverRect.Max.y - pad.y - size.y), ImVec2(x + size.x, axis.HoverRect.Max.y - pad.y)}, texts.x_label);
  }
  // Y labels are turned a quarter round: as wide as the text is high.
  const auto y_label = [&](ImAxis id, const std::string& label, const char* field, bool left) {
    const ImPlotAxis& axis = plot->Axes[id];
    const ImVec2 size = text_size(label);
    const float cy = plot->PlotRect.GetCenter().y;
    const float x = left ? axis.HoverRect.Min.x + pad.x : axis.HoverRect.Max.x - pad.x - size.y;
    dc.record_target(n, field, {ImVec2(x, cy - size.x * 0.5f), ImVec2(x + size.y, cy + size.x * 0.5f)}, label);
  };
  if (!texts.y_label.empty()) {
    y_label(ImAxis_Y1, texts.y_label, "y_label", true);
  }
  if (texts.has_y2 && !texts.y2_label.empty()) {
    y_label(ImAxis_Y2, texts.y2_label, "y2_label", false);
  }
  // The legend, when it is drawn.
  const ImRect& legend = plot->Items.Legend.Rect;
  if (!texts.legend_field.empty() && plot->Items.GetLegendCount() > 0 && (plot->Flags & ImPlotFlags_NoLegend) == 0 && legend.GetWidth() > 0.0f) {
    dc.record_target(n, texts.legend_field, {legend.Min, legend.Max}, "legend");
    // Each entry selects the series it names (a pie chart: one of its slices).
    // ImPlot leaves the entries' own rectangles empty: lay them out as its
    // legend does (rows of one text line, or one row when horizontal).
    ImPlotItemGroup& items = const_cast<ImPlotItemGroup&>(plot->Items); // its getters are not const
    const ImPlotStyle& style = ImPlot::GetStyle();
    const float line = ImGui::GetTextLineHeight();
    const bool horizontal = (items.Legend.Flags & ImPlotLegendFlags_Horizontal) != 0;
    ImVec2 at(legend.Min.x + style.LegendInnerPadding.x, legend.Min.y + style.LegendInnerPadding.y);
    std::vector<int> order(static_cast<size_t>(items.GetLegendCount()));
    for (size_t i = 0; i < order.size(); ++i) {
      order[i] = static_cast<int>(i);
    }
    if ((items.Legend.Flags & ImPlotLegendFlags_Sort) != 0) {
      std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
        return std::strcmp(items.GetLegendLabel(a), items.GetLegendLabel(b)) < 0;
      });
    }
    for (const int i : order) {
      const char* label = items.GetLegendLabel(i);
      const std::string shown = display_label(label);
      const float width = line + style.LegendInnerPadding.x + text_size(shown).x;
      const ImVec2 lo = at;
      const ImVec2 hi(horizontal ? at.x + width : legend.Max.x - style.LegendInnerPadding.x, at.y + line);
      if (horizontal) {
        at.x += width + style.LegendSpacing.x;
      } else {
        at.y += line + style.LegendSpacing.y;
      }
      if (shown.empty()) {
        continue;
      }
      if (const node* child = legend_owner(n, shown, texts.element_children)) {
        const bool by_label = child->cls->field_index("label") >= 0;
        dc.record_target(*child, by_label ? "label" : "", {lo, hi}, shown);
      }
    }
  }
}

} // namespace bdg::nymph
