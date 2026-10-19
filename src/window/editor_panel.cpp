// MIT License © 2026 Binary Dice Games
/// @file editor_panel.cpp
/// @brief The editor panel's ImGui context and its frames.
#include "window/editor_panel.hpp"
#include <imgui_impl_opengl3.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <utility>
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
    case window_key::v: return ImGuiKey_V;
    case window_key::x: return ImGuiKey_X;
    case window_key::y: return ImGuiKey_Y;
    case window_key::z: return ImGuiKey_Z;
    default: return ImGuiKey_None;
  }
}

} // namespace

editor_panel::editor_panel(std::string source, std::string file_name, const char* glsl_version)
    : model_(std::move(source)), file_name_(std::move(file_name)) {
  const panel_context_scope scope(nullptr);
  context_ = ImGui::CreateContext();
  ImGui::SetCurrentContext(context_);
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.LogFilename = nullptr;
  ImGui::GetStyle().FontSizeBase = 15.0f;
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
  t->imgui(&ImGui::GetStyle()); // the colours of the figure's theme, nothing else
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
  ImGui::SetNextWindowPos(ImVec2(strip_x(), 0.0f), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(kWidth, height_), ImGuiCond_Always);
  ImGui::Begin(
      "##nymph_editor",
      nullptr,
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove |
          ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
  const inspector_events events = inspector_.draw(model_, tree, status);
  draw_dialogs();
  ImGui::End();
  wants_keyboard_ = io.WantTextInput;
  modal_open_ = ImGui::IsPopupOpen(kSaveDialog) || ImGui::IsPopupOpen(kCloseDialog);
  popup_open_ = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
  ImGui::Render();
  if (frames_left_ > 0) {
    --frames_left_;
  }
  return events;
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
