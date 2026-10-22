// MIT License © 2026 Binary Dice Games
/// @file edit_toolbar.cpp
/// @brief The editor's floating toolbar (see edit_toolbar.hpp).
#include "edit/edit_toolbar.hpp"
#include <imuml.h>
#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <variant>
#include "edit/field_value.hpp"
#include "edit/image_fields.hpp"
#include "fonts/embedded_fonts.hpp"
#include "shapes/shape_catalog.hpp"

namespace bdg::nymph {

namespace {

constexpr int kPaletteColumns = 6;

/// "#rrggbb[aa]" to RGBA floats; false for anything else.
bool parse_rgba(const std::string& text, float rgba[4]) {
  if ((text.size() != 7 && text.size() != 9) || text[0] != '#') {
    return false;
  }
  unsigned v[4] = {0, 0, 0, 255};
  for (size_t i = 0; i < (text.size() - 1) / 2; ++i) {
    const char pair[3] = {text[1 + 2 * i], text[2 + 2 * i], 0};
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

std::string format_rgba(const float rgba[4]) {
  const auto byte = [](float f) { return static_cast<unsigned>(std::clamp(f, 0.0f, 1.0f) * 255.0f + 0.5f); };
  char buf[16];
  if (byte(rgba[3]) == 255) {
    std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", byte(rgba[0]), byte(rgba[1]), byte(rgba[2]));
  } else {
    std::snprintf(buf, sizeof(buf), "#%02X%02X%02X%02X", byte(rgba[0]), byte(rgba[1]), byte(rgba[2]), byte(rgba[3]));
  }
  return buf;
}

bool same_color(const std::string& a, const std::string& b) {
  float x[4], y[4];
  if (!parse_rgba(a, x) || !parse_rgba(b, y)) {
    return false;
  }
  for (int i = 0; i < 4; ++i) {
    if (std::fabs(x[i] - y[i]) > 0.5f / 255.0f) {
      return false;
    }
  }
  return true;
}

ImU32 to_u32(const float rgba[4]) {
  return ImGui::ColorConvertFloat4ToU32(ImVec4(rgba[0], rgba[1], rgba[2], rgba[3]));
}

/// A square button with a picture drawn by `draw(dl, lo, hi)`.
template <typename Draw>
bool icon_button(const char* id, const char* tooltip, bool on, Draw draw, float width = 0.0f) {
  const float s = ImGui::GetFrameHeight();
  const ImVec2 size(width > 0.0f ? width : s, s);
  const ImVec2 lo = ImGui::GetCursorScreenPos();
  const ImVec2 hi(lo.x + size.x, lo.y + size.y);
  const bool clicked = ImGui::InvisibleButton(id, size);
  const bool hovered = ImGui::IsItemHovered();
  ImDrawList* dl = ImGui::GetWindowDrawList();
  if (ImGui::IsItemActive() || on || hovered) {
    const ImGuiCol bg = ImGui::IsItemActive() ? ImGuiCol_ButtonActive : on ? ImGuiCol_Header : ImGuiCol_ButtonHovered;
    dl->AddRectFilled(lo, hi, ImGui::GetColorU32(bg), ImGui::GetStyle().FrameRounding);
  }
  draw(dl, lo, hi);
  if (tooltip != nullptr) {
    ImGui::SetItemTooltip("%s", tooltip);
  }
  return clicked;
}

/// A thin vertical line between groups of tools.
void group_separator() {
  ImGui::SameLine();
  const float s = ImGui::GetFrameHeight();
  const ImVec2 at = ImGui::GetCursorScreenPos();
  ImGui::GetWindowDrawList()->AddLine(
      ImVec2(at.x, at.y + s * 0.15f), ImVec2(at.x, at.y + s * 0.85f), ImGui::GetColorU32(ImGuiCol_Separator), 1.0f);
  ImGui::Dummy(ImVec2(1.0f, s));
  ImGui::SameLine();
}

/// Opens popup `id` under the button just drawn.
void open_below(const char* id) {
  ImGui::OpenPopup(id);
}
void place_below_last_item() {
  ImGui::SetNextWindowPos(ImVec2(ImGui::GetItemRectMin().x, ImGui::GetItemRectMax().y + 4.0f), ImGuiCond_Appearing);
}

/// The name a font file is shown with: no extension.
std::string font_label(const std::string& file) {
  const size_t dot = file.rfind('.');
  return dot == std::string::npos ? file : file.substr(0, dot);
}

/// The library shape that draws enumeration name `name` ("Box", "Diamond",
/// "cloud"), or "".
std::string library_shape(const std::string& name) {
  std::string lower;
  for (const char c : name) {
    if (std::isupper(static_cast<unsigned char>(c)) && !lower.empty() && lower.back() != '_') {
      lower += '_';
    }
    lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  static const std::map<std::string, std::string, std::less<>> aliases = {
      {"box", "rectangle"},
      {"rounded", "rounded_rectangle"},
      {"subroutine", "predefined_process"},
      {"rect", "rectangle"},
  };
  if (const auto it = aliases.find(lower); it != aliases.end()) {
    return it->second;
  }
  return lower;
}

// ── Icons ────────────────────────────────────────────────────────────────

void draw_slash(ImDrawList* dl, ImVec2 a, ImVec2 b) {
  dl->AddLine(ImVec2(a.x, b.y), ImVec2(b.x, a.y), IM_COL32(220, 50, 50, 255), 1.5f);
}

void draw_fill_icon(ImDrawList* dl, ImVec2 lo, ImVec2 hi, const std::string& color) {
  const float s = hi.y - lo.y;
  const ImVec2 a(lo.x + s * 0.24f, lo.y + s * 0.24f), b(hi.x - s * 0.24f, hi.y - s * 0.24f);
  float rgba[4];
  const ImU32 text = ImGui::GetColorU32(ImGuiCol_Text);
  if (parse_rgba(color, rgba)) {
    dl->AddRectFilled(a, b, to_u32(rgba), 3.0f);
    dl->AddRect(a, b, text, 3.0f, 0, 1.0f);
  } else {
    dl->AddRect(a, b, text, 3.0f, 0, 1.0f);
    draw_slash(dl, a, b);
  }
}

void draw_line_icon(ImDrawList* dl, ImVec2 lo, ImVec2 hi, const std::string& color) {
  const float s = hi.y - lo.y;
  const ImVec2 a(lo.x + s * 0.27f, lo.y + s * 0.27f), b(hi.x - s * 0.27f, hi.y - s * 0.27f);
  float rgba[4];
  if (parse_rgba(color, rgba)) {
    dl->AddRect(a, b, to_u32(rgba), 2.0f, 0, 3.0f);
  } else {
    dl->AddRect(a, b, ImGui::GetColorU32(ImGuiCol_Text), 2.0f, 0, 1.5f);
    draw_slash(dl, a, b);
  }
}

void draw_text_icon(ImDrawList* dl, ImVec2 lo, ImVec2 hi, const std::string& color) {
  const float s = hi.y - lo.y;
  const ImVec2 size = ImGui::CalcTextSize("A");
  const float cx = (lo.x + hi.x) * 0.5f;
  dl->AddText(ImVec2(cx - size.x * 0.5f, lo.y + s * 0.42f - size.y * 0.5f), ImGui::GetColorU32(ImGuiCol_Text), "A");
  const ImVec2 a(cx - s * 0.28f, lo.y + s * 0.72f), b(cx + s * 0.28f, lo.y + s * 0.84f);
  float rgba[4];
  if (parse_rgba(color, rgba)) {
    dl->AddRectFilled(a, b, to_u32(rgba), 1.0f);
  } else {
    dl->AddRect(a, b, ImGui::GetColorU32(ImGuiCol_TextDisabled), 1.0f);
  }
}

void draw_shapes_icon(ImDrawList* dl, ImVec2 lo, ImVec2 hi) {
  const float s = hi.y - lo.y;
  const ImU32 c = ImGui::GetColorU32(ImGuiCol_Text);
  const float x = lo.x + (hi.x - lo.x - s) * 0.5f;
  dl->AddTriangle(
      ImVec2(x + s * 0.5f, lo.y + s * 0.2f),
      ImVec2(x + s * 0.32f, lo.y + s * 0.48f),
      ImVec2(x + s * 0.68f, lo.y + s * 0.48f),
      c,
      1.3f);
  dl->AddRect(ImVec2(x + s * 0.2f, lo.y + s * 0.56f), ImVec2(x + s * 0.44f, lo.y + s * 0.8f), c, 1.0f, 0, 1.3f);
  dl->AddCircle(ImVec2(x + s * 0.68f, lo.y + s * 0.68f), s * 0.12f, c, 0, 1.3f);
}

void draw_roller_icon(ImDrawList* dl, ImVec2 lo, ImVec2 hi) {
  const float s = hi.y - lo.y;
  const ImU32 c = ImGui::GetColorU32(ImGuiCol_Text);
  const float x = lo.x + (hi.x - lo.x - s) * 0.5f;
  const auto p = [&](float u, float v) { return ImVec2(x + s * u, lo.y + s * v); };
  dl->AddRectFilled(p(0.22f, 0.2f), p(0.72f, 0.38f), c, 2.0f);
  const ImVec2 handle[] = {p(0.72f, 0.29f), p(0.8f, 0.29f), p(0.8f, 0.5f), p(0.48f, 0.55f), p(0.48f, 0.62f)};
  dl->AddPolyline(handle, 5, c, 0, 1.5f);
  dl->AddRectFilled(p(0.43f, 0.62f), p(0.53f, 0.84f), c, 1.5f);
}

void draw_eraser_icon(ImDrawList* dl, ImVec2 lo, ImVec2 hi) {
  const float s = hi.y - lo.y;
  const ImU32 c = ImGui::GetColorU32(ImGuiCol_Text);
  const float x = lo.x + (hi.x - lo.x - s) * 0.5f;
  const auto p = [&](float u, float v) { return ImVec2(x + s * u, lo.y + s * v); };
  dl->AddQuad(p(0.2f, 0.58f), p(0.52f, 0.26f), p(0.8f, 0.54f), p(0.48f, 0.86f), c, 1.3f);
  dl->AddQuadFilled(p(0.52f, 0.26f), p(0.8f, 0.54f), p(0.64f, 0.7f), p(0.36f, 0.42f), c);
  dl->AddLine(p(0.48f, 0.86f), p(0.84f, 0.86f), c, 1.3f);
}

void draw_duplicate_icon(ImDrawList* dl, ImVec2 lo, ImVec2 hi) {
  const float s = hi.y - lo.y;
  const ImU32 c = ImGui::GetColorU32(ImGuiCol_Text);
  const float x = lo.x + (hi.x - lo.x - s) * 0.5f;
  const auto p = [&](float u, float v) { return ImVec2(x + s * u, lo.y + s * v); };
  dl->AddRect(p(0.22f, 0.32f), p(0.62f, 0.8f), c, 2.0f, 0, 1.3f);
  const ImVec2 back[] = {p(0.38f, 0.32f), p(0.38f, 0.2f), p(0.78f, 0.2f), p(0.78f, 0.68f), p(0.62f, 0.68f)};
  dl->AddPolyline(back, 5, c, 0, 1.3f);
}

void draw_trash_icon(ImDrawList* dl, ImVec2 lo, ImVec2 hi) {
  const float s = hi.y - lo.y;
  const ImU32 c = ImGui::GetColorU32(ImGuiCol_Text);
  const float x = lo.x + (hi.x - lo.x - s) * 0.5f;
  const auto p = [&](float u, float v) { return ImVec2(x + s * u, lo.y + s * v); };
  dl->AddLine(p(0.22f, 0.3f), p(0.78f, 0.3f), c, 1.3f);
  dl->AddRect(p(0.42f, 0.2f), p(0.58f, 0.3f), c, 1.0f, 0, 1.3f);
  const ImVec2 body[] = {p(0.3f, 0.3f), p(0.34f, 0.82f), p(0.66f, 0.82f), p(0.7f, 0.3f)};
  dl->AddPolyline(body, 4, c, 0, 1.3f);
  dl->AddLine(p(0.44f, 0.42f), p(0.45f, 0.72f), c, 1.0f);
  dl->AddLine(p(0.56f, 0.42f), p(0.55f, 0.72f), c, 1.0f);
}

const char* tool_tooltip(toolbar_tool tool) {
  switch (tool) {
    case toolbar_tool::shape:
      return "Shape";
    case toolbar_tool::fill_color:
      return "Fill colour";
    case toolbar_tool::line_color:
      return "Line colour";
    case toolbar_tool::text_color:
      return "Text colour";
    case toolbar_tool::font:
      return "Font";
    case toolbar_tool::font_size:
      return "Text size (auto: inherited)\nClick to type, drag sideways to change";
    case toolbar_tool::line_weight:
      return "Line thickness\nClick to type, drag sideways to change";
    case toolbar_tool::line:
      return "Line style";
    case toolbar_tool::arrow:
      return "Arrow heads";
    case toolbar_tool::copy_style:
      return "Copy style: then click another element to give it this style";
    case toolbar_tool::clear_style:
      return "Clear colours and text style";
    case toolbar_tool::duplicate:
      return "Duplicate (Ctrl+D)";
    case toolbar_tool::remove:
      return "Delete (Del)";
  }
  return "";
}

/// The group a tool belongs to: a separator goes between groups.
int tool_group(toolbar_tool tool) {
  switch (tool) {
    case toolbar_tool::shape:
    case toolbar_tool::fill_color:
    case toolbar_tool::line_color:
    case toolbar_tool::text_color:
      return 0;
    case toolbar_tool::font:
    case toolbar_tool::font_size:
      return 1;
    case toolbar_tool::line_weight:
    case toolbar_tool::line:
    case toolbar_tool::arrow:
      return 2;
    case toolbar_tool::copy_style:
    case toolbar_tool::clear_style:
      return 3;
    case toolbar_tool::duplicate:
    case toolbar_tool::remove:
      return 4;
  }
  return 0;
}

/// Whether field `f` can be edited by `tool`'s widget.
bool tool_fits(toolbar_tool tool, const field_desc& f) {
  switch (tool) {
    case toolbar_tool::fill_color:
    case toolbar_tool::line_color:
    case toolbar_tool::text_color:
      return f.type == field_type::color;
    case toolbar_tool::font:
      return f.type == field_type::text;
    case toolbar_tool::font_size:
    case toolbar_tool::line_weight:
      return f.type == field_type::number || f.type == field_type::integer;
    case toolbar_tool::shape:
    case toolbar_tool::line:
    case toolbar_tool::arrow:
      return f.type == field_type::enumeration && f.names != nullptr;
    default:
      return false;
  }
}

constexpr toolbar_tool kFieldTools[] = {
    toolbar_tool::shape,
    toolbar_tool::fill_color,
    toolbar_tool::line_color,
    toolbar_tool::text_color,
    toolbar_tool::font,
    toolbar_tool::font_size,
    toolbar_tool::line_weight,
    toolbar_tool::line,
    toolbar_tool::arrow,
};

/// The colours and text style: what "clear style" removes.
constexpr const char* kClearedFields[] = {"fill_color", "color", "text_color", "font", "font_size", "line_weight"};

/// Fields that hold a value of the element's own type (an enumeration):
/// copied only between elements of one type.
bool type_specific(const std::string& field) {
  return field == "shape" || field == "line" || field == "arrow";
}

} // namespace

const char* toolbar_field(toolbar_tool tool) {
  switch (tool) {
    case toolbar_tool::shape:
      return "shape";
    case toolbar_tool::fill_color:
      return "fill_color";
    case toolbar_tool::line_color:
      return "color";
    case toolbar_tool::text_color:
      return "text_color";
    case toolbar_tool::font:
      return "font";
    case toolbar_tool::font_size:
      return "font_size";
    case toolbar_tool::line_weight:
      return "line_weight";
    case toolbar_tool::line:
      return "line";
    case toolbar_tool::arrow:
      return "arrow";
    default:
      return "";
  }
}

const std::array<const char*, 18>& toolbar_palette() {
  static const std::array<const char*, 18> colors = {
      "#D9D9D9",
      "#000000",
      "#404040",
      "#666666",
      "#999999",
      "#FFFFFF", // greys
      "#E53935",
      "#FB8C00",
      "#FDD835",
      "#43A047",
      "#1E88E5",
      "#8E24AA", // strong
      "#F8BBD0",
      "#FFE0B2",
      "#FFF9C4",
      "#C8E6C9",
      "#BBDEFB",
      "#E1BEE7", // pale
  };
  return colors;
}

const std::vector<std::string>& style_fields() {
  static const std::vector<std::string> names = {
      "shape", "fill_color", "color", "text_color", "font", "font_size", "line_weight", "line", "arrow"};
  return names;
}

edit_toolbar::edit_toolbar() = default;

edit_toolbar::~edit_toolbar() {
  if (shapes_ != nullptr) {
    ImUmlContext* const previous = ImUml::GetCurrentContext();
    ImUml::DestroyContext(shapes_);
    ImUml::SetCurrentContext(previous == shapes_ ? nullptr : previous);
  }
}

std::vector<toolbar_tool> edit_toolbar::tools_for(const editor_model& model) {
  std::vector<toolbar_tool> tools;
  if (!model.selection()) {
    return tools;
  }
  const std::vector<editable_field> fields = model.fields();
  bool styled = false;
  for (const toolbar_tool tool : kFieldTools) {
    const auto it = std::find_if(
        fields.begin(), fields.end(), [&](const editable_field& f) { return f.desc->name == toolbar_field(tool); });
    if (it != fields.end() && it->bound_to.empty() && tool_fits(tool, *it->desc)) {
      tools.push_back(tool);
      styled = true;
    }
  }
  const std::string& path = model.selection()->element;
  if (styled && path != kImagePath) {
    tools.push_back(toolbar_tool::copy_style);
    tools.push_back(toolbar_tool::clear_style);
  }
  if (path != kImagePath && path != "root") {
    tools.push_back(toolbar_tool::duplicate);
    tools.push_back(toolbar_tool::remove);
  }
  return tools;
}

layout_box
edit_toolbar::place(const std::optional<layout_box>& anchor, const layout_box& area, float width, float height) {
  constexpr float kGap = 10.0f;
  constexpr float kMargin = 6.0f;
  const float left = area.x + kMargin;
  const float right = area.x + area.width - kMargin - width;
  const float top = area.y + kMargin;
  const float bottom = area.y + area.height - kMargin - height;
  layout_box box{0.0f, 0.0f, width, height};
  const float cx = anchor ? anchor->x + anchor->width * 0.5f : area.x + area.width * 0.5f;
  box.x = right < left ? left : std::clamp(cx - width * 0.5f, left, right);
  if (anchor && anchor->y - kGap - height >= top) {
    box.y = anchor->y - kGap - height; // above
  } else if (anchor && anchor->y + anchor->height + kGap <= bottom) {
    box.y = anchor->y + anchor->height + kGap; // under
  } else {
    box.y = top;
  }
  return box;
}

bool edit_toolbar::copy_style(const editor_model& model) {
  if (!model.selection() || model.selection()->element == kImagePath) {
    return false;
  }
  copied_style clip;
  clip.from = model.selection()->element;
  clip.type = model.selected_type();
  for (const editable_field& f : model.fields()) {
    const auto& names = style_fields();
    if (std::find(names.begin(), names.end(), f.desc->name) == names.end() || !f.bound_to.empty()) {
      continue;
    }
    clip.fields.emplace_back(
        f.desc->name, f.written && !f.source_text.empty() ? std::optional<std::string>(f.source_text) : std::nullopt);
  }
  clip_ = std::move(clip);
  return true;
}

commit_result edit_toolbar::paste_style(editor_model& model) {
  commit_result r;
  if (!clip_ || !model.selection()) {
    r.error = "no style was copied";
    return r;
  }
  const copied_style clip = std::move(*clip_);
  clip_.reset();
  const std::string type = model.selected_type();
  std::vector<std::pair<std::string, std::optional<std::string>>> changes;
  for (const editable_field& f : model.fields()) {
    const auto it =
        std::find_if(clip.fields.begin(), clip.fields.end(), [&](const auto& c) { return c.first == f.desc->name; });
    if (it == clip.fields.end() || !f.bound_to.empty() || (type_specific(it->first) && type != clip.type)) {
      continue;
    }
    if (it->second) {
      if (*it->second != f.source_text) {
        changes.emplace_back(it->first, it->second);
      }
    } else if (f.written) {
      changes.emplace_back(it->first, std::nullopt);
    }
  }
  if (changes.empty()) {
    r.ok = true;
    r.source = model.source();
    return r;
  }
  return model.set_fields(model.selection()->element, changes);
}

commit_result edit_toolbar::clear_style(editor_model& model) {
  commit_result r;
  if (!model.selection()) {
    r.error = "nothing is selected";
    return r;
  }
  std::vector<std::pair<std::string, std::optional<std::string>>> changes;
  for (const editable_field& f : model.fields()) {
    const bool styled = std::any_of(
        std::begin(kClearedFields), std::end(kClearedFields), [&](const char* n) { return f.desc->name == n; });
    if (styled && f.written && f.bound_to.empty()) {
      changes.emplace_back(f.desc->name, std::nullopt);
    }
  }
  if (changes.empty()) {
    r.ok = true;
    r.source = model.source();
    return r;
  }
  return model.set_fields(model.selection()->element, changes);
}

void edit_toolbar::note(const commit_result& r, inspector_events& events) {
  if (r.ok) {
    if (r.source != events.source.value_or(std::string())) {
      events.source = r.source;
    }
    events.paths.then(r.paths);
    return;
  }
  events.error = r.line > 0 ? "line " + std::to_string(r.line) + ": " + r.error : r.error;
}

bool edit_toolbar::draw_shape(ImDrawList* dl, const std::string& name, ImVec2 lo, ImVec2 hi, ImU32 line, ImU32 fill) {
  ImUmlContext* const previous = ImUml::GetCurrentContext();
  if (shapes_ == nullptr) {
    shapes_ = ImUml::CreateContext();
    ImUml::SetCurrentContext(shapes_);
    register_shapes();
  }
  ImUml::SetCurrentContext(shapes_);
  const std::string shape = library_shape(name);
  const float zoom = std::max(0.2f, (hi.y - lo.y) / 48.0f);
  const bool drawn =
      ImUml::HasShape(shape.c_str()) && ImUml::RenderShape(dl, shape.c_str(), lo, hi, line, fill, 1.2f, zoom);
  ImUml::SetCurrentContext(previous);
  return drawn;
}

void edit_toolbar::color_tool(
    editor_model& model,
    const editable_field& f,
    toolbar_tool tool,
    inspector_events& events) {
  const std::string& name = f.desc->name;
  const std::string current = std::get<std::string>(f.current);
  const std::string popup = "##palette_" + name;
  const bool clicked =
      icon_button(("##" + name).c_str(), tool_tooltip(tool), false, [&](ImDrawList* dl, ImVec2 lo, ImVec2 hi) {
        if (tool == toolbar_tool::fill_color) {
          draw_fill_icon(dl, lo, hi, current);
        } else if (tool == toolbar_tool::line_color) {
          draw_line_icon(dl, lo, hi, current);
        } else {
          draw_text_icon(dl, lo, hi, current);
        }
      });
  if (clicked) {
    open_below(popup.c_str());
    custom_key_.clear();
  }
  place_below_last_item();
  if (!ImGui::BeginPopup(popup.c_str())) {
    return;
  }
  ImGui::TextDisabled("%s", tool_tooltip(tool));
  // The palette: round swatches, the current colour ringed.
  const float swatch = ImGui::GetFrameHeight() * 0.9f;
  const auto& palette = toolbar_palette();
  for (size_t i = 0; i < palette.size(); ++i) {
    if (i % kPaletteColumns != 0) {
      ImGui::SameLine(0.0f, 6.0f);
    }
    ImGui::PushID(static_cast<int>(i));
    const ImVec2 lo = ImGui::GetCursorScreenPos();
    if (ImGui::InvisibleButton("##swatch", ImVec2(swatch, swatch))) {
      note(model.commit(name, *encode_color(palette[i])), events);
      ImGui::CloseCurrentPopup();
    }
    float rgba[4];
    parse_rgba(palette[i], rgba);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 c(lo.x + swatch * 0.5f, lo.y + swatch * 0.5f);
    const float r = swatch * 0.5f - 2.0f;
    if (same_color(current, palette[i])) {
      dl->AddCircle(c, r + 1.5f, ImGui::GetColorU32(ImGuiCol_CheckMark), 0, 2.5f);
    } else if (ImGui::IsItemHovered()) {
      dl->AddCircleFilled(c, r + 2.0f, ImGui::GetColorU32(ImGuiCol_ButtonHovered));
    }
    dl->AddCircleFilled(c, r - 1.0f, to_u32(rgba));
    dl->AddCircle(c, r - 1.0f, ImGui::GetColorU32(ImGuiCol_Border), 0, 1.0f);
    ImGui::SetItemTooltip("%s", palette[i]);
    ImGui::PopID();
  }
  ImGui::Spacing();
  if (ImGui::Button("Automatic")) {
    if (f.written) {
      note(model.clear(name), events);
    }
    ImGui::CloseCurrentPopup();
  }
  ImGui::SetItemTooltip("Remove the colour: the element takes its default (the theme's or its parent's).");
  ImGui::SameLine();
  if (ImGui::Button(custom_key_ == name ? "Custom colour  ^" : "Custom colour...")) {
    if (custom_key_ == name) {
      custom_key_.clear();
    } else {
      custom_key_ = name;
      if (!parse_rgba(current, custom_)) {
        parse_rgba("#2471A3", custom_);
      }
    }
  }
  if (custom_key_ == name) {
    ImGui::SetNextItemWidth(swatch * kPaletteColumns + 6.0f * (kPaletteColumns - 1));
    if (ImGui::ColorPicker4(
            "##custom",
            custom_,
            ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_DisplayHex |
                ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_NoSmallPreview)) {
      // Dragging in the picker is one undo step.
      const std::string key = model.selection()->element + "/" + name;
      note(model.commit(name, *encode_color(format_rgba(custom_)), drag_key_ == key), events);
      drag_key_ = key;
    }
  }
  ImGui::EndPopup();
}

void edit_toolbar::shape_tool(editor_model& model, const editable_field& f, inspector_events& events) {
  const field_desc& d = *f.desc;
  const std::string current = d.names->format_enum(std::get<int64_t>(f.current));
  const ImU32 line = ImGui::GetColorU32(ImGuiCol_Text);
  const ImU32 fill = ImGui::GetColorU32(ImGuiCol_FrameBg);
  const std::string tip = std::string("Shape: ") + current;
  const bool clicked = icon_button("##shape", tip.c_str(), false, [&](ImDrawList* dl, ImVec2 lo, ImVec2 hi) {
    const float s = hi.y - lo.y;
    if (!draw_shape(
            dl,
            current,
            ImVec2(lo.x + s * 0.2f, lo.y + s * 0.25f),
            ImVec2(hi.x - s * 0.2f, hi.y - s * 0.25f),
            line,
            0)) {
      draw_shapes_icon(dl, lo, hi);
    }
  });
  if (clicked) {
    open_below("##shapes");
  }
  place_below_last_item();
  ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(FLT_MAX, 340.0f));
  if (!ImGui::BeginPopup("##shapes")) {
    return;
  }
  const std::vector<name_entry>& entries = d.names->entries();
  const float cell = ImGui::GetFrameHeight() * 1.35f;
  const auto grid = [&](const std::vector<const name_entry*>& items) {
    for (size_t i = 0; i < items.size(); ++i) {
      const name_entry& e = *items[i];
      if (i % kPaletteColumns != 0) {
        ImGui::SameLine(0.0f, 4.0f);
      }
      ImGui::PushID(e.name.c_str());
      const bool chosen = e.name == current;
      const bool picked = icon_button(
          "##cell",
          e.name.c_str(),
          chosen,
          [&](ImDrawList* dl, ImVec2 lo, ImVec2 hi) {
            const float pad = cell * 0.16f;
            if (!draw_shape(dl, e.name, ImVec2(lo.x + pad, lo.y + pad), ImVec2(hi.x - pad, hi.y - pad), line, fill)) {
              const std::string abbr = e.name.substr(0, 3);
              const ImVec2 size = ImGui::CalcTextSize(abbr.c_str());
              dl->AddText(ImVec2((lo.x + hi.x - size.x) * 0.5f, (lo.y + hi.y - size.y) * 0.5f), line, abbr.c_str());
            }
          },
          cell);
      if (picked) {
        note(model.commit(d.name, encode_enum(d, e.value)), events);
        ImGui::CloseCurrentPopup();
      }
      ImGui::PopID();
    }
  };
  // The library's shapes come in tabs by category; other lists in one grid.
  std::vector<std::string> categories;
  std::map<std::string, std::vector<const name_entry*>> by_category;
  for (const name_entry& e : entries) {
    std::string category = shape_category(e.name);
    if (category.empty()) {
      by_category.clear();
      break;
    }
    if (!by_category.contains(category)) {
      categories.push_back(category);
    }
    by_category[category].push_back(&e);
  }
  if (!by_category.empty() && categories.size() > 1) {
    if (ImGui::BeginTabBar("##categories", ImGuiTabBarFlags_FittingPolicyScroll)) {
      for (const std::string& category : categories) {
        std::string label = category;
        label[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(label[0])));
        const bool has_current =
            std::any_of(by_category[category].begin(), by_category[category].end(), [&](const name_entry* e) {
              return e->name == current;
            });
        if (ImGui::BeginTabItem(
                label.c_str(),
                nullptr,
                has_current && ImGui::IsWindowAppearing() ? ImGuiTabItemFlags_SetSelected : 0)) {
          grid(by_category[category]);
          ImGui::EndTabItem();
        }
      }
      ImGui::EndTabBar();
    }
  } else {
    std::vector<const name_entry*> all;
    for (const name_entry& e : entries) {
      all.push_back(&e);
    }
    grid(all);
  }
  ImGui::EndPopup();
}

void edit_toolbar::choice_tool(editor_model& model, const editable_field& f, float width, inspector_events& events) {
  const field_desc& d = *f.desc;
  const int64_t current = std::get<int64_t>(f.current);
  ImGui::SetNextItemWidth(width);
  if (ImGui::BeginCombo(("##" + d.name).c_str(), d.names->format_enum(current).c_str())) {
    for (const name_entry& e : d.names->entries()) {
      if (ImGui::Selectable(e.name.c_str(), e.value == current)) {
        note(model.commit(d.name, encode_enum(d, e.value)), events);
      }
    }
    ImGui::EndCombo();
  }
  ImGui::SetItemTooltip("%s", d.name == "line" ? "Line style" : "Arrow heads");
}

void edit_toolbar::font_tool(editor_model& model, const editable_field& f, inspector_events& events) {
  const std::string& current = std::get<std::string>(f.current);
  const std::string shown = current.empty() ? "Font" : font_label(current);
  // As wide as the name, within reason: the toolbar re-centres.
  const float text = ImGui::CalcTextSize(shown.c_str()).x + ImGui::GetStyle().FramePadding.x * 2.0f;
  ImGui::SetNextItemWidth(
      std::clamp(text + ImGui::GetFrameHeight(), ImGui::GetFrameHeight() * 3.0f, ImGui::GetFrameHeight() * 7.0f));
  if (ImGui::BeginCombo("##font", shown.c_str(), ImGuiComboFlags_HeightLarge)) {
    if (ImGui::Selectable("(inherit)", current.empty()) && f.written) {
      note(model.clear("font"), events);
    }
    if (ImGui::Selectable("default", current == "default")) {
      note(model.commit("font", "default"), events);
    }
    for (const font_entry& e : font_catalog()) {
      if (ImGui::Selectable(font_label(e.name).c_str(), current == e.name)) {
        note(model.commit("font", encode_text(e.name)), events);
      }
    }
    ImGui::EndCombo();
  }
  ImGui::SetItemTooltip("%s", current.empty() ? "Font (inherited)" : "Font");
}

void edit_toolbar::number_tool(editor_model& model, const editable_field& f, inspector_events& events) {
  const field_desc& d = *f.desc;
  const bool is_int = d.type == field_type::integer;
  double v = is_int ? static_cast<double>(std::get<int64_t>(f.current)) : std::get<double>(f.current);
  number_box_options options;
  options.integer = is_int;
  if (d.limits) {
    options.min = d.limits->min;
    options.max = d.limits->max;
  }
  if (d.name == "font_size") {
    options.zero_text = "auto";
    options.zero_start = model.inherited_font_size();
  }
  const number_box_change changed = numbers_.draw(d.name.c_str(), v, options, ImGui::GetFrameHeight() * 2.1f);
  if (!numbers_.typing()) {
    ImGui::SetItemTooltip(
        "%s", d.name == "font_size" ? tool_tooltip(toolbar_tool::font_size) : tool_tooltip(toolbar_tool::line_weight));
  }
  if (changed != number_box_change::none) {
    const std::string key = model.selection()->element + "/" + d.name;
    const bool merge = changed == number_box_change::dragged && drag_key_ == key;
    note(model.commit(d.name, is_int ? encode_int(static_cast<int64_t>(v)) : encode_number(v), merge), events);
    drag_key_ = changed == number_box_change::dragged ? key : std::string();
  }
}

inspector_events edit_toolbar::draw(
    editor_model& model,
    tree_menu& menu,
    const layout_box& area,
    const std::optional<layout_box>& anchor) {
  inspector_events events;
  shown_ = false;
  if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
    drag_key_.clear(); // a drag ends with the button
  }
  if (!model.selection() || area.empty()) {
    rect_ = {};
    return events;
  }
  // A copied style goes to the next element selected.
  if (clip_ && model.selection()->element != clip_->from) {
    if (model.selection()->element == kImagePath) {
      clip_.reset();
    } else {
      note(paste_style(model), events);
    }
  }
  const std::vector<toolbar_tool> tools = tools_for(model);
  if (tools.empty()) {
    rect_ = {};
    return events;
  }
  const std::vector<editable_field> fields = model.fields();
  const auto field = [&](toolbar_tool tool) -> const editable_field* {
    for (const editable_field& f : fields) {
      if (f.desc->name == toolbar_field(tool)) {
        return &f;
      }
    }
    return nullptr;
  };

  const layout_box box = place(anchor, area, width_, height_);
  ImGui::SetNextWindowPos(ImVec2(box.x, box.y), ImGuiCond_Always);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 6.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f));
  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::GetStyleColorVec4(ImGuiCol_PopupBg));
  constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
      ImGuiWindowFlags_NoMove;
  ImGui::Begin("##nymph_toolbar", nullptr, kFlags);
  ImGui::PopStyleColor();
  ImGui::PopStyleVar(5);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f)); // the popups'
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 6.0f));

  const std::string path = model.selection()->element;
  int group = -1;
  for (const toolbar_tool tool : tools) {
    if (group >= 0) {
      if (tool_group(tool) != group) {
        group_separator();
      } else {
        ImGui::SameLine();
      }
    }
    group = tool_group(tool);
    const editable_field* f = field(tool);
    switch (tool) {
      case toolbar_tool::shape:
        shape_tool(model, *f, events);
        break;
      case toolbar_tool::fill_color:
      case toolbar_tool::line_color:
      case toolbar_tool::text_color:
        color_tool(model, *f, tool, events);
        break;
      case toolbar_tool::font:
        font_tool(model, *f, events);
        break;
      case toolbar_tool::font_size:
      case toolbar_tool::line_weight:
        number_tool(model, *f, events);
        break;
      case toolbar_tool::line:
      case toolbar_tool::arrow:
        choice_tool(model, *f, ImGui::GetFrameHeight() * 3.2f, events);
        break;
      case toolbar_tool::copy_style:
        if (icon_button("##copy_style", tool_tooltip(tool), clip_.has_value(), draw_roller_icon)) {
          if (clip_) {
            clip_.reset();
          } else {
            copy_style(model);
          }
        }
        break;
      case toolbar_tool::clear_style:
        if (icon_button("##clear_style", tool_tooltip(tool), false, draw_eraser_icon)) {
          note(clear_style(model), events);
        }
        break;
      case toolbar_tool::duplicate:
        if (icon_button("##duplicate", tool_tooltip(tool), false, draw_duplicate_icon)) {
          menu.duplicate(model, path, events);
        }
        break;
      case toolbar_tool::remove:
        if (icon_button("##remove", tool_tooltip(tool), false, draw_trash_icon)) {
          menu.remove(model, path, events);
        }
        break;
    }
  }
  ImGui::PopStyleVar(2);
  const ImVec2 size = ImGui::GetWindowSize();
  const ImVec2 pos = ImGui::GetWindowPos();
  ImGui::End();
  // A soft shadow under it, over the figure (the panel draws after it).
  ImDrawList* shadow = ImGui::GetBackgroundDrawList();
  for (int i = 1; i <= 4; ++i) {
    const float d = static_cast<float>(i) * 1.5f;
    shadow->AddRectFilled(
        ImVec2(pos.x - d + 1.0f, pos.y - d + 3.0f),
        ImVec2(pos.x + size.x + d - 1.0f, pos.y + size.y + d + 3.0f),
        IM_COL32(0, 0, 0, 14),
        10.0f + d);
  }
  width_ = size.x;
  height_ = size.y;
  rect_ = place(anchor, area, width_, height_);
  shown_ = true;
  return events;
}

} // namespace bdg::nymph
