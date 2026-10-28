# nymph — Editor Workspace: Plan

Design: [DESIGN.md](DESIGN.md). Each step is a reviewable deliverable; the
next starts after its tests pass. Tests go in `tests/` with
`nymph_add_test(<name> <sources>)`, deterministic, with socket and file
names that include the test name, and no fixed sleeps. Panel UI is tested
headless against a plain ImGui context, as 08's inspector tests do; window
behaviour is checked by hand and recorded in each step's result. A
`CHANGELOG.md` entry is added with each user-visible step (1, 4, 7, 10).

## 1. Resizable divider

**Goal:** the user drags the divider between figure and panel to resize
the panel.

**Deliverables:** `editor_panel`: `kWidth` becomes a default; `strip_width()`
/ `set_strip_width()` clamped to [240, window width − 240]; a pure
`divider_hit(x, strip_x)` (6 CSS px zone). `viewer_window.cpp`: pointer
down in the zone starts a drag (before routing to figure or panel), moves
set the width, double-click restores 320, `E` keeps the width.
`native_window::set_cursor(cursor_shape)` on Win32 (`IDC_SIZEWE`) and X11
(`XCreateFontCursor`, `XC_sb_h_double_arrow`, through the run-time
loaded libX11). `docs/viewer.md` mentions the divider.

**Tests** (`test_editor_layout`): clamping at both ends and on a window too
narrow for both minimums (figure keeps 240, strip what is left, never
negative); hit zone edges; the figure box shrinks and grows with the
strip. By hand: drag on Windows and X11, cursor changes, figure redraws
while dragging.

**Result (done):** `src/edit/workspace_layout.hpp`, variable strip width in
`editor_panel`, divider drag and double-click in `viewer_window.cpp`,
`native_window::set_cursor` (X11 font cursors; Win32 `WM_SETCURSOR`),
`test_editor_layout` (7 tests). Dragged on Xvfb: the strip and the figure
re-lay out. Win32 not compiled here.

## 2. Structural source patches

**Goal:** elements can be inserted, removed, moved and the root wrapped in
the YAML text, with the paths that changed reported.

**Deliverables:** `src/edit/tree_edit.hpp/.cpp`: `path_map`,
`insert_child`, `remove_element`, `move_element`, `wrap_root`,
`rename_view_paths`, `find_references`. Flow and block lists, a missing
`children:` key, comments kept, indentation of the parent's list.

**Tests** (`test_tree_edit`): each operation on flow and block children
gives exactly the expected text; insert at 0, middle, end, into an element
without `children`; removing the last child leaves `children: []` (or
removes the key, whichever binds and reads better: decide here and record
it); move up/down in both styles; `wrap_root` of a flow and a block root
re-indents the old root and binds; `path_map` for each (descendants
included); `rename_view_paths` renames and drops entries;
`find_references` finds `FlowLink.from/to` and `Note.attach` in
`examples/flowchart.yaml` and nothing outside the diagram.

**Result (done):** `src/edit/tree_edit.*` and `src/edit/yaml_text.hpp` (the
position helpers moved out of `source_edit.cpp`), `test_tree_edit` (22
tests). Removing the last child leaves `children: []`.

## 3. Starters and structural operations in the model

**Goal:** `editor_model` adds, removes, moves, duplicates and wraps
elements with validation, undo and renamed paths everywhere.

**Deliverables:** `src/edit/element_starter.*`: a starter table for the
types that do not bind empty (found by a test that inserts every class into
a parent that takes it; a table, not a virtual per class, see DESIGN
section 9); fresh `id`s; `registry::classes_taken_by`; `editor_model`
operations of DESIGN section 3, `version()`, `commit_result::paths`;
`live_view::set_source_moved`; the viewer applies the map.

**Tests** (`test_editor_model` extended, `test_starters`): for every
registered class and every parent class that takes it, inserting its
starter (with `?` fields filled by the test) into a minimal source binds;
delete with and without references; refused delete of `root`; selection
and `view` entries follow a move; undo restores source and paths;
`version()` rises only on accepted changes; `live_view` keeps a plot's
zoom across a sibling insert before it.

**Result (done):** starters for ~60 types, a per-diagram key for sequence
notes (`over: [?]`); `test_starters` (3 tests, over 100 type-parent pairs;
a few placement rules beyond slots, such as one root `MindNode`, are left to
the New element dialog), 10 new `test_editor_model` tests, a `live_view`
test that a pan follows the root into a layout. `add_chart` stretches both
charts (a wrapped root otherwise overflows the image).

## 4. Tree context menu

**Goal:** the user adds, inserts, duplicates, moves and deletes elements
from the tree.

**Deliverables:** `src/edit/tree_menu.*` (menu of DESIGN section 5, New
element dialog, root "Add chart"), keyboard shortcuts, selection after the
change, `docs/viewer.md`.

**Tests** (`test_tree_menu`, headless ImGui): the Add child list of a
`Flowchart` is exactly the classes the registry says it takes; root menu
on a `Plot` root wraps it; a `FlowLink` from the menu with a selected node
opens the dialog prefilled and Insert binds; Delete on a referenced node
lists the references. By hand: every menu item on `examples/flowchart.yaml`
and `examples/dashboard.yaml`.

**Result (done):** `src/edit/tree_menu.*`, shortcuts through
`editor_panel::shortcut()`, `test_tree_menu` (8 tests). By hand on Xvfb:
Insert after a node offers only Flowchart types, a link is inserted from the
selected node, Delete of a referenced node lists its links, Ctrl+Z restores.
Added on request: a right-click on an element in the figure opens the same
menu; menus close on Escape and take the keys while open.

## 5. Table patches

**Goal:** a table's contents, columns and name can be changed in the
source, references kept working.

**Deliverables:** `src/edit/data_edit.*`: `write_table` (both forms),
`set_table`, `add_table` (including the single-to-named conversion),
`remove_table`, `rename_table`, `rename_column` with reference rewriting.

**Tests** (`test_data_edit`): CSV and columns forms round-trip through
`parse_data` cell for cell, including cells needing quotes, empty cells
and `~`; other top-level keys keep their bytes; conversion renames bare
references and binds; renames rewrite `$col` and `$t:col` but not `$2` or
`$$`; removing a referenced table is refused by the model with the
loader's message.

**Result (done):** `src/edit/data_edit.*`, `test_data_edit` (14 tests). Simple
names are written as plain keys; a one-column row of empty cells is written
`""` (a blank CSV line would be skipped).

## 6. CSV files and data operations in the model

**Goal:** `editor_model` edits cells, rows, columns and tables, and reads
and writes CSV files.

**Deliverables:** `src/edit/csv_file.*`; `editor_model::tables()` and the
data operations of DESIGN section 3; block paste parsing (CSV or tabs).

**Tests** (`test_csv_file`, `test_editor_model`): import then export gives
the same file for a quoted, UTF-8 file with a BOM (BOM dropped on write);
write is atomic (an unwritable target leaves no partial file); each data
operation is one undo step; a pasted block grows the table.

**Result (done):** `src/edit/csv_file.*`, model data operations,
`test_csv_file` (5 tests), 4 new `test_editor_model` tests.

## 7. Tabs and the table grid

**Goal:** the left side shows "Chart", one tab per table and "+", and a
table tab is an editable grid with import and export.

**Deliverables:** `src/edit/table_grid.*`, the ImGui file dialog, tab bar
and figure box in `editor_panel` (no `live_view` change: the viewer offsets
the pointer and the backend's viewport already puts the figure under the
tab bar), input routing by figure box, "Go to data" in the inspector,
`docs/viewer.md`.

**Tests** (`test_table_grid`, headless ImGui; `test_editor_layout`): typing in
a cell and pressing Enter commits once; Tab moves right; the header rename
calls `rename_column`; a 10 000-row table draws only visible rows; the
figure box is the left side under the tab bar on "Chart" and empty on a
data tab; pointer events outside it never reach `live_view`. By hand: edit
`examples/dashboard.yaml`'s tables, import and export a CSV, add a table.

**Result (done):** `test_table_grid` (5 tests: commits, files, the dialog's
entries, a 10 000-row table draws only visible rows, dialogs headless);
`find_column_uses()` for the `$` marks. By hand on Xvfb: a cell edited and
Enter moving down, the chart updated, "+" making the first table and then
naming both, Export writing the file.

## 8. Local socket and window wake

**Goal:** a stream socket on a file path and a cross-thread window wake
work on every platform.

**Deliverables:** `src/ipc/local_socket.*` (listen, accept, connect, line
read, write, close, `streambuf`), `src/ipc/editor_registry.*` (runtime
directory, ownership check, discovery files, stale cleanup),
`native_window::wake()` (Win32 `PostMessageW`, X11 pipe polled with the
display), `window_events::on_focus`, CMake: `src/ipc/` native only,
`ws2_32` on Windows. First, record in this step's result whether
`afunix.h` builds on MSVC and MSYS2 in CI and the socket path length used
on macOS.

**Tests** (`test_local_socket`, `test_editor_registry`): echo of lines in
both directions between threads; a closed peer ends reads; a second
connection while the first is open; a directory not owned by the user is
refused (POSIX); a file of a dead pid is cleaned up; the most recently
focused editor is picked. Wake: a thread calls `wake()` and a blocking
`pump(true)` returns (window tests skip without a display, as 04's do).

**Result (done):** `src/ipc/` in `nymph_mcp` (`ws2_32` on Windows),
`test_local_socket` (10 tests with the discovery files), `test_window_wake`
(passes on X11, skips without a display). Socket paths longer than 100
bytes fall back to `/tmp/nymph-<uid>`. `afunix.h` on MSVC and MSYS2 not
yet built (CI).

## 9. Editor MCP server

**Goal:** a running editor answers the tools of DESIGN section 6 over its
socket.

**Deliverables:** `src/mcp/editor_tools.*` (definitions and handlers on an
`editor_model` and a view interface), `src/mcp/editor_server.*` (accept
thread, session per connection, `ui_queue`), wiring in `viewer_window.cpp`
(start with the panel, stop and clean on exit, `--no-mcp`, status line,
"agent connected", "changed by the agent").

**Tests** (`test_editor_server`): a client over the socket initializes,
lists the tools (golden JSON) and runs each tool against an `editor_model`
whose `ui_queue` is drained by the test thread; a write with a stale
`expected_version` changes nothing; an invalid edit returns the loader's
message; closing the server fails a pending call; two clients at once are
served in order.

**Result (done):** `src/mcp/editor_tools.*`, `editor_server.*`,
`server_options` for the editor's own session, `editor_model::set_fields`,
`test_editor_server` (10 tests). Connections are shut for reading only on
close, so a "the editor was closed" reply still goes out.

## 10. Bridge in `nymph mcp`

**Goal:** an agent using `nymph mcp` reaches the open editor with no new
setup.

**Deliverables:** `src/mcp/editor_bridge.*`; the editor tools added to
`nymph mcp` with the `editor` argument; the "no editor open" error;
`docs/claude.md`, `docs/cursor.md`, `docs/agent-guide.md` and
`claude/skill/` (when to use the editor tools: the user refers to the open
chart); `docs/cli.md` (`--no-mcp`).

**Tests** (`test_mcp`, `test_mcp_process`): `tools/list` includes the
editor tools; with no discovery file a call returns the error result;
with a test editor server (step 9's, no window) in a per-test runtime
directory, `editor_state` and `editor_add_element` pass through; a stale
file is skipped and removed; `editor` picks between two servers. By hand,
recorded: Claude Code with the plugin, `nymph view --edit
examples/flowchart.yaml`, select a node, ask "connect this node to Done".

**Result (done):** `src/mcp/editor_bridge.*`, 4 bridge tests in
`test_editor_server`, `test_mcp` / `test_mcp_tools` updated for the longer
tool list; docs and the skill text. By hand: `nymph mcp` over standard input
against `nymph view --edit` on Xvfb read the selection, added a link from it
to "Done" and returned the view image. Claude Code itself not run. The
plugins' skill copies are left to the release sync (they pin 0.1.0, which
has no editor tools).

## 11. Figure image for the agent

**Goal:** `editor_view_image` returns what the user sees.

**Deliverables:** frame-buffer read-back of the figure box after the next
frame on the UI thread, PNG encode, `max_width` scaling; the
`render_png(source_with_view())` path on a data tab.

**Tests** (`test_editor_server`): the off-screen path returns a PNG of the
box's size; `max_width` scales it down. By hand: the read-back shows the
selection outline and not the panel, on Windows and X11 (scale 1 and 2).

**Result (done):** read-back in `viewer_window.cpp` (`glReadPixels` of the
figure box, whole-pixel downscaling, `encode_png`); checked by hand on Xvfb
(the image is the figure box with the selection outline, not the panel).
The off-screen path is covered by `test_editor_server`.

## Completion Criteria

- Every test above passes in CI on Linux, macOS, MSVC and MSYS2; the 08
  tests still pass and PNG output is unchanged.
- By hand on Windows and Linux: drag the divider; add a chart at the root
  of `examples/line.yaml`; add, link and delete nodes in
  `examples/flowchart.yaml`; edit, import and export tables of
  `examples/dashboard.yaml` and add one; from Claude Code, read the
  selection and connect two nodes; undo the agent's change.
- `spec/INDEX.md`, `README.md` (the `nymph view` sentence),
  `docs/viewer.md`, `docs/cli.md`, `docs/claude.md`, `docs/agent-guide.md`
  and `CHANGELOG.md` describe what was built.
