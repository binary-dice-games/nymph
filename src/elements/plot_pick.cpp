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

namespace {

/// ISO 8601 date (and time when not midnight) of a UTC time in seconds.
std::string iso_time(double seconds) {
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

} // namespace

std::optional<ImVec2> plot_pointer(const draw_context& dc, const char* label) {
  if (!dc.interactive() || !ImPlot::IsPlotHovered()) {
    return std::nullopt;
  }
  if (const ImPlotItem* item = ImPlot::GetItem(label); item != nullptr && !item->Show) {
    return std::nullopt; // hidden from the legend
  }
  return ImGui::GetIO().MousePos;
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
    return iso_time(value);
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

void finish_plot_picks(draw_context& dc) {
  std::optional<pick_candidate> c = dc.take_pick_candidate();
  if (!c) {
    return;
  }
  ImDrawList* dl = ImPlot::GetPlotDrawList();
  ImPlot::PushPlotClipRect();
  dl->AddCircle(c->anchor, 6.0f, c->color, 0, 2.0f);
  ImPlot::PopPlotClipRect();
  if (dc.tooltips()) {
    ImGui::BeginTooltip();
    if (!c->hit.label.empty()) {
      ImGui::TextUnformatted(c->hit.label.c_str());
    }
    for (const auto& [name, value] : c->hit.values) {
      ImGui::Text("%s: %s", name.c_str(), value.c_str());
    }
    ImGui::EndTooltip();
  }
  dc.set_hovered(std::move(c->hit));
}

} // namespace bdg::nymph
