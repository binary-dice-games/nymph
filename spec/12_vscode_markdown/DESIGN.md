# nymph — VS Code Markdown Preview: Design

**Status: design only; nothing is built.** [PLAN.md](PLAN.md) is the
ordered implementation plan; its step 1 checks the facts marked
*unverified* before code depends on them. This document describes only
what this project adds or changes. It builds on
[spec/04_web_viewer/DESIGN.md](../04_web_viewer/DESIGN.md) (*viewer
DESIGN*: the WebAssembly module, its JavaScript API, fonts on demand) and
borrows ideas from [spec/05_browser_extension/DESIGN.md](../05_browser_extension/DESIGN.md)
(*extension DESIGN*) and
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
mermaid is plain JavaScript. nymph's renderer is WebAssembly, and the
preview's Content Security Policy (VS Code `main`,
`extensions/markdown-language-features/src/preview/documentRenderer.ts`,
read 2026-10-20) does not allow it in the default setting:

| Preview security setting | CSP | WebAssembly | `fetch` (wasm, fonts) | `<iframe>` |
|---|---|---|---|---|
| Strict (default) | `default-src 'none'; img-src 'self' <webview> https: data:; script-src 'nonce-…'; style-src …; font-src …` | blocked (no `'wasm-unsafe-eval'`) | blocked (no `connect-src`) | blocked |
| Allow insecure content / local content | the same plus `http:` images | blocked | blocked | blocked |
| Disable | none | allowed | allowed | allowed |

So the chart is **rendered on the extension host** (Node.js, no CSP) by
the same WebAssembly module, and the preview receives a plain PNG as a
`data:` image, which every setting allows. Interaction (pan, zoom,
values) is one click away in a **live panel**: a webview the extension
owns, whose CSP it sets (section 3.4).

### In scope

- The Markdown preview: `nymph` fences become nymph PNGs (static, in the
  editor's light or dark theme), or an error panel.
- The editor: diagnostics at the right line for broken nymph blocks, YAML
  highlighting inside them, a CodeLens "Open live · Save PNG" above each.
- The live panel: the web viewer beside the editor, following the block
  as it is edited.
- Live charts inside the preview itself, only when the user has already
  set the preview's security to "Disable" (section 3.5); never suggested.
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
- Editing charts in the panel beyond saving the user's view back into
  the block (the view editor of spec/08 is native only).
- Any change to the source format or the viewer's JavaScript API.

### New and changed files

| Path | What |
|---|---|
| `vscode/package.json.in` | The extension manifest (version filled in by CMake) |
| `vscode/src/extension.js` | `activate()`: loads the renderer, registers everything, returns `extendMarkdownIt` |
| `vscode/src/fence.js` | The markdown-it plugin and the pure `renderBlock()` |
| `vscode/src/renderer.js` | `NymphRenderer`: the WebAssembly core in Node, synchronous render and check, cache, fonts |
| `vscode/src/blocks.js` | Finds `nymph` fences in a document's text (shared by diagnostics, CodeLens, the live panel) |
| `vscode/src/diagnostics.js`, `vscode/src/lens.js` | Editor diagnostics and CodeLens |
| `vscode/src/live-panel.js`, `vscode/media/live.js`, `live.css` | The live panel: host side and webview side |
| `vscode/preview/nymph-preview.css` | Preview styles for charts and error panels |
| `vscode/preview/nymph-preview.js` | The opportunistic live upgrade (section 3.5) |
| `vscode/syntaxes/nymph-injection.json` | TextMate injection: YAML inside `nymph` fences |
| `vscode/CMakeLists.txt`, `cmake/NymphVsix.cmake` | Staging and the `.vsix` zip (`NYMPH_VSCODE`) |
| `vscode/tests/node/run.mjs`, `vscode/tests/vscode/run.mjs` | Node tests; tests in a real VS Code |
| `web/nymph_web.cpp` | Two stateless exports: `nymph_render_png`, `nymph_check` (section 5.4) |
| `docs/vscode.md` | Install and use (created with the first working build) |

## 2. Design Goals

1. **Works with the preview's default security.** Charts show under the
   Strict setting; the extension never asks the user to loosen it.
2. **Same source, same picture.** A block draws exactly what
   `nymph chart.yaml --rasterizer software` draws (with the theme
   override of section 3.2), and the image is a nymph PNG carrying the
   block's YAML.
3. **Nothing else to install.** No `nymph` executable, no npm packages at
   run time: the WebAssembly module ships inside the extension.
4. **Typing stays fluid.** The preview re-renders on every edit; only a
   block whose text (or theme) changed is rendered again.
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

### 3.2 In the preview

The block becomes

```html
<div class="nymph-chart code-line" data-line="12">
  <img src="data:image/png;base64,…" width="640" height="360" alt="<description>">
</div>
```

- **Size.** The PNG is rendered at the source's `image.width` x `height`
  times `nymph.preview.scale` (default 2, for sharp text on high-DPI
  screens) and shown at the source's size, shrunk to the column by
  `max-width: 100%; height: auto`. The host cannot know the preview's
  width, so `fit: 'width'`-style reflow is the live panel's job.
- **Theme.** `nymph.preview.theme`: `auto` (default: `light` or `dark`
  from `vscode.window.activeColorTheme.kind`; high contrast maps to its
  base), or `source`, `light`, `dark`, `classic`. When the color theme or
  the setting changes, the extension runs `markdown.preview.refresh`.
  The PNG's embedded YAML is the block as written, not the override.
- **Scroll sync.** The wrapper keeps the `code-line` class and
  `data-line` attribute VS Code's preview uses to sync scrolling and
  double-click-to-edit (copied from the token's attributes; a plugin that
  drops them breaks both).
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

### 3.4 The live panel

**Opening.** The CodeLens above each `nymph` fence ("Open live",
"Save PNG…"; `nymph.codeLens.enabled`), the command "nymph: Open Live
Chart" (the block at the cursor), and, if step 1 shows the preview
follows `command:` links (*unverified*), a small "Open live" link under
each chart in the preview.

**What it is.** A `WebviewPanel` beside the editor, one per block (opening
the same block again reveals its panel), titled with the chart's title or
"nymph chart". Its CSP is
`default-src 'none'; script-src ${cspSource} 'wasm-unsafe-eval'; connect-src ${cspSource}; style-src ${cspSource}; img-src ${cspSource} data: blob:`;
it loads `nymph-viewer.js`, the core and `fonts/` from the extension
(`localResourceRoots` is the extension's `media/` only). The viewer runs
with `fit: 'width'`, `theme` from the webview body's `vscode-dark` /
`vscode-light` class (followed live), the right-click menu off (its
save items download, which webviews do not support).

**Following the block.** The panel is bound to (document URI, block
index). On a document change (debounced 300 ms) the host finds the
blocks again (`blocks.js`); the bound block is the one with the same
index, or, if the count changed, the one whose start line is nearest the
old one. Its new text goes to `setSource()`, keeping the user's zoom; a
failing text keeps the previous chart and shows the error in a status
line at the panel's top. Closing the document or deleting the block
leaves the last chart with a "source closed" status.

**Toolbar** (in the panel, messages to the host): "Save PNG…" and "Save
YAML…" (the viewer's `toPng()` / `toYaml()`, written by the host through
a save dialog), "Reset view", and "Write view to block" (replaces the
block's text with `toYaml()` through a `WorkspaceEdit`, so the zoom is
kept in the `view` key; an ordinary edit the user can undo).

### 3.5 Live in the preview, when the policy allows it

`nymph-preview.js` (a preview script, loaded in every preview) tries to
compile an 8-byte empty WebAssembly module. Under any setting but
"Disable" this throws and the script does nothing more. When it
succeeds, the script imports `nymph-viewer.js` from beside itself (from
`document.currentScript.src`) and replaces each `.nymph-chart` image,
when it scrolls into view, with a `<nymph-viewer fit="width"
theme="auto">` built from the image's PNG (`setPng`). This only helps
users who already chose "Disable"; the docs mention it in one line and
do not recommend the setting.

## 4. Key Abstractions

All plain JavaScript (CommonJS for the extension host entry, ES modules
in the webviews), no build step, no npm dependencies, matching
`extension/`.

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
- maps error positions to document lines from `token.map`.

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
| `contributes.markdown.previewScripts` | `["./preview/nymph-preview.js"]` |
| `contributes.grammars` | injection `markdown.nymph.codeblock` into `text.html.markdown`, embedding `source.yaml` |
| `contributes.commands` | `nymph.openLive`, `nymph.savePng` |
| `contributes.configuration` | section 5.2 |

### 5.2 Settings

| Key | Default | What |
|---|---|---|
| `nymph.preview.enabled` | `true` | Draw `nymph` blocks in the preview |
| `nymph.preview.theme` | `auto` | `auto`, `source`, `light`, `dark`, `classic` |
| `nymph.preview.scale` | `2` | PNG pixels per source pixel (1-3) |
| `nymph.diagnostics.enabled` | `true` | Editor diagnostics for broken blocks |
| `nymph.codeLens.enabled` | `true` | "Open live · Save PNG…" above blocks |

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
and are tested in `web/tests/node/run.mjs`.

## 6. Build and Packaging

- `NYMPH_VSCODE` (default off; requires `NYMPH_WEB`, so an Emscripten
  build) adds `vscode/CMakeLists.txt`. It stages `<build>/vscode/`: the
  sources, `package.json` (version from the project), `media/` with
  `nymph-viewer.js`, `nymph-viewer-core.js` / `.wasm` and `fonts/`,
  the icon, `LICENSE`, and a short `README.md`.
- `nymph-vscode-vsix` zips the staging folder into
  `nymph-vscode-<version>.vsix` with a generated
  `extension.vsixmanifest` and `[Content_Types].xml`
  (`cmake/NymphVsix.cmake`), so no npm is needed to build. The node
  test checks the zip's layout; step 10 checks `code --install-extension`
  accepts it.
- `release.yml` attaches the `.vsix`; publishing uses `npx @vscode/vsce
  publish --packagePath` (secret `VSCE_PAT`) and `npx ovsx publish`
  (`OVSX_PAT`), jobs skipped while the secrets are absent.

## 7. Testing Strategy

- **Node, no dependencies** (`vscode/tests/node/run.mjs`, CTest
  `nymph_vscode_node`): `renderBlock` (info matching, escaping,
  `data-line`, error panel, line mapping, size limit) with fake tokens;
  `findBlocks` on fixtures (nested fences, tildes, indentation,
  unterminated fence); `NymphRenderer` with the real core (a PNG whose
  embedded YAML equals the block; pixels equal to the native CLI's
  software render of the same source with the theme set in the YAML;
  fonts on demand; cache hits; errors); the `.vsix` layout.
- **In VS Code** (`vscode/tests/vscode/run.mjs`, CTest
  `nymph_vscode_integration`, skipped with 77 when `@vscode/test-electron`
  is not installed, as the Playwright tests): `markdown.api.render` of a
  fixture contains the chart and error HTML; diagnostics appear and clear;
  CodeLenses are listed; `nymph.openLive` opens a panel.
- **The live panel page** in Chromium through Playwright with a stub
  `acquireVsCodeApi` (like the stand-in host of
  [spec/06_claude_integration](../06_claude_integration/DESIGN.md)):
  source updates keep the zoom, errors show in the status line, the save
  and write-view messages carry the viewer's output.

## 8. Design Decisions

- **Render on the extension host, not in the preview.** The preview's
  default CSP forbids WebAssembly, fetches and frames (section 1).
  Rejected: telling users to set the preview's security to "Disable"
  (it lets every Markdown file run scripts in the preview); an `<iframe>`
  to an extension page (`frame-src` falls back to `'none'`); a JavaScript
  renderer for the preview (a second renderer, viewer DESIGN's rejected
  alternatives).
- **The WebAssembly module, not the `nymph` executable.** markdown-it's
  render rules are synchronous; a child process is asynchronous and
  needs an installed, matching `nymph` on `PATH`. The module is already
  built and tested in Node (`web/tests/node`), runs on every platform
  VS Code does, and in remote hosts (SSH, WSL, containers) without
  installing anything there.
- **Static image in the preview, live in a panel.** The only live option
  inside the preview needs the CSP gone; a panel is the extension's own
  webview, so it can allow `'wasm-unsafe-eval'` for itself alone.
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
- **No npm, CommonJS entry.** The repository has no `package.json` or
  npm dependencies; the `.vsix` is a zip CMake can write. A CommonJS
  `main` works on every supported VS Code (ESM extension entries are
  newer); the ES-module core loads through `import()`.

## 9. Constraints, Risks, Open Questions

- **Synchronous rendering blocks the extension host**, which other
  extensions share. A large 3D plot or a big data table may take hundreds
  of milliseconds. The cache limits this to once per change of a block;
  step 1 measures every example. If the slowest examples exceed 150 ms,
  step 3 switches to asynchronous rendering: a placeholder for an
  uncached block, the render in a `worker_threads` worker, then
  `markdown.preview.refresh` (the design keeps `render()` behind one
  function so this stays local).
- **Activation order** (section 4.1) and **command links in the
  preview** (section 3.4) are *unverified*; step 1 checks them. If the
  activation is not awaited, the plugin emits a "loading nymph" panel and
  refreshes the preview once the renderer is ready.
- **GitHub does not render `nymph` fences.** A README read on GitHub
  shows the YAML. `docs/vscode.md` suggests committing the PNG next to
  the block where it must show there (or the Chrome extension, which
  draws nymph PNGs).
- **Memory.** One module instance (about 30-60 MB with fonts registered,
  *unverified*) per extension host.
- **Untrusted sources.** Every document is untrusted input to the
  library, as in the viewer: no I/O, catalog-only fonts, escaped HTML,
  sizes bounded.

## 10. Integration Boundaries

- **Depends on** the web module and its node build (viewer DESIGN
  section 4, "Web module"; two exports added, section 5.4), the font
  catalog (viewer DESIGN, "Fonts on demand"), `nymph-viewer.js` for the
  live panel and preview upgrade, VS Code's Markdown extension contract
  (`markdownItPlugins`, `previewStyles`, `previewScripts`,
  `markdown.preview.refresh`, `markdown.api.render`), and the release
  workflow ([spec/07_release_pipeline](../07_release_pipeline/DESIGN.md)).
- **Nothing depends on it.** It changes no library behaviour; the two new
  exports are additive.
