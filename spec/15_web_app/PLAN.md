# nymph — Web App: Plan

Design: [DESIGN.md](DESIGN.md). Each step is a reviewable deliverable; the
next starts after its tests pass. Native tests go in `tests/` with
`nymph_add_test(<name> <sources>)`; web tests extend `web/tests/node/` and
`web/tests/browser/` (Playwright with the preinstalled Chromium, skipped
with 77 when Playwright is missing), serve the built site from a local
server on port 0, and wait on conditions, never fixed sleeps. A
`CHANGELOG.md` entry is added with each user-visible step (5 to 13).

## 1. Risk check: the editor module builds and draws

**Goal:** a throwaway editor module (the panel and the view drawn into one
WebGL2 canvas) runs in Chromium, and the sizes of both modules are known.

**Deliverables:** `web/editor/nymph_editor_web.cpp` with the minimum
exports to create a `live_view` plus an `editor_panel` on a canvas, feed
pointer events and draw (host logic temporarily inline, replaced in step
5); `nymph-editor` target in `web/CMakeLists.txt` linking `editor_tools`
too (through a temporary source list if step 4's library is not there
yet). Measured and written into this step's result: gzip sizes of
`nymph-viewer-core.wasm` before and after (must not change), of
`nymph-editor-core.wasm` with and without `editor_tools` and JSON, and
with ImGuiColorTextEdit's `TextEditor` drawn in the panel (built from a
temporary checkout; step 5 adds the submodule). If JSON
costs more than 150 KB gzip, update DESIGN.md section 5.2 (a smaller set
of exports instead of `tool()`) before step 4. `web/size_budget.json` with
the viewer's budget (measured + 2 %) and the editor's (measured + 10 %);
a `nymph_web_size` ctest that checks them.

**Tests:** `nymph_web_size` passes; a browser test loads the module on a
page, draws `examples/flowchart.yaml` with the panel, and finds both the
figure's and the panel's pixels non-blank in their regions, and a
`TextEditor` showing the source; clicking a
node (position from `live_view::targets_at` via a test export) changes the
panel's pixels.

**Result (done):** the real editor module was built at once instead of a throwaway (`web/editor/`). Sizes (gzip -9): viewer 824,330 bytes before and after (byte-identical `.wasm`), editor 1,077,431 without the source pane and 1,157,017 with ImGuiColorTextEdit (about 80 KB); the editor tools and JSON were kept (the editor's total is under the 1.5 MB the viewer was once held to), their own share was not measured apart. `web/size_budget.json` counts as Node's zlib at level 9 does: viewer 844,917 (+2 %), editor 1,279,233 (+10 %); `nymph_web_size` checks them. Two ImGui contexts in one WebGL2 context draw correctly in Chromium (checked by screenshot and the browser tests of step 6).

## 2. `editor_workspace`

**Goal:** the native editor runs on `editor_workspace`, with behaviour
unchanged.

**Deliverables:** `src/workspace/editor_workspace.hpp/.cpp` and
`workspace_host` (DESIGN.md section 5.1); `editor_panel.*` moved from
`src/window/` to `src/workspace/`; `nymph_workspace` library;
`viewer_window.cpp` reduced to the native host (window, loop, sizing, file
writes, close dialog, MCP server); `spec/14_editor_workspace/DESIGN.md`
paths corrected.

**Tests** (`test_editor_workspace`, fake host, no GL drawing): a pointer
in the figure box reaches the view, in the strip the panel, over the
toolbar the panel, a drag that starts in the figure stays with it across
the strip; the divider drag sets the strip width and double-click resets
it; keys go to the panel while it wants the keyboard, else to shortcuts;
`Ctrl+Z` undoes a commit; a right-click on a target opens its menu. All
existing tests pass. By hand on Xvfb: `nymph view --edit
examples/flowchart.yaml` selects, edits, drags the divider, adds and
deletes a node, as before.

**Result (done):** `src/workspace/editor_workspace.*`, `workspace_input.hpp` (keys, modifiers, cursor shapes, `route_key`), `workspace_tools.*` (the editor tools' host over a workspace); `editor_panel.*` moved to `src/workspace/`; `viewer_window.cpp` is the native host (about 480 lines, from 946). `test_editor_workspace` (13 tests, headless: an empty GLSL version runs frames without GL). Found and fixed: `apply()` asked for panel frames after every panel frame, so the editor never went idle (natively too). By sight on Xvfb: `nymph view --edit examples/flowchart.yaml` draws the figure, the tabs and the panel as before; clicks and keys on Xvfb were not driven (no xdotool here).

## 3. Save and CSV as host requests

**Goal:** the panel asks its host to save and to import or export CSV
instead of opening its own dialogs.

**Deliverables:** `inspector` Save and `table_grid` Import / Export emit
requests through `workspace_host` (`request_save`,
`request_csv_import`, `request_csv_export`); `editor_workspace::import_csv`;
the "overwrite?" modal moves next to the close dialog in the native host;
the ImGui CSV file dialogs move to `src/window/csv_dialog.*`, opened by
the native host. `read_csv_file` / `write_csv_file` are used only by the
native host.

**Tests:** fake host receives each request with the table name and CSV
text; `import_csv` replaces the table as one undo step and a bad CSV sets
the panel error; the existing `table_grid` tests adjusted to the request
form. By hand on Xvfb: Save asks and overwrites; Import / Export CSV work
as before.

**Result (done, changed):** the native host answers `request_save()` with the panel's existing "overwrite?" modal and keeps `table_grid`'s ImGui CSV dialogs (they need a file system); only the web host takes CSV as requests (`table_grid::set_host_files`, `editor_workspace::import_csv`). No `src/window/csv_dialog.*`. Tests in `test_editor_workspace` (requests with the table's CSV, import as one undo step, a bad CSV refused).

## 4. Editor tools without the MCP server, attributed changes

**Goal:** `editor_tools` runs in-process against an `editor_host`, in a
library without `nymph_cli` or sockets, and every change records who made
it and what it was.

**Deliverables:** `nymph_editor_tools` library (`editor_tools.*`, the tool
types it needs from `mcp/tools.hpp`, `nymph_json`); a direct
`call_editor_tool(host, name, args, origin) -> json` used by both the
socket server and the web module; `nymph_mcp` links it. `editor_model`
undo entries `{source, label, origin}`, `commit_result` carrying both,
`begin_group()` / `end_group()` (DESIGN.md section 5.1); undo / redo
tooltips with the label; `nymph mcp`'s agent edits get origin
`agent:mcp`.

**Tests:** `test_editor_tools` calls `editor_state`, `editor_set_fields`
(with a stale `expected_version`: refused), `editor_add_element` and
`editor_undo` directly on a fake host; `test_editor_model`: a commit's
label and origin are on its undo entry; three commits inside a group are
one undo step and one redo step; an empty group adds nothing; a refused
commit inside a group leaves the others; the existing
`test_editor_server` tests pass.

**Result (done):** `src/mcp/tool.*`, `nymph_editor_tools` library, `make_editor_tools()` / `call_editor_tool()`; `editor_model` undo entries `{source, label, origin}`, `begin_group()` / `end_group()`, `last_label()` / `last_origin()`, undo / redo tooltips. `test_editor_attribution` (10 tests).

## 5. Source pane

**Goal:** `nymph view --edit` shows the YAML in an ImGui code editor beside
the figure, in sync both ways, and opens a source that does not bind in
repair state.

**Deliverables:** `extern/ImGuiColorTextEdit` submodule (pinned,
`nymph_ensure_submodules()` in the `NYMPH_VIEW` / `NYMPH_WEB` blocks,
`TextEditor.cpp` built into `nymph_workspace`, its licence in the
third-party notices); `src/edit/yaml_language.*`; `src/edit/source_pane.*`
(DESIGN.md section 5.4: FiraMono in the panel atlas, palettes by theme,
debounced apply, error squiggle and marker, read-only panel while ahead,
span patching, undo routing, selection sync); `workspace_layout` source
region and divider; Source button and `Ctrl+U` in the tab bar;
`editor_workspace::open()` / `show_source()` / `repairing()`;
`editor_model` creatable from repair state; `view_command`: `--edit` with
a bad source opens in repair state; `editor_state` reports the pane;
`docs/viewer.md` and `docs/cli.md`.

**Tests** (`test_source_pane`, `test_yaml_language`, headless ImGui):
tokens of keys, strings, numbers, comments, type names and `$column`;
typing a new title applies after the pause and the model's source has it;
a broken text marks the loader's line and column, leaves the model and
figure as they were, makes the inspector read-only, and an agent write
(`call_editor_tool`) is refused; fixing it applies and clears the marks;
a panel commit changes only the changed span of the pane (cursor line
kept); `Ctrl+Z` in the pane undoes the document while in step and the text
while ahead; selecting a node moves the cursor to its line and putting the
cursor in a node's line selects it; `open()` with a broken source is in
repair state and becomes a normal editor once it binds; a source without
the pane shown is untouched (existing tests pass). By hand on Xvfb:
`nymph view --edit` on a broken file opens, is fixed and saved; drag the
pane's divider.

**Result (done, changed):** the pane sits between the figure and the strip, not left of the figure (DESIGN.md section 5.4: the figure must stay at x = 0). `src/workspace/source_pane.*` and `yaml_language.*` (not `src/edit/`: they need the text editor); `nymph_text_editor` library; repair state in `editor_model` (`repair_t`), `editor_panel` (`repair`) and `editor_workspace::open()`; `nymph view --edit` opens a broken file in repair state (`view_request::repair`). `test_source_pane` (13 tests: tokens, apply after the pause, refused text marked and locking agents, span patching keeping the cursor, undo routing, selection and cursor, repair, a click giving the pane the keyboard, Ctrl+V from the host's clipboard, Ctrl+U). Not driven by hand on Xvfb.

## 6. Web editor module and `nymph-editor.js`

**Goal:** a page can embed the editor with `createEditor()`, with the full
editor working by mouse, keyboard and the tool API.

**Deliverables:** `web/editor/nymph_editor_web.cpp` over
`editor_workspace` (exports with every exception caught); the browser
`workspace_host` (cursor, clipboard, requests as JS events);
`web/js/nymph-editor.js` (DESIGN.md sections 5.2 and 5.3: own WebGL2
context, `ResizeObserver`, pointer, keys, hidden `textarea`, clipboard,
fonts from `nymph-viewer.js`'s exported loader, `tool()`); a test-only
headless flag; `docs/viewer.md` section on the editor module.

**Tests:** Node: create headless from `examples/dashboard.yaml`;
`tool("editor_set_fields")` changes the title, `modified()` true,
`source()` has it, `undo()` restores and `modified()` is false;
`setSource(…, {asChange: true})` is undoable, without it is not;
`toPng()` equals `render_png(toYaml())`; a bad source rejects with line
and column. Browser: typing into the title field through the `textarea`
changes `source()`; `Ctrl+C` then `Ctrl+V` into another field copies the
text; typing in the source pane (shown with `showSource(true)`) changes
the figure and `source()`; `createEditor` with a broken source rejects,
and with `repair: true` opens with `repairing()` true; `Ctrl+D`
duplicates the selected node and the page sees
`defaultPrevented`; `csv-export` fires with the table's CSV and
`importCsv` replaces it; `save` fires on `Ctrl+S`; `toolDefinitions()`
equals the native `editor_tool_definitions()`; a `tool()` call with
`origin: "agent:test"` fires `change` with that origin and its label, and
inside `beginGroup` / `endGroup` two calls undo at once;
`editor_view_image` returns a PNG of the figure box with the selection
outline. By hand: Firefox and
Safari typing in a field and in the source pane (IME on one of them).

**Result (done):** `web/editor/nymph_editor_web.cpp`, `web/js/nymph-editor.js` (also `paneText()`, `targetRect()`, `importCsv()`); `nymph_web_editor_node` (11 tests, headless) and `nymph_web_editor_browser` (7 tests: draws, click and typing, Ctrl+D with the default prevented, Ctrl+S and CSV events, typing in the source pane, paste through the page's clipboard, tool definitions, agent changes in a group, the view image). Found and fixed: a mouse click moved the focus from the hidden text area to the canvas. Firefox and Safari typing and IME not tried.

## 7. Site and app shell: open and view

**Goal:** the site's root is the app; a user opens a local file and sees
it in the viewer, with clear messages for every failure.

**Deliverables:** `web/app/` (`index.html`, `app.css`, `app/main.js`,
`app/document.js`, `app/open.js`, `app/ui/*.js`), start screen (drop
zone, Open file, Paste), document screen with title, View mode, ⋯ menu
(theme, Reset view, About), the message banners of DESIGN.md section 3.2
for this step's cases; `nymph-site` target assembling
`<build>/web/site/` (section 3.1, examples moved to `examples/`, demo
reading them there, `version.json`, `config.json` with an empty `agents`
section, `agent/` with the agent guide and element pages); the command
registry `app/commands.js` (DESIGN.md section 3.11) with this step's
commands, every control calling it, origin checks and the confirmation
dialog for `external` commands from non-user origins; `app/device.js`
capabilities; the document screen's empty right-hand dock; `pages.yml` building
`nymph-site`, running the web tests and deploying it; `?v=<commit>` on
module URLs.

**Tests** (browser, on the built site): the start screen shows; a YAML
chosen with the file chooser draws (`rendered()`), its name in the title;
a nymph PNG dropped (synthetic `DataTransfer`) draws; pasted YAML draws; a
source with an error shows its line and column; a PNG without a chunk
shows the "no nymph chart" message; each button and shortcut logs a
`commands.call` and `commands.list()` validates as JSON Schema;
`app_download` called with origin `agent:test` opens the confirmation
dialog and Cancel downloads nothing; a test that fills the dock shrinks
the canvas and the chart redraws; the old URLs (`nymph-viewer.js`,
`.wasm`, `fonts/…`, `demo/`) answer 200; no request leaves the local
server.

**Result (done):** `web/app/` (index.html, app.css, `app/` modules: main, commands, device, document, open, share, store, ui, icons, providers/drive), `cmake/NymphSite.cmake` and the `nymph-site` target, examples at the site root, `pages.yml` builds, tests and deploys `build-web/web/site/`. Browser tests in `web/tests/browser/app.mjs` (23 tests for steps 7 to 13) and Node tests in `web/tests/node/app.mjs`.

## 8. Edit mode and saving

**Goal:** the user switches to Edit, edits, and saves to the same file
or as a download, without ever losing changes silently; every action is a
command.

**Deliverables:** View | Edit toggle with lazy load, progress line and idle
prefetch; hand-over both ways keeping the view (DESIGN.md section 4);
modified tracking ignoring the `view` key; Save ▾ (Save, Download YAML,
Download PNG) and `Ctrl+S`; save in place through `FileSystemFileHandle`
where available; PNG inputs saved as PNG; the Source button (switching
to Edit in View mode) and "Fix in source" (Edit in repair state at the
error line); the `document_*`, `editor` and remaining `app_*` commands
(editor tools registered on load, removed on destroy); unsaved-changes
dialog on open,
close and `beforeunload`; Edit disabled without WebGL2, with the reason.

**Tests** (browser): View → Edit after a zoom keeps the plot's x range
(`tool("editor_state")` view key) and is not modified; an edit marks the
title dot; Download YAML gives the edited source with the view; Download
PNG of a PNG input carries the edited source (`extract_source`); save in
place with a stubbed `showOpenFilePicker` writes the new bytes; opening
another file while modified shows the dialog and Cancel keeps the
document; Source in View mode opens Edit with the pane; a broken file's
"Fix in source" opens repair state, and once fixed the file can be saved
and viewed; `document_set_source` with origin `agent:test` works in View
mode (viewer only) and in Edit mode (one undo step); `editor_*` commands
are listed only in Edit; Edit → View shows the edited chart; the viewer
module was not
re-downloaded and the editor module was fetched only on Edit.

**Result (done):** as planned. "The editor module was fetched only on Edit" is not asserted: after the first chart the page prefetches it on idle (desktop only), which the mobile test checks never happens there.

## 9. URLs and share links

**Goal:** files open from a URL or a shared link, and the user can share
what is on screen.

**Deliverables:** From URL dialog and `?url=` (raw rewrites for GitHub
`blob/` and gists, 16 MB limit, `credentials: "omit"`, CORS message),
`?example=`, `#y=` encoding and decoding, `&mode=edit`,
`history.replaceState` after open; Share ▾ (Copy link with the long-link
warning, Copy embed code, Download); the demo page's "Open in the app"
links.

**Tests** (browser): `?url=` to a file on a second local server with CORS
draws, without CORS shows the CORS message, a 404 shows the status; a
GitHub `blob/` URL is rewritten (unit test of the rewrite function in
Node); `#y=` from Copy link reopens the same source byte for byte;
`?example=line&mode=edit` opens the editor; Copy embed code gives a snippet
that, on a blank page, draws the chart.

**Result (done).**

## 10. Start screen: recent, drafts, templates

**Goal:** the user gets back to recent work, recovers unsaved changes, and
can start from a template.

**Deliverables:** `app/store.js` (IndexedDB `recent` and `drafts`,
DESIGN.md section 3.5), thumbnails, Clear; draft writing and the restore
banner; New chart with `web/app/templates/*.yaml` (one per family) and
the examples gallery.

**Tests** (browser): a URL-opened file appears in Recent and reopens; a
local file opened through a stubbed handle reopens after a reload; an
edit, then a reload, offers the draft and Restore brings it back; saving
removes the draft; every template binds and draws (also a native test that
each template binds, `test_web_templates`); Clear empties both stores.

**Result (done):** templates in `web/app/templates/` with their PNGs (rendered with `--rasterizer software`, committed); `test_web_templates` (native) and the browser test open every template.

## 11. Google Drive

**Goal:** a user with the site configured opens a file from Drive and
saves it back, and nothing about Drive shows when it is not configured.

**Deliverables:** `app/providers/provider.js` (interface) and
`app/providers/drive.js` (DESIGN.md section 3.8: token client with
`drive.file`, Picker, download, save with the revision check, Save as);
`?drive=` route; `config.json` from `NYMPH_GOOGLE_*` cache variables and
the repository variables in `pages.yml`; CSP entries for the Google
origins; `docs/web-app.md` owner setup (Cloud project, consent screen,
client id, API key restricted to the site, Picker API).

**Tests** (browser, Google's scripts replaced by local stubs that record
calls): without config no Drive control exists; with config Open goes
through sign-in, Picker and download and draws; Save sends the PATCH with
`If-Match`; a 412 from the stub offers Overwrite / Save as a copy; no
Google URL is requested before a Drive action. By hand (needs the
owner's keys): open, edit and save a real Drive file; open by `?drive=`.

**Result (done, stubs only):** tested with Google's scripts and the Drive API replaced by stubs (open, `?drive=` address, save with the revision check, the conflict dialog). The real Drive (the owner's keys) is not tried.

## 12. Installable and offline

**Goal:** the app installs as a PWA, opens `.yaml` files from the
system, works offline, and updates only when the user agrees.

**Deliverables:** `manifest.webmanifest` (icons from `resources/icon.png`,
`file_handlers` for `.yaml` / `.yml`), `sw.js` (DESIGN.md section 3.9),
`launchQueue` handling, the update banner.

**Tests** (browser): after one visit, with the local server stopped, the
app reloads and opens an example; a new `version.json` shows the update
banner and does not reload by itself; URL fetches are not cached by the
service worker; `launchQueue` with a stubbed file opens it. By hand:
install on Chrome desktop and open a `.yaml` from the file manager.

**Result (partly done):** manifest (with a 192 px icon), service worker, `launchQueue`, update banner. Tested: offline reload after one visit. Not tested: the update banner with a new `version.json`, `launchQueue` with a stubbed file, installing on a desktop.

## 13. Mobile view-only, keyboard, accessibility

**Goal:** on a phone or tablet the app is a complete viewer with no way
into the editor, and on every device it works by keyboard.

**Deliverables:** `app/device.js` `isMobile()` (DESIGN.md section 3.10);
the shell hides Edit, Source, "Fix in source", New chart, Save in place
and Save to Drive on mobile, ignores `&mode=edit`, skips draft offers,
and never fetches, prefetches or precaches the editor module there;
responsive top bar (one ⋯ menu under 720 px of window width); Keyboard
shortcuts dialog; labels and focus order for every control; the editor
canvas' label saying it is not readable by screen readers and pointing to
Download YAML.

**Tests** (browser): emulated phone (390 x 844, touch, no fine pointer):
no Edit or Source control exists; `?example=line&mode=edit` opens in View;
a broken file shows the "open it on a computer" message with no action;
a pinch zooms the viewer; Share and Download work; no request for
`nymph-editor-core` is made (request log, service worker included). A
desktop page resized to 390 px wide keeps Edit (the top bar collapses)
and an open editor keeps running. An axe-core scan of the start and
document screens has no serious violations; every menu works with the
keyboard alone. By hand: a real phone (iOS Safari, Android Chrome) and a
tablet: view only.

**Result (partly done):** `app/device.js` capabilities, mobile view-only, the narrow top bar (Source and Share in the ⋯ menu under 720 px), Keyboard shortcuts dialog, labels. Tested on an emulated phone and a narrow desktop window. The axe-core scan and real phones and tablets are not done.

## 14. Documentation and release

**Goal:** users and the owner can find, use and configure the app.

**Deliverables:** `docs/web-app.md` (user guide: opening, editing, saving,
sharing, privacy; owner setup: Pages, repository variables, Google);
`README.md` (link to the app, row in Further Documentation);
`docs/viewer.md` (editor module, site layout); `docs/agent-guide.md` and
the skill text: agents may give users a `#y=` link to the app;
`CHANGELOG.md`; `spec/INDEX.md` status.

**Tests:** the docs' links resolve (existing link check, if any, else by
hand); a deploy from `main` serves the app at the site root.

**Result (done):** `docs/web-app.md`, README, `docs/viewer.md` (the editor module), `docs/building.md`, `docs/cli.md`, `docs/agent-guide.md`, CHANGELOG, `spec/INDEX.md`. The first deploy from `main` is not done yet.

## Completion Criteria

- Every test above passes in CI (native on Linux, macOS, MSVC and MSYS2;
  web tests in the web job and in `pages.yml`); PNG output and the viewer's
  API unchanged; the viewer within its size budget.
- On the deployed site, by hand: open a YAML from the computer, a PNG by
  drag and drop, a file from a GitHub URL, a shared `#y=` link; edit each
  (select, toolbar, add a node, edit a table, the source pane) and save in place
  (Chrome) and as a download (Firefox); reload with unsaved changes and
  restore; with the owner's Google keys, open and save a Drive file;
  install the PWA and open a file offline; on a phone, open, view, share
  and download a chart, with no edit control anywhere.
- `nymph view --edit` behaves as before on Linux and Windows, plus the
  source pane, repair state and labelled undo.
- Agent readiness (DESIGN.md section 11.2): every action is a command with
  a schema; agent-origin calls are attributed, undoable in groups, and
  confirmed when `external`; the dock and `agent/` files exist; nothing in
  this project calls a model API.
- `spec/INDEX.md`, `README.md`, `docs/web-app.md`, `docs/viewer.md` and
  `CHANGELOG.md` describe what was built.
