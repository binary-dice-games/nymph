// MIT License © 2026 Binary Dice Games
/// @file flow.cpp
/// @brief WindowTransition: a labelled arrow between two windows of a Screen, the
///        UI flow (spec/09_ui_mockups/DESIGN.md section 3, "UI flow").
#include <imgui.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <memory>
#include <optional>
#include "elements/ui/ui_common.hpp"
#include "render/text_style.hpp"
#include "schema/registry.hpp"

namespace bdg::nymph::ui {

namespace {

/// Space between the labels of transitions drawn side by side, pixels.
constexpr float kLaneSpace = 8.0f;
/// Gap between an arrow's ends and the window borders, pixels.
constexpr float kEndGap = 3.0f;
/// How far a self-transition's loop reaches out of the window, pixels.
constexpr float kLoopReach = 44.0f;
/// Padding of the label panel, pixels.
constexpr ImVec2 kLabelPadding(4.0f, 1.0f);

namespace transition_field {
constexpr size_t from = 0, to = 1, label = 2, color = 3, thickness = 4, dashed = 5;
}

ImVec2 add(ImVec2 a, ImVec2 b) {
  return ImVec2(a.x + b.x, a.y + b.y);
}
ImVec2 sub(ImVec2 a, ImVec2 b) {
  return ImVec2(a.x - b.x, a.y - b.y);
}
ImVec2 mul(ImVec2 a, float k) {
  return ImVec2(a.x * k, a.y * k);
}

/// The parameter t in [0, 1] at which the segment from `s` (inside `r`)
/// towards `e` leaves `r`; 1 if it does not.
float exit_param(const rect& r, ImVec2 s, ImVec2 e) {
  const ImVec2 d = sub(e, s);
  float t = 1.0f;
  if (d.x > 0.0f) {
    t = std::min(t, (r.max.x - s.x) / d.x);
  } else if (d.x < 0.0f) {
    t = std::min(t, (r.min.x - s.x) / d.x);
  }
  if (d.y > 0.0f) {
    t = std::min(t, (r.max.y - s.y) / d.y);
  } else if (d.y < 0.0f) {
    t = std::min(t, (r.min.y - s.y) / d.y);
  }
  return std::max(0.0f, t);
}

/// Distance between transitions drawn side by side: a label's height plus
/// some space, so their labels do not touch.
float lane_gap() {
  return ImGui::GetFontSize() + 2.0f * kLabelPadding.y + kLaneSpace;
}

bool overlaps(const rect& a, const rect& b) {
  return a.min.x < b.max.x && b.min.x < a.max.x && a.min.y < b.max.y && b.min.y < a.max.y;
}

/// A rectangle of `size` centred on `c`.
rect centred(ImVec2 c, ImVec2 size) {
  return {sub(c, mul(size, 0.5f)), add(c, mul(size, 0.5f))};
}

/// Where along the line from `a` to `b` a label panel of `size` is centred:
/// the middle if the panel clears both windows there, else the point
/// nearest the middle where it does (in steps of 1/32 of the line), else
/// the middle.
ImVec2 label_centre(ImVec2 a, ImVec2 b, ImVec2 size, const rect& from, const rect& to) {
  constexpr int kSteps = 16;
  for (int i = 0; i <= kSteps; ++i) {
    for (const int sign : {1, -1}) {
      const float t = 0.5f + static_cast<float>(sign * i) / (2.0f * kSteps);
      const ImVec2 c = add(a, mul(sub(b, a), t));
      const rect panel = centred(c, size);
      if (!overlaps(panel, from) && !overlaps(panel, to)) {
        return c;
      }
      if (i == 0) {
        break;
      }
    }
  }
  return mul(add(a, b), 0.5f);
}

/// A straight line from `a` to `b`, solid or dashed.
void line(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 color, float thickness, bool dashed) {
  if (!dashed) {
    dl->AddLine(a, b, color, thickness);
    return;
  }
  const float len = std::hypot(b.x - a.x, b.y - a.y);
  if (len <= 0.0f) {
    return;
  }
  const float dash = 6.0f + 2.0f * thickness, gap = 4.0f + thickness;
  const ImVec2 d = mul(sub(b, a), 1.0f / len);
  for (float t = 0.0f; t < len; t += dash + gap) {
    dl->AddLine(add(a, mul(d, t)), add(a, mul(d, std::min(t + dash, len))), color, thickness);
  }
}

/// A filled arrowhead with its tip at `tip`, pointing along unit `dir`;
/// returns the middle of its base, where the line ends.
ImVec2 arrowhead(ImDrawList* dl, ImVec2 tip, ImVec2 dir, ImU32 color, float thickness) {
  const float length = 7.0f + 2.5f * thickness, half = 3.5f + 1.5f * thickness;
  const ImVec2 base = sub(tip, mul(dir, length));
  const ImVec2 side(-dir.y * half, dir.x * half);
  dl->AddTriangleFilled(tip, add(base, side), sub(base, side), color);
  return base;
}

class transition_class : public element_class {
 public:
  transition_class()
      : element_class(
            "WindowTransition",
            std::string(kFlowSlot),
            {},
            with_text_style({
                text_field("from", "", "The id of the Window the transition starts from."),
                text_field("to", "", "The id of the Window it leads to (the arrowhead); may equal from (a loop)."),
                text_field(
                    "label", "", "The event that triggers it, shown on the arrow: \"ok\", \"cancel\", \"finished loading\"."),
                color_field("color", "Colour of the arrow; empty is the theme's text colour at 60 % opacity."),
                number_field("thickness", 1.5, "Line width in pixels.", value_range{1, 8}),
                bool_field("dashed", false, "Draws the line dashed (an optional or error path, say)."),
            }),
            "A labelled arrow from one Window to another on the same Screen: how one interface leads to the next.") {}

  void draw(const node& n, draw_context& dc) const override {
    const ui_frame& frame = dc.ui().frame();
    const std::optional<rect> from = window_rect(dc, n.get<std::string>(transition_field::from));
    const std::optional<rect> to = window_rect(dc, n.get<std::string>(transition_field::to));
    if (!from || !to) {
      return; // a window closed in a live view
    }
    const std::string path = dc.path_of(n);
    const auto lane_it = frame.lanes.find(path);
    const auto [lane, lanes] = lane_it == frame.lanes.end() ? std::pair<int, int>{0, 1} : lane_it->second;

    ImU32 color = 0;
    const std::string& written_color = n.get<std::string>(transition_field::color);
    if (!written_color.empty()) {
      color = ImGui::ColorConvertFloat4ToU32(color_of(written_color));
    } else {
      ImVec4 text = ImGui::GetStyle().Colors[ImGuiCol_Text];
      text.w *= 0.6f;
      color = ImGui::ColorConvertFloat4ToU32(text);
    }
    const float thickness = static_cast<float>(n.get<double>(transition_field::thickness));
    const bool dashed = n.get<bool>(transition_field::dashed);

    const std::string& label = n.get<std::string>(transition_field::label);
    const ImVec2 label_size =
        label.empty() ? ImVec2(0.0f, 0.0f) : add(ImGui::CalcTextSize(label.c_str()), mul(kLabelPadding, 2.0f));
    const rect screen{frame.screen_min, frame.screen_max};

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRect(frame.screen_min, frame.screen_max, true);
    std::optional<rect> panel;
    if (n.get<std::string>(transition_field::from) == n.get<std::string>(transition_field::to)) {
      panel = draw_loop(dl, *from, screen, lane, label_size, color, thickness, dashed);
    } else {
      panel = draw_arrow(
          dl,
          *from,
          *to,
          lane,
          lanes,
          label_size,
          color,
          thickness,
          dashed,
          n.get<std::string>(transition_field::from) < n.get<std::string>(transition_field::to));
    }
    if (panel && !label.empty()) {
      draw_label(n, dc, dl, *panel, frame.screen_background);
    }
    dl->PopClipRect();
  }

 private:
  /// The screen rectangle of the window with id `id` this frame, if drawn.
  static std::optional<rect> window_rect(const draw_context& dc, const std::string& id) {
    const ui_frame& frame = dc.ui().frame();
    const auto path = frame.window_paths.find(id);
    if (path == frame.window_paths.end()) {
      return std::nullopt;
    }
    const ui_entry* e = dc.ui().find(path->second);
    if (e == nullptr || e->closed || e->size.x <= 0.0f || e->size.y <= 0.0f) {
      return std::nullopt;
    }
    const ImVec2 min = add(frame.screen_min, e->pos);
    return rect{min, add(min, e->size)};
  }

  /// A straight arrow between two windows, in lane `lane` of `lanes`;
  /// returns where its label panel of `label_size` goes, or nothing when
  /// the windows overlap. `forward` says whether from < to, so both
  /// directions share one perpendicular for their lanes.
  static std::optional<rect> draw_arrow(
      ImDrawList* dl,
      const rect& from,
      const rect& to,
      int lane,
      int lanes,
      ImVec2 label_size,
      ImU32 color,
      float thickness,
      bool dashed,
      bool forward) {
    const ImVec2 ca = mul(add(from.min, from.max), 0.5f);
    const ImVec2 cb = mul(add(to.min, to.max), 0.5f);
    const float len = std::hypot(cb.x - ca.x, cb.y - ca.y);
    if (len < 1.0f) {
      return std::nullopt;
    }
    // Lanes are offset along the perpendicular of the pair's own direction
    // (smaller id to larger), the same for both directions.
    ImVec2 dir = mul(sub(cb, ca), 1.0f / len);
    const ImVec2 pair_dir = forward ? dir : mul(dir, -1.0f);
    const ImVec2 perp(-pair_dir.y, pair_dir.x);
    const float offset = (static_cast<float>(lane) - static_cast<float>(lanes - 1) * 0.5f) * lane_gap();
    const ImVec2 s = add(ca, mul(perp, offset));
    const ImVec2 e = add(cb, mul(perp, offset));
    const float t_start = exit_param(from, s, e);
    const float t_end = exit_param(to, e, s); // measured from e towards s
    if (t_start + t_end >= 1.0f) {
      return std::nullopt;
    }
    const ImVec2 seg = sub(e, s);
    const ImVec2 start = add(add(s, mul(seg, t_start)), mul(dir, kEndGap));
    const ImVec2 tip = sub(sub(e, mul(seg, t_end)), mul(dir, kEndGap));
    if ((tip.x - start.x) * dir.x + (tip.y - start.y) * dir.y <= 0.0f) {
      return std::nullopt;
    }
    const ImVec2 base = arrowhead(dl, tip, dir, color, thickness);
    line(dl, start, base, color, thickness, dashed);
    return centred(label_centre(start, tip, label_size, from, to), label_size);
  }

  /// A loop out of one side of the window and back into it, lane `lane`
  /// further out, on the side with the most room inside the Screen (right,
  /// left, bottom, top, in that order on ties); returns where its label
  /// panel of `label_size` goes, beyond the loop.
  static rect draw_loop(
      ImDrawList* dl,
      const rect& r,
      const rect& screen,
      int lane,
      ImVec2 label_size,
      ImU32 color,
      float thickness,
      bool dashed) {
    struct side {
      ImVec2 out; ///< Unit normal, out of the window.
      float room;
    };
    const side sides[] = {
        {ImVec2(1.0f, 0.0f), screen.max.x - r.max.x - label_size.x},
        {ImVec2(-1.0f, 0.0f), r.min.x - screen.min.x - label_size.x},
        {ImVec2(0.0f, 1.0f), screen.max.y - r.max.y - label_size.y},
        {ImVec2(0.0f, -1.0f), r.min.y - screen.min.y - label_size.y},
    };
    const side* best = &sides[0];
    for (const side& s : sides) {
      if (s.room > best->room) {
        best = &s;
      }
    }
    const ImVec2 out = best->out;
    const ImVec2 along(-out.y, out.x);
    // A third of the way along the chosen edge: arrows from other windows
    // aim at the centre, so the edge's middle is where they arrive.
    const ImVec2 third(r.min.x + r.width() / 3.0f, r.min.y + r.height() / 3.0f);
    const ImVec2 edge(
        out.x > 0.0f ? r.max.x : out.x < 0.0f ? r.min.x : third.x,
        out.y > 0.0f ? r.max.y : out.y < 0.0f ? r.min.y : third.y);
    const float edge_length = out.x != 0.0f ? r.height() : r.width();
    const float spread = std::min(14.0f, edge_length * 0.25f);
    const float reach = kLoopReach + lane_gap() * 1.5f * static_cast<float>(lane);
    const ImVec2 foot = add(edge, mul(out, kEndGap));
    const ImVec2 a = sub(foot, mul(along, spread));
    const ImVec2 tip = add(foot, mul(along, spread));
    const ImVec2 c1 = add(a, sub(mul(out, reach), mul(along, reach * 0.6f)));
    const ImVec2 c2 = add(tip, add(mul(out, reach), mul(along, reach * 0.6f)));
    // The curve ends at the arrowhead's base; it points back into the window.
    const ImVec2 base = arrowhead(dl, tip, mul(out, -1.0f), color, thickness);
    if (dashed) {
      constexpr int kSegments = 24;
      ImVec2 prev = a;
      for (int i = 1; i <= kSegments; ++i) {
        const float t = static_cast<float>(i) / kSegments;
        const float u = 1.0f - t;
        const ImVec2 p = add(
            add(mul(a, u * u * u), mul(c1, 3.0f * u * u * t)), add(mul(c2, 3.0f * u * t * t), mul(base, t * t * t)));
        if (i % 2 == 1) {
          dl->AddLine(prev, p, color, thickness);
        }
        prev = p;
      }
    } else {
      dl->AddBezierCubic(a, c1, c2, base, color, thickness);
    }
    // The label just beyond the loop's furthest point (3/4 of the reach for
    // a cubic with these controls).
    const ImVec2 apex = add(foot, mul(out, reach * 0.75f + 4.0f));
    const ImVec2 half = mul(label_size, 0.5f);
    const ImVec2 label_centre = add(apex, ImVec2(out.x * half.x, out.y * half.y));
    return centred(label_centre, label_size);
  }

  /// The label on a panel of the Screen's colour; a pick and an edit
  /// target.
  static void draw_label(const node& n, draw_context& dc, ImDrawList* dl, const rect& panel, ImU32 panel_color) {
    const std::string& label = n.get<std::string>(transition_field::label);
    const rect area{
        ImVec2(std::round(panel.min.x), std::round(panel.min.y)),
        ImVec2(std::round(panel.max.x), std::round(panel.max.y))};
    dl->AddRectFilled(area.min, area.max, panel_color, 3.0f);
    dl->AddText(add(area.min, kLabelPadding), ImGui::GetColorU32(ImGuiCol_Text), label.c_str());
    if (dc.interactive()) {
      // Over the label, and over the figure's own window rather than a UI
      // window drawn on top of it.
      const ImVec2 mouse = ImGui::GetIO().MousePos;
      if (mouse.x >= area.min.x && mouse.x < area.max.x && mouse.y >= area.min.y && mouse.y < area.max.y &&
          ImGui::IsWindowHovered()) {
        pick p;
        p.element = &n;
        p.type = "WindowTransition";
        p.label = label;
        p.values = {{"from", n.get<std::string>(transition_field::from)}, {"to", n.get<std::string>(transition_field::to)}};
        dc.set_hovered(std::move(p));
      }
    }
    dc.record_target(n, {}, area, label);
    dc.record_target(n, "label", area, label);
  }
};

} // namespace

void register_flow(registry& reg) {
  reg.add(std::make_unique<transition_class>());
}

} // namespace bdg::nymph::ui
