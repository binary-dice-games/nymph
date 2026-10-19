// MIT License © 2026 Binary Dice Games
/// @file draw_context.hpp
/// @brief What a drawing function may use while a figure frame is open.
#pragma once

#include <imgui.h>
#include <implot.h>
#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "document/view.hpp"
#include "render/image_options.hpp"
#include "render/ui_state.hpp"
#include "schema/node.hpp"

namespace bdg::nymph {

/// @brief An axis-aligned rectangle in logical pixels (ImGui screen space).
struct rect {
  ImVec2 min;
  ImVec2 max;
  float width() const {
    return max.x - min.x;
  }
  float height() const {
    return max.y - min.y;
  }
};

/// @brief What a live view reads back about one 2D plot after a frame: its
///        plot area and axis ranges (spec/04_web_viewer/DESIGN.md
///        section 4, `view_state()`).
struct plot_view_record {
  const node* element = nullptr;
  view_kind kind = view_kind::plot2d;
  rect area; ///< The plot area (inside the axes), logical pixels.
  double x_min = 0, x_max = 0;
  double y_min = 0, y_max = 0;
  bool has_y2 = false;
  double y2_min = 0, y2_max = 0;
  // 3D plots:
  double z_min = 0, z_max = 0;
  std::array<double, 4> rotation{0, 0, 0, 1};
  // Diagrams:
  double zoom = 1.0;
  std::array<double, 2> center{0, 0};
  std::vector<std::string> hidden; ///< Legend entries switched off.
};

/// @brief What is under the pointer in a live view: a data point, bar,
///        cell, task, flow or diagram node (spec/04_web_viewer/DESIGN.md
///        section 3, "Interaction per element family").
struct pick {
  const node* element = nullptr; ///< The series or element hit.
  std::string type; ///< Its type name, e.g. "PlotLine".
  std::string label; ///< Series label (without any "##" suffix) or id.
  int index = -1; ///< Point, bar, cell or node index; -1 for none.
  /// Values as the axes show them, in order: {"x", "3"}, {"y", "15.2"}.
  std::vector<std::pair<std::string, std::string>> values;
};

/// @brief One clickable thing a frame drew in edit mode: an element, or one
///        text field of an element (spec/08_view_editor/DESIGN.md section 4).
struct edit_record {
  const node* element = nullptr;
  std::string field; ///< The text field shown there; empty for the element as a whole.
  std::string text; ///< The text shown, for display.
  std::string read_only_reason; ///< Non-empty when the field cannot be edited.
  rect area; ///< Logical pixels.
};

/// @brief The ImUml nodes, edges and events one element submitted while it
///        was drawn, as index ranges `[first, last)` in submission order
///        (edit mode only; spec/08_view_editor/DESIGN.md section 4).
struct item_range {
  const node* element = nullptr;
  int first[3] = {0, 0, 0}; ///< Nodes, edges, events.
  int last[3] = {0, 0, 0};
};

/// @brief A pick offered while a plot is drawn; the plot keeps the closest.
struct pick_candidate {
  float distance = 0.0f; ///< From the pointer, logical pixels; 0 inside a shape.
  pick hit;
  ImVec2 anchor; ///< Where to draw the highlight ring, logical pixels.
  ImU32 color = 0; ///< The series colour, for the ring or outline.
  bool outline = false; ///< Highlight with a rectangle instead of a ring.
  ImVec2 outline_min, outline_max; ///< The rectangle, logical pixels.
};

/// @brief Largest distance at which a point is picked, logical pixels.
inline constexpr float kPickRadius = 8.0f;

/// @brief Handed to the drawing function once per frame, inside the
///        figure's full-display ImGui window, with the private ImGui, ImPlot
///        and ImPlot3D contexts current.
///
/// Each `figure` element draws in its rectangle (set_rect() / rect()): the
/// root gets content_rect(); the layout engine (plan step 11) sets the
/// rectangles of layout children.
class draw_context {
 public:
  explicit draw_context(const image_options& options) : options_(options) {}

  /// @brief The options the frame is rendered with.
  const image_options& options() const {
    return options_;
  }

  /// @brief Whether the frame belongs to a live view that takes pointer
  ///        input (spec/04_web_viewer/DESIGN.md section 4). False for
  ///        PNG rendering, where nothing interactive may change the image.
  bool interactive() const {
    return interactive_;
  }

  /// @brief Set by the live view; never by the PNG frame driver.
  void set_interactive(bool interactive) {
    interactive_ = interactive;
  }

  /// @brief How elements apply axis limits written in the source:
  ///        `ImPlotCond_Always` (fixed) for PNG rendering and for a live
  ///        view's first frames, `ImPlotCond_Once` afterwards so the user
  ///        can pan and zoom away from them. ImPlot treats an `Always` axis
  ///        as locked, which changes how it settles (an `Equal` plot then
  ///        adjusts its other axis), so the first frames must match the
  ///        PNG path exactly.
  ImPlotCond limits_cond() const {
    return (!interactive_ || lock_limits_) ? ImPlotCond_Always : ImPlotCond_Once;
  }

  /// @brief Set by the live view for the frames that settle fresh contexts.
  void set_lock_limits(bool lock) {
    lock_limits_ = lock;
  }

  // ── Saved view (the `view` key) ────────────────────────────────────────

  /// @brief The saved views of the figure and the path of every node
  ///        (both outlive the frames), or nulls for none.
  void set_views(const view_map* views, const std::unordered_map<const node*, std::string>* paths) {
    views_map_ = views;
    paths_ = paths;
  }

  /// @brief `n`'s saved view, or nullptr.
  const element_view* view_of(const node& n) const {
    if (views_map_ == nullptr || paths_ == nullptr) {
      return nullptr;
    }
    const auto p = paths_->find(&n);
    if (p == paths_->end()) {
      return nullptr;
    }
    const auto v = views_map_->find(p->second);
    return v == views_map_->end() ? nullptr : &v->second;
  }

  /// @brief Whether elements apply their saved view (and source limits)
  ///        this frame: always for PNGs, in a live view while fresh
  ///        contexts settle.
  bool apply_view() const {
    return limits_cond() == ImPlotCond_Always;
  }

  /// @brief The plot whose items are being drawn (set by Plot and Plot3D
  ///        around their children), or nullptr.
  const node* current_plot() const {
    return current_plot_;
  }
  void set_current_plot(const node* n) {
    current_plot_ = n;
    pending_hidden_.clear();
  }

  /// @brief Notes that legend entry `label` of the current plot is hidden.
  void record_hidden(const std::string& label) {
    if (interactive_) {
      pending_hidden_.push_back(label);
    }
  }

  /// @brief Records a view of a kind other than a 2D plot (3D plot,
  ///        diagram); the current plot's hidden entries are attached.
  void record_view(plot_view_record r) {
    if (interactive_) {
      r.hidden = pending_hidden_;
      views_.push_back(std::move(r));
    }
  }

  /// @brief Called by the live view before each frame: forgets every
  ///        rectangle (the figure, and its node addresses, may have
  ///        changed) and keeps the last frame's plot views as previous.
  void begin_frame() {
    rects_.clear();
    input_areas_.clear();
    previous_views_ = std::move(views_);
    views_.clear();
    candidate_.reset();
    hovered_.reset();
    targets_.clear();
  }

  // ── Picks (live views only) ───────────────────────────────────────────

  /// @brief Offers a pick for the plot being drawn; kept if within
  ///        kPickRadius and closer than any offered before.
  void offer_pick(pick_candidate c) {
    if (c.distance <= kPickRadius && (!candidate_ || c.distance < candidate_->distance)) {
      candidate_ = std::move(c);
    }
  }

  /// @brief The best pick offered since the last call, and forgets it.
  std::optional<pick_candidate> take_pick_candidate() {
    std::optional<pick_candidate> out = std::move(candidate_);
    candidate_.reset();
    return out;
  }

  /// @brief Sets what is under the pointer in this frame.
  void set_hovered(pick p) {
    hovered_ = std::move(p);
  }

  /// @brief What is under the pointer in this frame, if anything.
  const std::optional<pick>& hovered() const {
    return hovered_;
  }

  /// @brief Whether nymph draws its own tooltip for the hovered pick.
  bool tooltips() const {
    return tooltips_;
  }
  void set_tooltips(bool on) {
    tooltips_ = on;
  }

  // ── Edit targets (live views in edit mode only) ───────────────────────

  /// @brief Whether elements record what they draw for the editor
  ///        (spec/08_view_editor/DESIGN.md section 4). False for PNG
  ///        rendering and plain live views, where record_target() does
  ///        nothing.
  bool edit_mode() const {
    return edit_mode_;
  }
  void set_edit_mode(bool on) {
    edit_mode_ = on;
  }

  /// @brief Notes that `area` (logical pixels) shows `n`'s text field
  ///        `field`, or `n` as a whole when `field` is empty; the editor
  ///        can click it. Later records are drawn on top of earlier ones.
  /// @param text The text shown there, for the editor's label.
  /// @param read_only_reason Why the field cannot be edited ("" if it can).
  void record_target(
      const node& n,
      std::string_view field,
      const rect& area,
      std::string text = {},
      std::string read_only_reason = {}) {
    if (edit_mode_) {
      targets_.push_back({&n, std::string(field), std::move(text), std::move(read_only_reason), area});
    }
  }

  /// @brief The targets recorded in this frame, in drawing order.
  const std::vector<edit_record>& targets() const {
    return targets_;
  }

  /// @brief The ImUml items each element submitted since the last
  ///        clear_item_ranges(), in the order the elements finished.
  const std::vector<item_range>& item_ranges() const {
    return item_ranges_;
  }
  void clear_item_ranges() {
    item_ranges_.clear();
  }

  /// @brief In a live view, records the current ImPlot plot's area and
  ///        limits for `n`; call between BeginPlot and EndPlot, after its
  ///        items. Does nothing when not interactive.
  void record_current_plot(const node& n, bool with_y2, view_kind kind = view_kind::plot2d);

  /// @brief In a live view, records `r` as an area that takes pointer input
  ///        (a plot or diagram that pans and zooms), so the host lets the
  ///        wheel scroll the page everywhere else.
  void add_input_area(const rect& r) {
    if (interactive_) {
      input_areas_.push_back(r);
    }
  }

  /// @brief The input areas recorded in this frame.
  const std::vector<rect>& input_areas() const {
    return input_areas_;
  }

  /// @brief The plot views recorded in this frame.
  const std::vector<plot_view_record>& plot_views() const {
    return views_;
  }

  /// @brief `n`'s plot view recorded in the previous frame, or nullptr.
  const plot_view_record* previous_plot_view(const node& n) const {
    for (const plot_view_record& v : previous_views_) {
      if (v.element == &n) {
        return &v;
      }
    }
    return nullptr;
  }

  // ── User interfaces (spec/09_ui_mockups/DESIGN.md section 5) ───────────

  /// @brief The state of UI elements, by element path. Persists as long as
  ///        the context: three frames for a PNG, the contexts' lifetime in a
  ///        live view.
  ui_state& ui() {
    return ui_;
  }
  const ui_state& ui() const {
    return ui_;
  }

  /// @brief `n`'s element path ("root/children/1"), or a name unique to the
  ///        node when no paths were set (frames drawn without a figure).
  std::string path_of(const node& n) const {
    if (paths_ != nullptr) {
      const auto it = paths_->find(&n);
      if (it != paths_->end()) {
        return it->second;
      }
    }
    return "#" + std::to_string(reinterpret_cast<std::uintptr_t>(&n));
  }

  /// @brief Output pixels per logical pixel (`image.scale`).
  float scale() const {
    return static_cast<float>(options_.scale);
  }

  /// @brief Draws each child of `n`, in order, through draw_node(), each
  ///        under its own ImGui ID (its index), so two series or plots with
  ///        the same label do not collide.
  void draw_children(const node& n);

  /// @brief Draws `n` through its class, with its text style applied
  ///        (render/text_style.hpp), so its children inherit it.
  void draw_node(const node& n);

  /// @brief Its class's measure(), with `n`'s text style applied.
  ImVec2 measure_node(const node& n, float width);

  /// @brief Its class's arrange(), with `n`'s text style applied.
  void arrange_node(const node& n, const rect& r);

  /// @brief Registers the loaded font called `name` (an embedded font file
  ///        name, or "default").
  void add_font(std::string name, ImFont* font) {
    fonts_[std::move(name)] = font;
  }

  /// @brief The font called `name`, or nullptr (keep the current font) if
  ///        none was registered.
  ImFont* font(std::string_view name) const {
    const auto it = fonts_.find(name);
    return it == fonts_.end() ? nullptr : it->second;
  }

  /// @brief The name `font` was registered under, or "" if it was not.
  std::string_view font_name(const ImFont* font) const {
    for (const auto& [name, f] : fonts_) {
      if (f == font) {
        return name;
      }
    }
    return {};
  }

  /// @brief Sets the rectangle `n` draws in.
  void set_rect(const node& n, const rect& r) {
    rects_[&n] = r;
  }

  /// @brief Every rectangle set so far.
  const std::unordered_map<const node*, rect>& rects() const {
    return rects_;
  }

  /// @brief The rectangle `n` draws in; content_rect() if none was set.
  rect rect_of(const node& n) const {
    const auto it = rects_.find(&n);
    return it == rects_.end() ? content_rect() : it->second;
  }

  /// @brief The image minus `image.padding` on every side: where the root
  ///        element goes.
  rect content_rect() const {
    const float pad = static_cast<float>(options_.padding);
    return {
        ImVec2(pad, pad), ImVec2(static_cast<float>(options_.width) - pad, static_cast<float>(options_.height) - pad)};
  }

 private:
  const image_options& options_;
  bool interactive_ = false;
  bool lock_limits_ = false;
  const view_map* views_map_ = nullptr;
  const std::unordered_map<const node*, std::string>* paths_ = nullptr;
  const node* current_plot_ = nullptr;
  std::vector<std::string> pending_hidden_;
  bool tooltips_ = true;
  bool edit_mode_ = false;
  std::vector<edit_record> targets_;
  std::vector<item_range> item_ranges_;
  std::optional<pick_candidate> candidate_;
  std::optional<pick> hovered_;
  std::vector<plot_view_record> views_;
  std::vector<rect> input_areas_;
  std::vector<plot_view_record> previous_views_;
  std::unordered_map<const node*, rect> rects_;
  std::map<std::string, ImFont*, std::less<>> fonts_;
  ui_state ui_;
};

} // namespace bdg::nymph
