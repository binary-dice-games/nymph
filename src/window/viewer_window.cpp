// MIT License © 2026 Binary Dice Games
/// @file viewer_window.cpp
/// @brief show_window(): a Win32 window around a live_view, drawn with Dear
///        ImGui's OpenGL3 backend into an OpenGL 3.3 context of the
///        system's own opengl32.dll (spec/04_web_viewer/DESIGN.md
///        section 10). user32.dll, gdi32.dll and opengl32.dll are opened at
///        run time and every function is looked up by name, so none of them
///        is an import of the executable.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <imgui_impl_opengl3.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <exception>
#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include "cli/cli_support.hpp"
#include "nymph.hpp"
#include "render/gl/shared_library.hpp"
#include "view/live_view.hpp"
#include "window/viewer_window.hpp"

namespace bdg::nymph {

namespace {

// ── Functions looked up at run time ──────────────────────────────────────────

template <typename Fn>
void assign(Fn& fn, void* address) {
  static_assert(sizeof(Fn) == sizeof(void*), "function and data pointers differ in size");
  std::memcpy(&fn, &address, sizeof(fn));
}

/// The user32 / gdi32 / opengl32 functions the window uses.
struct win32_api {
  shared_library user32;
  shared_library gdi32;
  shared_library opengl32;

  ATOM(WINAPI* RegisterClassExW)(const WNDCLASSEXW*) = nullptr;
  BOOL(WINAPI* UnregisterClassW)(LPCWSTR, HINSTANCE) = nullptr;
  HWND(WINAPI* CreateWindowExW)(DWORD, LPCWSTR, LPCWSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID) =
      nullptr;
  BOOL(WINAPI* DestroyWindow)(HWND) = nullptr;
  BOOL(WINAPI* ShowWindow)(HWND, int) = nullptr;
  LRESULT(WINAPI* DefWindowProcW)(HWND, UINT, WPARAM, LPARAM) = nullptr;
  BOOL(WINAPI* PeekMessageW)(LPMSG, HWND, UINT, UINT, UINT) = nullptr;
  BOOL(WINAPI* GetMessageW)(LPMSG, HWND, UINT, UINT) = nullptr;
  BOOL(WINAPI* TranslateMessage)(const MSG*) = nullptr;
  LRESULT(WINAPI* DispatchMessageW)(const MSG*) = nullptr;
  VOID(WINAPI* PostQuitMessage)(int) = nullptr;
  BOOL(WINAPI* GetClientRect)(HWND, LPRECT) = nullptr;
  BOOL(WINAPI* AdjustWindowRectEx)(LPRECT, DWORD, BOOL, DWORD) = nullptr;
  BOOL(WINAPI* SetWindowPos)(HWND, HWND, int, int, int, int, UINT) = nullptr;
  BOOL(WINAPI* SetWindowTextW)(HWND, LPCWSTR) = nullptr;
  BOOL(WINAPI* ValidateRect)(HWND, const RECT*) = nullptr;
  HDC(WINAPI* GetDC)(HWND) = nullptr;
  int(WINAPI* ReleaseDC)(HWND, HDC) = nullptr;
  HCURSOR(WINAPI* LoadCursorW)(HINSTANCE, LPCWSTR) = nullptr;
  HWND(WINAPI* SetCapture)(HWND) = nullptr;
  BOOL(WINAPI* ReleaseCapture)() = nullptr;
  BOOL(WINAPI* TrackMouseEvent)(LPTRACKMOUSEEVENT) = nullptr;
  BOOL(WINAPI* SystemParametersInfoW)(UINT, UINT, PVOID, UINT) = nullptr;
  LONG_PTR(WINAPI* SetWindowLongPtrW)(HWND, int, LONG_PTR) = nullptr;
  LONG_PTR(WINAPI* GetWindowLongPtrW)(HWND, int) = nullptr;
  // Optional: Windows 10 and later.
  BOOL(WINAPI* SetProcessDpiAwarenessContext)(HANDLE) = nullptr;
  UINT(WINAPI* GetDpiForWindow)(HWND) = nullptr;
  BOOL(WINAPI* AdjustWindowRectExForDpi)(LPRECT, DWORD, BOOL, DWORD, UINT) = nullptr;

  int(WINAPI* ChoosePixelFormat)(HDC, const PIXELFORMATDESCRIPTOR*) = nullptr;
  BOOL(WINAPI* SetPixelFormat)(HDC, int, const PIXELFORMATDESCRIPTOR*) = nullptr;
  BOOL(WINAPI* SwapBuffers)(HDC) = nullptr;

  HGLRC(WINAPI* wglCreateContext)(HDC) = nullptr;
  BOOL(WINAPI* wglDeleteContext)(HGLRC) = nullptr;
  BOOL(WINAPI* wglMakeCurrent)(HDC, HGLRC) = nullptr;
  PROC(WINAPI* wglGetProcAddress)(LPCSTR) = nullptr;
  void(WINAPI* glViewport)(int, int, int, int) = nullptr;
  void(WINAPI* glClearColor)(float, float, float, float) = nullptr;
  void(WINAPI* glClear)(unsigned) = nullptr;

  /// Opens the libraries and looks every required function up.
  /// @throws window_error naming what is missing.
  void load() {
    std::string error;
    const auto open = [&](shared_library& lib, const char* name, bool pin) {
      const std::vector<std::string> names{name};
      lib = shared_library::open(names, error, pin);
      if (!lib.loaded()) {
        throw window_error("cannot open a window: " + error);
      }
    };
    open(user32, "user32.dll", false);
    open(gdi32, "gdi32.dll", false);
    open(opengl32, "opengl32.dll", true); // drivers dislike being unloaded and loaded again

    const auto need = [](const shared_library& lib, const char* name, auto& fn) {
      void* address = lib.symbol(name);
      if (address == nullptr) {
        throw window_error(std::string("cannot open a window: ") + lib.name() + " has no " + name);
      }
      assign(fn, address);
    };
    const auto want = [](const shared_library& lib, const char* name, auto& fn) { assign(fn, lib.symbol(name)); };

    need(user32, "RegisterClassExW", RegisterClassExW);
    need(user32, "UnregisterClassW", UnregisterClassW);
    need(user32, "CreateWindowExW", CreateWindowExW);
    need(user32, "DestroyWindow", DestroyWindow);
    need(user32, "ShowWindow", ShowWindow);
    need(user32, "DefWindowProcW", DefWindowProcW);
    need(user32, "PeekMessageW", PeekMessageW);
    need(user32, "GetMessageW", GetMessageW);
    need(user32, "TranslateMessage", TranslateMessage);
    need(user32, "DispatchMessageW", DispatchMessageW);
    need(user32, "PostQuitMessage", PostQuitMessage);
    need(user32, "GetClientRect", GetClientRect);
    need(user32, "AdjustWindowRectEx", AdjustWindowRectEx);
    need(user32, "SetWindowPos", SetWindowPos);
    need(user32, "SetWindowTextW", SetWindowTextW);
    need(user32, "ValidateRect", ValidateRect);
    need(user32, "GetDC", GetDC);
    need(user32, "ReleaseDC", ReleaseDC);
    need(user32, "LoadCursorW", LoadCursorW);
    need(user32, "SetCapture", SetCapture);
    need(user32, "ReleaseCapture", ReleaseCapture);
    need(user32, "TrackMouseEvent", TrackMouseEvent);
    need(user32, "SystemParametersInfoW", SystemParametersInfoW);
#if defined(_WIN64)
    need(user32, "SetWindowLongPtrW", SetWindowLongPtrW);
    need(user32, "GetWindowLongPtrW", GetWindowLongPtrW);
#else
    need(user32, "SetWindowLongW", SetWindowLongPtrW);
    need(user32, "GetWindowLongW", GetWindowLongPtrW);
#endif
    want(user32, "SetProcessDpiAwarenessContext", SetProcessDpiAwarenessContext);
    want(user32, "GetDpiForWindow", GetDpiForWindow);
    want(user32, "AdjustWindowRectExForDpi", AdjustWindowRectExForDpi);

    need(gdi32, "ChoosePixelFormat", ChoosePixelFormat);
    need(gdi32, "SetPixelFormat", SetPixelFormat);
    need(gdi32, "SwapBuffers", SwapBuffers);

    need(opengl32, "wglCreateContext", wglCreateContext);
    need(opengl32, "wglDeleteContext", wglDeleteContext);
    need(opengl32, "wglMakeCurrent", wglMakeCurrent);
    need(opengl32, "wglGetProcAddress", wglGetProcAddress);
    need(opengl32, "glViewport", glViewport); // OpenGL 1.1: exported, not in wglGetProcAddress
    need(opengl32, "glClearColor", glClearColor);
    need(opengl32, "glClear", glClear);
  }
};

win32_api& api() {
  static win32_api a;
  return a;
}

std::string windows_error(const char* what) {
  return std::string("cannot open a window: ") + what + " (Windows error " + std::to_string(GetLastError()) + ")";
}

std::wstring widen(const std::string& utf8) {
  if (utf8.empty()) {
    return {};
  }
  const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
  std::wstring out(static_cast<size_t>(std::max(n, 0)), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), out.data(), n);
  return out;
}

constexpr wchar_t kWindowClass[] = L"nymph_view_window";
constexpr DWORD kWindowStyle = WS_OVERLAPPEDWINDOW;
constexpr unsigned kColorBufferBit = 0x00004000;

// WGL_ARB_create_context
constexpr int WGL_CONTEXT_MAJOR_VERSION_ARB = 0x2091;
constexpr int WGL_CONTEXT_MINOR_VERSION_ARB = 0x2092;
constexpr int WGL_CONTEXT_PROFILE_MASK_ARB = 0x9126;
constexpr int WGL_CONTEXT_CORE_PROFILE_BIT_ARB = 0x0001;

class viewer;

/// The window class, the window, its device context and an OpenGL 3.3 core
/// context (made current on the creating thread).
class gl_window {
 public:
  /// @param owner Passed to the window procedure with every message.
  explicit gl_window(viewer* owner) {
    win32_api& a = api();
    instance_ = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &gl_window::wnd_proc;
    wc.hInstance = instance_;
    wc.hCursor = a.LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)); // IDC_ARROW
    wc.lpszClassName = kWindowClass;
    if (a.RegisterClassExW(&wc) == 0) {
      throw window_error(windows_error("no window class"));
    }
    registered_ = true;
    hwnd_ = a.CreateWindowExW(
        0, kWindowClass, L"nymph", kWindowStyle, CW_USEDEFAULT, CW_USEDEFAULT, 640, 480, nullptr, nullptr, instance_,
        owner);
    if (hwnd_ == nullptr) {
      throw window_error(windows_error("no window"));
    }
    dc_ = a.GetDC(hwnd_);
    if (dc_ == nullptr) {
      throw window_error(windows_error("no device context"));
    }
    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cAlphaBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;
    const int format = a.ChoosePixelFormat(dc_, &pfd);
    if (format == 0 || !a.SetPixelFormat(dc_, format, &pfd)) {
      throw window_error(windows_error("no OpenGL pixel format"));
    }
    create_context();
  }

  ~gl_window() {
    win32_api& a = api();
    if (context_ != nullptr) {
      a.wglMakeCurrent(nullptr, nullptr);
      a.wglDeleteContext(context_);
    }
    if (dc_ != nullptr) {
      a.ReleaseDC(hwnd_, dc_);
    }
    if (hwnd_ != nullptr) {
      a.DestroyWindow(hwnd_); // fails harmlessly when the user closed it
    }
    if (registered_) {
      a.UnregisterClassW(kWindowClass, instance_);
    }
  }
  gl_window(const gl_window&) = delete;
  gl_window& operator=(const gl_window&) = delete;

  HWND hwnd() const {
    return hwnd_;
  }
  HDC dc() const {
    return dc_;
  }

  /// Makes the context current again (the backend's shutdown needs it).
  void make_current() const {
    api().wglMakeCurrent(dc_, context_);
  }

  static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

 private:
  /// A legacy context finds wglCreateContextAttribsARB; then the 3.3 core
  /// context, left current.
  void create_context() {
    win32_api& a = api();
    HGLRC legacy = a.wglCreateContext(dc_);
    if (legacy == nullptr || !a.wglMakeCurrent(dc_, legacy)) {
      if (legacy != nullptr) {
        a.wglDeleteContext(legacy);
      }
      throw window_error(windows_error("no OpenGL context"));
    }
    using create_attribs_fn = HGLRC(WINAPI*)(HDC, HGLRC, const int*);
    create_attribs_fn create_attribs = nullptr;
    assign(create_attribs, reinterpret_cast<void*>(a.wglGetProcAddress("wglCreateContextAttribsARB")));
    HGLRC core = nullptr;
    if (create_attribs != nullptr) {
      const int attribs[] = {
          WGL_CONTEXT_MAJOR_VERSION_ARB, 3, WGL_CONTEXT_MINOR_VERSION_ARB, 3, WGL_CONTEXT_PROFILE_MASK_ARB,
          WGL_CONTEXT_CORE_PROFILE_BIT_ARB, 0};
      core = create_attribs(dc_, nullptr, attribs);
    }
    a.wglMakeCurrent(nullptr, nullptr);
    a.wglDeleteContext(legacy);
    if (core == nullptr) {
      throw window_error(
          create_attribs == nullptr ? "cannot open a window: the OpenGL driver cannot create OpenGL 3.3 contexts"
                                    : windows_error("no OpenGL 3.3 core context"));
    }
    context_ = core;
    if (!a.wglMakeCurrent(dc_, context_)) {
      throw window_error(windows_error("the OpenGL context cannot be made current"));
    }
    // Wait for the display's refresh: a frame is never drawn twice in one.
    using swap_interval_fn = BOOL(WINAPI*)(int);
    swap_interval_fn swap_interval = nullptr;
    assign(swap_interval, reinterpret_cast<void*>(a.wglGetProcAddress("wglSwapIntervalEXT")));
    if (swap_interval != nullptr) {
      swap_interval(1);
    }
  }

  HINSTANCE instance_ = nullptr;
  bool registered_ = false;
  HWND hwnd_ = nullptr;
  HDC dc_ = nullptr;
  HGLRC context_ = nullptr;
};

// ── The viewer ───────────────────────────────────────────────────────────────

/// A live_view in a gl_window: events in, frames out.
class viewer {
 public:
  explicit viewer(const view_request& request)
      : request_(request),
        name_(std::filesystem::path(request.args.input).filename().string()),
        window_(this),
        view_(view_options{request.args.fit, /*cpu_textures=*/false, /*tooltips=*/true}) {
    dpi_scale_ = current_scale();
    ready_ = true;
  }

  ~viewer() {
    ready_ = false;
    window_.make_current(); // the backend's shutdown (view_'s destruction) needs its context
  }

  /// Opens the window at the layout's size and runs until it is closed.
  void run() {
    view_.set_context_hooks(
        [] {
          ImGui_ImplOpenGL3_Init("#version 330");
          // Normally made by ImGui_ImplOpenGL3_NewFrame(), which nymph's own
          // frame loop does not call.
          ImGui_ImplOpenGL3_CreateDeviceObjects();
        },
        [] { ImGui_ImplOpenGL3_Shutdown(); });
    view_.set_source(request_.source);
    view_.set_theme(request_.args.theme);
    open_at_layout_size();
    sync_size();
    set_title("");
    api().ShowWindow(window_.hwnd(), SW_SHOW);

    MSG msg;
    for (;;) {
      if (view_.wants_frame()) {
        while (api().PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
          if (msg.message == WM_QUIT) {
            return finish();
          }
          api().TranslateMessage(&msg);
          api().DispatchMessageW(&msg);
        }
        present(true);
      } else if (api().GetMessageW(&msg, nullptr, 0, 0) > 0) { // idle: wait for input
        api().TranslateMessage(&msg);
        api().DispatchMessageW(&msg);
      } else {
        return finish();
      }
      if (error_) {
        return finish();
      }
    }
  }

  /// Handles one window message; returns false to leave it to Windows.
  bool on_message(UINT message, WPARAM wparam, LPARAM lparam, LRESULT& result) {
    result = 0;
    if (!ready_) {
      return false;
    }
    switch (message) {
      case WM_ERASEBKGND:
        result = 1;
        return true;
      case WM_PAINT:
        api().ValidateRect(window_.hwnd(), nullptr);
        present(false);
        return true;
      case WM_SIZE:
        minimized_ = wparam == SIZE_MINIMIZED;
        if (!minimized_ && !opening_) {
          sync_size();
          present(true);
        }
        return true;
      case WM_DPICHANGED: {
        const RECT* r = reinterpret_cast<const RECT*>(lparam);
        api().SetWindowPos(
            window_.hwnd(), nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
            SWP_NOZORDER | SWP_NOACTIVATE);
        return true;
      }
      case WM_MOUSEMOVE:
        if (!tracking_) {
          TRACKMOUSEEVENT t{sizeof(t), TME_LEAVE, window_.hwnd(), 0};
          api().TrackMouseEvent(&t);
          tracking_ = true;
        }
        cursor_x_ = static_cast<float>(GET_X_LPARAM(lparam));
        cursor_y_ = static_cast<float>(GET_Y_LPARAM(lparam));
        send(pointer_event::kind::move, 0, 0.0f, 0.0f);
        return true;
      case WM_MOUSELEAVE:
        tracking_ = false;
        send(pointer_event::kind::leave, 0, 0.0f, 0.0f);
        return true;
      case WM_LBUTTONDOWN:
      case WM_RBUTTONDOWN:
      case WM_MBUTTONDOWN:
        button(message, true, lparam);
        return true;
      case WM_LBUTTONUP:
      case WM_RBUTTONUP:
      case WM_MBUTTONUP:
        button(message, false, lparam);
        return true;
      case WM_MOUSEWHEEL:
        send(pointer_event::kind::wheel, 0, 0.0f, static_cast<float>(GET_WHEEL_DELTA_WPARAM(wparam)) / WHEEL_DELTA);
        return true;
      case WM_MOUSEHWHEEL:
        send(pointer_event::kind::wheel, 0, static_cast<float>(GET_WHEEL_DELTA_WPARAM(wparam)) / WHEEL_DELTA, 0.0f);
        return true;
      case WM_KEYDOWN:
        if ((lparam & (1 << 30)) == 0) { // not an auto-repeat
          on_key(static_cast<int>(wparam));
        }
        return true;
      case WM_CLOSE:
        api().DestroyWindow(window_.hwnd());
        return true;
      case WM_DESTROY:
        api().PostQuitMessage(0);
        return true;
      default:
        return false;
    }
  }

  /// Keeps an exception thrown inside a message for run() to rethrow: it
  /// must not travel through Windows' dispatch.
  void fail(std::exception_ptr e) {
    error_ = e;
    api().PostQuitMessage(1);
  }

 private:
  void finish() {
    if (error_) {
      std::rethrow_exception(error_);
    }
  }

  float current_scale() const {
    const win32_api& a = api();
    const UINT dpi = a.GetDpiForWindow != nullptr ? a.GetDpiForWindow(window_.hwnd()) : 96;
    return dpi > 0 ? static_cast<float>(dpi) / 96.0f : 1.0f;
  }

  /// The outer window size for a client area of `w` x `h` pixels.
  window_size outer_size(int w, int h) const {
    RECT r{0, 0, w, h};
    const win32_api& a = api();
    if (a.AdjustWindowRectExForDpi != nullptr) {
      a.AdjustWindowRectExForDpi(&r, kWindowStyle, FALSE, 0, static_cast<UINT>(std::lround(dpi_scale_ * 96.0f)));
    } else {
      a.AdjustWindowRectEx(&r, kWindowStyle, FALSE, 0);
    }
    return {static_cast<int>(r.right - r.left), static_cast<int>(r.bottom - r.top)};
  }

  /// Sizes the window to the layout (CSS pixels times the display scale),
  /// within the screen's work area.
  void open_at_layout_size() {
    RECT work{};
    int avail_w = 0, avail_h = 0;
    if (api().SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0)) {
      const window_size frame = outer_size(0, 0);
      avail_w = static_cast<int>(work.right - work.left) - frame.width;
      avail_h = static_cast<int>(work.bottom - work.top) - frame.height;
    }
    const window_size client = initial_window_size(
        static_cast<int>(std::lround(static_cast<float>(request_.width) * dpi_scale_)),
        static_cast<int>(std::lround(static_cast<float>(request_.height) * dpi_scale_)),
        avail_w,
        avail_h);
    resize_client(client.width, client.height);
  }

  /// Asks for a window whose client area is `w` x `h` pixels.
  void resize_client(int w, int h) {
    const window_size outer = outer_size(w, h);
    opening_ = true; // the WM_SIZE this causes is handled by the caller
    api().SetWindowPos(
        window_.hwnd(), nullptr, 0, 0, outer.width, outer.height, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    opening_ = false;
    sync_size();
  }

  /// Gives the view its box: the client area in CSS pixels, the display
  /// scale as its pixel ratio.
  void sync_size() {
    RECT r{};
    api().GetClientRect(window_.hwnd(), &r);
    dpi_scale_ = current_scale();
    client_w_ = static_cast<int>(r.right - r.left);
    client_h_ = static_cast<int>(r.bottom - r.top);
    if (client_w_ <= 0 || client_h_ <= 0) {
      return;
    }
    view_.resize(view_size{
        static_cast<float>(client_w_) / dpi_scale_, static_cast<float>(client_h_) / dpi_scale_, dpi_scale_});
  }

  /// Draws a new frame if one is due (`fresh`), or the last one again (an
  /// exposed window), and shows it.
  void present(bool fresh) {
    if (minimized_ || client_w_ <= 0 || client_h_ <= 0) {
      return;
    }
    if (fresh && view_.wants_frame()) {
      if (const ImDrawData* data = view_.frame(now())) {
        last_ = data;
      }
      enlarge_if_too_small();
    }
    if (last_ == nullptr) {
      return;
    }
    win32_api& a = api();
    a.glViewport(0, 0, client_w_, client_h_);
    const ImVec4 bg = ImGui::ColorConvertU32ToFloat4(view_.clear_color());
    a.glClearColor(bg.x, bg.y, bg.z, 1.0f);
    a.glClear(kColorBufferBit);
    view_.with_contexts([&] { ImGui_ImplOpenGL3_RenderDrawData(const_cast<ImDrawData*>(last_)); });
    a.SwapBuffers(window_.dc());
  }

  /// A window too small for the layout is drawn shrunk (the view falls back
  /// to the source size); ask for a window of the layout's size once.
  void enlarge_if_too_small() {
    const view_geometry g = view_.geometry();
    if (!g.fit_fallback) {
      asked_w_ = asked_h_ = 0;
      return;
    }
    const int want_w = static_cast<int>(std::lround(g.layout_width * dpi_scale_));
    const int want_h = static_cast<int>(std::lround(g.layout_height * dpi_scale_));
    if (want_w == asked_w_ && want_h == asked_h_) {
      return;
    }
    asked_w_ = want_w;
    asked_h_ = want_h;
    resize_client(want_w, want_h);
  }

  void button(UINT message, bool down, LPARAM lparam) {
    cursor_x_ = static_cast<float>(GET_X_LPARAM(lparam));
    cursor_y_ = static_cast<float>(GET_Y_LPARAM(lparam));
    const int index = message == WM_LBUTTONDOWN || message == WM_LBUTTONUP ? 0
        : message == WM_RBUTTONDOWN || message == WM_RBUTTONUP             ? 1
                                                                           : 2;
    // Keep the drag going outside the window.
    if (down) {
      if (buttons_down_++ == 0) {
        api().SetCapture(window_.hwnd());
      }
    } else if (buttons_down_ > 0 && --buttons_down_ == 0) {
      api().ReleaseCapture();
    }
    send(down ? pointer_event::kind::down : pointer_event::kind::up, index, 0.0f, 0.0f);
  }

  void send(pointer_event::kind type, int button, float wheel_x, float wheel_y) {
    pointer_event e;
    e.type = type;
    e.x = cursor_x_ / dpi_scale_;
    e.y = cursor_y_ / dpi_scale_;
    e.button = button; // left, right, middle are ImGui's 0, 1, 2
    e.wheel_x = wheel_x;
    e.wheel_y = wheel_y;
    view_.input(e);
  }

  void on_key(int key) {
    switch (key) {
      case VK_ESCAPE:
      case 'Q':
        api().DestroyWindow(window_.hwnd());
        break;
      case 'R':
        view_.reset_view();
        last_ = nullptr; // its draw data went with the old contexts
        break;
      case 'S':
        save("png");
        break;
      case 'Y':
        save("yaml");
        break;
      default:
        break;
    }
  }

  /// Writes the image or the YAML on screen next to the input.
  void save(const char* extension) {
    const std::string path = view_save_path(request_.args.input, extension);
    try {
      const std::string source = view_.source_with_view();
      std::ostringstream unused;
      if (std::string_view(extension) == "png") {
        const std::vector<uint8_t> png = render_png(source);
        write_output(path, as_text(png), unused);
      } else {
        write_output(path, source, unused);
      }
      set_title("saved " + std::filesystem::path(path).filename().string());
    } catch (const std::exception& e) {
      set_title(std::string("cannot save: ") + e.what());
    }
  }

  void set_title(const std::string& status) {
    const std::string title = "nymph - " + name_ + (status.empty() ? "" : " - " + status);
    api().SetWindowTextW(window_.hwnd(), widen(title).c_str());
  }

  /// Seconds on a monotonic clock.
  static double now() {
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return static_cast<double>(counter.QuadPart) / static_cast<double>(frequency.QuadPart);
  }

  const view_request& request_;
  std::string name_;
  bool ready_ = false; ///< Messages sent while the window is created find no view yet.
  gl_window window_;
  live_view view_; ///< After window_: destroyed first, while the context exists.
  const ImDrawData* last_ = nullptr; ///< The last frame, valid until the next one.
  std::exception_ptr error_;
  float cursor_x_ = 0.0f; ///< Client pixels.
  float cursor_y_ = 0.0f;
  float dpi_scale_ = 1.0f; ///< Display pixels per CSS pixel.
  int client_w_ = 0;
  int client_h_ = 0;
  int buttons_down_ = 0;
  bool tracking_ = false; ///< A WM_MOUSELEAVE is pending.
  bool minimized_ = false;
  bool opening_ = false;
  int asked_w_ = 0; ///< The size last asked of the window for a layout that did not fit.
  int asked_h_ = 0;
};

LRESULT CALLBACK gl_window::wnd_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
  win32_api& a = api();
  if (message == WM_NCCREATE) {
    const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
    a.SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
  }
  auto* v = reinterpret_cast<viewer*>(a.GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  if (v != nullptr) {
    LRESULT result = 0;
    try {
      if (v->on_message(message, wparam, lparam, result)) {
        return result;
      }
    } catch (...) {
      v->fail(std::current_exception());
      return 0;
    }
  }
  return a.DefWindowProcW(hwnd, message, wparam, lparam);
}

} // namespace

void show_window(const view_request& request) {
  win32_api& a = api();
  if (!a.user32.loaded()) {
    a.load();
  }
  if (a.SetProcessDpiAwarenessContext != nullptr) {
    // Per-monitor v2: the window gets real pixels and draws sharp on any display.
    a.SetProcessDpiAwarenessContext(reinterpret_cast<HANDLE>(-4));
  }
  viewer v(request);
  v.run();
}

} // namespace bdg::nymph
