# nymph — Web App: Design

**Status: built** (plan steps 1-14; see [PLAN.md](PLAN.md) for each
step's result). Tested on Linux: native GoogleTest (the workspace and the
source pane headless, the window on Xvfb by sight), the editor module in
Node, the editor and the app in Chromium through Playwright (WebGL2 through
SwiftShader). Not yet run: Firefox, Safari, a phone, a real Google Drive,
the installed app opening a file from the system, MSVC / MSYS2 / macOS
builds of the new code (CI first). [PLAN.md](PLAN.md) is the
implementation plan. This document describes only what this project
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
  recent files and draft recovery, light / dark theme, installable and
  offline (PWA).
- The editor in the browser: `editor_panel` and the editor's host logic
  compiled to WebAssembly, with a JavaScript API (`nymph-editor.js`).
- A **source pane** in the editor: the YAML text in an ImGui code editor
  ([ImGuiColorTextEdit](https://github.com/goossens/ImGuiColorTextEdit)),
  kept in sync with the figure and the panel, in the browser and in
  `nymph view --edit` alike (section 5.4). It is also how a file that does
  not bind gets repaired.
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
the MCP App (section 12 lists the hooks); Dropbox and OneDrive (same
interface, section 12); AI agent connectors (a later project; section 11
says what this one prepares for them); the viewer's public API, which stays as it is.

Files owned or changed:

| Path | What |
|---|---|
| `web/app/` | new: the app (HTML, CSS, ES modules, command registry, manifest, service worker, icons, templates) |
| `web/editor/` | new: `nymph_editor_web.cpp` (C exports), its CMake |
| `web/js/nymph-editor.js` | new: the editor's JavaScript API |
| `web/js/nymph-viewer.js` | exports its font loader and key mapping for `nymph-editor.js`; no API change |
| `web/CMakeLists.txt` | editor targets, `nymph-site` target, size checks |
| `src/workspace/` | new: `editor_workspace`, `workspace_host`; `editor_panel` moves here from `src/window/` |
| `src/edit/source_pane.*`, `src/edit/yaml_language.*` | new: the source pane and its YAML highlighting |
| `src/edit/editor_model.*`, `src/edit/workspace_layout.hpp` | repair state, labelled and attributed undo entries, undo groups, source-pane region and its divider |
| `extern/ImGuiColorTextEdit` | new submodule (MIT), pinned |
| `src/window/view_command.cpp` | `--edit` opens a source that does not bind in repair state |
| `src/window/viewer_window.cpp` | becomes a thin native host over `editor_workspace` |
| `src/edit/table_grid.*`, `inspector.*` | CSV import / export can be host requests (`set_host_files`); undo / redo tooltips |
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
│              (Edit: source pane | figure | panel strip, resizable)      │
└──────────────────────────────────────────────────────────────────────────┘
```

- Title: the file name (renamable for documents without a file), a dot
  while there are unsaved changes, an icon for where it came from.
- **View | Edit**: switches modules (section 4). Edit is disabled, with the
  reason as tooltip, when the browser has no WebGL2. On a mobile device it
  is not shown at all (section 3.10).
- **Source**: shows the editor's source pane (section 3.7); in View mode
  it switches to Edit first. Not shown on mobile.
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
| Source does not bind | the loader's message with line and column; "Fix in source" opens Edit in repair state (section 5.4) at that line. On mobile: "Open it on a computer to fix it", no action |
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
| `&mode=edit` | with any of the above: open in Edit (ignored on mobile: the chart opens in View) |

After opening, the address bar is replaced (`history.replaceState`) with
the route that reopens the document, or `/` for a local file. **Copy link**
gives `?url=` / `?example=` / `?drive=` when the origin has one, else the
`#y=` link; above 16 KB of fragment it warns that chat apps may cut it and
offers the file instead. A Drive link works only for people the file is
shared with, and says so.

### 3.7 Source text

The YAML text is edited in the editor module's source pane (section 5.4),
not in the page: the app has no text editor of its own. The **Source**
button toggles it (`editor.showSource()`); in View mode it first switches
to Edit, since the viewer module carries no editor code (goal 2). A file
that does not bind opens straight in Edit, in repair state, with the pane
at the error.

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
bypass it. GitHub Pages' fixed `max-age=600` is why the `.wasm` URLs
carry `?v=<commit>`: a deploy is picked up at once. The JavaScript module
URLs do not: `nymph-editor.js` imports `../nymph-viewer.js`, and a second
URL for it would load a second copy (and a second `NymphError` class).
The worker is registered as `sw.js?editor=0|1`, so the mobile page never
precaches the editor.

### 3.10 Mobile: view only

On a mobile device the app is a viewer: it opens, views, shares and
downloads charts, and never offers editing. The editor is built for a
mouse and a hardware keyboard (small ImGui targets, hover, right-click
menus, the divider, text input through a hidden `textarea`, which phone
keyboards handle poorly); a half-working editor is worse than none
(goal 1).

- **Mobile** is decided once, at load, by `isMobile()` in
  `app/device.js`: no fine pointer anywhere (`not (any-pointer: fine)`), or
  a screen (not window) narrower than 720 CSS px on its short side. It
  does not change when a desktop window is made narrow, so a user is never
  thrown out of an edit by resizing.
- The shell never tests `isMobile()` directly: `device.js` turns it into
  capabilities (`directEditing: false` on mobile), and each control asks
  for the capability it needs. Agent-assisted editing (section 11) is a
  separate capability, so it can later be offered on phones, where it
  needs only the viewer module, without bringing back the editor.
- Not shown on mobile: View | Edit, Source, "Fix in source", New chart
  (templates open in View, as examples), Save in place and Save to Drive
  (nothing is ever modified). Kept: every open source, Recent, Share,
  Download YAML and PNG, theme, About.
- `&mode=edit` is ignored; a draft found on mobile (made on a desktop in
  the same browser profile, rare) is not offered there.
- `nymph-editor-core.wasm` is never fetched or prefetched on mobile, and
  the service worker does not precache it there.
- The document screen's top bar collapses into a single ⋯ menu under
  720 px of window width (on desktop too), but only mobile hides Edit.

### 3.11 Commands

Every action of the app is a **command** in one registry
(`app/commands.js`), and every button, menu item, keyboard shortcut and
route calls it there; no control changes the document any other way. A
command is an MCP tool: a `name`, a `description`, a JSON Schema
`inputSchema`, a handler returning an MCP tool result (`content`,
`structuredContent`, `isError`), and two app fields:

| Field | Values |
|---|---|
| `scope` | `app` (always), `document` (a document is open; works with either module), `editor` (Edit mode only) |
| `effect` | `read`, `edit` (changes the document, undoable), `external` (reaches outside the browser tab: opens a URL or Drive file, saves, downloads, copies to the clipboard) |

- `document` commands work on the source text and need only the viewer:
  `document_state` (name, origin, mode, modified, version, error),
  `document_source`, `document_set_source` (validated by binding in
  whichever module is loaded, refused whole with the loader's error),
  `document_check` (binds without applying), `document_image` (PNG of what
  is on screen: `toPng()` or `editor_view_image`); `document_select` is
  an `editor` command (selection means nothing in View).
- `editor` commands are the editor tools (14 section 6), registered from
  `editor.toolDefinitions()` when the editor module loads and removed when
  it is destroyed.
- `app` commands: `app_new_document` (from a source or a template),
  `app_open_url`, `app_open_example`, `app_list_templates`, `app_save`,
  `app_download`, `app_copy_link`, `app_set_mode`, `app_set_theme`, and
  the ones only the shell's controls use: `app_open_file`,
  `app_open_url_dialog`, `app_open_drive`, `app_new_dialog`,
  `app_show_source`, `app_save_as`, `app_save_drive`, `app_copy_embed`,
  `app_reset_view`, `app_set_chart_theme`, `app_close`, `app_about`,
  `app_shortcuts`. `document_check` is an `app` command (it binds in a
  throwaway viewer). The registry and the open document are
  `window.nymphApp` (tests, and later the connectors).
- `commands.call(name, args, origin)` dispatches; `origin` is `user` for
  the shell's own calls. `commands.list(filter)` returns the definitions
  in `tools/list` form, filtered by what is available now (mode, device
  capabilities, configured providers).

Built now, for three reasons: the shell has one path for every action
(tests drive it the same way), the commands are already the tool list an
agent will be given (section 11), and their `effect` is where the
confirmation rule for agents lives.

## 4. Two Modules

```
 app (web/app/) ──► nymph-viewer.js ──► nymph-viewer-core.wasm   View (≈ 650 KB gzip, unchanged)
       │                ▲ fonts, keys
       └──────────► nymph-editor.js ──► nymph-editor-core.wasm   Edit (whole library + editor)
                         │
                         └─ nymph_editor_web.cpp ─► editor_workspace ─► live_view, editor_panel
                                                                   ├─► source_pane (ImGuiColorTextEdit)
                                                                   └─► editor_tools (JS tool API)
```

- The editor module is **self-contained**: the whole library plus the
  editor. It does not load the viewer module or share code with it.
- **View → Edit**: the app shows a progress line while `nymph-editor-core`
  loads (it is prefetched with `<link rel=prefetch>` once the viewer has
  drawn and the browser is idle, never on mobile), creates
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

/// What the workspace asks of its host. Every call is on the UI thread;
/// every method has a default, so a host overrides what it needs.
class workspace_host {
 public:
  virtual void set_cursor(cursor_shape shape);
  virtual void request_save();                         ///< Save button, Ctrl+S
  virtual void request_csv_import(const std::string& table);   ///< host files only; answer with import_csv()
  virtual void request_csv_export(const std::string& table, const std::string& file_name, const std::string& csv);
  virtual void request_size(float css_width, float css_height);  ///< native: grow the window
  virtual void decided(editor_panel::decision decision);        ///< a native dialog's answer
  virtual void changed(const commit_result& r);       ///< after any accepted change: label, origin
  virtual std::string status_line(const std::string& status) const;  ///< native: "agent connected"
  virtual void need_fonts(const std::vector<std::string>& fonts);    ///< web: fetch, then retry_view()
  virtual void make_current();                         ///< before a read-back
  virtual bool has_clipboard() const;                  ///< the page's clipboard instead of ImGui's
  virtual void set_clipboard(std::string_view text);
  virtual std::string clipboard();
};

class editor_workspace {
 public:
  editor_workspace(workspace_host& host, view_options options, std::string glsl_version, workspace_gl gl);
  void init_view();                    // ImGui's OpenGL3 backend in the view; "" GLSL: headless (tests)
  void open(const std::string& source, bool reset_view, bool edit);  // edit: a source that does not bind opens in repair state
  std::string show_panel(bool on, bool repair = false);  // "" or why the source cannot be edited
  void show_source(bool on, std::optional<int> line = {});
  bool repairing() const;
  bool pending() const;                // typed text waits for its pause
  void set_host_files(bool on);        // CSV import / export as host requests (web)
  void resize(float css_width, float css_height, float scale);
  void pointer(const pointer_event& e);
  key_outcome key(window_key key, key_mods mods, bool down, bool repeat);  // consumed / unhandled
  void text(char32_t codepoint);
  std::string import_csv(const std::string& table, std::string_view csv);
  bool wants_frame() const;
  void frame(double now_seconds);      // the view's and the panel's frames when due
  void render(int pixel_width, int pixel_height);  // GL context current; the host swaps
  void apply_change(const commit_result& r, const std::string& status);  // an agent's change
  void retry_view();                   // after need_fonts()
  std::vector<uint8_t> capture_figure(int max_width, int& width, int& height, double now);
  live_view& view();
  editor_panel* panel();               // nullptr while hidden
};

} // namespace bdg::nymph
```

It holds what `viewer_window` does today between the window and the
editor: pointer routing between the figure box, the strip, the toolbar and
popups (including drags that start in one and end in another), the
divider, key routing and the editor shortcuts (undo, redo, Delete,
`Ctrl+D`, `Alt+Up/Down`, `R`, `Ctrl+U` for the source pane), clicks and
right-clicks in the figure to selections and menus, the toolbar anchor,
tab changes, the source pane and its divider, and applying
`commit_result`s to the view (`set_source_moved`). `viewer_window` keeps
the native window, the event loop, the window size, file writes (Save over
the input, `S` / `Y` copies), the "unsaved changes" dialog on close, and
the MCP server and discovery file.

**Change attribution.** `editor_model`'s undo entries become `{source,
label, origin}` instead of bare sources: `label` names the change ("Set
title", "Add FlowNode", "Edit source"), `origin` says who made it (`user`,
or `agent:<connector>`). The panel's undo and redo buttons show the label
as tooltip; `commit_result` carries both, and the status line names an
agent change by its connector, as 14 does for `nymph mcp` agents.
`begin_group(label, origin)` / `end_group()` turn several commits into one
undo entry (an agent's turn of several tool calls is undone at once); the
panel never opens a group, so nothing changes for the user's own edits.
A change whose origin differs from the open group's ends the group first
(17_agent_connectors), so a user's edit made during an agent's turn is
never undone with it.

The panel's Save button and `Ctrl+S` call `request_save()`: the native
host answers by opening the panel's own "overwrite?" modal (which stays in
`editor_panel`, as does the close dialog), the web host fires `save`. CSV
Import / Export become `request_csv_*` when the host turns on host files
(the web editor: the browser's file picker, a download); the native host
keeps `table_grid`'s ImGui file dialogs, which need a file system.

`apply()` asks the panel for more frames only when something happened (a
change, a selection, a save, an error). Before, every panel frame ended in
a refresh, so `nymph view --edit` never went idle; this was found when the
web editor did not either.

### 5.2 JavaScript API (`web/js/nymph-editor.js`)

```ts
createEditor(container: HTMLElement, options: {
  source?: string; png?: Uint8Array;  // one of them
  fileName?: string;
  theme?: "source" | "light" | "dark" | "classic" | "auto";
  panel?: boolean;                    // default true
  sourcePane?: boolean;               // default false
  repair?: boolean;                   // default false: a source that does not bind rejects;
                                      // true: opens in repair state
  wasmUrl?: string; fontBaseUrl?: string;
}): Promise<NymphEditor>;             // rejects with NymphError (bind error, no WebGL2)

class NymphEditor {
  setSource(yaml: string, opts?: { asChange?: boolean; resetView?: boolean }): Promise<void>;
  source(): string;                   // current YAML, no view
  toYaml(): string;                   // with the view on screen (`view` key)
  toPng(): Uint8Array;                // render_png(toYaml())
  modified(): boolean;  markSaved(): void;  version(): number;
  undo(): void;  redo(): void;  resetView(): void;
  setTheme(theme: string): void;  showPanel(on: boolean): void;  focus(): void;
  showSource(on: boolean, line?: number): void;  repairing(): boolean;
  tool(name: string, args?: object, opts?: { origin?: string }): object;
                                      // the editor tools of 14 section 6, synchronous; origin default "user"
  toolDefinitions(): object[];        // their tools/list entries
  beginGroup(label: string, origin: string): void;  endGroup(): void;
  importCsv(table: string, csv: string): void; // answer to "csv-import"
  on(event: "change" | "selection" | "save" | "csv-import" | "csv-export" | "error",
     cb: (detail) => void): () => void;
  destroy(): void;
}
```

- `tool()` runs `editor_tools` (14 section 6) in-process: `editor_state`,
  `editor_element`, `editor_set_fields`, `editor_add_element`, ... with the
  same JSON arguments and results, and `expected_version`. It is the
  scripting surface for the app (e.g. selecting what an error names), for
  tests, and for agents later (section 11). A tool's error is
  a thrown `NymphError` with the tool's message.
- The `change` event's detail is `{version, label, origin}`, so the shell
  (and a later assistant) knows who changed what.
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
  takes focus, placed at the field, so IME composition works; its `input`
  events become `text()`.
- **Clipboard.** Copy: `set_clipboard` calls
  `navigator.clipboard.writeText` (inside the key event's user gesture).
  Paste: the `paste` event's text is stored as the clipboard before the
  `Ctrl+V` key reaches ImGui, as the viewer does for mockups.
- **Cursor**: `set_cursor` sets the canvas' CSS cursor.
- **`editor_view_image`**: the browser `editor_host` reads the figure box
  back with `glReadPixels` right after drawing it, as the native host does,
  so the tool returns what the user sees.
- **Fonts**: the viewer's loader (exported by `nymph-viewer.js`, which
  loads no wasm by being imported). The source pane's monospace font is
  fetched the same way the first time the pane opens.

### 5.4 Source pane (`src/workspace/source_pane.*`)

The YAML text beside the figure, in both hosts. A `TextEditor` from
ImGuiColorTextEdit drawn in the panel's ImGui context (which already covers
the whole window), so its keys, clipboard, popups and theme are the
panel's.

**Place.** The **Source** button at the right end of the tab bar, or
`Ctrl+U`, puts the pane between the figure (or the data table) and the
strip, with a divider that works as the strip's (6 px hit zone, at least
200 px for the pane and 240 px for the figure, double-click restores 40 %
of the space left of the strip). Not left of the figure: the figure's
draw data fills the bottom-left of the drawable (the OpenGL3 backend's
viewport starts at x = 0), so the figure must stay at x = 0. The pane stays
while the user changes tabs, so a table edit shows its YAML.
`workspace_layout` gains the region; `figure_box()` is what remains.

**Look.** `FiraMono-Regular.ttf` from the font catalog, added to the
panel's atlas (embedded natively; fetched on demand in the web build, the
default font until it arrives). The editor's dark or light palette by the
figure theme. Line numbers, two-space tabs inserted as spaces,
indentation folding, matching brackets for flow mappings, find and
replace (`Ctrl+F`, `Ctrl+H`). `yaml_language()` is a
`TextEditor::Language` with a custom tokenizer: comments, mapping keys,
strings, numbers, `true` / `false` / `null` / `yes` / `no`, element type
names (from the registry, wherever they appear), and `$column` references in
their own colour. The lines of a `|` block (CSV data) are coloured as
ordinary tokens: the editor's colorizer works line by line and cannot tell
them apart.

**Text to model.** The editor's change callback, 300 ms after the last
keystroke, calls `editor_model::set_source(text)`: one undo step of the
document. Accepted: the figure, panel and tree follow, markers are
cleared. Refused: the loader's error is drawn as a squiggle at its line
and column plus a line marker with the message as tooltip, and in the
panel's error area; the figure keeps the last good source. The pane is then
**ahead** of the model ("not applied" in its header), and until its text
binds again the inspector, toolbar and tree menus are read-only and agent
writes are refused ("the user is editing the source"), so nothing can
replace text the user is typing. Pending pane text is applied before any
other change, as the inspector's pending text is (14 section 7).

**Model to text.** Any other accepted change (panel, toolbar, tree,
tables, agent, document undo / redo) is written into the pane as one
`ReplaceSectionText` of the span that differs (common prefix and suffix
kept), so cursor, scroll and folds outside it stay.

**Undo.** One history, the document's. `Ctrl+Z` / `Ctrl+Y` in the pane
are the model's undo and redo while the pane is in step with the model,
and the text editor's own (fine-grained) undo while it is ahead, so the
user can step back to text that binds. The pane intercepts these keys
before the `TextEditor` sees them.

**Selection.** Selecting an element (figure, tree) moves the pane's cursor
to the element's first line (`node::line`) and marks its lines; moving the
cursor in the pane selects the innermost element whose text holds the
cursor's line (after 300 ms), so the figure's outline follows the text.

**Repair state.** `editor_workspace::open()` with a source that does not
bind has no model yet: the pane is shown, cursor at the error; the figure
box shows the error (or the last good chart); the strip says "Fix the
source to edit". The first text that binds creates the model, and the
workspace becomes a normal editor. Save works in repair state for YAML
(the text as it is); a PNG input waits until it binds. Natively,
`nymph view --edit broken.yaml` now opens in repair state instead of
exiting; plain `nymph view` still reports the error and exits.

The MCP tools see the pane: `editor_state` reports whether it is shown,
ahead of the model, and its error; `editor_source` returns the model's
source.

### 5.5 Look (`src/workspace/panel_style.*`, `src/edit/panel_widgets.hpp`)

The panel, its tabs, tables, dialogs, the floating toolbar and the source
pane share one flat palette taken from the app's CSS (`web/app/app.css`:
background, surfaces, border, text, muted, accent), light or dark, in both
hosts. Sizes follow the floating toolbar, which already looked like a web
app: rounded frames (7 px) and popups (10 px), thin 1 px borders, roomy
padding, thin transparent scrollbars, no window rounding for the docked
windows. The text font is Fira Sans (ImGui's bitmap font until its bytes
arrive), the pane's Fira Mono; the web page fetches both at `createEditor()`, so they reach the atlas together (Fira Mono added later, when the pane opens, showed stale glyphs in WebGL).
Save is a primary button in the accent; section headers (Elements, All
fields) are titles, not filled bars; the figure's selection and hover
outlines use the accent.

**Who decides.** `editor_panel::set_theme()` follows the figure's theme
(dark panel only for `dark`) until `set_appearance()` pins a
`panel_appearance` {dark, accent, accent_dark}; the workspace keeps the
pinned look across panel re-creation. The web app pins it from the page:
`data-theme` or `prefers-color-scheme`, and the CSS `--accent`, again on
each theme change. The chart itself keeps its own theme (the app's
**Theme…** dialog previews another, docs/themes.md): a diagram canvas stays as authored, as in
Lucidchart or Mermaid's editor.

**Rejected.** Deriving the panel's colours from the figure's ImGui theme
(the previous look): classic and light themes are ImGui's desktop palettes,
blue and grey, and a dark page then framed a light panel.

## 6. Build

- ImGuiColorTextEdit is a submodule, `extern/ImGuiColorTextEdit`, pinned
  to a commit that matches the ImGui submodule (1.92.9), initialized with
  `nymph_ensure_submodules()` when `NYMPH_VIEW` or `NYMPH_WEB` is on; it
  builds into a `nymph_text_editor` library (`TextEditor.cpp` only; not
  `TextDiff`, not the example or extras, which the source tarball leaves
  out). The app's About dialog names it; its MIT licence travels in
  `extern/` with the source.
- `NYMPH_WEB` builds both modules. New target `nymph-editor` from
  `web/editor/` linking `nymph`, `nymph_workspace` and
  `nymph_editor_tools`, with the viewer's link flags; output in
  `<build>/web/editor/`. No single-file build of the editor.
- New libraries in `src/CMakeLists.txt`: `nymph_workspace`
  (`src/workspace/`, ImGui's OpenGL3 backend; built when `NYMPH_VIEW` or
  `NYMPH_WEB`) and `nymph_editor_tools` (`editor_tools`, `mcp/tool.hpp`
  (the tool type, `tool_error`, `text_result()`) and `base64`, over
  `nymph` and `nymph_json`, without `nymph_cli`; built with `NYMPH_MCP`,
  `NYMPH_VIEW` or `NYMPH_WEB`). `nymph_window` and `nymph_mcp` link them.
  `call_editor_tool(host, name, args, origin)` runs a tool in process;
  `editor_host::locked_reason()` refuses writes while the source pane is
  ahead.
- `nymph-site` target: assembles `<build>/web/site/` (section 3.1) from the
  module outputs, `web/app/`, `examples/`, and
  `config.json` from CMake cache variables `NYMPH_GOOGLE_CLIENT_ID`,
  `NYMPH_GOOGLE_API_KEY`, `NYMPH_GOOGLE_APP_ID` (empty by default).
- Size checks (ctest, web builds): gzip sizes of
  `nymph-viewer-core.wasm`, `nymph-editor-core.wasm` and the app shell
  against `web/size_budget.json`. The viewer's budget is its size before
  this project plus 2 %; the editor's is set in plan step 1 from the
  measured size plus 10 %. Both were re-measured once mermaid import
  ([16_mermaid_import](../16_mermaid_import/DESIGN.md), built alongside
  this project) put its converters in both modules: viewer 941,867 bytes
  (budget 960,705), editor 1,280,195 (budget 1,408,215).
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
  schema. The canvas editor costs screen-reader access and native text
  widgets (section 8); the shell around it is DOM.
- **Extract `editor_workspace`, not copy `viewer_window`.** The routing
  rules (drags across regions, toolbar over the figure, keys while a popup
  is open) took several iterations natively; two copies would drift.
- **The editor tools as the JavaScript API.** They already describe the
  editor as data (tree, fields, tables) and change it with validation and
  undo. Reusing them gives the app and tests a complete, documented API
  for the price of linking JSON into the editor module (accepted if it
  fits the budget; plan step 1 measures).
- **The source text in ImGui (ImGuiColorTextEdit), not a DOM code editor
  such as CodeMirror.** The whole editor then has one look, one keyboard
  model, one clipboard path and one undo history, and the same pane works
  in `nymph view --edit`, where there is no DOM. A DOM editor would also
  need npm and a bundler in the site build, and a second sync path for the
  native host. Costs: no screen-reader access to the text, IME through
  the hidden `textarea` like every other field, and about
  14,000 lines of C++ in the editor module (never in the viewer). Chosen
  over ImGui's `InputTextMultiline`, which has no highlighting, line
  numbers, markers, folding or search. The goossens version is maintained,
  follows ImGui 1.92, and has the marker, squiggle, section-replace and
  change-callback APIs the pane needs; it has no YAML mode, so nymph
  writes one (`yaml_language`).
- **Commands shaped as MCP tools from the start**, not plain JS
  functions later wrapped for agents. Agent connectors (section 11) need a
  complete, described, validated tool list; building the shell on it now
  costs a schema per action and avoids a second path that agents would
  use and buttons would not (or the reverse). Rejected: letting agents edit
  only by replacing the whole source, which loses attribution, undo by
  step and the editor's checks.
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
- A command whose `effect` is `external` runs for a non-`user` origin only
  after the user confirms it in a dialog naming the command and its
  arguments; `edit` commands from any origin are validated, undoable and
  attributed like the user's own. The registry enforces this, not each
  caller.
- The app never navigates away or reloads with unsaved changes without
  asking, and writes a draft before the page can be discarded.
- One editor per page; any number of viewers (the demo page).
- Browsers: current Chrome, Edge, Firefox and Safari. Features that some
  lack (File System Access, `launchQueue`, `CompressionStream` in older
  Safari) are detected and their actions hidden or replaced, never broken.
- Accessibility: every shell control is a real button or menu with a label
  and keyboard access; the viewer canvas keeps its `aria-label`; the editor
  canvas, source pane included, is not reachable by a screen reader. Its
  label says so and points to Download YAML for editing elsewhere
  (section 12).
- `extern/ImGuiColorTextEdit` is not edited (CLAUDE.md, Dependencies).
  Everything nymph-specific (language, key interception, sync) is in
  `source_pane`; a needed upstream change is proposed upstream and the
  submodule bumped.
- Code style: C++ as nymph (`bdg::nymph`, `snake_case`); JavaScript as
  `web/js/` and `extension/` (ES modules, no framework, no build step).

## 9. Testing Strategy

1. **Native GoogleTest**: `editor_workspace` with a fake host and no GL
   frames drawn (pointer routing by region, divider drag, key routing and
   shortcuts, CSV and save requests, `import_csv`); `source_pane` against
   a plain ImGui context (typing applies after the pause, a refused text
   marks its line and locks the panel, model changes patch only the
   changed span, undo routing, selection to cursor and back, repair state);
   `yaml_language` tokens; the existing editor and window tests unchanged.
2. **Node** (web builds): the editor module with `panel` frames run but
   not presented (a test-only headless flag): create, `tool()` reads and
   writes, `setSource` with and without `asChange`, `modified`, `toYaml`,
   `toPng` against `render_png`.
3. **Browser** (Playwright, Chromium with SwiftShader), against
   `<build>/web/site/` on a local server: open by file chooser, drop and
   paste; `?url=` from the local server and a CORS failure message;
   `#y=` round trip; View → Edit → View keeps the view; a click on a target
   (rectangles from `tool("editor_state")`) selects it; a panel edit marks
   the document modified and Save downloads the expected YAML; typing in
   the source pane reaches the figure, and a broken file opens in repair
   state and is fixed there; the draft
   is restored after a reload; Drive with a stub of Google's scripts served
   locally; the service worker serves the shell offline.
4. **Commands**: in the browser, every shell control and shortcut calls a
   registered command (a test hook logs `commands.call`); `commands.list()`
   gives valid JSON Schemas; an `external` command called with an `agent:`
   origin waits for the confirmation dialog and does nothing on Cancel; an
   `edit` command from an agent origin is one undo step labelled with it.
5. **Size budget** test (section 6).
6. **By hand**, recorded in the plan: real Google Drive with the owner's
   keys, Firefox and Safari, a phone (view only).

## 10. Integration Boundaries

Depends on: [04_web_viewer](../04_web_viewer/DESIGN.md) (viewer module,
JS API, fonts on demand, saved view, `live_view`),
[08_view_editor](../08_view_editor/DESIGN.md) and
[14_editor_workspace](../14_editor_workspace/DESIGN.md) (editor model,
panel, toolbar, tree and data editing, editor tools),
[01_core_features](../01_core_features/DESIGN.md) (source format,
`extract_source`), [07_release_pipeline](../07_release_pipeline/DESIGN.md)
(CI web job, Emscripten version).

Changes others see: `nymph view --edit` gains the source pane and opens
broken sources in repair state; `editor_state` reports the pane;
`editor_panel` moves to `src/workspace/` and Save /
CSV become host requests (14's design is corrected where it says the panel
opens those dialogs); `nymph-viewer.js` exports two helpers; the demo
page's examples move to `examples/` at the site root; the site's root is
the app instead of a redirect to the demo.

Risks, checked first in the plan (step 1): the editor module's size with
JSON linked in; two ImGui contexts drawn into one WebGL2 context per frame
(native does it with desktop GL); ImGuiColorTextEdit under Emscripten and
its size. Later: ImGui text input (source pane included) through a hidden
`textarea` on Safari (step 6).

## 11. Prepared for Agents

A later project adds **agent connectors**: Claude, Cursor and Gemini help
the user create and edit charts in the app ("add a node for billing and
connect it to orders", "make a Gantt chart from this table"). This project
builds nothing an agent uses directly, but fixes the parts the connectors
will stand on, so they add a chat panel and the connectors, and change no
existing code path.

### 11.1 Two kinds of connector

| Kind | How it works | Claude | Gemini | Cursor |
|---|---|---|---|---|
| **In-app** | The app calls the model's API from the page, gives it `commands.list()` as tools, and runs the tool calls it returns through `commands.call(…, "agent:<id>")` | Messages API with the user's API key (browser access header) or a later OAuth sign-in | Gemini API with the user's API key | none: Cursor has no public model API |
| **External** | An agent the user already runs reaches the open app through MCP | Claude Desktop / Claude Code through `nymph mcp`; Claude in Chrome through WebMCP | Gemini CLI through `nymph mcp`; Gemini in Chrome through WebMCP | Cursor through `nymph mcp` (12_cursor_integration) |

External connectors reach the page by one of two routes, both of which
just forward MCP `tools/list` and `tools/call` to the registry:

- **WebMCP** (`navigator.modelContext`, where the browser has it): the app
  registers `commands.list()` as the page's tools. No bridge, no setup.
- **`nymph mcp` bridge**: `nymph mcp` already forwards editor tools to a
  running `nymph view --edit` (14 section 3, `editor_bridge`). A page cannot
  accept connections, so the page connects out to a loopback WebSocket
  opened by `nymph mcp`, after the user pairs them (a code shown by the
  app and typed into, or confirmed by, the agent side), with an `Origin`
  check. The bridge then lists the app as one more editor. Its security
  design (pairing, Chrome's local network access prompt, Safari) belongs to
  the later project.

### 11.2 What this project fixes for them

1. **One tool surface.** The command registry (section 3.11) is MCP-shaped
   and complete: an agent can do anything a user can through it, and
   nothing the user cannot. The editor's own tools (14 section 6) are part
   of it, unchanged, so an agent sees the same tools in the app, in `nymph
   view --edit` and through `nymph mcp`.
2. **Agents without the editor.** `document` commands need only the viewer,
   so an assistant can create and edit charts as text (set source, check,
   look at the image) on a phone or in View mode, where the editor module
   is never loaded (section 3.10's capabilities keep that open).
3. **Attribution and undo.** Every change carries `origin` and `label`
   (section 5.1); an agent's turn is one undo group; the `change` event
   says who changed what. The user can always see and undo what an agent
   did.
4. **Concurrency.** Agents write with `expected_version` (14 section 6); a
   user typing in the source pane locks agent writes (section 5.4); the
   user's edits always win.
5. **Consent.** `external` commands from an agent need the user's
   confirmation (section 8). Sending the chart to a model provider is
   itself a user decision made when connecting, in line with goal 4.
6. **A place for the chat.** The document screen has a right-hand **dock**
   (DOM, beside the canvas, hidden while empty; the canvas follows it
   through its `ResizeObserver`), so a chat panel needs no layout change.
   The chat is DOM, not ImGui: it must work in View mode and on phones,
   where there is no ImGui editor, and it shows streamed, formatted text and
   links. The editor's own panels stay ImGui.
7. **What agents read.** The agent guide and the element reference
   (`docs/agent-guide.md`, `docs/elements/`) are copied into the site as
   `agent/` by `nymph-site`, so an in-app connector can give them to the
   model without growing either module; `config.json` has an `agents`
   section, empty here, for connector settings.

### 11.3 Left to the later project

The chat panel; each connector (API keys or sign-in, streaming, the tool
loop, model choice); the WebMCP registration; the `nymph mcp` WebSocket
bridge and pairing; prompts and the use of the agent guide; whether
assistant editing is offered on mobile.

That project is [17_agent_connectors](../17_agent_connectors/DESIGN.md).
It builds only the external kind: the in-app kind (API keys in the page)
was dropped, so no chat panel is built and `device.assistant` becomes
`device.agents`.

## 12. Extension Points

- **Dropbox and OneDrive** providers (Chooser / Saver; File Picker v8),
  each enabled by its app key in `config.json`.
- **Drive "Open with"**: registering the app in the Google Workspace
  Marketplace SDK so Drive's own menu opens files in it (`?state=`).
- **"Open in the app"** from the Chrome extension's viewer menu and from
  the MCP App, as a `#y=` link.
- **Agent connectors** for Claude, Cursor and Gemini: section 11.
- **More in the source pane**: completion of field and type names from
  the schema (the editor's autocomplete API), hover help from field
  descriptions, a diff against the saved file (`TextDiff`).
- **A screen-reader path to the text**: a DOM mirror of the pane in the
  app, read-only or editable, fed by the `change` event.
- **Editing on phones and tablets**: a touch-first layout of the editor
  (the strip as an overlay or bottom sheet, larger targets, the phone
  keyboard through the `textarea`), after which `isMobile()` stops hiding
  Edit (section 3.10).
