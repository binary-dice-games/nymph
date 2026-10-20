// MIT License © 2026 Binary Dice Games
/// @file window_win32.cpp
/// @brief native_window over Win32 and WGL: a window and an OpenGL 3.3 core
///        context of the system's opengl32.dll. user32.dll, gdi32.dll and
///        opengl32.dll are opened at run time and every function is looked
///        up by name, so none of them is an import of the executable
///        (spec/04_web_viewer/DESIGN.md section 10).
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include "render/gl/shared_library.hpp"
#include "window/viewer_window.hpp"
#include "window/window_host.hpp"
#include "window/window_icon.hpp"

namespace bdg::nymph {

namespace {

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
  SHORT(WINAPI* GetKeyState)(int) = nullptr;
  BOOL(WINAPI* TrackMouseEvent)(LPTRACKMOUSEEVENT) = nullptr;
  BOOL(WINAPI* SystemParametersInfoW)(UINT, UINT, PVOID, UINT) = nullptr;
  LONG_PTR(WINAPI* SetWindowLongPtrW)(HWND, int, LONG_PTR) = nullptr;
  LONG_PTR(WINAPI* GetWindowLongPtrW)(HWND, int) = nullptr;
  LRESULT(WINAPI* SendMessageW)(HWND, UINT, WPARAM, LPARAM) = nullptr;
  int(WINAPI* GetSystemMetrics)(int) = nullptr;
  HICON(WINAPI* CreateIconIndirect)(PICONINFO) = nullptr;
  BOOL(WINAPI* DestroyIcon)(HICON) = nullptr;
  // Optional: Windows 10 and later.
  BOOL(WINAPI* SetProcessDpiAwarenessContext)(HANDLE) = nullptr;
  UINT(WINAPI* GetDpiForWindow)(HWND) = nullptr;
  BOOL(WINAPI* AdjustWindowRectExForDpi)(LPRECT, DWORD, BOOL, DWORD, UINT) = nullptr;
  int(WINAPI* GetSystemMetricsForDpi)(int, UINT) = nullptr;

  int(WINAPI* ChoosePixelFormat)(HDC, const PIXELFORMATDESCRIPTOR*) = nullptr;
  BOOL(WINAPI* SetPixelFormat)(HDC, int, const PIXELFORMATDESCRIPTOR*) = nullptr;
  BOOL(WINAPI* SwapBuffers)(HDC) = nullptr;
  HBITMAP(WINAPI* CreateDIBSection)(HDC, const BITMAPINFO*, UINT, void**, HANDLE, DWORD) = nullptr;
  HBITMAP(WINAPI* CreateBitmap)(int, int, UINT, UINT, const void*) = nullptr;
  BOOL(WINAPI* DeleteObject)(HGDIOBJ) = nullptr;

  HGLRC(WINAPI* wglCreateContext)(HDC) = nullptr;
  BOOL(WINAPI* wglDeleteContext)(HGLRC) = nullptr;
  BOOL(WINAPI* wglMakeCurrent)(HDC, HGLRC) = nullptr;
  PROC(WINAPI* wglGetProcAddress)(LPCSTR) = nullptr;

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
    need(user32, "GetKeyState", GetKeyState);
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
    want(user32, "GetSystemMetricsForDpi", GetSystemMetricsForDpi);
    need(user32, "SendMessageW", SendMessageW);
    need(user32, "GetSystemMetrics", GetSystemMetrics);
    need(user32, "CreateIconIndirect", CreateIconIndirect);
    need(user32, "DestroyIcon", DestroyIcon);

    need(gdi32, "ChoosePixelFormat", ChoosePixelFormat);
    need(gdi32, "SetPixelFormat", SetPixelFormat);
    need(gdi32, "SwapBuffers", SwapBuffers);
    need(gdi32, "CreateDIBSection", CreateDIBSection);
    need(gdi32, "CreateBitmap", CreateBitmap);
    need(gdi32, "DeleteObject", DeleteObject);

    need(opengl32, "wglCreateContext", wglCreateContext);
    need(opengl32, "wglDeleteContext", wglDeleteContext);
    need(opengl32, "wglMakeCurrent", wglMakeCurrent);
    need(opengl32, "wglGetProcAddress", wglGetProcAddress);
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

// WGL_ARB_create_context
constexpr int WGL_CONTEXT_MAJOR_VERSION_ARB = 0x2091;
constexpr int WGL_CONTEXT_MINOR_VERSION_ARB = 0x2092;
constexpr int WGL_CONTEXT_PROFILE_MASK_ARB = 0x9126;
constexpr int WGL_CONTEXT_CORE_PROFILE_BIT_ARB = 0x0001;

class win32_window : public native_window {
 public:
  explicit win32_window(window_events& events) : events_(events) {
    win32_api& a = api();
    if (!a.user32.loaded()) {
      a.load();
    }
    if (a.SetProcessDpiAwarenessContext != nullptr) {
      // Per-monitor v2: the window gets real pixels and draws sharp on any display.
      a.SetProcessDpiAwarenessContext(reinterpret_cast<HANDLE>(-4));
    }
    instance_ = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &win32_window::wnd_proc;
    wc.hInstance = instance_;
    wc.hCursor = a.LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)); // IDC_ARROW
    wc.lpszClassName = kWindowClass;
    if (a.RegisterClassExW(&wc) == 0) {
      throw window_error(windows_error("no window class"));
    }
    registered_ = true;
    hwnd_ = a.CreateWindowExW(
        0, kWindowClass, L"nymph", kWindowStyle, CW_USEDEFAULT, CW_USEDEFAULT, 640, 480, nullptr, nullptr, instance_,
        this);
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
    set_icons();
    create_context();
    ready_ = true;
  }

  ~win32_window() override {
    ready_ = false;
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
    for (HICON icon : {big_icon_, small_icon_}) {
      if (icon != nullptr) {
        a.DestroyIcon(icon);
      }
    }
    if (registered_) {
      a.UnregisterClassW(kWindowClass, instance_);
    }
  }

  void make_current() override {
    api().wglMakeCurrent(dc_, context_);
  }
  void swap() override {
    api().SwapBuffers(dc_);
  }
  void client_size(int& width, int& height) const override {
    RECT r{};
    api().GetClientRect(hwnd_, &r);
    width = static_cast<int>(r.right - r.left);
    height = static_cast<int>(r.bottom - r.top);
  }
  float scale() const override {
    const win32_api& a = api();
    const UINT dpi = a.GetDpiForWindow != nullptr ? a.GetDpiForWindow(hwnd_) : 96;
    return dpi > 0 ? static_cast<float>(dpi) / 96.0f : 1.0f;
  }
  void resize_client(int width, int height) override {
    const window_size outer = outer_size(width, height);
    api().SetWindowPos(hwnd_, nullptr, 0, 0, outer.width, outer.height, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
  }
  void available_client_size(int& width, int& height) const override {
    RECT work{};
    width = height = 0;
    if (api().SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0)) {
      const window_size frame = outer_size(0, 0);
      width = static_cast<int>(work.right - work.left) - frame.width;
      height = static_cast<int>(work.bottom - work.top) - frame.height;
    }
  }
  void set_title(const std::string& utf8) override {
    api().SetWindowTextW(hwnd_, widen(utf8).c_str());
  }
  void show() override {
    api().ShowWindow(hwnd_, SW_SHOW);
  }
  void close() override {
    api().DestroyWindow(hwnd_);
  }

  bool pump(bool block) override {
    win32_api& a = api();
    MSG msg;
    if (block) {
      if (a.GetMessageW(&msg, nullptr, 0, 0) <= 0) {
        return false;
      }
      a.TranslateMessage(&msg);
      a.DispatchMessageW(&msg);
    } else {
      while (a.PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
          return false;
        }
        a.TranslateMessage(&msg);
        a.DispatchMessageW(&msg);
      }
    }
    if (error_) {
      std::rethrow_exception(error_);
    }
    return true;
  }

  void* gl_proc(const char* name) override {
    void* p = reinterpret_cast<void*>(api().wglGetProcAddress(name));
    const auto value = reinterpret_cast<intptr_t>(p);
    if (value == 0 || value == 1 || value == 2 || value == 3 || value == -1) {
      p = api().opengl32.symbol(name); // OpenGL 1.1: exported, not in wglGetProcAddress
    }
    return p;
  }
  const char* glsl_version() const override {
    return "#version 330";
  }

 private:
  struct window_size {
    int width, height;
  };

  /// WM_SETICON: nymph's icon at the sizes Windows asks for at the
  /// window's DPI, ICON_BIG for the taskbar and Alt+Tab, ICON_SMALL for
  /// the title bar. Called again when the DPI changes.
  void set_icons() {
    win32_api& a = api();
    const image& icon = window_icon();
    if (icon.rgba.empty()) {
      return;
    }
    const UINT dpi = a.GetDpiForWindow != nullptr ? a.GetDpiForWindow(hwnd_) : 96;
    const auto metric = [&](int index) {
      const int value =
          a.GetSystemMetricsForDpi != nullptr ? a.GetSystemMetricsForDpi(index, dpi) : a.GetSystemMetrics(index);
      return value > 0 ? value : 32;
    };
    const auto replace = [&](HICON& slot, int size, WPARAM which) {
      HICON fresh = make_icon(scale_icon(icon, size));
      if (fresh == nullptr) {
        return;
      }
      a.SendMessageW(hwnd_, WM_SETICON, which, reinterpret_cast<LPARAM>(fresh));
      if (slot != nullptr) {
        a.DestroyIcon(slot);
      }
      slot = fresh;
    };
    replace(big_icon_, metric(SM_CXICON), ICON_BIG);
    replace(small_icon_, metric(SM_CXSMICON), ICON_SMALL);
  }

  /// An icon of `img`'s pixels with their alpha (a 32-bit top-down DIB and
  /// an all-zero mask); nullptr on failure.
  static HICON make_icon(const image& img) {
    win32_api& a = api();
    if (img.rgba.empty()) {
      return nullptr;
    }
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = img.width;
    bmi.bmiHeader.biHeight = -img.height; // top row first
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP color = a.CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (color == nullptr || bits == nullptr) {
      if (color != nullptr) {
        a.DeleteObject(color);
      }
      return nullptr;
    }
    auto* bgra = static_cast<uint8_t*>(bits);
    for (size_t i = 0; i < img.rgba.size(); i += 4) {
      bgra[i + 0] = img.rgba[i + 2];
      bgra[i + 1] = img.rgba[i + 1];
      bgra[i + 2] = img.rgba[i + 0];
      bgra[i + 3] = img.rgba[i + 3];
    }
    // The AND mask is ignored for 32-bit icons with alpha but must exist;
    // its rows are padded to 16 bits.
    const std::vector<uint8_t> mask_bits(static_cast<size_t>((img.width + 15) / 16 * 2) * img.height, 0);
    HBITMAP mask = a.CreateBitmap(img.width, img.height, 1, 1, mask_bits.data());
    HICON result = nullptr;
    if (mask != nullptr) {
      ICONINFO info{};
      info.fIcon = TRUE;
      info.hbmMask = mask;
      info.hbmColor = color;
      result = a.CreateIconIndirect(&info); // copies both bitmaps
      a.DeleteObject(mask);
    }
    a.DeleteObject(color);
    return result;
  }

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

  /// The outer window size for a client area of `w` x `h` pixels.
  window_size outer_size(int w, int h) const {
    RECT r{0, 0, w, h};
    const win32_api& a = api();
    if (a.AdjustWindowRectExForDpi != nullptr) {
      a.AdjustWindowRectExForDpi(&r, kWindowStyle, FALSE, 0, static_cast<UINT>(std::lround(scale() * 96.0f)));
    } else {
      a.AdjustWindowRectEx(&r, kWindowStyle, FALSE, 0);
    }
    return {static_cast<int>(r.right - r.left), static_cast<int>(r.bottom - r.top)};
  }

  void button(UINT message, bool down, LPARAM lparam) {
    const float x = static_cast<float>(GET_X_LPARAM(lparam));
    const float y = static_cast<float>(GET_Y_LPARAM(lparam));
    const int index = message == WM_LBUTTONDOWN || message == WM_LBUTTONUP ? 0
        : message == WM_RBUTTONDOWN || message == WM_RBUTTONUP             ? 1
                                                                           : 2;
    // Keep the drag going outside the window.
    if (down) {
      if (buttons_down_++ == 0) {
        api().SetCapture(hwnd_);
      }
    } else if (buttons_down_ > 0 && --buttons_down_ == 0) {
      api().ReleaseCapture();
    }
    events_.on_pointer(down ? pointer_event::kind::down : pointer_event::kind::up, x, y, index, 0.0f, 0.0f);
  }

  static window_key key_of(WPARAM vk) {
    switch (vk) {
      case VK_ESCAPE: return window_key::escape;
      case VK_RETURN: return window_key::enter;
      case VK_TAB: return window_key::tab;
      case VK_BACK: return window_key::backspace;
      case VK_DELETE: return window_key::del;
      case VK_LEFT: return window_key::left;
      case VK_RIGHT: return window_key::right;
      case VK_UP: return window_key::up;
      case VK_DOWN: return window_key::down;
      case VK_HOME: return window_key::home;
      case VK_END: return window_key::end;
      case VK_PRIOR: return window_key::page_up;
      case VK_NEXT: return window_key::page_down;
      case 'A': return window_key::a;
      case 'C': return window_key::c;
      case 'E': return window_key::e;
      case 'Q': return window_key::q;
      case 'R': return window_key::r;
      case 'S': return window_key::s;
      case 'V': return window_key::v;
      case 'X': return window_key::x;
      case 'Y': return window_key::y;
      case 'Z': return window_key::z;
      default: return window_key::other;
    }
  }

  /// Handles one message; false leaves it to Windows.
  bool on_message(UINT message, WPARAM wparam, LPARAM lparam, LRESULT& result) {
    result = 0;
    if (!ready_) { // sent while the window is created
      return false;
    }
    switch (message) {
      case WM_ERASEBKGND:
        result = 1;
        return true;
      case WM_PAINT:
        api().ValidateRect(hwnd_, nullptr);
        events_.on_expose();
        return true;
      case WM_SIZE:
        if (wparam != SIZE_MINIMIZED) {
          events_.on_resized();
        }
        return true;
      case WM_DPICHANGED: {
        const RECT* r = reinterpret_cast<const RECT*>(lparam);
        api().SetWindowPos(
            hwnd_, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
        set_icons();
        return true;
      }
      case WM_MOUSEMOVE:
        if (!tracking_) {
          TRACKMOUSEEVENT t{sizeof(t), TME_LEAVE, hwnd_, 0};
          api().TrackMouseEvent(&t);
          tracking_ = true;
        }
        events_.on_pointer(
            pointer_event::kind::move, static_cast<float>(GET_X_LPARAM(lparam)), static_cast<float>(GET_Y_LPARAM(lparam)),
            0, 0.0f, 0.0f);
        return true;
      case WM_MOUSELEAVE:
        tracking_ = false;
        events_.on_pointer(pointer_event::kind::leave, 0.0f, 0.0f, 0, 0.0f, 0.0f);
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
      case WM_MOUSEHWHEEL: {
        // Wheel messages carry screen coordinates; the viewer wants the last client position.
        const float notches = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wparam)) / WHEEL_DELTA;
        events_.on_pointer(
            pointer_event::kind::wheel, last_x_, last_y_, 0, message == WM_MOUSEHWHEEL ? notches : 0.0f,
            message == WM_MOUSEWHEEL ? notches : 0.0f);
        return true;
      }
      case WM_KEYDOWN:
      case WM_KEYUP: {
        const bool down = message == WM_KEYDOWN;
        key_mods mods;
        mods.ctrl = (api().GetKeyState(VK_CONTROL) & 0x8000) != 0;
        mods.shift = (api().GetKeyState(VK_SHIFT) & 0x8000) != 0;
        mods.alt = (api().GetKeyState(VK_MENU) & 0x8000) != 0;
        events_.on_key(key_of(wparam), mods, down, down && (lparam & (1 << 30)) != 0);
        return true;
      }
      case WM_CHAR: {
        // UTF-16 code units: a surrogate pair is two messages. Control codes
        // (Ctrl+letter, Enter, Tab, Backspace) are keys, not text.
        const auto unit = static_cast<char32_t>(wparam);
        if (unit >= 0xD800 && unit <= 0xDBFF) {
          high_surrogate_ = unit;
        } else if (unit >= 0xDC00 && unit <= 0xDFFF) {
          if (high_surrogate_ != 0) {
            events_.on_text(0x10000 + ((high_surrogate_ - 0xD800) << 10) + (unit - 0xDC00));
          }
          high_surrogate_ = 0;
        } else if (unit >= 0x20 && unit != 0x7f) {
          events_.on_text(unit);
        }
        return true;
      }
      case WM_CLOSE:
        if (events_.on_close_request()) {
          api().DestroyWindow(hwnd_);
        }
        return true;
      case WM_DESTROY:
        api().PostQuitMessage(0);
        return true;
      default:
        return false;
    }
  }

  static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    win32_api& a = api();
    if (message == WM_NCCREATE) {
      const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
      a.SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    }
    auto* w = reinterpret_cast<win32_window*>(a.GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (w != nullptr) {
      LRESULT result = 0;
      try {
        if (message == WM_MOUSEMOVE) {
          w->last_x_ = static_cast<float>(GET_X_LPARAM(lparam));
          w->last_y_ = static_cast<float>(GET_Y_LPARAM(lparam));
        }
        if (w->on_message(message, wparam, lparam, result)) {
          return result;
        }
      } catch (...) {
        // An exception must not travel through Windows' dispatch: pump() rethrows it.
        w->error_ = std::current_exception();
        a.PostQuitMessage(1);
        return 0;
      }
    }
    return a.DefWindowProcW(hwnd, message, wparam, lparam);
  }

  window_events& events_;
  HINSTANCE instance_ = nullptr;
  bool registered_ = false;
  HWND hwnd_ = nullptr;
  HDC dc_ = nullptr;
  HGLRC context_ = nullptr;
  HICON big_icon_ = nullptr; ///< Owned; destroyed after the window.
  HICON small_icon_ = nullptr; ///< Owned; destroyed after the window.
  bool ready_ = false;
  bool tracking_ = false; ///< A WM_MOUSELEAVE is pending.
  int buttons_down_ = 0;
  char32_t high_surrogate_ = 0; ///< First half of a surrogate pair typed, waiting for its second.
  float last_x_ = 0.0f; ///< Last pointer position, client pixels.
  float last_y_ = 0.0f;
  std::exception_ptr error_;
};

} // namespace

std::unique_ptr<native_window> create_native_window(window_events& events) {
  return std::make_unique<win32_window>(events);
}

} // namespace bdg::nymph
