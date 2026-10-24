# The web viewer

The web viewer shows a nymph chart live in a web page, from its YAML
source or from a nymph PNG: drag to pan, wheel or pinch to zoom, hover or
click for values. It is the nymph library compiled to WebAssembly and
drawn with WebGL2 (on the CPU when WebGL2 is missing), behind a small
JavaScript API and a `<nymph-viewer>` element, for markdown viewers,
chatbots, notebooks and documentation sites. Design:
[spec/04_web_viewer/DESIGN.md](../spec/04_web_viewer/DESIGN.md); build:
[building.md](building.md#web-viewer).

## Files to serve

From `build-web/web/`, served together from one folder:

| File | What |
|------|------|
| `nymph-viewer.js` | The API (an ES module). Import this. |
| `nymph-viewer-core.js`, `nymph-viewer-core.wasm` | The module (about 630 KB gzip), loaded on the first viewer. |
| `fonts/` | The font files and their licences, fetched only when a chart names a font. |
| `nymph-viewer.single.js` | Instead of the three files above: everything in one file (about 690 KB gzip). Fonts still come from `fonts/` next to it, or `fontBaseUrl`. |
| `editor/` | Optional: the [editor module](#the-editor-module), loaded only by pages that edit. |

The page's Content Security Policy must allow `'wasm-unsafe-eval'` (or
`'unsafe-eval'`) in `script-src`. The module reads no file and makes no
request itself: the API fetches `src` URLs and fonts.

## Live demo

The [web app](web-app.md) at <https://binary-dice-games.github.io/nymph/>
opens, views and edits charts; every example, live next to its PNG, is at
<https://binary-dice-games.github.io/nymph/demo/>. The `pages` workflow
(`.github/workflows/pages.yml`) rebuilds, tests and publishes the site on
each push to `main`; enable it once under Settings > Pages > Source: GitHub
Actions. To host your own copy, serve `build-web/web/site/`. The viewer's
files keep their paths there (`nymph-viewer.js`, `fonts/`, ...).

## Embedding

The element, with a URL (`.yaml` or a nymph `.png`) or with the YAML as
its text:

```html
<script type="module" src="/nymph/nymph-viewer.js"></script>

<nymph-viewer src="/charts/revenue.yaml"></nymph-viewer>
<nymph-viewer src="/charts/revenue.png" fit="width" theme="auto"></nymph-viewer>
<nymph-viewer src="/charts/pipeline.mmd"></nymph-viewer>  <!-- mermaid, converted -->
<nymph-viewer>
  root:
    type: Plot
    title: Revenue
    children:
      - {type: PlotLine, label: revenue, ys: [12, 15, 11, 18]}
</nymph-viewer>
```

A markdown renderer can turn a fenced block whose language is `nymph`
into the last form (escape `<` and `&` in the text). Attributes: `src`,
`fit` (`source`: laid out at the source size, shrunk to a narrower box;
`width`: laid out at the box width; `fill`: at the box size; `scale`:
laid out at the source size and scaled to the box width, up or down), `theme` (`source`, `auto` (light or dark as the system), or any
[chart theme](themes.md)), `tooltips` (`false` turns nymph's own off), `menu`
(`false` turns the right-click menu off), `reset-button` (`false` hides
the reset-view button), `touch-scroll` (vertical swipes
on the chart scroll the page). Events,
bubbling: `nymph-load` (shown; `detail` is `info()`), `nymph-hover`,
`nymph-click`, `nymph-view`, `nymph-error` (the error is also shown as
text in the element). Its `viewer` property is the `NymphViewer`.

The API:

```js
import { createViewer } from '/nymph/nymph-viewer.js';

const viewer = await createViewer(document.getElementById('chart'), { source: yamlText, theme: 'auto' });
viewer.on('hover', (pick) => console.log(pick?.label, pick?.values));
setInterval(() => viewer.setData({ revenue: latestValues() }), 250);
```

| Call | What |
|------|------|
| `createViewer(container, options)` | A viewer in `container` (a canvas is added to it). Options: `source` or `png`, `fit`, `theme`, `tooltips`, `menu` (`false` turns the right-click menu off), `resetButton` (`false` hides the reset-view button), `fileName` (for saving; default from `src`, or `chart`), `touchScroll` (vertical swipes on the chart scroll the page; default `false`), `rasterizer` (`auto`, `webgl`, `software`), and for the first viewer of the page `wasmUrl`, `fontBaseUrl`, `fetchFont(name)`. |
| `setSource(yaml, {resetView})` | Shows a new source; the zoom of elements that did not change is kept unless `resetView`. A [mermaid](mermaid.md) diagram is converted to nymph YAML first: `source()`, `toYaml()` and the saved files are that YAML, and `info()` adds `importedFrom: "mermaid"`, `mermaidKind` and `warnings`. |
| `setData(tables)` | Replaces the source's `data` (an object, or YAML / JSON text); the view is kept. |
| `setPng(bytes)` | Shows the source a nymph PNG carries; resolves `false` for a PNG without one (shown as a plain image). |
| `setTheme(theme)`, `resetView()`, `resize(w, h, ratio)` | [Chart theme](themes.md) override (`''` for the source's; returns a promise, as the theme may fetch a font), back to the source's view, box size (the element and `createViewer` follow their container themselves). |
| `themes()` | The chart themes: `[{id, name, description, base, dark, background, text_color, palette}]`, for a picker. |
| `on(event, callback)` | `hover` (pick or `null`), `click` (pick), `view` (every plot, Gantt chart, 3D plot and diagram after a pan, zoom, rotation or legend click settles: ranges, rotation, zoom, hidden series), `error` (an update failed). Returns a function that removes the listener. |
| `rendered()` | Resolves when every update so far is on screen. |
| `info()`, `state()`, `source()` | Rasterizer, description and size; geometry, hovered pick and plot views; the source on screen. |
| `toYaml()` | The chart's YAML as it is on screen: the source with the current data and the user's zoom, pan, rotation and hidden series saved in its [`view` key](format.md#view). The source as written when the user changed nothing. |
| `toPng()` | The image on screen as a nymph PNG, `toYaml()` embedded. |
| `save('png' \| 'yaml')` | Downloads `toPng()` or `toYaml()` (what the menu's items do). |
| `destroy()` | Removes the canvas and frees the viewer. |
| `importMermaid(text)` (module export) | Converts a mermaid diagram without drawing it: `{source, kind, warnings}`; rejects with a `NymphError` at the mermaid line and column. |

Updates resolve once the chart is shown and reject with a `NymphError`
(`line`, `column`, `kind`, `message`: the same text as the `nymph` CLI)
while the previous chart stays. They apply in call order; several
`setData()` calls waiting behind a font fetch collapse into the last.

A pick is `{element, type, label, index, values}`: `element` is the
element's path in the source (`root/children/1`), `values` its values as
the axes show them (`{x: "2026-03-02", y: "7.25 kg"}`).

## Interaction

| Element | Pointer | Hover / click |
|---------|---------|---------------|
| `Plot` | Drag pans, wheel zooms (over an axis: that axis), right-drag box-zooms, double-click fits, legend click hides a series. | Nearest point within 8 px of lines, scatter, stairs, stems, shaded, digital, error bars; the bar, histogram bin, heatmap or 2D-histogram cell, pie slice under the pointer. |
| `Plot3D` | Drag rotates, wheel zooms, double-click resets. | None yet. |
| `GanttChart` | Drag and wheel pan and zoom time; a legend click hides a section's rows. | The task or milestone: start, end, section. |
| `SankeyDiagram` | None. | The node (total) or flow (from, to, value). |
| UML diagrams, `Flowchart` | Drag pans, wheel zooms around the pointer, double-click fits. | The node or edge (not yet in sequence diagrams): id, name, label. |
| User interfaces ([format](format.md#user-interfaces)) | The widgets work: click a tree node or header to open it, a tab to show it, drag sliders, tick boxes, choose in combos, type in text fields (Ctrl+click a slider to type a value), open menus and popups; drag a window by its title, resize it by its edges, collapse or close it. Windows stay on their Screen. | The widget: type, label and its current value; a click on a button, menu item or link is a `click`. A `WindowTransition`'s label too. |

What the user changes of a user interface is view state, like a zoom:
`toYaml()` saves it in the `view` key and `view` events fire for it. While
a text field of the chart has the keyboard, keys go to it (the canvas
takes focus with the click; `preventDefault()` keeps them from the page),
and a paste is typed into the field; otherwise keys stay with the page.

A plot with `flags: NoInputs` does not move. The wheel scrolls the page
except over a plot or diagram that takes input. On touch screens a gesture
that starts on the chart goes to the chart only (the page neither scrolls
nor zooms): one finger pans, two fingers zoom. With `touchScroll: true`
(attribute `touch-scroll`) a vertical swipe scrolls the page instead.
Source limits (`x_min`, ...) are where a plot starts; double-click,
`resetView()` or the reset button goes back. The reset button (four
corners, in the chart's bottom-right corner) shows once the user pans,
zooms, rotates or hides a series, and resets every plot and diagram. New data outside the visible range is not
followed unless the axis has the `AutoFit` flag.

## In a window

`nymph view chart.yaml` (or a nymph PNG) shows the same viewer, the same
`live_view` code, in a native window: the interaction above, tooltips, and
a resizable window the chart follows. Keys: `R` resets the view, `S` / `Y`
save the image / YAML on screen next to the input as `*.view.png` /
`*.view.yaml` (with the [`view` key](format.md#view)), `Esc` or `Q`
closes; while a text field of a user interface has the keyboard, keys go
to the field instead (`Esc` leaves it). See [cli.md](cli.md).

## The editor module

`editor/nymph-editor.js` puts the editor of `nymph view --edit` in a page:
the same panel, toolbar, tree and data editing, and the source pane,
drawn with WebGL2 into one canvas that fills its container. It is a second
WebAssembly module (about 1.2 MB gzip) so pages that only view stay small;
it imports `../nymph-viewer.js` for its font loader, so serve it in an
`editor/` folder next to the viewer's files. Design:
[spec/15_web_app/DESIGN.md](../spec/15_web_app/DESIGN.md) section 5.

```js
import { createEditor } from './editor/nymph-editor.js';
const editor = await createEditor(document.getElementById('box'), { source: yamlText, sourcePane: true });
editor.on('change', (c) => console.log(c.label, c.origin, editor.source()));
editor.on('save', () => save(editor.toYaml()));
```

| Method / event | What |
|----------------|------|
| `createEditor(container, {source \| png, fileName, theme, appearance, panel, sourcePane, repair})` | The editor, or a `NymphError` (no WebGL2, a source that does not load; with `repair: true` such a source opens with only the source pane editing). |
| `open(yamlOrPng)`, `setSource(yaml, {asChange, label, origin})` | A new document; with `asChange`, one undo step. |
| `source()`, `toYaml()`, `toPng()`, `paneText()` | The YAML, with the view on screen, as a PNG; the pane's text. |
| `modified()`, `markSaved()`, `version()`, `state()` | Unsaved changes, the change counter, labels of undo / redo, the selection. |
| `undo()`, `redo()`, `resetView()`, `setTheme(t)`, `themes()`, `showPanel(on)`, `showSource(on, line)` | `setTheme` shows a chart theme without changing the source (a promise); the panel's **Theme** button writes `image.theme`. |
| `setAppearance({dark, accent, accentDark})` | The panel's look, to match the page: light or dark, accent colours as `#rrggbb` (nymph's green when left out). Without it the panel is dark only for a chart in the `dark` theme. |
| `tool(name, args, {origin})`, `toolDefinitions()` | The editor tools of `nymph mcp` (`editor_state`, `editor_set_fields`, `editor_add_element`, ...), run in the page; changes are attributed to `origin`. |
| `beginGroup(label, origin)`, `endGroup()` | Several changes as one undo step. |
| `importCsv(table, csv)`, `targetRect(path, field)`, `focus()`, `destroy()` | |
| `change` {label, origin, version, modified}, `selection`, `save`, `csv-import` {table}, `csv-export` {table, fileName, csv}, `error` | Events: the page saves, picks and writes files; the module does no I/O. |

### Editing in the window

`nymph view --edit chart.yaml` (or the `E` key in any `nymph view`) docks
an editor panel to the right of the figure; `E` hides it again. Drag the
divider between them to make the panel wider or narrower (double-click it
for the default width).

- **Source.** The **Source** tab button (or `Ctrl+U`) shows the YAML between
  the figure and the panel, highlighted, with line numbers, folding and
  find / replace (`Ctrl+F`, `Ctrl+H`). Typing applies after a short pause
  as one undo step; text that does not load is underlined at the error and
  the rest of the panel waits (agents too) until it loads again. Selecting
  an element moves the cursor to its line, and moving the cursor selects
  the element there. With `--edit`, a file that does not load opens with
  the source at the error instead of failing.

- **Click to select.** Click a title, an axis label or the legend of a plot, a
  `Label`, a node, group or link text of a diagram: the thing is outlined and
  the panel shows its field (a diagram node's `text`, a plot's `title`, ...)
  first, then **All fields** of its element (a legend entry selects its
  series, or the pie chart its slice belongs to; while editing, legend
  entries select instead of hiding series), each with a widget for its type:
  text boxes, colour pickers (empty means automatic), choices, flags,
  numbers. A click on an empty part selects the element (the plot, the
  diagram); the **Elements** list reaches everything else (a series, a plot
  without a title yet, `legend_location`). Dragging still pans and the wheel
  still zooms.
- **Toolbar.** A floating toolbar sits over the figure next to the
  selection, with the fields changed most often, each shown when the element
  has it: **shape** (a grid of shape pictures, in tabs by category for the
  shape library), **fill**, **line** and **text colour** (a palette of 18
  colours, **Automatic** and **Custom colour**), **font**, **text size**
  (`auto` inherits it), the **colour scheme** of a plot (also offered on a
  pie chart or heatmap, whose colours are their plot's) and, for lines,
  links and series, thickness, line style, arrow heads, **marker** and
  marker size. Then **copy style** (the next element you click takes the
  colours, text style and, between elements of one type, shape and line
  style), **clear style** (removes the colours and text style),
  **Duplicate** and **Delete**.
- **Numbers.** A number field is a box: click it to type a value (Enter,
  Tab or a click elsewhere accepts it, `Esc` cancels), or drag sideways over
  it to change the value (Shift: ten times finer, Ctrl: ten times coarser).
  A drag is one undo step.
- **The whole figure.** Clicking the background (the margin around the figure,
  away from every element) or choosing **Image** in the list selects the
  `image:` options: `width`, `height`, `scale`, `theme`, `padding`, and the
  text style of the whole figure. A new `width` or `height` resizes the
  window to that layout; `scale` only changes a saved PNG.
- **Changes are edits of the YAML.** Each change is written into the source
  (comments and layout of the file stay), checked like any source, and the
  figure redraws at once. A change that would make the source invalid is
  refused with the error in red, and nothing changes. **reset** next to a
  field removes it from the source (back to its default).
- **Undo / Redo** (buttons, or `Ctrl+Z` / `Ctrl+Y` when no text box has the
  keyboard). **Save** overwrites the file the view was opened on, after a
  confirmation: a PNG input is redrawn and replaced as a PNG (with the edited
  YAML inside), a YAML input is replaced as YAML; the zoom and pan on screen
  go in its `view` key. Closing the window with unsaved changes asks whether
  to save them, discard them or stay. (The `S` / `Y` keys still write copies,
  `*.view.png` / `*.view.yaml`, and never touch the input.)
- **Text style.** `font`, `font_size` and `text_color` of a plot, layout or
  diagram apply to all its text; every diagram item (node, link, note, ...)
  has its own, which falls back to the diagram's. A text that comes from the
  data (`text: $names`) is shown but not edited; **replace by this text**
  writes it into the field instead, and **columns** takes it from another
  column.
- **Lists and columns.** A list field (a series' `xs`, `ys`, `values`, ...)
  is a text box holding its value as in the source: `[1, 3, 2]`,
  `$monthly:cost`, or several columns `[$revenue, $cost]` (grouped bars). Its
  **columns** button lists every column of the data: choosing one points the
  series at it. To plot a new column, add it in the table's tab, add the
  series (right-click the plot, **Add child**), then choose the column for
  its `ys` (and `xs`).
- **Adding and removing elements.** Right-click an element, in the figure
  or in the **Elements** list, for its menu: **Add child**, **Insert
  before** / **after** (only the types that can go there: `FlowNode`,
  `FlowLink`, `Note`, ... in a `Flowchart`), **Duplicate** (`Ctrl+D`; ids
  are renamed), **Move up** / **down** (`Alt+Up` / `Alt+Down`) and
  **Delete** (`Del`). A new element starts valid: a fresh id, sample data for
  a series, and a link from the selected node to the next one; when it
  cannot (a link in a diagram with no nodes), a dialog asks for its fields.
  Deleting a node that links or notes name offers to delete them too. On the
  root, **Add chart below** / **to the right** puts the chart and a new one
  in a layout.
- **Data tables.** The tabs over the figure are **Chart** and one per
  `data` table: a table tab is a grid of its cells. Click a cell or a column
  name to change it (Enter moves down, Tab right); right-click a row or
  column header to insert or delete; `Ctrl+V` of several cells (from a
  spreadsheet) pastes them from that cell on. **Import CSV** / **Export
  CSV** read and write a file, **Paste CSV** replaces the table with the
  clipboard. **+** adds a table (with two tables each needs a name:
  `$column` references become `$table:column`). A `$` beside a column name
  lists the elements that use it; **Go to data** on such a field opens it. A
  table keeps its form in the file (CSV text or columns); comments inside
  that table are not kept.
- **AI agents.** While the editor is open, an agent that uses
  [`nymph mcp`](claude.md) (Claude Code, Claude Desktop, Cursor) can work on
  the chart on screen through its `editor_*` tools: read the elements, the
  selection, the tables and an image of the view, and change them. "Connect
  this node to the Purchases node" works on what you selected. Each change
  shows at once and can be undone; the panel says when an agent is
  connected. `--no-mcp` turns this off. Agents reach the editor through a
  socket in a folder only you can read (`$XDG_RUNTIME_DIR/nymph`, else
  `/tmp/nymph-<uid>`; `%LOCALAPPDATA%\nymph\editors` on Windows).
- **Limits.** Copy and paste work inside the panel only on Linux; the CSV
  file dialog is nymph's own (no system dialog).

## Saving and iterating

A right click (without dragging: a right-drag zooms to a box) opens a
menu: **Save as PNG** (the image on screen), **Save as YAML** and **About
nymph viewer**; the menu key or Shift+F10 opens it from the keyboard.
Both files keep the view: the zoom, pan, rotation and hidden series of
the elements the user changed go into the source's
[`view` key](format.md#view), so the chart reopens the same way in the
viewer and the `nymph` CLI draws the same PNG.

An AI agent in a chat can iterate without the CLI: show the chart with
`setSource(yaml)`, let the user explore it, read it back with `toYaml()`
(the user's view included), change the data or the elements, and show it
again. A `setSource()` whose `view` differs from the chart's starts from
that view; otherwise the user's zoom stays. Only what the user changed is
saved: an element the user never touched keeps following its data.

## Rendering

Every viewer of a page draws through one WebGL2 context (browsers allow
only about 16) and copies its frames to its own canvas; frames run only
after input, an update or a resize, and not while the chart is off
screen. Without WebGL2, or after the context is lost, viewers draw on the
CPU. Both look like the PNG the CLI renders (WebGL within the tolerance
of [spec/04_web_viewer/PLAN.md](../spec/04_web_viewer/PLAN.md) step 10).
