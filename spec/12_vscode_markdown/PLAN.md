# nymph — VS Code Markdown Preview: Implementation Plan

Design: [DESIGN.md](DESIGN.md). Builds on the web viewer
([spec/04_web_viewer](../04_web_viewer/DESIGN.md)) and the release
pipeline ([spec/07_release_pipeline](../07_release_pipeline/DESIGN.md)).

Steps go from what could invalidate the design (step 1), to the web
module (2-3), to the extension's pure pieces (4-5), to the extension in
VS Code (6-9), to the optional live panel (10), to release and manual
checks (11-12). Each step is a reviewable deliverable and ends with every
existing native, Node and browser test passing unmodified. The
extension's tests live in `vscode/tests/` and are registered in CTest
when `NYMPH_VSCODE` is on. Steps with user-visible behaviour (3 and 6
onwards) add a `CHANGELOG.md` entry. Steps that run VS Code record its
version.

## Steps

### 1. Feasibility check

**Goal:** the facts DESIGN.md marks *unverified* are answered, in a real
VS Code and with a real JavaScript build, and DESIGN.md is updated
before code depends on them.

**Deliverables:** a throwaway extension (not committed) with a
markdown-it plugin that emits a fixed `data:` PNG for `nymph` fences, a
loader script, and timing logs; a throwaway `-sWASM=0` build of the web
module; the answers recorded in this step's **Result** and folded into
DESIGN.md sections 3, 4, 5.1 and 9.

1. Does VS Code await an `async activate()` before calling
   `extendMarkdownIt` (first preview of a fresh window)?
2. Does `markdown.api.render` include the plugin's output (the test hook
   of step 6)?
3. With the fence rule wrapped and `code-line` / `data-line` kept, do
   scroll sync and double-click-to-edit work on the chart?
4. In a Strict preview: does a `<script>` added by a preview script, with
   `document.currentScript.nonce` and a `src` beside it, load? Is
   `WebAssembly.compile` refused (and allowed under "Disable")?
5. Preview updates: after an edit, does VS Code replace the body, or
   patch it (which nodes survive)? Is `vscode.markdown.updateContent`
   fired, and when? Which body classes carry the theme?
6. The JavaScript build: does the library build with `-sWASM=0
   -fexceptions -sDYNAMIC_EXECUTION=0 -sSINGLE_FILE=1` and all fonts
   embedded? Do `web/tests/node` pass on it? Record: file size raw and
   gzip (with and without fonts), parse and first-frame time in the
   preview, frame time while panning a 2D plot, a diagram and a 3D plot
   from `examples/`, each next to the WebAssembly build's numbers.
   Decide against DESIGN section 9's thresholds (30 fps, 2 s load).
7. Load the WebAssembly core in the extension host (Node) and time a
   synchronous software render of every file in `examples/` at scale 2:
   load time, p50, p95, slowest; module memory. Decide sync or worker
   rendering (DESIGN section 9).
8. Remote: the same extension over Remote-SSH or WSL and in a dev
   container; which host runs the plugin.
9. The oldest VS Code version where items 1-5 hold (for
   `engines.vscode`).

**Tests:** none automated.

**Result** (2026-10-20; Emscripten 6.0.12, Node 22, Playwright 1.56 with
Chromium 141 and SwiftShader WebGL2, a 4-core Linux VM). A real VS Code
was **not** run: `update.code.visualstudio.com` is blocked by the
session's network policy, so items 1-5 and 9 were answered from VS Code's
source (`extensions/markdown-language-features` on `main` and at tag
1.90.0) and a Chromium page reproducing the preview, and item 8 not at
all. The probe scripts stayed out of the repository, as planned.

1. **Yes.** `markdownExtensions.ts` maps each plugin contributor to
   `extension.activate().then(() => exports.extendMarkdownIt)` and the
   engine awaits it before rendering (main and 1.90.0).
2. **Yes, by source.** `markdown.api.render` (`commands/renderDocument.ts`)
   renders with the same engine, plugins included.
3. **Attributes yes; scroll sync and double-click not seen.** VS Code's
   core rule `source_map_data_attribute` runs after every plugin and sets
   `data-line` / `code-line` on the fence token, so the plugin only has to
   output the token's attributes. The preview's `dblclick` handler on
   `document` would also fire for a double-click on a live chart: the
   live box must stop it (DESIGN 3.4).
4. **Yes.** With the preview's exact Strict policy, a `<script>` added by
   a nonce'd script with `document.currentScript.nonce` loads, and the
   JavaScript build draws live with no CSP violation. The WebAssembly
   build is refused twice (compile: `wasm-eval`; the `.wasm` fetch:
   `connect-src`).
5. **Patched with morphdom**, then `vscode.markdown.updateContent` on
   `window` (unchanged since 1.90.0). Equal elements are kept
   (`areNodesEqual` ignores `data-line` and does not see shadow roots);
   others are morphed or replaced by position. Theme classes:
   `vscode-light` / `vscode-dark` / `vscode-high-contrast` on `body`
   (standard webview behaviour, not checked in VS Code). With the viewer
   in a shadow root and viewers re-attached by hash, the probe kept all
   viewers and their zoom over an edit outside the charts, updated an
   edited chart in place, and moved viewers when text was inserted above
   (DESIGN 3.4 rewritten from this).
6. **The library builds and runs as JavaScript.** `-sWASM=0 -fexceptions
   -sDYNAMIC_EXECUTION=0 -sSINGLE_FILE=1`, all fonts: 8.1 MB, 2.9 MB gzip
   (no fonts: 4.4 / 1.2 MB; the WebAssembly module is 1.94 / 0.77 MB);
   no `eval` or `new Function`. `web/tests/node` on it: 8 of 11 pass,
   including "every example renders like its committed PNG"; the 3
   failures only test font fetching and the `.wasm` file, which this
   build does not have. A classic-script version (exports stripped,
   `import.meta.url` replaced, IIFE) parses as a script. In Chromium:
   load 0.2 s, click to first live frame 0.6-1.0 s (WebAssembly: 0.25 s).
   Pan frame cost p50 / p95, WebGL: line 20/31 ms, class diagram 10/24,
   3D surface 20/34 (WebAssembly: 17/32, 12/27, 17/28); software
   rasterizer: 33/53, 20/31, 49/62 (WebAssembly: 10/17, 7/10, 17/28).
   **Decision:** live on click is kept (thresholds met with WebGL, which
   VS Code has; software-only machines get 16-32 fps). **New risk:**
   Emscripten 6.0.12 warns `-sWASM=0` is deprecated and scheduled for
   removal (issue 27608). Binaryen's standalone `wasm2js`, which stays,
   converted the library's module (relinked with `-fexceptions`, flags
   `--emscripten -O2` and the module's features enabled) without error
   (15.5 MB unminified, no `eval`); running it needs a shim, so step 3
   now starts there (DESIGN 4.5).
7. **Too slow to render on the extension host thread.** WebAssembly in
   Node, every example at scale 2: load 59 ms, render p50 207 ms, p95
   399 ms, max 431 ms (`ui_editor`, `bar_groups`, `sankey`); at scale 1:
   p50 58 ms, max 175 ms; 180 MB resident. **Decision:** render in a
   `worker_threads` worker, show a pending wrapper, refresh the preview
   when results arrive (DESIGN 3.2, 4.3). The JavaScript build takes
   about 2.2x as long in Node (p50 445 ms).
8. **Not checked.** Both the Markdown extension and this one default to
   the workspace extension host; checked in step 12.
9. **1.90.0** has the same activation, CSP, line attributes and update
   code as `main` (read, not run); `engines.vscode` stays `^1.90.0`.

### 2. Stateless exports in the web module

**Goal:** the WebAssembly core can render a source to a PNG with a theme
and scale override, and check a source, without a viewer.

**Deliverables:** `nymph_render_png` and `nymph_check` in
`web/nymph_web.cpp` (DESIGN section 5.4), with Doxygen comments; any
library helper they need for the overrides; viewer DESIGN section 4
("Web module") notes the two exports.

**Tests** (`web/tests/node/run.mjs`): a valid source renders, the PNG's
embedded YAML equals the input byte for byte; a `dark` override gives the
same pixels as the source with `image.theme: dark`; scale 2 doubles both
dimensions; a broken source returns 0 with the line, column and message
`nymph check` prints; a source naming a non-embedded font fails with
`fonts_needed` naming it, and succeeds after `nymph_register_font`;
`nymph_check` agrees with `nymph check` on the `tests/` error fixtures.

**Result:** *(to be filled in)*

### 3. The JavaScript build of the viewer

**Goal:** `NYMPH_WEB_JS` builds `nymph-viewer.classic.js`, a single
classic script with the same API and the same software-rendered pixels
as the WebAssembly build, that runs under a CSP without
`'wasm-unsafe-eval'`, `connect-src` or `eval`.

**Deliverables:** first, a shim (`web/js/wasm2js-shim.js`: the
`WebAssembly.Module` / `Instance` / `Memory` subset Emscripten's loader
uses, backed by the module `wasm2js` emits) and proof that the
step 1 conversion runs through it in Node; if it cannot be made to work,
`-sWASM=0` (step 1's measured build) is used instead while the pinned
Emscripten supports it, and DESIGN 4.5 says so. Then the `NYMPH_WEB_JS`
option (root `CMakeLists.txt`: `-fexceptions` instead of
`-fwasm-exceptions`, `-sDYNAMIC_EXECUTION=0`, all fonts embedded;
`web/CMakeLists.txt`: the `wasm2js` custom command with the module's
features, the classic-script assembly step); `web/js/nymph-viewer.js` with its
`export`s and the one `import.meta` line made replaceable, behaviour
unchanged; `docs/building.md` (the second build folder) and a note in
`docs/viewer.md`; viewer DESIGN section 6 ("Build") mentions the variant;
CI builds it on Linux; `CHANGELOG.md`: "Added: a single-file JavaScript
build of the web viewer for pages that forbid WebAssembly."

**Tests:** `web/tests/node/run.mjs` run against the classic file (loaded
with `vm.Script`, so as a script): every existing assertion except the
font-fetch and `.wasm`-size ones (step 1 showed they do not apply), and
PNGs of every example equal to the WebAssembly build's; the file contains no
`eval(` or `new Function`; the module file still parses as a module.
Browser (Playwright, 77 when missing): a page with
`script-src 'nonce-x'` and no other source loads it, draws a live chart,
pans it, and records no `securitypolicyviolation` event.

**Result:** *(to be filled in)*

### 4. `NymphRenderer`

**Goal:** Node code renders and checks sources with the WebAssembly core
in a worker thread, cached, with fonts loaded on demand from a folder,
without ever running the core on the calling thread.

**Deliverables:** `vscode/src/renderer.js` and `renderer-worker.js`
(DESIGN section 4.3: `peek`, async `render` / `check`, `onRendered`,
superseded requests dropped, restart after a crash);
`vscode/tests/node/run.mjs` (dependency-free runner, the style of
`extension/tests/node/run.mjs`); CTest `nymph_vscode_node`.

**Tests:** render result shape, description, size at scales 1 and 2;
same text twice is one core call (counted through a wrapped core);
different theme is a second call; `peek` returns nothing before and the
result after `render` resolves; of ten queued requests for one block only
the last runs; a worker killed mid-render is restarted and the next
request succeeds; LRU eviction at the entry and byte limits; errors are
cached too; a font is read once and only for catalog
names (a source naming `../x.ttf` is a plain error, no read); `check()`
never rasterizes.

**Result:** *(to be filled in)*

### 5. Fences: `renderBlock` and `findBlocks`

**Goal:** a `nymph` fence token becomes the chart or error HTML of
DESIGN sections 3.2 and 3.3, and fences are found in raw text the same
way markdown-it finds them.

**Deliverables:** `vscode/src/fence.js` (`nymphFence`, `renderBlock`),
`vscode/src/blocks.js` (`findBlocks`), fixtures under
`vscode/tests/fixtures/`.

**Tests** (Node, fake tokens and a fake renderer): `nymph`, `Nymph`,
`nymph extra words` match, `nymphx` and `yaml` do not; non-nymph tokens
go to the previous rule; `code-line` / `data-line` copied;
`data-nymph-hash` follows the text; an uncached block gives the pending
wrapper (with the last image of that block number, dimmed) and one render
request; `data-nymph-settings` carries the settings; `<`, `&`, quotes
escaped in message, `alt` and source; error line mapping (block line 18
at fence line 11 is document line 30); the 256 KiB limit; a throwing
renderer gives an error panel; `findBlocks` on backticks, tildes, longer
closing runs, indented fences, an unterminated fence at end of file, a
fence inside a list item.

**Result:** *(to be filled in)*

### 6. The extension in the preview, and its package

**Goal:** installing the `.vsix` makes `nymph` fences draw as images in
VS Code's Markdown preview.

**Deliverables:** `vscode/package.json.in` (DESIGN section 5.1, without
the preview script and panel commands yet), `vscode/src/extension.js`,
`vscode/preview/nymph-preview.css`, `vscode/CMakeLists.txt`,
`cmake/NymphVsix.cmake`, the `NYMPH_VSCODE` and `NYMPH_VSCODE_JS_CORE`
options in the root `CMakeLists.txt` and `docs/building.md`;
`vscode/tests/vscode/run.mjs` and CTest `nymph_vscode_integration` (77
when `@vscode/test-electron` is missing); `docs/vscode.md` and its row in
`README.md`; `CHANGELOG.md`: "Added: a VS Code extension that draws
`nymph` code blocks in the Markdown preview."

**Tests:** Node: the staged folder and the `.vsix` contain the manifest,
`extension.vsixmanifest`, `[Content_Types].xml`, the WebAssembly core,
`fonts/`, and `nymph-viewer.classic.js`; the manifest's version is the
project's. VS Code: `markdown.api.render` of a fixture with one good and
one broken block returns one `nymph-chart` with a decodable PNG and one
`nymph-error` with the right document line; a `mermaid` block in the same
file renders as before.

**Result:** *(to be filled in)*

### 7. Theme and settings

**Goal:** charts follow VS Code's light or dark theme and the settings of
DESIGN section 5.2.

**Deliverables:** theme resolution and the configuration and
`onDidChangeActiveColorTheme` listeners calling
`markdown.preview.refresh`; `nymph.preview.enabled`, `.theme`, `.scale`;
`docs/vscode.md` settings table; `CHANGELOG.md` entry.

**Tests:** Node: theme resolution for each `ColorThemeKind` and setting
value. VS Code: switching `workbench.colorTheme` between a light and a
dark theme changes the rendered PNG (its hash); `preview.enabled: false`
renders the fence as a code block; scale 1 halves the PNG size.

**Result:** *(to be filled in)*

### 8. Diagnostics and highlighting

**Goal:** broken blocks are underlined in the editor and `nymph` blocks
are highlighted as YAML.

**Deliverables:** `vscode/src/diagnostics.js` (DESIGN section 3.3),
`vscode/syntaxes/nymph-injection.json` and its `contributes.grammars`
entry; `CHANGELOG.md` entry.

**Tests:** VS Code: opening the fixture gives one diagnostic at the
broken block's document line and column, source "nymph"; fixing the
text clears it within a second; closing the document clears it; the
setting off publishes none. Node: the injection grammar is valid JSON
with the expected scope names.

**Result:** *(to be filled in)*

### 9. Live charts in the preview

**Goal:** under the Strict setting, clicking a chart in the preview makes
it a live viewer of the same size, kept across edits.

**Deliverables:** `vscode/preview/nymph-preview.js` (DESIGN section 3.4:
hint, nonce injection, the viewer in a shadow root with a `<slot>` for
the image, theme from body classes, update handling by wrapper and hash
as step 1's probe did, `dblclick` stopped at the live box, pool of 8,
`Esc` / click-outside releasing the wheel) and its manifest entry;
`nymph.preview.interactive`; the `vscode/tests/browser/` fixture page
built from step 6's `markdown.api.render` output with the Strict CSP and
VS Code's morphdom update code (copied, with its source and date);
`docs/vscode.md`; `CHANGELOG.md`: "Added: click a chart in the VS Code
preview to pan, zoom and read values."

**Tests:** Browser (Playwright, 77 when missing): before any click no
request for the classic build is made; a click loads it once, with no
`securitypolicyviolation`, and the viewer's `source()` equals the block;
the image stays until the first frame; a simulated update with the same
hash keeps the viewer and a zoom done before, a changed hash updates it
(zoom kept on an unchanged plot), text inserted above the charts moves
each viewer to its chart, a removed block destroys its viewer; a
double-click on a live chart does not reach `document`; a
ninth activation parks the least recently used; `always` activates on
scroll into view; `off` never loads the build; a dark body class gives
the dark theme and switching it re-themes live viewers; a source with a
font the build lacks stays an image with the hint.

**Result:** *(to be filled in)*

### 10. CodeLens and the live panel (optional)

**Goal:** "Save PNG…" writes a block's PNG; "Open live" shows the block
in a reflowing viewer beside the editor that follows edits and can write
the view back.

**Deliverables:** `vscode/src/lens.js` ("Save PNG…" always; "Open live"
with the panel), `vscode/src/live-panel.js`, `vscode/media/live.js`,
`live.css` (DESIGN section 3.5, messages of section 5.3), the commands in
the manifest; a Playwright test page for `live.js` with a stub
`acquireVsCodeApi`; `CHANGELOG.md` entry. Before starting, decide with
the owner whether step 9's live preview already covers the need; if so,
build only "Save PNG…" and record the decision here and in DESIGN
section 3.5.

**Tests:** VS Code: CodeLenses per block; `nymph.savePng` writes a PNG
equal to the preview's; `nymph.openLive` at a block opens a panel, again
reveals the same panel. Browser: `source` messages update the chart and
keep a zoom done before; a broken source keeps the chart and shows the
status; "Write view to block" posts YAML with a `view` key; malformed
messages are ignored. Node: block re-binding after an insertion above
the bound block and after its deletion.

**Result:** *(to be filled in)*

### 11. Release

**Goal:** a tag push attaches the `.vsix` to the release and, once the
owner adds the tokens, publishes to the VS Code Marketplace and Open VSX.

**Deliverables:** the release's web job builds both variants
(`NYMPH_WEB_JS` in its own folder) and `NYMPH_VSCODE`; the `.vsix` in
the release files and `SHA256SUMS`; two publish jobs skipped without
`VSCE_PAT` / `OVSX_PAT`; the one-time setup (publisher id, tokens) in
`docs/releasing.md`; spec/07 DESIGN's release file list.

**Tests:** a dry run of the workflow on a branch produces the `.vsix`;
`code --install-extension nymph-vscode-<version>.vsix` succeeds in CI's
Linux job and step 6's VS Code test passes against the installed
package.

**Result:** *(to be filled in)*

### 12. Manual verification

**Goal:** the extension is checked by hand where the automated tests
cannot reach.

**Deliverables (recorded here and in DESIGN.md's status line):** VS Code
stable on Linux, Windows and macOS: `docs/examples.md` copied into a
Markdown file previews every example; typing in a block feels fluid (no
visible stall of the editor); clicking a 2D plot, a diagram and a 3D
plot makes them live and they pan smoothly; scrolling past charts never
zooms them; light, dark and high-contrast themes; scroll sync;
Remote-SSH or WSL; a restricted (untrusted) workspace; VSCodium or
Cursor from Open VSX if published.

**Tests:** none automated.

**Result:** *(to be filled in)*

## Completion Criteria

1. Steps 2-9 (and 10 if built) pass in CI together with every existing
   test, which pass unmodified.
2. In VS Code with default settings, a Markdown file with `nymph` fences
   previews them as charts in the editor's theme; a click makes a chart
   live (pan, zoom, values) without changing the preview's security;
   broken blocks show an error panel and an editor diagnostic at the
   right line.
3. A preview whose charts are never clicked loads no renderer; no
   document can make the extension read a file, make a request or run
   code; the extension works in a restricted workspace.
4. The `.vsix` is a release file; `README.md`, `docs/vscode.md`,
   `docs/building.md`, `docs/viewer.md`, `docs/releasing.md`,
   `CHANGELOG.md` and `spec/INDEX.md` are current; DESIGN.md's
   *unverified* marks are resolved or restated as known limits.
