// MIT License © 2026 Binary Dice Games
/// @file live_view.cpp
/// @brief live_view: persistent contexts, sizing, and frames on demand
///        (spec/04_web_viewer/DESIGN.md section 4).
#include "view/live_view.hpp"
#include <imgui_internal.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <optional>
#include <set>
#include <unordered_map>
#include <variant>
#include <utility>
#include "core/error.hpp"
#include "document/view.hpp"
#include "document/yaml_doc.hpp"
#include "edit/image_fields.hpp"
#include "edit/source_edit.hpp"
#include "document/bind.hpp"
#include "elements/plot_pick.hpp"
#include "elements/register_builtin.hpp"
#include "fonts/fonts_used.hpp"
#include "layout/layout.hpp"
#include "nymph.hpp"
#include "render/draw_context.hpp"
#include "render/figure_renderer.hpp"
#include "render/frame_setup.hpp"
#include "themes/theme_catalog.hpp"

namespace bdg::nymph {

namespace {

/// Largest device pixel ratio used (DESIGN.md section 3, "Sizing").
constexpr float kMaxPixelRatio = 3.0f;
/// Largest side of the drawn image in device pixels (as render_frames()).
constexpr float kMaxPixels = 8192.0f;
/// Frames run after an input: ImPlot sees the hover one frame after the
/// pointer moves, ImGui hides a new tooltip window on its first frame, and
/// ImPlot applies a fit one frame late.
constexpr int kInputFrames = 3;

/// Paths of every node under `n` ("root/children/1/children/0").
void index_paths(const node& n, const std::string& path, std::unordered_map<const node*, std::string>& out) {
  out[&n] = path;
  for (size_t i = 0; i < n.children.size(); ++i) {
    index_paths(n.children[i], path + "/children/" + std::to_string(i), out);
  }
}

/// Pixels added around a text target when hit-testing.
constexpr float kTextSlop = 3.0f;

/// The first non-empty text among the `title`, `text`, `label` and `id`
/// fields of `n`, for the editor's tree.
std::string node_label(const node& n) {
  for (const char* name : {"title", "text", "label", "id"}) {
    const int index = n.cls->field_index(name);
    if (index >= 0 && std::holds_alternative<std::string>(n.values[static_cast<size_t>(index)])) {
      const std::string& text = n.get<std::string>(static_cast<size_t>(index));
      if (!text.empty()) {
        return text;
      }
    }
  }
  return {};
}

void list_elements(const node& n, const std::string& path, int depth, std::vector<edit_element>& out) {
  out.push_back({path, std::string(n.cls->name()), node_label(n), depth});
  for (size_t i = 0; i < n.children.size(); ++i) {
    list_elements(n.children[i], path + "/children/" + std::to_string(i), depth + 1, out);
  }
}

/// A dashed rectangle outline.
void dashed_rect(ImDrawList* dl, const ImVec2& a, const ImVec2& b, ImU32 color) {
  constexpr float kDash = 4.0f, kGap = 3.0f;
  const auto line = [&](ImVec2 p, ImVec2 q) {
    const float len = std::hypot(q.x - p.x, q.y - p.y);
    if (len <= 0.0f) {
      return;
    }
    const ImVec2 d((q.x - p.x) / len, (q.y - p.y) / len);
    for (float t = 0.0f; t < len; t += kDash + kGap) {
      const float e = std::min(t + kDash, len);
      dl->AddLine(ImVec2(p.x + d.x * t, p.y + d.y * t), ImVec2(p.x + d.x * e, p.y + d.y * e), color, 1.0f);
    }
  };
  line(a, ImVec2(b.x, a.y));
  line(ImVec2(b.x, a.y), b);
  line(b, ImVec2(a.x, b.y));
  line(ImVec2(a.x, b.y), a);
}

/// `v` to six significant digits, as the saved view keeps numbers readable.
double six_digits(double v) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.6g", v);
  return std::strtod(buf, nullptr);
}

/// The `view` entry of a user-interface element the user changed
/// (spec/09_ui_mockups/DESIGN.md section 4, "Saved view"): only what the
/// user changed of a window; a widget's current state.
element_view ui_view(view_kind kind, const ui_entry& e) {
  element_view v;
  switch (kind) {
    case view_kind::ui_window:
      if (e.moved) {
        v.position = std::array<double, 2>{std::round(e.pos.x), std::round(e.pos.y)};
      }
      if (e.resized) {
        v.size = std::array<double, 2>{std::round(e.size_full.x), std::round(e.size_full.y)};
      }
      if (e.collapse_changed) {
        v.collapsed = e.collapsed;
      }
      if (e.closed) {
        v.closed = true;
      }
      if (e.scrolled) {
        v.scroll = std::array<double, 2>{std::round(e.scroll.x), std::round(e.scroll.y)};
      }
      break;
    case view_kind::ui_scroll:
      if (e.scrolled) {
        v.scroll = std::array<double, 2>{std::round(e.scroll.x), std::round(e.scroll.y)};
      }
      break;
    case view_kind::ui_open:
    case view_kind::ui_flag:
    case view_kind::ui_active:
    case view_kind::ui_checked:
      v.flag = e.flag;
      break;
    case view_kind::ui_selected:
      if (e.closed) {
        v.closed = true;
      } else {
        v.flag = e.flag;
      }
      break;
    case view_kind::ui_index:
      v.index = e.index;
      break;
    case view_kind::ui_number: {
      std::vector<double> numbers;
      for (const double d : e.numbers) {
        numbers.push_back(six_digits(d));
      }
      if (!numbers.empty()) {
        v.numbers = std::move(numbers);
      }
      break;
    }
    case view_kind::ui_text:
    case view_kind::ui_color:
      v.text = e.text;
      break;
    default:
      break;
  }
  return v;
}

std::string join(const std::vector<std::string>& names) {
  std::string out;
  for (const std::string& n : names) {
    out += (out.empty() ? "" : ", ") + n;
  }
  return out;
}

} // namespace

fonts_needed::fonts_needed(std::vector<std::string> fonts)
    : std::runtime_error("fonts needed: " + join(fonts)), fonts_(std::move(fonts)) {}

// ── impl ─────────────────────────────────────────────────────────────────────

struct live_view::impl {
  view_options options;
  view_size size;
  std::string theme_override;
  unsigned outline_rgb = 0x288cff; ///< set_outline_color()

  std::unique_ptr<figure> fig;
  std::string source;
  std::unordered_map<const node*, std::string> paths;
  std::vector<plot_view> views; ///< Of the last frame.
  std::vector<rect> input_areas; ///< Of the last frame, logical pixels.
  std::optional<view_pick> hovered;
  std::optional<view_pick> click;
  /// The saved-view state of every viewable element after the last frame,
  /// and the paths the user changed (they are saved by source_with_view()).
  view_map last_states;
  std::set<std::string> touched;
  /// The view to start from instead of the source's `view` key: the view
  /// on screen carried to renamed paths (set_source() with a rename).
  view_map start_views;
  int input_frames_left = 0;

  // Edit mode (spec/08_view_editor/DESIGN.md section 4).
  bool edit_mode = false;
  std::vector<edit_target> edit_targets; ///< Of the last frame, in drawing order.
  std::optional<edit_target_id> selection;
  std::optional<edit_target> edit_click;
  std::optional<yaml_node> edit_doc; ///< The source parsed, to see which fields are `$column` references.

  // The ImGui side; recreated by reset_contexts().
  std::unique_ptr<imgui_contexts> contexts;
  image_options frame_options; ///< What draw_context sees; updated in place.
  std::unique_ptr<draw_context> ctx;
  texture_store textures;
  std::string applied_theme; ///< style_key() the contexts are configured with.
  ImU32 clear = IM_COL32(255, 255, 255, 255);

  std::function<void()> on_create; ///< set_context_hooks()
  std::function<void()> on_destroy;
  std::function<void(std::string_view)> clipboard_set; ///< set_clipboard_hooks()
  std::function<std::string()> clipboard_get;
  std::string clipboard_text; ///< What clipboard_get() returned, for ImGui's pointer.
  bool wants_keyboard = false; ///< A text field had the keyboard in the last frame.
  uint64_t ui_revision = 0; ///< ui_revision()
  std::string ui_signature; ///< The touched UI state after the last frame, written as the view key.

  view_geometry geom;
  bool overflowed = false; ///< The fit overflowed at the current size; laid out at the source size.
  int pending_frames = 0;
  int fresh_frames = 0; ///< Frames run since the contexts were created.
  std::optional<double> last_time;

  explicit impl(view_options o) : options(o) {
    reset_contexts();
  }

  ~impl() {
    shutdown_contexts();
  }

  impl(const impl&) = delete;
  impl& operator=(const impl&) = delete;

  /// Lets a GPU presenter shut its backend down before the contexts go.
  void shutdown_contexts() {
    if (contexts && on_destroy) {
      const context_switch on(*contexts);
      on_destroy();
    }
  }

  const std::string& effective_theme() const {
    static const std::string kLight = "light";
    if (fig) {
      return fig->image.theme; // bound with the override, if any
    }
    return theme_override.empty() ? kLight : theme_override;
  }

  /// The options the styles come from: the figure's (its theme applied),
  /// or the override's base colours before there is a figure.
  image_options style_options() const {
    if (fig) {
      return fig->image;
    }
    image_options o;
    if (const chart_theme* t = find_chart_theme(effective_theme())) {
      o.base = t->base;
    }
    return o;
  }

  /// What configure_style() depends on, to notice a change.
  std::string style_key() const {
    const image_options o = style_options();
    std::string key = o.base + "|" + o.background;
    for (const std::string& c : o.palette) {
      key += "|" + c;
    }
    return key;
  }

  ImGuiBackendFlags backend_flags() const {
    return options.cpu_textures ? (ImGuiBackendFlags_RendererHasTextures | ImGuiBackendFlags_RendererHasVtxOffset) : 0;
  }

  /// Fresh contexts: drops every view state.
  void reset_contexts() {
    shutdown_contexts();
    ctx.reset();
    contexts = std::make_unique<imgui_contexts>();
    textures = texture_store();
    ctx = std::make_unique<draw_context>(frame_options);
    ctx->set_interactive(true);
    ctx->set_tooltips(options.tooltips);
    const context_switch on(*contexts);
    applied_theme = style_key();
    configure_backend("nymph_live_view", backend_flags(), options.cpu_textures);
    configure_style(style_options());
    load_fonts(*ctx);
    update_clear_color();
    install_clipboard();
    wants_keyboard = false;
    if (on_create) {
      on_create();
    }
    last_time.reset();
    fresh_frames = 0;
  }

  /// Routes ImGui's clipboard to the host's hooks, if any; call with the
  /// contexts current. Without hooks ImGui keeps its own, in this context.
  void install_clipboard() {
    if (!clipboard_set || !clipboard_get) {
      return;
    }
    ImGuiPlatformIO& pio = ImGui::GetPlatformIO();
    pio.Platform_ClipboardUserData = this;
    pio.Platform_SetClipboardTextFn = [](ImGuiContext*, const char* text) {
      static_cast<impl*>(ImGui::GetPlatformIO().Platform_ClipboardUserData)->clipboard_set(text);
    };
    pio.Platform_GetClipboardTextFn = [](ImGuiContext*) -> const char* {
      impl* m = static_cast<impl*>(ImGui::GetPlatformIO().Platform_ClipboardUserData);
      m->clipboard_text = m->clipboard_get();
      return m->clipboard_text.c_str();
    };
  }

  /// Re-applies the theme's styles if it changed; call with the contexts
  /// current.
  void apply_theme() {
    if (applied_theme == style_key()) {
      return;
    }
    applied_theme = style_key();
    configure_style(style_options());
    update_clear_color();
  }

  void update_clear_color() {
    ImVec4 bg = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
    bg.w = 1.0f; // the image is opaque
    clear = ImGui::ColorConvertFloat4ToU32(bg);
  }

  /// The geometry at the source size, shrunk with CSS to the box width
  /// (view_fit::scale: also grown to it).
  view_geometry source_geometry() const {
    view_geometry g;
    g.layout_width = static_cast<float>(fig->image.width);
    g.layout_height = static_cast<float>(fig->image.height);
    if (size.width > 0.0f) {
      const float to_box = size.width / g.layout_width;
      g.css_scale = options.fit == view_fit::scale ? to_box : std::min(1.0f, to_box);
    }
    return g;
  }

  /// Layout size and scales for the current figure, box and fit; sets
  /// frame_options to match.
  void update_geometry() {
    if (!fig) {
      return;
    }
    view_geometry g = source_geometry();
    bool fallback = overflowed;
    if (!overflowed && (options.fit == view_fit::width || options.fit == view_fit::fill) && size.width > 0.0f) {
      view_geometry fit;
      fit.layout_width = std::round(size.width);
      if (options.fit == view_fit::width) {
        fit.layout_height = std::round(g.layout_height * size.width / g.layout_width);
      } else {
        fit.layout_height = size.height > 0.0f ? std::round(size.height) : g.layout_height;
      }
      image_options candidate = fig->image;
      candidate.width = static_cast<int>(fit.layout_width);
      candidate.height = static_cast<int>(fit.layout_height);
      candidate.scale = 1;
      try {
        check_options(candidate);
        g = fit;
      } catch (const std::invalid_argument&) {
        fallback = true; // a box the image options cannot describe
      }
    }
    const float ratio = std::clamp(size.pixel_ratio, 0.25f, kMaxPixelRatio);
    g.framebuffer_scale = std::min({g.css_scale * ratio, kMaxPixels / g.layout_width, kMaxPixels / g.layout_height});
    g.pixel_width = static_cast<int>(std::lround(g.layout_width * g.framebuffer_scale));
    g.pixel_height = static_cast<int>(std::lround(g.layout_height * g.framebuffer_scale));
    g.fit_fallback = fallback;
    geom = g;

    frame_options = fig->image;
    frame_options.width = static_cast<int>(g.layout_width);
    frame_options.height = static_cast<int>(g.layout_height);
    frame_options.scale = 1; // the framebuffer scale goes to ImGui directly
  }

  /// What a recorded view would save, rounded to what matters on screen.
  static element_view to_view(const plot_view_record& r) {
    // To 10^-decimals, dividing by an exact power of ten so that 1.44
    // stays 1.44 (multiplying by 1e-4 would give 1.4400000000000002).
    const auto round_dec = [](double v, int decimals) {
      if (decimals >= 0) {
        const double p = std::pow(10.0, std::min(decimals, 300));
        return std::round(v * p) / p;
      }
      const double p = std::pow(10.0, -decimals);
      return std::round(v / p) * p;
    };
    // Six significant digits of the visible span.
    const auto range = [&](double lo, double hi) {
      const int decimals = 5 - static_cast<int>(std::floor(std::log10(std::max(hi - lo, 1e-300))));
      return view_range{round_dec(lo, decimals), round_dec(hi, decimals)};
    };
    element_view v;
    switch (r.kind) {
      case view_kind::plot2d:
        v.x = range(r.x_min, r.x_max);
        v.y = range(r.y_min, r.y_max);
        if (r.has_y2) {
          v.y2 = range(r.y2_min, r.y2_max);
        }
        break;
      case view_kind::gantt:
        v.x = range(r.x_min, r.x_max);
        break;
      case view_kind::plot3d:
        v.x = range(r.x_min, r.x_max);
        v.y = range(r.y_min, r.y_max);
        v.z = range(r.z_min, r.z_max);
        v.rotation = std::array<double, 4>{
            round_dec(r.rotation[0], 6),
            round_dec(r.rotation[1], 6),
            round_dec(r.rotation[2], 6),
            round_dec(r.rotation[3], 6)};
        break;
      case view_kind::diagram:
        v.zoom = round_dec(r.zoom, 4);
        v.center = std::array<double, 2>{round_dec(r.center[0], 2), round_dec(r.center[1], 2)};
        break;
      case view_kind::none:
        break;
    }
    for (const std::string& h : r.hidden) {
      if (std::find(v.hidden.begin(), v.hidden.end(), h) == v.hidden.end()) {
        v.hidden.push_back(h);
      }
    }
    return v;
  }

  /// Counts a drawn frame. ImGui takes queued input a step per frame (a
  /// move, then a wheel, ...): frames go on until the queue is empty, or a
  /// pinch's last zoom steps would wait for the next touch and zoom it
  /// instead of panning.
  void end_frame() {
    --pending_frames;
    if (!ImGui::GetCurrentContext()->InputEventsQueue.empty()) {
      pending_frames = std::max(pending_frames, kInputFrames);
      input_frames_left = kInputFrames + 1;
    }
  }

  /// Notes the elements whose view changed in the frames after an input.
  void track_changes() {
    view_map now;
    for (const plot_view_record& r : ctx->plot_views()) {
      const auto it = paths.find(r.element);
      if (it != paths.end()) {
        now[it->second] = to_view(r);
      }
    }
    if (input_frames_left > 0) {
      --input_frames_left;
      for (const auto& [path, v] : now) {
        const auto before = last_states.find(path);
        if (before != last_states.end() && !(before->second == v)) {
          touched.insert(path);
        }
      }
    }
    last_states = std::move(now);
  }

  /// The `view` entries of the UI elements the user changed, by path.
  view_map ui_changes(const std::map<std::string, const node*, std::less<>>& nodes) const {
    view_map out;
    for (const auto& [path, e] : ctx->ui().entries()) {
      const auto it = nodes.find(path);
      if (!e.touched || it == nodes.end() || it->second->cls->name() != e.type) {
        continue;
      }
      const element_view v = ui_view(it->second->cls->viewable(), e);
      if (!v.empty()) {
        out[path] = v;
      }
    }
    return out;
  }

  /// The view to save: the source's `view` entries (or the carried ones),
  /// with what the user changed since (plots' state, user-interface elements') over them.
  view_map saved_views(const std::map<std::string, const node*, std::less<>>& nodes) const {
    // After a rename the carried view is the base: it holds the source's
    // entries and what the user had changed before.
    view_map saved = start_views.empty() ? fig->views : start_views;
    for (const std::string& path : touched) {
      if (const auto it = last_states.find(path); it != last_states.end()) {
        saved[path] = it->second;
      }
    }
    // User-interface elements keep what the user changed in their state.
    for (const auto& [path, v] : ui_changes(nodes)) {
      element_view& entry = saved[path];
      // Merged over the source's own entry: a window moved now keeps a size
      // saved before.
      if (v.position) {
        entry.position = v.position;
      }
      if (v.size) {
        entry.size = v.size;
      }
      if (v.scroll) {
        entry.scroll = v.scroll;
      }
      if (v.collapsed) {
        entry.collapsed = v.collapsed;
      }
      if (v.closed) {
        entry.closed = v.closed;
      }
      if (v.flag) {
        entry.flag = v.flag;
      }
      if (v.index) {
        entry.index = v.index;
      }
      if (v.numbers) {
        entry.numbers = v.numbers;
      }
      if (v.text) {
        entry.text = v.text;
      }
    }
    return saved;
  }

  /// Bumps ui_revision when the UI state the user changed differs from the
  /// last frame's.
  void track_ui_changes() {
    if (ctx->ui().entries().empty()) {
      return;
    }
    const auto nodes = element_paths(fig->root);
    std::map<std::string, view_kind, std::less<>> kinds;
    for (const auto& [path, n] : nodes) {
      kinds[path] = n->cls->viewable();
    }
    std::string signature = write_view(ui_changes(nodes), kinds);
    if (signature != ui_signature) {
      ui_signature = std::move(signature);
      ++ui_revision;
    }
  }

  /// The targets at (`x`, `y`) CSS pixels among `edit_targets`, best first.
  std::vector<edit_target> targets_at_css(float x, float y) const {
    std::vector<size_t> found;
    for (size_t i = 0; i < edit_targets.size(); ++i) {
      const edit_target& t = edit_targets[i];
      const float slop = t.field.empty() ? 0.0f : kTextSlop;
      if (x >= t.x - slop && x <= t.x + t.width + slop && y >= t.y - slop && y <= t.y + t.height + slop) {
        found.push_back(i);
      }
    }
    std::sort(found.begin(), found.end(), [&](size_t a, size_t b) {
      const float area_a = edit_targets[a].width * edit_targets[a].height;
      const float area_b = edit_targets[b].width * edit_targets[b].height;
      return area_a != area_b ? area_a < area_b : a > b;
    });
    std::vector<edit_target> out;
    for (const size_t i : found) {
      const edit_target& t = edit_targets[i];
      const bool seen = std::any_of(out.begin(), out.end(), [&](const edit_target& o) {
        return o.element == t.element && o.field == t.field;
      });
      if (!seen) {
        out.push_back(t.field.empty() ? whole_element(t.element) : t);
      }
    }
    return out;
  }

  /// The element-level target of `element`: an element recorded in several
  /// pieces (a diagram link's path, a participant's boxes and lifeline) is
  /// hit piece by piece but outlined and reported as their union.
  edit_target whole_element(std::string_view element) const {
    std::optional<edit_target> whole;
    for (const edit_target& t : edit_targets) {
      if (t.element != element || !t.field.empty()) {
        continue;
      }
      if (!whole) {
        whole = t;
        continue;
      }
      const float x1 = std::max(whole->x + whole->width, t.x + t.width);
      const float y1 = std::max(whole->y + whole->height, t.y + t.height);
      whole->x = std::min(whole->x, t.x);
      whole->y = std::min(whole->y, t.y);
      whole->width = x1 - whole->x;
      whole->height = y1 - whole->y;
    }
    return whole ? *whole : edit_target{};
  }

  std::optional<edit_target> find_target(std::string_view element, std::string_view field) const {
    bool has_whole = false;
    for (const edit_target& t : edit_targets) {
      if (t.element != element) {
        continue;
      }
      if (t.field == field) {
        return field.empty() ? whole_element(element) : t;
      }
      has_whole = has_whole || t.field.empty();
    }
    if (field.empty() && has_whole) {
      return whole_element(element);
    }
    return std::nullopt;
  }

  /// "bound to $col" when the source writes field `field` of `element` as a
  /// column reference (its text comes from the data, not from the field).
  std::string bound_reason(const std::string& element, const std::string& field) {
    if (field.empty()) {
      return {};
    }
    if (!edit_doc) {
      try {
        edit_doc = parse_yaml(source);
      } catch (const error&) {
        return {};
      }
    }
    const std::optional<std::string> ref = field_reference(*edit_doc, element, field);
    return ref ? "bound to " + *ref : std::string();
  }

  /// Converts the frame's recorded targets to CSS pixels. The whole figure
  /// comes first as the image options' target: the smallest target wins, so
  /// it is what a click on the margin, away from every element, finds.
  void collect_targets() {
    edit_targets.clear();
    const float s = geom.css_scale;
    edit_target background;
    background.element = std::string(kImagePath);
    background.type = "Image";
    background.width = geom.layout_width * s;
    background.height = geom.layout_height * s;
    edit_targets.push_back(std::move(background));
    for (const edit_record& r : ctx->targets()) {
      const auto it = paths.find(r.element);
      if (it == paths.end()) {
        continue;
      }
      edit_target t;
      t.element = it->second;
      t.type = std::string(r.element->cls->name());
      t.field = r.field;
      t.text = r.text;
      t.read_only_reason = r.read_only_reason.empty() ? bound_reason(t.element, t.field) : r.read_only_reason;
      t.x = r.area.min.x * s;
      t.y = r.area.min.y * s;
      t.width = r.area.width() * s;
      t.height = r.area.height() * s;
      edit_targets.push_back(std::move(t));
    }
  }

  /// Outlines the target under the pointer (dashed) and the selection
  /// (solid) on the foreground draw list; inside the figure's frame.
  void draw_edit_overlay() {
    const ImGuiIO& io = ImGui::GetIO();
    const float s = geom.css_scale > 0.0f ? geom.css_scale : 1.0f;
    const auto to_logical = [&](const edit_target& t) {
      return std::pair<ImVec2, ImVec2>{ImVec2(t.x / s, t.y / s), ImVec2((t.x + t.width) / s, (t.y + t.height) / s)};
    };
    const auto outline = [&](int alpha) {
      return IM_COL32((outline_rgb >> 16) & 0xff, (outline_rgb >> 8) & 0xff, outline_rgb & 0xff, alpha);
    };
    const ImU32 hover_color = outline(200);
    const ImU32 select_color = outline(255);
    if (io.MousePos.x > -FLT_MAX / 2) {
      const std::vector<edit_target> under = targets_at_css(io.MousePos.x * s, io.MousePos.y * s);
      if (!under.empty()) {
        const auto [a, b] = to_logical(under.front());
        dashed_rect(ImGui::GetForegroundDrawList(), a, b, hover_color);
      }
    }
    if (selection) {
      if (const std::optional<edit_target> t = find_target(selection->element, selection->field)) {
        const auto [a, b] = to_logical(*t);
        const float grow = t->element == kImagePath ? -1.0f : 1.0f; // the figure's own edge stays inside it
        ImGui::GetForegroundDrawList()->AddRect(
            ImVec2(a.x - grow, a.y - grow), ImVec2(b.x + grow, b.y + grow), select_color, 0.0f, 0, 2.0f);
      }
    }
  }

  /// One frame in the current contexts.
  void run_frame(double now) {
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(geom.layout_width, geom.layout_height);
    io.DisplayFramebufferScale = ImVec2(geom.framebuffer_scale, geom.framebuffer_scale);
    const double step = last_time ? now - *last_time : 1.0 / 60.0;
    io.DeltaTime = static_cast<float>(std::clamp(step, 1e-4, 0.25));
    last_time = now;
    ctx->begin_frame();
    ctx->set_views(start_views.empty() ? &fig->views : &start_views, &paths);
    // Source limits stay fixed while fresh contexts settle (static parity
    // with render_figure()); then they let go for pan and zoom.
    ctx->set_lock_limits(fresh_frames < kFigureFrames);
    ctx->set_edit_mode(edit_mode);
    ++fresh_frames;
    const node& root = fig->root;
    // In edit mode a click selects; it must not move ImGui's focus away from
    // a mockup's window onto the figure's own (its title bar would turn
    // inactive, unlike the PNG of the edited source).
    ImGuiWindow* const focused_before = ImGui::GetCurrentContext()->NavWindow;
    run_figure_frame(frame_options, *ctx, [&](draw_context& dc) {
      layout_root(root, dc);
      dc.draw_node(root);
      if (edit_mode) {
        collect_targets();
        draw_edit_overlay();
      }
    });
    if (options.cpu_textures) {
      textures.apply_all(*ImGui::GetDrawData());
    }
    ImGuiWindow* const focused = ImGui::GetCurrentContext()->NavWindow;
    if (edit_mode && focused_before != nullptr && focused != focused_before && focused != nullptr &&
        std::string_view(focused->Name) == kFigureWindowName) {
      ImGui::FocusWindow(focused_before);
    }
    wants_keyboard = io.WantTextInput;
    track_ui_changes();
    hovered.reset();
    if (const std::optional<pick>& p = ctx->hovered()) {
      view_pick v;
      const auto it = paths.find(p->element);
      v.element = it == paths.end() ? std::string() : it->second;
      v.type = p->type;
      v.label = p->label;
      v.index = p->index;
      v.values = p->values;
      hovered = std::move(v);
    }
    // A click: the primary button released where it was pressed.
    const float slop = io.MouseDragThreshold;
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) &&
        io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Left] <= slop * slop && hovered) {
      click = hovered;
    }
    if (!edit_mode) {
      edit_targets.clear();
    } else if (
        ImGui::IsMouseReleased(ImGuiMouseButton_Left) && io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Left] <= slop * slop &&
        io.MousePos.x > -FLT_MAX / 2) {
      const float s = geom.css_scale > 0.0f ? geom.css_scale : 1.0f;
      const std::vector<edit_target> under = targets_at_css(io.MousePos.x * s, io.MousePos.y * s);
      if (!under.empty()) {
        edit_click = under.front();
      }
    }
    input_areas = ctx->input_areas();
    track_changes();
    views.clear();
    const float s = geom.css_scale;
    for (const plot_view_record& r : ctx->plot_views()) {
      plot_view v;
      const auto it = paths.find(r.element);
      v.element = it == paths.end() ? std::string() : it->second;
      v.x = r.area.min.x * s;
      v.y = r.area.min.y * s;
      v.width = r.area.width() * s;
      v.height = r.area.height() * s;
      v.x_min = r.x_min;
      v.x_max = r.x_max;
      v.y_min = r.y_min;
      v.y_max = r.y_max;
      v.has_y2 = r.has_y2;
      v.y2_min = r.y2_min;
      v.y2_max = r.y2_max;
      v.kind = r.kind == view_kind::gantt ? "gantt"
          : r.kind == view_kind::plot3d   ? "plot3d"
          : r.kind == view_kind::diagram  ? "diagram"
                                          : "plot";
      v.z_min = r.z_min;
      v.z_max = r.z_max;
      v.rotation = r.rotation;
      v.zoom = r.zoom;
      v.center = r.center;
      v.hidden = r.hidden;
      views.push_back(std::move(v));
    }
  }
};

// ── live_view ────────────────────────────────────────────────────────────────

live_view::live_view(view_options options) : impl_(std::make_unique<impl>(options)) {}
live_view::~live_view() = default;
live_view::live_view(live_view&&) noexcept = default;
live_view& live_view::operator=(live_view&&) noexcept = default;

void live_view::set_source_moved(std::string_view source, const path_rename& rename) {
  impl& m = *impl_;
  if (!m.fig) {
    set_source(source);
    return;
  }
  // The view on screen, under the paths its elements have in `source`.
  view_map carried;
  for (auto& [path, v] : m.saved_views(element_paths(m.fig->root))) {
    if (const std::optional<std::string> to = rename(path)) {
      carried[*to] = v;
    }
  }
  std::set<std::string> touched;
  for (const std::string& path : m.touched) {
    if (const std::optional<std::string> to = rename(path)) {
      touched.insert(*to);
    }
  }
  set_source(source, true); // throws before anything changes
  m.start_views = std::move(carried);
  m.touched = std::move(touched); // still the user's changes: a save keeps them
}

void live_view::set_source(std::string_view source, bool reset_view) {
  impl& m = *impl_;
  figure f = bind_source(source, builtin_registry(), bind_options{m.theme_override});
  if (std::vector<std::string> missing = fonts_missing(f); !missing.empty()) {
    throw fonts_needed(std::move(missing));
  }
  // A layout that does not fit the source's own size is an error of the
  // source; checked in separate contexts so the view state survives it.
  (void)layout_figure(f);

  // A different saved view is meant to be shown: start from it.
  if (m.fig && !(m.fig->views == f.views)) {
    reset_view = true;
  }
  m.fig = std::make_unique<figure>(std::move(f));
  m.source = std::string(source);
  m.paths.clear();
  index_paths(m.fig->root, "root", m.paths);
  m.views.clear();
  m.hovered.reset();
  m.click.reset();
  m.edit_click.reset();
  m.edit_targets.clear();
  m.edit_doc.reset();
  if (m.selection) {
    const auto gone = m.selection->element != kImagePath &&
        std::none_of(m.paths.begin(), m.paths.end(), [&](const auto& p) { return p.second == m.selection->element; });
    if (gone) {
      m.selection.reset();
    }
  }
  m.last_states.clear();
  if (reset_view) {
    m.touched.clear();
    m.start_views.clear();
  }
  m.overflowed = false;
  if (reset_view) {
    m.reset_contexts();
  } else {
    const context_switch on(*m.contexts);
    load_fonts(*m.ctx); // fonts registered since the last update
    m.apply_theme();
    // Series colours as the new source gives them, not as they were.
    m.ctx->set_rehidden(forget_plot_items());
  }
  m.update_geometry();
  m.pending_frames = kFigureFrames;
}

void live_view::set_data(std::string_view data_yaml) {
  impl& m = *impl_;
  if (!m.fig) {
    throw std::logic_error("set_data: no figure to replace the data of");
  }
  const data_replacement r = replace_data(m.source, data_yaml);
  try {
    set_source(r.source);
  } catch (const error& e) {
    if (e.line() >= r.first_line && e.line() < r.first_line + r.line_count) {
      // In the new data: say where in what the caller passed.
      throw error(e.line() - r.first_line + 1, std::max(1, e.column() - 2), std::string("data: ") + e.what());
    }
    throw;
  }
}

bool live_view::set_png(std::span<const uint8_t> png, bool reset_view) {
  impl& m = *impl_;
  const std::optional<std::string> source = extract_source(png);
  if (!source) {
    m.fig.reset();
    m.source.clear();
    m.paths.clear();
    m.views.clear();
    m.hovered.reset();
    m.click.reset();
    m.pending_frames = 0;
    return false;
  }
  set_source(*source, reset_view);
  return true;
}

void live_view::set_theme(std::string theme) {
  impl& m = *impl_;
  if (!theme.empty() && !is_known_theme(theme)) {
    throw std::invalid_argument("unknown theme '" + theme + "' (themes: " + chart_theme_list() + ")");
  }
  if (theme == m.theme_override) {
    return;
  }
  const std::string previous = m.theme_override;
  m.theme_override = std::move(theme);
  if (m.fig) {
    // The theme fills the fields the source leaves out: bind again.
    try {
      set_source(std::string(m.source), false);
    } catch (...) {
      m.theme_override = previous;
      throw;
    }
    return;
  }
  {
    const context_switch on(*m.contexts);
    m.apply_theme();
  }
  m.update_geometry();
  m.pending_frames = kFigureFrames;
}

std::string live_view::theme() const {
  return impl_->effective_theme();
}

std::string live_view::theme_base() const {
  return impl_->style_options().base;
}

void live_view::resize(view_size size) {
  impl& m = *impl_;
  m.size = size;
  m.overflowed = false;
  m.update_geometry();
  m.pending_frames = kFigureFrames;
}

void live_view::input(const pointer_event& e) {
  impl& m = *impl_;
  const context_switch on(*m.contexts);
  ImGuiIO& io = ImGui::GetIO();
  io.AddMouseSourceEvent(e.touch ? ImGuiMouseSource_TouchScreen : ImGuiMouseSource_Mouse);
  const float s = m.geom.css_scale > 0.0f ? m.geom.css_scale : 1.0f;
  if (e.type == pointer_event::kind::leave) {
    io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
  } else {
    io.AddMousePosEvent(e.x / s, e.y / s);
  }
  if (e.type == pointer_event::kind::down || e.type == pointer_event::kind::up) {
    if (e.button >= 0 && e.button < ImGuiMouseButton_COUNT) {
      io.AddMouseButtonEvent(e.button, e.type == pointer_event::kind::down);
    }
  } else if (e.type == pointer_event::kind::wheel) {
    io.AddMouseWheelEvent(e.wheel_x, e.wheel_y);
  }
  m.pending_frames = std::max(m.pending_frames, kInputFrames);
  m.input_frames_left = kInputFrames + 1; // ImPlot applies some changes a frame late
}

void live_view::input_key(const key_event& e) {
  impl& m = *impl_;
  const context_switch on(*m.contexts);
  ImGuiIO& io = ImGui::GetIO();
  io.AddKeyEvent(ImGuiMod_Ctrl, e.ctrl);
  io.AddKeyEvent(ImGuiMod_Shift, e.shift);
  io.AddKeyEvent(ImGuiMod_Alt, e.alt);
  io.AddKeyEvent(ImGuiMod_Super, e.super);
  if (e.key != ImGuiKey_None) {
    io.AddKeyEvent(e.key, e.down);
  }
  m.pending_frames = std::max(m.pending_frames, kInputFrames);
  m.input_frames_left = kInputFrames + 1;
}

void live_view::input_text(std::u32string_view text) {
  impl& m = *impl_;
  const context_switch on(*m.contexts);
  for (const char32_t c : text) {
    ImGui::GetIO().AddInputCharacter(static_cast<unsigned int>(c));
  }
  m.pending_frames = std::max(m.pending_frames, kInputFrames);
  m.input_frames_left = kInputFrames + 1;
}

uint64_t live_view::ui_revision() const {
  return impl_->ui_revision;
}

bool live_view::wants_keyboard() const {
  return impl_->wants_keyboard;
}

void live_view::set_clipboard_hooks(
    std::function<void(std::string_view)> set,
    std::function<std::string()> get) {
  impl& m = *impl_;
  m.clipboard_set = std::move(set);
  m.clipboard_get = std::move(get);
  const context_switch on(*m.contexts);
  m.install_clipboard();
}

void live_view::reset_view() {
  impl& m = *impl_;
  m.touched.clear();
  m.last_states.clear();
  m.reset_contexts();
  m.views.clear();
  m.update_geometry();
  m.pending_frames = kFigureFrames;
}

std::vector<plot_view> live_view::view_state() const {
  return impl_->views;
}

bool live_view::takes_wheel(float x, float y) const {
  const impl& m = *impl_;
  const float s = m.geom.css_scale > 0.0f ? m.geom.css_scale : 1.0f;
  const ImVec2 p(x / s, y / s);
  for (const rect& r : m.input_areas) {
    if (p.x >= r.min.x && p.x <= r.max.x && p.y >= r.min.y && p.y <= r.max.y) {
      return true;
    }
  }
  return false;
}

const std::optional<view_pick>& live_view::hovered() const {
  return impl_->hovered;
}

std::optional<view_pick> live_view::take_click() {
  std::optional<view_pick> out = std::move(impl_->click);
  impl_->click.reset();
  return out;
}

void live_view::set_edit_mode(bool on) {
  impl& m = *impl_;
  if (m.edit_mode == on) {
    return;
  }
  m.edit_mode = on;
  m.edit_click.reset();
  if (!on) {
    m.edit_targets.clear();
  }
  m.pending_frames = std::max(m.pending_frames, kInputFrames);
}

bool live_view::edit_mode() const {
  return impl_->edit_mode;
}

std::vector<edit_target> live_view::targets_at(float x, float y) const {
  return impl_->edit_mode ? impl_->targets_at_css(x, y) : std::vector<edit_target>();
}

std::optional<edit_target> live_view::target_for(std::string_view element, std::string_view field) const {
  return impl_->edit_mode ? impl_->find_target(element, field) : std::nullopt;
}

void live_view::set_outline_color(unsigned rgb) {
  impl& m = *impl_;
  if (m.outline_rgb == rgb) {
    return;
  }
  m.outline_rgb = rgb;
  m.pending_frames = std::max(m.pending_frames, 1);
}

void live_view::set_selection(std::optional<edit_target_id> selection) {
  impl& m = *impl_;
  if (m.selection == selection) {
    return;
  }
  m.selection = std::move(selection);
  m.pending_frames = std::max(m.pending_frames, 1);
}

const std::optional<edit_target_id>& live_view::selection() const {
  return impl_->selection;
}

std::vector<edit_element> live_view::element_list() const {
  std::vector<edit_element> out;
  if (impl_->fig) {
    out.push_back({std::string(kImagePath), "Image", "", 0});
    list_elements(impl_->fig->root, "root", 0, out);
  }
  return out;
}

std::optional<edit_target> live_view::take_edit_click() {
  std::optional<edit_target> out = std::move(impl_->edit_click);
  impl_->edit_click.reset();
  return out;
}

const ImDrawData* live_view::frame(double now_seconds) {
  impl& m = *impl_;
  if (!m.fig || m.pending_frames <= 0) {
    return nullptr;
  }
  try {
    const context_switch on(*m.contexts);
    m.run_frame(now_seconds);
    m.end_frame();
    return ImGui::GetDrawData();
  } catch (const error&) {
    // The frame is left open: these contexts cannot be used again.
    m.reset_contexts();
    if (m.geom.layout_width == static_cast<float>(m.fig->image.width) &&
        m.geom.layout_height == static_cast<float>(m.fig->image.height)) {
      throw; // fails at the source size too (set_source() checked it, so a bug)
    }
    // The fit overflowed at this box size: lay out at the source size.
    m.overflowed = true;
    m.update_geometry();
    m.pending_frames = kFigureFrames;
  }
  const context_switch on(*m.contexts);
  m.run_frame(now_seconds);
  m.end_frame();
  return ImGui::GetDrawData();
}

void live_view::with_contexts(const std::function<void()>& f) {
  const context_switch on(*impl_->contexts);
  f();
}

void live_view::set_context_hooks(std::function<void()> on_create, std::function<void()> on_destroy) {
  impl& m = *impl_;
  m.on_create = std::move(on_create);
  m.on_destroy = std::move(on_destroy);
  if (m.on_create) {
    const context_switch on(*m.contexts);
    m.on_create();
  }
}

bool live_view::wants_frame() const {
  return impl_->fig != nullptr && impl_->pending_frames > 0;
}

ImU32 live_view::clear_color() const {
  return impl_->clear;
}

view_geometry live_view::geometry() const {
  return impl_->geom;
}

bool live_view::has_figure() const {
  return impl_->fig != nullptr;
}

const std::string& live_view::description() const {
  static const std::string kEmpty;
  return impl_->fig ? impl_->fig->description : kEmpty;
}

std::string_view live_view::source() const {
  return impl_->source;
}

std::string live_view::source_with_view() const {
  const impl& m = *impl_;
  if (!m.fig) {
    return m.source;
  }
  const auto nodes = element_paths(m.fig->root);
  std::map<std::string, view_kind, std::less<>> kinds;
  for (const auto& [path, n] : nodes) {
    kinds[path] = n->cls->viewable();
  }
  const view_map saved = m.saved_views(nodes);
  const std::string text = write_view(saved, kinds);
  if (text == write_view(m.fig->views, kinds)) {
    return m.source; // nothing changed: the source as written
  }
  return replace_top_level(m.source, "view", text).source;
}

const texture_store& live_view::textures() const {
  return impl_->textures;
}

} // namespace bdg::nymph
