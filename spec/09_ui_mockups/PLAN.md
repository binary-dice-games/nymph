# nymph — User Interface Mockups: Plan

Design: [DESIGN.md](DESIGN.md). Each step is a reviewable deliverable; the
next starts after its tests pass. Tests go in `tests/` with
`nymph_add_test(<name> <sources>)`, deterministic, no fixed ports or
shared temp names; PNG comparisons use the software rasterizer. Every step
ends with the full suite green and every existing example's PNG bytes
unchanged (DESIGN goal 5). A `CHANGELOG.md` entry is added in the steps
that ship user-visible behaviour (3 onwards).

## 1. Several slots per element class

**Goal:** an element class can fill more than one slot, and binding,
errors and the schema output handle it; nothing visible changes yet.

**Deliverables:** `src/schema/element_class.*`: `slots()` (primary
first), `slot()` kept as the primary; `accepts_slot()` and
`registry::names_accepting()` over every slot; `node::placed_as` (the slot
the parent accepted it in), set by `src/document/bind.cpp`; the "goes
inside" error listing parents for every slot; `src/cli/schema_doc.cpp` and
`src/schema/schema.cpp` listing all slots.

**Tests** (`test_schema`, `test_bind`): a fake class with slots `{a, b}` is
accepted by a parent of `a` and of `b`, with `placed_as` set accordingly;
a parent accepting neither gives the error naming both parents; `nymph
schema` prints both slots; existing schema, bind and docs tests unchanged.

**Result (done):** `element_class::slots()` / `fills_slot()` / `also_fits()` /
`placement_of()` / `placed_as()`, `node::placed_as`, `registry::names_taken_by()` and
`names_accepting(const element_class&)`; `test_bind` (`bind_slots`, 5 tests). Children are listed
in the parent's slot order, each once, so existing messages are unchanged.

## 2. Screen, Window and the first widgets (risk step)

**Goal:** a `Screen` with several positioned `Window`s holding `Text`,
`Button` and `Checkbox` renders to a deterministic PNG.

**Deliverables:** `src/elements/ui/` with `register_ui()` (called from
`register_builtin_elements()`): `screen.cpp` (`Screen`, `Window` as
`ui.window` and figure, placement, cascade, `###path` names, stock style
pushes and the Screen style fields, background), `ui_element.hpp`
(`ui_element_class`, `with_widget_fields()`: `width`, `disabled`,
`tooltip`, `opens`), `ui_state.hpp` and `draw_context::ui_value<T>()`
(PNG: filled from the source each frame), `Text`, `Button`, `Checkbox`.
Bind-time geometry check (window outside the Screen). The unique-ID check
(DESIGN section 3, "Unique IDs"): `draw_context::set_check_ids()`
recording `GetItemID()` after every item and throwing on a repeat with
both paths; ImGui assertions enabled in test builds. Measure how many
frames auto-fitted windows need; if more than three, add
`element_class::settle_frames()` and use it in `render_frames()`.

**Tests** (`test_ui_screen`): two windows render at their positions
(title bar pixels at the expected rectangles, later window on top where
they overlap); `width: 0` auto-fits to the content (window rectangle read
back from the layout record equals the widest widget plus padding);
`width: -1` reaches the Screen edge; a `Window` as `root` fills the image;
`Checkbox value: true` draws the check mark (pixel at the box centre
differs from `false`); a window past the Screen edge is a positioned error
with the numbers; same title in two windows gives two windows; two renders
give the same bytes; a source without UI renders as before. IDs: two
sibling `Button`s labelled `OK`, two `Button`s labelled `##` and two
windows titled `Tool###x` all render with `set_check_ids(true)` and no
ImGui assertion; a deliberately broken draw (the check's own test, a fake
element that pops its ID) makes the check throw, naming both paths.

**Result (done):** `src/elements/ui/` (`ui_common`, `screen`, `widgets`, `register_ui`),
`src/render/ui_state.hpp`, `draw_context::ui()` / `path_of()`; `render_figure()` now always indexes
paths. Three frames settle auto-fitted windows, so no `settle_frames()` hook was needed.
`test_ui_screen` (15 tests, with `tests/ui_test_util.hpp`); `docs/elements.md` regenerated.

## 3. UI flow: transitions between windows

**Goal:** labelled arrows connect windows of a Screen and follow them.

**Deliverables:** `Window.id`; `WindowTransition` (slot `ui.flow`, accepted by
`Screen`): `from`, `to`, `label`, `color`, `thickness`, `dashed`, text
style; bind errors for an unknown `from` / `to` (with a suggestion) and a
duplicate window `id`; the Screen draws its windows, then the arrows from
the windows' rectangles of the same frame (edge to edge along the centre
line, parallel offsets for several transitions between one pair, a loop
for `from` = `to`, label panel at the middle); label pick and edit target.

**Tests** (`test_ui_flow`): an arrow between two windows starts and ends
on their edges (pixels of the line colour just outside each window's
border, none inside the target window's body); the arrowhead points at
`to`; two transitions between one pair do not overlap (their label
rectangles are disjoint); a self-transition draws a loop right of the
window; `to: logn` is an error suggesting `login`; two windows with
`id: a` are an error at the second; a transition to a `closable` window
closed in a live view is not drawn; moving a window in a live view moves
its arrow's end; the arrow's label is a pick in a live view.

**Result (done):** `WindowTransition` (named so because the state diagrams own `Transition`),
`Window.id`, `src/elements/ui/flow.cpp`; the Screen draws windows, then transitions (lanes and
window ids in `ui_frame`). Labels slide along the arrow to clear the windows; loops go to the side
with the most room. `test_ui_flow` (10 tests, including the live-view pick, a dragged window and a
closed one).

## 4. Widget catalog: text, buttons, values, choices

**Goal:** every non-container widget of DESIGN section 3 renders from its
fields.

**Deliverables:** in `src/elements/ui/`: text family (`TextUnformatted`
or `"%s"`, never the text as a format), buttons, `ProgressBar` (0-1),
the slider / drag / input templates
registered for every name and count, `SliderAngle`, `VSlider*`, ranges,
`InputText*`, colours, `Combo`, `ListBox`, `Selectable`, `RadioButton`;
`int_format_field()` beside `double_format_field()`; `items` as a text
array accepting `$column`. `Separator` / `SeparatorText` gain the
`ui.widget` slot and draw ImGui's own in a window (`placed_as`).

**Tests** (`test_ui_widgets`): each element binds with its defaults and
renders inside a window (one parameterised test over every registered
`ui.widget` class, so a new one cannot be forgotten); `SliderFloat3`
needs exactly 3 values, `SliderInt` an integer; `format: "%s"` and
`"%n"` are errors on float and integer widgets; `Combo items: $names`
takes the column; a `Text` of `50% %d %s` draws those characters
literally (its item width equals `CalcTextSize` of the same string);
`selected` out of range is an error; the docs test sees every new type in
`docs/elements.md`; every widget class drawn twice with the same label in
one window passes the ID check (parameterised over the registry).

**Result (done):** `ui_flags.*`, `value_widgets.cpp` (one class per family, registered per ImGui
name), the text, button, choice widgets in `widgets.cpp`; `Separator` / `SeparatorText` fill
`ui.widget` too. The registry test found that an empty `Combo` refused its own
default `selected: 0`; an empty list now means no selection. `test_ui_widgets` (9 tests).

## 5. Placeholder images (risk step)

**Goal:** `Image` and `ImageButton` draw any built-in placeholder, sharp
and deterministic, in windows and in layouts.

**Deliverables:** `cmake/NymphPlaceholders.cmake` embedding
`placeholders/*.svg` with the fonts' `NymphEmbedFile.cmake`;
`placeholders/` with the twelve SVGs of DESIGN section
3, drawn for nymph, and a `README.md` stating the authoring rules and
licence; `src/placeholders/` (`parse_svg()` for the subset,
`placeholder_names()`, parse-once cache,
`draw_placeholder()` with `contain` / `cover` / `stretch`, rounding clip,
colour roles); `Image` (slots `ui.widget` and `figure`, natural size
160 x 120) and `ImageButton`; `image` an enumeration generated from the
catalog.

**Tests** (`test_placeholders`): every embedded file passes the authoring
rules (square viewBox, allowed elements, the three colours, simple
polygons); every placeholder renders at 32, 160 and 640 px with the
software rasterizer, twice with the same bytes, and at `scale: 2` without
blur (edge pixels as sharp as at scale 1: the anti-aliasing fringe stays
within 2 device pixels); `contain` centres a wide image in a tall
rectangle, `cover` covers it and nothing is drawn outside it; `tint:
"#2F80ED"` draws every full-tone shape pixel in that colour and the
half-tone parts at half its alpha (pixels sampled inside each kind of
shape), a `#RRGGBBAA` tint applies its alpha, and `background` colours
the box and the cut-outs; with `background: "#00000000"` the pixels
around the shape equal the window background; with neither set, the
colours follow the light and dark themes;
`image: persn` is an error suggesting `person`; an `Image` in a
`VerticalLayout` takes its natural size; the PNG of an `Image` figure is
equal with the OpenGL rasterizer within the GL tolerance (when a device is
available); an extra test-only SVG breaking a rule is reported by the
checker with the rule.

**Result (done):** nymph's own SVG-subset parser (`src/placeholders/svg_shapes.*`) instead of
nanosvg (no submodule; the files are nymph's), `src/placeholders/placeholders.*`,
`cmake/NymphPlaceholders.cmake`, twelve drawings and `placeholders/README.md`,
`src/elements/ui/image_widgets.cpp`, `name_table` built at run time. Rounding paints the corners
in the window colour (ImGui clips only to rectangles). Enumeration errors now suggest the closest
name (every enum field). `test_placeholders` (13 tests).

## 6. Containers and figures in windows

**Goal:** trees, headers, tabs, children, groups and layout helpers nest
widgets, and figure elements draw inside windows.

**Deliverables:** `TreeNode`, `CollapsingHeader` (`open` through
`SetNextItemOpen` with `limits_cond()`), `TabBar` / `TabItem` /
`TabItemButton` (`selected` through `SetSelected`), `Child`, `Group`,
`Indent`, `Disabled`, `SameLine`, `NewLine`, `Spacing`, `Dummy`;
`ui::draw_figure_in_flow()` (DESIGN section 4, "Figures in
windows") used by every container for `figure` children.

**Tests** (`test_ui_containers`): a closed `TreeNode`'s children are not
drawn (no edit / pick record for them, window auto-height smaller than
open); two `TabItem`s with the second `selected` draw the second's
content; `SameLine` puts two buttons on one row (same item y); a `Plot` in
a `Window` with `height: -1` fills the rest of the window; a
`VerticalLayout` of `Label`s inside a `TabItem` lays out as in a layout;
`Disabled` draws its children with the disabled alpha; three frames (or
`settle_frames()`) settle a tab bar (renders at frames 3 and 4 equal);
the ID fixture (one label repeated across siblings, nested trees, two
`TabItem`s, `Child`ren and `Group`s) passes the ID check, and the two
same-labelled `TreeNode`s open independently (one `open: true`, one not).

**Result (done):** `src/elements/ui/containers.cpp` (13 classes), flag tables for trees, tab bars,
tab items and child regions, `ui::draw_children()` / `draw_figure_in_flow()`,
`element_class::set_primary_slot()` (tabs fill only `ui.tab`). Two fixes found here: ImGui's concave
fill needs one winding, so placeholder paths and rounding masks are normalised before filling (a
round avatar came out as a diamond); `tests/ui_test_util.hpp` clears targets per frame. Tab bars
settle within the three frames. `test_ui_containers` (9 tests).

## 7. Tables

**Goal:** `Table` draws data columns and widget rows.

**Deliverables:** `Table`, `TableColumn` (`label`, `width`, `cells`
accepting `$column`), `TableRow` (one child per cell); headers when any
column has a label; `freeze`, `height`, `flags`; errors for column cell
lists of different lengths and a `TableRow` with more cells than columns.

**Tests** (`test_ui_table`): a table from `$name` / `$size` columns shows
one row per data row (row count from the table's internal state, cell text
targets); widget rows hold a `Checkbox` per row; mismatched lengths and an
extra cell are positioned errors; `Borders|RowBg` flags parse; a table
with `ScrollY` and `height` scrolls instead of growing; repeated column
labels and a `Checkbox ##` in every row pass the ID check.

**Result (done):** `src/elements/ui/tables.cpp`, table and column flag tables; data cells record
`cells` targets (read-only in the editor when bound). `test_ui_table` (7 tests).

## 8. Menus and popups (risk step)

**Goal:** menu bars, menus, popups, modals and tooltips can be shown in a
PNG and opened by `opens`.

**Deliverables:** `MainMenuBar` (a bar across the top of the Screen, not
the display), `MenuBar` (sets the window's `MenuBar` flag), `Menu`
(`open`), `MenuItem`; `Popup`, `PopupModal` (`id`, `open`, `x`, `y`; modal
dim limited to the Screen), `Tooltip` (`open`); the `opens` field opening
a popup in a live view, and placing an `open` popup under that widget in a
PNG; popup IDs derived from element paths (`OpenPopup(ImGuiID)`,
`BeginPopupEx`); errors for an `opens` naming no popup, a duplicate popup
`id`, and more than one open menu chain.

**Tests** (`test_ui_menus`): `Menu open: true` draws its items below the
bar; a modal at the Screen's centre with the area outside the Screen
undimmed (pixels outside the Screen equal the source without the modal);
`Popup` placed under its `opens` button; `Tooltip open: true` beside its
widget; the error cases above; two popups with the same title text, two
menus labelled `File` in two windows' menu bars and same-labelled
`MenuItem`s pass the ID check; a `Button` in a nested `Child` opens a popup
declared on the Screen (different ID stacks meet).

**Result (done):** `src/elements/ui/menus.cpp` (7 classes); the Screen draws windows, then popups,
then transitions. Found here: ImGui keeps one popup stack, so several open popups or menus replaced
each other every frame and none showed; a Screen now allows one, and a second is a bind error.
The main menu bar's zero padding leaked into its menus (popped right after its `Begin`).
`test_ui_menus` (9 tests, two of them live: a nested button opening a Screen popup, a menu item
click).

## 9. Interaction in live views

**Goal:** in `live_view` the user can use the widgets with the pointer,
and the state persists across frames.

**Deliverables:** `live_view` owns the `ui_state` across frames;
`limits_cond()` drives every source-state setter; window clamping to the
Screen after moves and resizes; `RadioButton` sibling rule; widget picks
(`hovered()`, `take_click()`) from item rectangles; `takes_wheel()` over
scrolling windows; `io.ConfigInputTextCursorBlink = false`.

**Tests** (`test_live_view_ui`, synthetic input through
`live_view_driver.hpp`): static parity with `render_figure()` for every
UI example; clicking a `TreeNode` label opens it (its child's pick appears
at the expected place); clicking a tab switches content; dragging a
slider changes `values` of its pick; dragging a window's title moves it
and it stops at the Screen edge; clicking a `Button` returns a click pick
`{type: Button, label}`; a button with `opens` shows its popup; after
input stops, `wants_frame()` goes false within the settling frames.

**Result (done):** windows stay on their Screen (size constraints and clamping after moves),
cursor blink off in every context; the rest (state persisting in the live view's context, picks,
clicks, wheel areas, the RadioButton rule) came with the earlier steps. `test_live_view_ui`
(static parity on three sources, tree, tab, slider, radio, button, window drag, wheel, reset).

## 10. Keyboard and text input

**Goal:** text fields and keyboard-driven widgets work in `nymph view`.

**Deliverables:** `live_view::input_key()`, `input_text()`,
`wants_keyboard()`, `set_clipboard_hooks()`; `src/window/viewer_window.cpp`
routes keys and text to the figure while `wants_keyboard()` (otherwise the
viewer's shortcuts, as today) and wires the clipboard; ImGuiKey mapping in
`window_win32.cpp` and `window_x11.cpp` (extending the editor's key set:
letters, digits, function keys as ImGui needs them).

**Tests** (`test_live_view_ui`, `test_view_command`): click an
`InputText`, type "Moon", the pick's value is `Moon`; backspace and Ctrl+A
work; Ctrl+click on a slider and typing sets a value; `wants_keyboard()`
is true only while a field is active; the key router (a pure function of
`wants_keyboard()` and the key) sends `q` to the field when it is active
and to the viewer otherwise. Manual check on Windows and Linux: type in a
field, copy and paste, `Q` closes only outside a field.

**Result (done):** `live_view::input_key()`, `input_text()`, `wants_keyboard()` (ImGui's
`WantTextInput`), `set_clipboard_hooks()`; `imgui_key()` shared in `window_host.hpp`; `route_key()`
in `view_command`. No platform file changed: the editor's key set already covers a text field. No
system clipboard in the native window yet (the hooks are there for it). Tests in
`test_live_view_ui` (typing, Ctrl+click on a slider, the clipboard hooks) and `test_view_command`
(routing). Driving the window by hand is still to do.

## 11. Saved view for UI state

**Goal:** what the user changed is saved in the `view` key, re-applied in
PNGs and live views, and kept across `set_source()`.

**Deliverables:** `src/document/view.*`: the UI entries of DESIGN section 4
(`open`, `value`, `selected`, `active`, `checked`, window geometry,
`collapsed`, `closed`, `scroll`, Screen `order`, tab `selected` / `order`)
with positioned errors and suggestions for malformed entries;
`source_with_view()` writing touched UI elements; applying entries while
settling and in PNGs (Screen window order and the tab bar's `order` are not
saved; a tab's selection is saved on the `TabItem`); `set_source()` keeping a state when path, type and
source value are unchanged; `reset_view()` dropping all.

**Tests** (`test_saved_view` additions): expand a node, move a window,
type a value, then `source_with_view()` holds exactly those three entries;
rendering that source gives the same draw data as the live view; nothing
changed gives the source back byte for byte; a `set_source()` changing a
slider's source value replaces the user's value, one changing another
widget keeps it; an entry of a removed element is ignored; `{open: 3}` is
a positioned error.

**Result (done):** eleven `view_kind`s for UI elements with their keys (`document/view.*`),
`element_view` UI members, `write_view()` given the kinds; widgets start from the saved entry while
the source applies and keep the value they would save (`ui_entry::numbers`, `text`, and a window's
`moved` / `resized` / `scrolled` / `collapse_changed`); `source_with_view()` merges touched entries.
Tests in `test_live_view_ui` (saved entries, the saved source rendering the same pixels as the
screen, nothing changed giving the source back, a new source value winning, entries applied in
PNGs, positioned errors).

## 12. Web viewer and MCP App

**Goal:** UI sources are interactive in the browser, including typing.

**Deliverables:** `web/nymph_web.cpp` exports for key, text and
`wants_keyboard`; `nymph-viewer.js`: focusable canvas for UI figures,
`keydown` / `keyup` / text mapping with `preventDefault()` only while
`wants_keyboard()`, clipboard through `navigator.clipboard` where allowed;
`hover` / `click` / `view` events for widgets (no API change). The MCP App
and the Chrome extension need no change; verify the App passes the UI
`view` entries back in `ui/update-model-context`.

**Tests** (Node and Playwright, when the toolchain is configured): the
module renders every UI example within the GL tolerance of its committed
PNG; in Chromium, clicking a tree node opens it, typing into an
`InputText` changes the `view` event's value, the page does not scroll
while typing, and Tab leaves the canvas when no field is active; `toYaml()`
holds the UI entries.

**Result (partly done):** `nymph_viewer_key()` (DOM key names mapped in C++), `nymph_viewer_text()`,
`nymph_viewer_wants_keyboard()`, `wantsKeyboard` and `uiRevision` in the state; `live_view::ui_revision()`
so `view` events fire for widgets; `nymph-viewer.js` sends keys while a field has the keyboard,
every key release, and paste as text (ImGui reads its clipboard synchronously); copying writes the
page clipboard where allowed. The placeholder SVGs lost their XML comments (the single-file viewer is
inlined into HTML) and a test keeps them out. The MCP App needed only a wording change. The Node test
drives a text field through the module; **the Chromium tests were not run** (the checkout's
`node_modules/playwright` is incomplete here).

## 13. View editor

**Goal:** `nymph view --edit` selects and edits widgets.

**Deliverables:** `ui_element_class` records the item rectangle as the
element target and `label` / `text` / `title` as text targets
(`record_target()`); windows get `NoInputs` in edit mode; the element
list shows windows and widgets.

**Tests** (`test_view_edit_flow` additions): in edit mode, a click on a
`Button` selects its `label` target instead of returning a widget click;
committing a new label redraws equal to rendering the patched source; a
window `title` is editable; edit mode off: draw data unchanged.

**Result (done):** UI windows, child regions, popups and the main menu bar take no mouse input in
edit mode (`NoMouseInputs`, which keeps their focus, unlike `NoInputs`); widgets record a text target
for their `label` or `text`; the live view gives focus back to a mockup window after an edit-mode
click on the figure's own window (`kFigureWindowName`). Tests in `test_view_edit_flow` (select and
edit a button label, a window title and a text; edit mode alone draws the same).

## 14. Documentation, examples and agent guidance

**Goal:** agents and users can find and use the feature.

**Deliverables:** `docs/format.md` "User interfaces" section (Screen,
windows, the naming rule, figures in windows, popups, the `view`
entries); `docs/elements.md` regenerated; `examples/ui_editor.yaml` (the
DESIGN example grown into a three-window tool), `examples/ui_settings.yaml`
(a single `Window` as root: tabs, a table, a modal), `examples/ui_flow.yaml` (a sign-in, loading and inbox flow with transitions),
`examples/ui_placeholders.yaml`
(a contact sheet of every placeholder with its name) and their PNGs;
`docs/examples.md`; `docs/agent-guide.md` and `claude/skill/` recipe
"Mock a user interface"; `docs/viewer.md` (keyboard, widget events);
README "Core Concepts" row; `CHANGELOG.md`; DESIGN.md updated to what was
built; `spec/INDEX.md` status.

**Tests:** the docs test (schema markdown in sync), the example PNG
tests (committed PNGs equal fresh renders), the skill check.

**Result (done):** `docs/format.md` "User interfaces" (and the `view` table), `docs/examples.md`
with `ui_editor`, `ui_settings`, `ui_flow` and `ui_placeholders` (PNGs drawn with the software
rasterizer), `docs/agent-guide.md` section, recipe and error rows, `claude/skill/nymph/SKILL.md`
(and the plugin copies), `docs/viewer.md`, `docs/cli.md`, README, CHANGELOG. Writing the examples
found two more defects, fixed: a figure in a window with a fixed `height` was drawn at its natural
size, and ImGui's `DefaultOpen` and negative `Button` sizes were refused (both are ImGui usage
agents write). `clang-format` was not available here: the new files follow the style by hand and
should go through `format-all.ps1`.

## Completion Criteria

- All tests above pass; the full suite is green on Linux and CI builds
  macOS, MSVC and MSYS2. *Linux: all 684 tests and the 11 Node tests of the
  web module pass; CI, the Chromium tests and the window by hand are not
  yet run.*
- No source, whatever its labels, produces an ImGui ID conflict: the ID
  check passes over every example and fixture.
- Every placeholder of the catalog can be named by `Image` and renders
  sharp at any size and scale, in windows and in layouts.
- Every existing example renders the same bytes; plain live views and the
  editor behave as before.
- An agent can write a multi-window mockup from ImGui names alone, render
  it to a PNG that shows every window as written, and get a positioned
  error for any mistake.
- In `nymph view` and the web viewer the user can expand tree nodes,
  switch tabs, tick, drag, type, open menus and popups and move windows;
  saving keeps what they did, and the saved PNG shows it.
- DESIGN.md matches the code; `spec/INDEX.md` marks the project done.
