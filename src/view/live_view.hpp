// MIT License © 2026 Binary Dice Games
/// @file live_view.hpp
/// @brief A figure drawn frame after frame, with persistent ImGui, ImPlot,
///        ImPlot3D and ImUml state: the platform-neutral heart of the
///        interactive viewer (spec/04_web_viewer/DESIGN.md section 4).
#pragma once

#include <imgui.h>
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "render/texture_store.hpp"

namespace bdg::nymph {

/// @brief The box a view is shown in: CSS pixels and the device pixel
///        ratio. A width or height of 0 means "no constraint".
struct view_size {
  float width = 0.0f;
  float height = 0.0f;
  float pixel_ratio = 1.0f;
};

/// @brief How the source's `image.width` / `height` relate to the box
///        (spec/04_web_viewer/DESIGN.md section 3, "Sizing").
enum class view_fit {
  source, ///< Lay out at the source size; shrink with CSS when the box is narrower.
  width, ///< Lay out at the box width; height scaled to keep the aspect ratio.
  fill, ///< Lay out at the box size.
  scale, ///< Lay out at the source size; scale with CSS to the box width, up or down.
};

/// @brief Fixed choices of a live view.
struct view_options {
  view_fit fit = view_fit::source;
  /// @brief Answer ImGui's texture requests with a CPU texture_store (for
  ///        the software rasterizer). A GPU presenter that answers them
  ///        itself sets this to false.
  bool cpu_textures = true;
  /// @brief Draw nymph's tooltip for the hovered pick. Off, picks are
  ///        still reported (hovered()) for the host to show its own.
  bool tooltips = true;
};

/// @brief Where and how big the last frame is.
struct view_geometry {
  float layout_width = 0.0f; ///< Logical pixels the layout runs at (ImGui's DisplaySize).
  float layout_height = 0.0f;
  float css_scale = 1.0f; ///< CSS pixels per logical pixel (below 1 when shrunk; above 1 only with view_fit::scale).
  float framebuffer_scale = 1.0f; ///< Device pixels per logical pixel.
  int pixel_width = 0; ///< Size of the drawn image in device pixels.
  int pixel_height = 0;
  bool fit_fallback = false; ///< The fit could not be honoured; laid out at the source size.
};

/// @brief One pointer event, in CSS pixels from the view's top-left corner.
struct pointer_event {
  enum class kind { move, down, up, wheel, leave };
  kind type = kind::move;
  float x = 0.0f;
  float y = 0.0f;
  int button = 0; ///< down / up: 0 primary, 1 secondary, 2 middle.
  float wheel_x = 0.0f; ///< wheel: notches; positive y zooms in (away from the user).
  float wheel_y = 0.0f;
  bool touch = false; ///< From a touch screen (ImGui tunes hover and drag for it).
};

/// @brief One key going down or up (input_key()).
struct key_event {
  ImGuiKey key = ImGuiKey_None; ///< ImGui's key, so every host maps keys once.
  bool down = true;
  bool ctrl = false;
  bool shift = false;
  bool alt = false;
  bool super = false;
};

/// @brief What is under the pointer: a data point, bar, cell, task, flow
///        or diagram node (spec/04_web_viewer/DESIGN.md section 3).
struct view_pick {
  std::string element; ///< Element path of the series or element hit.
  std::string type; ///< Its type name, e.g. "PlotLine".
  std::string label; ///< Series label or diagram id.
  int index = -1; ///< Point, bar, cell or node index; -1 for none.
  /// Values as the axes show them, in order: {"x", "3"}, {"y", "15.2"}.
  std::vector<std::pair<std::string, std::string>> values;

  bool operator==(const view_pick&) const = default;
};

/// @brief A plot, Gantt chart, 3D plot or diagram as the last frame left
///        it: where it is and what it shows.
struct plot_view {
  std::string element; ///< Element path, e.g. "root/children/1".
  std::string kind; ///< "plot", "gantt", "plot3d" or "diagram".
  float x = 0.0f, y = 0.0f; ///< Plot area (inside the axes; the element for 3D plots and diagrams), CSS pixels.
  float width = 0.0f, height = 0.0f;
  double x_min = 0, x_max = 0; ///< Plots, Gantt charts, 3D plots.
  double y_min = 0, y_max = 0;
  bool has_y2 = false;
  double y2_min = 0, y2_max = 0;
  double z_min = 0, z_max = 0; ///< 3D plots.
  std::array<double, 4> rotation{0, 0, 0, 1}; ///< 3D plots: quaternion x, y, z, w.
  double zoom = 1.0; ///< Diagrams: on top of the fit.
  std::array<double, 2> center{0, 0}; ///< Diagrams: content point at the centre.
  std::vector<std::string> hidden; ///< Legend entries switched off.
};

/// @brief One thing the editor can select, as the last frame drew it
///        (spec/08_view_editor/DESIGN.md section 4): an element, or one
///        text field of an element.
struct edit_target {
  std::string element; ///< Element path, e.g. "root/children/1".
  std::string type; ///< Its type name, e.g. "Plot".
  std::string field; ///< The text field shown there ("title"); empty for the element as a whole.
  std::string text; ///< The text shown there.
  std::string read_only_reason; ///< Non-empty when the field cannot be edited (e.g. "bound to $names").
  float x = 0.0f, y = 0.0f; ///< Where it is, CSS pixels from the view's top-left corner.
  float width = 0.0f, height = 0.0f;

  bool operator==(const edit_target&) const = default;
};

/// @brief What the editor has selected: an element, and optionally one of
///        its text fields.
struct edit_target_id {
  std::string element;
  std::string field;

  bool operator==(const edit_target_id&) const = default;
};

/// @brief One element of the figure for the editor's tree.
struct edit_element {
  std::string path; ///< "root/children/1".
  std::string type; ///< "Plot".
  std::string label; ///< Its title, text, label or id, whichever it has; may be empty.
  int depth = 0; ///< 0 for the root.
};

/// @brief Thrown by an update whose figure names fonts without bytes. The
///        previous figure stays; register the fonts (register_font()) and
///        repeat the update.
class fonts_needed : public std::runtime_error {
 public:
  explicit fonts_needed(std::vector<std::string> fonts);
  /// @brief The missing font file names, sorted.
  const std::vector<std::string>& fonts() const {
    return fonts_;
  }

 private:
  std::vector<std::string> fonts_;
};

/// @brief A figure kept on screen: updates, resizes and frames on demand.
///
/// Owns private ImGui, ImPlot, ImPlot3D and ImUml contexts for its
/// lifetime and makes them current only inside its own calls, restoring
/// the previous ones afterwards; several live views may exist on one
/// thread. Not thread-safe: use each from one thread at a time, and never
/// two at once (ImGui's current context is process-global).
class live_view {
 public:
  explicit live_view(view_options options = {});
  ~live_view();
  live_view(live_view&&) noexcept;
  live_view& operator=(live_view&&) noexcept;
  live_view(const live_view&) = delete;
  live_view& operator=(const live_view&) = delete;

  /// @brief Replaces the figure with `source`.
  /// @param reset_view Drop the view state (zoom, hidden series) of the
  ///        previous figure; otherwise elements with an unchanged ID path
  ///        keep it.
  /// @throws nymph::error for a problem in the source, including a layout
  ///         that does not fit its own `image` size; fonts_needed. Either
  ///         way the previous figure stays.
  void set_source(std::string_view source, bool reset_view = false);

  /// @brief Replaces the figure's data with `data_yaml` (the value of a
  ///        top-level `data` key, as YAML or JSON) and keeps the rest of
  ///        the source and the view state.
  /// @throws nymph::error as set_source(); a problem inside `data_yaml` is
  ///         positioned in `data_yaml` and its message starts "data: ".
  ///         fonts_needed. Either way the previous figure stays.
  void set_data(std::string_view data_yaml);

  /// @brief Shows the source a nymph PNG carries, as set_source().
  /// @return false, and no figure, for a PNG that carries no nymph source
  ///         (the host shows it as a plain image).
  /// @throws std::runtime_error for a corrupt PNG (the previous figure
  ///         stays); as set_source().
  bool set_png(std::span<const uint8_t> png, bool reset_view = true);

  /// @brief Overrides `image.theme`: "light", "dark", "classic", or "" for
  ///        the source's own.
  /// @throws std::invalid_argument for an unknown theme.
  void set_theme(std::string theme);

  /// @brief The theme the figure is drawn with: the override of
  ///        set_theme(), else the source's `image.theme`; "light" without a
  ///        figure.
  std::string theme() const;

  /// @brief Sets the box the view is shown in.
  void resize(view_size size);

  /// @brief Feeds one pointer event to the figure (pan, zoom, hover); the
  ///        next frames apply it.
  void input(const pointer_event& event);

  /// @brief Feeds one key to the figure (user-interface widgets: a text
  ///        field, Ctrl+click on a slider; spec/09_ui_mockups/DESIGN.md
  ///        section 6). Send keys while wants_keyboard(), and every key-up.
  void input_key(const key_event& event);

  /// @brief Feeds typed characters to the figure's active text field.
  void input_text(std::u32string_view text);

  /// @brief Whether a text field of the figure has the keyboard (after the
  ///        last frame): keys and text then belong to the figure, not to the
  ///        host's shortcuts.
  bool wants_keyboard() const;

  /// @brief A number that changes whenever the state the user changed of a
  ///        user-interface element changes (what source_with_view() would
  ///        save of it), so a host can report view changes for widgets as it
  ///        does for plots.
  uint64_t ui_revision() const;

  /// @brief Lets the figure's text fields use the system clipboard: `set`
  ///        receives copied text, `get` returns text to paste. Without hooks
  ///        copy and paste work within the view only.
  void set_clipboard_hooks(std::function<void(std::string_view)> set, std::function<std::string()> get);

  /// @brief Drops every view state (zoom, pan, hidden series): the figure
  ///        is drawn as its source says, as after the first update.
  void reset_view();

  /// @brief Every 2D plot and Gantt chart as the last frame left it, in
  ///        drawing order.
  std::vector<plot_view> view_state() const;

  /// @brief Whether the wheel at (`x`, `y`), CSS pixels, would pan or zoom
  ///        something in the last frame (a plot or diagram that takes
  ///        input); elsewhere the host should let the page scroll.
  bool takes_wheel(float x, float y) const;

  /// @brief What the pointer was over in the last frame.
  const std::optional<view_pick>& hovered() const;

  /// @brief The pick under a primary-button click (press and release
  ///        without dragging), once: later calls return nothing until the
  ///        next click.
  std::optional<view_pick> take_click();

  // ── Edit mode (spec/08_view_editor/DESIGN.md sections 4 and 6) ────────

  /// @brief Turns edit mode on or off. On, elements record what they draw,
  ///        the target under the pointer and the selection are outlined, and
  ///        a primary click without a drag is reported by take_edit_click().
  ///        Off (the default), nothing differs from a plain view: draw data
  ///        and picks are the same.
  void set_edit_mode(bool on);
  bool edit_mode() const;

  /// @brief The targets under (`x`, `y`), CSS pixels, best first: the
  ///        smallest, and of equal size the one drawn last. Text targets
  ///        are padded by 3 px. An element recorded in pieces (a diagram
  ///        link's path) is listed once, as the union of its pieces. Empty
  ///        outside edit mode.
  std::vector<edit_target> targets_at(float x, float y) const;

  /// @brief The target of `element` (path) and `field` in the last frame;
  ///        with an empty `field`, the element's own. nullopt when it was
  ///        not drawn (a series, an element out of view) or not in edit mode.
  std::optional<edit_target> target_for(std::string_view element, std::string_view field) const;

  /// @brief Outlines the selection (or none). A selection whose element is
  ///        gone after set_source() is dropped.
  void set_selection(std::optional<edit_target_id> selection);
  const std::optional<edit_target_id>& selection() const;

  /// @brief The image options ("image", type "Image"), then every element of
  ///        the figure in drawing order with its depth, for a tree list. A
  ///        click away from every element targets the image options too.
  std::vector<edit_element> element_list() const;

  /// @brief The best target under a primary-button click (press and
  ///        release without dragging), once: later calls return nothing
  ///        until the next click. Only in edit mode.
  std::optional<edit_target> take_edit_click();

  /// @brief Runs one frame if one is due (wants_frame()).
  /// @param now_seconds A monotonic clock; the step since the last frame is
  ///        ImGui's DeltaTime (1/60 s for the first frame).
  /// @return The frame's draw data, valid until the next call on this view;
  ///         nullptr when no frame was due or there is no figure.
  const ImDrawData* frame(double now_seconds);

  /// @brief Whether frame() would draw: after an update, a resize or a
  ///        theme change, until the frames that settle it have run.
  bool wants_frame() const;

  /// @brief The theme's window background, opaque: what the image is
  ///        cleared to.
  ImU32 clear_color() const;

  /// @brief Size and scale of the last frame (or of the next, before any).
  view_geometry geometry() const;

  /// @brief Runs `f` with the view's ImGui, ImPlot, ImPlot3D and ImUml
  ///        contexts current, e.g. a GPU presenter's RenderDrawData() for
  ///        the draw data frame() returned.
  void with_contexts(const std::function<void()>& f);

  /// @brief Calls `on_create` (now, and whenever the view recreates its
  ///        contexts: reset_view(), an overflow fallback) and `on_destroy`
  ///        (before contexts are destroyed, the view's destruction
  ///        included), each with those contexts current: where a GPU
  ///        presenter initialises and shuts down its ImGui backend.
  void set_context_hooks(std::function<void()> on_create, std::function<void()> on_destroy);

  /// @brief Whether a figure was set.
  bool has_figure() const;
  /// @brief The figure's `description`.
  const std::string& description() const;
  /// @brief The source of the figure on screen.
  std::string_view source() const;

  /// @brief source() with the view on screen saved in its `view` key
  ///        (spec/04_web_viewer/DESIGN.md section 3, "Saved view"): the
  ///        elements the user zoomed, panned, rotated or hid series of,
  ///        plus the source's own entries for the others. The source as it
  ///        is when nothing changed.
  /// @throws nymph::error for a source whose top level is not `key: value`
  ///         lines and whose view must change.
  std::string source_with_view() const;
  /// @brief The CPU textures (view_options::cpu_textures) the draw data
  ///        refers to.
  const texture_store& textures() const;

 private:
  struct impl;
  std::unique_ptr<impl> impl_;
};

} // namespace bdg::nymph
