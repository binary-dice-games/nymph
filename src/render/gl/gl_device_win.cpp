// MIT License © 2026 Binary Dice Games
/// @file gl_device_win.cpp
/// @brief gl_device over WGL, opened at run time from opengl32.dll: an
///        OpenGL 3.3 core context on a hidden window that is never shown
///        (spec/02_opengl_rasterizer/DESIGN.md section 3, "Windows and
///        MSYS2 (WGL)"). No wgl* or gl* function is called by its linker
///        name, so opengl32.dll is not an import of the executable.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include "render/gl/gl_device.hpp"
#include "render/gl/shared_library.hpp"

namespace bdg::nymph {

namespace {

// ── WGL declarations (looked up at run time) ─────────────────────────────────

using wgl_create_context_fn = HGLRC(WINAPI*)(HDC);
using wgl_delete_context_fn = BOOL(WINAPI*)(HGLRC);
using wgl_make_current_fn = BOOL(WINAPI*)(HDC, HGLRC);
using wgl_get_proc_address_fn = PROC(WINAPI*)(LPCSTR);
using wgl_get_current_context_fn = HGLRC(WINAPI*)();
using wgl_get_current_dc_fn = HDC(WINAPI*)();
using wgl_create_context_attribs_fn = HGLRC(WINAPI*)(HDC, HGLRC, const int*);

constexpr int WGL_CONTEXT_MAJOR_VERSION_ARB = 0x2091;
constexpr int WGL_CONTEXT_MINOR_VERSION_ARB = 0x2092;
constexpr int WGL_CONTEXT_PROFILE_MASK_ARB = 0x9126;
constexpr int WGL_CONTEXT_CORE_PROFILE_BIT_ARB = 0x0001;

constexpr wchar_t kWindowClass[] = L"nymph_opengl_window";

template <typename Fn>
void assign(Fn& fn, void* address) {
  static_assert(sizeof(Fn) == sizeof(void*), "function and data pointers differ in size");
  std::memcpy(&fn, &address, sizeof(fn));
}

template <typename Fn>
void* address_of(Fn fn) {
  void* address = nullptr;
  std::memcpy(&address, &fn, sizeof(address));
  return address;
}

std::string last_error(const char* what) {
  return std::string(what) + " (Windows error " + std::to_string(GetLastError()) + ")";
}

} // namespace

// ── Platform state ───────────────────────────────────────────────────────────

struct gl_platform {
  shared_library opengl32;
  wgl_create_context_fn create_context = nullptr;
  wgl_delete_context_fn delete_context = nullptr;
  wgl_make_current_fn make_current = nullptr;
  wgl_get_proc_address_fn get_proc_address = nullptr;
  wgl_get_current_context_fn get_current_context = nullptr;
  wgl_get_current_dc_fn get_current_dc = nullptr;

  HINSTANCE instance = nullptr;
  bool registered_class = false;
  HWND window = nullptr;
  HDC dc = nullptr;
  HGLRC context = nullptr;

  ~gl_platform() {
    if (context != nullptr) {
      if (get_current_context() == context) {
        make_current(nullptr, nullptr);
      }
      delete_context(context);
    }
    if (dc != nullptr) {
      ReleaseDC(window, dc);
    }
    if (window != nullptr) {
      DestroyWindow(window);
    }
    if (registered_class) {
      // Fails while another device's window uses the class; that device
      // unregisters it later.
      UnregisterClassW(kWindowClass, instance);
    }
  }

  /// GL function lookup for gl_api::load(): wglGetProcAddress for what is
  /// newer than OpenGL 1.1, opengl32.dll's exports for the rest.
  static void* lookup(const char* name, void* user) {
    auto* self = static_cast<gl_platform*>(user);
    void* p = address_of(self->get_proc_address(name));
    const auto value = reinterpret_cast<intptr_t>(p);
    if (value == 0 || value == 1 || value == 2 || value == 3 || value == -1) {
      p = self->opengl32.symbol(name);
    }
    return p;
  }
};

namespace {

/// Loads the wgl functions; returns an empty string or the missing name.
std::string load_wgl(gl_platform& p) {
  const struct {
    const char* name;
    void** slot;
  } entries[] = {
      {"wglCreateContext", reinterpret_cast<void**>(&p.create_context)},
      {"wglDeleteContext", reinterpret_cast<void**>(&p.delete_context)},
      {"wglMakeCurrent", reinterpret_cast<void**>(&p.make_current)},
      {"wglGetProcAddress", reinterpret_cast<void**>(&p.get_proc_address)},
      {"wglGetCurrentContext", reinterpret_cast<void**>(&p.get_current_context)},
      {"wglGetCurrentDC", reinterpret_cast<void**>(&p.get_current_dc)},
  };
  for (const auto& e : entries) {
    void* address = p.opengl32.symbol(e.name);
    if (address == nullptr) {
      return std::string("WGL function ") + e.name + " not found";
    }
    std::memcpy(e.slot, &address, sizeof(address));
  }
  return {};
}

/// A hidden window with a plain RGBA8 pixel format, for a device context.
std::string create_window(gl_platform& p) {
  p.instance = GetModuleHandleW(nullptr);
  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(wc);
  wc.style = CS_OWNDC;
  wc.lpfnWndProc = DefWindowProcW;
  wc.hInstance = p.instance;
  wc.lpszClassName = kWindowClass;
  if (RegisterClassExW(&wc) != 0) {
    p.registered_class = true;
  } else if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
    return last_error("no window class for OpenGL");
  }
  p.window = CreateWindowExW(0, kWindowClass, L"nymph", WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, p.instance, nullptr);
  if (p.window == nullptr) {
    return last_error("no hidden window for OpenGL");
  }
  p.dc = GetDC(p.window);
  if (p.dc == nullptr) {
    return last_error("no device context for OpenGL");
  }
  PIXELFORMATDESCRIPTOR pfd{};
  pfd.nSize = sizeof(pfd);
  pfd.nVersion = 1;
  pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
  pfd.iPixelType = PFD_TYPE_RGBA;
  pfd.cColorBits = 32;
  pfd.cAlphaBits = 8;
  pfd.iLayerType = PFD_MAIN_PLANE;
  const int format = ChoosePixelFormat(p.dc, &pfd);
  if (format == 0 || !SetPixelFormat(p.dc, format, &pfd)) {
    return last_error("no OpenGL pixel format");
  }
  return {};
}

/// A legacy context to find wglCreateContextAttribsARB, then the 3.3 core
/// context, left current.
std::string create_context(gl_platform& p) {
  HGLRC legacy = p.create_context(p.dc);
  if (legacy == nullptr) {
    return last_error("no OpenGL context");
  }
  if (!p.make_current(p.dc, legacy)) {
    p.delete_context(legacy);
    return last_error("the OpenGL context cannot be made current");
  }
  wgl_create_context_attribs_fn create_attribs = nullptr;
  assign(create_attribs, address_of(p.get_proc_address("wglCreateContextAttribsARB")));
  HGLRC core = nullptr;
  if (create_attribs != nullptr) {
    const int attribs[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB,
        3,
        WGL_CONTEXT_MINOR_VERSION_ARB,
        3,
        WGL_CONTEXT_PROFILE_MASK_ARB,
        WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        0};
    core = create_attribs(p.dc, nullptr, attribs);
  }
  p.make_current(nullptr, nullptr);
  p.delete_context(legacy);
  if (create_attribs == nullptr) {
    return "no wglCreateContextAttribsARB: the OpenGL driver cannot create OpenGL 3.3 contexts";
  }
  if (core == nullptr) {
    return last_error("no OpenGL 3.3 core context");
  }
  p.context = core;
  if (!p.make_current(p.dc, core)) {
    return last_error("the OpenGL context cannot be made current");
  }
  return {};
}

} // namespace

// ── gl_device ────────────────────────────────────────────────────────────────

gl_device::gl_device(std::unique_ptr<gl_platform> platform) : platform_(std::move(platform)) {}

gl_device::~gl_device() = default;

gl_device::result gl_device::create(const gl_device_options& options) {
  auto platform = std::make_unique<gl_platform>();
  gl_platform& p = *platform;
  const std::vector<std::string> names =
      options.library_names.empty() ? std::vector<std::string>{"opengl32.dll"} : options.library_names;
  std::string error;
  p.opengl32 = shared_library::open(names, error, true);
  if (!p.opengl32.loaded()) {
    return {nullptr, error};
  }
  if (std::string missing = load_wgl(p); !missing.empty()) {
    return {nullptr, missing};
  }

  // Whatever is current on this thread now is current again on return.
  const std::array<void*, 4> saved{p.get_current_dc(), p.get_current_context(), nullptr, nullptr};
  std::unique_ptr<gl_device> device(new gl_device(std::move(platform)));
  std::string reason = create_window(p);
  if (reason.empty()) {
    reason = create_context(p);
  }
  if (reason.empty()) {
    reason = device->finish(&gl_platform::lookup, &p, options);
  }
  device->restore(saved, 0);
  if (!reason.empty()) {
    return {nullptr, reason};
  }
  return {std::move(device), {}};
}

bool gl_device::make_current(std::array<void*, 4>& saved, unsigned& saved_api) const {
  const gl_platform& p = *platform_;
  saved = {p.get_current_dc(), p.get_current_context(), nullptr, nullptr};
  saved_api = 0;
  return p.make_current(p.dc, p.context) != FALSE;
}

void gl_device::restore(const std::array<void*, 4>& saved, unsigned) const {
  const gl_platform& p = *platform_;
  p.make_current(static_cast<HDC>(saved[0]), static_cast<HGLRC>(saved[1]));
}

} // namespace bdg::nymph
