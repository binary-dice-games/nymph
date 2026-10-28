// MIT License © 2026 Binary Dice Games
/// @file editor_panel.cpp
/// @brief The editor panel's ImGui context and its frames.
#include "workspace/editor_panel.hpp"
#include <imgui_impl_opengl3.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <utility>
#include "core/error.hpp"
#include "edit/image_fields.hpp"
#include "fonts/embedded_fonts.hpp"
#include "render/frame_setup.hpp"
#include "workspace/panel_style.hpp"

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
    case window_key::f: return ImGuiKey_F;
    case window_key::h: return ImGuiKey_H;
    case window_key::u: return ImGuiKey_U;
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
  ImGui::GetStyle().FontSizeBase = kUiFontSize;
  apply_appearance(appearance_);
  headless_ = glsl_version == nullptr || *glsl_version == '\0';
  if (!headless_) {
    ImGui_ImplOpenGL3_Init(glsl_version);
  } else {
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures; // texture requests stay pending: nothing draws
  }
}

editor_panel::~editor_panel() {
  const panel_context_scope scope(context_);
  if (!headless_) {
    ImGui_ImplOpenGL3_Shutdown();
  }
  ImGui::DestroyContext(context_);
}

void editor_panel::set_clipboard(std::function<void(std::string_view)> set, std::function<std::string()> get) {
  clipboard_set_ = std::move(set);
  clipboard_get_ = std::move(get);
  const panel_context_scope scope(context_);
  ImGuiPlatformIO& pio = ImGui::GetPlatformIO();
  pio.Platform_ClipboardUserData = this;
  pio.Platform_SetClipboardTextFn = [](ImGuiContext*, const char* text) {
    auto* self = static_cast<editor_panel*>(ImGui::GetPlatformIO().Platform_ClipboardUserData);
    if (self->clipboard_set_) {
      self->clipboard_set_(text != nullptr ? text : "");
    }
  };
  pio.Platform_GetClipboardTextFn = [](ImGuiContext*) -> const char* {
    auto* self = static_cast<editor_panel*>(ImGui::GetPlatformIO().Platform_ClipboardUserData);
    self->clipboard_text_ = self->clipboard_get_ ? self->clipboard_get_() : std::string();
    return self->clipboard_text_.c_str();
  };
}

void editor_panel::set_theme(const std::string& name) {
  if (name == theme_) {
    return;
  }
  theme_ = name;
  if (!appearance_pinned_) {
    panel_appearance a = appearance_;
    a.dark = name == "dark";
    apply_appearance(a);
  }
}

void editor_panel::check_fonts() {
  if (ui_font_ == nullptr && !font_data("FiraSans-Regular.ttf").empty()) {
    refresh();
  }
}

void editor_panel::set_appearance(const panel_appearance& appearance) {
  if (appearance_pinned_ && appearance == appearance_) {
    return;
  }
  appearance_pinned_ = true;
  apply_appearance(appearance);
}

void editor_panel::apply_appearance(const panel_appearance& appearance) {
  appearance_ = appearance;
  const panel_context_scope scope(context_);
  apply_panel_style(ImGui::GetStyle(), appearance);
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
  return frames_left_ > 0 || inspector_.pending() || inspector_.number_typing() || toolbar_.number_typing();
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

  if (ui_font_ == nullptr) {
    // The panel's text in the figures' sans font (Fira Sans), once its
    // bytes are there; ImGui's bitmap font until then.
    const std::span<const uint8_t> sans = font_data("FiraSans-Regular.ttf");
    if (!sans.empty()) {
      ImFontConfig config;
      config.FontDataOwnedByAtlas = false;
      ui_font_ = io.Fonts->AddFontFromMemoryTTF(const_cast<uint8_t*>(sans.data()), static_cast<int>(sans.size()), kUiFontSize, &config);
      io.FontDefault = ui_font_;
      refresh(); // a few frames more, drawn with it
    }
  }
  if (!headless_) {
    ImGui_ImplOpenGL3_NewFrame();
  }
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
  // The floating toolbar over the figure, first: the fields below show
  // what it changed.
  const layout_box toolbar_before = toolbar_.rect();
  const inspector_events tool_events = toolbar_.draw(model_, inspector_.menu(), figure_box(), anchor_);
  const layout_box toolbar_after = toolbar_.rect();
  if (toolbar_after.x != toolbar_before.x || toolbar_after.y != toolbar_before.y ||
      toolbar_after.width != toolbar_before.width || toolbar_after.height != toolbar_before.height) {
    refresh(); // it moved or changed size: place it again
  }
  ImGui::SetNextWindowPos(ImVec2(regions.strip.x, 0.0f), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(regions.strip.width, height_), ImGuiCond_Always);
  ImGui::Begin("##nymph_editor", nullptr, kFixed);
  inspector_events events = inspector_.draw(model_, tree, status);
  if (tool_events.source && !events.source) {
    events.source = tool_events.source;
    events.paths = tool_events.paths;
  }
  if (!events.select && tool_events.select) {
    events.select = tool_events.select;
  }
  if (!tool_events.error.empty()) {
    events.error = tool_events.error;
    inspector_.set_error(tool_events.error);
  } else if (tool_events.source) {
    inspector_.set_error("");
  }
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
    ImGui::TextUnformatted(
        (save_question_.empty() ? "Overwrite " + file_name_ + " with your changes?" : save_question_).c_str());
    ImGui::TextDisabled("%s", save_note_.empty() ? "The file is replaced; this cannot be undone." : save_note_.c_str());
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
    ImGui::TextDisabled("%s", save_question_.empty() ? "Save them over the file before closing?"
                                                     : ("Save them as " + file_name_ + " before closing?").c_str());
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
  if (headless_) {
    return;
  }
  if (ImDrawData* data = ImGui::GetDrawData()) {
    ImGui_ImplOpenGL3_RenderDrawData(data);
  }
}

} // namespace bdg::nymph
