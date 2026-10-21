// MIT License © 2026 Binary Dice Games
/// @file inspector.cpp
/// @brief The editor's panel (see inspector.hpp).
#include "edit/inspector.hpp"
#include <imgui.h>
#include <algorithm>
#include <array>
#include <cfloat>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <type_traits>
#include <variant>
#include "edit/element_icons.hpp"
#include "edit/field_value.hpp"
#include "fonts/embedded_fonts.hpp"
#include "schema/element_class.hpp"

namespace bdg::nymph {

namespace {

constexpr size_t kTextBuffer = 4096;
constexpr const char* kDefaultColor = "#2471A3";

/// "#rrggbb[aa]" to ImGui's RGBA floats; false for anything else.
bool parse_color(const std::string& text, float rgba[4]) {
  if ((text.size() != 7 && text.size() != 9) || text[0] != '#') {
    return false;
  }
  unsigned v[4] = {0, 0, 0, 255};
  for (size_t i = 0; i < (text.size() - 1) / 2; ++i) {
    char pair[3] = {text[1 + 2 * i], text[2 + 2 * i], 0};
    char* end = nullptr;
    v[i] = static_cast<unsigned>(std::strtoul(pair, &end, 16));
    if (end != pair + 2) {
      return false;
    }
  }
  for (int i = 0; i < 4; ++i) {
    rgba[i] = static_cast<float>(v[i]) / 255.0f;
  }
  return true;
}

std::string format_color(const float rgba[4]) {
  const auto byte = [](float f) { return static_cast<unsigned>(std::clamp(f, 0.0f, 1.0f) * 255.0f + 0.5f); };
  char buf[16];
  if (byte(rgba[3]) == 255) {
    std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", byte(rgba[0]), byte(rgba[1]), byte(rgba[2]));
  } else {
    std::snprintf(buf, sizeof(buf), "#%02X%02X%02X%02X", byte(rgba[0]), byte(rgba[1]), byte(rgba[2]), byte(rgba[3]));
  }
  return buf;
}

/// A short text for a value the inspector only shows.
std::string preview(const value& v) {
  std::string out;
  const auto join = [&](const auto& items) {
    for (size_t i = 0; i < items.size() && i < 8; ++i) {
      if constexpr (std::is_same_v<std::decay_t<decltype(items[i])>, std::string>) {
        out += (i ? ", " : "") + items[i];
      } else {
        out += (i ? ", " : "") + std::to_string(items[i]);
      }
    }
    if (items.size() > 8) {
      out += ", ... (" + std::to_string(items.size()) + ")";
    }
  };
  if (const auto* d = std::get_if<std::vector<double>>(&v)) {
    join(*d);
  } else if (const auto* i = std::get_if<std::vector<int64_t>>(&v)) {
    join(*i);
  } else if (const auto* s = std::get_if<std::vector<std::string>>(&v)) {
    join(*s);
  }
  return out.empty() ? "(empty)" : out;
}

bool takes_line_breaks(const field_desc& f) {
  return f.description.find("line break") != std::string::npos || f.description.find("new line") != std::string::npos;
}

bool single_bit(int64_t v) {
  return v > 0 && (v & (v - 1)) == 0;
}

} // namespace

widget_kind widget_for(const field_desc& f) {
  switch (f.type) {
    case field_type::boolean:
      return widget_kind::checkbox;
    case field_type::integer:
      return widget_kind::integer;
    case field_type::number:
      return widget_kind::number;
    case field_type::text:
      if (f.name == "font") {
        return widget_kind::font;
      }
      return takes_line_breaks(f) ? widget_kind::multiline_text : widget_kind::text;
    case field_type::color:
      return widget_kind::color;
    case field_type::enumeration:
      return f.names != nullptr ? widget_kind::choice : widget_kind::read_only;
    case field_type::flags: {
      if (f.names == nullptr) {
        return widget_kind::read_only;
      }
      // Bits to combine, or named combinations (ImPlot's legend corners) to choose from.
      const bool bits_only = std::all_of(f.names->entries().begin(), f.names->entries().end(), [](const name_entry& e) {
        return e.value == 0 || single_bit(e.value);
      });
      return bits_only ? widget_kind::flags : widget_kind::choice;
    }
    case field_type::number_array:
    case field_type::integer_array:
    case field_type::text_array:
      return widget_kind::read_only;
  }
  return widget_kind::read_only;
}

void inspector::note(const commit_result& r, inspector_events& events) {
  if (r.ok) {
    error_.clear();
    events.source = r.source;
    return;
  }
  error_ = r.error;
  if (r.line > 0) {
    error_ = "line " + std::to_string(r.line) + ": " + error_;
  }
}

commit_result inspector::commit_pending(editor_model& model) {
  commit_result r;
  r.ok = true;
  if (!pending_) {
    r.ok = false;
    return r;
  }
  const pending_text p = *pending_;
  last_attempt_ = p;
  pending_.reset();
  if (!model.selection() || model.selection()->element != p.element) {
    r.ok = false;
    return r;
  }
  const field_desc* desc = model.find_field(p.field);
  const bool default_empty = desc != nullptr && std::holds_alternative<std::string>(desc->default_value) &&
      std::get<std::string>(desc->default_value).empty();
  if (p.text.empty() && default_empty) {
    return model.clear(p.field);
  }
  return model.commit(p.field, encode_text(p.text), p.merge);
}

inspector_events inspector::flush(editor_model& model) {
  inspector_events events;
  if (pending_) {
    const commit_result r = commit_pending(model);
    if (r.ok || !r.error.empty()) {
      note(r, events);
    }
  }
  return events;
}

inspector_events inspector::edit_text(editor_model& model, const std::string& field, const std::string& text) {
  if (model.selection()) {
    pending_ = pending_text{model.selection()->element, field, text, 0.0, false};
  }
  return flush(model);
}

void inspector::draw_field(editor_model& model, const editable_field& f, bool focused, inspector_events& events) {
  const field_desc& d = *f.desc;
  const std::string element = model.selection()->element;
  const std::string key = element + "/" + d.name;
  ImGui::PushID(d.name.c_str());

  // Label line: the name (brighter when it is what was clicked), a reset
  // button for written fields, and the description as a tooltip.
  ImGui::PushStyleColor(ImGuiCol_Text, focused ? ImGui::GetStyle().Colors[ImGuiCol_CheckMark] : ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
  ImGui::TextUnformatted(d.name.c_str());
  ImGui::PopStyleColor();
  if (!d.description.empty()) {
    ImGui::SetItemTooltip("%s", d.description.c_str());
  }
  if (f.written && f.bound_to.empty()) {
    ImGui::SameLine();
    if (ImGui::SmallButton("reset")) {
      note(model.clear(d.name), events);
    }
    ImGui::SetItemTooltip("Remove the field from the source: back to its default.");
  }
  if (!f.scope_note.empty()) {
    ImGui::SameLine();
    ImGui::TextDisabled("(%s)", f.scope_note.c_str());
  }

  ImGui::SetNextItemWidth(-FLT_MIN);
  const widget_kind kind = f.bound_to.empty() ? widget_for(d) : widget_kind::read_only;
  switch (kind) {
    case widget_kind::checkbox: {
      bool v = std::get<bool>(f.current);
      if (ImGui::Checkbox("##v", &v)) {
        note(model.commit(d.name, encode_bool(v)), events);
      }
      break;
    }
    case widget_kind::integer:
    case widget_kind::number: {
      const bool is_int = kind == widget_kind::integer;
      double v = is_int ? static_cast<double>(std::get<int64_t>(f.current)) : std::get<double>(f.current);
      double lo = d.limits ? d.limits->min : 0.0, hi = d.limits ? d.limits->max : 0.0;
      const double span = d.limits ? hi - lo : 100.0;
      const float speed = static_cast<float>(is_int ? std::max(1.0, span / 200.0) : std::max(span / 400.0, 0.01));
      bool changed = false;
      if (is_int) {
        int64_t iv = static_cast<int64_t>(v);
        int64_t ilo = static_cast<int64_t>(lo), ihi = static_cast<int64_t>(hi);
        changed = ImGui::DragScalar("##v", ImGuiDataType_S64, &iv, speed, d.limits ? &ilo : nullptr, d.limits ? &ihi : nullptr);
        v = static_cast<double>(iv);
      } else {
        changed = ImGui::DragScalar("##v", ImGuiDataType_Double, &v, speed, d.limits ? &lo : nullptr, d.limits ? &hi : nullptr, "%.6g");
      }
      if (changed) {
        note(model.commit(d.name, is_int ? encode_int(static_cast<int64_t>(v)) : encode_number(v), drag_key_ == key), events);
        drag_key_ = key;
      }
      if (ImGui::IsItemDeactivated() && drag_key_ == key) {
        drag_key_.clear();
      }
      break;
    }
    case widget_kind::text:
    case widget_kind::multiline_text: {
      std::string shown = std::get<std::string>(f.current);
      if (pending_ && pending_->element == element && pending_->field == d.name) {
        shown = pending_->text;
      } else if (rejected_ && rejected_->element == element && rejected_->field == d.name) {
        shown = rejected_->text;
      }
      std::array<char, kTextBuffer> buffer{};
      std::snprintf(buffer.data(), buffer.size(), "%s", shown.c_str());
      const bool changed = kind == widget_kind::multiline_text
          ? ImGui::InputTextMultiline("##v", buffer.data(), buffer.size(), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 3.5f))
          : ImGui::InputText("##v", buffer.data(), buffer.size());
      if (changed) {
        rejected_.reset();
        pending_ = pending_text{element, d.name, buffer.data(), ImGui::GetTime(), typing_key_ == key};
        typing_key_ = key;
      }
      if (ImGui::IsItemDeactivated()) {
        if (typing_key_ == key) {
          typing_key_.clear();
        }
        if (pending_ && pending_->field == d.name) {
          const commit_result r = commit_pending(model);
          note(r, events);
          if (!r.ok && !r.error.empty()) {
            rejected_ = last_attempt_;
          }
        }
      }
      break;
    }
    case widget_kind::font: {
      const std::string& current = std::get<std::string>(f.current);
      if (ImGui::BeginCombo("##v", current.empty() ? "(inherit)" : current.c_str())) {
        if (ImGui::Selectable("(inherit)", current.empty())) {
          note(model.clear(d.name), events);
        }
        if (ImGui::Selectable("default", current == "default")) {
          note(model.commit(d.name, "default"), events);
        }
        for (const font_entry& e : font_catalog()) {
          if (ImGui::Selectable(e.name, current == e.name)) {
            note(model.commit(d.name, encode_text(e.name)), events);
          }
        }
        ImGui::EndCombo();
      }
      break;
    }
    case widget_kind::color: {
      const std::string& current = std::get<std::string>(f.current);
      float rgba[4] = {0, 0, 0, 1};
      if (current.empty() || !parse_color(current, rgba)) {
        if (ImGui::Button("automatic: click to choose")) {
          note(model.commit(d.name, *encode_color(kDefaultColor)), events);
        }
      } else {
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::ColorEdit4(
                "##v",
                rgba,
                ImGuiColorEditFlags_DisplayHex | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoOptions |
                    ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_Uint8)) {
          note(model.commit(d.name, *encode_color(format_color(rgba)), drag_key_ == key), events);
          drag_key_ = key;
        }
        if (ImGui::IsItemDeactivated() && drag_key_ == key) {
          drag_key_.clear();
        }
      }
      break;
    }
    case widget_kind::choice: {
      const int64_t current = std::get<int64_t>(f.current);
      const bool is_flags = d.type == field_type::flags;
      if (ImGui::BeginCombo("##v", (is_flags ? d.names->format_flags(current) : d.names->format_enum(current)).c_str())) {
        for (const name_entry& e : d.names->entries()) {
          if (ImGui::Selectable(e.name.c_str(), e.value == current)) {
            note(model.commit(d.name, is_flags ? encode_flags(d, e.value) : encode_enum(d, e.value)), events);
          }
        }
        ImGui::EndCombo();
      }
      break;
    }
    case widget_kind::flags: {
      int64_t current = std::get<int64_t>(f.current);
      for (const name_entry& e : d.names->entries()) {
        if (!single_bit(e.value)) {
          continue;
        }
        bool on = (current & e.value) != 0;
        if (ImGui::Checkbox(e.name.c_str(), &on)) {
          current = on ? (current | e.value) : (current & ~e.value);
          note(model.commit(d.name, encode_flags(d, current)), events);
        }
      }
      break;
    }
    case widget_kind::read_only: {
      if (!f.bound_to.empty()) {
        const std::string shown = std::holds_alternative<std::string>(f.current) ? std::get<std::string>(f.current) : "";
        ImGui::TextDisabled("%s", f.bound_to.c_str());
        ImGui::TextWrapped("%s", shown.c_str());
        if (d.type == field_type::text && ImGui::SmallButton("replace by this text")) {
          note(model.commit(d.name, encode_text(shown), false, true), events);
        }
        ImGui::SetItemTooltip("Writes the text into the field instead of taking it from the data.");
      } else if (const auto* text = std::get_if<std::string>(&f.current)) {
        ImGui::TextDisabled("%s", text->c_str());
      } else {
        ImGui::TextDisabled("%s", preview(f.current).c_str());
      }
      break;
    }
  }
  ImGui::PopID();
}

void inspector::draw_tree(editor_model& model, const std::vector<edit_element>& tree, inspector_events& events) {
  const std::string selected = model.selection() ? model.selection()->element : "";
  // A selection made in the figure opens its ancestors and scrolls to it.
  const bool reveal = selected != revealed_;
  revealed_ = selected;
  std::vector<int> open; // depths of the nodes pushed with TreePush
  int skip_below = -1; // depth of a closed node whose descendants are hidden
  const float icon = ImGui::GetTextLineHeight();
  for (size_t i = 0; i < tree.size(); ++i) {
    const edit_element& e = tree[i];
    while (!open.empty() && open.back() >= e.depth) {
      ImGui::TreePop();
      open.pop_back();
    }
    if (skip_below >= 0) {
      if (e.depth > skip_below) {
        continue;
      }
      skip_below = -1;
    }
    const bool leaf = i + 1 == tree.size() || tree[i + 1].depth <= e.depth;
    const bool is_selected = e.path == selected;
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick |
        ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
    if (leaf) {
      flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }
    if (is_selected) {
      flags |= ImGuiTreeNodeFlags_Selected;
    }
    if (reveal && !leaf && selected.size() > e.path.size() && selected.compare(0, e.path.size(), e.path) == 0 &&
        selected[e.path.size()] == '/') {
      ImGui::SetNextItemOpen(true);
    }
    ImGui::PushID(e.path.c_str());
    const bool node_open = ImGui::TreeNodeEx("##node", flags);
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
      events.select = edit_target_id{e.path, ""};
      revealed_ = e.path; // already in view
    }
    if (is_selected && reveal) {
      ImGui::SetScrollHereY(0.5f);
    }
    ImGui::SameLine(0.0f, 0.0f);
    const ImVec2 at = ImGui::GetCursorScreenPos();
    draw_element_icon(ImGui::GetWindowDrawList(), at, icon, e.type);
    ImGui::Dummy(ImVec2(icon, icon));
    ImGui::SameLine();
    ImGui::TextUnformatted(e.type.c_str());
    if (!e.label.empty()) {
      ImGui::SameLine();
      ImGui::TextDisabled("%s", e.label.substr(0, e.label.find('\n')).c_str());
    }
    ImGui::PopID();
    if (!leaf) {
      if (node_open) {
        open.push_back(e.depth);
      } else {
        skip_below = e.depth;
      }
    }
  }
  while (!open.empty()) {
    ImGui::TreePop();
    open.pop_back();
  }
}

inspector_events inspector::draw(editor_model& model, const std::vector<edit_element>& tree, const std::string& status) {
  inspector_events events;

  // A different selection forgets what was typed and refused.
  const std::string selected = model.selection() ? model.selection()->element + "/" + model.selection()->field : "";
  if (selected != last_selection_) {
    last_selection_ = selected;
    rejected_.reset();
    pending_.reset();
    typing_key_.clear();
    drag_key_.clear();
  }

  // Toolbar.
  ImGui::BeginDisabled(!model.can_undo());
  if (ImGui::Button("Undo")) {
    if (const std::optional<std::string> s = model.undo()) {
      events.source = *s;
      events.undo = true;
      error_.clear();
    }
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(!model.can_redo());
  if (ImGui::Button("Redo")) {
    if (const std::optional<std::string> s = model.redo()) {
      events.source = *s;
      events.undo = true;
      error_.clear();
    }
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  events.reset_view = ImGui::Button("Reset view");
  ImGui::BeginDisabled(!model.modified());
  events.save = ImGui::Button("Save");
  ImGui::EndDisabled();
  if (!status.empty()) {
    ImGui::TextDisabled("%s", status.c_str());
  }
  ImGui::Separator();

  // The error panel at the bottom takes the height of its text, at most
  // 40% of the panel; the rest scrolls above it.
  const ImGuiStyle& style = ImGui::GetStyle();
  const auto error_height = [&] {
    if (error_.empty()) {
      return 0.0f;
    }
    // The child's border and padding, and room for a scrollbar, narrow the text.
    const float wrap = ImGui::GetContentRegionAvail().x - style.WindowPadding.x * 2.0f - style.ScrollbarSize;
    const float text = ImGui::CalcTextSize(error_.c_str(), nullptr, false, wrap).y;
    const float frame = style.WindowPadding.y * 2.0f + style.ChildBorderSize * 2.0f + style.ItemSpacing.y;
    return std::min(text + frame, ImGui::GetContentRegionAvail().y * 0.4f);
  };
  const float reserved = error_height();
  ImGui::BeginChild("##body", ImVec2(0, reserved > 0.0f ? -(reserved + style.ItemSpacing.y) : 0.0f));

  // The elements.
  if (ImGui::CollapsingHeader("Elements", ImGuiTreeNodeFlags_DefaultOpen)) {
    const float rows = static_cast<float>(tree.size() + 1);
    ImGui::BeginChild("##elements", ImVec2(0, std::min(220.0f, ImGui::GetTextLineHeightWithSpacing() * rows)), ImGuiChildFlags_Borders);
    draw_tree(model, tree, events);
    ImGui::EndChild();
  }

  // The selection.
  if (!model.selection()) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    ImGui::TextWrapped("Click a text or an element in the figure, or choose one above.");
    ImGui::PopStyleColor();
  } else {
    const std::vector<editable_field> fields = model.fields();
    const std::string focus = model.selection()->field;
    ImGui::SeparatorText((model.selected_type() + "  " + model.selection()->element).c_str());
    const editable_field* first = nullptr;
    for (const editable_field& f : fields) {
      if (f.desc->name == focus) {
        first = &f;
      }
    }
    if (first != nullptr) {
      draw_field(model, *first, true, events);
      ImGui::Separator();
    }
    if (ImGui::CollapsingHeader("All fields", first != nullptr ? 0 : ImGuiTreeNodeFlags_DefaultOpen)) {
      for (const editable_field& f : fields) {
        if (&f != first) {
          draw_field(model, f, false, events);
        }
      }
    }
  }

  // A text typed a moment ago is committed once the pause is over.
  if (pending_ && ImGui::GetTime() - pending_->changed_at >= kTypingPause) {
    const commit_result r = commit_pending(model);
    note(r, events);
    if (!r.ok && !r.error.empty()) {
      rejected_ = last_attempt_; // keep showing what was typed
    }
  }
  ImGui::EndChild();

  // The error panel, below the fields.
  if (!error_.empty()) {
    ImGui::BeginChild("##error", ImVec2(0, 0), ImGuiChildFlags_Borders);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.2f, 0.2f, 1.0f));
    ImGui::TextWrapped("%s", error_.c_str());
    ImGui::PopStyleColor();
    ImGui::EndChild();
  }
  return events;
}

} // namespace bdg::nymph
