// MIT License © 2026 Binary Dice Games
/// @file containers.cpp
/// @brief Widgets that hold widgets (TreeNode, CollapsingHeader, TabBar /
///        TabItem, Child, Group, Indent, Disabled) and the layout helpers
///        (SameLine, NewLine, Spacing, Dummy) (spec/09_ui_mockups/DESIGN.md
///        section 3, "Widgets").
#include <imgui.h>
#include <imgui_internal.h>
#include <memory>
#include "elements/ui/ui_common.hpp"
#include "elements/ui/ui_flags.hpp"
#include "schema/registry.hpp"

namespace bdg::nymph::ui {

namespace {

field_desc label_field(std::string description = "Text of the header; \"##\" hides the rest (ImGui labels).") {
  return text_field("label", "", std::move(description));
}

/// Containers take widgets and figure elements; a Child also a MenuBar.
std::vector<std::string> child_slots(bool menu_bar = false) {
  std::vector<std::string> slots = widget_container_slots();
  if (menu_bar) {
    slots.insert(slots.begin(), std::string(kMenuBarSlot));
  }
  return slots;
}

ImVec2 pair_of(const node& n, size_t index, ImVec2 fallback) {
  const auto& v = n.get<std::vector<double>>(index);
  return v.size() == 2 ? ImVec2(static_cast<float>(v[0]), static_cast<float>(v[1])) : fallback;
}

void check_pair(const element_class& cls, const node& n, size_t index, double min, double max, const char* what) {
  const auto& v = n.get<std::vector<double>>(index);
  if (v.empty()) {
    return;
  }
  if (v.size() != 2) {
    cls.fail_at(n, index, std::string("must be two numbers ") + what + ", not " + std::to_string(v.size()));
  }
  for (const double d : v) {
    if (!(d >= min && d <= max)) {
      cls.fail_at(n, index, "values must be between " + pick_number(min) + " and " + pick_number(max));
    }
  }
}

// ── TreeNode / CollapsingHeader ──────────────────────────────────────────────

class tree_node_class : public widget_class {
 public:
  explicit tree_node_class(bool header)
      : widget_class(
            header ? "CollapsingHeader" : "TreeNode",
            child_slots(),
            {label_field(),
             bool_field("open", false, "Whether it is expanded, showing its children."),
             flags_field(
                 "flags",
                 0,
                 tree_node_flag_names(),
                 "ImGuiTreeNodeFlags without the prefix, like Leaf|Bullet; DefaultOpen opens it when open is not "
                 "written.")},
            header ? "ImGui::CollapsingHeader: a full-width header that shows or hides its children."
                   : "ImGui::TreeNode: an expandable node of a tree; its children are indented under it.",
            false),
        header_(header) {
    set_view_kind(view_kind::ui_open);
  }

 protected:
  item_result draw_item(const node& n, draw_context& dc) const override {
    bool apply = false;
    ui_entry& e = state(n, dc, n.get<bool>(1), apply);
    const auto flags = static_cast<ImGuiTreeNodeFlags>(n.get<int64_t>(2));
    if (apply) {
      // `open`, else ImGui's DefaultOpen flag; what the user left wins.
      const element_view* v = saved(n, dc);
      const bool source_open = written(n, 1) ? n.get<bool>(1) : (flags & ImGuiTreeNodeFlags_DefaultOpen) != 0;
      e.flag = v != nullptr && v->flag ? *v->flag : source_open;
      ImGui::SetNextItemOpen(e.flag, ImGuiCond_Always);
    }
    const char* label = n.get<std::string>(0).c_str();
    const bool open = header_ ? ImGui::CollapsingHeader(label, flags) : ImGui::TreeNodeEx(label, flags);
    item_result r;
    if (open != e.flag) {
      e.flag = open;
      e.touched = true;
      r.clicked = true;
    }
    r.values = {{"open", open ? "true" : "false"}};
    finish_item(n, dc, r);
    r.finished = true;
    if (open) {
      draw_children(n, dc);
      if (!header_ && (flags & ImGuiTreeNodeFlags_NoTreePushOnOpen) == 0) {
        ImGui::TreePop();
      }
    }
    return r;
  }

 private:
  bool header_;
};

// ── TabBar / TabItem / TabItemButton ─────────────────────────────────────────

class tab_bar_class : public widget_class {
 public:
  tab_bar_class()
      : widget_class(
            "TabBar",
            {std::string(kTabSlot)},
            {text_field("label", "", "ID of the tab bar (not shown)."),
             flags_field("flags", 0, tab_bar_flag_names(), "ImGuiTabBarFlags without the prefix, like Reorderable.")},
            "ImGui::BeginTabBar: a row of tabs (TabItem children); the selected tab shows its children.",
            false) {}

 protected:
  item_result draw_item(const node& n, draw_context& dc) const override {
    const std::string& label = n.get<std::string>(0);
    item_result r;
    r.finished = true; // the bar is not an item; its tabs are
    if (ImGui::BeginTabBar(label.empty() ? "##tabs" : label.c_str(), static_cast<ImGuiTabBarFlags>(n.get<int64_t>(1)))) {
      draw_children(n, dc);
      ImGui::EndTabBar();
    }
    return r;
  }
};

class tab_item_class : public widget_class {
 public:
  tab_item_class()
      : widget_class(
            "TabItem",
            child_slots(),
            {label_field("Text of the tab; \"##\" hides the rest."),
             bool_field("selected", false, "Shows this tab; without one selected, the first tab is shown."),
             bool_field("closable", false, "Shows a close button on the tab; in a live view it removes the tab."),
             flags_field("flags", 0, tab_item_flag_names(), "ImGuiTabItemFlags without the prefix.")},
            "ImGui::BeginTabItem: one tab of a TabBar; its children show while it is selected.",
            false) {
    // A tab goes in a TabBar, not among widgets.
    set_primary_slot(std::string(kTabSlot));
    set_view_kind(view_kind::ui_selected);
  }

 protected:
  item_result draw_item(const node& n, draw_context& dc) const override {
    bool apply = false;
    ui_entry& e = state(n, dc, n.get<bool>(1), apply);
    const element_view* v = apply ? saved(n, dc) : nullptr;
    if (apply) {
      e.closed = v != nullptr && v->closed && *v->closed;
    }
    item_result r;
    r.finished = true;
    if (e.closed) {
      return r;
    }
    ImGuiTabItemFlags flags = static_cast<ImGuiTabItemFlags>(n.get<int64_t>(3));
    if (apply && (v != nullptr && v->flag ? *v->flag : n.get<bool>(1))) {
      flags |= ImGuiTabItemFlags_SetSelected;
    }
    bool open = true;
    const bool visible = ImGui::BeginTabItem(n.get<std::string>(0).c_str(), n.get<bool>(2) ? &open : nullptr, flags);
    if (!apply && visible != e.flag) {
      e.touched = true;
      r.clicked = visible;
    }
    e.flag = visible;
    r.values = {{"selected", visible ? "true" : "false"}};
    finish_item(n, dc, r);
    if (visible) {
      draw_children(n, dc);
      ImGui::EndTabItem();
    }
    if (!open) {
      e.closed = true;
      e.touched = true;
    }
    return r;
  }
};

class tab_item_button_class : public widget_class {
 public:
  tab_item_button_class()
      : widget_class(
            "TabItemButton",
            {},
            {label_field("Text of the button tab."),
             flags_field("flags", 0, tab_item_flag_names(), "ImGuiTabItemFlags without the prefix, like Trailing.")},
            "ImGui::TabItemButton: a tab that acts as a button (a \"+\" to add a tab).",
            false) {
    set_primary_slot(std::string(kTabSlot));
  }

 protected:
  item_result draw_item(const node& n, draw_context&) const override {
    item_result r;
    r.clicked =
        ImGui::TabItemButton(n.get<std::string>(0).c_str(), static_cast<ImGuiTabItemFlags>(n.get<int64_t>(1)));
    return r;
  }
};

// ── Child ────────────────────────────────────────────────────────────────────

class child_class : public widget_class {
 public:
  child_class()
      : widget_class(
            "Child",
            child_slots(true),
            {text_field("label", "", "ID of the region (not shown)."),
             numbers_field(
                 "size",
                 "Size [width, height]: positive is pixels, 0 fills the space left (or fits the content with an "
                 "AutoResize flag), negative stops that far before the edge. Empty is [0, 0]."),
             flags_field("child_flags", 0, child_flag_names(), "ImGuiChildFlags without the prefix, like Borders."),
             flags_field("flags", 0, window_flag_names(), "ImGuiWindowFlags without the prefix."),
             numbers_field("scroll", "Initial scroll position [x, y] in pixels; empty for none.")},
            "ImGui::BeginChild: a scrolling region inside a window.",
            false) {
    set_view_kind(view_kind::ui_scroll);
  }

  void check(const node& n) const override {
    check_pair(*this, n, 1, -4096, 4096, "[width, height]");
    check_pair(*this, n, 4, 0, 1e6, "[x, y]");
  }

 protected:
  item_result draw_item(const node& n, draw_context& dc) const override {
    bool apply = false;
    ui_entry& e = state(n, dc, n.values[4], apply);
    const auto& scroll = n.get<std::vector<double>>(4);
    const element_view* v = apply ? saved(n, dc) : nullptr;
    if (v != nullptr && v->scroll) {
      ImGui::SetNextWindowScroll(ImVec2(static_cast<float>((*v->scroll)[0]), static_cast<float>((*v->scroll)[1])));
    } else if (apply && scroll.size() == 2) {
      ImGui::SetNextWindowScroll(ImVec2(static_cast<float>(scroll[0]), static_cast<float>(scroll[1])));
    }
    ImGuiWindowFlags flags = static_cast<ImGuiWindowFlags>(n.get<int64_t>(3));
    if (dc.edit_mode()) {
      flags |= ImGuiWindowFlags_NoMouseInputs;
    }
    for (const node& child : n.children) {
      if (child.cls->placed_as(child) == kMenuBarSlot) {
        flags |= ImGuiWindowFlags_MenuBar;
      }
    }
    const std::string& label = n.get<std::string>(0);
    item_result r;
    r.finished = true;
    if (ImGui::BeginChild(
            label.empty() ? "##child" : label.c_str(),
            pair_of(n, 1, ImVec2(0.0f, 0.0f)),
            static_cast<ImGuiChildFlags>(n.get<int64_t>(2)),
            flags)) {
      draw_children(n, dc);
    }
    const ImVec2 now = ImGui::GetCurrentWindow()->Scroll;
    if (!apply && (now.x != e.scroll.x || now.y != e.scroll.y)) {
      e.scrolled = true;
      e.touched = true;
    }
    e.scroll = now;
    const rect area{ImGui::GetWindowPos(), ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowWidth(), ImGui::GetWindowPos().y + ImGui::GetWindowHeight())};
    ImGui::EndChild();
    dc.add_input_area(area);
    if (dc.edit_mode()) {
      dc.record_target(n, {}, area, label);
    }
    return r;
  }
};

// ── Group, Indent, Disabled ──────────────────────────────────────────────────

class group_class : public widget_class {
 public:
  group_class()
      : widget_class(
            "Group",
            child_slots(),
            {},
            "ImGui::BeginGroup: its children act as one item, so a SameLine after it puts the next widget beside the "
            "whole group.",
            false) {}

 protected:
  item_result draw_item(const node& n, draw_context& dc) const override {
    ImGui::BeginGroup();
    draw_children(n, dc);
    ImGui::EndGroup();
    item_result r;
    r.has_id = false; // EndGroup may give the group a child's ID
    return r;
  }
};

class indent_class : public widget_class {
 public:
  indent_class()
      : widget_class(
            "Indent",
            child_slots(),
            {number_field("width", 0.0, "Indent in pixels; 0 is the style's IndentSpacing.", value_range{0, 1024})},
            "ImGui::Indent: its children start further right.",
            false) {}

 protected:
  item_result draw_item(const node& n, draw_context& dc) const override {
    const float w = static_cast<float>(n.get<double>(0));
    ImGui::Indent(w);
    draw_children(n, dc);
    ImGui::Unindent(w);
    item_result r;
    r.finished = true;
    return r;
  }
};

class disabled_class : public widget_class {
 public:
  disabled_class()
      : widget_class(
            "Disabled",
            child_slots(),
            {},
            "ImGui::BeginDisabled: its children are greyed out and take no input.",
            false) {}

 protected:
  item_result draw_item(const node& n, draw_context& dc) const override {
    ImGui::BeginDisabled(true);
    draw_children(n, dc);
    ImGui::EndDisabled();
    item_result r;
    r.finished = true;
    return r;
  }
};

// ── SameLine, NewLine, Spacing, Dummy ────────────────────────────────────────

class same_line_class : public widget_class {
 public:
  same_line_class()
      : widget_class(
            "SameLine",
            {},
            {number_field(
                 "offset", 0.0, "Position from the left of the window; 0 follows the previous widget.", value_range{0, 8192}),
             number_field("spacing", -1.0, "Space after the previous widget; -1 is the style's.", value_range{-1, 1024})},
            "ImGui::SameLine: the next widget goes on the same line as the previous one.",
            false) {}

 protected:
  item_result draw_item(const node& n, draw_context&) const override {
    ImGui::SameLine(static_cast<float>(n.get<double>(0)), static_cast<float>(n.get<double>(1)));
    item_result r;
    r.finished = true;
    return r;
  }
};

class spacing_class : public widget_class {
 public:
  explicit spacing_class(bool new_line)
      : widget_class(
            new_line ? "NewLine" : "Spacing",
            {},
            {},
            new_line ? "ImGui::NewLine: ends a line of SameLine widgets, or adds an empty line."
                     : "ImGui::Spacing: a little vertical space.",
            false),
        new_line_(new_line) {}

 protected:
  item_result draw_item(const node&, draw_context&) const override {
    if (new_line_) {
      ImGui::NewLine();
    } else {
      ImGui::Spacing();
    }
    item_result r;
    r.finished = true;
    return r;
  }

 private:
  bool new_line_;
};

class dummy_class : public widget_class {
 public:
  dummy_class()
      : widget_class(
            "Dummy",
            {},
            {numbers_field("size", "Size [width, height] of the empty space, pixels.")},
            "ImGui::Dummy: empty space of a given size.",
            false) {}

  void check(const node& n) const override {
    check_pair(*this, n, 0, 0, 8192, "[width, height]");
  }

 protected:
  item_result draw_item(const node& n, draw_context&) const override {
    ImGui::Dummy(pair_of(n, 0, ImVec2(0.0f, 0.0f)));
    item_result r;
    r.has_id = false;
    return r;
  }
};

} // namespace

void register_containers(registry& reg) {
  reg.add(std::make_unique<tree_node_class>(false));
  reg.add(std::make_unique<tree_node_class>(true));
  reg.add(std::make_unique<tab_bar_class>());
  reg.add(std::make_unique<tab_item_class>());
  reg.add(std::make_unique<tab_item_button_class>());
  reg.add(std::make_unique<child_class>());
  reg.add(std::make_unique<group_class>());
  reg.add(std::make_unique<indent_class>());
  reg.add(std::make_unique<disabled_class>());
  reg.add(std::make_unique<same_line_class>());
  reg.add(std::make_unique<spacing_class>(false));
  reg.add(std::make_unique<spacing_class>(true));
  reg.add(std::make_unique<dummy_class>());
}

} // namespace bdg::nymph::ui
