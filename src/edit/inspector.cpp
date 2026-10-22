// MIT License © 2026 Binary Dice Games
/// @file inspector.cpp
/// @brief The editor's panel (see inspector.hpp).
#include "edit/inspector.hpp"
#include <imgui.h>
#include <imgui_internal.h> // ImGuiContext::ColorPickerRef
#include <algorithm>
#include <array>
#include <cfloat>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include "edit/element_icons.hpp"
#include "edit/field_value.hpp"
#include "edit/image_fields.hpp"
#include "edit/number_box.hpp"
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
      } else if constexpr (std::is_same_v<std::decay_t<decltype(items[i])>, double>) {
        out += (i ? ", " : "") + encode_number(items[i]); // 1.5, not 1.500000
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
      return widget_kind::list;
  }
  return widget_kind::read_only;
}

namespace {

/// A "columns" button whose menu lists every column of the data as a
/// reference. @return The one chosen, or "".
std::string column_picker(const editor_model& model) {
  std::string chosen;
  ImGui::SameLine();
  if (ImGui::SmallButton("columns")) {
    ImGui::OpenPopup("##columns");
  }
  ImGui::SetItemTooltip("Take the values from a column of the data.");
  if (ImGui::BeginPopup("##columns")) {
    const std::vector<std::string> refs = model.column_references();
    if (refs.empty()) {
      ImGui::TextDisabled("No data: add a table with the + tab.");
    }
    for (const std::string& ref : refs) {
      if (ImGui::Selectable(ref.c_str())) {
        chosen = ref;
      }
    }
    ImGui::EndPopup();
  }
  return chosen;
}

/// "Go to data" for a field that names a column: its table and column.
std::optional<std::pair<std::string, std::string>> data_of(const std::string& reference) {
  if (reference.size() < 2 || reference[0] != '$') {
    return std::nullopt;
  }
  const std::string ref = reference.substr(1);
  const size_t colon = ref.find(':');
  return colon == std::string::npos ? std::make_pair(std::string(), ref)
                                    : std::make_pair(ref.substr(0, colon), ref.substr(colon + 1));
}

} // namespace

inspector_events inspector::edit_list(editor_model& model, const std::string& field, const std::string& yaml) {
  inspector_events events;
  const size_t a = yaml.find_first_not_of(" \t");
  const std::string text = a == std::string::npos ? std::string() : yaml.substr(a, yaml.find_last_not_of(" \t") - a + 1);
  if (text.empty()) {
    note(model.clear(field), events);
  } else {
    note(model.commit(field, text, false, /*replace_binding=*/true), events);
  }
  return events;
}

void inspector::note(const commit_result& r, inspector_events& events) {
  if (r.ok) {
    error_.clear();
    events.source = r.source;
    events.paths.then(r.paths);
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
  if (f.written && (f.bound_to.empty() || widget_for(d) == widget_kind::list)) {
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
  const widget_kind kind = widget_for(d) == widget_kind::list ? widget_kind::list
      : f.bound_to.empty()                                    ? widget_for(d)
                                                              : widget_kind::read_only;
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
      number_box_options options;
      options.integer = is_int;
      if (d.limits) {
        options.min = d.limits->min;
        options.max = d.limits->max;
      }
      if (d.name == "font_size" && element != kImagePath) {
        options.zero_text = "auto (inherited)";
        options.zero_start = model.inherited_font_size();
      }
      const number_box_change changed = numbers_.draw("##v", v, options);
      if (changed != number_box_change::none) {
        // A drag is one undo step however many values it goes through.
        const bool merge = changed == number_box_change::dragged && drag_key_ == key;
        note(model.commit(d.name, is_int ? encode_int(static_cast<int64_t>(v)) : encode_number(v), merge), events);
        drag_key_ = changed == number_box_change::dragged ? key : std::string();
      }
      if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) && drag_key_ == key) {
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
          open_picker_key_ = key; // the starting colour may look like the automatic one: choose at once
        }
      } else {
        if (open_picker_key_ == key) {
          // As a click on ColorEdit4's swatch does: inside its ID, with the
          // colour it started from.
          open_picker_key_.clear();
          ImGui::PushID("##v");
          ImGui::OpenPopup("picker");
          ImGui::PopID();
          ImGui::GetCurrentContext()->ColorPickerRef = ImVec4(rgba[0], rgba[1], rgba[2], rgba[3]);
        }
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
    case widget_kind::list: {
      // The list as YAML, committed when the field is left (Enter, Tab, a click).
      const bool typing = list_key_ == key;
      char scratch[sizeof(list_buffer_)];
      char* buffer = typing ? list_buffer_ : scratch;
      if (!typing) {
        std::snprintf(scratch, sizeof(scratch), "%s", f.source_text.c_str());
      }
      ImGui::SetNextItemWidth(-ImGui::CalcTextSize("columns").x - ImGui::GetStyle().FramePadding.x * 2.0f -
                              ImGui::GetStyle().ItemSpacing.x);
      ImGui::InputTextWithHint("##v", "[1, 2, 3] or $table:column", buffer, sizeof(list_buffer_));
      if (ImGui::IsItemActivated()) {
        list_key_ = key;
        std::snprintf(list_buffer_, sizeof(list_buffer_), "%s", f.source_text.c_str());
      }
      if (ImGui::IsItemDeactivatedAfterEdit() && list_key_ == key) {
        const std::string typed = list_buffer_;
        if (typed != f.source_text) {
          const inspector_events e = edit_list(model, d.name, typed);
          events.source = e.source;
          events.paths.then(e.paths);
        }
      }
      if (ImGui::IsItemDeactivated() && list_key_ == key) {
        list_key_.clear();
      }
      if (const std::string ref = column_picker(model); !ref.empty()) {
        const inspector_events e = edit_list(model, d.name, ref);
        events.source = e.source;
      }
      // What it holds now, and the way to its data.
      ImGui::TextDisabled("%s", preview(f.current).c_str());
      if (const auto data = data_of(f.bound_to)) {
        ImGui::SameLine();
        if (ImGui::SmallButton("Go to data")) {
          events.show_data = data;
        }
      }
      break;
    }
    case widget_kind::read_only: {
      if (!f.bound_to.empty()) {
        const std::string shown = std::holds_alternative<std::string>(f.current) ? std::get<std::string>(f.current) : "";
        ImGui::TextDisabled("%s", f.bound_to.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Go to data")) {
          // "$t:col" or "$col": the table and the column to show.
          const std::string ref = f.bound_to.substr(1);
          const size_t colon = ref.find(':');
          events.show_data = colon == std::string::npos ? std::make_pair(std::string(), ref)
                                                        : std::make_pair(ref.substr(0, colon), ref.substr(colon + 1));
        }
        if (const std::string ref = column_picker(model); !ref.empty()) {
          note(model.commit(d.name, ref, false, true), events);
        }
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

void inspector::drag_drop(editor_model& model, const edit_element& e, bool open, inspector_events& events) {
  constexpr const char* kPayload = "nymph_element";
  if (e.depth > 0 && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
    ImGui::SetDragDropPayload(kPayload, e.path.c_str(), e.path.size() + 1);
    ImGui::TextUnformatted(e.type.c_str());
    ImGui::EndDragDropSource();
  }
  if (!ImGui::BeginDragDropTarget()) {
    return;
  }
  const ImGuiPayload* payload =
      ImGui::AcceptDragDropPayload(kPayload, ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect);
  if (payload != nullptr) {
    const std::string dragged(static_cast<const char*>(payload->Data));
    const ImVec2 lo = ImGui::GetItemRectMin();
    const ImVec2 hi = ImGui::GetItemRectMax();
    const float fraction = (ImGui::GetMousePos().y - lo.y) / std::max(hi.y - lo.y, 1.0f);
    if (const std::optional<drop_place> place = plan_drop(model, dragged, e, fraction, open)) {
      // The line shows where it lands: above or below the row, or a frame round it.
      const bool into = place->parent == e.path;
      const bool above = fraction < 0.5f;
      const ImU32 color = ImGui::GetColorU32(ImGuiCol_DragDropTarget);
      ImDrawList* list = ImGui::GetWindowDrawList();
      if (into) {
        list->AddRect(lo, hi, color, 0.0f, 0, 2.0f);
      } else {
        const float y = above ? lo.y : hi.y;
        list->AddLine(ImVec2(lo.x, y), ImVec2(hi.x, y), color, 2.0f);
      }
      if (payload->IsDelivery()) {
        menu_.drop(model, dragged, *place, events);
      }
    }
  }
  ImGui::EndDragDropTarget();
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
    const bool visible_children = !leaf && node_open;
    drag_drop(model, e, visible_children, events);
    menu_.row(model, e, events);
    if (events.select && events.select->element == e.path) {
      revealed_ = e.path; // a right-click selects too: in view, no scroll
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

void inspector::draw_info(editor_model& model, inspector_events& events) {
  constexpr const char* kInfoDialog = "Description and source";
  if (info_open_) {
    ImGui::OpenPopup(kInfoDialog);
    info_open_ = false;
  }
  // A modal dialog: it takes the pointer and keyboard from the figure.
  const ImGuiViewport* vp = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(
      ImVec2(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(std::min(460.0f, vp->Size.x * 0.9f), std::min(400.0f, vp->Size.y * 0.9f)), ImGuiCond_Appearing);
  if (ImGui::BeginPopupModal(kInfoDialog, nullptr, ImGuiWindowFlags_NoSavedSettings)) {
    const ImGuiStyle& style = ImGui::GetStyle();
    const struct {
      const char* key;
      const char* title;
      const char* help;
    } entries[] = {
        {"description", "Description", "The alt text of the image: what the figure shows."},
        {"source", "Source", "How the data was made (the prompt, query or code); kept in the file, not drawn."},
    };
    constexpr float kDividerHeight = 6.0f;
    // The close button takes the bottom row; the two boxes share the rest,
    // after the headings and the divider.
    const float button_row = ImGui::GetFrameHeight() + style.ItemSpacing.y;
    const float heading = (ImGui::GetTextLineHeight() + style.ItemSpacing.y) * 2.0f + style.ItemSpacing.y;
    const float total = std::max(
        ImGui::GetContentRegionAvail().y - button_row - heading * 2.0f - kDividerHeight - style.ItemSpacing.y * 3.0f, 40.0f);
    const float first = std::clamp(total * info_split_, 20.0f, std::max(total - 20.0f, 20.0f));
    for (size_t i = 0; i < 2; ++i) {
      const auto& entry = entries[i];
      const std::string key = entry.key;
      ImGui::PushID(entry.key);
      ImGui::SeparatorText(entry.title);
      ImGui::TextDisabled("%s", entry.help);
      const bool typing = info_key_ == key;
      const std::string current = model.top_level_text(key);
      char scratch[sizeof(info_buffer_)];
      char* buffer = typing ? info_buffer_ : scratch;
      if (!typing) {
        std::snprintf(scratch, sizeof(scratch), "%s", current.c_str());
      }
      const float height = i == 0 ? first : total - first;
      ImGui::InputTextMultiline(
          "##text", buffer, sizeof(info_buffer_), ImVec2(-FLT_MIN, height), ImGuiInputTextFlags_WordWrap);
      if (ImGui::IsItemActivated()) {
        info_key_ = key;
        std::snprintf(info_buffer_, sizeof(info_buffer_), "%s", current.c_str());
      }
      if (ImGui::IsItemDeactivated() && info_key_ == key) {
        const std::string typed = info_buffer_;
        info_key_.clear();
        if (typed != current) {
          note(model.set_top_level_text(key, typed), events);
        }
      }
      if (i == 0) {
        // The divider between the boxes: drag it to share the space.
        ImGui::InvisibleButton("##split", ImVec2(-FLT_MIN, kDividerHeight));
        const bool grabbed = ImGui::IsItemHovered() || ImGui::IsItemActive();
        if (grabbed) {
          ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        }
        if (ImGui::IsItemActive()) {
          info_split_ = std::clamp((first + ImGui::GetIO().MouseDelta.y) / total, 0.1f, 0.9f);
        }
        const ImVec2 lo = ImGui::GetItemRectMin();
        const ImVec2 hi = ImGui::GetItemRectMax();
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(lo.x, (lo.y + hi.y) * 0.5f),
            ImVec2(hi.x, (lo.y + hi.y) * 0.5f),
            ImGui::GetColorU32(grabbed ? ImGuiCol_SeparatorActive : ImGuiCol_Separator),
            2.0f);
      }
      ImGui::PopID();
    }
    if (ImGui::Button("Close") || (info_key_.empty() && ImGui::IsKeyPressed(ImGuiKey_Escape, false))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
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
  ImGui::SameLine();
  if (ImGui::Button("Info")) {
    info_open_ = !info_open_;
  }
  ImGui::SetItemTooltip("Edit the description (alt text) and the source note of the figure.");
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
    ImGui::BeginChild("##elements", ImVec2(0, elements_height_), ImGuiChildFlags_Borders);
    draw_tree(model, tree, events);
    ImGui::EndChild();
    // The divider under the tree: drag it to resize the tree.
    ImGui::InvisibleButton("##divider", ImVec2(-FLT_MIN, 6.0f));
    if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
      ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    }
    if (ImGui::IsItemActive()) {
      elements_height_ = std::clamp(elements_height_ + ImGui::GetIO().MouseDelta.y, 40.0f, 4000.0f);
    }
    const ImVec2 lo = ImGui::GetItemRectMin();
    const ImVec2 hi = ImGui::GetItemRectMax();
    const float mid = (lo.y + hi.y) * 0.5f;
    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(lo.x, mid),
        ImVec2(hi.x, mid),
        ImGui::GetColorU32(ImGui::IsItemHovered() || ImGui::IsItemActive() ? ImGuiCol_SeparatorActive : ImGuiCol_Separator),
        2.0f);
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

  menu_.dialogs(model, events);
  draw_info(model, events);
  if (!events.error.empty()) {
    error_ = events.error;
  } else if (events.source && events.paths.empty() == false) {
    error_.clear();
  }

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
