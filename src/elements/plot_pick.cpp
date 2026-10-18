// MIT License © 2026 Binary Dice Games
/// @file plot_pick.cpp
/// @brief Picks inside an open ImPlot plot.
#include "elements/plot_pick.hpp"
#include <implot_internal.h> // the current plot's axes, ticks and items
#include <algorithm>
#include <cmath>
#include <cstdio>
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

} // namespace bdg::nymph
