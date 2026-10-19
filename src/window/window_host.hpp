// MIT License © 2026 Binary Dice Games
/// @file window_host.hpp
/// @brief The platform layer of `nymph view`: one native window with an
///        OpenGL 3.3 context and its events (internal;
///        spec/04_web_viewer/DESIGN.md section 10). Implemented once per
///        platform (window_win32.cpp, window_x11.cpp); every system
///        library is opened at run time.
#pragma once

#include <imgui.h>
#include <memory>
#include <string>
#include "view/live_view.hpp"

namespace bdg::nymph {

/// @brief The keys `nymph view` reacts to: its own shortcuts, and what a
///        text field of the editor panel needs.
enum class window_key {
  escape, q, r, s, y, e,
  enter, tab, backspace, del, left, right, up, down, home, end, page_up, page_down,
  a, c, v, x, z,
  other
};

/// @brief ImGui's key for `key` (ImGuiKey_None for keys ImGui does not
///        need: the viewer's own letters).
ImGuiKey imgui_key(window_key key);

/// @brief The modifier keys held with a key.
struct key_mods {
  bool ctrl = false;
  bool shift = false;
  bool alt = false;
};

/// @brief What a native_window reports; implemented by the viewer. Called
///        from native_window::pump() (and, on Windows, while the window is
///        created, when the viewer is not ready yet and ignores it).
class window_events {
 public:
  virtual ~window_events() = default;
  /// The client area changed size.
  virtual void on_resized() = 0;
  /// Part of the window was uncovered: show the last frame again.
  virtual void on_expose() = 0;
  /// The user asked to close the window (its close button, Alt+F4, the
  /// window manager). Return false to keep it open (the viewer asks first
  /// when there are unsaved changes); close() always closes.
  virtual bool on_close_request() = 0;
  /// A pointer event; `x`, `y` in client pixels.
  virtual void on_pointer(pointer_event::kind type, float x, float y, int button, float wheel_x, float wheel_y) = 0;
  /// A key went down or up. `repeat`: a held key's auto-repeat (down only).
  virtual void on_key(window_key key, key_mods mods, bool down, bool repeat) = 0;
  /// A character was typed (a Unicode code point; never a control code).
  virtual void on_text(char32_t codepoint) = 0;
};

/// @brief A window with an OpenGL 3.3 core context, current on the creating
///        thread from construction until destruction.
class native_window {
 public:
  virtual ~native_window() = default;

  virtual void make_current() = 0;
  virtual void swap() = 0;
  /// @brief The client area in device pixels.
  virtual void client_size(int& width, int& height) const = 0;
  /// @brief Device pixels per CSS pixel (the display's scale).
  virtual float scale() const = 0;
  /// @brief Asks for a client area of the given size.
  virtual void resize_client(int width, int height) = 0;
  /// @brief The largest client area that fits the screen's usable area.
  virtual void available_client_size(int& width, int& height) const = 0;
  virtual void set_title(const std::string& utf8) = 0;
  virtual void show() = 0;
  /// @brief Closes the window; pump() then returns false.
  virtual void close() = 0;
  /// @brief Dispatches pending events to the window_events, waiting for one
  ///        first when `block`.
  /// @return false once the window is closed.
  virtual bool pump(bool block) = 0;
  /// @brief The address of an OpenGL function of the current context.
  virtual void* gl_proc(const char* name) = 0;
  /// @brief The GLSL version line for ImGui's OpenGL3 backend.
  virtual const char* glsl_version() const = 0;
};

/// @brief Creates the window (hidden) and its context.
/// @throws window_error when there is no display or no OpenGL 3.3 context.
std::unique_ptr<native_window> create_native_window(window_events& events);

} // namespace bdg::nymph
