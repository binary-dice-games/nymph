// MIT License © 2026 Binary Dice Games
/// @file widgets.cpp
/// @brief Text and button widgets (spec/09_ui_mockups/DESIGN.md section 3,
///        "Widgets").
#include <imgui.h>
#include <cfloat>
#include <memory>
#include "elements/ui/ui_common.hpp"
#include "elements/ui/ui_flags.hpp"
#include "schema/registry.hpp"

namespace bdg::nymph::ui {

namespace {

field_desc label_field(std::string description = "Text shown on the widget; \"##\" hides the rest (ImGui labels).") {
  return text_field("label", "", std::move(description));
}

field_desc size_field() {
  return numbers_field(
      "size",
      "Size [width, height] in pixels (ImGui's size argument): 0 on an axis fits the label, a negative value stops "
      "that far before the right or bottom edge (-1 reaches it); empty fits both.");
}

/// `size` as an ImVec2 ({0, 0} when empty; -1 is ImGui's -FLT_MIN, "to the edge").
ImVec2 size_of(const node& n, size_t index) {
  const auto& v = n.get<std::vector<double>>(index);
  if (v.size() != 2) {
    return ImVec2(0.0f, 0.0f);
  }
  const auto axis = [](double d) { return d == -1.0 ? -FLT_MIN : static_cast<float>(d); };
  return ImVec2(axis(v[0]), axis(v[1]));
}

/// Checks a `size` pair: empty or two numbers from -4096 to 4096.
void check_size(const element_class& cls, const node& n, size_t index) {
  const auto& v = n.get<std::vector<double>>(index);
  if (v.empty()) {
    return;
  }
  if (v.size() != 2) {
    cls.fail_at(n, index, "must be two numbers [width, height], not " + std::to_string(v.size()));
  }
  for (const double d : v) {
    if (!(d >= -4096.0 && d <= 4096.0)) {
      cls.fail_at(n, index, "values must be between -4096 and 4096");
    }
  }
}

// ── Text ─────────────────────────────────────────────────────────────────────

class text_class : public widget_class {
 public:
  text_class()
      : widget_class(
            "Text",
            {},
            {text_field("text", "", "The text, shown as written (never a printf format).")},
            "ImGui::Text: a line of text.",
            false) {}

  std::string label_of(const node& n) const override {
    return n.get<std::string>(0);
  }

 protected:
  item_result draw_item(const node& n, draw_context&) const override {
    ImGui::TextUnformatted(n.get<std::string>(0).c_str());
    return {};
  }
};

/// TextColored, TextDisabled, TextWrapped, BulletText.
enum class text_kind { colored, disabled, wrapped, bullet };

class text_variant_class : public widget_class {
 public:
  explicit text_variant_class(text_kind kind)
      : widget_class(name_of(kind), {}, fields(kind), description(kind), false), kind_(kind) {}

  std::string label_of(const node& n) const override {
    return n.get<std::string>(0);
  }

 protected:
  item_result draw_item(const node& n, draw_context&) const override {
    const char* text = n.get<std::string>(0).c_str();
    switch (kind_) {
      case text_kind::colored: {
        const std::string& color = n.get<std::string>(1);
        if (!color.empty()) {
          ImGui::PushStyleColor(ImGuiCol_Text, color_of(color));
        }
        ImGui::TextUnformatted(text);
        if (!color.empty()) {
          ImGui::PopStyleColor();
        }
        break;
      }
      case text_kind::disabled:
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextUnformatted(text);
        ImGui::PopStyleColor();
        break;
      case text_kind::wrapped:
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        break;
      case text_kind::bullet:
        ImGui::BulletText("%s", text);
        break;
    }
    return {};
  }

 private:
  static std::string name_of(text_kind kind) {
    switch (kind) {
      case text_kind::colored:
        return "TextColored";
      case text_kind::disabled:
        return "TextDisabled";
      case text_kind::wrapped:
        return "TextWrapped";
      case text_kind::bullet:
        return "BulletText";
    }
    return {};
  }

  static std::vector<field_desc> fields(text_kind kind) {
    std::vector<field_desc> f{text_field("text", "", "The text, shown as written (never a printf format).")};
    if (kind == text_kind::colored) {
      f.push_back(color_field("color", "Colour of the text; empty is the theme's."));
    }
    return f;
  }

  static std::string description(text_kind kind) {
    switch (kind) {
      case text_kind::colored:
        return "ImGui::TextColored: text in a colour.";
      case text_kind::disabled:
        return "ImGui::TextDisabled: greyed-out text.";
      case text_kind::wrapped:
        return "ImGui::TextWrapped: text wrapped at the window's edge.";
      case text_kind::bullet:
        return "ImGui::BulletText: text after a bullet.";
    }
    return {};
  }

  text_kind kind_;
};

class label_text_class : public widget_class {
 public:
  label_text_class()
      : widget_class(
            "LabelText",
            {},
            {label_field("Text on the right, like a widget's label."), text_field("text", "", "The value shown on the left.")},
            "ImGui::LabelText: a read-only value with a label, aligned like other widgets.") {}

 protected:
  item_result draw_item(const node& n, draw_context&) const override {
    ImGui::LabelText(n.get<std::string>(0).c_str(), "%s", n.get<std::string>(1).c_str());
    return {};
  }
};

class text_link_class : public widget_class {
 public:
  text_link_class()
      : widget_class(
            "TextLink",
            {},
            {label_field("The link text.")},
            "ImGui::TextLink: underlined text that can be clicked; it reports a click and never opens a URL.",
            false) {}

 protected:
  item_result draw_item(const node& n, draw_context&) const override {
    item_result r;
    r.clicked = ImGui::TextLink(n.get<std::string>(0).c_str());
    return r;
  }
};

class bullet_class : public widget_class {
 public:
  bullet_class() : widget_class("Bullet", {}, {}, "ImGui::Bullet: a bullet point; the next widget follows on the same line.", false) {}

 protected:
  item_result draw_item(const node&, draw_context&) const override {
    ImGui::Bullet();
    return {};
  }
};

// ── Button ───────────────────────────────────────────────────────────────────

class button_class : public widget_class {
 public:
  button_class()
      : widget_class("Button", {}, {label_field(), size_field()}, "ImGui::Button: a push button.", false) {}

  void check(const node& n) const override {
    check_size(*this, n, 1);
  }

 protected:
  item_result draw_item(const node& n, draw_context&) const override {
    item_result r;
    r.clicked = ImGui::Button(n.get<std::string>(0).c_str(), size_of(n, 1));
    return r;
  }
};

class small_button_class : public widget_class {
 public:
  small_button_class()
      : widget_class("SmallButton", {}, {label_field()}, "ImGui::SmallButton: a button without vertical padding.", false) {}

 protected:
  item_result draw_item(const node& n, draw_context&) const override {
    item_result r;
    r.clicked = ImGui::SmallButton(n.get<std::string>(0).c_str());
    return r;
  }
};

class arrow_button_class : public widget_class {
 public:
  arrow_button_class()
      : widget_class(
            "ArrowButton",
            {},
            {text_field("label", "", "ID of the button (not shown)."),
             enum_field("direction", ImGuiDir_Right, direction_names(), "Where the arrow points.")},
            "ImGui::ArrowButton: a square button with an arrow.",
            false) {}

 protected:
  item_result draw_item(const node& n, draw_context&) const override {
    item_result r;
    const std::string id = n.get<std::string>(0).empty() ? std::string("##arrow") : n.get<std::string>(0);
    r.clicked = ImGui::ArrowButton(id.c_str(), static_cast<ImGuiDir>(n.get<int64_t>(1)));
    return r;
  }
};

class progress_bar_class : public widget_class {
 public:
  progress_bar_class()
      : widget_class(
            "ProgressBar",
            {},
            {number_field("fraction", 0.0, "How far along, 0 to 1.", value_range{0, 1}),
             numbers_field(
                 "size",
                 "Size [width, height] in pixels; a negative width fills the row less that many pixels, empty fills "
                 "the row at the frame height."),
             text_field("overlay", "", "Text on the bar; empty shows the percentage.")},
            "ImGui::ProgressBar: a bar filled to a fraction.",
            false) {}

  void check(const node& n) const override {
    const auto& v = n.get<std::vector<double>>(1);
    if (!v.empty() && (v.size() != 2 || !(v[0] >= -4096 && v[0] <= 4096 && v[1] >= 0 && v[1] <= 4096))) {
      fail_at(n, 1, "must be [width, height]: width -4096 to 4096, height 0 to 4096");
    }
  }

 protected:
  item_result draw_item(const node& n, draw_context&) const override {
    const auto& v = n.get<std::vector<double>>(1);
    ImVec2 size(-FLT_MIN, 0.0f);
    if (v.size() == 2) {
      size = ImVec2(v[0] == -1.0 ? -FLT_MIN : static_cast<float>(v[0]), static_cast<float>(v[1]));
    }
    const std::string& overlay = n.get<std::string>(2);
    ImGui::ProgressBar(static_cast<float>(n.get<double>(0)), size, overlay.empty() ? nullptr : overlay.c_str());
    item_result r;
    r.values = {{"fraction", pick_number(n.get<double>(0))}};
    return r;
  }
};

// ── Checkbox ─────────────────────────────────────────────────────────────────

class checkbox_class : public widget_class {
 public:
  checkbox_class()
      : widget_class(
            "Checkbox",
            {},
            {label_field(), bool_field("value", false, "Whether the box is ticked.")},
            "ImGui::Checkbox: a box that can be ticked.",
            false) {
    set_view_kind(view_kind::ui_flag);
  }

 protected:
  item_result draw_item(const node& n, draw_context& dc) const override {
    const bool source = n.get<bool>(1);
    bool apply = false;
    ui_entry& e = state(n, dc, source, apply);
    if (apply) {
      const element_view* v = saved(n, dc);
      e.flag = v != nullptr && v->flag ? *v->flag : source;
    }
    item_result r;
    if (ImGui::Checkbox(n.get<std::string>(0).c_str(), &e.flag)) {
      e.touched = true;
      r.clicked = true;
    }
    r.values = {{"value", e.flag ? "true" : "false"}};
    return r;
  }
};

// ── RadioButton ──────────────────────────────────────────────────────────────

class radio_button_class : public widget_class {
 public:
  radio_button_class()
      : widget_class(
            "RadioButton",
            {},
            {label_field(), bool_field("active", false, "Whether this button is the chosen one.")},
            "ImGui::RadioButton: one choice of several. In a live view a click chooses it and clears the other "
            "RadioButtons of the same parent.",
            false) {
    set_view_kind(view_kind::ui_active);
  }

 protected:
  item_result draw_item(const node& n, draw_context& dc) const override {
    bool apply = false;
    ui_entry& e = state(n, dc, n.get<bool>(1), apply);
    if (apply) {
      const element_view* v = saved(n, dc);
      e.flag = v != nullptr && v->flag ? *v->flag : n.get<bool>(1);
    }
    item_result r;
    if (ImGui::RadioButton(n.get<std::string>(0).c_str(), e.flag)) {
      r.clicked = true;
      if (!e.flag) {
        choose(dc, dc.path_of(n));
      }
    }
    r.values = {{"active", e.flag ? "true" : "false"}};
    return r;
  }

 private:
  /// Makes `path` the active RadioButton among its siblings.
  static void choose(draw_context& dc, const std::string& path) {
    const std::string parent = path.substr(0, path.rfind("/children/") + std::string("/children/").size());
    for (auto& [other, entry] : dc.ui().mutable_entries()) {
      const bool sibling = other.size() > parent.size() && other.compare(0, parent.size(), parent) == 0 &&
          other.find('/', parent.size()) == std::string::npos;
      if (sibling && entry.type == "RadioButton") {
        if (entry.flag != (other == path)) {
          entry.flag = other == path;
          entry.touched = true;
        }
      }
    }
  }
};

// ── Combo, ListBox, Selectable ───────────────────────────────────────────────

/// The items as C strings (ImGui's Combo and ListBox take an array).
std::vector<const char*> item_pointers(const std::vector<std::string>& items) {
  std::vector<const char*> out;
  out.reserve(items.size());
  for (const std::string& item : items) {
    out.push_back(item.c_str());
  }
  return out;
}

class choice_class : public widget_class {
 public:
  explicit choice_class(bool list_box)
      : widget_class(
            list_box ? "ListBox" : "Combo",
            {},
            fields(list_box),
            list_box ? "ImGui::ListBox: a framed list of items, one selected."
                     : "ImGui::Combo: a box showing the selected item that opens a list of the others."),
        list_box_(list_box) {
    set_view_kind(view_kind::ui_index);
  }

  void check(const node& n) const override {
    const auto& items = n.get<std::vector<std::string>>(1);
    const int64_t selected = n.get<int64_t>(2);
    // With no items there is nothing to select, whatever `selected` says.
    if (!items.empty() && selected >= static_cast<int64_t>(items.size())) {
      fail_at(n, 2, "is " + std::to_string(selected) + " but there are " + std::to_string(items.size()) + " items");
    }
  }

 protected:
  item_result draw_item(const node& n, draw_context& dc) const override {
    bool apply = false;
    ui_entry& e = state(n, dc, n.values[2], apply);
    const auto& items = n.get<std::vector<std::string>>(1);
    if (apply) {
      const element_view* v = saved(n, dc);
      e.index = items.empty() ? -1 : static_cast<int>(v != nullptr && v->index ? *v->index : n.get<int64_t>(2));
      if (e.index >= static_cast<int>(items.size())) {
        e.index = -1; // a saved index the list no longer has
      }
    }
    const std::vector<const char*> pointers = item_pointers(items);
    const int height = static_cast<int>(n.get<int64_t>(3));
    item_result r;
    bool changed = false;
    if (list_box_) {
      changed = ImGui::ListBox(n.get<std::string>(0).c_str(), &e.index, pointers.data(), static_cast<int>(pointers.size()), height);
    } else {
      changed = ImGui::Combo(n.get<std::string>(0).c_str(), &e.index, pointers.data(), static_cast<int>(pointers.size()), height);
    }
    if (changed) {
      e.touched = true;
      r.clicked = true;
    }
    r.values = {{"selected", pick_number(e.index)}};
    if (e.index >= 0 && e.index < static_cast<int>(items.size())) {
      r.values.emplace_back("item", items[static_cast<size_t>(e.index)]);
    }
    return r;
  }

 private:
  static std::vector<field_desc> fields(bool list_box) {
    return {
        label_field(),
        texts_field("items", "The items, or a $column whose cells are the items."),
        int_field("selected", list_box ? -1 : 0, "Index of the selected item, from 0; -1 for none.", value_range{-1, 1e6}),
        int_field(
            "height_in_items",
            -1,
            list_box ? "Rows shown before scrolling; -1 is ImGui's default." : "Rows of the open list; -1 is ImGui's default.",
            value_range{-1, 1000}),
    };
  }

  bool list_box_;
};

class selectable_class : public widget_class {
 public:
  selectable_class()
      : widget_class(
            "Selectable",
            {},
            {label_field(),
             bool_field("selected", false, "Whether it is highlighted as selected."),
             flags_field("flags", 0, selectable_flag_names(), "ImGuiSelectableFlags without the prefix."),
             numbers_field("size", "Size [width, height] in pixels; 0 on an axis fills the row or fits the text.")},
            "ImGui::Selectable: a line of text that highlights when selected.",
            false) {
    set_view_kind(view_kind::ui_selected);
  }

  void check(const node& n) const override {
    check_size(*this, n, 3);
  }

 protected:
  item_result draw_item(const node& n, draw_context& dc) const override {
    bool apply = false;
    ui_entry& e = state(n, dc, n.get<bool>(1), apply);
    if (apply) {
      const element_view* v = saved(n, dc);
      e.flag = v != nullptr && v->flag ? *v->flag : n.get<bool>(1);
    }
    item_result r;
    if (ImGui::Selectable(
            n.get<std::string>(0).c_str(), &e.flag, static_cast<ImGuiSelectableFlags>(n.get<int64_t>(2)), size_of(n, 3))) {
      e.touched = true;
      r.clicked = true;
    }
    r.values = {{"selected", e.flag ? "true" : "false"}};
    return r;
  }
};

} // namespace

void register_widgets(registry& reg) {
  reg.add(std::make_unique<text_class>());
  reg.add(std::make_unique<button_class>());
  reg.add(std::make_unique<checkbox_class>());
  for (const text_kind kind : {text_kind::colored, text_kind::disabled, text_kind::wrapped, text_kind::bullet}) {
    reg.add(std::make_unique<text_variant_class>(kind));
  }
  reg.add(std::make_unique<label_text_class>());
  reg.add(std::make_unique<text_link_class>());
  reg.add(std::make_unique<bullet_class>());
  reg.add(std::make_unique<small_button_class>());
  reg.add(std::make_unique<arrow_button_class>());
  reg.add(std::make_unique<progress_bar_class>());
  reg.add(std::make_unique<radio_button_class>());
  reg.add(std::make_unique<choice_class>(false));
  reg.add(std::make_unique<choice_class>(true));
  reg.add(std::make_unique<selectable_class>());
}

} // namespace bdg::nymph::ui
