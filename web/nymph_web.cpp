// MIT License © 2026 Binary Dice Games
/// @file nymph_web.cpp
/// @brief The C exports of the WebAssembly module: a table of live views
///        driven by web/js/nymph-viewer.js (spec/04_web_viewer/DESIGN.md
///        section 4, "Web module"). Internal: the JavaScript module is the
///        public contract.
///
/// Every export catches every exception (CLAUDE.md, C ABI boundaries) and
/// reports it through nymph_last_error(). Strings and buffers handed back
/// stay valid until the next call of the same kind.
#include <emscripten/emscripten.h>
#include <cstdint>
#include <cstdio>
#include <map>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>
#include "core/error.hpp"
#include "fonts/embedded_fonts.hpp"
#include "nymph.hpp"
#include "render/image.hpp"
#include "render/soft_rasterizer.hpp"
#include "view/live_view.hpp"
#include "webgl_presenter.hpp"

namespace {

using namespace bdg::nymph;

// ── JSON ─────────────────────────────────────────────────────────────────────

/// Appends `s` as a JSON string literal.
void json_string(std::string& out, std::string_view s) {
  out += '"';
  for (const char c : s) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", c);
          out += buf;
        } else {
          out += c;
        }
    }
  }
  out += '"';
}

void json_number(std::string& out, double v) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.17g", v);
  out += buf;
}

/// `{"kind":...,"warnings":[{line,column,message}...]}` and, with
/// `source`, the converted source first.
std::string import_json(const mermaid_import& m, bool with_source) {
  std::string out = "{";
  if (with_source) {
    out += "\"source\":";
    json_string(out, m.source);
    out += ',';
  }
  out += "\"kind\":";
  json_string(out, m.kind);
  out += ",\"warnings\":[";
  for (size_t i = 0; i < m.warnings.size(); ++i) {
    out += i == 0 ? "" : ",";
    out += "{\"line\":" + std::to_string(m.warnings[i].line) + ",\"column\":" + std::to_string(m.warnings[i].column) +
        ",\"message\":";
    json_string(out, m.warnings[i].message);
    out += '}';
  }
  return out + "]}";
}

void json_pick(std::string& out, const std::optional<view_pick>& p) {
  if (!p) {
    out += "null";
    return;
  }
  out += "{\"element\":";
  json_string(out, p->element);
  out += ",\"type\":";
  json_string(out, p->type);
  out += ",\"label\":";
  json_string(out, p->label);
  out += ",\"index\":" + std::to_string(p->index) + ",\"values\":{";
  for (size_t i = 0; i < p->values.size(); ++i) {
    out += i == 0 ? "" : ",";
    json_string(out, p->values[i].first);
    out += ':';
    json_string(out, p->values[i].second);
  }
  out += "}}";
}

// ── Viewers ──────────────────────────────────────────────────────────────────

struct viewer {
  live_view view;
  image pixels; ///< The last software frame (WebGL: its size only).
  bool webgl = false;
  /// What the last set_source() converted from mermaid
  /// (spec/16_mermaid_import/DESIGN.md section 5.5): JSON, or "null".
  std::string imported = "null";
  explicit viewer(view_options o) : view(o) {}
};

std::map<int, std::unique_ptr<viewer>>& viewers() {
  static std::map<int, std::unique_ptr<viewer>> table;
  return table;
}

viewer& get(int handle) {
  const auto it = viewers().find(handle);
  if (it == viewers().end()) {
    throw std::invalid_argument("no viewer " + std::to_string(handle));
  }
  return *it->second;
}

std::string& last_error() {
  static std::string json = "null";
  return json;
}

// ── Keyboard (spec/09_ui_mockups/DESIGN.md section 6) ────────────────────────

/// ImGui's key for a DOM KeyboardEvent.key: the editing keys a text field
/// needs and the letters and digits of Ctrl shortcuts; ImGuiKey_None for the
/// others (their characters arrive as text).
ImGuiKey dom_key(std::string_view name) {
  static const std::map<std::string_view, ImGuiKey> named{
      {"Tab", ImGuiKey_Tab},
      {"ArrowLeft", ImGuiKey_LeftArrow},
      {"ArrowRight", ImGuiKey_RightArrow},
      {"ArrowUp", ImGuiKey_UpArrow},
      {"ArrowDown", ImGuiKey_DownArrow},
      {"PageUp", ImGuiKey_PageUp},
      {"PageDown", ImGuiKey_PageDown},
      {"Home", ImGuiKey_Home},
      {"End", ImGuiKey_End},
      {"Insert", ImGuiKey_Insert},
      {"Delete", ImGuiKey_Delete},
      {"Backspace", ImGuiKey_Backspace},
      {"Enter", ImGuiKey_Enter},
      {"Escape", ImGuiKey_Escape},
      {" ", ImGuiKey_Space},
  };
  if (const auto it = named.find(name); it != named.end()) {
    return it->second;
  }
  if (name.size() == 1) {
    const char c = name[0];
    if (c >= 'a' && c <= 'z') {
      return static_cast<ImGuiKey>(ImGuiKey_A + (c - 'a'));
    }
    if (c >= 'A' && c <= 'Z') {
      return static_cast<ImGuiKey>(ImGuiKey_A + (c - 'A'));
    }
    if (c >= '0' && c <= '9') {
      return static_cast<ImGuiKey>(ImGuiKey_0 + (c - '0'));
    }
  }
  return ImGuiKey_None;
}

/// The code points of UTF-8 `text` (invalid bytes are skipped).
std::u32string utf32(std::string_view text) {
  std::u32string out;
  for (size_t i = 0; i < text.size();) {
    const auto b = static_cast<unsigned char>(text[i]);
    const int extra = b < 0x80 ? 0 : (b >> 5) == 0x6 ? 1 : (b >> 4) == 0xE ? 2 : (b >> 3) == 0x1E ? 3 : -1;
    if (extra < 0 || i + static_cast<size_t>(extra) >= text.size() + (extra == 0 ? 1 : 0)) {
      ++i;
      continue;
    }
    char32_t c = extra == 0 ? b : b & (0x3F >> extra);
    for (int k = 1; k <= extra; ++k) {
      c = (c << 6) | (static_cast<unsigned char>(text[i + static_cast<size_t>(k)]) & 0x3F);
    }
    out += c;
    i += static_cast<size_t>(extra) + 1;
  }
  return out;
}

/// What the figure's text fields last copied: ImGui reads the clipboard
/// synchronously, the browser's is asynchronous, so pasting from outside
/// comes in as typed text (the page's paste event).
std::string& copied() {
  static std::string text;
  return text;
}

std::string& scratch() {
  static std::string text;
  return text;
}

std::vector<uint8_t>& png_buffer() {
  static std::vector<uint8_t> bytes;
  return bytes;
}

/// The status codes the exports return.
enum status : int { kOk = 0, kError = 1, kFontsNeeded = 2, kPlainPng = 3 };

/// Runs `f`, turning exceptions into a status and last_error().
template <typename F>
int guarded(F&& f) {
  last_error() = "null";
  try {
    return f();
  } catch (const fonts_needed& e) {
    std::string& j = last_error();
    j = "{\"kind\":\"fonts\",\"message\":";
    json_string(j, e.what());
    j += ",\"fonts\":[";
    for (size_t i = 0; i < e.fonts().size(); ++i) {
      j += i == 0 ? "" : ",";
      json_string(j, e.fonts()[i]);
    }
    j += "]}";
    return kFontsNeeded;
  } catch (const error& e) {
    std::string& j = last_error();
    j = "{\"kind\":\"source\",\"line\":" + std::to_string(e.line()) + ",\"column\":" + std::to_string(e.column()) +
        ",\"message\":";
    json_string(j, e.what());
    j += '}';
    return kError;
  } catch (const std::exception& e) {
    std::string& j = last_error();
    j = "{\"kind\":\"other\",\"message\":";
    json_string(j, e.what());
    j += '}';
    return kError;
  } catch (...) {
    last_error() = "{\"kind\":\"other\",\"message\":\"unknown error\"}";
    return kError;
  }
}

} // namespace

extern "C" {

/// @brief The last error as JSON ({kind, message, line, column, fonts}),
///        or "null".
EMSCRIPTEN_KEEPALIVE const char* nymph_last_error() {
  return last_error().c_str();
}

/// @brief nymph's version, "MAJOR.MINOR.PATCH".
EMSCRIPTEN_KEEPALIVE const char* nymph_version() {
  static const std::string v(version());
  return v.c_str();
}

/// @brief Every catalog font as JSON: [{name, size, embedded}].
EMSCRIPTEN_KEEPALIVE const char* nymph_font_catalog() {
  std::string& j = scratch();
  j = "[";
  for (const font_entry& f : font_catalog()) {
    j += j.size() == 1 ? "{\"name\":" : ",{\"name\":";
    json_string(j, f.name);
    j += ",\"size\":" + std::to_string(f.size) + ",\"available\":" + (font_data(f.name).empty() ? "false" : "true") +
        "}";
  }
  j += ']';
  return j.c_str();
}

/// @brief register_font(): 0, or 1 with last_error().
EMSCRIPTEN_KEEPALIVE int nymph_register_font(const char* name, const uint8_t* bytes, int size) {
  return guarded([&] {
    register_font(name, std::span<const uint8_t>(bytes, static_cast<size_t>(size)));
    return kOk;
  });
}

/// @brief Creates the shared WebGL2 context on `target` (a key of
///        Emscripten's specialHTMLTargets). 1 on success, 0 without WebGL2.
EMSCRIPTEN_KEEPALIVE int nymph_gl_init(const char* target) {
  return web::gl_init(target) ? 1 : 0;
}

/// @brief A new viewer; returns its handle (> 0), or 0 with last_error().
/// @param fit 0 source, 1 width, 2 fill, 3 scale.
/// @param webgl draw with the shared WebGL2 context (nymph_gl_init() first);
///        else on the CPU.
EMSCRIPTEN_KEEPALIVE int nymph_viewer_create(int fit, int tooltips, int webgl) {
  int handle = 0;
  guarded([&] {
    static int next = 1;
    view_options o;
    o.fit = fit == 1 ? view_fit::width : (fit == 2 ? view_fit::fill : (fit == 3 ? view_fit::scale : view_fit::source));
    o.tooltips = tooltips != 0;
    const bool gl = webgl != 0 && web::gl_ready();
    o.cpu_textures = !gl;
    auto v = std::make_unique<viewer>(o);
    v->webgl = gl;
    v->view.set_clipboard_hooks(
        [](std::string_view text) {
          copied() = std::string(text);
          // The page's clipboard too, where the browser allows it.
          EM_ASM(
              {
                if (typeof navigator !== 'undefined' && navigator.clipboard) {
                  navigator.clipboard.writeText(UTF8ToString($0, $1)).catch(() => {});
                }
              },
              copied().data(),
              copied().size());
        },
        [] { return copied(); });
    if (gl) {
      web::gl_attach(v->view);
    }
    handle = next++;
    viewers()[handle] = std::move(v);
    return kOk;
  });
  return handle;
}

/// @brief Whether the viewer draws with WebGL (1) or on the CPU (0).
EMSCRIPTEN_KEEPALIVE int nymph_viewer_is_webgl(int handle) {
  const auto it = viewers().find(handle);
  return it != viewers().end() && it->second->webgl ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE void nymph_viewer_destroy(int handle) {
  viewers().erase(handle);
}

/// @brief set_source(): 0, 1 (error) or 2 (fonts needed), with last_error().
EMSCRIPTEN_KEEPALIVE int nymph_viewer_set_source(int handle, const char* text, int size, int reset_view) {
  return guarded([&] {
    viewer& v = get(handle);
    const std::string_view source(text, static_cast<size_t>(size));
    if (is_mermaid(source)) {
      // Mermaid is converted here; the viewer holds nymph YAML.
      const mermaid_import m = import_mermaid(source);
      v.view.set_source(m.source, reset_view != 0);
      v.imported = import_json(m, false);
    } else {
      v.view.set_source(source, reset_view != 0);
      v.imported = "null";
    }
    return kOk;
  });
}

/// @brief set_data(): as nymph_viewer_set_source().
EMSCRIPTEN_KEEPALIVE int nymph_viewer_set_data(int handle, const char* text, int size) {
  return guarded([&] {
    get(handle).view.set_data(std::string_view(text, static_cast<size_t>(size)));
    return kOk;
  });
}

/// @brief set_png(): as nymph_viewer_set_source(), or 3 for a PNG without a
///        nymph source.
EMSCRIPTEN_KEEPALIVE int nymph_viewer_set_png(int handle, const uint8_t* bytes, int size, int reset_view) {
  return guarded([&] {
    get(handle).imported = "null";
    const bool nymph =
        get(handle).view.set_png(std::span<const uint8_t>(bytes, static_cast<size_t>(size)), reset_view != 0);
    return nymph ? kOk : kPlainPng;
  });
}

/// @brief set_theme(): "" for the source's.
EMSCRIPTEN_KEEPALIVE int nymph_viewer_set_theme(int handle, const char* theme) {
  return guarded([&] {
    get(handle).view.set_theme(theme);
    return kOk;
  });
}

EMSCRIPTEN_KEEPALIVE int nymph_viewer_resize(int handle, float width, float height, float pixel_ratio) {
  return guarded([&] {
    get(handle).view.resize({width, height, pixel_ratio});
    return kOk;
  });
}

/// @brief One pointer event; `type` 0 move, 1 down, 2 up, 3 wheel, 4 leave.
EMSCRIPTEN_KEEPALIVE int
nymph_viewer_pointer(int handle, int type, float x, float y, int button, float wheel_x, float wheel_y, int touch) {
  return guarded([&] {
    pointer_event e;
    e.type = static_cast<pointer_event::kind>(type < 0 || type > 4 ? 0 : type);
    e.x = x;
    e.y = y;
    e.button = button;
    e.wheel_x = wheel_x;
    e.wheel_y = wheel_y;
    e.touch = touch != 0;
    get(handle).view.input(e);
    return kOk;
  });
}

/// @brief One key going down (`down` 1) or up; `name` is the DOM
///        KeyboardEvent.key, `mods` bits 1 Ctrl, 2 Shift, 4 Alt, 8 Meta.
EMSCRIPTEN_KEEPALIVE int nymph_viewer_key(int handle, const char* name, int down, int mods) {
  return guarded([&] {
    key_event e;
    e.key = dom_key(name);
    e.down = down != 0;
    e.ctrl = (mods & 1) != 0;
    e.shift = (mods & 2) != 0;
    e.alt = (mods & 4) != 0;
    e.super = (mods & 8) != 0;
    get(handle).view.input_key(e);
    return kOk;
  });
}

/// @brief Typed or pasted text (UTF-8) for the figure's active text field.
EMSCRIPTEN_KEEPALIVE int nymph_viewer_text(int handle, const char* utf8) {
  return guarded([&] {
    get(handle).view.input_text(utf32(utf8));
    return kOk;
  });
}

/// @brief 1 while a text field of the figure has the keyboard.
EMSCRIPTEN_KEEPALIVE int nymph_viewer_wants_keyboard(int handle) {
  const auto it = viewers().find(handle);
  return it != viewers().end() && it->second->view.wants_keyboard() ? 1 : 0;
}

/// @brief 1 when the wheel at (x, y), CSS pixels, would pan or zoom
///        something (the host then keeps it from scrolling the page).
EMSCRIPTEN_KEEPALIVE int nymph_viewer_takes_wheel(int handle, float x, float y) {
  const auto it = viewers().find(handle);
  return it != viewers().end() && it->second->view.takes_wheel(x, y) ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE int nymph_viewer_reset_view(int handle) {
  return guarded([&] {
    get(handle).view.reset_view();
    return kOk;
  });
}

EMSCRIPTEN_KEEPALIVE int nymph_viewer_wants_frame(int handle) {
  const auto it = viewers().find(handle);
  return it != viewers().end() && it->second->view.wants_frame() ? 1 : 0;
}

/// @brief Runs a due frame and draws it with the software rasterizer.
/// @return 1 when new pixels are ready (nymph_viewer_pixels()), 0 when no
///         frame was due, -1 on error (last_error()).
EMSCRIPTEN_KEEPALIVE int nymph_viewer_frame_software(int handle, double now_seconds) {
  int drawn = 0;
  const int s = guarded([&] {
    viewer& v = get(handle);
    if (const ImDrawData* d = v.view.frame(now_seconds)) {
      v.pixels = rasterize(*d, v.view.textures(), v.view.clear_color());
      drawn = 1;
    }
    return kOk;
  });
  return s == kOk ? drawn : -1;
}

/// @brief Runs a due frame and draws it into the bottom-left corner of the
///        shared WebGL canvas (nymph_viewer_pixel_width() x _height()).
/// @return 1 when drawn (copy it out before yielding), 0 when no frame was
///         due, -1 on error (last_error()).
EMSCRIPTEN_KEEPALIVE int nymph_viewer_frame_webgl(int handle, double now_seconds) {
  int drawn = 0;
  const int s = guarded([&] {
    viewer& v = get(handle);
    if (!v.webgl) {
      throw std::logic_error("the viewer draws on the CPU");
    }
    if (const ImDrawData* d = v.view.frame(now_seconds)) {
      // ImGui's backend takes the draw data mutable; it is the view's own.
      web::gl_draw(v.view, *const_cast<ImDrawData*>(d), &v.pixels.width, &v.pixels.height);
      v.pixels.rgba.clear();
      drawn = 1;
    }
    return kOk;
  });
  return s == kOk ? drawn : -1;
}

/// @brief The last software frame: RGBA, nymph_viewer_pixel_width() x
///        nymph_viewer_pixel_height().
EMSCRIPTEN_KEEPALIVE const uint8_t* nymph_viewer_pixels(int handle) {
  const auto it = viewers().find(handle);
  return it == viewers().end() ? nullptr : it->second->pixels.rgba.data();
}

EMSCRIPTEN_KEEPALIVE int nymph_viewer_pixel_width(int handle) {
  const auto it = viewers().find(handle);
  return it == viewers().end() ? 0 : it->second->pixels.width;
}

EMSCRIPTEN_KEEPALIVE int nymph_viewer_pixel_height(int handle) {
  const auto it = viewers().find(handle);
  return it == viewers().end() ? 0 : it->second->pixels.height;
}

/// @brief The viewer's state as JSON: geometry, figure, description,
///        hovered pick, the click since the last call (taken), plot views.
EMSCRIPTEN_KEEPALIVE const char* nymph_viewer_state(int handle) {
  std::string& j = scratch();
  j = "null";
  guarded([&] {
    live_view& v = get(handle).view;
    const view_geometry g = v.geometry();
    j = "{\"geometry\":{\"layoutWidth\":";
    json_number(j, g.layout_width);
    j += ",\"layoutHeight\":";
    json_number(j, g.layout_height);
    j += ",\"cssScale\":";
    json_number(j, g.css_scale);
    j += ",\"framebufferScale\":";
    json_number(j, g.framebuffer_scale);
    j += ",\"pixelWidth\":" + std::to_string(g.pixel_width) + ",\"pixelHeight\":" + std::to_string(g.pixel_height) +
        ",\"fitFallback\":" + (g.fit_fallback ? "true" : "false") + "}";
    j += std::string(",\"hasFigure\":") + (v.has_figure() ? "true" : "false");
    j += std::string(",\"wantsFrame\":") + (v.wants_frame() ? "true" : "false");
    j += std::string(",\"wantsKeyboard\":") + (v.wants_keyboard() ? "true" : "false");
    j += ",\"uiRevision\":" + std::to_string(v.ui_revision());
    j += ",\"description\":";
    json_string(j, v.description());
    j += ",\"hovered\":";
    json_pick(j, v.hovered());
    j += ",\"click\":";
    json_pick(j, v.take_click());
    j += ",\"views\":[";
    const std::vector<plot_view> views = v.view_state();
    for (size_t i = 0; i < views.size(); ++i) {
      const plot_view& p = views[i];
      j += i == 0 ? "{\"element\":" : ",{\"element\":";
      json_string(j, p.element);
      j += ",\"kind\":";
      json_string(j, p.kind);
      const std::pair<const char*, double> fields[] = {
          {"x", p.x},
          {"y", p.y},
          {"width", p.width},
          {"height", p.height},
          {"xMin", p.x_min},
          {"xMax", p.x_max},
          {"yMin", p.y_min},
          {"yMax", p.y_max}};
      for (const auto& [name, value] : fields) {
        j += std::string(",\"") + name + "\":";
        json_number(j, value);
      }
      if (p.has_y2) {
        j += ",\"y2Min\":";
        json_number(j, p.y2_min);
        j += ",\"y2Max\":";
        json_number(j, p.y2_max);
      }
      if (p.kind == "plot3d") {
        j += ",\"zMin\":";
        json_number(j, p.z_min);
        j += ",\"zMax\":";
        json_number(j, p.z_max);
        j += ",\"rotation\":[";
        for (size_t q = 0; q < 4; ++q) {
          j += q == 0 ? "" : ",";
          json_number(j, p.rotation[q]);
        }
        j += ']';
      }
      if (p.kind == "diagram") {
        j += ",\"zoom\":";
        json_number(j, p.zoom);
        j += ",\"center\":[";
        json_number(j, p.center[0]);
        j += ',';
        json_number(j, p.center[1]);
        j += ']';
      }
      j += ",\"hidden\":[";
      for (size_t h = 0; h < p.hidden.size(); ++h) {
        j += h == 0 ? "" : ",";
        json_string(j, p.hidden[h]);
      }
      j += "]}";
    }
    j += "]}";
    return kOk;
  });
  return j.c_str();
}

/// @brief What the last set_source() converted from mermaid, as JSON
///        `{kind, warnings}`, or "null" for a nymph source.
EMSCRIPTEN_KEEPALIVE const char* nymph_viewer_imported(int handle) {
  std::string& j = scratch();
  j = "null";
  guarded([&] {
    j = get(handle).imported;
    return kOk;
  });
  return j.c_str();
}

/// @brief Converts a mermaid diagram: JSON `{source, kind, warnings}`, or ""
///        with last_error() (kind "source", the mermaid line and column).
EMSCRIPTEN_KEEPALIVE const char* nymph_import_mermaid(const char* text, int size) {
  std::string& j = scratch();
  j.clear();
  guarded([&] {
    j = import_json(import_mermaid(std::string_view(text, static_cast<size_t>(size))), true);
    return kOk;
  });
  return j.c_str();
}

/// @brief The viewer's source (with set_data() applied).
EMSCRIPTEN_KEEPALIVE const char* nymph_viewer_source(int handle) {
  std::string& j = scratch();
  j.clear();
  guarded([&] {
    j = std::string(get(handle).view.source());
    return kOk;
  });
  return j.c_str();
}

/// @brief The viewer's source with the view on screen saved in its `view`
///        key (live_view::source_with_view()); "" with last_error() when it
///        cannot be written.
EMSCRIPTEN_KEEPALIVE const char* nymph_viewer_yaml(int handle) {
  std::string& j = scratch();
  j.clear();
  guarded([&] {
    j = get(handle).view.source_with_view();
    return kOk;
  });
  return j.c_str();
}

/// @brief render_png() of nymph_viewer_yaml(): the image on screen, its
///        source embedded. 0 with the bytes in nymph_png_data() /
///        nymph_png_size(), or 1 with last_error().
EMSCRIPTEN_KEEPALIVE int nymph_viewer_to_png(int handle) {
  return guarded([&] {
    png_buffer() = render_png(get(handle).view.source_with_view());
    return kOk;
  });
}

EMSCRIPTEN_KEEPALIVE const uint8_t* nymph_png_data() {
  return png_buffer().data();
}

EMSCRIPTEN_KEEPALIVE int nymph_png_size() {
  return static_cast<int>(png_buffer().size());
}

} // extern "C"
