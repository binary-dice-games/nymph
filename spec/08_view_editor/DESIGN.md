# nymph — View Editor: Design

**Status: done** (plan steps 1-10; see [PLAN.md](PLAN.md)). Built and tested with
MSVC on Windows, including the window and the panel driven by hand; the X11 side
(key and text input) was written but not compiled or run. Sections 3, 4 and 6
describe what was built.

## 1. Purpose / Scope

`nymph view --edit chart.yaml` (or the `E` key in a running `nymph view`)
opens the **editor**, a panel docked to the right edge of the `nymph view`
window. The user clicks a text or an element in the figure; the editor shows its
parameters (text, font, size, colour, and every other field of the element)
and each change redraws the figure at once. The editor changes the YAML
source, nothing else: what is on screen is always what `nymph chart.yaml`
would render from the edited source.

It covers:

- Selecting by clicking in the figure (plot title, axis labels, legend, a
  `Label`, a diagram node, a link label, a group title) or from an element
  list in the editor, for things that cannot be clicked (a plot with no
  title yet, `legend_location`, grid flags).
- Editing any field of the selected element with a widget fitted to its
  type, built from the element's schema (`field_desc`), so every present and
  future element is editable without per-element UI code.
- Undo / redo and saving.

It also adds to the source format optional `font`, `font_size` and
`text_color` fields on every diagram item that draws text (section 7), so a
single node's or link's text can be styled; unset, they inherit.

It does not cover: adding, removing or reordering elements; editing the
`data` tables (a text bound to a `$column` is shown read-only, section 7)
(both added later by [14_editor_workspace](../14_editor_workspace/DESIGN.md));
separate styles for a plot's title, labels and legend (a plot's text shares
the plot's style); the web viewer and the Chrome
extension (the editor is native only); live data feeds.

Builds on [04_web_viewer](../04_web_viewer/DESIGN.md) (`live_view`, picks,
saved `view` key, section 10 native window) and
[01_core_features](../01_core_features/DESIGN.md) (schema, source format,
text style) and [03_uml_diagrams](../03_uml_diagrams/DESIGN.md) (ImUml).
Files owned or changed: new `src/edit/` (`source_edit`, `editor_model`,
`inspector`), `src/window/viewer_window.cpp`, `src/window/window_host.hpp`
and the two platform files (text and key input), `src/view/live_view.*`,
`src/render/draw_context.*`, `src/elements/*` (target recording),
`src/elements/uml/*` (per-item text style), `imuml/` (text runs, per-item
text style), `src/window/view_command.*` (`--edit`), `docs/format.md`.

## 2. Design Goals

1. **The source is the only model.** Every edit is a text patch to the YAML
   source, re-parsed and validated like any source. No parallel object tree
   to keep in sync, nothing that cannot be saved, a PNG saved from the
   editor round-trips.
2. **Click-to-edit, with a schema panel behind it.** Clicking is the main
   path; the same inspector is reachable without clicking, so nothing
   editable is unreachable.
3. **No per-element editing code.** What can be edited, with which widget,
   its range and enum names come from the schema. An element only has to
   say where its texts are drawn (section 4).
4. **Zero cost and zero change outside edit mode.** PNG output and the
   plain viewer are byte-identical to today; target recording is skipped
   unless the view is in edit mode (as picks are skipped unless
   `interactive()`).
5. **Comments and layout of the source survive an edit.** The patch changes
   the one value, not the file.
6. **Bad input never breaks the view.** An edit that makes the source
   invalid is rejected with the error shown in the editor; the figure keeps
   the last good source.

## 3. Key Abstractions

```
 pointer in figure area ──► live_view (edit mode) ──► edit_target (rect, path, field)
       ▲                           │ set_source(patched)        │ selection
       │ frames                    ▼                            ▼
 viewer ◄──────────── source_edit (text patch) ◄──── editor_model ◄── inspector (ImGui)
   │  pointer / keys / text in panel area ───────────────────────────────┘
   └─ one native window: figure on the left, panel strip on the right
```

- **`edit_record`** (`src/render/draw_context.hpp`): one clickable thing the
  last frame drew: the `node`, `field` (the text field it shows, or empty for
  the element as a whole), the `text` shown, a `read_only_reason`, and the
  `area` (logical pixels). Recorded by elements through
  `draw_context::record_target()`, a no-op unless edit mode is on; drawing
  order is the order of the list. Cleared at `begin_frame()`, so it always
  describes the last complete frame. `live_view` turns the records into
  `edit_target`s (element path, type, CSS pixels) once per frame.
- **`item_range`** (`draw_context.hpp`): the ImUml nodes, edges and events
  an element submitted while it drew, as index ranges, taken in
  `draw_context::draw_node()` from `ImUml::GetItemCounts()` before and after
  (edit mode only). It is how a diagram text is traced back to the child
  element that made it, with no code in the individual diagram items.
- **`source_edit`** (`src/edit/source_edit.hpp`): pure functions on source
  text: `set_field(source, element_path, field, yaml_value)` and
  `remove_field(...)`. Uses the node and field positions the parser already
  records (`node::line/column`, per-field value positions) to splice the
  value in place; inserts a missing key at the end of the element's mapping,
  in flow (`{type: Plot, ...}`) or block style, matching the element.
  Returns the new source or a positioned `nymph::error`.
- **`editor_model`** (`src/edit/editor_model.hpp`): platform-neutral state of
  the editor: current source, selection (an element path plus optional
  focused field), undo / redo stacks (whole-source snapshots; sources are
  small), last error, and the list of editable fields of the selection
  (schema + current values, parsed from the source). `commit(field, value)`
  runs `source_edit`, validates the result by binding it (a throwaway
  `live_view` is not needed: `bind` is enough), pushes undo and returns the
  new source for the view. Testable without a window.
- **`inspector`** (`src/edit/inspector.hpp`): draws the editor's contents
  with Dear ImGui from an `editor_model`: undo / redo / save, element
  tree (collapsible `TreeNode`s, an icon per element type from
  `src/edit/element_icons.*`, ancestors opened when the figure selects a
  nested element), "Selection" section (focused field first), "All fields"
  section, all in a scrolling area above an error panel that appears at the
  bottom while the last change is refused. Pure ImGui, no OpenGL, so it
  runs headless in tests against a plain ImGui context.
- **Docked panel** (in `src/window/viewer_window.cpp`): a strip of
  `panel_width` logical pixels (320) at the right of the one window. The
  viewer owns a second, plain ImGui context (not the figure's), also drawn
  with `imgui_impl_opengl3` into the same GL context, restricted to the
  strip by `glViewport` / scissor. Pointer events with x in the strip go to
  the panel's context, the others to the figure (offset-free: the figure
  starts at x = 0); keyboard and text go to the panel while a panel text
  field is active, otherwise the viewer's keys apply. The figure's view box
  is the client area minus the strip.
- **Field widgets by type**: `boolean` checkbox; `integer` / `number` a
  number box (`src/edit/number_box.*`) clamped to the `limits`; `text`
  single or multi-line input (a field
  whose description says line breaks start a new line is multi-line);
  `color` colour picker plus hex input and "automatic" (empty); `enumeration`
  combo from `names`; `flags` one checkbox per name; `font` (a `text` field
  with the font check) a combo of `nymph fonts`; arrays and `$column`
  references are shown read-only as source text.
- **`number_box`** (`src/edit/number_box.hpp`): a thin adapter over ImGui's
  own `DragScalar` (`io.ConfigDragClickToInputText`: a click without
  dragging types the value; Enter, Tab or a click elsewhere accepts,
  Escape cancels; values are clamped to the limits). A sideways drag
  changes it: the step per pixel is a power of ten near 1/200 of the
  limits' span, or of the value's size when the span is unknown or huge,
  at most one unit for a bounded range and at least 0.1 for whole numbers
  (ImGui's own Shift / Alt make it finer / coarser); a drag lands on a
  multiple of its step so it writes `0.3`, not `0.30000000000000004`. It
  reports `dragged` (one undo step per gesture) or `typed`. `font_size` 0
  shows as "auto" (the format text) and a drag from it starts at the
  inherited size (`editor_model::inherited_font_size()`). An earlier
  hand-built box (a button drawn by hand, swapped for an `InputText` on a
  click) lost the keyboard in the browser and was replaced.
- **`edit_toolbar`** (`src/edit/edit_toolbar.hpp`): a floating toolbar for
  the fields changed most often, drawn as its own window in the panel's
  ImGui context (which covers the whole window) over the figure, centred
  above the selection's rectangle, else under it, else at the top of the
  figure. Tools appear when the element has the field and does not take it
  from the data: `shape` (a grid of shape pictures drawn with
  `ImUml::RenderShape` from a private ImUml context, tabs by category for
  the shape library; flowchart names map to library shapes), `fill_color`,
  `color`, `text_color` (a palette of 18 colours, Automatic, a custom
  picker), `colormap` (rows of ImPlot colormap stripes from a private ImPlot
  context; an element without colours of its own, such as a pie chart or a
  heatmap, edits its plot's: `colormap_owner()`), `font`, `font_size`,
  `line_weight`, `line`, `arrow`, `marker` (a grid of marker pictures),
  `marker_size`; then copy
  style (a format painter: the next element selected takes the style as one
  `set_fields()` step; enumerations only between elements of one type),
  clear style, duplicate and delete (through `tree_menu`, so deleting a
  referenced node asks). The viewer passes the selection's rectangle
  (`live_view::target_for()`) and sends the pointer to the panel over the
  toolbar (`editor_panel::over_toolbar()`), except during a drag that began
  in the figure.

## 4. Finding the object under the cursor

The question wish answered for its widgets ("what is at (x, y)?") and the
answer is reused: **capture each object's rectangle while drawing, key it
by an identity, clear it every frame, and hit-test the map.** wish does it
in a central `render_node()` override that stores `hit_test_map_[id] =
{rect, hovered, ...}` right after each widget is drawn; the map is rebuilt
every frame so a query describes the last completed frame (wish
`src/automation/DESIGN.md`, "Hit-test capture mechanism"). nymph differs in
two ways: its elements draw into ImPlot plots and ImUml diagrams whose
inner texts are not ImGui widgets, so the central hook alone yields only
element rectangles; and identity is the existing element path
(`root/children/1`), already used by picks and the `view` key.

Capture happens at three levels, coarse to fine:

1. **Elements (central).** The frame driver records, for every element, its
   rectangle (`draw_context::rect()` already holds it). Clicking anywhere in
   an element selects it. No element code needed.
2. **Plot texts.** `Plot`, `GanttChart` (`record_plot_texts()` in
   `plot_pick.cpp`), `Plot3D`, `SankeyDiagram`, `Label` and `SeparatorText`
   record their title, axis labels, legend or text. ImPlot's rectangles are
   read from its internal structs right after `EndPlot()` (a pointer taken
   before it stays valid): the title is centred over `PlotRect` at the top of
   `CanvasRect`; an axis label sits inside the axis' `HoverRect` less
   `PlotPadding` (X at the bottom, Y turned a quarter round, so as wide as the
   text is high); the legend is `Items.Legend.Rect`, only when drawn. The
   rectangles were checked against rendered output. Each legend entry is
   also a target of the series it names (field `label`; a pie chart's slice
   label selects the chart). ImPlot leaves `ImPlotItem::LegendHoverRect`
   empty, so the entries are laid out from the legend rectangle as ImPlot
   does (rows one text line high, `LegendInnerPadding` / `LegendSpacing`,
   sorted with `ImPlotLegendFlags_Sort`, one row when horizontal). In edit
   mode the plot adds `ImPlotLegendFlags_NoButtons`, so a click selects
   instead of hiding the series (drawing is unchanged). The series' own
   lines and bars are not targets; their legend entry and the Elements list
   reach them.
3. **Diagram texts.** ImUml draws text with its own `TextCentered` /
   `TextAt` helpers. With `ImUmlDiagramFlags_CaptureText` the diagram
   records every text it draws as an `ImUmlTextRun` (`Kind`: node, edge,
   event or title; `Item`: index in submission order; screen rectangle; the
   text), published in `ImUmlDiagramInfo::TextRuns` after `End*()`. Drawing is
   unchanged (the flag only adds recording). `diagram_class::record_texts()`
   finds the element that made each item from the `item_range`s (the
   narrowest, for groups) and maps the run to the field **whose value equals
   the text shown** (`field_showing()`): a node's `text` or `name`, a link's
   `label`, a group's `title`; a node that shows its `id` for an empty text
   maps to its first empty `text` / `label` / `name` / `title`; an
   autonumbered message ("3. text") is matched without its number. This
   needs no per-diagram code and no run roles. A text no field shows (a
   class's id, its member lines, a guillemet stereotype) stays with the
   element target.
4. **Diagram shapes.** With `ImUmlDiagramFlags_CaptureItems` the diagram
   also records the screen areas of the shapes each item draws as
   `ImUmlItemArea`s (`ImUmlDiagramInfo::ItemAreas`): a node's or
   participant's box, a note's or fragment's frame as one rectangle; an edge,
   a message, a lifeline or an `else` line as its path cut into pieces of a
   few pixels, each padded by 4 pixels, so a diagonal arrow is not one large
   rectangle. `record_texts()` turns each area into an element-level target
   of the element that made the item (same `item_range` lookup), before the
   texts, so a click on an arrow or a box selects its link or node rather
   than the diagram.

**Hit test:** among targets whose rectangle contains the pointer, the one
with the smallest area wins, ties going to the later draw order (what is on
top). Text targets are therefore chosen over their element, and a node text
over its diagram. A click on the empty part of a plot selects the plot.
Rectangles are padded by 3 logical pixels so thin text is easy to hit. An
element recorded in several pieces is hit piece by piece but reported and
outlined as the union of its pieces (`targets_at()`, `target_for()`).

**Click, pan and zoom coexist:** `live_view` already reports a primary
click "pressed and released without dragging" (`take_click()`), so a drag
still pans and the wheel still zooms. In edit mode a click selects instead
of only reporting a pick. Hover outlines the target under the pointer
(thin, dashed); the selection is outlined solid. Both are drawn by
`live_view` on the figure window's foreground draw list, after the figure,
in edit mode only.

## 5. Data Flow

1. `nymph view --edit f.yaml`: as today the source is checked by a throwaway
   view, then the window opens at the layout's size plus the panel strip
   (clamped to the screen; a smaller window gives the figure less, and the
   existing "too small" fallback applies). `E` shows or hides the strip,
   growing or shrinking the window by its width when the screen allows.
2. A frame in edit mode: elements call `record_target()`; `live_view` keeps
   the map and draws the overlay.
3. Click in the figure area: `viewer` asks `live_view::targets_at(x, y)`,
   passes the best to `editor_model::select(path, field)`; the panel
   redraws.
4. The user changes a widget: `inspector` calls `editor_model::commit()`;
   `source_edit` patches the text; the result is bound to validate; on
   success the viewer calls `live_view::set_source(new_source, false)`.
   Elements keep their view state (zoom, pan, hidden series) because their
   path is unchanged (`set_source` already keeps state by path).
5. Typing in a text field commits on every change after a 150 ms pause or
   on Enter / focus loss, so the figure follows typing without a parse per
   keystroke. Colour drags and sliders commit continuously but coalesce
   into one undo step per gesture.
6. Save is one button. After a confirmation dialog it overwrites the input
   file: a PNG input is rendered again (`render_png()`) and replaced as a PNG, a
   YAML input is replaced as YAML, both from `source_with_view()`. Closing
   the window (its button, Esc, Q) with changes not saved opens an "unsaved
   changes" dialog (save, discard, cancel); `window_events::on_close_request()`
   lets the viewer veto the platform's close. Both dialogs are ImGui modals in
   the panel's context, which covers the whole window, so they dim the figure
   too; they take the keyboard (Enter saves, Esc cancels). The `S` / `Y` keys
   still write copies, `<input>.view.png` / `.view.yaml`, never over the input
   (`view_save_path`).

## 6. Public API Contract

`live_view` additions (all no-ops until `set_edit_mode(true)`):

```cpp
void set_edit_mode(bool on);                       // frames recorded and overlay drawn from the next frame
std::vector<edit_target> targets_at(float x, float y) const;  // CSS px; best first
std::optional<edit_target> target_for(std::string_view element, std::string_view field) const;
void set_selection(std::optional<edit_target_id> id);          // outline; cleared when its element is gone
const std::optional<edit_target_id>& selection() const;
std::vector<edit_element> element_list() const;    // path, type, label, depth: for the editor's tree
std::optional<edit_target> take_edit_click();      // best target under a click without a drag, once
```

`editor_model` (`src/edit/editor_model.hpp`): `select()`, `fields()` (schema
order, current value, `written`, `bound_to`, `scope_note`), `commit(field,
yaml_value, merge, replace_binding)`, `clear(field)`, `undo()`, `redo()`,
`reset()`. `field_value.hpp`: `encode_text / bool / int / number / color /
enum / flags` and `find_source_problem()`. `inspector` (`src/edit/inspector.hpp`):
`widget_for(field_desc)`, `draw()`, `edit_text()`, `flush()`. `number_box`:
`draw(id, value, options, width) -> number_box_change`, `drag_step()`,
`tidy()`. `edit_toolbar`: `tools_for(model)`,
`place(anchor, area, w, h)`, `draw(model, menu, area, anchor)`,
`contains(x, y)`, `copy_style()`, `paste_style()`, `clear_style()`. The panel
itself is `src/window/editor_panel.*` (its ImGui context and the OpenGL3
backend), driven by `viewer_window.cpp`.

`source_edit`: `std::string set_field(std::string_view source, std::string_view
element_path, std::string_view field, std::string_view yaml_value)`;
`remove_field(...)` has the same shape. Throws `nymph::error` for an unknown
path or field, a source it cannot patch (top level not `key: value` lines,
as `replace_top_level`), or a value the field's type rejects (the message
is the one the loader would give). The caller validates the whole result.

`editor_model::commit(field, value) -> commit_result {ok, source | error}`.
An error leaves source, selection and undo stack untouched.

`window_events` / `native_window` additions: `on_text(std::u32string_view)`,
`on_key(window_key, modifiers)` extended with the keys a text field needs
(backspace, delete, arrows, home, end, enter, tab, and `ctrl` + a, c, v, x,
y, z), and `set_clipboard` / `clipboard`. Still one window.

Source format (docs/format.md, schema): `font`, `font_size`, `text_color` on
diagram items that draw text (section 7); the ImUml API additions are
`ImUmlDiagramFlags_CaptureText`, `ImUmlTextRun`, `ImUmlDiagramInfo::TextRuns`
and `ImUml::GetItemCounts()`.

CLI: `nymph view --edit` (docs/cli.md, docs/viewer.md); `E` toggles the
panel.

## 7. Design Decisions

- **Click-to-edit plus a schema panel, not either alone.** A fixed
  parameter form (title, labels, legend) suits plots but cannot reach
  diagram texts, and a click-only scheme cannot reach absent or invisible
  things (an empty title, `legend_location`). Both read the same schema, so
  the cost of having both is one tree list and one "All fields" section.
  Click selects, the schema panel edits.
- **A docked panel in the one window, not a second native window.** A
  second window needs a second GL context, a shared X11 connection and an
  event loop serving both. A panel needs only text and key input in the
  existing window. The panel has its own ImGui context so its style, font
  and input never touch the figure's. Cost: the figure's box shrinks by the
  strip, which the window grows to absorb when the screen allows.
- **Edit the YAML text, not an object model.** An object model would
  need a serializer that preserves comments, flow vs block style and
  `$column` references; the text patch keeps all of that and is checked by
  the one loader. Rejected: re-emitting the YAML from the parsed tree.
- **Snapshot undo.** Sources are a few KB; whole-source snapshots cannot
  get out of step with the text. Capped at 200 steps.
- **Per-item text style, falling back to the element's.** Today `font`,
  `font_size` and `text_color` exist only per element (a diagram, a plot, a
  `Label`) and are inherited down the tree; diagram nodes, groups and notes
  have only `color` / `fill_color` (outline and fill). This project adds the
  same three fields, optional, to every diagram item that draws text (nodes,
  groups, links, notes, messages, transitions, class members' owner, ...):
  empty / 0 means the item's diagram value, which itself falls back to the
  enclosing elements and `image`. The base class `item_class` appends
  `with_text_style()` to every item's fields, so every diagram gets them in
  one place; `draw_node()` already wraps each element's draw in its
  `text_style_scope`, which now also pushes `ImUmlCol_Text` (ImUml reads the
  font, size and colour as it records the item, so its layout follows the
  font size). A plot's title, ticks and legend still share the plot's one
  style; the inspector labels such a field "applies to all text of this
  Plot". Per-plot-text styles are not added.
- **Text from data is read-only.** A `text` or `text_array` field written as
  `$column` takes its texts from a data cell, possibly shared by several
  elements. The inspector shows the reference and its current text but
  edits only literals. Editing cells (and a "replace by literal" action) is
  a later step because the `data` tables are not yet patchable by position.
- **The image options are selected like an element.** `image:` is not an
  element, so the editor gives it the path `image` and the type "Image"
  (`src/edit/image_fields.*` describes its eight options as fields, so the
  same widgets, ranges and validation apply). `live_view` records the whole
  figure as the lowest target (the smallest wins, so only the margin and the
  gaps of no element reach it) and lists "Image" first in `element_list()`.
  `source_edit` patches the `image` mapping, creates it for the first option
  and `editor_model::clear()` drops it with its last. In the window a changed
  `width` / `height` resizes the window to the new layout (the figure otherwise
  follows the window, `--fit fill`); `scale` has no effect on the screen.
- **Pan, zoom and view state are not edit operations.** They stay mouse
  gestures on the figure, saved in the `view` key as today. The editor's
  "View" row offers Reset view and shows the current axis ranges; typing
  ranges writes the `view` entry through the existing saved-view code.
- **A toolbar for the frequent fields, not a second inspector.** The panel
  lists every field, which makes the few changed most often (colours, shape,
  text size) slow to find. The toolbar repeats only those, next to the
  selection, and builds itself from the same schema: an element gets a tool
  when it has the field, so no per-element code. It lives in the panel's
  ImGui context (its popups, keyboard and undo are the panel's) rather than
  in the figure's, which is redrawn from the source and must stay
  byte-identical to the PNG.
- **A number box instead of ImGui's drag field.** `DragScalar` needs a
  double-click or Ctrl+click to type, drags at a speed unrelated to the
  value, and with a `%.2f` format would show (and type over) a rounded
  value. The number box types on a plain click, shows the shortest decimal,
  and scales its drag step to the limits or the value.
- **Legend position by field, not by dragging.** `legend_location` is a combo
  of the nine locations. Dragging the legend to a corner is a possible
  later nicety; ImPlot's own legend context menus stay off.
- **Overlay in `live_view`, not in elements.** Outlines are drawn in one
  place from the target map; elements only record rectangles.

## 8. Constraints and Invariants

- Edit mode off: `record_target()` returns before touching anything; draw
  data and PNG bytes equal today's (tested). Edit mode on, nothing hovered
  or selected: draw data equal to edit mode off.
- Single thread, one window, one GL context. The figure's ImGui / ImPlot /
  ImUml contexts are only current inside `live_view` calls, the panel's own
  ImGui context only inside the viewer's panel calls; neither leaks into the
  other. The panel is drawn after the figure, with its own viewport, scissor
  and clear; GL state the figure's backend leaves behind must not change the
  panel's output.
- The panel's ImGui context uses the embedded default font; it never
  shares an atlas with the figure.
- A rejected edit changes nothing: not the source, the undo stack, the
  selection, or the figure.
- Selection is by element path; it survives text edits, is dropped when the
  path no longer exists.
- Platform: text input is `WM_CHAR` on Win32 and `XLookupString` /
  `Xutf8LookupString` on X11. No new library at link time: everything is
  opened at run time as for `nymph view`. System clipboard: Win32 first;
  X11 selection ownership is later, until then copy/paste works inside the
  panel only and the plan says so.
- High DPI: the panel's ImGui style and strip width scale with the window's
  scale.
- A new diagram field must not change existing output: unset per-item style
  draws exactly as before (existing diagram tests and PNGs unchanged).
- Code style: `src/edit/` follows nymph conventions (`snake_case`, `bdg::nymph`);
  ImUml additions follow ImGui naming (`imuml/` exception).

## 9. Integration Boundaries

Depends on: `live_view` and `draw_context` (04), the schema and
`field_desc` (01), `bind` / `replace_top_level` / saved `view` (01, 04),
`window_host` (04 section 10), ImUml (03). Depended on by: `nymph view`
only. The web viewer is untouched; `edit_target` recording is compiled in
(it is small) but only `nymph view` turns it on.

Risks, checked first in the plan: whether ImPlot's title / axis-label /
legend rectangles can be read (step 4); two ImGui contexts drawn by
`imgui_impl_opengl3` into one GL context in one frame (step 8); text input
and key mapping on X11 (step 8).
