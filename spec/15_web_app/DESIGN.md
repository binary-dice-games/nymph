# nymph — Web App: Design

**Status: design in review.** Nothing is built. [PLAN.md](PLAN.md) is the
ordered implementation plan. This document describes only what this project
adds or changes; the viewer it builds on is
[04_web_viewer](../04_web_viewer/DESIGN.md) (*viewer DESIGN*), the editor is
[08_view_editor](../08_view_editor/DESIGN.md) and
[14_editor_workspace](../14_editor_workspace/DESIGN.md) (*editor DESIGNs*).

## 1. Purpose / Scope

GitHub Pages today publishes the viewer's demo page
(`https://binary-dice-games.github.io/nymph/demo/`) and the viewer's files
for embedding. This project turns the site's root into a **web app for
viewing and editing nymph files**, usable by people who never install the
CLI: someone receives a nymph PNG or YAML from an AI agent or a colleague,
opens it in the app, looks at it live, edits it, and saves or shares it.

The main flow:

1. The user picks a file: from the computer (file picker, drag and drop,
   paste), from a URL, from Google Drive, from a shared link, from the recent
   list, or starts from a template or an example.
2. The file opens in the **viewer** (the existing `nymph-viewer-core.wasm`):
   pan, zoom, values on hover.
3. **Edit** loads the **editor**, a second WebAssembly module
   (`nymph-editor-core.wasm`) with the editor of `nymph view --edit`: click
   to select, inspector, floating toolbar, tree editing, data tables.
4. The user saves (back to the same file where the browser allows it, to
   Drive, or as a download, YAML or PNG) or copies a share link.

In scope:

- The app: a static single-page site (`web/app/`), no framework, no server.
  Start screen, document screen, open sources, save targets, share links,
  recent files and draft recovery, source text editor, light / dark theme,
  installable and offline (PWA).
- The editor in the browser: `editor_panel` and the editor's host logic
  compiled to WebAssembly, with a JavaScript API (`nymph-editor.js`).
- A refactor that makes this possible without a second copy of the host
  logic: the platform-neutral part of `viewer_window.cpp` moves to a new
  `editor_workspace`, used by `nymph view` and by the web editor.
- Building the whole site with CMake (`nymph-site` target), so a local
  preview is exactly what Pages serves; the Pages workflow deploys it.
- Google Drive as the first cloud provider, behind a provider interface.

Not in scope: accounts or server-side storage (files stay in the browser
and the user's own storage); real-time collaboration; the editor's MCP
server in the browser (the JavaScript API exposes the same tools, section
5.2, but no agent connects to it here); changes to the Chrome extension or
the MCP App (section 11 lists the hooks); Dropbox and OneDrive (same
interface, section 11); the viewer's public API, which stays as it is.

Files owned or changed:

| Path | What |
|---|---|
| `web/app/` | new: the app (HTML, CSS, ES modules, manifest, service worker, icons, templates) |
| `web/editor/` | new: `nymph_editor_web.cpp` (C exports), its CMake |
| `web/js/nymph-editor.js` | new: the editor's JavaScript API |
| `web/js/nymph-viewer.js` | exports its font loader and key mapping for `nymph-editor.js`; no API change |
| `web/CMakeLists.txt` | editor targets, `nymph-site` target, size checks |
| `src/workspace/` | new: `editor_workspace`, `workspace_host`; `editor_panel` moves here from `src/window/` |
| `src/window/viewer_window.cpp` | becomes a thin native host over `editor_workspace` |
| `src/window/csv_dialog.*` | new: the native CSV file dialogs, moved out of `table_grid` |
| `src/edit/table_grid.*`, `inspector.*` | CSV import / export and Save become host requests |
| `src/mcp/editor_tools.*` | built in a library the web editor links without `nymph_cli` |
| `src/CMakeLists.txt` | `nymph_workspace` and `nymph_editor_tools` libraries |
| `.github/workflows/pages.yml` | builds `nymph-site`, runs the web tests, deploys |
| `docs/web-app.md` | new: user guide and the owner's one-time setup |
| `docs/viewer.md`, `README.md`, `CHANGELOG.md` | the app, the editor module |

## 2. Design Goals

1. **A product, not a demo.** Every path a user can take ends in a clear
   state: a chart, an editor, or a message saying what went wrong and what
   to do. No dead buttons (an unconfigured provider is not shown), no lost
   work (unsaved changes survive a closed tab), no surprise uploads.
2. **The viewer stays light.** `nymph-viewer-core.wasm` does not grow
   (budget: today's gzip size plus 2 %, checked in CI). All editor code lives
   in the second module, loaded only when the user edits.
3. **One editor, two hosts.** The browser editor is the native editor:
   same `editor_model`, inspector, toolbar, tree menu and table grid, same
   pointer and key routing. Host-specific code is only what a platform
   must do differently (files, clipboard, cursor, window size, the MCP
   socket).
4. **Files never leave the browser** unless the user sends them somewhere
   (Drive, a download). No analytics, no proxy, no server. Third-party
   scripts (Google) load only when the user picks that provider.
5. **Static hosting only.** Everything works from GitHub Pages' plain file
   serving (no custom headers, fixed `max-age`), and from any static host or
   `python -m http.server` on the built folder.
6. **Stable URLs for embedders.** `nymph-viewer.js`, its `.wasm`,
   `nymph-viewer.single.js`, `fonts/` and `demo/` keep their paths.

## 3. The App

### 3.1 Site layout

```
https://binary-dice-games.github.io/nymph/
  index.html, app/…            the app (web/app/), entry for every route
  manifest.webmanifest, sw.js  PWA
  config.json                  provider settings (public values), written at build time
  version.json                 {version, commit}: cache key, About
  nymph-viewer.js, nymph-viewer-core.js, nymph-viewer-core.wasm, nymph-viewer.single.js   unchanged
  fonts/                       unchanged, shared by both modules
  editor/nymph-editor.js, editor/nymph-editor-core.js, editor/nymph-editor-core.wasm
  examples/<name>.yaml|png, examples/index.json   one copy, used by the app and the demo
  demo/                        unchanged page (examples now read from ../examples/), each with "Open in the app"
```

### 3.2 Screens

**Start screen** (no document open):

- A drop zone with the main actions: **Open file**, **From URL**,
  **Google Drive** (when configured), **New chart**. Pasting anywhere opens
  the pasted YAML or PNG.
- **Recent**: the last 20 documents (name, where from, when, a thumbnail),
  each reopenable when its source still can be (section 3.5).
- **Templates**: a blank chart of each family (plot, layout, flowchart,
  class diagram, sequence diagram, Gantt chart, Sankey, UI mockup), and the
  examples gallery.
- A short line: what nymph is, "your files stay in this browser", links to
  the CLI install, the docs and GitHub.

**Document screen**:

```
┌──────────────────────────────────────────────────────────────────────────┐
│ nymph ▸ sales.yaml ●   [ View | Edit ]   Source   Save ▾   Share ▾   ⋯  │
├──────────────────────────────────────────────────────────────────────────┤
│ (banner: error / "restored unsaved changes" / "read-only: from a URL")   │
│                                                                          │
│   viewer canvas (View)  or  editor canvas: figure | panel strip (Edit)   │
│                                              (Source drawer, resizable)  │
└──────────────────────────────────────────────────────────────────────────┘
```

- Title: the file name (renamable for documents without a file), a dot
  while there are unsaved changes, an icon for where it came from.
- **View | Edit**: switches modules (section 4). Edit is disabled, with the
  reason as tooltip, when the browser has no WebGL2.
- **Source**: a drawer with the YAML text (section 3.7).
- **Save ▾**: Save (back to where it came from, when possible, else the
  same as Download YAML), Download YAML, Download PNG, Save to Drive.
  `Ctrl+S` is Save.
- **Share ▾**: Copy link, Copy embed code (`<nymph-viewer>` snippet with
  the script URL of this site), Download.
- **⋯**: theme (auto / light / dark), Reset view, Keyboard shortcuts,
  About (version, commit, licences), Close document.

**Messages**, as a banner over the canvas, each with its action:

| Case | Message and action |
|---|---|
| Source does not bind | the loader's message with line and column; "Fix in source" opens the drawer at that line. Edit stays disabled until it binds |
| PNG without a nymph chart | "This image has no nymph chart in it (it may have been re-encoded by the site it came from). Ask for the YAML, or the original PNG." |
| URL refused by CORS / network | "That server does not let other sites read the file. Download it and open it here." with the URL |
| HTTP error, too large, not a nymph file | status code / size limit / what was found |
| Font could not be loaded | the viewer's error, with Retry |
| Unsaved changes on open, close or leave | dialog: Save, Discard, Cancel (and the browser's own prompt on tab close) |
| Draft found | "Restore unsaved changes from 10:42?" Restore / Discard |
| New app version | "A new version is ready" Reload |

### 3.3 Documents

The app holds one **document** at a time (JavaScript, `app/document.js`):

```js
{ id, name, origin, source, savedSource, view, mode: "view" | "edit" }
origin: { kind: "local" | "url" | "drive" | "link" | "example" | "template" | "paste",
          handle?, url?, fileId?, revision? }
```

- `source` is the YAML text; a PNG input is reduced to its YAML on open
  (`extract_source`), and remembered as PNG so Save writes a PNG back.
- In View mode the app owns `source`. In Edit mode the editor owns it and
  the app copies it on every `change` event (`editor.source()`).
- **Modified** means `source` differs from `savedSource`, comparing the
  sources without their `view` key: panning and zooming alone never asks
  "save changes?", but a save includes the view on screen (`toYaml()`), as
  `nymph view` does.

### 3.4 Open sources

Every source ends in `open(bytes, name, origin)`, which sniffs the content
(PNG signature, else UTF-8 text), applies a 16 MB limit, and shows it.

| Source | How |
|---|---|
| File picker | `showOpenFilePicker()` where available (Chromium: keeps a handle for Save in place), else `<input type=file>`; `.yaml`, `.yml`, `.png` |
| Drag and drop | on the whole window, with an overlay; the first file |
| Paste | `paste` event: text is YAML, an image file is a PNG (clipboard images are usually re-encoded, so the "no nymph chart" message is likely, and says so) |
| URL | a dialog, or `?url=` (section 3.6). `fetch` with `credentials: "omit"`, http(s) only. GitHub `blob/` and gist page URLs are rewritten to their raw URLs (which allow CORS) |
| Google Drive | section 3.8 |
| Recent | section 3.5 |
| Template / example | `app/templates/*.yaml`, `examples/` |
| OS "Open with" | the installed PWA's `file_handlers` for `.yaml`, `.yml` (`launchQueue`); `.png` is not claimed |

### 3.5 Recent files and drafts

Kept in IndexedDB (`nymph-app` database), in this browser only, with a
"Clear" action on the start screen:

- **recent**: name, origin, last opened, a 160 px thumbnail (from
  `toPng()` of the viewer, scaled), and how to reopen it: the
  `FileSystemFileHandle` (permission asked again on use), the URL, the Drive
  file id, or for other origins a copy of the source when it is under
  1 MB. Entries that cannot be reopened are shown greyed with the reason.
- **drafts**: the source of a modified document, written two seconds after
  the last change and on `visibilitychange`, removed on save or discard.
  Opening a document with a newer draft (same recent entry) offers to
  restore it. A draft is the one thing that makes "no lost work" true for a
  closed tab or a crash.

### 3.6 Links

| Route | Opens |
|---|---|
| `/` | start screen |
| `?url=<url>` | that URL (read-only origin: Save is Download) |
| `?example=<name>` | an example |
| `?drive=<fileId>` | a Drive file (asks the user to sign in) |
| `#y=<data>` | a shared source: `deflate-raw` (`CompressionStream`), base64url. In the fragment, so it never reaches a server |
| `&mode=edit` | with any of the above: open in Edit |

After opening, the address bar is replaced (`history.replaceState`) with
the route that reopens the document, or `/` for a local file. **Copy link**
gives `?url=` / `?example=` / `?drive=` when the origin has one, else the
`#y=` link; above 16 KB of fragment it warns that chat apps may cut it and
offers the file instead. A Drive link works only for people the file is
shared with, and says so.

### 3.7 Source drawer

The YAML text in a code editor: CodeMirror 6 with its YAML mode, line
numbers, search, the loader's error underlined at its line and column. It
is bundled from npm at build time (section 6) and loaded the first time the
drawer opens, so the start screen does not pay for it.

- Typing re-binds after 300 ms without a keystroke. In View mode the viewer
  shows the new source; in Edit mode it goes in as
  `editor.setSource(text, {asChange: true})`, one undo step in the editor.
  A source that does not bind keeps the last good chart and shows the
  error; nothing else changes.
- An edit made in the editor (panel, toolbar, undo) replaces the drawer's
  text, keeping the cursor line when the text around it is the same.
- It is also the repair path: a file that does not bind opens with the
  drawer at the error, and the accessible way to edit (the editor canvas is
  not reachable by a screen reader, section 8).

### 3.8 Google Drive

A **provider** (`app/providers/<name>.js`) has `available()`, `open()` (a
picker, resolves to bytes, name and origin), `save(document, bytes)` (back
to the same file), `saveAs(document, bytes, name)`, and `label`/`icon`.
Drive is the first:

- Sign-in with Google Identity Services (token client, scope
  `drive.file`: the app sees only files the user picks with it or creates
  with it, and the scope needs no security review). The token stays in
  memory, never stored.
- Open: Google Picker (YAML and PNG files), then `files.get?alt=media`.
- Save: `PATCH upload/drive/v3/files/{id}?uploadType=media` with the
  revision the app read (`If-Match` on the file's `headRevisionId`; a
  conflict asks Overwrite / Save as a copy). Save as: multipart create in a
  folder chosen with the Picker.
- Its scripts (`accounts.google.com/gsi/client`, `apis.google.com/js/api.js`)
  are loaded only when the user clicks a Drive action.
- `config.json` carries the OAuth client id, the browser API key and the
  Cloud project number, from repository variables at build time. Missing
  values hide every Drive action. These values are public by design
  (restricted to the site's origin in the Cloud console);
  `docs/web-app.md` has the owner's setup.

### 3.9 Offline and updates

The service worker (`sw.js`) caches the app shell and both modules on
install, and fonts and examples as they are fetched, keyed by
`version.json`'s commit. A new version installs in the background; the app
offers to reload, never reloads by itself (it could lose an open edit). The
service worker serves only same-origin GET requests; URL and Drive fetches
bypass it. GitHub Pages' fixed `max-age=600` is why module URLs carry
`?v=<commit>`: a deploy is picked up at once, without stale `.wasm` next to
a new loader.

## 4. Two Modules

```
 app (web/app/) ──► nymph-viewer.js ──► nymph-viewer-core.wasm   View (≈ 650 KB gzip, unchanged)
       │                ▲ fonts, keys
       └──────────► nymph-editor.js ──► nymph-editor-core.wasm   Edit (whole library + editor)
                         │
                         └─ nymph_editor_web.cpp ─► editor_workspace ─► live_view, editor_panel
                                                                   └─► editor_tools (JS tool API)
```

- The editor module is **self-contained**: the whole library plus the
  editor. It does not load the viewer module or share code with it.
- **View → Edit**: the app shows a progress line while `nymph-editor-core`
  loads (it is prefetched with `<link rel=prefetch>` once the viewer has
  drawn and the browser is idle, on screens at least 720 px wide), creates
  the editor with the viewer's source and view, then destroys the viewer.
  Zoom and pan carry over through the `view` key (viewer DESIGN, "Saved
  view"); the editor treats them as the starting view, not as an edit.
- **Edit → View**: the viewer is created from `editor.toYaml()`, then the
  editor is destroyed (its WebGL context and memory freed). Undo history
  does not survive the switch; the app says so in the toggle's tooltip.
- Both modules fetch fonts from the same `fonts/` URLs, so the second
  fetch comes from the browser's cache; registration happens in each.

## 5. The Editor Module

### 5.1 `editor_workspace` (`src/workspace/`)

The platform-neutral host logic now inside `viewer_window.cpp`, as a class
both hosts drive. It owns the `live_view` and, while the panel is shown,
the `editor_panel`; it takes input in CSS pixels and draws with the
OpenGL3 backend into the current GL context.

```cpp
namespace bdg::nymph {

/// What the workspace asks of its host. Every call is on the UI thread.
class workspace_host {
 public:
  virtual ~workspace_host() = default;
  virtual void set_cursor(cursor_shape shape) = 0;
  virtual void set_clipboard(std::string_view text) = 0;
  virtual std::string clipboard() = 0;                 ///< last known text; may be stale on the web
  virtual void request_save() = 0;                     ///< Save button, Ctrl+S
  virtual void request_csv_import(const std::string& table) = 0;   ///< answer with import_csv()
  virtual void request_csv_export(const std::string& table, std::string file_name, std::string csv) = 0;
  virtual void request_size(float css_width, float css_height) {}  ///< native: grow the window
  virtual void changed(const commit_result& r) {}      ///< after any accepted change
};

class editor_workspace {
 public:
  editor_workspace(view_options options, workspace_host& host, const char* glsl_version);
  void set_source(std::string_view source, bool reset_view);  // throws nymph::error, fonts_needed
  void show_panel(bool on);            // creates / destroys the editor_panel; throws for an invalid source
  void resize(float css_width, float css_height, float scale);
  void pointer(const pointer_event& e);
  bool key(window_key key, key_mods mods, bool down, bool repeat);   // true: consumed
  void text(char32_t codepoint);
  void import_csv(const std::string& table, std::string_view csv);  // answer to request_csv_import
  bool wants_frame() const;
  bool frame(double now_seconds);      // runs the view's and the panel's frames; false: nothing to draw
  void render();                       // GL context current
  bool wants_keyboard() const;         // a panel or figure text field has the keyboard
  live_view& view();
  editor_panel* panel();               // nullptr while hidden
};

} // namespace bdg::nymph
```

It holds what `viewer_window` does today between the window and the
editor: pointer routing between the figure box, the strip, the toolbar and
popups (including drags that start in one and end in another), the
divider, key routing and the editor shortcuts (undo, redo, Delete,
`Ctrl+D`, `Alt+Up/Down`, `R`), clicks and right-clicks in the figure to
selections and menus, the toolbar anchor, tab changes, and applying
`commit_result`s to the view (`set_source_moved`). `viewer_window` keeps
the native window, the event loop, the window size, file writes (Save over
the input, `S` / `Y` copies), the "unsaved changes" dialog on close, and
the MCP server and discovery file.

Save in the panel stops opening the panel's own "overwrite?" modal; it
calls `request_save()`, and the native host opens the same modal (moved
next to the close dialog), the web host fires `save`. CSV Import / Export
become `request_csv_*`: the native host opens the existing ImGui file
dialogs (moved to `src/window/csv_dialog.*`), the web host opens the
browser's file picker or downloads the file.

### 5.2 JavaScript API (`web/js/nymph-editor.js`)

```ts
createEditor(container: HTMLElement, options: {
  source?: string; png?: Uint8Array;  // one of them
  fileName?: string;
  theme?: "source" | "light" | "dark" | "classic" | "auto";
  panel?: boolean;                    // default true
  wasmUrl?: string; fontBaseUrl?: string;
}): Promise<NymphEditor>;             // rejects with NymphError when the source does not bind

class NymphEditor {
  setSource(yaml: string, opts?: { asChange?: boolean; resetView?: boolean }): Promise<void>;
  source(): string;                   // current YAML, no view
  toYaml(): string;                   // with the view on screen (`view` key)
  toPng(): Uint8Array;                // render_png(toYaml())
  modified(): boolean;  markSaved(): void;  version(): number;
  undo(): void;  redo(): void;  resetView(): void;
  setTheme(theme: string): void;  showPanel(on: boolean): void;  focus(): void;
  tool(name: string, args?: object): object;   // the editor tools of 14 section 6, synchronous
  importCsv(table: string, csv: string): void; // answer to "csv-import"
  on(event: "change" | "selection" | "save" | "csv-import" | "csv-export" | "error",
     cb: (detail) => void): () => void;
  destroy(): void;
}
```

- `tool()` runs `editor_tools` (14 section 6) in-process: `editor_state`,
  `editor_element`, `editor_set_fields`, `editor_add_element`, ... with the
  same JSON arguments and results, and `expected_version`. It is the
  scripting surface for the app (e.g. the drawer's error navigation), for
  tests, and for a later agent connection (section 11). A tool's error is
  a thrown `NymphError` with the tool's message.
- `asChange: true` is one undo step (`editor_model::set_source`); without
  it the source replaces the document (new undo history, not modified).
- The module needs WebGL2: `createEditor` rejects with a clear error
  otherwise; there is no software presenter in the editor.

### 5.3 Browser host (`web/editor/nymph_editor_web.cpp`, `nymph-editor.js`)

- **Drawing.** The editor draws straight into its own visible canvas with
  its own WebGL2 context (one editor per page), as `nymph view` draws to
  its window: no hidden shared canvas, no copy (the viewer needs those for
  many charts per page; the editor is one large canvas redrawn often).
  Frames run on `requestAnimationFrame` only while `wants_frame()`.
- **Size.** The canvas fills its container (`ResizeObserver`); the
  workspace gets the CSS size and `devicePixelRatio`. The figure follows
  the box (`fit: fill`), as in `nymph view`; `request_size` is ignored.
- **Pointer** as the viewer (pointer capture, wheel only over what takes
  it, pinch to zoom), routed by the workspace.
- **Keyboard and text.** The canvas is focusable. Keys use the viewer's
  key mapping; `preventDefault()` only when the workspace consumed the key
  (so `Ctrl+D` duplicates instead of bookmarking, but the browser keeps
  `Ctrl+T`). While a text field has the keyboard, a hidden `textarea`
  takes focus, placed at the field, so IME composition and phone keyboards
  work; its `input` events become `text()`.
- **Clipboard.** Copy: `set_clipboard` calls
  `navigator.clipboard.writeText` (inside the key event's user gesture).
  Paste: the `paste` event's text is stored as the clipboard before the
  `Ctrl+V` key reaches ImGui, as the viewer does for mockups.
- **Cursor**: `set_cursor` sets the canvas' CSS cursor.
- **Fonts**: the viewer's loader (exported by `nymph-viewer.js`, which
  loads no wasm by being imported).

## 6. Build

- `NYMPH_WEB` builds both modules. New target `nymph-editor` from
  `web/editor/` linking `nymph`, `nymph_workspace` and
  `nymph_editor_tools`, with the viewer's link flags; output in
  `<build>/web/editor/`. No single-file build of the editor.
- New libraries in `src/CMakeLists.txt`: `nymph_workspace`
  (`src/workspace/`, ImGui's OpenGL3 backend; built when `NYMPH_VIEW` or
  `NYMPH_WEB`) and `nymph_editor_tools` (`editor_tools` and the tool
  types it needs, over `nymph` and `nymph_json`, without `nymph_cli`).
  `nymph_window` and `nymph_mcp` link them.
- `nymph-site` target: assembles `<build>/web/site/` (section 3.1) from the
  module outputs, `web/app/`, `examples/`, the CodeMirror bundle, and
  `config.json` from CMake cache variables `NYMPH_GOOGLE_CLIENT_ID`,
  `NYMPH_GOOGLE_API_KEY`, `NYMPH_GOOGLE_APP_ID` (empty by default).
- CodeMirror: `web/app/package.json` and `package-lock.json` pin it;
  `nymph-site` runs `npm ci` and `esbuild` into `site/app/vendor/` when
  `npm` is found, and otherwise the drawer falls back to a plain text area
  (with line numbers and the error line marked), so a build without Node
  still produces a working site.
- Size checks (ctest, web builds): gzip sizes of
  `nymph-viewer-core.wasm`, `nymph-editor-core.wasm` and the app shell
  against `web/size_budget.json`. The viewer's budget is its size before
  this project plus 2 %; the editor's is set in plan step 1 from the
  measured size plus 10 %.
- `pages.yml` builds `nymph-site`, runs the Node and browser web tests, and
  deploys `<build>/web/site/`. Repository variables feed the Google cache
  variables.

## 7. Design Decisions

- **The editor in a second module, not in the viewer.** The viewer is
  embedded in chats and pages where most charts are only looked at; it
  must stay small (goal 2). Rejected: Emscripten dynamic linking (a main
  module with the library and a side module with the editor). A main module
  must keep and export every symbol a side module might use, which stops
  the linker from dropping dead code and makes the viewer larger; dynamic
  linking also slows calls and is less tested. Cost of the choice: the
  editor downloads the library a second time (once per version, then
  cached).
- **Edit is a module switch, not both at once.** Keeping the viewer
  alive under the editor would double memory and need two copies of the
  view state. The `view` key already carries zoom and pan between them.
- **The ImGui editor in a canvas, not a DOM rewrite.** The native editor
  is a large, tested piece (inspector, toolbar, tree menu, tables, number
  box); a DOM version would be a second editor to keep in step with the
  schema. The canvas editor costs accessibility and native text widgets;
  the source drawer (real DOM) covers both, and the shell around it is
  DOM.
- **Extract `editor_workspace`, not copy `viewer_window`.** The routing
  rules (drags across regions, toolbar over the figure, keys while a popup
  is open) took several iterations natively; two copies would drift.
- **The editor tools as the JavaScript API.** They already describe the
  editor as data (tree, fields, tables) and change it with validation and
  undo. Reusing them gives the app and tests a complete, documented API
  for the price of linking JSON into the editor module (accepted if it
  fits the budget; plan step 1 measures).
- **CodeMirror bundled at build time, not a CDN.** A CDN would be a
  third-party request on every visit (goal 4) and an availability risk.
  Not a submodule either: it is a set of npm packages; a pinned lockfile is
  the reproducible form. The text-area fallback keeps Node optional.
- **`drive.file` scope.** Full Drive scopes need Google's security review
  and show a frightening consent screen; `drive.file` limits the app to
  files the user hands it, which is all it needs.
- **Shared links in the fragment.** The source in `#y=` needs no server
  and is not sent to GitHub's servers or logs. Large charts get a warning
  rather than a server-side short link.
- **No proxy for URLs.** A proxy would see users' files and needs a
  server. CORS failures get an explanation instead.
- **Save in place only where the browser gives a handle.** Chromium's File
  System Access API can write back to the opened file; elsewhere Save is a
  download, and the Save menu says which it will do.
- **One app at the root.** The site's root becomes the product; the demo
  stays at `demo/` for links already shared.

## 8. Constraints and Invariants

- The viewer module, its JavaScript API and its URLs do not change
  (goal 6); the viewer's gzip size stays within its budget.
- Native `nymph view` and `nymph view --edit` behave exactly as before the
  refactor (existing tests, plus the native workspace tests, plus the
  window driven on Xvfb as in 14).
- The editor module touches the DOM only through `nymph-editor.js`; the
  wasm does no I/O (viewer DESIGN goal 5): files, URLs and Drive are the
  app's job, bytes go in and out.
- Untrusted input: file contents are never put in the page as HTML
  (`textContent` only); file names are shown as text; URL fetches omit
  credentials and follow only http(s). A Content Security Policy in
  `index.html` allows `'self'`, `'wasm-unsafe-eval'`, and the Google origins
  only for the Drive provider; `connect-src` must allow any https origin for
  URL opening.
- The app never navigates away or reloads with unsaved changes without
  asking, and writes a draft before the page can be discarded.
- One editor per page; any number of viewers (the demo page).
- Browsers: current Chrome, Edge, Firefox and Safari. Features that some
  lack (File System Access, `launchQueue`, `CompressionStream` in older
  Safari) are detected and their actions hidden or replaced, never broken.
- Accessibility: every shell control is a real button or menu with a label
  and keyboard access; the viewer canvas keeps its `aria-label`; the editor
  canvas is labelled as a visual editor and points to the Source drawer.
- Code style: C++ as nymph (`bdg::nymph`, `snake_case`); JavaScript as
  `web/js/` and `extension/` (ES modules, no framework, no build step
  except the CodeMirror bundle).

## 9. Testing Strategy

1. **Native GoogleTest**: `editor_workspace` with a fake host and no GL
   frames drawn (pointer routing by region, divider drag, key routing and
   shortcuts, CSV and save requests, `import_csv`); the existing editor and
   window tests unchanged.
2. **Node** (web builds): the editor module with `panel` frames run but
   not presented (a test-only headless flag): create, `tool()` reads and
   writes, `setSource` with and without `asChange`, `modified`, `toYaml`,
   `toPng` against `render_png`.
3. **Browser** (Playwright, Chromium with SwiftShader), against
   `<build>/web/site/` on a local server: open by file chooser, drop and
   paste; `?url=` from the local server and a CORS failure message;
   `#y=` round trip; View → Edit → View keeps the view; a click on a target
   (rectangles from `tool("editor_state")`) selects it; a panel edit marks
   the document modified and Save downloads the expected YAML; the drawer's
   edit reaches the editor and an invalid edit shows its error; the draft
   is restored after a reload; Drive with a stub of Google's scripts served
   locally; the service worker serves the shell offline.
4. **Size budget** test (section 6).
5. **By hand**, recorded in the plan: real Google Drive with the owner's
   keys, Firefox and Safari, a phone.

## 10. Integration Boundaries

Depends on: [04_web_viewer](../04_web_viewer/DESIGN.md) (viewer module,
JS API, fonts on demand, saved view, `live_view`),
[08_view_editor](../08_view_editor/DESIGN.md) and
[14_editor_workspace](../14_editor_workspace/DESIGN.md) (editor model,
panel, toolbar, tree and data editing, editor tools),
[01_core_features](../01_core_features/DESIGN.md) (source format,
`extract_source`), [07_release_pipeline](../07_release_pipeline/DESIGN.md)
(CI web job, Emscripten version).

Changes others see: `editor_panel` moves to `src/workspace/` and Save /
CSV become host requests (14's design is corrected where it says the panel
opens those dialogs); `nymph-viewer.js` exports two helpers; the demo
page's examples move to `examples/` at the site root; the site's root is
the app instead of a redirect to the demo.

Risks, checked first in the plan (step 1): the editor module's size with
JSON linked in; two ImGui contexts drawn into one WebGL2 context per frame
(native does it with desktop GL); ImGui text input through a hidden
`textarea` on Safari and phones (step 5).

## 11. Extension Points

- **Dropbox and OneDrive** providers (Chooser / Saver; File Picker v8),
  each enabled by its app key in `config.json`.
- **Drive "Open with"**: registering the app in the Google Workspace
  Marketplace SDK so Drive's own menu opens files in it (`?state=`).
- **"Open in the app"** from the Chrome extension's viewer menu and from
  the MCP App, as a `#y=` link.
- **Agents in the browser**: the `tool()` API behind a `postMessage` or
  WebMCP bridge, so a chat app can drive the open editor as `nymph mcp`
  drives `nymph view --edit`.
- **Phone editing beyond the basics**: plan step 13 makes the strip an
  overlay under 720 px; a touch-first panel layout (larger targets, a
  bottom sheet) is later work.
