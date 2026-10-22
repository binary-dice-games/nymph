// MIT License © 2026 Binary Dice Games
/// @file editor_panel.cpp
/// @brief The editor panel's ImGui context and its frames.
#include "window/editor_panel.hpp"
#include <imgui_impl_opengl3.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <utility>
#include "edit/image_fields.hpp"
#include "render/frame_setup.hpp"

namespace bdg::nymph {

namespace {

constexpr const char* kSaveDialog = "Save changes";
constexpr const char* kCloseDialog = "Unsaved changes";

/// Makes the panel's context current for a scope and puts the previous one
/// back.
class panel_context_scope {
 public:
  explicit panel_context_scope(ImGuiContext* context) : previous_(ImGui::GetCurrentContext()) {
    ImGui::SetCurrentContext(context);
  }
  ~panel_context_scope() {
    ImGui::SetCurrentContext(previous_);
  }
  panel_context_scope(const panel_context_scope&) = delete;
  panel_context_scope& operator=(const panel_context_scope&) = delete;

 private:
  ImGuiContext* previous_;
};

/// The panel's look beyond the figure theme's colours: rounder, roomier
/// controls and softer borders. Only the panel's own context uses it; the
/// figure and rendered images keep the theme as is.
void apply_panel_style(ImGuiStyle& style) {
  style.WindowRounding = 8.0f;
  style.ChildRounding = 6.0f;
  style.FrameRounding = 6.0f;
  style.PopupRounding = 8.0f;
  style.GrabRounding = 6.0f;
  style.ScrollbarRounding = 8.0f;
  style.TabRounding = 6.0f;
  style.WindowPadding = ImVec2(10.0f, 8.0f);
  style.FramePadding = ImVec2(10.0f, 6.0f);
  style.ItemSpacing = ImVec2(8.0f, 7.0f);
  style.ItemInnerSpacing = ImVec2(6.0f, 5.0f);
  style.IndentSpacing = 18.0f;
  style.ScrollbarSize = 14.0f;
  style.GrabMinSize = 12.0f;
  style.WindowBorderSize = 0.0f;
  style.ChildBorderSize = 1.0f;
  style.PopupBorderSize = 1.0f;
  style.FrameBorderSize = 0.0f;
  style.TabBarBorderSize = 1.0f;
  style.SeparatorTextBorderSize = 2.0f;
  style.SeparatorTextPadding = ImVec2(16.0f, 4.0f);
  style.WindowMenuButtonPosition = ImGuiDir_Right;
  style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
}

/// Softens the theme's frame borders and gives hovered/pressed buttons a
/// clear step, whatever the theme.
void soften_colors(ImGuiStyle& style) {
  ImVec4* c = style.Colors;
  c[ImGuiCol_Border].w = std::min(c[ImGuiCol_Border].w, 0.45f);
  c[ImGuiCol_Separator].w = std::min(c[ImGuiCol_Separator].w, 0.5f);
  c[ImGuiCol_PopupBg].w = 1.0f;
}

} // namespace

ImGuiKey imgui_key(window_key key) {
  switch (key) {
    case window_key::escape: return ImGuiKey_Escape;
    case window_key::enter: return ImGuiKey_Enter;
    case window_key::tab: return ImGuiKey_Tab;
    case window_key::backspace: return ImGuiKey_Backspace;
    case window_key::del: return ImGuiKey_Delete;
    case window_key::left: return ImGuiKey_LeftArrow;
    case window_key::right: return ImGuiKey_RightArrow;
    case window_key::up: return ImGuiKey_UpArrow;
    case window_key::down: return ImGuiKey_DownArrow;
    case window_key::home: return ImGuiKey_Home;
    case window_key::end: return ImGuiKey_End;
    case window_key::page_up: return ImGuiKey_PageUp;
    case window_key::page_down: return ImGuiKey_PageDown;
    case window_key::a: return ImGuiKey_A;
    case window_key::c: return ImGuiKey_C;
    case window_key::d: return ImGuiKey_D;
    case window_key::v: return ImGuiKey_V;
    case window_key::x: return ImGuiKey_X;
    case window_key::y: return ImGuiKey_Y;
    case window_key::z: return ImGuiKey_Z;
    default: return ImGuiKey_None;
  }
}

editor_panel::editor_panel(std::string source, std::string file_name, const char* glsl_version)
    : model_(std::move(source)), file_name_(std::move(file_name)) {
  const panel_context_scope scope(nullptr);
  context_ = ImGui::CreateContext();
  ImGui::SetCurrentContext(context_);
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.LogFilename = nullptr;
  ImGui::GetStyle().FontSizeBase = 15.0f;
  apply_panel_style(ImGui::GetStyle());
  set_theme("light");
  ImGui_ImplOpenGL3_Init(glsl_version);
}

editor_panel::~editor_panel() {
  const panel_context_scope scope(context_);
  ImGui_ImplOpenGL3_Shutdown();
  ImGui::DestroyContext(context_);
}

void editor_panel::set_theme(const std::string& name) {
  if (name == theme_) {
    return;
  }
  const theme* t = find_theme(name);
  if (t == nullptr) {
    return;
  }
  theme_ = name;
  const panel_context_scope scope(context_);
  t->imgui(&ImGui::GetStyle()); // the colours of the figure's theme
  soften_colors(ImGui::GetStyle());
  refresh();
}

void editor_panel::resize(float width, float height, float scale) {
  width_ = width;
  height_ = height;
  scale_ = scale;
  refresh();
}

void editor_panel::pointer(pointer_event::kind type, float x, float y, int button, float wheel_x, float wheel_y) {
  const panel_context_scope scope(context_);
  ImGuiIO& io = ImGui::GetIO();
  if (type == pointer_event::kind::leave) {
    io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
  } else {
    io.AddMousePosEvent(x, y);
  }
  if ((type == pointer_event::kind::down || type == pointer_event::kind::up) && button >= 0 &&
      button < ImGuiMouseButton_COUNT) {
    io.AddMouseButtonEvent(button, type == pointer_event::kind::down);
  } else if (type == pointer_event::kind::wheel) {
    io.AddMouseWheelEvent(wheel_x, wheel_y);
  }
  refresh();
}

void editor_panel::key(window_key key, key_mods mods, bool down, bool repeat) {
  const panel_context_scope scope(context_);
  ImGuiIO& io = ImGui::GetIO();
  io.AddKeyEvent(ImGuiMod_Ctrl, mods.ctrl);
  io.AddKeyEvent(ImGuiMod_Shift, mods.shift);
  io.AddKeyEvent(ImGuiMod_Alt, mods.alt);
  const ImGuiKey k = imgui_key(key);
  if (k != ImGuiKey_None) {
    if (repeat) {
      io.AddKeyEvent(k, false); // a new press: ImGui makes its own repeats from a held key
    }
    io.AddKeyEvent(k, down);
  }
  refresh();
}

void editor_panel::text(char32_t codepoint) {
  const panel_context_scope scope(context_);
  ImGui::GetIO().AddInputCharacter(static_cast<unsigned>(codepoint));
  refresh();
}

inspector_events editor_panel::shortcut(window_key key, key_mods mods) {
  inspector_events events;
  const std::optional<edit_target_id>& selection = model_.selection();
  if (!selection || selection->element == kImagePath || selection->element == "root") {
    return events;
  }
  const std::string path = selection->element;
  tree_menu& menu = inspector_.menu();
  if (key == window_key::del && !mods.ctrl && !mods.alt) {
    menu.remove(model_, path, events);
  } else if (key == window_key::d && mods.ctrl && !mods.alt) {
    menu.duplicate(model_, path, events);
  } else if ((key == window_key::up || key == window_key::down) && mods.alt && !mods.ctrl) {
    menu.move(model_, path, key == window_key::up ? -1 : +1, events);
  }
  refresh();
  return events;
}

void editor_panel::ask_save() {
  asked_ = ask::save;
  refresh();
}

bool editor_panel::ask_close() {
  if (!model_.modified()) {
    return false;
  }
  asked_ = ask::close;
  refresh();
  return true;
}

bool editor_panel::wants_frame() const {
  return frames_left_ > 0 || inspector_.pending();
}

inspector_events editor_panel::frame(double now_seconds, const std::vector<edit_element>& tree, const std::string& status) {
  const panel_context_scope scope(context_);
  ImGuiIO& io = ImGui::GetIO();
  io.DisplaySize = ImVec2(width_, height_);
  io.DisplayFramebufferScale = ImVec2(scale_, scale_);
  const double step = last_time_ ? now_seconds - *last_time_ : 1.0 / 60.0;
  io.DeltaTime = static_cast<float>(std::clamp(step, 1e-4, 0.25));
  last_time_ = now_seconds;
  ImGui::GetStyle().FontScaleDpi = scale_;

  ImGui_ImplOpenGL3_NewFrame();
  ImGui::NewFrame();
  const workspace_layout regions = layout();
  constexpr ImGuiWindowFlags kFixed = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
      ImGuiWindowFlags_NoBringToFrontOnFocus;
  inspector_events table_events;
  draw_tabs(table_events);
  if (tab_) {
    // A data table in place of the figure.
    ImGui::SetNextWindowPos(ImVec2(regions.left.x, regions.left.y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(regions.left.width, regions.left.height), ImGuiCond_Always);
    ImGui::Begin("##nymph_table", nullptr, kFixed);
    grid_.draw(model_, *tab_, table_events);
    ImGui::End();
  }
  ImGui::SetNextWindowPos(ImVec2(regions.strip.x, 0.0f), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(regions.strip.width, height_), ImGuiCond_Always);
  ImGui::Begin("##nymph_editor", nullptr, kFixed);
  inspector_events events = inspector_.draw(model_, tree, status);
  draw_dialogs();
  grid_.dialogs(model_, table_events);
  draw_table_dialogs(table_events);
  ImGui::End();
  // A table change: shown like any other (the inspector's own come first).
  if (table_events.source && !events.source) {
    events.source = table_events.source;
  }
  if (!table_events.error.empty()) {
    events.error = table_events.error;
    inspector_.set_error(table_events.error);
  } else if (table_events.source) {
    inspector_.set_error("");
  }
  if (events.show_data) {
    show_tab(events.show_data->first);
    grid_.show_column(events.show_data->first, events.show_data->second);
  }
  // A tab whose table went away (an undo, a rename) falls back to the chart.
  if (tab_) {
    const std::vector<data_table> tables = model_.tables();
    if (std::none_of(tables.begin(), tables.end(), [&](const data_table& t) { return t.name == *tab_; })) {
      tab_.reset();
      select_tab_ = true;
    }
  }
  wants_keyboard_ = io.WantTextInput;
  modal_open_ = ImGui::IsPopupOpen(kSaveDialog) || ImGui::IsPopupOpen(kCloseDialog);
  popup_open_ = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
  ImGui::Render();
  if (frames_left_ > 0) {
    --frames_left_;
  }
  return events;
}

void editor_panel::show_tab(std::optional<std::string> name) {
  tab_ = std::move(name);
  select_tab_ = true;
  refresh();
}

void editor_panel::draw_tabs(inspector_events& events) {
  const workspace_layout regions = layout();
  ImGui::SetNextWindowPos(ImVec2(regions.tabs.x, regions.tabs.y), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(regions.tabs.width, regions.tabs.height), ImGuiCond_Always);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 4.0f));
  ImGui::Begin(
      "##nymph_tabs", nullptr,
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove |
          ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar);
  ImGui::PopStyleVar();
  if (ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_FittingPolicyScroll)) {
    const bool select = select_tab_;
    select_tab_ = false;
    if (ImGui::BeginTabItem("Chart", nullptr, select && !tab_ ? ImGuiTabItemFlags_SetSelected : 0)) {
      if (!select) {
        tab_.reset();
      }
      ImGui::EndTabItem();
    }
    for (const data_table& t : model_.tables()) {
      const std::string label = (t.name.empty() ? std::string("data") : t.name) + "##table_" + t.name;
      const bool chosen = select && tab_ && *tab_ == t.name;
      if (ImGui::BeginTabItem(label.c_str(), nullptr, chosen ? ImGuiTabItemFlags_SetSelected : 0)) {
        if (!select) {
          tab_ = t.name;
        }
        ImGui::EndTabItem();
      }
      if (ImGui::BeginPopupContextItem(("##tab_menu_" + t.name).c_str())) {
        ImGui::BeginDisabled(t.name.empty());
        if (ImGui::MenuItem("Rename")) {
          renaming_ = t.name;
          std::snprintf(first_name_, sizeof(first_name_), "%s", t.name.c_str());
          table_ask_ = table_ask::rename;
          open_table_dialog_ = true;
        }
        ImGui::EndDisabled();
        if (t.name.empty()) {
          ImGui::SetItemTooltip("The single table has no name; add a second table to name both.");
        }
        if (ImGui::MenuItem("Import CSV")) {
          grid_.ask_import(t.name);
        }
        if (ImGui::MenuItem("Export CSV")) {
          grid_.ask_export(t.name);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Delete")) {
          const commit_result r = model_.remove_table(t.name);
          if (r.ok) {
            events.source = r.source;
          } else {
            events.error = r.error;
          }
        }
        ImGui::EndPopup();
      }
    }
    if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing | ImGuiTabItemFlags_NoTooltip)) {
      const std::vector<data_table> tables = model_.tables();
      if (tables.size() == 1 && tables.front().name.empty()) {
        // The single table needs a name too: ask for both.
        std::snprintf(first_name_, sizeof(first_name_), "table1");
        std::snprintf(second_name_, sizeof(second_name_), "table2");
        table_ask_ = table_ask::name_both;
        open_table_dialog_ = true;
      } else {
        const std::string name = tables.empty() ? std::string() : model_.fresh_table_name();
        const commit_result r = model_.add_table(name, editor_model::starter_table());
        if (r.ok) {
          events.source = r.source;
          tab_ = name;
          select_tab_ = true;
        } else {
          events.error = r.error;
        }
      }
    }
    ImGui::SetItemTooltip("Add a data table");
    ImGui::EndTabBar();
  }
  ImGui::End();
}

void editor_panel::draw_table_dialogs(inspector_events& events) {
  constexpr const char* kNameBoth = "Name the tables";
  constexpr const char* kRename = "Rename table";
  if (open_table_dialog_) {
    ImGui::OpenPopup(table_ask_ == table_ask::rename ? kRename : kNameBoth);
    open_table_dialog_ = false;
  }
  const ImVec2 middle(width_ * 0.5f, height_ * 0.5f);
  const ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;
  const bool escape = ImGui::IsKeyPressed(ImGuiKey_Escape, false);
  ImGui::SetNextWindowPos(middle, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  if (ImGui::BeginPopupModal(kNameBoth, nullptr, flags)) {
    ImGui::TextWrapped("With two tables each needs a name; references to the first become $name:column.");
    ImGui::InputText("first table", first_name_, sizeof(first_name_));
    ImGui::InputText("new table", second_name_, sizeof(second_name_));
    if (ImGui::Button("Add", ImVec2(100, 0))) {
      const commit_result r = model_.add_table(second_name_, editor_model::starter_table(), first_name_);
      if (r.ok) {
        events.source = r.source;
        tab_ = second_name_;
        select_tab_ = true;
      } else {
        events.error = r.error;
      }
      table_ask_ = table_ask::none;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100, 0)) || escape) {
      table_ask_ = table_ask::none;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
  ImGui::SetNextWindowPos(middle, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  if (ImGui::BeginPopupModal(kRename, nullptr, flags)) {
    ImGui::InputText("name", first_name_, sizeof(first_name_));
    if (ImGui::Button("Rename", ImVec2(100, 0))) {
      const commit_result r = model_.rename_table(renaming_, first_name_);
      if (r.ok) {
        events.source = r.source;
        if (tab_ && *tab_ == renaming_) {
          tab_ = first_name_;
          select_tab_ = true;
        }
      } else {
        events.error = r.error;
      }
      table_ask_ = table_ask::none;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100, 0)) || escape) {
      table_ask_ = table_ask::none;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

void editor_panel::draw_dialogs() {
  if (asked_ == ask::save) {
    ImGui::OpenPopup(kSaveDialog);
  } else if (asked_ == ask::close) {
    ImGui::OpenPopup(kCloseDialog);
  }
  asked_ = ask::none;
  const ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;
  const ImVec2 middle(width_ * 0.5f, height_ * 0.5f);
  const bool escape = ImGui::IsKeyPressed(ImGuiKey_Escape, false);
  const bool enter = ImGui::IsKeyPressed(ImGuiKey_Enter, false);

  ImGui::SetNextWindowPos(middle, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  if (ImGui::BeginPopupModal(kSaveDialog, nullptr, flags)) {
    ImGui::TextUnformatted(("Overwrite " + file_name_ + " with your changes?").c_str());
    ImGui::TextDisabled("The file is replaced; this cannot be undone.");
    ImGui::Spacing();
    if (ImGui::Button("Save", ImVec2(110, 0)) || enter) {
      decision_ = decision::save;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SetItemDefaultFocus();
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(110, 0)) || escape) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  ImGui::SetNextWindowPos(middle, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  if (ImGui::BeginPopupModal(kCloseDialog, nullptr, flags)) {
    ImGui::TextUnformatted((file_name_ + " has changes that are not saved.").c_str());
    ImGui::TextDisabled("Save them over the file before closing?");
    ImGui::Spacing();
    if (ImGui::Button("Save", ImVec2(100, 0)) || enter) {
      decision_ = decision::save_and_close;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SetItemDefaultFocus();
    ImGui::SameLine();
    if (ImGui::Button("Discard", ImVec2(100, 0))) {
      decision_ = decision::discard_and_close;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100, 0)) || escape) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

void editor_panel::render() {
  const panel_context_scope scope(context_);
  if (ImDrawData* data = ImGui::GetDrawData()) {
    ImGui_ImplOpenGL3_RenderDrawData(data);
  }
}

} // namespace bdg::nymph
