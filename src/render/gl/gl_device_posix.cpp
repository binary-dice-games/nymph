// MIT License © 2026 Binary Dice Games
/// @file gl_device_posix.cpp
/// @brief gl_device over EGL, opened at run time from libEGL.so.1: a
///        headless OpenGL 3.3 core context with no X or Wayland display
///        (spec/02_opengl_rasterizer/DESIGN.md section 3, "Linux (EGL)").
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "render/gl/gl_device.hpp"
#include "render/gl/shared_library.hpp"

namespace bdg::nymph {

namespace {

// ── EGL declarations (nymph's own; no EGL header is needed) ──────────────────

using EGLDisplay = void*;
using EGLConfig = void*;
using EGLContext = void*;
using EGLSurface = void*;
using EGLDeviceEXT = void*;
using EGLint = int32_t;
using EGLBoolean = unsigned int;
using EGLenum = unsigned int;
using EGLAttrib = intptr_t;

constexpr EGLint EGL_ALPHA_SIZE = 0x3021;
constexpr EGLint EGL_BLUE_SIZE = 0x3022;
constexpr EGLint EGL_GREEN_SIZE = 0x3023;
constexpr EGLint EGL_RED_SIZE = 0x3024;
constexpr EGLint EGL_SURFACE_TYPE = 0x3033;
constexpr EGLint EGL_NONE = 0x3038;
constexpr EGLint EGL_RENDERABLE_TYPE = 0x3040;
constexpr EGLint EGL_EXTENSIONS = 0x3055;
constexpr EGLint EGL_HEIGHT = 0x3056;
constexpr EGLint EGL_WIDTH = 0x3057;
constexpr EGLint EGL_DRAW = 0x3059;
constexpr EGLint EGL_READ = 0x305A;
constexpr EGLint EGL_CONTEXT_MAJOR_VERSION = 0x3098;
constexpr EGLint EGL_CONTEXT_MINOR_VERSION = 0x30FB;
constexpr EGLint EGL_CONTEXT_OPENGL_PROFILE_MASK = 0x30FD;
constexpr EGLint EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT = 0x0001;
constexpr EGLint EGL_PBUFFER_BIT = 0x0001;
constexpr EGLint EGL_OPENGL_BIT = 0x0008;
constexpr EGLenum EGL_OPENGL_API = 0x30A2;
constexpr EGLenum EGL_PLATFORM_DEVICE_EXT = 0x313F;
constexpr EGLenum EGL_PLATFORM_SURFACELESS_MESA = 0x31DD;

#define NYMPH_EGLAPI

/// X(return type, member name, parameters): EGL 1.4 entry points, looked
/// up with dlsym. The EGL name is "egl" + member name.
#define NYMPH_EGL_CORE(X)                                                                                              \
  X(void*, GetProcAddress, (const char* name))                                                                         \
  X(EGLint, GetError, ())                                                                                              \
  X(EGLDisplay, GetDisplay, (void* native))                                                                            \
  X(EGLBoolean, Initialize, (EGLDisplay dpy, EGLint * major, EGLint * minor))                                          \
  X(const char*, QueryString, (EGLDisplay dpy, EGLint name))                                                           \
  X(EGLBoolean, ChooseConfig, (EGLDisplay dpy, const EGLint* attribs, EGLConfig* configs, EGLint size, EGLint* count)) \
  X(EGLBoolean, BindAPI, (EGLenum api))                                                                                \
  X(EGLenum, QueryAPI, ())                                                                                             \
  X(EGLContext, CreateContext, (EGLDisplay dpy, EGLConfig config, EGLContext share, const EGLint* attribs))            \
  X(EGLBoolean, DestroyContext, (EGLDisplay dpy, EGLContext ctx))                                                      \
  X(EGLSurface, CreatePbufferSurface, (EGLDisplay dpy, EGLConfig config, const EGLint* attribs))                       \
  X(EGLBoolean, DestroySurface, (EGLDisplay dpy, EGLSurface surface))                                                  \
  X(EGLBoolean, MakeCurrent, (EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx))                       \
  X(EGLContext, GetCurrentContext, ())                                                                                 \
  X(EGLDisplay, GetCurrentDisplay, ())                                                                                 \
  X(EGLSurface, GetCurrentSurface, (EGLint which))

struct egl_api {
#define NYMPH_EGL_MEMBER(ret, name, params) ret(NYMPH_EGLAPI* name) params = nullptr;
  NYMPH_EGL_CORE(NYMPH_EGL_MEMBER)
#undef NYMPH_EGL_MEMBER
  // Extensions, through eglGetProcAddress (may stay null).
  EGLDisplay(NYMPH_EGLAPI* GetPlatformDisplay)(EGLenum platform, void* native, const EGLAttrib* attribs) = nullptr;
  EGLDisplay(NYMPH_EGLAPI* GetPlatformDisplayEXT)(EGLenum platform, void* native, const EGLint* attribs) = nullptr;
  EGLBoolean(NYMPH_EGLAPI* QueryDevicesEXT)(EGLint max, EGLDeviceEXT* devices, EGLint* count) = nullptr;
  const char*(NYMPH_EGLAPI* QueryDeviceStringEXT)(EGLDeviceEXT device, EGLint name) = nullptr;
};

template <typename Fn>
void assign(Fn& fn, void* address) {
  static_assert(sizeof(Fn) == sizeof(void*), "function and data pointers differ in size");
  std::memcpy(&fn, &address, sizeof(fn));
}

bool has_extension(const char* list, const char* name) {
  if (list == nullptr) {
    return false;
  }
  const size_t n = std::strlen(name);
  for (const char* p = list; (p = std::strstr(p, name)) != nullptr; p += n) {
    const bool starts = p == list || p[-1] == ' ';
    const bool ends = p[n] == '\0' || p[n] == ' ';
    if (starts && ends) {
      return true;
    }
  }
  return false;
}

std::string hex(EGLint code) {
  char buf[16];
  std::snprintf(buf, sizeof(buf), "0x%04X", static_cast<unsigned>(code));
  return buf;
}

} // namespace

// ── Platform state ───────────────────────────────────────────────────────────

struct gl_platform {
  shared_library egl_library;
  shared_library gl_library; ///< libOpenGL / libGL, only when needed
  egl_api egl;
  EGLDisplay display = nullptr;
  EGLContext context = nullptr;
  EGLSurface surface = nullptr; ///< 1x1 pbuffer, or null (surfaceless)
  bool proc_address_for_core = false;

  ~gl_platform() {
    if (display == nullptr) {
      return;
    }
    if (egl.GetCurrentContext() == context && context != nullptr) {
      egl.MakeCurrent(display, nullptr, nullptr, nullptr);
    }
    if (surface != nullptr) {
      egl.DestroySurface(display, surface);
    }
    if (context != nullptr) {
      egl.DestroyContext(display, context);
    }
    // The display is not terminated: EGL displays are shared per process
    // (another device may use the same one), and eglTerminate would
    // destroy its contexts too. libEGL stays pinned, so this is one
    // initialized display for the process lifetime.
  }

  /// GL function lookup for gl_api::load().
  static void* lookup(const char* name, void* user) {
    auto* self = static_cast<gl_platform*>(user);
    if (self->proc_address_for_core) {
      return self->egl.GetProcAddress(name);
    }
    if (void* p = self->gl_library.symbol(name)) {
      return p;
    }
    return self->egl.GetProcAddress(name);
  }
};

namespace {

/// The EGL displays worth trying, best first: each GPU device, then Mesa's
/// surfaceless platform, then the default display.
std::vector<EGLDisplay> candidate_displays(const egl_api& egl, const gl_device_options& options) {
  std::vector<EGLDisplay> displays;
  const char* client = egl.QueryString(nullptr, EGL_EXTENSIONS); // EGL_EXT_client_extensions
  auto platform_display = [&](EGLenum platform, void* native) -> EGLDisplay {
    if (egl.GetPlatformDisplay != nullptr) {
      return egl.GetPlatformDisplay(platform, native, nullptr);
    }
    if (egl.GetPlatformDisplayEXT != nullptr) {
      return egl.GetPlatformDisplayEXT(platform, native, nullptr);
    }
    return nullptr;
  };
  if (has_extension(client, "EGL_EXT_platform_device") && egl.QueryDevicesEXT != nullptr) {
    EGLDeviceEXT devices[16];
    EGLint count = 0;
    if (egl.QueryDevicesEXT(16, devices, &count) && count > 0) {
      for (EGLint i = 0; i < count; ++i) {
        const char* exts =
            egl.QueryDeviceStringEXT != nullptr ? egl.QueryDeviceStringEXT(devices[i], EGL_EXTENSIONS) : nullptr;
        if (!options.allow_software_gl && has_extension(exts, "EGL_MESA_device_software")) {
          continue;
        }
        if (EGLDisplay d = platform_display(EGL_PLATFORM_DEVICE_EXT, devices[i])) {
          displays.push_back(d);
        }
      }
    }
  }
  if (has_extension(client, "EGL_MESA_platform_surfaceless")) {
    if (EGLDisplay d = platform_display(EGL_PLATFORM_SURFACELESS_MESA, nullptr)) {
      displays.push_back(d);
    }
  }
  if (EGLDisplay d = egl.GetDisplay(nullptr)) {
    displays.push_back(d);
  }
  return displays;
}

/// Creates a 3.3 core context on `display` and makes it current. Returns
/// an empty string, or why not.
std::string create_context(gl_platform& p, EGLDisplay display) {
  egl_api& egl = p.egl;
  EGLint major = 0;
  EGLint minor = 0;
  if (!egl.Initialize(display, &major, &minor)) {
    return "EGL display cannot be initialized (EGL error " + hex(egl.GetError()) + ")";
  }
  const char* exts = egl.QueryString(display, EGL_EXTENSIONS);
  const bool egl15 = major > 1 || (major == 1 && minor >= 5);
  if (!egl15 && !has_extension(exts, "EGL_KHR_create_context")) {
    return "EGL " + std::to_string(major) + "." + std::to_string(minor) + " cannot create OpenGL 3.3 contexts";
  }
  const bool surfaceless = has_extension(exts, "EGL_KHR_surfaceless_context");
  if (!egl.BindAPI(EGL_OPENGL_API)) {
    return "EGL display has no desktop OpenGL";
  }
  const EGLint config_attribs[] = {
      EGL_SURFACE_TYPE,
      surfaceless ? 0 : EGL_PBUFFER_BIT,
      EGL_RENDERABLE_TYPE,
      EGL_OPENGL_BIT,
      EGL_RED_SIZE,
      8,
      EGL_GREEN_SIZE,
      8,
      EGL_BLUE_SIZE,
      8,
      EGL_ALPHA_SIZE,
      8,
      EGL_NONE};
  EGLConfig config = nullptr;
  EGLint count = 0;
  if (!egl.ChooseConfig(display, config_attribs, &config, 1, &count) || count < 1) {
    return "no EGL config for OpenGL with 8-bit RGBA";
  }
  const EGLint context_attribs[] = {
      EGL_CONTEXT_MAJOR_VERSION,
      3,
      EGL_CONTEXT_MINOR_VERSION,
      3,
      EGL_CONTEXT_OPENGL_PROFILE_MASK,
      EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
      EGL_NONE};
  EGLContext context = egl.CreateContext(display, config, nullptr, context_attribs);
  if (context == nullptr) {
    return "no OpenGL 3.3 core context (EGL error " + hex(egl.GetError()) + ")";
  }
  EGLSurface surface = nullptr;
  if (!surfaceless) {
    const EGLint pbuffer_attribs[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
    surface = egl.CreatePbufferSurface(display, config, pbuffer_attribs);
    if (surface == nullptr) {
      egl.DestroyContext(display, context);
      return "no EGL pbuffer surface (EGL error " + hex(egl.GetError()) + ")";
    }
  }
  if (!egl.MakeCurrent(display, surface, surface, context)) {
    const EGLint error = egl.GetError();
    if (surface != nullptr) {
      egl.DestroySurface(display, surface);
    }
    egl.DestroyContext(display, context);
    return "the OpenGL context cannot be made current (EGL error " + hex(error) + ")";
  }
  p.display = display;
  p.context = context;
  p.surface = surface;
  p.proc_address_for_core = egl15 || has_extension(exts, "EGL_KHR_get_all_proc_addresses") ||
      has_extension(egl.QueryString(nullptr, EGL_EXTENSIONS), "EGL_KHR_client_get_all_proc_addresses");
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
      options.library_names.empty() ? std::vector<std::string>{"libEGL.so.1", "libEGL.so"} : options.library_names;
  std::string error;
  p.egl_library = shared_library::open(names, error, true);
  if (!p.egl_library.loaded()) {
    return {nullptr, error};
  }
  bool complete = true;
#define NYMPH_EGL_LOAD(ret, name, params)                                  \
  assign(p.egl.name, p.egl_library.symbol("egl" #name));                   \
  if (p.egl.name == nullptr) {                                             \
    complete = false;                                                      \
    error = error.empty() ? "EGL function egl" #name " not found" : error; \
  }
  NYMPH_EGL_CORE(NYMPH_EGL_LOAD)
#undef NYMPH_EGL_LOAD
  if (!complete) {
    return {nullptr, error};
  }
  assign(p.egl.GetPlatformDisplay, p.egl.GetProcAddress("eglGetPlatformDisplay"));
  assign(p.egl.GetPlatformDisplayEXT, p.egl.GetProcAddress("eglGetPlatformDisplayEXT"));
  assign(p.egl.QueryDevicesEXT, p.egl.GetProcAddress("eglQueryDevicesEXT"));
  assign(p.egl.QueryDeviceStringEXT, p.egl.GetProcAddress("eglQueryDeviceStringEXT"));

  // Whatever is current on this thread now is current again on return.
  const EGLenum saved_api = p.egl.QueryAPI();
  const EGLDisplay saved_display = p.egl.GetCurrentDisplay();
  const EGLContext saved_context = p.egl.GetCurrentContext();
  const EGLSurface saved_draw = p.egl.GetCurrentSurface(EGL_DRAW);
  const EGLSurface saved_read = p.egl.GetCurrentSurface(EGL_READ);
  std::unique_ptr<gl_device> device(new gl_device(std::move(platform)));
  const std::array<void*, 4> saved{saved_display, saved_draw, saved_read, saved_context};

  // When every display fails, report the failure that got furthest (a
  // context that was refused says more than a display that did not open),
  // and the first of those.
  std::string reason;
  std::string best_reason = "no EGL display";
  bool best_reached_context = false;
  for (EGLDisplay display : candidate_displays(p.egl, options)) {
    reason = create_context(p, display);
    if (!reason.empty()) {
      if (best_reason == "no EGL display") {
        best_reason = reason;
      }
      continue;
    }
    if (!p.proc_address_for_core) {
      const std::vector<std::string> gl_names{"libOpenGL.so.0", "libGL.so.1"};
      std::string gl_error;
      p.gl_library = shared_library::open(gl_names, gl_error, true);
    }
    reason = device->finish(&gl_platform::lookup, &p, options);
    if (reason.empty()) {
      break;
    }
    if (!best_reached_context) {
      best_reason = reason;
      best_reached_context = true;
    }
    // Try the next display with a fresh context.
    p.egl.MakeCurrent(p.display, nullptr, nullptr, nullptr);
    if (p.surface != nullptr) {
      p.egl.DestroySurface(p.display, p.surface);
    }
    p.egl.DestroyContext(p.display, p.context);
    p.display = p.context = p.surface = nullptr;
  }
  device->restore(saved, saved_api);
  if (p.context == nullptr) {
    return {nullptr, best_reason};
  }
  return {std::move(device), {}};
}

bool gl_device::make_current(std::array<void*, 4>& saved, unsigned& saved_api) const {
  const egl_api& egl = platform_->egl;
  saved_api = egl.QueryAPI();
  saved = {
      egl.GetCurrentDisplay(),
      egl.GetCurrentSurface(EGL_DRAW),
      egl.GetCurrentSurface(EGL_READ),
      egl.GetCurrentContext()};
  egl.BindAPI(EGL_OPENGL_API);
  if (!egl.MakeCurrent(platform_->display, platform_->surface, platform_->surface, platform_->context)) {
    restore(saved, saved_api);
    return false;
  }
  return true;
}

void gl_device::restore(const std::array<void*, 4>& saved, unsigned saved_api) const {
  const egl_api& egl = platform_->egl;
  if (saved[3] != nullptr) {
    egl.BindAPI(saved_api);
    egl.MakeCurrent(saved[0], saved[1], saved[2], saved[3]);
    return;
  }
  if (platform_->display != nullptr && egl.GetCurrentContext() != nullptr) {
    egl.MakeCurrent(platform_->display, nullptr, nullptr, nullptr);
  }
  if (saved_api != 0) {
    egl.BindAPI(saved_api);
  }
}

} // namespace bdg::nymph
