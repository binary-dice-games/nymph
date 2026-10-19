// MIT License © 2026 Binary Dice Games
/// @file menus.cpp
/// @brief Menus, popups and tooltips (spec/09_ui_mockups/DESIGN.md section
///        4, "Popups shown in the PNG"): MainMenuBar, MenuBar, Menu,
///        MenuItem, Popup, PopupModal, Tooltip.
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cfloat>
#include <memory>
#include "core/error.hpp"
#include "elements/ui/ui_common.hpp"
#include "elements/ui/ui_flags.hpp"
#include "render/text_style.hpp"
#include "schema/registry.hpp"

namespace bdg::nymph::ui {

namespace {

/// What menus take: menus and items, and any widget (ImGui allows sliders
/// and checkboxes in menus).
std::vector<std::string> menu_slots() {
  return {std::string(kMenuSlot), std::string(kWidgetSlot)};
}

/// At most one Menu child of `n` may be shown open: ImGui keeps one open
/// menu per level.
void check_one_open_menu(const element_class& cls, const node& n) {
  const node* open = nullptr;
  for (const node& child : n.children) {
    if (child.cls->name() != "Menu") {
      continue;
    }
    const int index = child.cls->field_index("open");
    if (child.get<bool>(static_cast<size_t>(index))) {
      if (open != nullptr) {
        child.cls->fail_at(
            child, static_cast<size_t>(index), "is true for a second Menu of this " + std::string(cls.name()) +
                "; only one menu per level can be open");
      }
      open = &child;
    }
  }
}

// ── MenuBar / MainMenuBar ────────────────────────────────────────────────────

class menu_bar_class : public element_class {
 public:
  menu_bar_class()
      : element_class(
            "MenuBar",
            std::string(kMenuBarSlot),
            menu_slots(),
            {},
            "ImGui::BeginMenuBar: the menu bar of a Window or Child, holding Menus.") {}

  void check(const node& n) const override {
    check_one_open_menu(*this, n);
  }

  void draw(const node& n, draw_context& dc) const override {
    if (ImGui::BeginMenuBar()) {
      draw_children(n, dc);
      ImGui::EndMenuBar();
    }
  }
};

class main_menu_bar_class : public element_class {
 public:
  main_menu_bar_class()
      : element_class(
            "MainMenuBar",
            std::string(kWindowSlot),
            menu_slots(),
            {},
            "ImGui::BeginMainMenuBar: a menu bar across the top of the Screen (leave room for it above the windows).") {}

  void check(const node& n) const override {
    check_one_open_menu(*this, n);
  }

  void draw(const node& n, draw_context& dc) const override {
    const ui_frame& frame = dc.ui().frame();
    ImGui::SetNextWindowPos(frame.screen_min, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(frame.screen_max.x - frame.screen_min.x, ImGui::GetFrameHeight()), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(0.0f, 0.0f));
    constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;
    const std::string name = "##main_menu_bar###" + dc.path_of(n);
    const bool visible = ImGui::Begin(name.c_str(), nullptr, kFlags | (dc.edit_mode() ? ImGuiWindowFlags_NoMouseInputs : 0));
    ImGui::PopStyleVar(4); // the bar's own style; its menus take the Screen's
    if (visible) {
      if (ImGui::BeginMenuBar()) {
        draw_children(n, dc);
        ImGui::EndMenuBar();
      }
    }
    if (dc.edit_mode()) {
      dc.record_target(
          n,
          {},
          {ImGui::GetWindowPos(),
           ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowWidth(), ImGui::GetWindowPos().y + ImGui::GetWindowHeight())});
    }
    ImGui::End();
  }
};

// ── Menu / MenuItem ──────────────────────────────────────────────────────────

class menu_class : public widget_class {
 public:
  menu_class()
      : widget_class(
            "Menu",
            menu_slots(),
            {text_field("label", "", "Text of the menu; \"##\" hides the rest."),
             bool_field("open", false, "Shows the menu open, with its items (one open Menu per level)."),
             bool_field("enabled", true, "Whether the menu can be opened.")},
            "ImGui::BeginMenu: a menu in a MenuBar or in another Menu; its children are its items.",
            false) {
    set_primary_slot(std::string(kMenuSlot));
    set_view_kind(view_kind::ui_open);
  }

  void check(const node& n) const override {
    check_one_open_menu(*this, n);
  }

 protected:
  item_result draw_item(const node& n, draw_context& dc) const override {
    bool apply = false;
    ui_entry& e = state(n, dc, n.get<bool>(1), apply);
    const std::string& label = n.get<std::string>(0);
    const element_view* v = apply ? saved(n, dc) : nullptr;
    if (apply && (v != nullptr && v->flag ? *v->flag : n.get<bool>(1))) {
      ImGui::OpenPopup(label.c_str());
    }
    const bool open = ImGui::BeginMenu(label.c_str(), n.get<bool>(2));
    item_result r;
    if (!apply && open != e.flag) {
      e.touched = true;
    }
    e.flag = open;
    r.values = {{"open", open ? "true" : "false"}};
    finish_item(n, dc, r);
    r.finished = true;
    if (open) {
      draw_children(n, dc);
      ImGui::EndMenu();
    }
    return r;
  }
};

class menu_item_class : public widget_class {
 public:
  menu_item_class()
      : widget_class(
            "MenuItem",
            {},
            {text_field("label", "", "Text of the item; \"##\" hides the rest."),
             text_field("shortcut", "", "Shortcut text shown on the right (\"Ctrl+S\"); it does nothing."),
             bool_field("checked", false, "Shows a check mark; when written, a click in a live view toggles it."),
             bool_field("enabled", true, "Whether the item can be chosen.")},
            "ImGui::MenuItem: an item of a Menu; a click reports the item and closes the menu.",
            false) {
    set_primary_slot(std::string(kMenuSlot));
    set_view_kind(view_kind::ui_checked);
  }

 protected:
  item_result draw_item(const node& n, draw_context& dc) const override {
    bool apply = false;
    ui_entry& e = state(n, dc, n.get<bool>(2), apply);
    if (apply) {
      const element_view* v = saved(n, dc);
      e.flag = v != nullptr && v->flag ? *v->flag : n.get<bool>(2);
    }
    const bool checkable = written(n, 2);
    const std::string& shortcut = n.get<std::string>(1);
    item_result r;
    r.clicked = ImGui::MenuItem(
        n.get<std::string>(0).c_str(),
        shortcut.empty() ? nullptr : shortcut.c_str(),
        checkable ? &e.flag : nullptr,
        n.get<bool>(3));
    if (r.clicked && checkable) {
      e.touched = true;
    }
    if (checkable) {
      r.values = {{"checked", e.flag ? "true" : "false"}};
    }
    return r;
  }
};

// ── Popup / PopupModal ───────────────────────────────────────────────────────

namespace popup_field {
constexpr size_t id = 0, open = 1, x = 2, y = 3, flags = 4, title = 5, closable = 6;
}

class popup_class : public element_class {
 public:
  explicit popup_class(bool modal)
      : element_class(
            modal ? "PopupModal" : "Popup",
            std::string(kWindowSlot),
            {std::string(kWidgetSlot), std::string(kMenuSlot), std::string(kFigureSlot)},
            fields(modal),
            modal ? "ImGui::BeginPopupModal: a dialog over the Screen, which dims the rest; shown when open is true "
                    "or a widget that opens it is clicked."
                  : "ImGui::BeginPopup: a small window shown when open is true or a widget that opens it is clicked; "
                    "a click outside closes it in a live view."),
        modal_(modal) {}

  view_kind viewable() const override {
    return view_kind::ui_open;
  }

  void draw(const node& n, draw_context& dc) const override {
    ui_frame& frame = dc.ui().frame();
    const std::string path = dc.path_of(n);
    const std::string& id = n.get<std::string>(popup_field::id);
    const std::string name = (modal_ ? n.get<std::string>(popup_field::title) : std::string()) + "###popup " + path;
    const bool source_open = n.get<bool>(popup_field::open);
    bool apply = false;
    ui_entry& e = dc.ui().entry(path, this->name(), source_open, dc.apply_view(), apply);
    const bool requested = !id.empty() &&
        std::find(frame.popups_to_open.begin(), frame.popups_to_open.end(), id) != frame.popups_to_open.end();
    const element_view* v = apply && dc.apply_view() ? dc.view_of(n) : nullptr;
    const bool opening = (apply && (v != nullptr && v->flag ? *v->flag : source_open)) || requested;
    if (opening) {
      ImGui::OpenPopup(name.c_str());
    }
    if (modal_ && ImGui::IsPopupOpen(name.c_str())) {
      dim_screen(frame, path);
    }
    if (opening) {
      place(n, frame, id);
    }
    const auto flags = static_cast<ImGuiWindowFlags>(n.get<int64_t>(popup_field::flags)) |
        ImGuiWindowFlags_NoSavedSettings | (dc.edit_mode() ? ImGuiWindowFlags_NoMouseInputs : 0);
    bool keep = true;
    const bool visible = modal_
        ? ImGui::BeginPopupModal(name.c_str(), n.get<bool>(popup_field::closable) ? &keep : nullptr, flags)
        : ImGui::BeginPopup(name.c_str(), flags);
    if (visible) {
      draw_children(n, dc);
      const rect area{
          ImGui::GetWindowPos(),
          ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowWidth(), ImGui::GetWindowPos().y + ImGui::GetWindowHeight())};
      dc.add_input_area(area);
      if (dc.edit_mode()) {
        dc.record_target(n, {}, area, modal_ ? n.get<std::string>(popup_field::title) : id);
      }
      ImGui::EndPopup();
    }
    if (!apply && visible != e.flag) {
      e.touched = true;
    }
    e.flag = visible;
  }

 private:
  static std::vector<field_desc> fields(bool modal) {
    std::vector<field_desc> f{
        text_field("id", "", "The name widgets refer to in their opens field; unique on its Screen."),
        bool_field("open", false, "Shows the popup (in the PNG too)."),
        number_field(
            "x",
            0.0,
            "Left edge relative to the Screen when written; else under the widget that opens it, else (modals) "
            "centred.",
            value_range{0, 8192}),
        number_field("y", 0.0, "Top edge relative to the Screen (see x).", value_range{0, 8192}),
        flags_field("flags", 0, window_flag_names(), "ImGuiWindowFlags without the prefix."),
    };
    if (modal) {
      f.push_back(text_field("title", "", "Text of the dialog's title bar."));
      f.push_back(bool_field("closable", false, "Shows a close button in the title bar."));
    }
    return with_text_style(std::move(f));
  }

  /// Sets where the popup opens.
  void place(const node& n, const ui_frame& frame, const std::string& id) const {
    if (written(n, popup_field::x) || written(n, popup_field::y)) {
      ImGui::SetNextWindowPos(
          ImVec2(
              frame.screen_min.x + static_cast<float>(n.get<double>(popup_field::x)),
              frame.screen_min.y + static_cast<float>(n.get<double>(popup_field::y))),
          ImGuiCond_Always);
      return;
    }
    const auto opener = frame.openers.find(id);
    if (opener != frame.openers.end()) {
      ImGui::SetNextWindowPos(ImVec2(opener->second.first.x, opener->second.second.y), ImGuiCond_Always);
      return;
    }
    if (modal_) {
      ImGui::SetNextWindowPos(
          ImVec2((frame.screen_min.x + frame.screen_max.x) * 0.5f, (frame.screen_min.y + frame.screen_max.y) * 0.5f),
          ImGuiCond_Always,
          ImVec2(0.5f, 0.5f));
      return;
    }
    ImGui::SetNextWindowPos(ImVec2(frame.screen_min.x + 16.0f, frame.screen_min.y + 16.0f), ImGuiCond_Always);
  }

  /// A window over the Screen only, in the theme's dim colour, under the
  /// modal: ImGui's own dim covers the whole display (and is switched off
  /// by the Screen).
  static void dim_screen(const ui_frame& frame, const std::string& path) {
    const ImVec4 bg = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
    const bool dark = 0.299f * bg.x + 0.587f * bg.y + 0.114f * bg.z < 0.5f;
    const ImVec4 dim = dark ? ImVec4(0.8f, 0.8f, 0.8f, 0.35f) : ImVec4(0.2f, 0.2f, 0.2f, 0.35f);
    ImGui::SetNextWindowPos(frame.screen_min, ImGuiCond_Always);
    ImGui::SetNextWindowSize(
        ImVec2(frame.screen_max.x - frame.screen_min.x, frame.screen_max.y - frame.screen_min.y), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, dim);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoMove;
    const std::string name = "##dim###dim " + path;
    ImGui::Begin(name.c_str(), nullptr, kFlags);
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
  }

  bool modal_;
};

// ── Tooltip ──────────────────────────────────────────────────────────────────

class tooltip_class : public widget_class {
 public:
  tooltip_class()
      : widget_class(
            "Tooltip",
            widget_container_slots(),
            {bool_field("open", false, "Shows the tooltip (in the PNG too); else it shows while the widget before it is hovered.")},
            "ImGui::BeginItemTooltip: a tooltip window for the widget just before it, holding any widgets.",
            false) {}

 protected:
  item_result draw_item(const node& n, draw_context& dc) const override {
    item_result r;
    r.finished = true; // not an item; it describes the previous one
    const ImVec2 item_max = ImGui::GetItemRectMax();
    const ImVec2 item_min = ImGui::GetItemRectMin();
    const bool open = n.get<bool>(0);
    const bool hovered = dc.interactive() && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip);
    if (!open && !hovered) {
      return r;
    }
    if (open) {
      ImGui::SetNextWindowPos(ImVec2(item_min.x + 8.0f, item_max.y + 4.0f), ImGuiCond_Always);
    }
    if (ImGui::BeginTooltip()) {
      draw_children(n, dc);
      ImGui::EndTooltip();
    }
    return r;
  }
};

} // namespace

void register_menus(registry& reg) {
  reg.add(std::make_unique<menu_bar_class>());
  reg.add(std::make_unique<main_menu_bar_class>());
  reg.add(std::make_unique<menu_class>());
  reg.add(std::make_unique<menu_item_class>());
  reg.add(std::make_unique<popup_class>(false));
  reg.add(std::make_unique<popup_class>(true));
  reg.add(std::make_unique<tooltip_class>());
}

} // namespace bdg::nymph::ui
