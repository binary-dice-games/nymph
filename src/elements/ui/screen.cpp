// MIT License © 2026 Binary Dice Games
/// @file screen.cpp
/// @brief Screen and Window: real ImGui windows placed on an area of the
///        image (spec/09_ui_mockups/DESIGN.md sections 3 and 4).
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <map>
#include <memory>
#include <optional>
#include "core/error.hpp"
#include "core/suggest.hpp"
#include "elements/ui/ui_common.hpp"
#include "elements/ui/ui_flags.hpp"
#include "render/text_style.hpp"
#include "schema/registry.hpp"

namespace bdg::nymph::ui {

namespace {

/// Where cascaded windows start, and the step between them.
constexpr float kCascadeStart = 16.0f;
constexpr float kCascadeStep = 24.0f;
/// What a window without a fixed size counts for in a Screen's natural size.
constexpr float kAutoWindowWidth = 240.0f, kAutoWindowHeight = 160.0f;
/// Margin a Screen's natural size leaves after its windows.
constexpr float kScreenMargin = 16.0f;

/// Checks an optional `[x, y]` pair field.
void check_pair(const element_class& cls, const node& n, size_t index, double min, double max) {
  const auto& v = n.get<std::vector<double>>(index);
  if (v.empty()) {
    return;
  }
  if (v.size() != 2) {
    cls.fail_at(n, index, "must be two numbers [x, y], not " + std::to_string(v.size()));
  }
  for (const double d : v) {
    if (!(d >= min && d <= max)) {
      cls.fail_at(n, index, "values must be between " + pick_number(min) + " and " + pick_number(max));
    }
  }
}

// ── Window ───────────────────────────────────────────────────────────────────

namespace window_field {
constexpr size_t title = 0, x = 1, y = 2, width = 3, height = 4, collapsed = 5, closable = 6, flags = 7, scroll = 8,
                 id = 9;
}

class window_class : public element_class {
 public:
  window_class()
      : element_class(
            "Window",
            std::string(kWindowSlot),
            {std::string(kMenuBarSlot), std::string(kWidgetSlot), std::string(kFigureSlot)},
            with_text_style({
                text_field(
                    "title",
                    "",
                    "Text of the title bar. ImGui's \"##\" hides the rest from the title; window titles need not be "
                    "unique."),
                number_field(
                    "x",
                    0.0,
                    "Left edge relative to the Screen, pixels. Windows that write neither x nor y are cascaded from "
                    "(16, 16). Not allowed when the Window is placed by a layout (as root or in a layout).",
                    value_range{0, 8192}),
                number_field("y", 0.0, "Top edge relative to the Screen, pixels (see x).", value_range{0, 8192}),
                number_field(
                    "width",
                    0.0,
                    "Width: positive is pixels, 0 fits the content (ImGui's auto-fit), negative reaches the Screen's "
                    "right edge. As a layout element: the usual size hint.",
                    value_range{-1000, 8192}),
                number_field(
                    "height",
                    0.0,
                    "Height: positive is pixels, 0 fits the content, negative reaches the Screen's bottom edge. As a "
                    "layout element: the usual size hint.",
                    value_range{-1000, 8192}),
                bool_field("collapsed", false, "Shows only the title bar."),
                bool_field("closable", false, "Shows a close button; in a live view it hides the window."),
                flags_field(
                    "flags",
                    0,
                    window_flag_names(),
                    "ImGuiWindowFlags without the prefix, like NoResize|NoCollapse. A MenuBar child sets MenuBar."),
                numbers_field("scroll", "Initial scroll position [x, y] in pixels; empty for none."),
                text_field("id", "", "A name WindowTransitions refer to (from, to); unique on its Screen."),
            }),
            "An ImGui window: title bar, decorations and widgets. On a Screen it is placed at x, y; as root or in a "
            "layout it fills its rectangle.") {
    also_fits(std::string(kFigureSlot));
  }

  view_kind viewable() const override {
    return view_kind::ui_window;
  }

  void check(const node& n) const override {
    check_pair(*this, n, window_field::scroll, 0, 1e6);
    if (placed_as(n) == kFigureSlot) {
      check_opens(n, {}); // popups live on a Screen
    }
    if (placed_as(n) == kFigureSlot && (written(n, window_field::x) || written(n, window_field::y))) {
      fail_at(
          n,
          written(n, window_field::x) ? window_field::x : window_field::y,
          "is not allowed here: the layout places this Window (x and y apply on a Screen)");
    }
  }

  ImVec2 measure(const node&, draw_context&, float) const override {
    return ImVec2(kAutoWindowWidth + 160.0f, kAutoWindowHeight + 80.0f);
  }

  /// Position relative to the Screen: x, y, or the next cascade step.
  static ImVec2 position(const node& n, int cascade_index) {
    if (written(n, window_field::x) || written(n, window_field::y)) {
      return ImVec2(
          static_cast<float>(n.get<double>(window_field::x)), static_cast<float>(n.get<double>(window_field::y)));
    }
    const float at = kCascadeStart + kCascadeStep * static_cast<float>(cascade_index);
    return ImVec2(at, at);
  }

  /// Whether the window takes a cascade step (it writes neither x nor y).
  static bool cascades(const node& n) {
    return !written(n, window_field::x) && !written(n, window_field::y);
  }

  /// Checks that a positioned window fits a Screen of `size`.
  void check_fits(const node& n, ImVec2 pos, ImVec2 screen) const {
    const float w = static_cast<float>(n.get<double>(window_field::width));
    const float h = static_cast<float>(n.get<double>(window_field::height));
    if (w > 0.0f && pos.x + w > screen.x + 0.5f) {
      fail_at(
          n,
          window_field::width,
          "puts the window's right edge at x = " + pick_number(pos.x + w) + ", the Screen is " +
              pick_number(screen.x) + " wide");
    }
    if (h > 0.0f && pos.y + h > screen.y + 0.5f) {
      fail_at(
          n,
          window_field::height,
          "puts the window's bottom edge at y = " + pick_number(pos.y + h) + ", the Screen is " +
              pick_number(screen.y) + " high");
    }
    if (pos.x >= screen.x || pos.y >= screen.y) {
      fail_at(
          n,
          pos.x >= screen.x ? window_field::x : window_field::y,
          "places the window outside the Screen (" + pick_number(screen.x) + " x " + pick_number(screen.y) + ")");
    }
  }

  void draw(const node& n, draw_context& dc) const override {
    const bool as_figure = placed_as(n) == kFigureSlot;
    const std::string path = dc.path_of(n);
    ui_frame& frame = dc.ui().frame();

    ImVec2 area_min, area_max, pos, size;
    if (as_figure) {
      const rect r = dc.rect_of(n);
      area_min = r.min;
      area_max = r.max;
      size = ImVec2(r.width(), r.height());
    } else {
      area_min = frame.screen_min;
      area_max = frame.screen_max;
      pos = position(n, frame.cascade);
      if (cascades(n)) {
        ++frame.cascade;
      }
      const float w = static_cast<float>(n.get<double>(window_field::width));
      const float h = static_cast<float>(n.get<double>(window_field::height));
      size.x = w > 0.0f ? w : w < 0.0f ? std::max(1.0f, area_max.x - area_min.x - pos.x) : 0.0f;
      size.y = h > 0.0f ? h : h < 0.0f ? std::max(1.0f, area_max.y - area_min.y - pos.y) : 0.0f;
    }
    const bool collapsed = n.get<bool>(window_field::collapsed);
    const auto& scroll = n.get<std::vector<double>>(window_field::scroll);

    bool apply = false;
    ui_entry& e = dc.ui().entry(
        path,
        name(),
        std::vector<double>{pos.x, pos.y, size.x, size.y, collapsed ? 1.0 : 0.0},
        dc.apply_view(),
        apply);
    // What the user left (the `view` key) wins over the source while it applies.
    const element_view* v = apply && dc.apply_view() ? dc.view_of(n) : nullptr;
    if (v != nullptr && !as_figure) {
      if (v->position) {
        pos = ImVec2(static_cast<float>((*v->position)[0]), static_cast<float>((*v->position)[1]));
      }
      if (v->size) {
        size = ImVec2(static_cast<float>((*v->size)[0]), static_cast<float>((*v->size)[1]));
      }
    }
    const bool start_collapsed = v != nullptr && v->collapsed ? *v->collapsed : collapsed;
    if (apply) {
      e.closed = v != nullptr && v->closed && *v->closed;
      e.collapsed = start_collapsed;
    }
    if (e.closed) {
      return;
    }
    if (apply) {
      ImGui::SetNextWindowPos(ImVec2(area_min.x + pos.x, area_min.y + pos.y), ImGuiCond_Always);
      ImGui::SetNextWindowSize(size, ImGuiCond_Always);
      ImGui::SetNextWindowCollapsed(start_collapsed, ImGuiCond_Always);
      if (v != nullptr && v->scroll) {
        ImGui::SetNextWindowScroll(ImVec2(static_cast<float>((*v->scroll)[0]), static_cast<float>((*v->scroll)[1])));
      } else if (scroll.size() == 2) {
        ImGui::SetNextWindowScroll(ImVec2(static_cast<float>(scroll[0]), static_cast<float>(scroll[1])));
      }
    }
    if (!as_figure) {
      // A window never grows past the Screen's edge: an auto-fitted one
      // scrolls instead, and the user cannot resize one past it.
      const ImVec2 at = apply ? pos : e.pos;
      const ImVec2 room(
          std::max(32.0f, area_max.x - area_min.x - at.x), std::max(32.0f, area_max.y - area_min.y - at.y));
      ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), room);
    }

    ImGuiWindowFlags flags = static_cast<ImGuiWindowFlags>(n.get<int64_t>(window_field::flags)) |
        ImGuiWindowFlags_NoSavedSettings;
    for (const node& child : n.children) {
      if (child.cls->placed_as(child) == kMenuBarSlot) {
        flags |= ImGuiWindowFlags_MenuBar;
      }
    }
    if (as_figure) {
      flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
    }
    if (dc.edit_mode()) {
      flags |= ImGuiWindowFlags_NoMouseInputs; // a click selects (spec/08_view_editor) instead of pressing
    }
    std::optional<stock_window_style> style;
    if (as_figure) {
      style.emplace();
    }
    bool open = true;
    const bool closable = n.get<bool>(window_field::closable);
    const std::string window_name = n.get<std::string>(window_field::title) + "###" + path;
    const bool visible = ImGui::Begin(window_name.c_str(), closable ? &open : nullptr, flags);
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    dc.ui().note_id(window->ID, path);
    if (!as_figure && !apply) {
      // Moved by the user: kept inside the Screen (from the next frame on).
      const ImVec2 clamped(
          std::clamp(window->Pos.x, area_min.x, std::max(area_min.x, area_max.x - window->Size.x)),
          std::clamp(window->Pos.y, area_min.y, std::max(area_min.y, area_max.y - window->Size.y)));
      if (clamped.x != window->Pos.x || clamped.y != window->Pos.y) {
        ImGui::SetWindowPos(clamped);
      }
    }
    if (visible) {
      draw_children(n, dc);
    }
    const ImVec2 new_pos(window->Pos.x - area_min.x, window->Pos.y - area_min.y);
    if (!apply) {
      // Only what the user changed is saved: a move, a resize (dragging an
      // edge or the grip of this window), a scroll, a collapse.
      const ImGuiContext& g = *ImGui::GetCurrentContext();
      if (new_pos.x != e.pos.x || new_pos.y != e.pos.y) {
        e.moved = e.touched = true;
      }
      if ((window->SizeFull.x != e.size_full.x || window->SizeFull.y != e.size_full.y) && g.ActiveIdWindow == window &&
          ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        e.resized = e.touched = true;
      }
      if (window->Scroll.x != e.scroll.x || window->Scroll.y != e.scroll.y) {
        e.scrolled = e.touched = true;
      }
      if (e.collapsed != window->Collapsed) {
        e.collapse_changed = e.touched = true;
      }
    }
    e.pos = new_pos;
    e.size = window->Size;
    e.size_full = window->SizeFull;
    e.scroll = window->Scroll;
    e.collapsed = window->Collapsed;
    const rect area{window->Pos, ImVec2(window->Pos.x + window->Size.x, window->Pos.y + window->Size.y)};
    dc.add_input_area(area);
    if (dc.edit_mode()) {
      dc.record_target(n, {}, area, n.get<std::string>(window_field::title));
      if ((flags & ImGuiWindowFlags_NoTitleBar) == 0) {
        dc.record_target(
            n,
            "title",
            {window->Pos, ImVec2(window->Pos.x + window->Size.x, window->Pos.y + window->TitleBarHeight)},
            n.get<std::string>(window_field::title));
      }
    }
    ImGui::End();
    if (!open) {
      e.closed = true;
      e.touched = true;
    }
  }
};

// ── Screen ───────────────────────────────────────────────────────────────────

namespace screen_field {
constexpr size_t width = 0, height = 1, background = 2, window_rounding = 3, window_padding = 4, window_border = 5,
                 frame_rounding = 6, frame_padding = 7, frame_border = 8, item_spacing = 9;
}

class screen_class : public element_class {
 public:
  screen_class()
      : element_class(
            "Screen",
            std::string(kFigureSlot),
            {std::string(kWindowSlot), std::string(kFlowSlot)},
            with_text_style({
                number_field(
                    "width",
                    0.0,
                    "Width hint: 0 is the natural width (the windows' extent), a positive value fixed pixels, a "
                    "negative value a stretch weight in a HorizontalLayout. As root it fills the image.",
                    value_range{-1000, 8192}),
                number_field(
                    "height",
                    0.0,
                    "Height hint: 0 is the natural height, a positive value fixed pixels, a negative value a stretch "
                    "weight in a VerticalLayout. As root it fills the image.",
                    value_range{-1000, 8192}),
                color_field(
                    "background",
                    "Colour behind the windows; empty is the theme's window background shaded by 10 % (darker in light "
                    "themes, lighter in dark ones), so windows stand out."),
                number_field("window_rounding", 0.0, "Corner radius of windows, pixels.", value_range{0, 32}),
                number_field("window_padding", 8.0, "Space inside windows around their content, pixels.", value_range{0, 64}),
                bool_field("window_border", true, "Draws a border around windows."),
                number_field("frame_rounding", 0.0, "Corner radius of buttons, inputs and other framed widgets.", value_range{0, 32}),
                numbers_field("frame_padding", "Padding inside framed widgets [x, y]; empty is ImGui's [4, 3]."),
                bool_field("frame_border", false, "Draws a border around framed widgets."),
                numbers_field("item_spacing", "Space between widgets [x, y]; empty is ImGui's [8, 4]."),
            }),
            "An area where ImGui windows live, each at its own position, with their popups, menu bar and the "
            "transitions between them; usually the root.") {}

  void check(const node& n) const override {
    check_pair(*this, n, screen_field::frame_padding, 0, 64);
    check_pair(*this, n, screen_field::item_spacing, 0, 64);
    check_flow(n);
    check_popups(n);
    size_t count = 0;
    count_ui(n, count);
    if (count > kMaxUiElements) {
      throw error(n.line, n.column, "Screen has " + std::to_string(count) + " elements; at most " +
                                        std::to_string(kMaxUiElements) + " are allowed");
    }
  }

  ImVec2 measure(const node& n, draw_context&, float) const override {
    ImVec2 extent(0.0f, 0.0f);
    int cascade = 0;
    for (const node& child : n.children) {
      if (child.cls->name() != "Window") {
        continue;
      }
      const ImVec2 pos = window_class::position(child, cascade);
      if (window_class::cascades(child)) {
        ++cascade;
      }
      const float w = static_cast<float>(child.get<double>(window_field::width));
      const float h = static_cast<float>(child.get<double>(window_field::height));
      extent.x = std::max(extent.x, pos.x + (w > 0.0f ? w : kAutoWindowWidth));
      extent.y = std::max(extent.y, pos.y + (h > 0.0f ? h : kAutoWindowHeight));
    }
    return ImVec2(extent.x + kScreenMargin, extent.y + kScreenMargin);
  }

  void arrange(const node& n, draw_context&, const rect& r) const override {
    int cascade = 0;
    for (const node& child : n.children) {
      if (child.cls->name() != "Window") {
        continue;
      }
      const auto& window = static_cast<const window_class&>(*child.cls);
      const ImVec2 pos = window_class::position(child, cascade);
      if (window_class::cascades(child)) {
        ++cascade;
      }
      window.check_fits(child, pos, ImVec2(r.width(), r.height()));
    }
  }

  void draw(const node& n, draw_context& dc) const override {
    const rect r = dc.rect_of(n);
    ImGui::GetWindowDrawList()->AddRectFilled(r.min, r.max, background(n));

    ui_frame& frame = dc.ui().frame();
    const ui_frame saved = frame;
    frame.in_screen = true;
    frame.screen_min = r.min;
    frame.screen_max = r.max;
    frame.cascade = 0;

    const auto pair = [&](size_t index, ImVec2 fallback) {
      const auto& v = n.get<std::vector<double>>(index);
      return v.size() == 2 ? ImVec2(static_cast<float>(v[0]), static_cast<float>(v[1])) : fallback;
    };
    const float window_padding = static_cast<float>(n.get<double>(screen_field::window_padding));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(window_padding, window_padding));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, n.get<bool>(screen_field::window_border) ? 1.0f : 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, static_cast<float>(n.get<double>(screen_field::window_rounding)));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, static_cast<float>(n.get<double>(screen_field::frame_rounding)));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, n.get<bool>(screen_field::frame_border) ? 1.0f : 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, pair(screen_field::frame_padding, ImVec2(4.0f, 3.0f)));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, pair(screen_field::item_spacing, ImVec2(8.0f, 4.0f)));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, static_cast<float>(n.get<double>(screen_field::window_rounding)));
    // ImGui dims the whole display behind a modal; a PopupModal dims only
    // its Screen instead (menus.cpp).
    ImGui::GetStyle().Colors[ImGuiCol_ModalWindowDimBg].w = 0.0f;
    // Windows first; then popups, which open under the widgets that open
    // them in this frame; then the transitions between windows, from the
    // windows' rectangles of this frame. Each child under its index as ID,
    // as draw_children() does.
    for (const int pass : {0, 1, 2}) {
      if (pass == 2) {
        prepare_flow(n, dc);
      }
      for (size_t i = 0; i < n.children.size(); ++i) {
        const node& child = n.children[i];
        if (pass_of(child) != pass) {
          continue;
        }
        ImGui::PushID(static_cast<int>(i));
        dc.draw_node(child);
        ImGui::PopID();
      }
    }
    ImGui::PopStyleVar(8);
    frame = saved;
  }

 private:
  /// 0 for windows and menu bars, 1 for popups, 2 for transitions.
  static int pass_of(const node& child) {
    if (child.cls->placed_as(child) == kFlowSlot) {
      return 2;
    }
    return is_popup(child) ? 1 : 0;
  }

  static bool is_popup(const node& n) {
    return n.cls->name() == "Popup" || n.cls->name() == "PopupModal";
  }

  /// Popup ids are unique; every `opens` names one.
  static void check_popups(const node& n) {
    std::vector<std::string> ids;
    for (const node& child : n.children) {
      if (!is_popup(child)) {
        continue;
      }
      const size_t index = field(child, "id");
      const std::string& id = child.get<std::string>(index);
      if (id.empty()) {
        continue;
      }
      if (std::find(ids.begin(), ids.end(), id) != ids.end()) {
        child.cls->fail_at(child, index, "'" + id + "' is used by another popup on this Screen");
      }
      ids.push_back(id);
    }
    check_opens(n, ids);
    // ImGui keeps one stack of open popups: one open popup, or one open
    // menu of a menu bar, per Screen.
    const node* first = nullptr;
    for (const node& child : n.children) {
      if (is_popup(child) && child.get<bool>(field(child, "open"))) {
        note_open(child, field(child, "open"), first);
      }
    }
    find_open_menus(n, false, first);
  }

  /// Fails at `n`'s `open` field if another popup or menu is open already.
  static void note_open(const node& n, size_t index, const node*& first) {
    if (first != nullptr) {
      n.cls->fail_at(
          n,
          index,
          "is true, but a " + std::string(first->cls->name()) + " (line " + std::to_string(first->line) +
              ") is open already; ImGui shows one open popup or menu at a time");
    }
    first = &n;
  }

  /// Notes the open Menus of every menu bar under `n` (their own open
  /// submenus are one chain, checked by the menus).
  static void find_open_menus(const node& n, bool in_bar, const node*& first) {
    for (const node& child : n.children) {
      const bool bar = child.cls->name() == "MenuBar" || child.cls->name() == "MainMenuBar";
      if (in_bar && child.cls->name() == "Menu" && child.get<bool>(field(child, "open"))) {
        note_open(child, field(child, "open"), first);
      }
      if (!(in_bar && child.cls->name() == "Menu")) {
        find_open_menus(child, bar, first);
      }
    }
  }

  /// Window `id` field index, `from` / `to` of a WindowTransition.
  static size_t field(const node& n, std::string_view name) {
    return static_cast<size_t>(n.cls->field_index(name));
  }

  /// Unique window ids; every WindowTransition's `from` and `to` name one.
  static void check_flow(const node& n) {
    std::vector<std::string> ids;
    for (const node& child : n.children) {
      if (child.cls->name() != "Window") {
        continue;
      }
      const std::string& id = child.get<std::string>(window_field::id);
      if (id.empty()) {
        continue;
      }
      if (std::find(ids.begin(), ids.end(), id) != ids.end()) {
        child.cls->fail_at(child, window_field::id, "'" + id + "' is used by another Window on this Screen");
      }
      ids.push_back(id);
    }
    for (const node& child : n.children) {
      if (child.cls->placed_as(child) != kFlowSlot) {
        continue;
      }
      for (const char* end : {"from", "to"}) {
        const size_t index = field(child, end);
        const std::string& id = child.get<std::string>(index);
        if (std::find(ids.begin(), ids.end(), id) == ids.end()) {
          child.cls->fail_at(
              child,
              index,
              (id.empty() ? std::string("must name a Window id") : "names no Window id '" + id + "'") +
                  did_you_mean(id, ids) + (ids.empty() ? "; no Window on this Screen has an id" : "; ids: " + join(ids)));
        }
      }
    }
  }

  static std::string join(const std::vector<std::string>& names) {
    std::string out;
    for (const std::string& name : names) {
      out += (out.empty() ? "" : ", ") + name;
    }
    return out;
  }

  /// Fills the frame's window paths by id and the lane of every transition
  /// (transitions between the same two windows, either way, side by side).
  static void prepare_flow(const node& n, draw_context& dc) {
    ui_frame& frame = dc.ui().frame();
    frame.window_paths.clear();
    frame.lanes.clear();
    frame.screen_background = background(n);
    for (const node& child : n.children) {
      if (child.cls->name() == "Window" && !child.get<std::string>(window_field::id).empty()) {
        frame.window_paths[child.get<std::string>(window_field::id)] = dc.path_of(child);
      }
    }
    std::map<std::string, std::vector<std::string>> pairs; // "a\nb" (sorted) -> transition paths
    for (const node& child : n.children) {
      if (child.cls->placed_as(child) != kFlowSlot) {
        continue;
      }
      std::string from = child.get<std::string>(field(child, "from"));
      std::string to = child.get<std::string>(field(child, "to"));
      if (to < from) {
        std::swap(from, to);
      }
      pairs[from + "\n" + to].push_back(dc.path_of(child));
    }
    for (const auto& [key, paths] : pairs) {
      for (size_t i = 0; i < paths.size(); ++i) {
        frame.lanes[paths[i]] = {static_cast<int>(i), static_cast<int>(paths.size())};
      }
    }
  }

  static void count_ui(const node& n, size_t& count) {
    for (const node& child : n.children) {
      ++count;
      count_ui(child, count);
    }
  }

  /// The background colour: the field, or WindowBg shaded by 10 %.
  static ImU32 background(const node& n) {
    const std::string& color = n.get<std::string>(screen_field::background);
    if (!color.empty()) {
      return ImGui::ColorConvertFloat4ToU32(color_of(color));
    }
    ImVec4 bg = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
    const float luma = 0.299f * bg.x + 0.587f * bg.y + 0.114f * bg.z;
    const float target = luma > 0.5f ? 0.0f : 1.0f;
    bg.x += (target - bg.x) * 0.1f;
    bg.y += (target - bg.y) * 0.1f;
    bg.z += (target - bg.z) * 0.1f;
    bg.w = 1.0f;
    return ImGui::ColorConvertFloat4ToU32(bg);
  }
};

} // namespace

void register_flow(registry& reg);

void register_screen(registry& reg) {
  reg.add(std::make_unique<screen_class>());
  reg.add(std::make_unique<window_class>());
  register_flow(reg);
}

} // namespace bdg::nymph::ui
