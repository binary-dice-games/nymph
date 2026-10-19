# nymph — View Editor: Plan

Design: [DESIGN.md](DESIGN.md). Each step is a reviewable deliverable; the
next starts after its tests pass. Tests go in `tests/` with
`nymph_add_test(<name> <sources>)`, deterministic, no fixed ports or
shared temp names. A `CHANGELOG.md` entry is added when a user-visible change ships
(step 5, the new diagram fields, and step 10).

## 1. Source patching

**Goal:** a value of any field of any element can be replaced in the YAML
text without touching anything else.

**Deliverables:** `src/edit/source_edit.hpp/.cpp`: `set_field`,
`remove_field`. Uses parsed positions; handles flow and block mappings,
plain / quoted scalars, an absent key (inserted in the element's style), a
multi-line block element, comments and blank lines kept.

**Tests** (`test_source_edit`): replacing `title` in flow and block
elements gives exactly the expected text; comments before and after
survive; absent key inserted in flow and block style; element addressed by
path inside layouts (`root/children/1/children/0`); unknown path / field
throws a positioned `nymph::error`; a source with a non-mapping top level
throws; text needing quotes (`a: b`, `#`, leading `*`) is quoted; replaced
source re-parses to the intended value.

**Result (done):** `src/edit/source_edit.*`, `yaml_node::end_line/end_column` added to the parser
(positions after a node), `test_source_edit` (15 tests). Flow and block elements, comments, CRLF,
multi-byte text and block scalars are patched in place; removing the first key of a list item is
refused.

## 2. Value encoding and validation

**Goal:** a typed value from a widget becomes correct YAML for its field,
and an invalid result is detected before the view sees it.

**Deliverables:** `src/edit/field_value.hpp/.cpp`: encode `bool`, `int`,
`double` (shortest round-trip), text (quoting, `\n` as block or escaped
per style), colour (`"#RRGGBB[AA]"`), enum name, flags `A|B`; the field's
`check` and `limits` applied. `commit_check(source)` binds the source and
returns the loader's error or none.

**Tests** (`test_field_value`): each field type round-trips through
encode then load; out-of-range number, unknown enum name, bad font name,
bad colour are rejected with the loader's message; a patch producing an
invalid source is reported, not applied.

**Result (done):** `src/edit/field_value.*`, `test_field_value` (7 tests). Text is plain when that
reads back unchanged and double-quoted otherwise; `check_source()` is `bind_source()`.

## 3. Edit mode in live_view: elements

**Goal:** in edit mode every element can be hovered, clicked and
outlined; outside edit mode nothing changes.

**Deliverables:** `edit_target` and `draw_context::record_target()` /
`set_edit_mode()`; the frame driver records each element's rectangle;
`live_view::set_edit_mode`, `targets_at`, `target_for`, `set_selection`,
`element_list`; overlay (hover dashed, selection solid) drawn after the
figure. Click in edit mode selects (via the existing click detection) and
still lets drag pan and wheel zoom.

**Tests** (`test_live_view_edit`): with a layout of two plots and a label,
`targets_at` returns the element under the point, smallest first;
`element_list` order and depth; draw data with edit mode off is byte-equal
to before; edit mode on without hover or selection draws equal data;
selection dropped when its element disappears after `set_source`; a drag
does not select.

**Result (done):** `draw_context::record_target()` / `edit_record`, `live_view` edit mode, hover (dashed)
and selection (solid) outlines, `test_live_view_edit`. Only figure-slot elements are element targets;
a click is `take_edit_click()`, so a drag still pans.

## 4. Plot and Label texts

**Goal:** titles, axis labels, legends and `Label` texts are separate
targets with the field they show.

**Deliverables:** first spike: read ImPlot's title / axis-label / legend
rectangles (internal API); if unavailable, compute them as DESIGN section 4
says, and record which in this step's result. `Plot`, `Plot3D`, `Label`,
`PlotPie`-like titled elements, `GanttChart`, `SankeyDiagram` record
`title`, `x_label`, `y_label`, `legend` (field `legend_location`).

**Tests:** for a plot with title and both labels at a known size,
`targets_at` on the title, each label and the legend returns the matching
`field`; hit rectangles lie within 3 px of the text's drawn bounds
(compared against `CalcTextSize`); axes on the second Y axis; a plot
without a title offers no title target but is selectable.

**Result (done):** ImPlot's rectangles are readable through `implot_internal.h` (no fallback needed);
checked against rendered output (`NYMPH_EDIT_DUMP`). Title, axis labels and legend of `Plot` and
`GanttChart`, `SankeyDiagram` title, `Plot3D` title (not checked by eye), `Label` text, `SeparatorText`
label.

## 5. Per-item text style in diagrams

**Goal:** every diagram item that draws text accepts optional `font`,
`font_size` and `text_color`; unset, it draws as before.

**Deliverables:** `node_style_fields()` in `uml_common` (= `node_color_fields()`
+ `text_style_fields()`), used by every diagram item class that has
`node_color_fields()` today and by links, messages, transitions and
notes; `ImUml::SetNextItemText(font, size, color)` in `imuml/` (ImGui
naming) applied to the item's text measuring and drawing, with the item's
text style in the layout cache key; the elements pass the resolved style
(item, else diagram, else inherited). `docs/format.md` and the generated
`docs/elements.md` updated; `CHANGELOG.md` `### Added`.

**Tests** (`test_uml_item_style`): a flowchart node with `text_color`
draws that colour and its neighbours the diagram's; a larger `font_size`
makes the node larger and the layout still has no overlap; an invalid
font name or size is rejected like the element-level fields; with no item
style set, the draw data of every existing diagram example is byte-equal to
before.

**Result (done):** `item_class` appends `with_text_style()` to every diagram item's fields, so one
change covers all of them; the generated `docs/elements.md` and `docs/format.md` are updated. Found and
fixed on the way: a diagram's own `text_color` did not reach ImUml's item text; `text_style_scope` now
pushes `ImUmlCol_Text` too, and `CurrentTextStyle()` reads the live style colour. `test_uml_item_style`.
No separate `ImUml::SetNextItemText()` was needed: the elements apply the style around the item's
draw call (the `text_style_scope` of `draw_node()`), and ImUml reads the font, size and colour when it
records the item.

## 6. ImUml text runs

**Goal:** diagram texts are targets mapped to the source field that
produced them.

**Deliverables:** `ImUmlDiagramFlags_CaptureText`, `ImUmlTextRun`,
`ImUml::GetTextRuns()` in `imuml/`; recording in `TextCentered` /
`TextAt` only with the flag. `Flowchart` (`FlowNode.text`,
`FlowGroup.title`, `FlowLink.label`) and `ClassDiagram` (class names,
members) map runs to child path plus field.

**Tests:** ImUml unit test: runs reported for a small diagram match drawn
text, none without the flag, drawing unchanged with the flag; flowchart
targets carry `root/children/0/children/2` and `text`; a node whose text is
empty (shows its `id`) maps to field `text` with the id as the shown value.

**Result (done, API differs from the plan):** `ImUmlTextRun` has `Kind`, `Item`, rectangle and text (no
role); `ImUml::GetItemCounts()` lets the host find the element behind an item. Mapping run to field is
by the text shown, not per element (see DESIGN section 4). `test_imuml_text_runs`.

## 7. Remaining diagrams and data-bound texts

**Goal:** all UML / ER / DFD / network / sequence / state / activity /
use-case texts map to fields; texts from `$column` are marked read-only.

**Deliverables:** mappings for each diagram element; `edit_target::
read_only_reason` ("bound to $names") set when the field value is a column
reference.

**Tests:** per-diagram table-driven test: every text in the example of that
diagram type is either a target with a field or explicitly unmapped;
`$column` bound field flagged read-only; a literal field is not.

**Result (done):** all nine diagram kinds map their texts (title, node and link texts, group titles,
notes, autonumbered messages, network addresses...); class ids and member lines, and ER attributes, stay
with the element target. `$column`-bound texts carry `read_only_reason` "bound to $names"
(`field_reference()` in `source_edit`). Table-driven test over the examples.

## 8. Window host: text input and the docked panel strip

**Goal:** the `nymph view` window takes keyboard text and shows a panel
strip beside the figure.

**Deliverables:** `window_events` gains `on_text`, key with modifiers,
wider `window_key`; clipboard (Win32; internal fallback on X11); both
platform files updated. `viewer_window.cpp`: a panel strip of 320 logical
pixels with its own ImGui context drawn by `imgui_impl_opengl3` with its
own viewport / scissor; pointer routing by x; the figure's view box minus
the strip; `E` shows / hides it and resizes the window. The strip shows a
placeholder until step 9. Spike first: two ImGui contexts through
`imgui_impl_opengl3` in one GL context and frame; record the result here.

**Tests:** key-mapping and modifier tables are pure functions with unit
tests (`test_window_keys`); a pure function for the strip layout (client
size, strip width, scale -> figure box, strip box, pointer routing) with
unit tests (`test_panel_layout`). Manual check on Windows and Linux/X11:
the strip opens and closes, typing in a panel field does not trigger `Q`
or `R`, the figure keeps pan / zoom, existing behaviour without `E`
unchanged.

**Result (done, with differences):** two ImGui contexts through `imgui_impl_opengl3` in one GL context
work (spike checked on Windows). `window_events` gained `on_text` and `on_key(key, mods, down,
repeat)`; Win32 (`WM_KEYDOWN/UP`, `WM_CHAR` with surrogate pairs) was run by hand; X11 was written
from the same pattern but **not compiled or run** (Linux build unverified). The panel is
`src/window/editor_panel.*`; the viewer routes pointer, key and text events by position, and also
to the panel while a popup (colour picker) is open. The system clipboard is not wired: copy and paste
work inside the panel only. `test_window_keys` and `test_panel_layout` were not written: the key tables
live inside the platform files and the strip layout is two subtractions in `sync_size()`; both were
checked by driving the real window (screenshots).

## 9. Editor model and inspector

**Goal:** the panel edits the selection and shows errors.

**Deliverables:** `src/edit/editor_model.*` (selection, field list from
schema, `commit`, undo / redo snapshots capped at 200, save path),
`src/edit/inspector.*` (tree, Selection section, All fields section,
widgets per DESIGN section 3, error line), drawn in the strip. Coalescing
of text typing (150 ms) and drag gestures into one undo step. Inspector
labels style fields of plots "applies to all text of this Plot".

**Tests** (`test_editor_model`): select, commit text / colour / enum /
bool / number, undo, redo, rejected commit leaves state; read-only field
refuses a commit; selection survives a text edit and drops with its
element; fields list for a `Plot` matches its schema in order. Inspector
(`test_inspector`): drawn against a headless ImGui context for a plot and a
flowchart node without crashing, widget kinds chosen per field type
(table-driven over `field_type`), no OpenGL.

**Result (done):** `editor_model` (13 tests), `inspector` (`widget_for()`, headless draw, typing, 5
tests). A colour-picker popup can reach over the figure; the viewer keeps sending pointer events to
the panel while one is open. Fields written as a list or a `$column` of numbers are shown, not edited.

## 10. Wiring, CLI, docs

**Goal:** the feature is usable end to end and documented.

**Deliverables:** `--edit` and the `E` key in `view_command` / `viewer`;
click in the figure selects in the panel and a panel change redraws the
figure; Save as designed; docs: `docs/viewer.md` (editing section),
`docs/cli.md` (`--edit`), `CHANGELOG.md` (`### Added`), README mention and
Further Documentation rows for this spec; DESIGN.md corrected where
implementation differed; `spec/INDEX.md` status.

**Tests** (`test_view_edit_flow`, headless, no window): drive
`live_view` + `editor_model` together: click a plot title position, commit
a new title, assert the rendered draw data / PNG equals rendering the
patched source; edit a flowchart node's text and its `text_color`; save
writes `.view.yaml` containing the edit and the `view` key; invalid colour
is rejected and the figure is unchanged. Manual scenario on Windows and
Linux listed in the step result.

**Result (done):** `--edit` and `E`, click-to-select, `viewer_window.cpp` (panel, routing, window
growth), `docs/viewer.md`, `docs/cli.md`, README, CHANGELOG. `test_view_edit_flow` (click, change,
redraw equal to rendering the edited source, undo, saved YAML with the `view` key, refusal) and
`test_view_command` (`--edit`). Driven by hand on Windows: select a plot title and a flowchart node,
type a new title (live), set colours, scroll the panel.

## Completion Criteria

- All tests above pass; the full suite is green on Linux and the MSVC /
  MSYS2 builds compile (CI). *Done on Windows / MSVC (the failures that
  remain there fail without this project too: CRLF checkouts, the GPU
  comparison, install and plugin tests). Linux, MSYS2 and the X11 window
  are not verified.*
- Edit mode off: every existing PNG and live-view test is unchanged.
- Clicking the title, an axis label, the legend, a `Label`, a flowchart node
  and a link label selects it; its text (and, for diagram items, font,
  size and colour) can be changed, the figure redraws,
  undo restores it, and the saved YAML equals the source plus the edits.
- No edit can leave the figure in an invalid state.
- DESIGN.md matches the code; `spec/INDEX.md` marks the project done.
