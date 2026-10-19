# nymph — User Interface Mockups: Design

**Status: implemented (plan steps 1-14; see [PLAN.md](PLAN.md) for each
step's result). Verified on Linux (GCC, the software rasterizer, the web
module in Node); the Chromium tests of the web viewer, the native window
driven by hand, macOS and Windows builds are not yet verified.**
[PLAN.md](PLAN.md) is the ordered implementation plan. This document describes only what this project adds
or changes; the rest of the architecture is
[spec/01_core_features/DESIGN.md](../01_core_features/DESIGN.md)
(*core DESIGN*), [spec/04_web_viewer/DESIGN.md](../04_web_viewer/DESIGN.md)
(*viewer DESIGN*) and [spec/08_view_editor/DESIGN.md](../08_view_editor/DESIGN.md)
(*editor DESIGN*).

## 1. Purpose / Scope

nymph draws charts and diagrams with Dear ImGui. This project lets a
source describe a **user interface** instead: one or more ImGui windows,
each holding a tree of ImGui widgets (buttons, sliders, tree nodes, tabs,
tables, menus, ...). nymph renders every window into the PNG, so an AI
agent can design or mock a user interface and show it to the user the way
it shows a chart. In a live view (`nymph view`, the web viewer, the
Claude MCP App) the same interface is drawn in real time and the user can
use it: expand tree nodes, switch tabs, tick checkboxes, drag sliders,
type in text fields, open menus, move, resize and collapse windows.

```yaml
image: {width: 900, height: 560}
root:
  type: Screen
  children:
    - type: Window
      title: Scene
      x: 16
      y: 16
      width: 260
      height: 360
      children:
        - type: TreeNode
          label: World
          open: true
          children:
            - {type: Selectable, label: Camera}
            - {type: Selectable, label: Sun, selected: true}
        - {type: Separator}
        - {type: Button, label: Add object}
    - type: Window
      title: Properties
      x: 292
      y: 16
      width: 320
      children:
        - {type: InputText, label: Name, value: Sun}
        - {type: SliderFloat, label: Intensity, value: 0.8, min: 0, max: 2}
        - {type: ColorEdit3, label: Color, value: "#FFE4B0"}
        - {type: Checkbox, label: Cast shadows, value: true}
```

It covers:

- A `Screen` element (a figure element holding windows) and a `Window`
  element, with ImGui's window decorations, flags and placement.
- An element for every user-facing widget function of `imgui.h`, named as
  the function (section 3), with its parameters as fields.
- ImPlot / ImPlot3D plots, layouts and the other existing figure elements
  inside windows (section 4, "Figures in windows").
- Interaction in live views: pointer, keyboard and text input reach the
  widgets; the state the user changes is kept by element path, saved in
  the `view` key and reported to the host.
- **UI flow**: labelled arrows between the windows of a Screen
  (`WindowTransition`), showing how one interface leads to the next ("ok",
  "cancel", "finished loading"), so a single image can show a whole flow
  of screens (section 3, "UI flow").
- The view editor (`nymph view --edit`) selecting and editing widgets.

It does not cover: generating application code from the mockup; actions
(a button does not run anything; it only reports a click, and can open a
popup, section 4); ImGui's docking and multi-viewport features (nymph
pins the non-docking branch, core DESIGN section 8); drag and drop; keyboard
navigation between widgets (`ImGuiConfigFlags_NavEnableKeyboard` stays
off); IME composition in the web viewer; images read from files or
embedded in the source (an `Image` shows one of nymph's built-in
placeholder images, section 3).

Files owned or changed: new `src/elements/ui/` (the element family,
`register_ui()`), `src/schema/element_class.*` (several slots per class),
`src/document/bind.cpp`, `src/cli/schema_doc.cpp`, `src/schema/schema.cpp`
(slot lookups), `src/render/draw_context.*` (UI state, figure-in-flow),
`src/render/frame_setup.*` (frame count), `src/view/live_view.*` (UI state,
keyboard and text input), `src/document/view.*` (UI entries of the `view`
key), `src/window/viewer_window.cpp` (key routing), `web/` (keyboard
input, focus), `src/elements/layout_elements.cpp` (`Separator` /
`SeparatorText` in windows), new `placeholders/` (the placeholder SVG
files), `src/placeholders/` (SVG-subset parser, catalog and drawing),
`cmake/NymphPlaceholders.cmake` (their embedding, with the fonts'
`NymphEmbedFile.cmake`), `src/schema/convert.cpp` (a suggestion for an
unknown enumeration name), `docs/`, `examples/`, `claude/skill/`.

## 2. Design Goals

1. **ImGui's vocabulary.** An agent that knows Dear ImGui can write a
   mockup without reading nymph's schema first: element types are ImGui
   function names (`TreeNode`, `SliderFloat3`, `BeginTable` → `Table`),
   fields are the function's parameters, flags are ImGui's flag names
   without their prefix.
2. **The PNG is the interface as the source describes it.** Every widget
   is drawn in the state its fields give (open, selected, value, collapsed,
   scroll position), deterministically, with no input. Nothing appears in
   a PNG that the source did not ask for.
3. **The live view starts from the PNG.** Before any input, a live view
   draws exactly the PNG (viewer DESIGN, goal 1 and "static parity"); the
   user's changes are view state, like a zoomed plot, never edits to the
   source.
4. **Real ImGui widgets, not pictures of them.** Widgets are drawn by
   ImGui's own functions in real ImGui windows, so they look and behave as
   in an ImGui application, and interaction is ImGui's own.
5. **No change to anything else.** Sources without UI elements render the
   same bytes; plain live views behave as before; every existing test
   passes unmodified.
6. **Strict and safe, as the rest of nymph.** Unknown types, fields and
   flag names are errors; no source text reaches printf unchecked; widget
   geometry that would leave the screen is an error, not a clipped window.
7. **Labels never have to be unique.** ImGui identifies items by label;
   nymph gives every element an ID of its own (section 3, "Unique IDs"),
   so two `Button`s labelled `OK` are always two buttons, never an ID
   conflict, shared state or an ImGui assertion.

## 3. Source Format

### Elements and slots

| Slot | Elements of the slot | Accepted by |
|---|---|---|
| `figure` (existing) | `Screen`, `Window` (filling its layout rectangle) and every existing figure element | layouts, root, and every UI container (figures in windows, section 4) |
| `ui.window` | `Window`, `MainMenuBar`, `Popup`, `PopupModal` | `Screen` |
| `ui.flow` | `WindowTransition` | `Screen` |
| `ui.widget` | every widget below, `Separator`, `SeparatorText` | `Window`, `Child`, `Group`, `Indent`, `Disabled`, `TreeNode`, `CollapsingHeader`, `TabItem`, `TableRow`, `Popup`, `PopupModal`, `Menu`, `Tooltip` |
| `ui.menubar` | `MenuBar` | `Window`, `Child` |
| `ui.menu` | `Menu`, `MenuItem` | `MenuBar`, `MainMenuBar`, `Menu` |
| `ui.tab` | `TabItem`, `TabItemButton` | `TabBar` |
| `ui.table` | `TableColumn`, `TableRow` | `Table` |

A class may now fill several slots (section 5): `Window` is both a
`ui.window` and a `figure`, `Separator` and `SeparatorText` are both
`figure` and `ui.widget`, `MenuItem` and `Separator` are accepted in menus
because menus accept `ui.widget`.

- **`Screen`** (figure): the area windows live in, usually `root`. Its
  `width` / `height` hints follow the layout rules (core DESIGN section 3,
  "Layout"); its natural size is the bounding box of its children with an
  explicit position and size, plus 16 px. Fields: `background` (colour;
  empty is the theme's window background mixed 10 % towards black, or
  towards white in dark themes, so windows stand out), the style overrides
  of section 4, and the text style.
- **`Window`** (`ui.window` and figure): `title` (shown in the title bar;
  ImGui's `##` / `###` suffixes are allowed), `x`, `y` (relative to the
  Screen; default: cascaded, 24 px apart, from (16, 16)), `width`,
  `height` (positive: pixels; 0: fitted to the content, ImGui's auto-fit;
  negative: to the Screen's right / bottom edge, so `x: 0, y: 0, width:
  -1, height: -1` fills it), `collapsed`, `closable` (an X button), `flags`
  (`ImGuiWindowFlags` without the prefix: `NoTitleBar`, `NoResize`,
  `NoScrollbar`, `AlwaysAutoResize`, `HorizontalScrollbar`, ...; the
  `MenuBar` flag is set by a `MenuBar` child, not written), `scroll`
  (`[x, y]`, the initial scroll), `id` (a name `WindowTransition`s refer to,
  unique on its Screen), and the text style. The last window is the
  focused one (active title bar), as ImGui focuses each window as it
  appears. A `Window` used as a
  figure element (as `root`, or in a layout) takes its rectangle from the
  layout engine; `x` / `y` are then an error.
- **Order.** Windows are drawn in list order, the last on top; popups and
  menus above all of them, as in ImGui.

### UI flow

A mockup often shows several interfaces and how the user moves between
them: a login window leads to the main window on "ok", a loading screen
to the result on "finished loading". A `WindowTransition` child of the Screen
draws a labelled arrow from one window to another:

```yaml
root:
  type: Screen
  children:
    - {type: Window, id: login, title: Sign in, x: 16, y: 40, width: 240, height: 150, children: [...]}
    - {type: Window, id: loading, title: Loading, x: 360, y: 60, width: 200, height: 100, children: [...]}
    - {type: Window, id: main, title: Inbox, x: 660, y: 16, width: 300, height: 400, children: [...]}
    - {type: WindowTransition, from: login, to: loading, label: ok}
    - {type: WindowTransition, from: loading, to: main, label: finished loading}
    - {type: WindowTransition, from: loading, to: login, label: error, dashed: true}
```

- **Fields:** `from` and `to` (window `id`s; an unknown id is an error with
  a suggestion, and two windows with the same `id` are an error), `label`
  (the event that triggers the transition; may be empty), `color` (empty
  is the theme's text colour at 60 % alpha), `thickness` (1 to 8, default
  1.5), `dashed`, and the text style for the label.
- **Geometry.** The arrow runs along the line between the two windows'
  centres, from the edge of one window's rectangle to the edge of the
  other's (3 px short of each), with an arrowhead at `to`. Several
  transitions between the same two windows (either direction) are drawn
  side by side, one label height plus 8 px apart, so "ok" and "cancel"
  never overlap. `from` equal to `to` draws a loop out of the side of the
  window with the most room inside the Screen (right, left, bottom, top
  on ties), a third of the way along that edge, away from where arrows
  from other windows arrive. The label sits on a small panel of the
  Screen's background colour at the middle of the arrow; when the panel
  would cover either window there, it slides along the arrow to the
  nearest point where it does not.
- **Live windows.** Arrows are drawn after the windows, from their
  rectangles in the same frame, onto the Screen's own draw list (below all
  windows): when the user moves, resizes or collapses a window, its arrows
  follow; an arrow to a closed window is not drawn. A window drawn over an
  arrow hides that part of it, as overlapping windows hide each other.
- **Only windows.** `from` and `to` name `Window`s of the same Screen;
  popups, menus and widgets are not targets (a later extension could start
  an arrow at a widget).
- **Picks.** In a live view, hovering a label reports a pick (`type:
  WindowTransition`, `label`), and a click on it a click; in edit mode the label
  is a text target.

### Widgets

The rule for names: every user-facing widget function of `imgui.h`
(1.92.9) has an element of the same name, `Begin*` / `End*` pairs drop the
`Begin` (`BeginTable` → `Table`, `BeginMenu` → `Menu`) and become
containers whose `children` are what goes between the two calls. A
parameter becomes a field of the same name in `snake_case`; the value a
widget edits through a pointer is `value` (or `open`, `selected`,
`active`, `checked` where ImGui's own name says so).

| Family | Elements | Main fields |
|---|---|---|
| Text | `Text`, `TextColored`, `TextDisabled`, `TextWrapped`, `LabelText`, `BulletText`, `TextLink`, `Bullet`, `SeparatorText` | `text`, `label`, `color` |
| Buttons | `Button`, `SmallButton`, `ArrowButton`, `Checkbox`, `RadioButton`, `ProgressBar` | `label`, `value`, `active`, `direction`, `fraction`, `overlay`, `size` |
| Sliders | `SliderFloat`..`SliderFloat4`, `SliderInt`..`SliderInt4`, `SliderAngle`, `VSliderFloat`, `VSliderInt` | `value` (number, or 2-4 numbers), `min`, `max`, `format`, `flags` |
| Drags | `DragFloat`..`DragFloat4`, `DragInt`..`DragInt4`, `DragFloatRange2`, `DragIntRange2` | as sliders, plus `speed` |
| Inputs | `InputText`, `InputTextWithHint`, `InputTextMultiline`, `InputFloat`..`InputFloat4`, `InputInt`..`InputInt4`, `InputDouble` | `value`, `hint`, `step`, `step_fast`, `format`, `flags` |
| Colours | `ColorEdit3`, `ColorEdit4`, `ColorPicker3`, `ColorPicker4`, `ColorButton` | `value` (colour), `flags` |
| Choices | `Combo`, `ListBox`, `Selectable` | `items` (text array, or `$column`), `selected`, `height_in_items` |
| Trees | `TreeNode`, `CollapsingHeader` | `label`, `open`, `flags`; children |
| Tabs | `TabBar`, `TabItem`, `TabItemButton` | `selected` on the `TabItem`, `closable`, `flags` |
| Tables | `Table`, `TableColumn`, `TableRow` | `Table`: `flags`, `freeze`, `height`; `TableColumn`: `label`, `width`, `cells` (text array, or `$column`); `TableRow`: one child per cell |
| Menus | `MainMenuBar`, `MenuBar`, `Menu`, `MenuItem` | `label`, `shortcut`, `checked`, `enabled` |
| Popups | `Popup`, `PopupModal`, `Tooltip` | `id`, `open`, `x`, `y`; children |
| Layout | `SameLine`, `NewLine`, `Spacing`, `Dummy`, `Separator`, `Indent`, `Group`, `Child`, `Disabled` | `offset`, `spacing`, `size`, `width`, `child_flags`, `scroll` |
| Images | `Image` (also a figure element), `ImageButton` | `image` (a placeholder name), `width`, `height`, `fit`, `tint`, `background`; below |

Fields every widget has (`with_widget_fields()`, appended like
`with_text_style()`): `width` (`SetNextItemWidth`: positive pixels,
negative to the container's right edge less that many pixels, 0 ImGui's
default), `disabled` (draws it between `BeginDisabled` / `EndDisabled`),
`tooltip` (`SetItemTooltip`, live views only), `opens` (the `id` of a
`Popup` / `PopupModal` a click opens, section 4), and the text style.

`Table` gives two ways to fill cells. Data tables: `TableColumn` children
with `cells: $column` (one text row per cell; every column the same
length, an error otherwise), the way agents hold tabular data. Widgets in
cells: `TableRow` children, one child element per cell (`Text`,
`Checkbox`, a `Button`, ...). Both may be combined: column rows first.
Headers are shown when any column has a `label` (`TableHeadersRow`).

Not included, and why:

| ImGui | Why |
|---|---|
| `PlotLines`, `PlotHistogram` | `PlotHistogram` already names an ImPlot series; a `Plot` inside the window draws both better. |
| `Image` of an arbitrary texture | The source never names a file (core DESIGN section 8) and does not carry pixels; `Image` names a built-in placeholder. |
| `InputScalar*`, `DragScalar*`, `SliderScalar*`, `CheckboxFlags`, `Value()` | Covered by the typed elements. |
| `SetTooltip`, `BeginItemTooltip`, `SetItemTooltip` | The `tooltip` field; `Tooltip` for a tooltip shown in the PNG. |
| `Columns` | Legacy; `Table` replaces it. |
| Drag and drop, multi-select, docking, viewports, `Log*`, `ShowDemoWindow` and the other tool windows | Not interface content, not in the non-docking branch, or no mockup use. |

The full list with every field is `nymph schema` and `docs/elements.md`,
generated from the registry as for every element.

### Unique IDs

ImGui identifies every item by an ID hashed from its label (the part
after `###` if there is one, else the whole label) and the ID stack.
Two items with the same ID in the same scope are one item to ImGui: they
press, open and hover together, ImGui flags an ID conflict, and some
containers (tab bars, tables, popups) assert. Labels in a mockup are
written by an agent and repeat all the time (`OK` in two dialogs, three
`Selectable`s called `Item`, empty `##` labels), so nymph never lets a
label alone decide an ID, and the source never has to make labels unique:

1. **Path-scoped IDs.** Every element is drawn under `PushID(child
   index)` (core DESIGN section 4, `draw_context::draw_children`, which
   already does it), so an item's ID is the hash of its element path plus
   its label. Siblings differ by index; elements in different containers
   differ by their parents' indices. This covers every widget, `TreeNode`,
   `TabItem`, `Menu`, `MenuItem`, `Child`, table columns and cells.
2. **Windows.** A window's ImGui name is global, not scoped by the ID
   stack, so it is `title###<element path>`: two windows may share a
   title, and a `###` the author wrote in a title only affects what is
   displayed (ImGui hashes from the last `###`).
3. **Popups.** `OpenPopup(label)` and `BeginPopup(label)` hash with the
   ID stack where each is called, which differs between the `Button` that
   `opens` a popup and the popup itself. nymph derives a popup's ID from
   its element path and uses the ID overloads (`OpenPopup(ImGuiID)`,
   `BeginPopupEx`), so the two always meet and two popups never share an
   ID. The `id` field a popup has is a nymph name that `opens` refers to;
   a duplicate `id` is a bind error.
4. **Everything nymph keeps is keyed by path**, never by label:
   `ui_state`, picks, edit targets and the `view` entries.

`##` and `###` in labels keep their ImGui meaning for display (`##`
hides the rest), so sources copied from ImGui code look the same.

**Checked, not assumed.** A test-only `draw_context` switch
(`set_check_ids(true)`) records `ImGui::GetItemID()` after every item each
frame and throws `std::logic_error` naming both element paths on a
repeat. The UI tests run with it on, over every example and over a
fixture that repeats one label in every container kind (siblings, nested,
tabs, menus, table cells, popups, windows with the same title). Unlike
ImGui's own conflict highlighting, which reports only the item under the
mouse, this sees every item without input. ImGui assertions stay on in
test builds, so an ID clash inside ImGui fails a test too.

### Placeholder images

Mockups need images to show layout and intent (a portrait, a product
photo, a map), but a source carries no pixels and names no file (core
DESIGN section 8). nymph instead builds in a small catalog of
**placeholder images**, which `Image` and `ImageButton` name:

```yaml
- {type: Image, image: person, width: 96, height: 96, rounding: 48}
- {type: Image, image: landscape, width: -1, height: 180, label: "Hero photo, 1600 x 600"}
- {type: Image, image: logo, width: 64, height: 64, tint: "#2F80ED", background: "#00000000"}
```

| Name | Shows |
|---|---|
| `cross` (default) | The wireframe box with diagonals: "an image goes here" |
| `person` | Head-and-shoulders silhouette, for a portrait photo |
| `people` | Two silhouettes, for a team or group photo |
| `landscape` | Mountains and sun, for any photo |
| `product` | A box, for a product shot |
| `logo` | A generic mark |
| `video` | A frame with a play triangle |
| `map` | A folded map with a pin |
| `chart` | Bars, for a chart thumbnail |
| `document` | A page with lines |
| `icon` | A small square glyph, for app or file icons |
| `building` | A building, for a venue or company |

- **Fields** (`Image`; `ImageButton` adds `label`, its ID and accessible
  name): `image` (an enumeration of the catalog, so a wrong name gets the
  list and a suggestion, and `nymph schema Image` lists them), `width` /
  `height` (pixels, or the hints of the context: negative fills, 0 is the
  natural 160 x 120), `fit` (`contain`, the default, keeps the aspect
  ratio and centres; `cover` fills and clips; `stretch`), `tint` (a
  colour, `"#RRGGBB"` or `"#RRGGBBAA"`: the colour of the whole
  placeholder shape; its half-tone parts are drawn in the same colour at
  half its alpha, so one field recolours the image consistently; empty is
  the theme's `TextDisabled`), `background` (the colour behind the shape
  and of its cut-outs; empty is the theme's `FrameBg`; `"#00000000"`
  makes it transparent, so the shape alone sits on the window), `rounding`
  (corner radius of the background; half the size gives a circle; ImGui
  clips only to rectangles, so the corners outside the rounding are
  painted in the window's background colour over the shape), `border`, `label` (a caption centred over the
  bottom of the image, for intent such as "Hero photo, 1600 x 600").
- **Everywhere.** `Image` fills both the `ui.widget` and `figure` slots,
  so a placeholder also works in layouts and dashboards outside windows.
- **Monochrome by design.** A placeholder is one shape colour on one
  background colour, the grey-box style of wireframes. It follows the
  theme by default, reads as "not the final image", and `tint` /
  `background` recolour it per use (a brand-coloured logo, a dark avatar
  on a light card). A transparent `background` has no cut-outs to show,
  so a placeholder whose design needs them (checked per file: it uses
  `#FFFFFF`) draws them in the window's background colour instead.
- **Vector, drawn by ImGui.** Each placeholder is an SVG file in
  `placeholders/`, embedded in the binary the way fonts are (core DESIGN
  section 8, "Fonts"; `cmake/NymphPlaceholders.cmake`) and parsed once
  per process by nymph's own parser for the subset below
  (`src/placeholders/svg_shapes`). Its paths are drawn
  with `ImDrawList` (cubic curves flattened by ImGui, `PathFillConcave`,
  `PathStroke`) in the item's rectangle, so it is sharp at any size and
  `image.scale`, needs no texture, and goes through nymph's rasterizers
  like any other geometry (deterministic bytes, core DESIGN goal 3). Every
  build embeds every placeholder (a few KB in all), the web build too,
  so nothing is fetched on demand.
- **Authoring rules** (checked by a test over every file, so a new file
  that breaks them fails the build's tests): a square `viewBox`; only
  `path`, `rect`, `circle`, `ellipse`, `polygon` and `line` (the parser
  turns them into lines and cubic curves); fill or stroke colour `#000000` (drawn in
  `tint`), `#808080` (tint at half alpha, for depth) or `#FFFFFF` (drawn
  in `background`, to cut shapes out, since ImGui has no even-odd fill);
  every filled path a simple polygon; no arcs in paths (write them as
  curves), groups, transforms, gradients, text, images or masks.
  `preserveAspectRatio="none"` on the `<svg>` makes an image always
  stretch (`cross`, whose diagonals run corner to corner). Painter's
  order is the file order.
- **Adding one** is dropping an SVG that follows the rules into
  `placeholders/` (with a licence note if it is not nymph's own; the set
  above is drawn for nymph, MIT): the catalog and the enumeration are
  generated, nothing else changes.

## 4. Behaviour

### Drawing (PNG and live views alike)

- **Windows are real ImGui windows.** `Screen::draw()` calls `Begin()` for
  each window while the figure's full-display window (core DESIGN section
  4, `render_figure()`) is still open; ImGui makes them separate top-level
  windows above the figure window, which has `NoBringToFrontOnFocus` and
  stays below. A window is placed with `SetNextWindowPos` /
  `SetNextWindowSize` at the Screen's origin plus its `x`, `y`.
- **ImGui's stock window style inside a Screen.** The frame setup zeroes
  window padding, border and rounding for the figure window. A Screen
  pushes ImGui's defaults back (`WindowPadding` 8x8, `WindowBorderSize`
  1, the theme's `PopupBorderSize`) and its own overrides on top:
  `window_rounding`, `frame_rounding`, `frame_padding`, `item_spacing`,
  `window_padding`, `frame_border` (fields of `Screen`, default ImGui's,
  so a mockup can look modern with two fields). A `Window` used as a
  figure element pushes the same defaults.
- **State from the source, applied like limits.** Every stateful widget
  applies its source state with the condition `dc.limits_cond()` already
  gives plots (viewer DESIGN section 3, "Axis limits"): `Always` for PNGs
  and while a live view's fresh contexts settle, `Once` after that. Tree
  and header openness through `SetNextItemOpen`, tab selection through
  `ImGuiTabItemFlags_SetSelected`, window position, size, collapse and
  scroll through `SetNextWindow*`; values are kept in `ui_state` (below).
- **Values live in `ui_state`.** ImGui edits a widget's value through a
  pointer (`bool*`, `float*`, a text buffer). `draw_context` owns a
  `ui_state` store keyed by element path: in a PNG it is filled from the
  source each frame; in a live view it persists, initialised from the
  source (section 5).
- **Frames.** `render_frames()` runs three frames (core DESIGN section 4),
  and that is enough: auto-fitted windows, tab bars, tables and popups
  settle over the first two (measured in plan steps 2 to 8), so the frame
  count did not change and other figures keep their bytes (goal 5).
- **Determinism.** Widgets are drawn at the fixed `DeltaTime` and time of
  the frame driver, with no mouse; the software rasterizer samples the
  line texture in integers, so it gives the same bytes on every machine. `ProgressBar.fraction` is 0-1:
  ImGui's indeterminate bar (a negative fraction) depends on time and is
  not offered.
- **No printf.** `Text*` draw with `TextUnformatted` (or `"%s"`), never
  with the text as a format. `format` fields of float widgets are
  `double_format_field()` (one floating-point conversion); integer widgets
  get a new `int_format_field()` with the same rule for one `%d` / `%i` /
  `%u` / `%x`.
- **Popups shown in the PNG.** A `Popup` or `PopupModal` with `open: true`
  is opened (`OpenPopup`) on the first frame and placed at its `x`, `y`,
  else under the widget whose `opens` names it, else (modals) at the
  Screen's centre. A `Tooltip` placed right after a widget, with `open:
  true`, is drawn under that widget (without it, on hover in live views).
  Menus are shown open with `Menu.open: true`. ImGui keeps one stack of
  open popups, so a Screen shows at most one open popup or one open menu
  of a menu bar (with its open submenus); a second is a bind error
  rather than a popup that silently replaces the first. A modal's dimmed
  background covers the Screen, not the whole image: ImGui's own dim
  (the whole display) is switched off and a borderless window the size of
  the Screen, in the theme's dim colour, is drawn under the modal.
- **Popup IDs.** A popup's ImGui name ends in `###popup <element path>`,
  and the Screen both opens it and begins it in its popup pass (after the
  windows), under the same ID stack; widgets with `opens` only record the
  request and their rectangle. So the ID always matches, wherever the
  opener sits, and the popup opens under its opener.
- **Geometry checks.** A window with an explicit position and size that
  does not fit inside its Screen is an error at bind time, with the
  numbers (`window ends at x = 940, the Screen is 900 wide`), as layout
  overflow is (core DESIGN section 7). An auto-fitted window is constrained
  to the Screen (`SetNextWindowSizeConstraints`) and scrolls if its content
  is larger; ImGui draws the scrollbar, so the agent sees it.

### Figures in windows

Any `figure` element may be a child of a UI container: a `Plot` in a
`Window`, a `VerticalLayout` of `Label`s in a `TabItem`, a `GanttChart` in
a `CollapsingHeader`. It is placed by ImGui's flow layout: at the cursor,
with its `width` / `height` hints read as for a root (positive: pixels; 0:
natural size, measured at the available width; negative: the rest of the
available width or height, `GetContentRegionAvail()`). The layout engine
then arranges it in that rectangle and draws it there, and an
`ImGui::Dummy` of its size advances the cursor. So every existing element
works inside a mockup with no change to it. A figure element that needs
more than the available height scrolls with the window rather than being
an overflow error, since a window has a scrollbar and a layout does not.

### Interaction (live views)

| Element | What the user can do |
|---|---|
| `Window` | Drag the title bar (moves, kept inside the Screen), drag edges (resizes), double-click the title or click its arrow (collapses), X (closes, when `closable`), scroll with the wheel. Clicking raises it. |
| `TreeNode`, `CollapsingHeader` | Click toggles open. |
| `TabBar` | Click selects a tab; X closes a `closable` tab; drag reorders when `Reorderable`. |
| `Checkbox`, `RadioButton`, `Selectable`, `MenuItem` (with `checked`) | Click toggles; a `RadioButton` click clears the other `RadioButton`s with the same parent. |
| Sliders, drags | Drag; Ctrl+click types a value. |
| Inputs | Click focuses; typing, the editing keys and the clipboard keys work (section 6). |
| `Combo`, `Menu` | Click opens the list or menu; a choice closes it. |
| `ColorEdit*`, `ColorPicker*` | ImGui's own pickers. |
| `Table` | Resizing, reordering and sorting columns as its `flags` allow; scrolling. |
| `Button`, `SmallButton`, `ArrowButton`, `ImageButton`, `TextLink`, `MenuItem` | Click reports a pick (below); a widget with `opens` opens its popup. `TextLink` never opens a URL. |

- **Clicks and picks.** Hovering a widget sets `hovered()` to a `view_pick`
  (`element` path, `type`, `label`, and `values`: its current value) and a
  primary click on it is returned by `take_click()`, so hosts get `hover` /
  `click` events from widgets as from plots. nymph draws no tooltip of its
  own for widget picks (only the `tooltip` field's). `takes_wheel()` is
  true over every window that scrolls.
- **Coexistence with figures.** Over a window ImGui sends input to the
  window; elsewhere the existing figure interaction applies. A window
  dragged over a plot is above it.
- **Idle.** Frames still run only after input (viewer DESIGN goal 4).
  `io.ConfigInputTextCursorBlink` is off, so an active text field needs no
  timer; hover highlights fade within the existing settling frames.

### Saved view

The state the user changes is view state (goal 3) and is saved in the
`view` key by element path (viewer DESIGN section 3, "Saved view"), only
for touched elements, and applied like limits (`Always` while settling,
in PNGs too), so a PNG saved from the viewer shows the interface as the
user left it:

```yaml
view:
  root/children/0: {x: 40, y: 30}
  root/children/0/children/0: {open: false}
  root/children/1/children/1: {value: 1.25}
  root/children/1/children/0: {value: "Moon"}
```

| Element | View kind | Entry |
|---|---|---|
| `Window` | `ui_window` | `x`, `y` (moved), `width`, `height` (resized by its edges or grip), `collapsed`, `closed`, `scroll`: only what the user changed |
| `Child` | `ui_scroll` | `scroll` |
| `TreeNode`, `CollapsingHeader`, `Menu`, `Popup`, `PopupModal` | `ui_open` | `open` |
| `Selectable`, `TabItem` | `ui_selected` | `selected`; a tab also `closed` |
| `Combo`, `ListBox` | `ui_index` | `selected` (an index) |
| `Checkbox` | `ui_flag` | `value` |
| `RadioButton` | `ui_active` | `active` (a click saves the siblings it cleared too) |
| `MenuItem` | `ui_checked` | `checked` |
| Sliders, drags, numeric inputs | `ui_number` | `value`: a number or a list (`SliderAngle` in degrees), six significant digits |
| Text inputs | `ui_text` | `value` |
| Colour editors and pickers | `ui_color` | `value`, `"#RRGGBB"` or `"#RRGGBBAA"` |

Not saved: the order of windows the user raised, a table's column widths
and sort order, and which menu or popup a click opened only for a moment
(a menu closes on the next click anyway). A window moved or resized saves
only those keys, so an auto-fitted window that was only moved keeps
fitting its content.

A `setSource()` keeps a widget's live state while its path and type are
unchanged and the source value it started from is the same; when the
agent changes the source value, the new value wins. `resetView()` returns
every widget to the source. Unknown paths are ignored, malformed entries
are positioned errors (viewer DESIGN rules).

## 5. Key Abstractions

```
 Screen ─┬─ Window ──── widgets / containers ──── figures in flow
         ├─ Window        │                         (layout engine at the cursor)
         └─ MainMenuBar   ▼
                 draw_context: ui_state (path → value), limits_cond(), picks, targets
                          ▲                          │
          live_view: persistent ui_state, pointer / key / text input, saved view
```

- **Several slots per class** (`src/schema/element_class`). `slot()`
  becomes `slots()` (a short list, the first being the primary, used where
  code asks "is this a figure element"); `accepts_slot()` and the
  registry's `names_accepting()` check any of them. `bind()` accepts a
  child if its parent accepts any of its slots and records which one
  matched on the `node` (`node::placed_as`), so `draw()` knows whether a
  `Separator` is in a layout or a window and a `Window` whether the layout
  or its own `x` / `y` places it. `nymph schema` lists every slot.
- **`ui_state`** (`src/elements/ui/ui_state.hpp`): per element path, the
  values ImGui edits through pointers: `bool`, up to four `float` / `int`,
  a colour, a text buffer (grown as typed, capped at 64 KiB), an index; and
  the `source_value` it was initialised from and a `touched` flag. Owned by
  the `live_view` across frames, by the frame driver for one PNG.
  `draw_context::ui_value<T>(node)` returns the slot, creating it from the
  node's field on first use. It is the one place widgets read or write
  state, so the saved view and `setSource()` rules are applied once.
- **`ui_element_class`** (`src/elements/ui/ui_element.hpp`): base of every
  widget: adds the common fields, wraps `draw()` in `disabled`, item width,
  tooltip, `opens`, the pick offer (`GetItemRect*` after the item) and
  the editor target record. A widget class implements `draw_item()` only;
  families that differ only by component count or type (`SliderFloat`..
  `SliderInt4`, `DragFloat`.., `InputFloat`..) are one template
  registered several times.
- **`screen_class` and `window_class`** (`src/elements/ui/screen.cpp`):
  Screen geometry, style pushes, window placement and clamping, the
  `###path` names, the order of windows and popups, the modal dim.
- **Figures in the flow** (`ui::draw_children()` / `draw_figure_in_flow()`,
  `src/elements/ui/ui_common`): every UI container draws its children
  through `ui::draw_children()`, which places a `figure` child by the
  "Figures in windows" rule (measure at the available width, arrange at
  the cursor, draw, `Dummy` of its size) and draws widgets as they are.
  A positive hint is the size; otherwise a figure is never smaller than
  its natural size, so narrowing a window in a live view scrolls it
  rather than making a layout overflow.
- **Placeholder catalog** (`src/placeholders/`): `placeholder_names()`
  (sorted, for the enumeration), `find_placeholder()` and
  `draw_placeholder(draw list, image, box, style)`. The SVG bytes come from
  the generated `embedded_placeholders()` table; each is parsed once per
  process (`std::call_once`) by `parse_svg()` into shapes (colour role,
  fill or stroke, width, subpaths of lines and cubic curves in viewBox
  units).
- **Keyboard and text input** (`live_view`): `key_event` (key, down,
  modifiers) and typed text (UTF-32), forwarded to ImGui's `AddKeyEvent` /
  `AddInputCharacter`, and `wants_keyboard()` (ImGui's `WantTextInput`
  after the last frame: a text field, or a slider typed into with
  Ctrl+click, is active) so the host knows whether a key is the figure's
  or its own shortcut. Not `WantCaptureKeyboard`: it is also true while a
  modal is open, and a modal shown from the source would then keep `Q`
  from closing `nymph view`. Clipboard through `PlatformIO` hooks the host
  sets (`set_clipboard_hooks`); without them ImGui's in-context fallback
  applies.

## 6. Public API Contract

Source format: the elements of section 3, documented in
`docs/format.md` ("User interfaces") and generated into
`docs/elements.md`; the `view` entries of section 4.

`live_view` additions (`src/view/live_view.hpp`):

```cpp
struct key_event {                 // ImGuiKey values, so hosts map once
  ImGuiKey key; bool down;
  bool ctrl, shift, alt, super;
};
void input_key(const key_event& event);
void input_text(std::u32string_view text);          // typed characters
bool wants_keyboard() const;                        // keys belong to the figure now
void set_clipboard_hooks(std::function<void(std::string_view)> set,
                         std::function<std::string()> get);
```

`view_pick` gains nothing: widget picks use its existing fields (`type`
`"Button"`, `label`, `values` `{"value", "0.8"}`).

`nymph view`: keys go to the editor panel while it has the keyboard, then
to the figure while `wants_keyboard()` (`route_key()` in
`src/window/view_command`), so typing `q` in a text field does not close
the window; otherwise the viewer's keys (`Esc`, `Q`, `R`, `S`, `Y`, `E`)
apply as today, and key releases also reach the figure. The window's key
set of the editor (editing keys, Ctrl+A/C/V/X/Y/Z, typed text) is enough;
no new key is mapped. The native window has no system clipboard yet
(editor DESIGN section 8), so copy and paste work within the view.

Web viewer (`nymph-viewer.js`): the canvas is focusable already (for the
menu key); while a text field of the figure has the keyboard
(`wants_keyboard()`), `keydown` goes to the figure (`nymph_viewer_key()`
with the DOM key name, mapped to ImGui's keys in C++, and printable keys
as text) with `preventDefault()`; every `keyup` reaches it; a `paste`
event is typed into the field (ImGui reads its clipboard synchronously,
the browser's is not), and copying also writes the page clipboard where
allowed. The `click` and `hover` events carry widget picks; the `view`
event fires when `live_view::ui_revision()` changes. No new API method.

Library: `render_png()`, `check_source()` and the CLI are unchanged in
signature; `nymph check` reports UI errors like any other.

## 7. Design Decisions

- **ImGui names over a nymph vocabulary.** The request asks for "any of
  the UI elements in the ImGui library", and agents have read a great deal
  of ImGui code; `SliderFloat3` says exactly what it draws. Rejected: a
  smaller set of generic elements (`Slider` with `components` and `type`
  fields), which would be fewer classes but a vocabulary every agent must
  learn from the schema. The class count (about 90) costs nothing at run
  time; templates keep the code small.
- **`Screen` holding positioned windows, not windows in layouts.** A
  mockup of several windows is about overlap, placement and z-order, which
  the measure / arrange layouts cannot express, while ImGui windows are
  positioned that way already. `Screen` is a figure element, so a mockup
  can sit next to a caption or a chart in a layout. A single `Window` is
  also a figure element, the shortest source for the common
  one-window mockup.
- **Several slots per class, not new names for `Separator` and
  `SeparatorText`.** They exist as figure elements and are ImGui widgets
  too; a second name for each (`UiSeparator`) would break goal 1. The
  schema change is small (a list instead of one string), and `Window`
  needs the same thing.
- **Real ImGui windows nested in the figure frame, not child windows.**
  `BeginChild` regions cannot overlap, be raised, moved by their title bar
  or collapse; real windows do all of it with ImGui's code. They are not
  clipped by the Screen, so nymph keeps them inside it (clamping in live
  views, an error for explicit geometry in sources).
- **User interaction is view state, not source edits.** Ticking a box in
  the viewer is like zooming a plot: the author's source stays, `R` /
  `resetView()` goes back to it, the `view` key saves what the user left,
  and the MCP App already passes the `view` key back to the agent (spec/06
  section on `ui/update-model-context`), so the agent learns what the user
  expanded or typed. Editing the source stays the editor's job (editor
  DESIGN). Rejected: writing widget changes into the source's fields,
  which would lose the author's initial state.
- **Values in a nymph store, open / selected / window state in ImGui.**
  ImGui already keeps tree openness, tab selection and window geometry in
  persistent contexts and offers `Cond_Once` setters; duplicating them
  would fight ImGui. Values have no ImGui-side storage, so nymph owns
  them. The saved view reads both back after each frame.
- **Figures in windows by ImGui's flow, not a nested layout pass of the
  whole window.** Widgets have no measure / arrange hooks and do not need
  them: ImGui lays them out. Only figure children need a rectangle, and the
  cursor plus the available region is enough to give one.
- **Popups and menus shown by a field, not only by interaction.** A PNG has
  no clicks, and a mockup of a menu or a dialog is a common request
  (`open: true`). The same field is the initial state in a live view.
- **Transitions as Screen children naming window `id`s.** (Named
  `WindowTransition` because `Transition` is the state diagrams' element,
  drawn by ImUml.) A transition
  belongs to neither window, so it is a sibling of both, like a `FlowLink`
  in a `Flowchart`; `id`s rather than paths keep it readable and stable
  when windows are reordered. Drawn after the windows from their
  rectangles of the same frame, arrows follow windows the user moves, and
  stay below them, where nothing is hidden that a window does not already
  hide. Rejected: running the windows through the diagram layout of
  spec/03 (windows are positioned by the author, and a mockup's layout is
  its message), and routing around obstacles (straight arrows between
  windows placed with gaps read well; routing is a later extension).
- **IDs from element paths, not from labels.** Asking authors for unique
  labels (or `##` suffixes) would make every agent-written mockup one
  repeated `OK` away from an ImGui assertion, and an ID conflict shows
  nothing wrong in a PNG; it breaks only later, in a live view. Path-based
  IDs cost one `PushID` per element, which nymph already does. Rejected:
  a bind-time "duplicate label" error (pushes the problem onto the
  author for no gain) and hashing labels with a counter (IDs, and so
  state, would shift when an earlier sibling's label changed).
- **Built-in vector placeholders, not images in the source.** Embedding
  image bytes in the YAML (base64) would make sources large, hard for an
  agent to write or read back, and unsafe to decode (an image decoder for
  untrusted input); referencing files breaks "the source never names a
  file" and the PNG round trip. A fixed catalog keeps the source a few
  words (`image: person`) and the PNG self-contained. SVG drawn through
  `ImDrawList` was chosen over embedded bitmaps (blurry when scaled, a
  texture path in three presenters, colours fixed per theme) and over a
  full SVG rasterizer such as nanosvg's (float coverage would break
  deterministic bytes, and it is a second rasterizer). The files are
  nymph's own and keep to a subset, so a parser of about 500 lines reads
  them; nanosvg's parser was the first plan, but a submodule for that is
  more to build and pin than the code it saves, and the subset rules
  (no arcs, three colours, simple polygons) are checked by nymph anyway.
- **No actions.** A button that runs code would need a scripting
  language in the source and breaks "the source never names a file or
  runs anything". `opens` covers the one navigation a mockup usually
  needs; everything else is reported to the host, which can act on it.
- **Cursor blink off.** Keeps live views idle (viewer goal 4) and makes a
  focused text field look the same in every frame, so a saved PNG matches
  the screen.

## 8. Constraints and Invariants

- A source without UI elements: the same PNG bytes and the same live-view
  draw data as before (tested on every example).
- Static parity: a live view of a UI source, after its settling frames
  with no input, gives the same draw data as `render_figure()`.
- `ui_state` is read and written only through `draw_context`; one state
  per element path; a state whose element is gone is dropped at the next
  `set_source()`.
- No source text reaches printf unchecked; every `format` field is
  checked at bind time.
- Windows never leave their Screen: explicit geometry is checked at bind
  time, live moves and resizes are clamped each frame.
- Limits: at most `kMaxUiElements` (10,000) UI elements per figure, a text
  buffer at most 64 KiB, `items` and `cells` lists bounded by the data
  limits; all ordinary `nymph::error`s.
- No two items drawn in one frame have the same ImGui ID, whatever the
  labels (section 3, "Unique IDs"); the UI tests check it with
  `set_check_ids(true)`. Window names are `title###path`; popup IDs come
  from paths; nothing nymph stores is keyed by label.
- Placeholders are drawn only with `ImDrawList` geometry; no texture is
  created for them. Every embedded placeholder passes the authoring
  rules (tested).
- Single thread, as `live_view` (viewer DESIGN section 4).
- Platforms: no new dependency, no platform code outside the existing
  window and web layers. ImGui keys are mapped in `window_win32.cpp`,
  `window_x11.cpp` (both already report keys and text, editor DESIGN
  section 6) and `nymph-viewer.js`.
- Code style: `src/elements/ui/` follows nymph conventions (`snake_case`,
  `bdg::nymph`); type names in the source are ImGui's (goal 1).

## 9. Integration Boundaries

Depends on: the schema, binding and layout engine (core DESIGN), `live_view`,
picks and the saved view (viewer DESIGN), the editor's key and text input
and target recording (editor DESIGN), ImGui 1.92.9 (tables, tab bars,
`BeginDisabled`, `SetItemTooltip`, `TextLink`, `PathFillConcave`,
`OpenPopup(ImGuiID)`, `ImDrawList` paths for arrows; `BeginPopupEx` and some window and tab state from
`imgui_internal.h`, as ImPlot state is today). No new dependency.

Depended on by: `nymph view`, the web viewer, the Chrome extension and the
MCP App, which get UI sources with no change of their own beyond keyboard
input; the agent guide and the Claude skill text, which gain a "mock a
user interface" recipe.

The editor (`--edit`): widgets record their item rectangle as the element
target and their `label` / `text` as text targets; in edit mode UI windows
take no input (`ImGuiWindowFlags_NoInputs` is added), so a click selects
instead of pressing a button.

Later (not in this project): column widths and sort order in the saved
view; a `hovered` / `active` state field to show a pressed button in a PNG;
IME composition in the web viewer; keyboard navigation; system clipboard
on X11 (editor DESIGN section 8).

Risks, checked first in the plan: whether three frames settle auto-fitted
windows, tab bars and tables (step 2); windows begun inside the open
figure window, their order and clamping, and the unique-ID check (step 2);
placeholder SVGs drawn through `ImDrawList` without holes (step 5); a
modal's dim limited to the Screen (step 8).
