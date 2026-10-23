# nymph — Web App: Plan

Design: [DESIGN.md](DESIGN.md). Each step is a reviewable deliverable; the
next starts after its tests pass. Native tests go in `tests/` with
`nymph_add_test(<name> <sources>)`; web tests extend `web/tests/node/` and
`web/tests/browser/` (Playwright with the preinstalled Chromium, skipped
with 77 when Playwright is missing), serve the built site from a local
server on port 0, and wait on conditions, never fixed sleeps. A
`CHANGELOG.md` entry is added with each user-visible step (5, 6, 7, 8, 9,
10, 11, 12, 13).

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
`nymph-editor-core.wasm` with and without `editor_tools` and JSON. If JSON
costs more than 150 KB gzip, update DESIGN.md section 5.2 (a smaller set
of exports instead of `tool()`) before step 4. `web/size_budget.json` with
the viewer's budget (measured + 2 %) and the editor's (measured + 10 %);
a `nymph_web_size` ctest that checks them.

**Tests:** `nymph_web_size` passes; a browser test loads the module on a
page, draws `examples/flowchart.yaml` with the panel, and finds both the
figure's and the panel's pixels non-blank in their regions; clicking a
node (position from `live_view::targets_at` via a test export) changes the
panel's pixels.

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

## 4. Editor tools without the MCP server

**Goal:** `editor_tools` runs in-process against an `editor_host`, in a
library without `nymph_cli` or sockets.

**Deliverables:** `nymph_editor_tools` library (`editor_tools.*`, the tool
types it needs from `mcp/tools.hpp`, `nymph_json`); a direct
`call_editor_tool(host, name, args) -> json` used by both the socket
server and the web module; `nymph_mcp` links it.

**Tests:** `test_editor_tools` calls `editor_state`, `editor_set_fields`
(with a stale `expected_version`: refused), `editor_add_element` and
`editor_undo` directly on a fake host; the existing `test_editor_server`
tests pass.

## 5. Web editor module and `nymph-editor.js`

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
text; `Ctrl+D` duplicates the selected node and the page sees
`defaultPrevented`; `csv-export` fires with the table's CSV and
`importCsv` replaces it; `save` fires on `Ctrl+S`. By hand: Firefox and
Safari typing (IME on one of them).

## 6. Site and app shell: open and view

**Goal:** the site's root is the app; a user opens a local file and sees
it in the viewer, with clear messages for every failure.

**Deliverables:** `web/app/` (`index.html`, `app.css`, `app/main.js`,
`app/document.js`, `app/open.js`, `app/ui/*.js`), start screen (drop
zone, Open file, Paste), document screen with title, View mode, ⋯ menu
(theme, Reset view, About), the message banners of DESIGN.md section 3.2
for this step's cases; `nymph-site` target assembling
`<build>/web/site/` (section 3.1, examples moved to `examples/`, demo
reading them there, `version.json`, `config.json`); `pages.yml` building
`nymph-site`, running the web tests and deploying it; `?v=<commit>` on
module URLs.

**Tests** (browser, on the built site): the start screen shows; a YAML
chosen with the file chooser draws (`rendered()`), its name in the title;
a nymph PNG dropped (synthetic `DataTransfer`) draws; pasted YAML draws; a
source with an error shows its line and column; a PNG without a chunk
shows the "no nymph chart" message; the old URLs (`nymph-viewer.js`,
`.wasm`, `fonts/…`, `demo/`) answer 200; no request leaves the local
server.

## 7. Edit mode and saving

**Goal:** the user switches to Edit, edits, and saves to the same file
or as a download, without ever losing changes silently.

**Deliverables:** View | Edit toggle with lazy load, progress line and idle
prefetch; hand-over both ways keeping the view (DESIGN.md section 4);
modified tracking ignoring the `view` key; Save ▾ (Save, Download YAML,
Download PNG) and `Ctrl+S`; save in place through `FileSystemFileHandle`
where available; PNG inputs saved as PNG; unsaved-changes dialog on open,
close and `beforeunload`; Edit disabled without WebGL2, with the reason.

**Tests** (browser): View → Edit after a zoom keeps the plot's x range
(`tool("editor_state")` view key) and is not modified; an edit marks the
title dot; Download YAML gives the edited source with the view; Download
PNG of a PNG input carries the edited source (`extract_source`); save in
place with a stubbed `showOpenFilePicker` writes the new bytes; opening
another file while modified shows the dialog and Cancel keeps the
document; Edit → View shows the edited chart; the viewer module was not
re-downloaded and the editor module was fetched only on Edit.

## 8. URLs and share links

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

## 9. Source drawer

**Goal:** the user edits the YAML text beside the chart, both ways in
sync, and repairs a file that does not bind.

**Deliverables:** `web/app/package.json` / lockfile pinning CodeMirror 6
and esbuild; `nymph-site` bundles it to `site/app/vendor/` when `npm` is
found, else the text-area fallback; `app/ui/source.js` (debounced
re-bind, error mark at line and column, sync with `change` events, cursor
kept); "Fix in source" from the error banner; the drawer resizable,
right or bottom by width.

**Tests** (browser): typing a new title in the drawer changes the viewer
and, in Edit, `editor.undo()` reverts it in one step; a panel edit appears
in the drawer; a syntax error marks the right line and keeps the last
chart; opening a broken file opens the drawer at the error and fixing it
draws the chart and enables Edit; the fallback (bundle removed) still
edits.

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

## 13. Narrow screens, keyboard, accessibility

**Goal:** the app is usable on a phone (viewing fully, editing in a basic
form) and by keyboard.

**Deliverables:** responsive shell (menus collapse under 720 px); in Edit
under 720 px the strip is an overlay toggled by a Panel button
(`workspace_layout` overlay mode, native unchanged); Keyboard shortcuts
dialog; labels and focus order for every control; the editor canvas'
label pointing to the drawer.

**Tests**: native `test_editor_layout` cases for the overlay mode;
browser at 390 x 844 with touch: a pinch zooms the viewer, Edit shows the
figure with the Panel button, the panel overlay edits a title; an axe-core
scan of the start and document screens has no serious violations; every
menu works with the keyboard alone. By hand: a real phone (iOS Safari,
Android Chrome).

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

## Completion Criteria

- Every test above passes in CI (native on Linux, macOS, MSVC and MSYS2;
  web tests in the web job and in `pages.yml`); PNG output and the viewer's
  API unchanged; the viewer within its size budget.
- On the deployed site, by hand: open a YAML from the computer, a PNG by
  drag and drop, a file from a GitHub URL, a shared `#y=` link; edit each
  (select, toolbar, add a node, edit a table, the drawer) and save in place
  (Chrome) and as a download (Firefox); reload with unsaved changes and
  restore; with the owner's Google keys, open and save a Drive file;
  install the PWA and open a file offline; view and edit a title on a
  phone.
- `nymph view --edit` behaves as before on Linux and Windows.
- `spec/INDEX.md`, `README.md`, `docs/web-app.md`, `docs/viewer.md` and
  `CHANGELOG.md` describe what was built.
