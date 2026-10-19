// MIT License © 2026 Binary Dice Games
/// @file window_x11.cpp
/// @brief native_window over X11 and GLX: a window and an OpenGL 3.3 core
///        context from the system's libX11.so.6 and libGL.so.1. Both are
///        opened at run time and every function is looked up by name; the
///        X11 and GLX declarations are nymph's own (no X or GL header is
///        needed), so nothing is linked
///        (spec/04_web_viewer/DESIGN.md section 10).
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include "render/gl/shared_library.hpp"
#include "window/viewer_window.hpp"
#include "window/window_host.hpp"

namespace bdg::nymph {

namespace {

// ── X11 / GLX declarations (64-bit layouts; looked up at run time) ───────────

using XDisplay = void;
using XWindow = unsigned long;
using XAtom = unsigned long;
using XBool = int;
using GLXFBConfig = void*;
using GLXContext = void*;

struct XVisualInfo {
  void* visual;
  unsigned long visualid;
  int screen;
  int depth;
  int c_class;
  unsigned long red_mask, green_mask, blue_mask;
  int colormap_size;
  int bits_per_rgb;
};

struct XSetWindowAttributes {
  unsigned long background_pixmap;
  unsigned long background_pixel;
  unsigned long border_pixmap;
  unsigned long border_pixel;
  int bit_gravity;
  int win_gravity;
  int backing_store;
  unsigned long backing_planes;
  unsigned long backing_pixel;
  XBool save_under;
  long event_mask;
  long do_not_propagate_mask;
  XBool override_redirect;
  unsigned long colormap;
  unsigned long cursor;
};

struct XAnyEvent {
  int type;
  unsigned long serial;
  XBool send_event;
  XDisplay* display;
  XWindow window;
};
struct XKeyOrButtonEvent { // XKeyEvent, XButtonEvent and XMotionEvent share this prefix
  int type;
  unsigned long serial;
  XBool send_event;
  XDisplay* display;
  XWindow window, root, subwindow;
  unsigned long time;
  int x, y, x_root, y_root;
  unsigned state;
  unsigned detail; // keycode or button
  XBool same_screen;
};
struct XConfigureEvent {
  int type;
  unsigned long serial;
  XBool send_event;
  XDisplay* display;
  XWindow event, window;
  int x, y, width, height, border_width;
  XWindow above;
  XBool override_redirect;
};
struct XExposeEvent {
  int type;
  unsigned long serial;
  XBool send_event;
  XDisplay* display;
  XWindow window;
  int x, y, width, height, count;
};
struct XClientMessageEvent {
  int type;
  unsigned long serial;
  XBool send_event;
  XDisplay* display;
  XWindow window;
  XAtom message_type;
  int format;
  long data[5];
};
union XEvent {
  int type;
  XAnyEvent any;
  XKeyOrButtonEvent key;
  XConfigureEvent configure;
  XExposeEvent expose;
  XClientMessageEvent client;
  long pad[24];
};

constexpr int KeyPress = 2, ButtonPress = 4, ButtonRelease = 5, MotionNotify = 6, LeaveNotify = 8, Expose = 12,
              DestroyNotify = 17, ConfigureNotify = 22, ClientMessage = 33;
constexpr long KeyPressMask = 1L << 0, ButtonPressMask = 1L << 2, ButtonReleaseMask = 1L << 3,
               LeaveWindowMask = 1L << 5, PointerMotionMask = 1L << 6, ExposureMask = 1L << 15,
               StructureNotifyMask = 1L << 17;
constexpr unsigned long CWBorderPixel = 1UL << 3, CWEventMask = 1UL << 11, CWColormap = 1UL << 13;
constexpr unsigned long kKeysymEscape = 0xff1b;

// GLX_ARB_create_context and the FBConfig attributes
constexpr int GLX_DOUBLEBUFFER = 5, GLX_RED_SIZE = 8, GLX_GREEN_SIZE = 9, GLX_BLUE_SIZE = 10, GLX_ALPHA_SIZE = 11,
              GLX_X_VISUAL_TYPE = 0x22, GLX_TRUE_COLOR = 0x8002, GLX_DRAWABLE_TYPE = 0x8010, GLX_RENDER_TYPE = 0x8011,
              GLX_X_RENDERABLE = 0x8012, GLX_WINDOW_BIT = 1, GLX_RGBA_BIT = 1;
constexpr int GLX_CONTEXT_MAJOR_VERSION_ARB = 0x2091, GLX_CONTEXT_MINOR_VERSION_ARB = 0x2092,
              GLX_CONTEXT_PROFILE_MASK_ARB = 0x9126, GLX_CONTEXT_CORE_PROFILE_BIT_ARB = 1;

template <typename Fn>
void assign(Fn& fn, void* address) {
  static_assert(sizeof(Fn) == sizeof(void*), "function and data pointers differ in size");
  std::memcpy(&fn, &address, sizeof(fn));
}

using x_error_handler = int (*)(XDisplay*, void*);

/// The libX11 and libGL functions the window uses.
struct x11_api {
  shared_library x11;
  shared_library gl;

  XDisplay*(*XOpenDisplay)(const char*) = nullptr;
  int (*XCloseDisplay)(XDisplay*) = nullptr;
  int (*XDefaultScreen)(XDisplay*) = nullptr;
  XWindow (*XRootWindow)(XDisplay*, int) = nullptr;
  XWindow (*XCreateWindow)(
      XDisplay*, XWindow, int, int, unsigned, unsigned, unsigned, int, unsigned, void*, unsigned long,
      XSetWindowAttributes*) = nullptr;
  int (*XDestroyWindow)(XDisplay*, XWindow) = nullptr;
  int (*XMapWindow)(XDisplay*, XWindow) = nullptr;
  int (*XResizeWindow)(XDisplay*, XWindow, unsigned, unsigned) = nullptr;
  unsigned long (*XCreateColormap)(XDisplay*, XWindow, void*, int) = nullptr;
  int (*XFreeColormap)(XDisplay*, unsigned long) = nullptr;
  int (*XPending)(XDisplay*) = nullptr;
  int (*XNextEvent)(XDisplay*, XEvent*) = nullptr;
  int (*XFlush)(XDisplay*) = nullptr;
  int (*XSync)(XDisplay*, XBool) = nullptr;
  int (*XFree)(void*) = nullptr;
  XAtom (*XInternAtom)(XDisplay*, const char*, XBool) = nullptr;
  int (*XSetWMProtocols)(XDisplay*, XWindow, XAtom*, int) = nullptr;
  int (*XChangeProperty)(XDisplay*, XWindow, XAtom, XAtom, int, int, const unsigned char*, int) = nullptr;
  int (*XDisplayWidth)(XDisplay*, int) = nullptr;
  int (*XDisplayHeight)(XDisplay*, int) = nullptr;
  unsigned long (*XLookupKeysym)(void*, int) = nullptr;
  x_error_handler (*XSetErrorHandler)(x_error_handler) = nullptr;

  GLXFBConfig*(*glXChooseFBConfig)(XDisplay*, int, const int*, int*) = nullptr;
  XVisualInfo*(*glXGetVisualFromFBConfig)(XDisplay*, GLXFBConfig) = nullptr;
  void*(*glXGetProcAddressARB)(const unsigned char*) = nullptr;
  XBool (*glXMakeCurrent)(XDisplay*, XWindow, GLXContext) = nullptr;
  void (*glXSwapBuffers)(XDisplay*, XWindow) = nullptr;
  void (*glXDestroyContext)(XDisplay*, GLXContext) = nullptr;

  /// Opens the libraries and looks every function up.
  /// @throws window_error naming what is missing.
  void load() {
    std::string error;
    const auto open = [&](shared_library& lib, std::vector<std::string> names, const char* what, bool pin) {
      lib = shared_library::open(names, error, pin);
      if (!lib.loaded()) {
        throw window_error(std::string("cannot open a window: ") + what + " is not installed (" + error + ")");
      }
    };
    open(x11, {"libX11.so.6"}, "the X11 client library libX11", false);
    open(gl, {"libGL.so.1", "libGLX.so.0"}, "OpenGL (libGL.so.1)", true);
    const auto need = [](const shared_library& lib, const char* name, auto& fn) {
      void* address = lib.symbol(name);
      if (address == nullptr) {
        throw window_error(std::string("cannot open a window: ") + lib.name() + " has no " + name);
      }
      assign(fn, address);
    };
    need(x11, "XOpenDisplay", XOpenDisplay);
    need(x11, "XCloseDisplay", XCloseDisplay);
    need(x11, "XDefaultScreen", XDefaultScreen);
    need(x11, "XRootWindow", XRootWindow);
    need(x11, "XCreateWindow", XCreateWindow);
    need(x11, "XDestroyWindow", XDestroyWindow);
    need(x11, "XMapWindow", XMapWindow);
    need(x11, "XResizeWindow", XResizeWindow);
    need(x11, "XCreateColormap", XCreateColormap);
    need(x11, "XFreeColormap", XFreeColormap);
    need(x11, "XPending", XPending);
    need(x11, "XNextEvent", XNextEvent);
    need(x11, "XFlush", XFlush);
    need(x11, "XSync", XSync);
    need(x11, "XFree", XFree);
    need(x11, "XInternAtom", XInternAtom);
    need(x11, "XSetWMProtocols", XSetWMProtocols);
    need(x11, "XChangeProperty", XChangeProperty);
    need(x11, "XDisplayWidth", XDisplayWidth);
    need(x11, "XDisplayHeight", XDisplayHeight);
    need(x11, "XLookupKeysym", XLookupKeysym);
    need(x11, "XSetErrorHandler", XSetErrorHandler);
    need(gl, "glXChooseFBConfig", glXChooseFBConfig);
    need(gl, "glXGetVisualFromFBConfig", glXGetVisualFromFBConfig);
    need(gl, "glXGetProcAddressARB", glXGetProcAddressARB);
    need(gl, "glXMakeCurrent", glXMakeCurrent);
    need(gl, "glXSwapBuffers", glXSwapBuffers);
    need(gl, "glXDestroyContext", glXDestroyContext);
  }
};

x11_api& api() {
  static x11_api a;
  return a;
}

/// Set by the X error handler while a request is expected to possibly fail.
bool g_x_error = false;
int on_x_error(XDisplay*, void*) {
  g_x_error = true;
  return 0;
}

class x11_window : public native_window {
 public:
  explicit x11_window(window_events& events) : events_(events) {
    x11_api& a = api();
    if (!a.x11.loaded()) {
      a.load();
    }
    display_ = a.XOpenDisplay(nullptr);
    if (display_ == nullptr) {
      throw window_error("cannot open a window: no X display (is DISPLAY set?)");
    }
    const int screen = a.XDefaultScreen(display_);
    screen_w_ = a.XDisplayWidth(display_, screen);
    screen_h_ = a.XDisplayHeight(display_, screen);

    const int fb_attribs[] = {GLX_X_RENDERABLE, 1, GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT, GLX_RENDER_TYPE, GLX_RGBA_BIT,
                              GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR, GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8,
                              GLX_ALPHA_SIZE, 8, GLX_DOUBLEBUFFER, 1, 0};
    int count = 0;
    GLXFBConfig* configs = a.glXChooseFBConfig(display_, screen, fb_attribs, &count);
    if (configs == nullptr || count == 0) {
      throw window_error("cannot open a window: no suitable OpenGL framebuffer configuration");
    }
    const GLXFBConfig config = configs[0];
    a.XFree(configs);
    XVisualInfo* visual = a.glXGetVisualFromFBConfig(display_, config);
    if (visual == nullptr) {
      throw window_error("cannot open a window: no X visual for OpenGL");
    }
    colormap_ = a.XCreateColormap(display_, a.XRootWindow(display_, screen), visual->visual, 0);
    XSetWindowAttributes attrs{};
    attrs.colormap = colormap_;
    attrs.event_mask = KeyPressMask | ButtonPressMask | ButtonReleaseMask | LeaveWindowMask | PointerMotionMask |
        ExposureMask | StructureNotifyMask;
    width_ = 640;
    height_ = 480;
    window_ = a.XCreateWindow(
        display_, a.XRootWindow(display_, screen), 0, 0, static_cast<unsigned>(width_), static_cast<unsigned>(height_),
        0, visual->depth, 1 /* InputOutput */, visual->visual, CWBorderPixel | CWEventMask | CWColormap, &attrs);
    a.XFree(visual);
    if (window_ == 0) {
      throw window_error("cannot open a window: the X server refused to create one");
    }
    wm_delete_ = a.XInternAtom(display_, "WM_DELETE_WINDOW", 0);
    a.XSetWMProtocols(display_, window_, &wm_delete_, 1);
    create_context(config);
    ready_ = true;
  }

  ~x11_window() override {
    ready_ = false;
    x11_api& a = api();
    if (display_ == nullptr) {
      return;
    }
    if (context_ != nullptr) {
      a.glXMakeCurrent(display_, 0, nullptr);
      a.glXDestroyContext(display_, context_);
    }
    if (window_ != 0) {
      a.XDestroyWindow(display_, window_);
    }
    if (colormap_ != 0) {
      a.XFreeColormap(display_, colormap_);
    }
    a.XCloseDisplay(display_);
  }

  void make_current() override {
    api().glXMakeCurrent(display_, window_, context_);
  }
  void swap() override {
    api().glXSwapBuffers(display_, window_);
  }
  void client_size(int& width, int& height) const override {
    width = width_;
    height = height_;
  }
  float scale() const override {
    return 1.0f;
  }
  void resize_client(int width, int height) override {
    width_ = std::max(width, 1);
    height_ = std::max(height, 1);
    api().XResizeWindow(display_, window_, static_cast<unsigned>(width_), static_cast<unsigned>(height_));
    api().XFlush(display_);
  }
  void available_client_size(int& width, int& height) const override {
    // Leave room for the panels and the title bar.
    width = std::max(screen_w_ - 40, 0);
    height = std::max(screen_h_ - 120, 0);
  }
  void set_title(const std::string& utf8) override {
    x11_api& a = api();
    const XAtom name = a.XInternAtom(display_, "_NET_WM_NAME", 0);
    const XAtom type = a.XInternAtom(display_, "UTF8_STRING", 0);
    a.XChangeProperty(
        display_, window_, name, type, 8, 0 /* PropModeReplace */,
        reinterpret_cast<const unsigned char*>(utf8.data()), static_cast<int>(utf8.size()));
    // The classic name, for window managers that do not read _NET_WM_NAME
    // (predefined atoms: WM_NAME is 39, STRING is 31).
    a.XChangeProperty(
        display_, window_, 39, 31, 8, 0, reinterpret_cast<const unsigned char*>(utf8.data()),
        static_cast<int>(utf8.size()));
    a.XFlush(display_);
  }
  void show() override {
    api().XMapWindow(display_, window_);
    api().XFlush(display_);
  }
  void close() override {
    closed_ = true;
  }

  bool pump(bool block) override {
    x11_api& a = api();
    a.XFlush(display_);
    if (block && !closed_) {
      XEvent e;
      a.XNextEvent(display_, &e); // waits for the next event
      handle(e);
    }
    while (!closed_ && a.XPending(display_) > 0) {
      XEvent e;
      a.XNextEvent(display_, &e);
      handle(e);
    }
    return !closed_;
  }

  void* gl_proc(const char* name) override {
    x11_api& a = api();
    void* p = a.glXGetProcAddressARB(reinterpret_cast<const unsigned char*>(name));
    return p != nullptr ? p : a.gl.symbol(name);
  }
  const char* glsl_version() const override {
    return "#version 330";
  }

 private:
  /// A 3.3 core context, left current.
  void create_context(GLXFBConfig config) {
    x11_api& a = api();
    using create_fn = GLXContext (*)(XDisplay*, GLXFBConfig, GLXContext, XBool, const int*);
    create_fn create = nullptr;
    assign(create, a.gl.symbol("glXCreateContextAttribsARB"));
    if (create == nullptr) {
      assign(create, a.glXGetProcAddressARB(reinterpret_cast<const unsigned char*>("glXCreateContextAttribsARB")));
    }
    if (create == nullptr) {
      throw window_error("cannot open a window: the OpenGL driver cannot create OpenGL 3.3 contexts");
    }
    const int attribs[] = {GLX_CONTEXT_MAJOR_VERSION_ARB, 3, GLX_CONTEXT_MINOR_VERSION_ARB, 3,
                           GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB, 0};
    g_x_error = false;
    const auto previous = a.XSetErrorHandler(&on_x_error);
    context_ = create(display_, config, nullptr, 1, attribs);
    a.XSync(display_, 0);
    a.XSetErrorHandler(previous);
    if (context_ == nullptr || g_x_error) {
      context_ = nullptr;
      throw window_error("cannot open a window: no OpenGL 3.3 core context (the driver is too old?)");
    }
    if (!a.glXMakeCurrent(display_, window_, context_)) {
      throw window_error("cannot open a window: the OpenGL context cannot be made current");
    }
    using interval_fn = void (*)(XDisplay*, XWindow, int);
    interval_fn interval = nullptr;
    assign(interval, a.glXGetProcAddressARB(reinterpret_cast<const unsigned char*>("glXSwapIntervalEXT")));
    if (interval != nullptr) {
      interval(display_, window_, 1); // a frame is never drawn twice in one refresh
    }
  }

  static window_key key_of(unsigned long keysym) {
    switch (keysym) {
      case kKeysymEscape:
        return window_key::escape;
      case 'q':
      case 'Q':
        return window_key::q;
      case 'r':
      case 'R':
        return window_key::r;
      case 's':
      case 'S':
        return window_key::s;
      case 'y':
      case 'Y':
        return window_key::y;
      default:
        return window_key::other;
    }
  }

  void handle(XEvent& e) {
    if (!ready_) {
      return;
    }
    const float x = static_cast<float>(e.key.x);
    const float y = static_cast<float>(e.key.y);
    switch (e.type) {
      case ConfigureNotify:
        if (e.configure.width != width_ || e.configure.height != height_) {
          width_ = e.configure.width;
          height_ = e.configure.height;
          events_.on_resized();
        }
        break;
      case Expose:
        if (e.expose.count == 0) {
          events_.on_expose();
        }
        break;
      case MotionNotify:
        events_.on_pointer(pointer_event::kind::move, x, y, 0, 0.0f, 0.0f);
        break;
      case LeaveNotify:
        events_.on_pointer(pointer_event::kind::leave, 0.0f, 0.0f, 0, 0.0f, 0.0f);
        break;
      case ButtonPress:
      case ButtonRelease: {
        const bool down = e.type == ButtonPress;
        switch (e.key.detail) {
          case 1:
            events_.on_pointer(down ? pointer_event::kind::down : pointer_event::kind::up, x, y, 0, 0.0f, 0.0f);
            break;
          case 2:
            events_.on_pointer(down ? pointer_event::kind::down : pointer_event::kind::up, x, y, 2, 0.0f, 0.0f);
            break;
          case 3:
            events_.on_pointer(down ? pointer_event::kind::down : pointer_event::kind::up, x, y, 1, 0.0f, 0.0f);
            break;
          case 4:
          case 5: // the wheel is a button 4 / 5 press; up zooms in
            if (down) {
              events_.on_pointer(pointer_event::kind::wheel, x, y, 0, 0.0f, e.key.detail == 4 ? 1.0f : -1.0f);
            }
            break;
          case 6:
          case 7:
            if (down) {
              events_.on_pointer(pointer_event::kind::wheel, x, y, 0, e.key.detail == 7 ? 1.0f : -1.0f, 0.0f);
            }
            break;
          default:
            break;
        }
        break;
      }
      case KeyPress:
        events_.on_key(key_of(api().XLookupKeysym(&e.key, 0)));
        break;
      case ClientMessage:
        if (static_cast<XAtom>(e.client.data[0]) == wm_delete_) {
          closed_ = true;
        }
        break;
      case DestroyNotify:
        closed_ = true;
        break;
      default:
        break;
    }
  }

  window_events& events_;
  XDisplay* display_ = nullptr;
  XWindow window_ = 0;
  unsigned long colormap_ = 0;
  GLXContext context_ = nullptr;
  XAtom wm_delete_ = 0;
  int width_ = 0; ///< Client area; the last size asked for or reported.
  int height_ = 0;
  int screen_w_ = 0;
  int screen_h_ = 0;
  bool ready_ = false;
  bool closed_ = false;
};

} // namespace

std::unique_ptr<native_window> create_native_window(window_events& events) {
  return std::make_unique<x11_window>(events);
}

} // namespace bdg::nymph
