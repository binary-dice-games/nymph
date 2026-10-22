# nymph — Editor Workspace: Design

**Status: built** (plan steps 1-11, see [PLAN.md](PLAN.md)). Built and tested
on Linux (unit tests; the window driven on Xvfb through synthetic X events,
`nymph mcp` driven over standard input). The Win32 code (cursor, wake, Alt
keys, Winsock `AF_UNIX`) and macOS were not compiled here: CI is their first
build. Sections below describe what was built.

## 1. Purpose / Scope

Turns `nymph view --edit` (the editor of
[08_view_editor](../08_view_editor/DESIGN.md)) from a field inspector into a
small workspace:

1. **Resizable panel.** The divider between the figure (left) and the editor
   panel (right) can be dragged.
2. **Tree editing.** A right-click on an element, in the element tree or
   in the figure, opens a menu to add, insert, duplicate, move and delete
   elements. "Add" offers only the types the parent accepts (`FlowNode`,
   `FlowLink`, ... in a `Flowchart`). Adding a chart at the root puts it next
   to the existing one.
3. **Data tabs.** The left side gets tabs: "Chart" (the figure) and one tab
   per `data` table, an editable grid with CSV import and export. A `+` tab
   adds a table.
4. **Editor MCP server.** A running editor is an MCP server. An AI agent can
   read its state (the element tree, the tables, the selection, an image of
   the figure as shown) and edit it through tools, so the user can ask for
   "connect this node to the Purchases node". The agent reaches the editor
   through the `nymph mcp` server it already uses
   ([06_claude_integration](../06_claude_integration/DESIGN.md)).

It does not cover: the web viewer or the Chrome extension (the editor stays
native); drag-and-drop reordering in the tree or in the figure; formulas or
types in the data grid (cells stay text, as in the source); the MCP server
over HTTP or for remote machines (section 10); several open files in one
window.

Files owned or changed: `src/edit/` (new `tree_edit`, `data_edit`,
`element_starter`, `csv_file`, `table_grid`, `tree_menu`, `workspace_layout`,
and `yaml_text` (the text-position helpers `source_edit` had, now shared);
`source_edit`, `editor_model` and `inspector` extended),
`src/window/editor_panel.*`, `src/window/viewer_window.cpp`,
`src/window/window_host.hpp` and the two platform files (cursor shape,
focus, cross-thread wake, the D key), `src/view/live_view.*`
(`set_source_moved`), new `src/ipc/` (local socket, discovery files), new
`src/mcp/editor_tools.*`, `editor_server.*` and `editor_bridge.*`,
`src/mcp/mcp_session.cpp` and `server_options.hpp` (the editor's own
session, the bridge in `nymph mcp`), `src/schema/registry.*`
(`classes_taken_by`), `src/window/view_command.*` (`--no-mcp`),
`docs/viewer.md`, `docs/cli.md`, `docs/claude.md`, `docs/agent-guide.md`,
`claude/skill/`.

## 2. Design Goals

1. **The source stays the only model** (08 goal 1). Structural edits, cell
   edits and agent edits are all text patches to the YAML, validated by
   binding, undoable, and refused whole when invalid.
2. **The schema decides what can be added where.** The add menu, the MCP
   tools and validation read the registry's slots (`accepts()`,
   `fills_slot()`); no per-element menu code.
3. **The agent and the user edit the same thing.** Agent edits go through
   the same `editor_model` calls as the panel, on the UI thread, appear at
   once, and are one undo step each. The agent sees what the user sees,
   including the selection.
4. **Paths stay correct after structure changes.** Element paths are the
   identity of selections, view state and the `view` key; an insert, move,
   delete or wrap renames them all consistently.
5. **No new link-time dependency for the window, and only the user can
   reach the editor.** The MCP endpoint is a local socket in a user-only
   directory, not a network port.
6. **Outside `--edit`, nothing changes.** No tabs, no socket, same frames.

## 3. Key Abstractions

```
                 ┌──────────── nymph view --edit (UI thread) ───────────────┐
 agent ─stdio─► nymph mcp ─local socket─► editor_server ─► ui_queue ─┐       │
                 │   (editor_bridge)        (thread per conn.)       ▼       │
                 │                         editor_tools ─► editor_model ◄── tree_menu / table_grid / inspector
                 │                                              │ source           (panel ImGui context)
                 │                                              ▼
                 │                              live_view (figure box, rename_paths)
                 └───────────────────────────────────────────────────────────┘
```

- **`tree_edit`** (`src/edit/tree_edit.hpp`): pure functions on source text,
  like `source_edit`: `insert_child(source, parent, index, element_yaml)`,
  `remove_element(source, path)`, `move_element(source, path, new_index)`, `reparent_element(source, path, new_parent, index)` (drag and drop in the tree: block text re-indented when both lists are blocks, else one flow value),
  `wrap_root(source, layout_type)`. Each returns the new source and a
  **`path_map`**: for every element path that changed, its new path, or
  none when it was removed. The splice keeps the list's style: a flow item
  (`- {type: X, ...}`) when its siblings are flow, a block item otherwise;
  a missing `children:` key is added. `rename_view_paths(source, map)`
  rewrites the keys of the top-level `view` mapping and drops entries of
  removed elements.
- **`element_starter`** (`src/edit/element_starter.hpp`): the YAML of a new
  element of a type, valid where it goes. One table, `starter_fields(type,
  diagram)`, holds the extra fields of the ~60 types that do not bind bare,
  as the inside of a flow mapping: `PlotLine` `ys: [1, 3, 2]`, `FlowLink`
  `from: ?, to: ?`, `GanttChart` a first task, `DragFloat3` `value: [0, 0,
  0]`; a key `Diagram/Type` overrides one type in one diagram
  (`SequenceDiagram/Note`: `over: [?]`). A field named `id` gets a fresh
  value (`node1`, `node2`, ...). Each `?` takes the next of the candidate
  ids (`editor_model::candidate_ids()`: the selected node first, then the
  diagram's ids in document order); with none left it is written `""` and
  named in `unfilled`, and the New element dialog asks (section 5).
  `test_starters` inserts every registered type into every parent that
  takes it and checks it binds.
- **References** (`find_references(source, path)` in `tree_edit`): the
  elements of the same diagram (nearest ancestor that is a diagram, else
  the whole tree) with a `text` or `text_array` field whose value, or one
  of whose items, equals the removed element's `id`. `FlowLink.from`,
  `Note.attach` and their kin are found with no per-diagram code. Delete
  offers "Delete with N elements that refer to it".
- **`data_edit`** (`src/edit/data_edit.hpp`): table edits on source text.
  `write_table(table, form)` serializes one table in its form (`csv`: a
  `|` block, cells quoted only when needed; `columns`: a mapping of flow
  lists), cells verbatim. `set_table(source, name, table)`,
  `add_table(source, name, table)`, `remove_table`, `rename_table`,
  `rename_column`. Renames rewrite the column references (`$col`,
  `$table:col`, `$2` is left alone) in every element. Adding a second table
  to a single unnamed table converts `data` to named tables: the existing
  table takes a name the user gives (default `table1`) and every bare
  `$col` becomes `$table1:col`.
- **`csv_file`** (`src/edit/csv_file.hpp`): `read_csv_file(path)` (via
  `parse_csv`), `write_csv_file(path, table)` (RFC 4180, CRLF-free, UTF-8,
  written to a temporary file and renamed, as `output_path` in 06).
- **`editor_model`** (extended): structural and data operations with the
  same contract as `commit()`: `add_element(parent, index, type,
  fields)`, `remove_element(path, with_references)`, `move_element`,
  `duplicate_element`, `wrap_root(layout)`, `set_cell`, `set_table`,
  `add_row`, `remove_rows`, `add_column`, `remove_column`,
  `rename_column`, `add_table`, `remove_table`, `rename_table`,
  `paste_cells`, `set_fields(path, fields)` (several fields of any element
  as one step, for the agent) and `set_source(yaml)` (a whole new source).
  Each returns a `commit_result` with the `path_map`. The model applies the
  map to its selection and to the `view` key; the viewer passes it to
  `live_view::set_source_moved()`. `version()` counts accepted changes, undo
  and redo included (section 6). `tables()` reads the current tables.
- **List fields** (`inspector`, added after the plan): `xs`, `ys`,
  `values` and other list fields, read-only in 08, get a one-line YAML text
  box (`[1, 3, 2]`, `$monthly:cost`, `[$a, $b]`), committed when left, and
  a **columns** button listing `editor_model::column_references()`; text
  fields bound to a column get the same button. `editable_field` carries
  the value as written (`source_text`). Replacing a block list puts the new
  value on its key's line (`source_edit::set_field`).
- **`tree_menu`** (`src/edit/tree_menu.hpp`): the right-click menu of a tree
  row (and of an element right-clicked in the figure, `open_at()`), the New
  element dialog and the Delete dialog, ImGui only, drawn from the registry.
  Its choices are computed apart (`child_types()`, `element_draft`) and
  tested without simulated clicks.
- **`table_grid`** (`src/edit/table_grid.hpp`): one table as an ImGui table:
  a header row of editable column names, a row-number column, one
  `InputText` per visible cell (`ImGuiListClipper` for long tables), a
  toolbar (Import CSV, Export CSV, Paste CSV: the clipboard's CSV replaces
  the table; Add row, Add column), and right-click menus on row and column
  headers (insert, delete; a column is renamed by editing its name). A
  column that elements refer to shows a `$` whose tooltip names them.
- **Workspace layout** (`editor_panel`, `src/edit/workspace_layout.hpp`):
  the panel's ImGui context already covers the whole window (its dialogs
  dim the figure). It now draws three regions: the tab bar along the top
  of the left side (28 CSS px), the table grid when a data tab is active,
  and the strip on the right. `figure_box()` is what is left for the
  figure: the left side under the tab bar while "Chart" is active, nothing
  otherwise. Pointer events inside the figure box go to `live_view` with
  their y less the tab bar, all others to the panel. No `live_view` change
  is needed to draw the figure there: the OpenGL3 backend sets a viewport
  of the figure's size from the bottom-left corner, which is exactly the
  left side under the tab bar. The window grows by the tab bar's height as
  it grows by the strip's width.
- **Divider**: the strip's left edge, a 6 CSS px hit zone. Hover sets the
  resize cursor (`native_window::set_cursor`); dragging sets the strip
  width, clamped to [240, window width − 240]; double-click restores 320.
  The width lasts for the session (hiding and showing the panel with `E`
  keeps it) and is not saved.
- **`editor_server`** (`src/mcp/editor_server.hpp`): accepts connections on
  the editor's local socket, one thread per connection, each running an
  ordinary `mcp::session` over the socket (newline-delimited JSON-RPC, as
  stdio). Its tools are `editor_tools` (section 6). A tool handler posts a
  job to the **`ui_queue`** and waits for its result; the UI thread runs
  queued jobs at the top of each loop iteration. `native_window::wake()`
  (thread-safe) breaks a blocking `pump()`.
- **`local_socket`** (`src/ipc/local_socket.hpp`): a stream socket on a
  file path (`AF_UNIX` on Linux, macOS, MSYS2 and Windows 10 1803+ through
  `afunix.h`), with listen, accept, connect, read line, write, close, and
  a `std::streambuf` adapter so `run_mcp`'s loop serves it unchanged.
- **Discovery files** (`src/ipc/editor_registry.hpp`): each editor writes
  `<dir>/<pid>.json` (`pid`, `socket`, `file`, `version`, `focused`: the
  time its window last took focus), whole then renamed, and removes it on
  exit. `<dir>` is `$NYMPH_EDITORS_DIR` (tests), else
  `$XDG_RUNTIME_DIR/nymph`, else `$TMPDIR` (or `/tmp`)`/nymph-<uid>` on
  POSIX, `%LOCALAPPDATA%\nymph\editors` on Windows; it is created
  user-only (0700, refused when another user owns it; the user's default
  ACL on Windows). The socket is `<dir>/<pid>.sock`, or the same name in
  `/tmp/nymph-<uid>` (the temporary folder on Windows) when that path
  would pass the 100-byte limit.
- **The editor's session**: `server_options` gains `builtin_tools`,
  `extra_tools`, `server_name` and `instructions`; the editor's sessions
  have only the editor tools, as `nymph-editor`, with instructions of their
  own. `nymph mcp`'s session adds the bridge after its chart tools.
- **`editor_bridge`** (`src/mcp/editor_bridge.hpp`): in `nymph mcp`, the
  editor tools forward to an editor: they read the discovery directory,
  drop files whose process is gone or whose socket refuses, pick the editor
  named by the optional `editor` argument (a file path or pid) or else the
  one focused last, keep one connection per editor (MCP `initialize`
  once), and pass `tools/call` through. With no editor running, they
  return an error result telling the agent to ask the user to run `nymph
  view --edit <file>`.

## 4. The left side: tabs and tables

- Tabs, left to right: **Chart**, one per table in `data` order (an
  unnamed single table is titled "data"), and **+**. Right-click on a table
  tab: Rename, Delete, Import CSV, Export CSV.
- **+** with no `data` creates a single unnamed table (one column `x`, three
  empty rows). With one unnamed table it opens a dialog naming both
  tables (section 3, `data_edit`). Otherwise it adds `tableN` and opens its
  tab.
- A cell commits on Enter, Tab or focus loss (not on a typing pause: a
  table edit rewrites a whole table, so one undo step per cell, not per
  keystroke). Enter moves down, Tab right. Ctrl+V of several lines of CSV
  or tab-separated text into a cell pastes a block from there, growing the
  table as needed, as one undo step.
- **Import CSV** opens an ImGui dialog with a path field (starting in the
  input file's folder) and a list of that folder's `.csv` files and
  subfolders; the file replaces the table (header included). **Export
  CSV** has the same dialog with a file name field (default `<table>.csv`)
  and asks before overwriting. One dialog for every platform, no native
  file dialog library.
- An inspector field bound to a column (shown read-only in 08) gets a "Go
  to data" link that opens the table's tab with the column selected.
- The figure keeps running while a data tab is shown; it is not drawn, and
  the next frame of "Chart" shows the new data.

## 5. Tree editing

Right-click on a tree row opens:

| Item | Shown when | Does |
|---|---|---|
| Add child ▸ | the element accepts children | types filling any slot it accepts, grouped by slot, appended last |
| Insert before ▸ / after ▸ | the element has a parent | types the parent accepts |
| Add chart below ▸ / to the right ▸ | the row is `root` and not a layout | see below |
| Duplicate | the element has a parent | copies its YAML text after it; `id` fields renamed to fresh ids |
| Move up / down | it has a sibling that way | swaps with it |
| Delete | the element has a parent | removes it; with references, asks (section 3) |

`Delete` and `Ctrl+D` (duplicate) work on the selected element when no
text field has the keyboard; `Alt+Up/Down` moves it (Windows sends Alt
keys as `WM_SYSKEYDOWN`, handled for these two). The "Image" row has no
menu. A right-click (press and release in place) on an element in the
figure selects it and opens the same menu at the pointer; a right-click
in the tree selects the row without scrolling the tree. While a menu is
open the panel takes the keys, so Escape closes the menu, not the window. Type lists come from
`registry::classes_taken_by(parent)` (a list form of
`names_taken_by()`), sorted by name, with the element icons of 08.

**Adding a chart at the root.** When `root` is a layout that takes figures
(`VerticalLayout`, `HorizontalLayout`), the new chart is appended to it.
Otherwise "below" wraps the root in a `VerticalLayout` and "to the right"
in a `HorizontalLayout`, whose children are the old root and the new
chart (`wrap_root`): `root` becomes `root/children/0`, and its descendants
move with it in the `path_map`. Both get `height: -1` (below) or `width:
-1` (to the right) when their type has the field and the old root does not
set it: a root fills the image, but in a layout it wants its natural size,
and two natural sizes rarely fit the image. The new chart is chosen from
the `figure` slot's types.

**New element dialog.** When a starter leaves `?` fields that the editor
cannot fill, or the new element does not bind, a modal shows every field
of the type (the inspector's widgets) with the starter values and the
loader's error; Insert stays disabled until the source with the element
binds. Link ends and other `?` fields are filled from the candidate ids; an
"ids" button beside `from`, `to`, `over` and `attach` lists the diagram's
ids. Values are YAML, as in the source. The Delete dialog lists the
elements that name the one being deleted and offers "Delete with them",
"Delete only this" (usually refused by the loader) and "Cancel".

After an insert the new element is selected and its row scrolled to; after
a delete the selection moves to the next sibling, else the parent.

## 6. MCP tools of the editor

The same definitions (`editor_tool_definitions()` in
`src/mcp/editor_tools.cpp`) are served by the editor and listed by `nymph
mcp`, which adds the optional `editor` argument to each. Paths are element
paths (`root/children/2`); values in `fields` are JSON (string, number,
boolean, list) and encoded with `field_value`'s encoders, or a string
starting with `$` for a column reference; `null` clears a field.

| Tool | Arguments | Result |
|---|---|---|
| `editor_state` | — | file, `modified`, `version`, active tab, selection (path, type, field, its written fields), the tree (path, type, label, `id`, depth, child types it accepts), tables (name, columns, row count), the `view` key, the error the panel shows |
| `editor_source` | — | the whole YAML and `version` |
| `editor_element` | `path` | every field: value, written or default, `bound_to`; the child types it accepts |
| `editor_table` | `table`, optional `offset`, `limit` (rows) | the rows as CSV text, total row count |
| `editor_view_image` | optional `max_width` | PNG of the figure box as on screen, with the selection outline, plus its size |
| `editor_select` | `path`, optional `field`; or `tab` | selects in the panel and shows the tab |
| `editor_set_fields` | `path`, `fields` | new `version` |
| `editor_add_element` | `parent`, `type`, optional `fields`, `index` | new path, `path_map` |
| `editor_remove_element` | `path`, optional `with_references` | `path_map` |
| `editor_move_element` | `path`, `index` | `path_map` |
| `editor_set_cells` | `table`, `cells`: list of `{row, column, value}` | new `version` |
| `editor_set_table` | `table`, `csv`, optional `create` | new `version` |
| `editor_remove_table` | `table` | new `version` |
| `editor_set_source` | `source` | new `version`, `path_map` empty (paths may have changed) |
| `editor_undo` / `editor_redo` | — | new `version` |

Every write takes an optional `expected_version`: when it differs from the
model's `version()` (the user changed something since the agent read the
state), the call fails with "the chart changed since you read it" and
changes nothing. Each write is one undo step and is refused, with the
loader's message and position, exactly as a panel edit would be. A write
by the agent puts "changed by the agent: <tool>" in the panel's status
line (an added element is also selected, so it is outlined); the line
says "agent connected" while a connection is open. `editor_remove_element`
of a referenced element without `with_references` names the referrers.
`editor_table` returns 200 rows unless `limit` says otherwise.

`editor_view_image` draws the figure's last frame into the back buffer
and reads the figure box back (`glReadPixels` on the UI thread) before
drawing the window again, so it is exactly what the user sees: zoom, pan,
theme, selection outline. Scaled down by whole-pixel blocks to
`max_width` (default 1200). On a data tab it is `render_png(source_with_view())`.

## 7. Data Flow

**Agent edit:**

1. The agent calls `editor_add_element` on `nymph mcp` (stdio).
2. `editor_bridge` picks the editor, forwards `tools/call` over its socket.
3. The editor's connection thread runs the handler: it posts a job to
   `ui_queue` and calls `wake()`; it waits (no timeout while the window
   lives; the window closing fails pending jobs with "the editor was
   closed").
4. The UI thread, at the top of the loop: flushes the inspector's pending
   text (so a half-typed edit is not lost or merged), runs the job
   against `editor_model`, applies the result like a panel edit
   (`live_view::set_source_moved` for a structural change, selection), and
   fulfils the job's promise.
5. The result goes back the same way.

**Structural edit from the panel:** `tree_menu` → `editor_model::add_element`
→ `tree_edit::insert_child` → `rename_view_paths` → bind → on success the
viewer calls `live_view::set_source_moved(new, map)`, so zoom, pan, hidden
series and moved windows follow their elements to their new paths.

**Startup and exit:** when the panel first opens (`--edit` or `E`), the
editor creates the socket and its discovery file and starts the accept
thread; while the panel is hidden the tools answer "press E". `--no-mcp`
turns this off. On exit (or a failure of
the server, which is shown in the status line and never stops the editor)
the accept thread is stopped, connections are closed, pending jobs fail,
and the socket and file are removed. A stale file left by a crash is
removed by the next `nymph mcp` that cannot connect to it.

## 8. Public API Contract

`live_view`: `void set_source_moved(std::string_view source, const
path_rename& rename)`, `path_rename` a function from an old element path to
its new one (nullopt: removed). It takes the view on screen as a saved view
would write it (the source's `view` entries with the user's changes over
them), renames its paths, replaces the figure with fresh contexts and
starts from that view; the user's changes stay "changed", so a save keeps
them. A plain overload taking a rename function was ambiguous with
`set_source(source, bool)` (a lambda converts to `bool`), hence the name.

`tree_edit` / `data_edit`: throw `nymph::error` for a path or table that is
not there, a source they cannot patch (as `source_edit`), and a table name
that is empty or has `:`. They do not validate the result; `editor_model`
does.

`editor_model`: each new operation returns `commit_result` (now with
`path_map paths`); a refused change leaves source, selection, undo stack
and `version()` untouched.

`native_window`: `set_cursor(cursor_shape)` (`arrow`, `resize_horizontal`,
`text`), `wake()` (any thread), and `window_events::on_focus(bool)`.

`registry`: `std::vector<const element_class*> classes_taken_by(const
element_class& parent) const`.

CLI: `nymph view --edit [--no-mcp] <file>`. MCP: the tools of section 6, in
the editor and in `nymph mcp`.

## 9. Design Decisions

- **A local socket and a bridge in `nymph mcp`, not HTTP.** Every agent
  that has nymph already talks to `nymph mcp` over stdio (the Claude Code
  plugin, Claude Desktop, Cursor), so the editor tools arrive with no new
  configuration. A loopback HTTP server would need a port, an `Origin` and
  `Host` check against browser pages (DNS rebinding) and a token for other
  users on the machine; a socket in a user-only directory needs none of
  them. The editor still runs a real MCP session on the socket, so HTTP
  is a new front end if a client ever needs a URL (section 10).
- **Agent edits on the UI thread through `editor_model`**, not on the
  connection thread with a lock. The model, the inspector and `live_view`
  are single-threaded (08 section 8); one queue keeps them so and gives
  agent edits undo, validation and redraw for free.
- **Pull, with a version check**, not change notifications. The agent
  reads state when asked to do something; `expected_version` stops it from
  overwriting a change the user made in between. Resource subscriptions
  are a later addition (section 10).
- **Whole-table rewrite for data edits**, not per-cell splices. Cells
  are text either way, the two forms (CSV, columns) and their quoting are
  easy to write, and column insertions would otherwise touch every row.
  Lost: comments inside that table's value. The rest of the file keeps its
  bytes.
- **Starters in one table**, not a form for every new element, and not a
  virtual on every class (the first design). Most types need one or two
  values to bind; what a new element starts with is the editor's concern,
  and one table in `src/edit/` with a test over every registered type
  keeps ~20 element files unchanged. The dialog is the generic fallback.
- **References found by `id` equality**, not a reference field type.
  Diagrams already name items by `id` in text fields; matching values is
  enough for the delete prompt and needs no schema change. A false match
  (a label that happens to equal an id) only adds an element to a list the
  user confirms.
- **Wrap the root in a layout to add a chart**, not a list of roots. The
  format has one `root`; layouts are how charts combine, so the saved file
  is what a user would write.
- **The view state moves through a saved view**, not by renaming ImPlot's
  and ImUml's internal state. Their state is keyed by ImGui IDs made from
  element paths; the saved-view machinery already turns live state into
  data by path and back.
- **Removing the last child leaves `children: []`**, so the parent keeps a
  list to add to and the text stays valid YAML in both styles.
- **Tabs replace the figure**, not a split under it. The figure needs the
  space, and the user asked for tabs. A split view is a later option.
- **ImGui file dialog**, not native ones. X11 has none; the Win32 one
  would be a second code path. A path field and a folder list are enough
  for CSV files.

## 10. Constraints and Invariants

- Outside `--edit`: no tabs, no socket, no discovery file, no thread; frames
  equal today's (08 section 8 tests still pass).
- Only the UI thread touches `editor_model`, the panel, `live_view` and
  OpenGL. Connection threads only parse, post and wait. Shutdown joins
  every thread before the model is destroyed.
- A tool call never blocks the UI thread on the socket: replies are written
  by the connection thread.
- A refused edit (panel or agent) changes nothing (08). A structural edit
  either applies its `path_map` everywhere (source `view` key, model
  selection, `live_view`) or nowhere.
- The socket directory is created user-only and checked to be owned by the
  user before use; a directory owned by someone else is an error, not a
  fallback. Socket paths stay under 100 bytes (macOS limit 104); the
  editor shortens to `<runtime>/nymph/e<pid>.sock` when needed.
- Platform: the socket and wake code are `src/ipc/local_socket.cpp` with
  `#if defined(_WIN32)` guards (Winsock, `afunix.h`, `ws2_32` linked, a
  system library); X11's `wake()` writes to a pipe that `pump(true)` polls
  with the display's connection; Win32's is `PostMessageW`. The web build
  compiles none of `src/ipc/`.
- Data grid: at most `kMaxDataRows` rows (the loader's limit); the clipper
  keeps a frame's cost to the visible rows.
- Tests: socket and discovery paths include the test name (parallel
  ctest); no fixed sleeps, waits on the condition with a timeout.

## 11. Integration Boundaries

Depends on: [08_view_editor](../08_view_editor/DESIGN.md) (editor model,
inspector, panel, `source_edit`, field encoders), [04_web_viewer](../04_web_viewer/DESIGN.md)
section 10 (`window_host`, `live_view`, view state by path),
[06_claude_integration](../06_claude_integration/DESIGN.md) (MCP session,
tool registry, `run_mcp`, the skill text), [01_core_features](../01_core_features/DESIGN.md)
(schema slots, `data` tables, `parse_csv`, column references).

Changes others see: `nymph mcp` lists the editor tools (the skill text and
`docs/agent-guide.md` say when to use them: the user has a chart open in
the editor and refers to "this", "the selected" or what is on screen).
`live_view`'s figure box is a no-op for the web viewer (its box is the
canvas).

Risks: `AF_UNIX` with `afunix.h` on MSVC and MSYS2, `SHUT_RD` waking a
blocked read on macOS and Windows, and the Win32 cursor, wake and Alt-key
code are not compiled or run yet (CI first). The X11 wake pipe and the
figure read-back were checked on Linux.

## 12. Extension Points

- Streamable HTTP front end on loopback for clients without stdio, with an
  `Origin` check and a token.
- `resources/subscribe` on an `editor://state` resource so an agent hears
  selection changes.
- Drag-and-drop in the tree; a chart and table split view; formulas.
