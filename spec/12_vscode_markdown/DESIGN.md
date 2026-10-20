# nymph — VS Code Markdown Preview: Design

**Status: design only; nothing is built.** [PLAN.md](PLAN.md) is the
ordered implementation plan; its step 1 checks the facts marked
*unverified* before code depends on them. This document describes only
what this project adds or changes. It builds on
[spec/04_web_viewer/DESIGN.md](../04_web_viewer/DESIGN.md) (*viewer
DESIGN*: the WebAssembly module, its JavaScript API, fonts on demand) and
borrows ideas from [spec/05_browser_extension/DESIGN.md](../05_browser_extension/DESIGN.md)
(*extension DESIGN*: cover until the first frame, viewer pool) and
[spec/10_gemini_extension/DESIGN.md](../10_gemini_extension/DESIGN.md)
(*Gemini DESIGN*: charts from code blocks, error panels).

## 1. Purpose / Scope

A Visual Studio Code extension that makes a fenced code block labelled
`nymph` in a Markdown file show as a chart in VS Code's built-in Markdown
preview, the way the mermaid extension does for `mermaid` blocks:

````markdown
```nymph
root:
  type: Plot
  title: Revenue
  children:
    - {type: PlotLine, label: revenue, ys: [12, 15, 11, 18]}
```
````

### The constraint that shapes the design

The mermaid extension draws in the preview with a preview script, because
mermaid is plain JavaScript. nymph's renderer is C++ compiled to
WebAssembly, and the preview's Content Security Policy (VS Code `main`,
`extensions/markdown-language-features/src/preview/documentRenderer.ts`,
read 2026-10-20) does not allow it in the default setting:

| Preview security setting | CSP | WebAssembly | `fetch` (wasm, fonts) | `<iframe>` | nonce'd scripts |
|---|---|---|---|---|---|
| Strict (default) | `default-src 'none'; img-src 'self' <webview> https: data:; script-src 'nonce-…'; style-src …; font-src …` | blocked (no `'wasm-unsafe-eval'`) | blocked (no `connect-src`) | blocked | allowed |
| Allow insecure content / local content | the same plus `http:` images | blocked | blocked | blocked | allowed |
| Disable | none | allowed | allowed | allowed | allowed |

Two things are allowed under every setting: `data:` images and the
extension's own preview scripts (they carry the page's nonce). The design
uses both:

1. **A static PNG first.** The extension host (Node.js, no CSP) renders
   each block with the WebAssembly core and puts the PNG in the HTML as a
   `data:` image. It shows at once, costs the preview nothing, and is
   what exports and copies carry.
2. **Live on click, from a JavaScript build of the same C++.** Emscripten
   can compile the library to plain JavaScript (`-sWASM=0`, wasm2js)
   instead of WebAssembly. That build needs no `'wasm-unsafe-eval'` and
   no fetch: a small preview script injects it, with the page's nonce, the
   first time the user clicks a chart, and the image becomes the live
   viewer (pan, zoom, values on hover). A rewrite of the renderer in
   JavaScript is rejected (Decision "Compiled, not rewritten").

### In scope

- The Markdown preview: `nymph` fences become nymph PNGs (static, in the
  editor's theme) or an error panel; a click makes a chart live, under
  every security setting.
- The JavaScript build of the viewer (`nymph-viewer.classic.js`): a
  single classic script, no WebAssembly, no `eval`, fonts embedded.
- The editor: diagnostics at the right line for broken nymph blocks, YAML
  highlighting inside them, a CodeLens "Save PNG…" above each.
- A live panel beside the editor that follows the block while it is
  edited and can write the user's view back into it (optional, last
  feature step).
- Packaging: a `.vsix` built by CMake, attached to releases, published to
  the VS Code Marketplace and Open VSX (gated on the owner's tokens, as
  the Chrome Web Store in
  [spec/07_release_pipeline/DESIGN.md](../07_release_pipeline/DESIGN.md)).

### Out of scope

- vscode.dev / github.dev (the web extension host is a web worker with
  its own rules; a later step if asked).
- Markdown cells of notebooks (a different renderer API,
  `vscode.markdown-it-renderer`; a later addition).
- Guessing: `yaml` blocks are never drawn, even if they are nymph
  sources (Decision "Explicit label").
- Editing charts beyond writing the user's view back into the block (the
  view editor of spec/08 is native only).
- Any change to the source format; the viewer's JavaScript API is
  unchanged, only packaged a second way (section 4.5).

### New and changed files

| Path | What |
|---|---|
| `vscode/package.json.in` | The extension manifest (version filled in by CMake) |
| `vscode/src/extension.js` | `activate()`: loads the renderer, registers everything, returns `extendMarkdownIt` |
| `vscode/src/fence.js` | The markdown-it plugin and the pure `renderBlock()` |
| `vscode/src/renderer.js` | `NymphRenderer`: the WebAssembly core in Node, synchronous render and check, cache, fonts |
| `vscode/src/blocks.js` | Finds `nymph` fences in a document's text (shared by diagnostics, CodeLens, the live panel) |
| `vscode/src/diagnostics.js`, `vscode/src/lens.js` | Editor diagnostics and CodeLens |
| `vscode/preview/nymph-preview.js`, `nymph-preview.css` | The preview script (the click-to-live loader, section 3.4) and styles |
| `vscode/src/live-panel.js`, `vscode/media/live.js`, `live.css` | The live panel: host side and webview side |
| `vscode/syntaxes/nymph-injection.json` | TextMate injection: YAML inside `nymph` fences |
| `vscode/CMakeLists.txt`, `cmake/NymphVsix.cmake` | Staging and the `.vsix` zip (`NYMPH_VSCODE`) |
| `vscode/tests/node/run.mjs`, `vscode/tests/vscode/run.mjs`, `vscode/tests/browser/` | Node tests; tests in a real VS Code; Playwright tests of the preview script and the panel |
| `web/nymph_web.cpp` | Two stateless exports: `nymph_render_png`, `nymph_check` (section 5.4) |
| `web/CMakeLists.txt`, root `CMakeLists.txt` | `NYMPH_WEB_JS`: the JavaScript build (section 4.5) |
| `web/js/nymph-viewer.js` | No `import.meta` outside one replaceable line (section 4.5) |
| `docs/vscode.md` | Install and use (created with the first working build) |

## 2. Design Goals

1. **Works with the preview's default security.** Charts show, and go
   live, under the Strict setting; the extension never asks the user to
   loosen it.
2. **Same source, same picture.** A block draws exactly what
   `nymph chart.yaml --rasterizer software` draws (with the theme
   override of section 3.2), static or live, and the image is a nymph PNG
   carrying the block's YAML.
3. **Nothing else to install.** No `nymph` executable, no npm packages at
   run time: both builds of the core ship inside the extension.
4. **Pay only for what is used.** A preview whose charts are never
   clicked loads one small script and no renderer; typing re-renders
   only the block whose text changed.
5. **Errors where the author looks.** In the preview, in place of the
   chart, and as an editor diagnostic at the block's line and column.
6. **Untrusted documents are safe.** Opening any Markdown file runs no
   code from it, reads no file and makes no request because of it.

## 3. Behaviour

### 3.1 Which blocks

A fence (backticks or tildes) whose info string's first word is `nymph`,
case-insensitive (`nymph`, `Nymph`, `NYMPH`); the rest of the info string
is ignored. Every other fence renders as before, through the renderer
that was there before the plugin (so other plugins, such as mermaid,
keep working). A block over 256 KiB gets an error panel without being
parsed.

### 3.2 In the preview: the image

The block becomes

```html
<div class="nymph-chart code-line" data-line="12" data-nymph-index="0" data-nymph-hash="3f9a…">
  <img src="data:image/png;base64,…" width="640" height="360" alt="<description>">
</div>
```

- **Size.** The PNG is rendered at the source's `image.width` x `height`
  times `nymph.preview.scale` (default 2, for sharp text on high-DPI
  screens) and shown at the source's size, shrunk to the column by
  `max-width: 100%; height: auto`.
- **Theme.** `nymph.preview.theme`: `auto` (default: `light` or `dark`
  from `vscode.window.activeColorTheme.kind`; high contrast maps to its
  base), or `source`, `light`, `dark`, `classic`. When the color theme or
  the setting changes, the extension runs `markdown.preview.refresh`.
  The PNG's embedded YAML is the block as written, not the override.
- **Scroll sync.** The wrapper keeps the `code-line` class and
  `data-line` attribute VS Code's preview uses to sync scrolling and
  double-click-to-edit (copied from the token's attributes; a plugin that
  drops them breaks both).
- **Identity.** `data-nymph-index` is the block's ordinal among the
  document's `nymph` blocks, `data-nymph-hash` the SHA-256 (hex, first 16
  bytes) of its text; the preview script uses them to keep live viewers
  across preview updates (section 3.4).
- **`alt`.** The source's `description`, or "nymph chart".
- **Export.** The chart is ordinary HTML with a `data:` image, so
  anything that renders through VS Code's markdown-it instance (the
  `markdown.api.render` command, extensions that export the preview's
  HTML) carries it.

### 3.3 Errors

A block that fails is replaced by

```html
<div class="nymph-error code-line" data-line="12">
  <p>nymph: line 18, column 3 (document line 30): column 'month', row 1: 'Jan' is not a number</p>
  <pre><code class="language-yaml">…the block's text…</code></pre>
</div>
```

The message is the library's (the same text as the `nymph` CLI); the
position is given in the block and in the document (block line + fence
line + 1). The source stays visible below, so a broken block never hides
what was written. Nothing in the message or source is inserted unescaped.

**Diagnostics.** Independently of the preview, for every open Markdown
document with `nymph` fences, `nymph.diagnostics.enabled` (default on)
checks each block with `check()` (bind only, no rasterizing) 300 ms
after the last edit and publishes one `Error` diagnostic per broken block
at its document position, source "nymph". Closing the document clears
them.

### 3.4 In the preview: live on click

`nymph.preview.interactive`: `click` (default), `always`, `off`.

**Activation.** With `click`, a chart shows a small "live" hint on hover;
a click on it makes it live. With `always`, a chart goes live when it
scrolls into view (`IntersectionObserver`). A live chart takes the wheel
over its plots and diagrams (viewer DESIGN, "Interaction"), which is why
`click` is the default: scrolling a document must not zoom a chart the
user only scrolled past. `Esc` or a click outside returns the wheel to
the page (the chart stays live; `flags: NoInputs` semantics are not
used, the viewer simply stops receiving wheel events).

**Loading.** `nymph-preview.js` is small (a loader, under 10 KB). On the
first activation it adds a `<script>` element whose `src` is
`nymph-viewer.classic.js` next to itself (from `document.currentScript`)
and whose `nonce` is its own (`document.currentScript.nonce`), which the
Strict CSP allows (*unverified*: step 1). While it loads (several MB,
section 9) the chart shows a spinner over the image. The script is
loaded once per preview webview.

**Swap.** The viewer is created with `setPng()` from the image's own
bytes (decoded from the `data:` URL, no fetch), `fit: 'source'` (same
size as the image, so nothing moves), `menu: false` (its save items
download, which webviews do not support), and the theme from the body's
`vscode-dark` / `vscode-light` / `vscode-high-contrast` class, followed
by a `MutationObserver`. The image stays under the canvas until the
viewer's first frame (extension DESIGN, "cover until the first frame"),
then is hidden. If the viewer fails (any error, including a font the
build lacks), the image stays, with the hint "could not go live: <message>".

**Preview updates.** VS Code updates the preview's DOM after every edit
and fires `vscode.markdown.updateContent` on `window`. The script keeps
its viewers in a map keyed by `data-nymph-index` and, after each update:

1. a chart whose hash is unchanged gets its existing viewer moved back
   into the new wrapper (zoom kept);
2. a chart whose hash changed gets `setPng()` with the new image on the
   viewer of the same index (the viewer keeps the zoom of elements whose
   IDs did not change, viewer DESIGN "Updates");
3. viewers whose index no longer exists are destroyed.

Whether VS Code replaces or patches the DOM (it may diff it), and so
which of these paths runs, is *unverified*; step 1 records it.

**Pool.** At most 8 live viewers per preview; activating a ninth parks
the one least recently used back to its image (its view state is lost,
as in extension DESIGN's pool, which this mirrors in simplified form).

### 3.5 The live panel (optional)

**Opening.** The command "nymph: Open Live Chart" (the block at the
cursor) and an "Open live" CodeLens beside "Save PNG…".

**What it is.** A `WebviewPanel` beside the editor, one per block
(opening the same block again reveals its panel), titled with the chart's
title or "nymph chart". Its CSP is
`default-src 'none'; script-src ${cspSource} 'wasm-unsafe-eval'; connect-src ${cspSource}; style-src ${cspSource}; img-src ${cspSource} data: blob:`,
so it uses the faster WebAssembly build (`nymph-viewer.js`, the core and
`fonts/` from the extension's `media/`). The viewer runs with
`fit: 'width'` and the theme from the body class.

**Following the block.** The panel is bound to (document URI, block
index). On a document change (debounced 300 ms) the host finds the blocks
again (`blocks.js`); the bound block is the one with the same index, or,
if the count changed, the one whose start line is nearest the old one.
Its text goes to `setSource()`, keeping the user's zoom; a failing text
keeps the previous chart and shows the error in a status line. Closing
the document or deleting the block leaves the last chart with a "source
closed" status.

**Toolbar** (messages to the host): "Save PNG…" and "Save YAML…" (the
viewer's `toPng()` / `toYaml()`, written by the host through a save
dialog), "Reset view", and "Write view to block" (replaces the block's
text with `toYaml()` through a `WorkspaceEdit`, so the zoom is kept in
the `view` key; an ordinary edit the user can undo).

The panel is what the preview cannot give: a reflowing view while the
user types, and saving files. If the preview's live mode covers what
users want, this step can be dropped without changing anything else.

## 4. Key Abstractions

The extension is plain JavaScript (CommonJS for the extension host
entry, classic scripts in the preview, ES modules in the panel), no build
step, no npm dependencies, matching `extension/`.

### 4.1 `extension.js`

`activate(context)` is `async`: it creates the `NymphRenderer` (loading
the WebAssembly, about 100 ms, *unverified*) before returning
`{ extendMarkdownIt(md) }`, so the first preview never sees a renderer
that is not ready. VS Code's Markdown extension awaits a contributing
extension's activation before calling `extendMarkdownIt`
(*unverified*; step 1). It also registers the diagnostics, the CodeLens
provider, the commands, and the theme / configuration listeners that
call `markdown.preview.refresh`.

### 4.2 `fence.js`

`nymphFence(md, { renderer, settings })` wraps `md.renderer.rules.fence`:
for a `nymph` token it returns `renderBlock(token, renderer, settings)`,
otherwise it calls the previous rule. `renderBlock` is pure (testable
without markdown-it):

- never throws; any exception becomes an error panel;
- escapes every text it inserts (`&`, `<`, `>`, `"`, `'`);
- keeps the token's `code-line` / `data-line` attributes;
- maps error positions to document lines from `token.map`;
- numbers `nymph` blocks in document order (`data-nymph-index`; the
  counter lives in markdown-it's per-render `env`).

### 4.3 `NymphRenderer` (`renderer.js`)

The WebAssembly core (`nymph-viewer-core.js` and `.wasm`, built for
`ENVIRONMENT=web,worker,node`, viewer DESIGN section 4) loaded once per
extension host with a dynamic `import()`.

| Call | Result |
|---|---|
| `NymphRenderer.create({ coreUrl, fontDir })` | async; resolves when the module is instantiated |
| `render(source, { theme, scale })` | sync; `{ png: Uint8Array, width, height, description }` or `{ error: { line, column, message } }` |
| `check(source)` | sync; `null` or `{ error }` |

Invariants:

- **Cache.** Results (PNGs and errors) are kept in an LRU keyed by the
  SHA-256 of source, theme and scale: at most 64 entries and 32 MB. A
  preview refresh that changes one block renders one block.
- **Fonts.** On a `fonts_needed` error (viewer DESIGN, "Fonts on
  demand") the renderer reads each named font from the extension's
  `fonts/` with synchronous `fs`, registers it (the core checks size and
  SHA-256 against its catalog) and retries once. Only catalog names are
  read; a source cannot name a path.
- **No I/O for sources.** The core sees bytes only (viewer DESIGN
  decision "No I/O in wasm"); the renderer never reads a file named by a
  block.
- One module instance, called from the extension host thread only.

### 4.4 `blocks.js`

`findBlocks(text) -> [{ index, fenceLine, startLine, endLine, info,
source }]`, the same fence rules as markdown-it's (CommonMark: opening
run of 3+ backticks or tildes, up to 3 spaces of indent, closed by a run
at least as long). Used where the markdown-it token stream is not
available (diagnostics, CodeLens, the live panel). Tested against
markdown-it's output on the same fixtures in the VS Code test.

### 4.5 The JavaScript build (`nymph-viewer.classic.js`)

The same library and `web/nymph_web.cpp`, compiled by Emscripten to
JavaScript, plus the same `web/js/nymph-viewer.js` API, in one classic
(non-module) script that defines `globalThis.NymphViewer` (`createViewer`,
`NymphError`, and the `<nymph-viewer>` element). It is a second configure
of the tree, `NYMPH_WEB=ON NYMPH_WEB_JS=ON` (its own build folder), which
differs from the WebAssembly build only in:

| Option | WebAssembly build | JavaScript build | Why |
|---|---|---|---|
| Output | `-sWASM=1` | `-sWASM=0` (wasm2js) | No WebAssembly compile under the Strict CSP |
| Exceptions | `-fwasm-exceptions` | `-fexceptions` (JavaScript-based) | wasm2js does not support WebAssembly exceptions; the library reports errors by throwing |
| `eval` | default | `-sDYNAMIC_EXECUTION=0` | Strict forbids `eval` and `new Function` |
| Packaging | ES module, wasm beside it | `-sSINGLE_FILE=1 -sEXPORT_ES6=0`, API appended without `export` | Classic `<script>`, nothing to fetch |
| Fonts | none embedded, fetched on demand | all embedded (`NYMPH_EMBED_FONTS`) | No `fetch` in the preview |

Invariants:

- **One API source.** `nymph-viewer.js` is not forked. Its only
  module-only constructs (`export` statements and the `import.meta.url`
  default of `fontBaseUrl`) are confined so that a CMake step can strip
  the `export` keywords and replace the one `import.meta` line; the node
  test checks the classic file parses as a script and the module file as
  a module.
- **Same pixels.** The software rasterizer gives the same bytes in both
  builds (checked in tests); WebGL frames may differ only as the
  WebAssembly build's do.
- **No dynamic code.** The built file contains no `eval(` or
  `new Function` (checked in tests), and loading it under the Strict CSP
  raises no `securitypolicyviolation` event.

Only the VS Code extension uses it now; it is also usable on any page
whose CSP forbids WebAssembly but allows a script (a note in
`docs/viewer.md`).

## 5. Contracts

### 5.1 Manifest (`package.json`)

| Key | Value |
|---|---|
| `name`, `publisher`, `displayName` | `nymph`, the owner's publisher id (*owner*), "nymph charts" |
| `engines.vscode` | `^1.90.0` (*unverified*: the oldest version step 1 checks) |
| `main` | `./src/extension.js` |
| `activationEvents` | `onLanguage:markdown` (the Markdown extension also activates plugin contributors) |
| `capabilities.untrustedWorkspaces` | `{ "supported": true }` (goal 6: no code from the workspace runs) |
| `contributes.markdown.markdownItPlugins` | `true` |
| `contributes.markdown.previewStyles` | `["./preview/nymph-preview.css"]` |
| `contributes.markdown.previewScripts` | `["./preview/nymph-preview.js"]` (the loader only; the classic build is injected on demand) |
| `contributes.grammars` | injection `markdown.nymph.codeblock` into `text.html.markdown`, embedding `source.yaml` |
| `contributes.commands` | `nymph.savePng`, `nymph.openLive` (with the panel) |
| `contributes.configuration` | section 5.2 |

### 5.2 Settings

| Key | Default | What |
|---|---|---|
| `nymph.preview.enabled` | `true` | Draw `nymph` blocks in the preview |
| `nymph.preview.theme` | `auto` | `auto`, `source`, `light`, `dark`, `classic` |
| `nymph.preview.scale` | `2` | PNG pixels per source pixel (1-3) |
| `nymph.preview.interactive` | `click` | `click`, `always`, `off` (section 3.4) |
| `nymph.diagnostics.enabled` | `true` | Editor diagnostics for broken blocks |
| `nymph.codeLens.enabled` | `true` | CodeLenses above blocks |

The preview script reads `interactive` and `theme` from a
`data-nymph-settings` attribute the plugin puts on each chart wrapper
(preview scripts cannot read VS Code settings).

### 5.3 Live panel messages

Webview to host: `ready`, `save { kind: 'png' | 'yaml', bytes | text }`,
`writeView { yaml }`, `error { message }`. Host to webview:
`source { yaml, resetView }`, `status { text }`. Every message from the
webview is validated (type, sizes) before use.

### 5.4 Web module additions (`web/nymph_web.cpp`)

| Export | Contract |
|---|---|
| `int nymph_render_png(const char* text, int size, const char* theme, float scale)` | Renders with the software rasterizer, `theme` (`""` keeps the source's) and `scale` overriding `image.theme` / `image.scale`; the PNG embeds the source as given. Returns 1 and leaves the bytes for `nymph_png_data()` / `nymph_png_size()`, or 0 with `nymph_last_error()` (JSON `{line, column, kind, message}`, as today). `kind` `fonts_needed` names the missing fonts. |
| `int nymph_check(const char* text, int size)` | Binds only (what `nymph check` does). 1, or 0 with `nymph_last_error()`. |

Both are stateless (no viewer handle), never throw across the boundary,
exist in both builds, and are tested in `web/tests/node/run.mjs`.

## 6. Build and Packaging

- `NYMPH_WEB_JS` (default off; only with `NYMPH_WEB`) switches the web
  build to the JavaScript options of section 4.5 and outputs
  `nymph-viewer.classic.js` instead of the module files. It is a separate
  build folder (for example `build-web-js`) because the exception model
  is a whole-program compile option.
- `NYMPH_VSCODE` (default off; in the WebAssembly web build) adds
  `vscode/CMakeLists.txt`, with `NYMPH_VSCODE_JS_CORE` naming the
  JavaScript build's `nymph-viewer.classic.js`. It stages
  `<build>/vscode/`: the sources, `package.json` (version from the
  project), `media/` with `nymph-viewer.js`, `nymph-viewer-core.js` /
  `.wasm` and `fonts/`, `preview/` with the loader, styles and
  `nymph-viewer.classic.js`, the icon, `LICENSE`, and a short
  `README.md`.
- `nymph-vscode-vsix` zips the staging folder into
  `nymph-vscode-<version>.vsix` with a generated
  `extension.vsixmanifest` and `[Content_Types].xml`
  (`cmake/NymphVsix.cmake`), so no npm is needed to build. The node test
  checks the zip's layout; the release step checks
  `code --install-extension` accepts it.
- `release.yml` builds both web variants, attaches the `.vsix`, and
  publishes with `npx @vscode/vsce publish --packagePath` (secret
  `VSCE_PAT`) and `npx ovsx publish` (`OVSX_PAT`), jobs skipped while the
  secrets are absent.

## 7. Testing Strategy

- **Web module, Node** (`web/tests/node/run.mjs`, both builds): the two
  exports; for the JavaScript build also: software-rasterized PNGs equal
  to the WebAssembly build's for every example, the file parses as a
  classic script, contains no `eval(` / `new Function`.
- **Extension, Node, no dependencies** (`vscode/tests/node/run.mjs`,
  CTest `nymph_vscode_node`): `renderBlock` (info matching, escaping,
  `data-line`, index and hash, error panel, line mapping, size limit)
  with fake tokens; `findBlocks` on fixtures; `NymphRenderer` with the
  real core (embedded YAML equals the block; pixels equal to the native
  CLI's software render with the theme set in the YAML; fonts on demand;
  cache hits; errors); the `.vsix` layout.
- **Preview script, Chromium through Playwright**
  (`vscode/tests/browser/`, 77 when Playwright is missing): a fixture page
  with the Strict CSP and a nonce, built from `markdown.api.render`
  output: a click loads the classic build with no CSP violation and makes
  the chart live (`viewer.source()` equals the block); a simulated
  content update with the same hash keeps the viewer and its zoom, a
  changed hash updates it, a removed block destroys it; the pool parks
  the ninth; `interactive: off` loads nothing.
- **In VS Code** (`vscode/tests/vscode/run.mjs`, CTest
  `nymph_vscode_integration`, 77 when `@vscode/test-electron` is
  missing): `markdown.api.render` of a fixture contains the chart and
  error HTML; diagnostics appear and clear; CodeLenses are listed;
  `nymph.openLive` opens a panel.
- **The live panel page** with Playwright and a stub `acquireVsCodeApi`
  (like the stand-in host of
  [spec/06_claude_integration](../06_claude_integration/DESIGN.md)).

## 8. Design Decisions

- **Compiled, not rewritten.** The renderer is Dear ImGui, ImPlot,
  ImPlot3D, ImUml, nymph's layout and its software rasterizer, tens of
  thousands of lines of C++. A JavaScript rewrite would be a second
  renderer that drifts from the PNGs (viewer DESIGN's rejected
  alternatives). Emscripten's JavaScript output keeps one source of
  truth; the price is size and speed (section 9).
- **Image first, live on click.** The PNG is instant, small next to the
  classic build, needs no script, and survives export. Loading a
  multi-MB script for every preview, and wheel-zooming charts the user
  scrolls past, would make reading worse. Rejected: live by default
  (`always` stays a setting).
- **Render the image on the extension host with WebAssembly, not the
  `nymph` executable.** markdown-it's render rules are synchronous; a
  child process is asynchronous and needs an installed, matching `nymph`
  on `PATH`. The module is already built and tested in Node
  (`web/tests/node`) and runs in remote hosts (SSH, WSL, containers)
  without installing anything there.
- **Inject the classic build with the page's nonce, not as a preview
  script.** Every preview script is loaded into every preview; the
  loader keeps unclicked previews at under 10 KB. Rejected: an `<iframe>`
  or a worker (`frame-src` / `worker-src` fall back to `'none'`); telling
  users to set the preview's security to "Disable" (it lets every
  Markdown file run scripts in the preview).
- **Fonts embedded in the classic build.** The preview cannot fetch;
  embedding all fonts (about 2.8 MB, more as base64) is simplest. If
  step 1 finds the file too large, the build embeds the default font
  only and a chart naming another font stays an image with "could not
  go live: font X" (the image is already right, rendered on the host).
- **`data:` images, not files.** `img-src data:` is allowed in every
  setting, works in remote hosts, and leaves nothing on disk. Rejected:
  PNGs in the extension's storage (the preview's `localResourceRoots`
  are not the extension's to extend). Cost: base64 HTML of roughly
  100-400 KB per chart at scale 2.
- **Explicit label.** Only `nymph` fences are drawn; authors choose
  their labels, and a `yaml` block stays YAML. (Gemini DESIGN sniffs
  `yaml` blocks because a chatbot's labels are not under the user's
  control; here they are.)
- **Theme from VS Code, not the preview's CSS.** The host renders, so it
  needs the theme before the HTML exists; `activeColorTheme.kind` gives
  it, and a refresh follows a change. Custom preview styles
  (`markdown.styles`) are not seen; `nymph.preview.theme` covers them.
- **The panel keeps the WebAssembly build.** Its CSP is the extension's,
  so it can allow `'wasm-unsafe-eval'` and get the faster build and
  fonts on demand.
- **No npm, CommonJS entry.** The repository has no `package.json` or
  npm dependencies; the `.vsix` is a zip CMake can write. A CommonJS
  `main` works on every supported VS Code (ESM extension entries are
  newer); the ES-module core loads through `import()`.

## 9. Constraints, Risks, Open Questions

- **Size and speed of the JavaScript build.** wasm2js output is usually
  several times the WebAssembly size uncompressed and runs a few times
  slower (*unverified* for nymph: step 1 measures file size, parse time,
  first frame and pan/zoom frame times on every example). Accepted if a
  typical chart pans at 30 fps or better and the file loads within 2 s
  on a laptop; otherwise the default becomes `off` for live mode in the
  preview and the panel becomes the interactive route.
- **wasm2js coverage.** JavaScript-based exceptions, 64-bit integer
  lowering and memory growth are supported by wasm2js, but the whole
  library has never been built this way; step 1 builds it and runs the
  web Node tests on it.
- **Nonce injection and preview updates** (section 3.4) are
  *unverified*; step 1 checks both. If a nonce'd injected script is
  refused, the classic build becomes a second preview script (loaded in
  every preview, parsed lazily by wrapping its body in a function called
  on first click).
- **Synchronous rendering blocks the extension host**, which other
  extensions share. A large 3D plot or big data table may take hundreds
  of milliseconds. The cache limits this to once per change of a block;
  step 1 measures every example. If the slowest exceed 150 ms, step 4
  switches to asynchronous rendering: a placeholder for an uncached
  block, the render in a `worker_threads` worker, then
  `markdown.preview.refresh` (`render()` stays behind one function so
  this stays local).
- **Activation order** (section 4.1) is *unverified*. If activation is
  not awaited, the plugin emits a "loading nymph" panel and refreshes the
  preview once the renderer is ready.
- **GitHub does not render `nymph` fences.** A README read on GitHub
  shows the YAML. `docs/vscode.md` suggests committing the PNG next to
  the block where it must show there (or the Chrome extension, which
  draws nymph PNGs).
- **Memory.** One WebAssembly instance in the extension host (about
  30-60 MB with fonts registered, *unverified*) and one JavaScript
  instance per preview that went live.
- **Untrusted sources.** Every document is untrusted input to the
  library, as in the viewer: no I/O, catalog-only fonts, escaped HTML,
  sizes bounded.

## 10. Integration Boundaries

- **Depends on** the web module and its node build (viewer DESIGN
  section 4, "Web module"; two exports and a second build variant added,
  sections 4.5 and 5.4), the font catalog (viewer DESIGN, "Fonts on
  demand"), `nymph-viewer.js` (both packagings), VS Code's Markdown
  extension contract (`markdownItPlugins`, `previewStyles`,
  `previewScripts`, `vscode.markdown.updateContent`,
  `markdown.preview.refresh`, `markdown.api.render`), and the release
  workflow ([spec/07_release_pipeline](../07_release_pipeline/DESIGN.md)).
- **Depends on it:** nothing yet; the classic build is available to
  other strict-CSP hosts later (for example frame-less pages in
  [spec/05_browser_extension](../05_browser_extension/DESIGN.md)'s inline
  mode).
