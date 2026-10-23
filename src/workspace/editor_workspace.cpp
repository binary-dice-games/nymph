// MIT License © 2026 Binary Dice Games
/// @file editor_workspace.cpp
/// @brief The editor's host logic shared by `nymph view` and the web editor
///        (see editor_workspace.hpp; spec/15_web_app/DESIGN.md section 5.1).
#include "workspace/editor_workspace.hpp"
#include <imgui_impl_opengl3.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include "core/error.hpp"
#include "edit/data_edit.hpp"
#include "edit/image_fields.hpp"
#include "nymph.hpp"
#include "png/png_meta.hpp"

namespace bdg::nymph {

namespace {

constexpr unsigned kColorBufferBit = 0x00004000;
constexpr unsigned kRgba = 0x1908; // GL_RGBA
constexpr unsigned kUnsignedByte = 0x1401; // GL_UNSIGNED_BYTE
constexpr unsigned kPackAlignment = 0x0D05; // GL_PACK_ALIGNMENT

template <typename Fn>
void assign(Fn& fn, void* address) {
  static_assert(sizeof(Fn) == sizeof(void*), "function and data pointers differ in size");
  std::memcpy(&fn, &address, sizeof(fn));
}

} // namespace

double workspace_now() {
  using clock = std::chrono::steady_clock;
  return std::chrono::duration<double>(clock::now().time_since_epoch()).count();
}

bool workspace_gl::load(const std::function<void*(const char*)>& load) {
  assign(viewport, load("glViewport"));
  assign(clear_color, load("glClearColor"));
  assign(clear, load("glClear"));
  assign(read_pixels, load("glReadPixels"));
  assign(pixel_store, load("glPixelStorei"));
  return viewport != nullptr && clear_color != nullptr && clear != nullptr;
}

editor_workspace::editor_workspace(workspace_host& host, view_options options, std::string glsl_version, workspace_gl gl)
    : host_(host), glsl_version_(std::move(glsl_version)), gl_(gl), view_(options) {}

editor_workspace::~editor_workspace() {
  panel_.reset(); // its backend, while the context exists
}

void editor_workspace::init_view() {
  if (glsl_version_.empty()) {
    return; // headless: the view keeps its CPU textures, nothing is drawn
  }
  const std::string glsl = glsl_version_;
  view_.set_context_hooks(
      [glsl] {
        if (!ImGui_ImplOpenGL3_Init(glsl.c_str())) {
          throw std::runtime_error("the OpenGL driver cannot run the viewer");
        }
        // Normally made by ImGui_ImplOpenGL3_NewFrame(), which nymph's own
        // frame loop does not call.
        ImGui_ImplOpenGL3_CreateDeviceObjects();
      },
      [] { ImGui_ImplOpenGL3_Shutdown(); });
}

void editor_workspace::set_saved_source(std::string source) {
  saved_source_ = std::move(source);
  if (panel_ != nullptr) {
    panel_->model().set_original(saved_source_);
  }
}

void editor_workspace::set_host_files(bool on) {
  host_files_ = on;
  if (panel_ != nullptr) {
    panel_->grid().set_host_files(on);
  }
}

void editor_workspace::open(const std::string& source, bool reset_view, bool edit) {
  try {
    view_.set_source(source, reset_view);
  } catch (const fonts_needed&) {
    throw;
  } catch (const error&) {
    if (!edit) {
      throw;
    }
    // Repair state: the panel holds the text, the figure shows nothing.
    show_panel(false);
    saved_source_ = source;
    repair_source_ = source;
    const std::string refused = show_panel(true, true);
    repair_source_.reset();
    if (!refused.empty()) {
      throw;
    }
    return;
  }
  show_panel(false);
  saved_source_ = source;
  if (edit) {
    const std::string refused = show_panel(true);
    if (!refused.empty()) {
      throw std::runtime_error("cannot edit: " + refused);
    }
  }
}

void editor_workspace::retry_view() {
  if (panel_ == nullptr || panel_->model().repairing()) {
    return;
  }
  inspector_events events;
  events.source = panel_->model().source();
  try {
    view_.set_source(*events.source);
  } catch (const fonts_needed& e) {
    host_.need_fonts(e.fonts());
    return;
  } catch (const std::exception& e) {
    status_ = std::string("cannot show: ") + e.what();
  }
  panel_->refresh();
}

void editor_workspace::show_source(bool on, std::optional<int> line) {
  if (panel_ != nullptr) {
    panel_->show_source(on, line);
    relayout();
  }
}

std::string editor_workspace::show_panel(bool on, bool repair) {
  if ((panel_ != nullptr) == on) {
    return {};
  }
  if (on) {
    try {
      const std::string source = repair_source_ ? *repair_source_ : std::string(view_.source());
      panel_ = std::make_unique<editor_panel>(source, file_name_, glsl_version_.c_str(), repair);
    } catch (const std::exception& e) {
      return e.what();
    }
    panel_->model().set_original(saved_source_);
    panel_->set_strip_width(strip_wanted_);
    panel_->grid().set_host_files(host_files_);
    if (host_.has_clipboard()) {
      panel_->set_clipboard([this](std::string_view text) { host_.set_clipboard(text); }, [this] { return host_.clipboard(); });
    }
    image_w_ = panel_->model().image().width;
    image_h_ = panel_->model().image().height;
    view_.set_edit_mode(true);
  } else {
    panel_.reset();
    view_.set_edit_mode(false);
    view_.set_selection(std::nullopt);
    pointer_in_panel_ = false;
    panel_drag_ = false;
    figure_drag_ = false;
    divider_drag_ = drag::none;
    set_cursor(cursor_shape::arrow);
  }
  last_ = nullptr;
  relayout();
  return {};
}

void editor_workspace::resize(float css_width, float css_height, float scale) {
  css_w_ = css_width;
  css_h_ = css_height;
  scale_ = scale > 0.0f ? scale : 1.0f;
  relayout();
}

void editor_workspace::relayout() {
  if (css_w_ <= 0.0f || css_h_ <= 0.0f) {
    return; // minimised, or not sized yet
  }
  if (panel_ != nullptr) {
    panel_->resize(css_w_, css_h_, scale_);
    const layout_box left = panel_->layout().left;
    view_.resize(view_size{std::max(left.width, 1.0f), std::max(left.height, 1.0f), scale_});
  } else {
    view_.resize(view_size{css_w_, css_h_, scale_});
  }
}

float editor_workspace::strip_width() const {
  return panel_ != nullptr ? panel_->strip_width() : 0.0f;
}

float editor_workspace::tab_height() const {
  return panel_ != nullptr ? editor_panel::kTabBarHeight : 0.0f;
}

// ── Input ────────────────────────────────────────────────────────────────────

void editor_workspace::pointer(const pointer_event& in) {
  pointer_event e = in;
  if (panel_ == nullptr) {
    view_.input(e);
    return;
  }
  if (divider(e.type, e.x, e.button)) {
    return;
  }
  // The panel (strip, tab bar, a data table) takes the pointer everywhere
  // but over the figure, and a drag that began in it. The figure starts
  // under the tab bar.
  const layout_box figure = panel_->figure_box();
  const bool over_panel = !figure.contains(e.x, e.y) || panel_->popup_open() || panel_->over_toolbar(e.x, e.y);
  e.y -= figure.y;
  if (e.type == pointer_event::kind::down) {
    panel_drag_ = over_panel;
    figure_drag_ = !over_panel;
  }
  // A drag that began in the figure (a pan) stays with it over the toolbar.
  const bool to_panel = panel_drag_ || (over_panel && !(figure_drag_ && panel_->over_toolbar(e.x, e.y + figure.y)));
  if (to_panel) {
    if (!pointer_in_panel_) { // the figure sees the pointer leave
      view_.input({pointer_event::kind::leave, 0.0f, 0.0f, 0});
    }
    pointer_in_panel_ = true;
    panel_->pointer(e.type, e.x, e.y + figure.y, e.button, e.wheel_x, e.wheel_y);
  } else {
    if (pointer_in_panel_) {
      panel_->pointer(pointer_event::kind::leave, 0.0f, 0.0f, 0, 0.0f, 0.0f);
    }
    pointer_in_panel_ = false;
    view_.input(e);
    figure_right_click(e.type, e.x, e.y, e.button, figure);
  }
  if (e.type == pointer_event::kind::up) {
    panel_drag_ = false;
    figure_drag_ = false;
  }
}

bool editor_workspace::divider(pointer_event::kind type, float x, int button) {
  if (divider_drag_ != drag::none) {
    if (type == pointer_event::kind::move) {
      if (divider_drag_ == drag::strip) {
        panel_->set_strip_width(css_w_ - x);
        strip_wanted_ = panel_->wanted_strip_width();
      } else {
        panel_->set_source_width(panel_->strip_x() - x);
      }
      relayout();
    } else if (type == pointer_event::kind::up) {
      divider_drag_ = drag::none;
    }
    return true;
  }
  const bool free = !panel_drag_ && !panel_->popup_open() && !panel_->modal_open();
  const bool over_strip = free && divider_hit(x, panel_->strip_x());
  const bool over_source = free && !over_strip && panel_->source_shown() && divider_hit(x, panel_->source_x());
  set_cursor(over_strip || over_source ? cursor_shape::resize_horizontal : cursor_shape::arrow);
  if (!(over_strip || over_source) || type != pointer_event::kind::down || button != 0) {
    return false;
  }
  const double now = workspace_now();
  if (now - divider_clicked_at_ < kDoubleClick) {
    // A double-click puts the divider back where it starts.
    if (over_strip) {
      panel_->set_strip_width(kDefaultStripWidth);
      strip_wanted_ = kDefaultStripWidth;
    } else {
      panel_->set_source_width(0.0f);
    }
    divider_clicked_at_ = -1.0;
    relayout();
  } else {
    divider_clicked_at_ = now;
    divider_drag_ = over_strip ? drag::strip : drag::source;
  }
  // The figure and the strip see the pointer leave.
  view_.input({pointer_event::kind::leave, 0.0f, 0.0f, 0});
  panel_->pointer(pointer_event::kind::leave, 0.0f, 0.0f, 0, 0.0f, 0.0f);
  pointer_in_panel_ = false;
  return true;
}

void editor_workspace::figure_right_click(pointer_event::kind type, float x, float y, int button, const layout_box& figure) {
  if (button != 1) {
    return;
  }
  if (type == pointer_event::kind::down) {
    right_down_ = std::make_pair(x, y);
    return;
  }
  if (type != pointer_event::kind::up || !right_down_) {
    return;
  }
  const auto [x0, y0] = *right_down_;
  right_down_.reset();
  if (std::abs(x - x0) > 4.0f || std::abs(y - y0) > 4.0f) {
    return; // a drag, not a click
  }
  const std::vector<edit_target> targets = view_.targets_at(x, y);
  if (targets.empty() || targets.front().element == "image") {
    return; // the image options have no menu
  }
  const std::string path = targets.front().element;
  panel_->model().select(edit_target_id{path, ""});
  view_.set_selection(panel_->model().selection());
  panel_->open_element_menu(path, x + figure.x, y + figure.y);
}

void editor_workspace::set_cursor(cursor_shape shape) {
  if (shape != cursor_) {
    cursor_ = shape;
    host_.set_cursor(shape);
  }
}

key_outcome editor_workspace::key(window_key key, key_mods mods, bool down, bool repeat) {
  const key_event figure_key{imgui_key(key), down, mods.ctrl, mods.shift, mods.alt, false};
  // Shortcuts that hold wherever the keyboard is (spec/15_web_app/DESIGN.md
  // section 5.4): the source pane, saving, and the document's undo while
  // the pane is in step with the model.
  if (panel_ != nullptr && mods.ctrl && !mods.alt) {
    if (key == window_key::u) {
      if (down && !repeat) {
        show_source(!panel_->source_shown());
      }
      return key_outcome::consumed;
    }
    if (key == window_key::s && !panel_->modal_open()) {
      if (down && !repeat) {
        flush_pending();
        host_.request_save();
      }
      return key_outcome::consumed;
    }
    const bool undo_key = key == window_key::z || key == window_key::y;
    if (undo_key && panel_->source_focused() && !panel_->source_ahead() && !panel_->modal_open()) {
      if (down) {
        if (key == window_key::y || mods.shift) {
          redo();
        } else {
          undo();
        }
      }
      return key_outcome::consumed;
    }
  }
  // A popup of the panel (a menu, a colour picker) takes the keys too:
  // Escape closes it, not the window.
  const bool panel_keys = panel_ != nullptr && (panel_->wants_keyboard() || panel_->modal_open() || panel_->popup_open());
  switch (route_key(panel_keys, view_.wants_keyboard())) {
    case key_route::panel:
      panel_->key(key, mods, down, repeat);
      return key_outcome::consumed;
    case key_route::figure:
      view_.input_key(figure_key);
      return key_outcome::consumed;
    case key_route::viewer:
      break;
  }
  if (!down) {
    view_.input_key(figure_key); // a key pressed in a field may be released after it
    return key_outcome::consumed;
  }
  if (panel_ == nullptr) {
    return key_outcome::unhandled;
  }
  if (repeat) {
    return key_outcome::consumed;
  }
  const inspector_events tree = panel_->shortcut(key, mods);
  if (tree.source || tree.select) {
    apply(tree);
    return key_outcome::consumed;
  }
  if (mods.ctrl) {
    if (key == window_key::z && !mods.shift) {
      undo();
    } else if (key == window_key::y || (key == window_key::z && mods.shift)) {
      redo();
    }
    return key_outcome::consumed;
  }
  return key_outcome::unhandled;
}

void editor_workspace::text(char32_t codepoint) {
  switch (route_key(panel_ != nullptr && panel_->wants_keyboard(), view_.wants_keyboard())) {
    case key_route::panel:
      panel_->text(codepoint);
      break;
    case key_route::figure:
      view_.input_text(std::u32string_view(&codepoint, 1));
      break;
    case key_route::viewer:
      break;
  }
}

bool editor_workspace::wants_keyboard() const {
  return view_.wants_keyboard() ||
      (panel_ != nullptr && (panel_->wants_keyboard() || panel_->modal_open() || panel_->popup_open()));
}

// ── Frames ───────────────────────────────────────────────────────────────────

bool editor_workspace::wants_frame() const {
  return view_.wants_frame() || (panel_ != nullptr && panel_->wants_frame());
}

void editor_workspace::frame(double now_seconds) {
  if (css_w_ <= 0.0f || css_h_ <= 0.0f) {
    return;
  }
  if (view_.wants_frame()) {
    if (const ImDrawData* data = view_.frame(now_seconds)) {
      last_ = data;
    }
    ask_size_if_too_small();
  }
  if (panel_ == nullptr) {
    return;
  }
  if (const std::optional<edit_target> clicked = view_.take_edit_click()) {
    panel_->model().select(edit_target_id{clicked->element, clicked->field});
    view_.set_selection(panel_->model().selection());
    panel_->refresh();
  }
  panel_->set_theme(view_.theme());
  panel_->set_anchor(selection_box());
  if (panel_->wants_frame()) {
    const inspector_events events = panel_->frame(now_seconds, view_.element_list(), host_.status_line(status_));
    apply(events);
    take_file_requests();
    const editor_panel::decision decision = panel_ != nullptr ? panel_->take_decision() : editor_panel::decision::none;
    if (decision != editor_panel::decision::none) {
      host_.decided(decision);
    }
  }
}

void editor_workspace::take_file_requests() {
  if (panel_ == nullptr) {
    return;
  }
  for (const table_grid::file_request& r : panel_->grid().take_requests()) {
    if (r.import_csv) {
      host_.request_csv_import(r.table);
      continue;
    }
    for (const data_table& t : panel_->model().tables()) {
      if (t.name == r.table) {
        host_.request_csv_export(r.table, (r.table.empty() ? std::string("data") : r.table) + ".csv", csv_text(t.cells));
      }
    }
  }
}

void editor_workspace::draw_figure(int pixel_width, int pixel_height) {
  if (glsl_version_.empty() || gl_.viewport == nullptr) {
    return; // headless
  }
  gl_.viewport(0, 0, pixel_width, pixel_height);
  const ImVec4 bg = ImGui::ColorConvertU32ToFloat4(view_.clear_color());
  gl_.clear_color(bg.x, bg.y, bg.z, 1.0f);
  gl_.clear(kColorBufferBit);
  // The figure's draw data fills the bottom-left of the drawable (OpenGL
  // counts from the bottom), which is the left side under the tab bar. A
  // data tab hides it.
  if (last_ != nullptr && (panel_ == nullptr || (!panel_->figure_box().empty() && !panel_->model().repairing()))) {
    view_.with_contexts([&] { ImGui_ImplOpenGL3_RenderDrawData(const_cast<ImDrawData*>(last_)); });
  }
}

void editor_workspace::render(int pixel_width, int pixel_height) {
  pixel_w_ = pixel_width;
  pixel_h_ = pixel_height;
  if (pixel_width <= 0 || pixel_height <= 0 || !has_frame()) {
    return;
  }
  draw_figure(pixel_width, pixel_height);
  if (panel_ != nullptr) {
    panel_->render();
  }
}

// ── Changes ──────────────────────────────────────────────────────────────────

inspector_events editor_workspace::events_of(const std::optional<std::string>& source) {
  inspector_events events;
  events.source = source;
  return events;
}

void editor_workspace::apply(const inspector_events& events) {
  if (panel_ == nullptr) {
    return;
  }
  if (events.source) {
    try {
      if (events.paths.empty()) {
        view_.set_source(*events.source);
      } else {
        view_.set_source_moved(*events.source, [&](std::string_view p) { return events.paths.apply(p); });
        last_ = nullptr; // the contexts were recreated
      }
    } catch (const fonts_needed& e) {
      host_.need_fonts(e.fonts());
    } catch (const std::exception& e) {
      status_ = std::string("cannot show: ") + e.what();
    }
    fit_to_image();
  }
  if (events.select) {
    panel_->model().select(*events.select);
  }
  if (events.reset_view) {
    reset_view();
  }
  // The model decides what is selected (an undo may remove the element).
  view_.set_selection(panel_->model().selection());
  if (events.source) {
    commit_result r;
    r.ok = true;
    r.source = *events.source;
    r.paths = events.paths;
    r.label = panel_->model().last_label();
    r.origin = panel_->model().last_origin();
    host_.changed(r);
  }
  if (events.save) {
    host_.request_save();
  }
  // Frames only when something happened: every panel frame ends here.
  const bool happened = events.source || events.select || events.reset_view || events.save || events.undo ||
      !events.error.empty() || events.show_data;
  if (panel_ != nullptr && happened) {
    panel_->refresh();
  }
}

void editor_workspace::apply_change(const commit_result& r, const std::string& status) {
  inspector_events events;
  events.source = r.source;
  events.paths = r.paths;
  status_ = status;
  if (panel_ != nullptr) {
    panel_->panel().set_error("");
  }
  apply(events);
}

void editor_workspace::flush_pending() {
  if (panel_ != nullptr && panel_->panel().pending()) {
    apply(panel_->panel().flush(panel_->model()));
  }
}

std::string editor_workspace::import_csv(const std::string& table, std::string_view csv) {
  if (panel_ == nullptr) {
    return "the editor panel is hidden";
  }
  std::string message;
  try {
    const commit_result r = panel_->model().set_table(table, table_from_csv(csv));
    if (r.ok) {
      panel_->panel().set_error("");
      apply(events_of(r.source));
      return {};
    }
    message = r.error;
  } catch (const std::exception& e) {
    message = std::string("the file is not a CSV table: ") + e.what();
  }
  panel_->panel().set_error(message);
  panel_->refresh();
  return message;
}

void editor_workspace::undo() {
  if (panel_ != nullptr) {
    flush_pending();
    apply(events_of(panel_->model().undo()));
  }
}

void editor_workspace::redo() {
  if (panel_ != nullptr) {
    flush_pending();
    apply(events_of(panel_->model().redo()));
  }
}

void editor_workspace::reset_view() {
  view_.reset_view();
  last_ = nullptr; // its draw data went with the old contexts
}

void editor_workspace::mark_saved() {
  if (panel_ != nullptr) {
    panel_->model().mark_saved();
  }
  saved_source_ = std::string(view_.source());
}

bool editor_workspace::modified() const {
  return panel_ != nullptr ? panel_->model().modified() : std::string(view_.source()) != saved_source_;
}

std::optional<layout_box> editor_workspace::selection_box() const {
  const std::optional<edit_target_id>& selection = panel_->model().selection();
  if (!selection || selection->element == kImagePath) {
    return std::nullopt;
  }
  std::optional<edit_target> t = view_.target_for(selection->element, "");
  if (!t) {
    t = view_.target_for(selection->element, selection->field);
  }
  if (!t) {
    t = view_.target_for(selection->element, "label"); // a series: its legend entry
  }
  if (!t) {
    return std::nullopt;
  }
  const layout_box figure = panel_->figure_box();
  return layout_box{figure.x + t->x, figure.y + t->y, t->width, t->height};
}

void editor_workspace::fit_to_image() {
  const image_options& image = panel_->model().image();
  if (image.width == image_w_ && image.height == image_h_) {
    return;
  }
  image_w_ = image.width;
  image_h_ = image.height;
  host_.request_size(static_cast<float>(image.width) + strip_width(), static_cast<float>(image.height) + tab_height());
}

void editor_workspace::ask_size_if_too_small() {
  const view_geometry g = view_.geometry();
  if (!g.fit_fallback) {
    asked_w_ = asked_h_ = 0.0f;
    return;
  }
  const float want_w = g.layout_width + strip_width();
  const float want_h = g.layout_height + tab_height();
  if (want_w == asked_w_ && want_h == asked_h_) {
    return;
  }
  asked_w_ = want_w;
  asked_h_ = want_h;
  host_.request_size(want_w, want_h);
}

std::vector<uint8_t> editor_workspace::capture_figure(int max_width, int& width, int& height, double now_seconds) {
  const bool shown = panel_ == nullptr || !panel_->figure_box().empty();
  if (!shown || gl_.read_pixels == nullptr || pixel_w_ <= 0 || pixel_h_ <= 0) {
    const std::vector<uint8_t> png = render_png(view_.source_with_view());
    width = png.size() > 24 ? (png[16] << 24 | png[17] << 16 | png[18] << 8 | png[19]) : 0;
    height = png.size() > 24 ? (png[20] << 24 | png[21] << 16 | png[22] << 8 | png[23]) : 0;
    return png;
  }
  host_.make_current();
  if (view_.wants_frame()) {
    if (const ImDrawData* data = view_.frame(now_seconds)) {
      last_ = data;
    }
  }
  if (last_ == nullptr) {
    return {};
  }
  // The figure alone, in the bottom-left of the back buffer (as render()
  // draws it), read before anything else is drawn over it.
  const layout_box box = panel_ != nullptr ? panel_->figure_box() : layout_box{0, 0, css_w_, css_h_};
  const int w = std::max(1, static_cast<int>(std::lround(box.width * scale_)));
  const int h = std::max(1, static_cast<int>(std::lround(box.height * scale_)));
  draw_figure(pixel_w_, pixel_h_);
  image img;
  img.width = w;
  img.height = h;
  img.rgba.resize(static_cast<size_t>(w) * h * 4);
  if (gl_.pixel_store != nullptr) {
    gl_.pixel_store(kPackAlignment, 1);
  }
  std::vector<uint8_t> flipped(img.rgba.size());
  gl_.read_pixels(0, 0, w, h, kRgba, kUnsignedByte, flipped.data());
  for (int y = 0; y < h; ++y) { // OpenGL's rows go up
    std::memcpy(img.at(0, y), flipped.data() + static_cast<size_t>(h - 1 - y) * w * 4, static_cast<size_t>(w) * 4);
  }
  for (size_t i = 3; i < img.rgba.size(); i += 4) {
    img.rgba[i] = 255;
  }
  render(pixel_w_, pixel_h_); // the back buffer as it was
  // Smaller when asked: whole-pixel steps, each the average of its block.
  const int step = max_width > 0 && w > max_width ? (w + max_width - 1) / max_width : 1;
  if (step > 1) {
    image small;
    small.width = w / step;
    small.height = h / step;
    small.rgba.resize(static_cast<size_t>(small.width) * small.height * 4);
    for (int y = 0; y < small.height; ++y) {
      for (int x = 0; x < small.width; ++x) {
        for (int c = 0; c < 4; ++c) {
          int sum = 0;
          for (int dy = 0; dy < step; ++dy) {
            for (int dx = 0; dx < step; ++dx) {
              sum += img.at(x * step + dx, y * step + dy)[c];
            }
          }
          small.at(x, y)[c] = static_cast<uint8_t>(sum / (step * step));
        }
      }
    }
    img = std::move(small);
  }
  width = img.width;
  height = img.height;
  return encode_png(img, "", "");
}

} // namespace bdg::nymph
