// MIT License © 2026 Binary Dice Games
/// @file nymph_editor_web.cpp
/// @brief The C exports of the editor's WebAssembly module: one
///        editor_workspace (figure, panel, routing) drawn into its own WebGL2
///        canvas, driven by web/js/nymph-editor.js
///        (spec/15_web_app/DESIGN.md sections 5.2 and 5.3). Internal: the
///        JavaScript module is the public contract.
///
/// One editor per module instance. Every export catches every exception
/// (CLAUDE.md, C ABI boundaries) and reports it through
/// nymph_editor_last_error(). Strings and buffers handed back stay valid
/// until the next call of the same kind. Events go to the page through
/// Module.nymphEditorEvent(kind, json).
#include <GLES3/gl3.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <cstdint>
#include <map>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include "core/error.hpp"
#include "fonts/embedded_fonts.hpp"
#include "mcp/editor_tools.hpp"
#include "nymph.hpp"
#include "view/live_view.hpp"
#include "workspace/editor_workspace.hpp"
#include "workspace/workspace_tools.hpp"

namespace {

using namespace bdg::nymph;
using mcp::json;

// ── Events to the page ───────────────────────────────────────────────────────

/// Calls Module.nymphEditorEvent(kind, payload) when the page set it.
void send(const char* kind, const std::string& payload) {
  EM_ASM(
      {
        if (Module['nymphEditorEvent']) {
          Module['nymphEditorEvent'](UTF8ToString($0), UTF8ToString($1, $2));
        }
      },
      kind,
      payload.data(),
      payload.size());
}

/// The page's clipboard as last seen: ImGui reads it synchronously, the
/// browser's is asynchronous, so the page hands over a paste's text before
/// the Ctrl+V key (nymph_editor_set_clipboard()).
std::string& page_clipboard() {
  static std::string text;
  return text;
}

// ── The browser host ─────────────────────────────────────────────────────────

class web_host : public workspace_host {
 public:
  void set_cursor(cursor_shape shape) override {
    send("cursor", shape == cursor_shape::resize_horizontal ? "\"col-resize\"" : shape == cursor_shape::text ? "\"text\"" : "\"default\"");
  }
  void request_save() override {
    send("save", "{}");
  }
  void request_csv_import(const std::string& table) override {
    send("csv-import", json{{"table", table}}.dump());
  }
  void request_csv_export(const std::string& table, const std::string& file_name, const std::string& csv) override {
    send("csv-export", json{{"table", table}, {"fileName", file_name}, {"csv", csv}}.dump());
  }
  void changed(const commit_result& r) override {
    pending_change = json{{"label", r.label}, {"origin", r.origin}};
  }
  void need_fonts(const std::vector<std::string>& fonts) override {
    send("fonts", json(fonts).dump());
  }
  bool has_clipboard() const override {
    return true;
  }
  void set_clipboard(std::string_view text) override {
    page_clipboard() = std::string(text);
    send("copy", json(std::string(text)).dump());
  }
  std::string clipboard() override {
    return page_clipboard();
  }
  void make_current() override {
    if (context > 0) {
      emscripten_webgl_make_context_current(context);
    }
  }

  EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context = 0;
  std::optional<json> pending_change; ///< Sent after the call that made it, with the new version.
};

struct editor {
  web_host host;
  std::unique_ptr<editor_workspace> ws;
  std::unique_ptr<workspace_tools> tools;
  bool panel_wanted = true;
  std::string file_name = "chart.yaml";
  std::optional<edit_target_id> last_selection;
  int pixel_w = 0;
  int pixel_h = 0;
};

std::unique_ptr<editor>& the_editor() {
  static std::unique_ptr<editor> e;
  return e;
}

editor& get() {
  if (!the_editor()) {
    throw std::logic_error("no editor: call nymph_editor_create() first");
  }
  return *the_editor();
}

/// Sends the change (and its new version) the last call made, if any.
void flush_events(editor& e) {
  if (e.host.pending_change) {
    json payload = *e.host.pending_change;
    e.host.pending_change.reset();
    payload["version"] = e.ws->panel() != nullptr ? e.ws->panel()->model().version() : 0;
    payload["modified"] = e.ws->modified();
    send("change", payload.dump());
  }
  const std::optional<edit_target_id> selection =
      e.ws->panel() != nullptr ? e.ws->panel()->model().selection() : std::nullopt;
  const bool same = selection.has_value() == e.last_selection.has_value() &&
      (!selection || (selection->element == e.last_selection->element && selection->field == e.last_selection->field));
  if (!same) {
    e.last_selection = selection;
    send("selection", selection ? json{{"path", selection->element}, {"field", selection->field}}.dump() : "null");
  }
}

// ── Errors ───────────────────────────────────────────────────────────────────

std::string& last_error() {
  static std::string j = "null";
  return j;
}

std::string& scratch() {
  static std::string text;
  return text;
}

std::vector<uint8_t>& png_buffer() {
  static std::vector<uint8_t> bytes;
  return bytes;
}

enum status : int { kOk = 0, kError = 1, kFontsNeeded = 2, kPlainPng = 3 };

template <typename F>
int guarded(F&& f) {
  last_error() = "null";
  try {
    const int s = f();
    if (the_editor() && the_editor()->ws) {
      flush_events(*the_editor());
    }
    return s;
  } catch (const fonts_needed& e) {
    last_error() = json{{"kind", "fonts"}, {"message", e.what()}, {"fonts", e.fonts()}}.dump();
    return kFontsNeeded;
  } catch (const error& e) {
    last_error() = json{{"kind", "source"}, {"line", e.line()}, {"column", e.column()}, {"message", e.what()}}.dump();
    return kError;
  } catch (const std::exception& e) {
    last_error() = json{{"kind", "other"}, {"message", e.what()}}.dump();
    return kError;
  } catch (...) {
    last_error() = R"({"kind":"other","message":"unknown error"})";
    return kError;
  }
}

// ── Keys ─────────────────────────────────────────────────────────────────────

/// The workspace key for a DOM KeyboardEvent.key.
window_key dom_key(std::string_view name) {
  static const std::map<std::string_view, window_key> named{
      {"Escape", window_key::escape},
      {"Enter", window_key::enter},
      {"Tab", window_key::tab},
      {"Backspace", window_key::backspace},
      {"Delete", window_key::del},
      {"ArrowLeft", window_key::left},
      {"ArrowRight", window_key::right},
      {"ArrowUp", window_key::up},
      {"ArrowDown", window_key::down},
      {"Home", window_key::home},
      {"End", window_key::end},
      {"PageUp", window_key::page_up},
      {"PageDown", window_key::page_down},
  };
  if (const auto it = named.find(name); it != named.end()) {
    return it->second;
  }
  if (name.size() == 1) {
    switch (name[0] | 0x20) { // lower case
      case 'a': return window_key::a;
      case 'c': return window_key::c;
      case 'd': return window_key::d;
      case 'e': return window_key::e;
      case 'f': return window_key::f;
      case 'h': return window_key::h;
      case 'q': return window_key::q;
      case 'r': return window_key::r;
      case 's': return window_key::s;
      case 'u': return window_key::u;
      case 'v': return window_key::v;
      case 'x': return window_key::x;
      case 'y': return window_key::y;
      case 'z': return window_key::z;
      default: break;
    }
  }
  return window_key::other;
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

/// Shows `source` as a new document (no undo history), the panel as asked;
/// with `repair`, a source that does not bind opens in repair state.
void open_source(editor& e, std::string_view source, bool reset_view, bool repair) {
  const bool shown = e.ws->panel() != nullptr && e.ws->panel()->source_shown();
  if (repair && e.panel_wanted) {
    e.ws->open(std::string(source), reset_view, true);
  } else {
    e.ws->view().set_source(source, reset_view); // throws before anything changes
    e.ws->open(std::string(source), reset_view, e.panel_wanted);
  }
  if (shown && e.ws->panel() != nullptr) {
    e.ws->show_source(true);
  }
  e.last_selection.reset();
}

} // namespace

extern "C" {

/// @brief The last error as JSON ({kind, message, line, column, fonts}), or "null".
EMSCRIPTEN_KEEPALIVE const char* nymph_editor_last_error() {
  return last_error().c_str();
}

EMSCRIPTEN_KEEPALIVE const char* nymph_editor_version() {
  static const std::string v(version());
  return v.c_str();
}

/// @brief Every catalog font as JSON [{name, size, embedded}], for the page's
///        font loader.
EMSCRIPTEN_KEEPALIVE const char* nymph_editor_font_catalog() {
  json list = json::array();
  for (const font_entry& f : font_catalog()) {
    list.push_back({{"name", f.name}, {"size", f.size}, {"embedded", f.data != nullptr}});
  }
  scratch() = list.dump();
  return scratch().c_str();
}

EMSCRIPTEN_KEEPALIVE int nymph_editor_register_font(const char* name, const uint8_t* bytes, int size) {
  return guarded([&] {
    register_font(name, std::span<const uint8_t>(bytes, static_cast<size_t>(size)));
    return kOk;
  });
}

/// @brief Creates the editor: a WebGL2 context on `target` (a key of
///        Emscripten's specialHTMLTargets), or none when `headless` (frames
///        run, nothing is drawn: tests).
EMSCRIPTEN_KEEPALIVE int nymph_editor_create(const char* target, int headless, int panel) {
  return guarded([&] {
    the_editor().reset();
    auto e = std::make_unique<editor>();
    e->panel_wanted = panel != 0;
    workspace_gl gl;
    std::string glsl;
    if (headless == 0) {
      EmscriptenWebGLContextAttributes attrs;
      emscripten_webgl_init_context_attributes(&attrs);
      attrs.majorVersion = 2;
      attrs.minorVersion = 0;
      attrs.alpha = false;
      attrs.depth = false;
      attrs.stencil = false;
      attrs.antialias = false; // ImGui anti-aliases with its own geometry
      attrs.preserveDrawingBuffer = false;
      e->host.context = emscripten_webgl_create_context(target, &attrs);
      if (e->host.context <= 0) {
        throw std::runtime_error("this browser cannot draw with WebGL2, which the editor needs");
      }
      emscripten_webgl_make_context_current(e->host.context);
      gl.viewport = [](int x, int y, int w, int h) { glViewport(x, y, w, h); };
      gl.clear_color = [](float r, float g, float b, float a) { glClearColor(r, g, b, a); };
      gl.clear = [](unsigned mask) { glClear(mask); };
      gl.read_pixels = [](int x, int y, int w, int h, unsigned f, unsigned t, void* p) { glReadPixels(x, y, w, h, f, t, p); };
      gl.pixel_store = [](unsigned p, int v) { glPixelStorei(p, v); };
      glsl = "#version 300 es";
    }
    view_options options{view_fit::fill, /*cpu_textures=*/headless != 0, /*tooltips=*/true};
    e->ws = std::make_unique<editor_workspace>(e->host, options, glsl, gl);
    e->ws->init_view();
    e->ws->set_host_files(true);
    editor* raw = e.get();
    e->tools = std::make_unique<workspace_tools>(*e->ws, [raw] { return raw->file_name; });
    the_editor() = std::move(e);
    return kOk;
  });
}

EMSCRIPTEN_KEEPALIVE void nymph_editor_destroy() {
  if (the_editor()) {
    the_editor()->host.make_current();
    the_editor()->tools.reset();
    the_editor()->ws.reset();
    if (the_editor()->host.context > 0) {
      emscripten_webgl_destroy_context(the_editor()->host.context);
    }
    the_editor().reset();
  }
}

EMSCRIPTEN_KEEPALIVE int nymph_editor_set_file_name(const char* name) {
  return guarded([&] {
    get().file_name = name;
    get().ws->set_file_name(name);
    return kOk;
  });
}

/// @brief Opens `text` as a new document: 0, 1 (does not bind) or 2 (fonts needed).
EMSCRIPTEN_KEEPALIVE int nymph_editor_open(const char* text, int size, int reset_view, int repair) {
  return guarded([&] {
    editor& e = get();
    e.host.make_current();
    open_source(e, std::string_view(text, static_cast<size_t>(size)), reset_view != 0, repair != 0);
    return kOk;
  });
}

/// @brief Shows or hides the source pane; `line` > 0 puts its cursor there.
EMSCRIPTEN_KEEPALIVE int nymph_editor_show_source(int on, int line) {
  return guarded([&] {
    get().ws->show_source(on != 0, line > 0 ? std::optional<int>(line) : std::nullopt);
    return kOk;
  });
}

/// @brief The figure again after the fonts a "fonts" event named were registered.
EMSCRIPTEN_KEEPALIVE int nymph_editor_retry_view() {
  return guarded([&] {
    get().host.make_current();
    get().ws->retry_view();
    return kOk;
  });
}

/// @brief Opens the source a nymph PNG carries; 3 for a PNG without one.
EMSCRIPTEN_KEEPALIVE int nymph_editor_open_png(const uint8_t* bytes, int size) {
  return guarded([&] {
    const std::optional<std::string> source = extract_source(std::span<const uint8_t>(bytes, static_cast<size_t>(size)));
    if (!source) {
      return static_cast<int>(kPlainPng);
    }
    editor& e = get();
    e.host.make_current();
    open_source(e, *source, true, false);
    return static_cast<int>(kOk);
  });
}

/// @brief Replaces the source as one undo step (an edit made elsewhere).
EMSCRIPTEN_KEEPALIVE int nymph_editor_change_source(const char* text, int size, const char* label, const char* origin) {
  return guarded([&] {
    editor& e = get();
    if (e.ws->panel() == nullptr) {
      throw std::runtime_error("the editor panel is hidden");
    }
    editor_model& m = e.ws->panel()->model();
    e.ws->flush_pending();
    m.set_origin(origin);
    m.set_label(label);
    const commit_result r = m.set_source(std::string(text, static_cast<size_t>(size)));
    m.set_origin("user");
    m.set_label("");
    if (!r.ok) {
      throw error(r.line, r.column, r.error);
    }
    e.host.make_current();
    e.ws->apply_change(r, std::string());
    return kOk;
  });
}

EMSCRIPTEN_KEEPALIVE int nymph_editor_show_panel(int on) {
  return guarded([&] {
    editor& e = get();
    e.panel_wanted = on != 0;
    e.host.make_current();
    const std::string refused = e.ws->show_panel(on != 0);
    if (!refused.empty()) {
      throw std::runtime_error("cannot edit: " + refused);
    }
    return kOk;
  });
}

EMSCRIPTEN_KEEPALIVE int nymph_editor_set_theme(const char* theme) {
  return guarded([&] {
    get().ws->view().set_theme(theme);
    return kOk;
  });
}

/// @brief The panel's look, the page's: `dark` 0 or 1, `accent` and
///        `accent_dark` 0xRRGGBB (the page's accent in its light and dark
///        palettes; negative: nymph's green).
EMSCRIPTEN_KEEPALIVE int nymph_editor_set_appearance(int dark, int accent, int accent_dark) {
  return guarded([&] {
    panel_appearance a;
    a.dark = dark != 0;
    if (accent >= 0) {
      a.accent = static_cast<unsigned>(accent);
    }
    if (accent_dark >= 0) {
      a.accent_dark = static_cast<unsigned>(accent_dark);
    }
    get().ws->set_appearance(a);
    return kOk;
  });
}

EMSCRIPTEN_KEEPALIVE int nymph_editor_resize(float width, float height, float pixel_ratio) {
  return guarded([&] {
    editor& e = get();
    e.ws->resize(width, height, pixel_ratio);
    e.pixel_w = static_cast<int>(width * pixel_ratio + 0.5f);
    e.pixel_h = static_cast<int>(height * pixel_ratio + 0.5f);
    return kOk;
  });
}

/// @brief One pointer event, CSS pixels; `type` 0 move, 1 down, 2 up, 3 wheel, 4 leave.
EMSCRIPTEN_KEEPALIVE int nymph_editor_pointer(int type, float x, float y, int button, float wheel_x, float wheel_y, int touch) {
  return guarded([&] {
    pointer_event p;
    p.type = static_cast<pointer_event::kind>(type < 0 || type > 4 ? 0 : type);
    p.x = x;
    p.y = y;
    p.button = button;
    p.wheel_x = wheel_x;
    p.wheel_y = wheel_y;
    p.touch = touch != 0;
    get().ws->pointer(p);
    return kOk;
  });
}

/// @brief A key; `name` is the DOM KeyboardEvent.key, `mods` bits 1 Ctrl
///        (or Cmd), 2 Shift, 4 Alt. @return 1 when the editor used it (the
///        page then prevents the browser's default), 0 if not, -1 on error.
EMSCRIPTEN_KEEPALIVE int nymph_editor_key(const char* name, int down, int repeat, int mods) {
  int used = 0;
  const int s = guarded([&] {
    const key_mods m{(mods & 1) != 0, (mods & 2) != 0, (mods & 4) != 0};
    used = get().ws->key(dom_key(name), m, down != 0, repeat != 0) == key_outcome::consumed ? 1 : 0;
    return kOk;
  });
  return s == kOk ? used : -1;
}

/// @brief Typed text (UTF-8) for the field that has the keyboard.
EMSCRIPTEN_KEEPALIVE int nymph_editor_text(const char* utf8) {
  return guarded([&] {
    for (char32_t c : utf32(utf8)) {
      get().ws->text(c);
    }
    return kOk;
  });
}

/// @brief The page's clipboard text, before a paste key reaches the editor.
EMSCRIPTEN_KEEPALIVE void nymph_editor_set_clipboard(const char* utf8) {
  page_clipboard() = utf8;
}

EMSCRIPTEN_KEEPALIVE int nymph_editor_wants_keyboard() {
  return the_editor() && the_editor()->ws->wants_keyboard() ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE int nymph_editor_wants_frame() {
  return the_editor() && the_editor()->ws->wants_frame() ? 1 : 0;
}

/// @brief Runs due frames and draws them. @return 1 drawn, 0 nothing due, -1 error.
EMSCRIPTEN_KEEPALIVE int nymph_editor_frame(double now_seconds) {
  int drawn = 0;
  const int s = guarded([&] {
    editor& e = get();
    if (!e.ws->wants_frame()) {
      return kOk;
    }
    e.host.make_current();
    e.ws->frame(now_seconds);
    e.ws->render(e.pixel_w, e.pixel_h);
    drawn = 1;
    return kOk;
  });
  return s == kOk ? drawn : -1;
}

/// @brief The current source (without the view).
EMSCRIPTEN_KEEPALIVE const char* nymph_editor_source() {
  scratch().clear();
  guarded([&] {
    editor& e = get();
    scratch() = e.ws->panel() != nullptr ? e.ws->panel()->model().source() : std::string(e.ws->view().source());
    return kOk;
  });
  return scratch().c_str();
}

/// @brief The source pane's text (it differs from nymph_editor_source()
///        while typing waits for its pause, or does not bind).
EMSCRIPTEN_KEEPALIVE const char* nymph_editor_pane_text() {
  scratch().clear();
  guarded([&] {
    editor& e = get();
    scratch() = e.ws->panel() != nullptr ? e.ws->panel()->pane().text() : std::string();
    return kOk;
  });
  return scratch().c_str();
}

/// @brief The source with the view on screen in its `view` key.
EMSCRIPTEN_KEEPALIVE const char* nymph_editor_yaml() {
  scratch().clear();
  guarded([&] {
    scratch() = get().ws->view().source_with_view();
    return kOk;
  });
  return scratch().c_str();
}

/// @brief render_png() of nymph_editor_yaml(): its size, the bytes at
///        nymph_editor_png_data(); -1 on error.
EMSCRIPTEN_KEEPALIVE int nymph_editor_to_png() {
  int size = -1;
  guarded([&] {
    png_buffer() = render_png(get().ws->view().source_with_view());
    size = static_cast<int>(png_buffer().size());
    return kOk;
  });
  return size;
}

EMSCRIPTEN_KEEPALIVE const uint8_t* nymph_editor_png_data() {
  return png_buffer().data();
}

/// @brief The editor's state as JSON: modified, version, undo / redo labels,
///        selection, panel shown, strip width, tab height, keyboard.
EMSCRIPTEN_KEEPALIVE const char* nymph_editor_state() {
  scratch() = "null";
  guarded([&] {
    editor& e = get();
    json j{{"modified", e.ws->modified()},
           {"panel", e.ws->panel_shown()},
           {"stripWidth", e.ws->strip_width()},
           {"tabHeight", e.ws->tab_height()},
           {"repairing", e.ws->repairing()},
           {"viewWantsFrame", e.ws->view().wants_frame()},
           {"panelWantsFrame", e.ws->panel() != nullptr && e.ws->panel()->wants_frame()},
           {"sourceShown", e.ws->panel() != nullptr && e.ws->panel()->source_shown()},
           {"locked", e.ws->panel() != nullptr ? e.ws->panel()->locked_reason() : std::string()},
           {"wantsKeyboard", e.ws->wants_keyboard()}};
    if (const editor_panel* p = e.ws->panel()) {
      const editor_model& m = const_cast<editor_panel*>(p)->model();
      j["version"] = m.version();
      j["undo"] = m.undo_label();
      j["redo"] = m.redo_label();
      j["selection"] = m.selection() ? json{{"path", m.selection()->element}, {"field", m.selection()->field}} : json();
      j["error"] = const_cast<editor_panel*>(p)->panel().error();
    }
    scratch() = j.dump();
    return kOk;
  });
  return scratch().c_str();
}

/// @brief Where element `path`'s `field` ("" the element) is drawn, CSS
///        pixels in the editor's canvas, as JSON {x, y, width, height}, or "null".
EMSCRIPTEN_KEEPALIVE const char* nymph_editor_target(const char* path, const char* field) {
  scratch() = "null";
  guarded([&] {
    editor& e = get();
    const std::optional<edit_target> t = e.ws->view().target_for(path, field);
    if (t) {
      const float x0 = e.ws->panel() != nullptr ? e.ws->panel()->figure_box().x : 0.0f;
      const float y0 = e.ws->panel() != nullptr ? e.ws->panel()->figure_box().y : 0.0f;
      scratch() = json{{"x", x0 + t->x}, {"y", y0 + t->y}, {"width", t->width}, {"height", t->height}}.dump();
    }
    return kOk;
  });
  return scratch().c_str();
}

/// @brief Runs editor tool `name` with JSON `args`, changes attributed to
///        `origin`: the CallToolResult as JSON.
EMSCRIPTEN_KEEPALIVE const char* nymph_editor_tool(const char* name, const char* args, const char* origin) {
  scratch() = "null";
  guarded([&] {
    editor& e = get();
    e.host.make_current();
    const json parsed = json::parse(args, nullptr, false);
    if (parsed.is_discarded()) {
      throw std::invalid_argument("the arguments are not JSON");
    }
    scratch() = mcp::call_editor_tool(*e.tools, name, parsed, origin).dump();
    return kOk;
  });
  return scratch().c_str();
}

/// @brief The editor tools' tools/list entries as JSON.
EMSCRIPTEN_KEEPALIVE const char* nymph_editor_tool_definitions() {
  scratch() = mcp::editor_tool_definitions().dump();
  return scratch().c_str();
}

EMSCRIPTEN_KEEPALIVE int nymph_editor_begin_group(const char* label, const char* origin) {
  return guarded([&] {
    editor& e = get();
    if (e.ws->panel() != nullptr) {
      e.ws->flush_pending();
      e.ws->panel()->model().begin_group(label, origin);
    }
    return kOk;
  });
}

EMSCRIPTEN_KEEPALIVE int nymph_editor_end_group() {
  return guarded([&] {
    editor& e = get();
    if (e.ws->panel() != nullptr) {
      e.ws->panel()->model().end_group();
    }
    return kOk;
  });
}

EMSCRIPTEN_KEEPALIVE int nymph_editor_undo() {
  return guarded([&] {
    get().host.make_current();
    get().ws->undo();
    return kOk;
  });
}

EMSCRIPTEN_KEEPALIVE int nymph_editor_redo() {
  return guarded([&] {
    get().host.make_current();
    get().ws->redo();
    return kOk;
  });
}

EMSCRIPTEN_KEEPALIVE int nymph_editor_reset_view() {
  return guarded([&] {
    get().ws->reset_view();
    return kOk;
  });
}

EMSCRIPTEN_KEEPALIVE int nymph_editor_mark_saved() {
  return guarded([&] {
    get().ws->mark_saved();
    return kOk;
  });
}

/// @brief Replaces table `table` with CSV text (the answer to a csv-import event).
EMSCRIPTEN_KEEPALIVE int nymph_editor_import_csv(const char* table, const char* csv) {
  return guarded([&] {
    get().host.make_current();
    const std::string refused = get().ws->import_csv(table, csv);
    if (!refused.empty()) {
      throw std::runtime_error(refused);
    }
    return kOk;
  });
}

} // extern "C"
