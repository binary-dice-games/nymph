// MIT License © 2026 Binary Dice Games
/// @file ui_common.cpp
/// @brief widget_class and the helpers shared by the user-interface
///        elements.
#include "elements/ui/ui_common.hpp"
#include <cfloat>
#include <charconv>
#include <algorithm>
#include <cmath>
#include "core/suggest.hpp"
#include "elements/plot_style.hpp"
#include "render/text_style.hpp"

namespace bdg::nymph::ui {

namespace {

/// nullptr if `text` formats one integer acceptably, else the problem.
const char* check_int_format(std::string_view text) {
  int conversions = 0;
  for (size_t i = 0; i < text.size(); ++i) {
    if (text[i] != '%') {
      continue;
    }
    ++i;
    if (i < text.size() && text[i] == '%') {
      continue; // "%%" is a literal percent sign
    }
    while (i < text.size() && (text[i] == '-' || text[i] == '+' || text[i] == ' ' || text[i] == '0')) {
      ++i;
    }
    int digits = 0;
    while (i < text.size() && text[i] >= '0' && text[i] <= '9') {
      ++i;
      ++digits;
    }
    if (digits > 2) {
      return "has a width above 99";
    }
    if (i >= text.size() || std::string_view("diuxX").find(text[i]) == std::string_view::npos) {
      return "must format one integer with %d, %i, %u, %x or %X (like \"%d px\"); write %% for a percent sign";
    }
    if (++conversions > 1) {
      return "may contain only one %d, %i, %u, %x or %X conversion";
    }
  }
  return nullptr;
}

} // namespace

void check_opens(const node& n, const std::vector<std::string>& popup_ids) {
  for (const node& child : n.children) {
    const int index = child.cls->field_index("opens");
    if (index >= 0) {
      const std::string& id = child.get<std::string>(static_cast<size_t>(index));
      if (!id.empty() && std::find(popup_ids.begin(), popup_ids.end(), id) == popup_ids.end()) {
        std::string list;
        for (const std::string& p : popup_ids) {
          list += (list.empty() ? "" : ", ") + p;
        }
        child.cls->fail_at(
            child,
            static_cast<size_t>(index),
            "names no Popup or PopupModal id '" + id + "'" + did_you_mean(id, popup_ids) +
                (popup_ids.empty() ? "; popups go on a Screen, with an id" : "; ids: " + list));
      }
    }
    check_opens(child, popup_ids);
  }
}

std::vector<std::string> widget_container_slots() {
  return {std::string(kWidgetSlot), std::string(kFigureSlot)};
}

field_desc int_format_field(std::string name, std::string def, std::string description) {
  field_desc f = text_field(std::move(name), std::move(def), std::move(description));
  f.check = [](std::string_view text) -> std::string {
    const char* problem = check_int_format(text);
    return problem == nullptr ? std::string() : std::string(problem) + ", not '" + std::string(text) + "'";
  };
  return f;
}

ImVec4 color_of(const std::string& text) {
  return parse_hex_color(text);
}

std::string pick_number(double v) {
  char buf[32];
  const auto r = std::to_chars(buf, buf + sizeof(buf), v);
  return std::string(buf, r.ptr);
}

// ── Children ─────────────────────────────────────────────────────────────────

void draw_children(const node& n, draw_context& dc) {
  for (size_t i = 0; i < n.children.size(); ++i) {
    const node& child = n.children[i];
    ImGui::PushID(static_cast<int>(i));
    if (child.cls->placed_as(child) == kFigureSlot) {
      draw_figure_in_flow(child, dc);
    } else {
      dc.draw_node(child);
    }
    ImGui::PopID();
  }
}

void draw_figure_in_flow(const node& n, draw_context& dc) {
  const ImVec2 avail = ImGui::GetContentRegionAvail();
  const size_hints h = n.cls->hints(n);
  // A positive hint is the size, as in a layout; otherwise never below the
  // natural size, so a narrow window scrolls instead of overflowing a layout.
  float width = h.width > 0.0f ? h.width : std::max(1.0f, avail.x);
  const ImVec2 natural = dc.measure_node(n, width);
  if (h.width <= 0.0f) {
    width = std::max(width, natural.x);
  }
  float height = h.height > 0.0f ? h.height : h.height < 0.0f ? std::max(avail.y, natural.y) : natural.y;
  height = std::max(height, 1.0f);
  const ImVec2 pos = ImGui::GetCursorScreenPos();
  const rect r{pos, ImVec2(pos.x + std::round(width), pos.y + std::round(height))};
  dc.set_rect(n, r);
  dc.arrange_node(n, r);
  dc.draw_node(n);
  // The element drew at its rectangle; one item of its size takes the room.
  ImGui::SetCursorScreenPos(pos);
  ImGui::Dummy(ImVec2(r.width(), r.height()));
}

// ── stock_window_style ───────────────────────────────────────────────────────

stock_window_style::stock_window_style() {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
}

stock_window_style::~stock_window_style() {
  ImGui::PopStyleVar(3);
}

// ── widget_class ─────────────────────────────────────────────────────────────

namespace {

std::vector<field_desc> with_common_fields(std::vector<field_desc> fields, bool item_width) {
  if (item_width) {
    fields.push_back(number_field(
        "width",
        0.0,
        "Item width in pixels (ImGui's SetNextItemWidth): 0 is ImGui's default, a positive value fixed pixels, a "
        "negative value stops that many pixels before the container's right edge (-1 reaches it).",
        value_range{-4096, 4096}));
  }
  fields.push_back(bool_field("disabled", false, "Draws the widget greyed out and ignores input (BeginDisabled)."));
  fields.push_back(text_field(
      "tooltip", "", "Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG."));
  fields.push_back(text_field(
      "opens",
      "",
      "The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view."));
  return with_text_style(std::move(fields));
}

} // namespace

widget_class::widget_class(
    std::string name,
    std::vector<std::string> accepts,
    std::vector<field_desc> fields,
    std::string description,
    bool item_width)
    : element_class(
          std::move(name),
          std::string(kWidgetSlot),
          std::move(accepts),
          with_common_fields(std::move(fields), item_width),
          std::move(description)),
      item_width_(item_width) {}

size_t widget_class::common(std::string_view name) const {
  const int index = field_index(name);
  return static_cast<size_t>(index);
}

std::string widget_class::label_of(const node& n) const {
  const int index = field_index("label");
  if (index >= 0) {
    const std::string& label = n.get<std::string>(static_cast<size_t>(index));
    // ImGui does not show the part from "##" on.
    return label.substr(0, label.find("##"));
  }
  return {};
}

ui_entry& widget_class::state(const node& n, draw_context& dc, const value& source, bool& apply) {
  return dc.ui().entry(dc.path_of(n), n.cls->name(), source, dc.apply_view(), apply);
}

void widget_class::draw(const node& n, draw_context& dc) const {
  const bool disabled = n.get<bool>(common("disabled"));
  if (disabled) {
    ImGui::BeginDisabled();
  }
  if (item_width_) {
    const float w = static_cast<float>(n.get<double>(common("width")));
    if (w > 0.0f) {
      ImGui::SetNextItemWidth(w);
    } else if (w < 0.0f) {
      ImGui::SetNextItemWidth(w >= -1.0f ? -FLT_MIN : w + 1.0f);
    }
  }
  const item_result r = draw_item(n, dc);
  if (!r.finished) {
    finish_item(n, dc, r);
  }
  if (disabled) {
    ImGui::EndDisabled();
  }
}

void widget_class::finish_item(const node& n, draw_context& dc, const item_result& r) const {
  const std::string path = dc.path_of(n);
  if (r.has_id) {
    dc.ui().note_id(ImGui::GetItemID(), path);
  }
  const rect area{ImGui::GetItemRectMin(), ImGui::GetItemRectMax()};
  const std::string& opens = n.get<std::string>(common("opens"));
  if (!opens.empty()) {
    dc.ui().frame().openers[opens] = {area.min, area.max};
  }
  if (dc.interactive()) {
    const std::string& tooltip = n.get<std::string>(common("tooltip"));
    if (!tooltip.empty()) {
      ImGui::SetItemTooltip("%s", tooltip.c_str());
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      pick p;
      p.element = &n;
      p.type = std::string(name());
      p.label = label_of(n);
      p.values = r.values;
      dc.set_hovered(std::move(p));
    }
    if (r.clicked && !opens.empty()) {
      dc.ui().frame().popups_to_open.push_back(opens);
    }
  }
  if (dc.edit_mode()) {
    dc.record_target(n, {}, area, label_of(n));
    // The text it shows, for the editor: its label, else its text.
    for (const char* field : {"label", "text"}) {
      const int index = field_index(field);
      if (index >= 0 && fields()[static_cast<size_t>(index)].type == field_type::text) {
        dc.record_target(n, field, area, n.get<std::string>(static_cast<size_t>(index)));
        break;
      }
    }
  }
}

} // namespace bdg::nymph::ui
