# nymph — VS Code Markdown Preview: Implementation Plan

Design: [DESIGN.md](DESIGN.md). Builds on the web viewer
([spec/04_web_viewer](../04_web_viewer/DESIGN.md)) and the release
pipeline ([spec/07_release_pipeline](../07_release_pipeline/DESIGN.md)).

Steps go from what could invalidate the design (step 1), to the pure
pieces (2-4), to the extension in VS Code (5-8), to the optional live
preview (9), to release and manual checks (10-11). Each step is a
reviewable deliverable and ends with every existing native, Node and
browser test passing unmodified. The extension's tests live in
`vscode/tests/` and are registered in CTest when `NYMPH_VSCODE` is on.
Steps with user-visible behaviour (5 onwards) add a `CHANGELOG.md`
entry. Steps that run VS Code record its version.

## Steps

### 1. Feasibility check

**Goal:** the facts DESIGN.md marks *unverified* are answered in a real
VS Code, and DESIGN.md is updated before code depends on them.

**Deliverables:** a throwaway extension (not committed) with a
markdown-it plugin that emits a fixed `data:` PNG for `nymph` fences and
logs timings; the answers recorded in this step's **Result** and folded
into DESIGN.md sections 3, 4, 5.1 and 9.

1. Does VS Code await an `async activate()` before calling
   `extendMarkdownIt` (first preview of a fresh window)?
2. Does `markdown.api.render` include the plugin's output (the test hook
   of step 5)?
3. With the fence rule wrapped and `code-line` / `data-line` kept, do
   scroll sync and double-click-to-edit work on the chart?
4. Does a `command:` link in the preview run an extension command, and
   with what restrictions?
5. Confirm the Strict CSP blocks `WebAssembly.compile` in a preview
   script, and that "Disable" allows it (section 3.5).
6. Load `nymph-viewer-core.js` in the extension host (Node) and time a
   synchronous software render of every file in `examples/` at scale 2:
   load time, p50, p95, slowest; module memory. Decide sync or worker
   rendering (DESIGN section 9) here.
7. Remote: the same extension over Remote-SSH or WSL and in a dev
   container; which host runs the plugin.
8. The oldest VS Code version where items 1-3 hold (for
   `engines.vscode`).

**Tests:** none automated.

**Result:** *(to be filled in)*

### 2. Stateless exports in the web module

**Goal:** the WebAssembly core can render a source to a PNG with a theme
and scale override, and check a source, without a viewer.

**Deliverables:** `nymph_render_png` and `nymph_check` in
`web/nymph_web.cpp` (DESIGN section 5.4), with Doxygen comments; any
library helper they need for the overrides; viewer DESIGN section 4
("Web module") notes the two exports.

**Tests** (`web/tests/node/run.mjs`): a valid source renders, the PNG's
embedded YAML equals the input byte for byte; `theme: dark` override
gives the same pixels as the source with `image.theme: dark`; scale 2
doubles both dimensions; a broken source returns 0 with the line, column
and message `nymph check` prints; a source naming a non-embedded font
fails with `fonts_needed` naming it, and succeeds after
`nymph_register_font`; `nymph_check` agrees with `nymph check` on the
`tests/` error fixtures.

**Result:** *(to be filled in)*

### 3. `NymphRenderer`

**Goal:** Node code renders and checks sources synchronously with the
core, cached, with fonts loaded on demand from a folder.

**Deliverables:** `vscode/src/renderer.js` (DESIGN section 4.3);
`vscode/tests/node/run.mjs` (dependency-free runner, the style of
`extension/tests/node/run.mjs`); CTest `nymph_vscode_node`. If step 1
chose worker rendering, also the worker and the async `render()`.

**Tests:** render result shape, description, size at scales 1 and 2;
same text twice is one core call (counted through a wrapped core);
different theme is a second call; LRU eviction at the entry and byte
limits; errors are cached too; a font is read once and only for catalog
names (a source naming `../x.ttf` is a plain error, no read); `check()`
never rasterizes.

**Result:** *(to be filled in)*

### 4. Fences: `renderBlock` and `findBlocks`

**Goal:** a `nymph` fence token becomes the chart or error HTML of
DESIGN sections 3.2 and 3.3, and fences are found in raw text the same
way markdown-it finds them.

**Deliverables:** `vscode/src/fence.js` (`nymphFence`, `renderBlock`),
`vscode/src/blocks.js` (`findBlocks`), fixtures under
`vscode/tests/fixtures/`.

**Tests** (Node, fake tokens and a fake renderer): `nymph`, `Nymph`,
`nymph extra words` match, `nymphx` and `yaml` do not; non-nymph tokens
go to the previous rule; `code-line` / `data-line` copied; `<`, `&`,
quotes escaped in message, `alt` and source; error line mapping
(block line 18 at fence line 11 is document line 30); the 256 KiB limit;
a throwing renderer gives an error panel; `findBlocks` on backticks,
tildes, longer closing runs, indented fences, unterminated fence at end
of file, a fence inside a list item.

**Result:** *(to be filled in)*

### 5. The extension in the preview, and its package

**Goal:** installing the `.vsix` makes `nymph` fences draw in VS Code's
Markdown preview.

**Deliverables:** `vscode/package.json.in` (DESIGN section 5.1, without
the live-panel commands yet), `vscode/src/extension.js`,
`vscode/preview/nymph-preview.css`, `vscode/CMakeLists.txt`,
`cmake/NymphVsix.cmake`, the `NYMPH_VSCODE` option in the root
`CMakeLists.txt` and `docs/building.md`; `vscode/tests/vscode/run.mjs`
and CTest `nymph_vscode_integration` (77 when `@vscode/test-electron` is
missing); `docs/vscode.md` and its row in `README.md`; `CHANGELOG.md`:
"Added: a VS Code extension that draws `nymph` code blocks in the
Markdown preview."

**Tests:** Node: the staged folder and the `.vsix` contain the manifest,
`extension.vsixmanifest`, `[Content_Types].xml`, the core, `fonts/`;
the manifest's version is the project's. VS Code: `markdown.api.render`
of a fixture with one good and one broken block returns one
`nymph-chart` with a decodable PNG and one `nymph-error` with the right
document line; a `mermaid` block in the same file renders as before.

**Result:** *(to be filled in)*

### 6. Theme and settings

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

### 7. Diagnostics and highlighting

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

### 8. CodeLens and the live panel

**Goal:** "Open live" shows the block in an interactive viewer beside the
editor that follows edits; "Save PNG…" writes the block's PNG.

**Deliverables:** `vscode/src/lens.js`, `vscode/src/live-panel.js`,
`vscode/media/live.js`, `live.css` (DESIGN section 3.4, messages of
section 5.3), the commands in the manifest, the preview link if step 1
item 4 allows it; a Playwright test page for `live.js` with a stub
`acquireVsCodeApi` under `vscode/tests/browser/`; `CHANGELOG.md` entry.

**Tests:** VS Code: one CodeLens pair per block; `nymph.openLive` at a
block opens a panel, again reveals the same panel; `nymph.savePng`
writes a PNG equal to the preview's. Browser (Playwright, 77 when
missing): `source` messages update the chart and keep a zoom done before;
a broken source keeps the chart and shows the status; "Write view to
block" posts YAML with a `view` key; malformed messages are ignored.
Node: block re-binding after an insertion above the bound block and after
its deletion.

**Result:** *(to be filled in)*

### 9. Live charts in a preview without a policy

**Goal:** with the preview's security set to "Disable", charts in the
preview become live viewers; with any other setting nothing changes.

**Deliverables:** `vscode/preview/nymph-preview.js` (DESIGN section 3.5)
and its manifest entry; one line in `docs/vscode.md`; `CHANGELOG.md`
entry.

**Tests:** Browser: the script on a page with the Strict CSP leaves the
images and makes no request; on a page without a CSP it replaces an
image with a viewer showing the same source (`viewer.source()` equals
the PNG's YAML).

**Result:** *(to be filled in)*

### 10. Release

**Goal:** a tag push attaches the `.vsix` to the release and, once the
owner adds the tokens, publishes to the VS Code Marketplace and Open VSX.

**Deliverables:** `NYMPH_VSCODE` in the release's web build and the
`.vsix` in the release files and `SHA256SUMS`; two publish jobs skipped
without `VSCE_PAT` / `OVSX_PAT`; the one-time setup (publisher id,
tokens) in `docs/releasing.md`; spec/07 DESIGN's release file list.

**Tests:** a dry run of the workflow on a branch produces the `.vsix`;
`code --install-extension nymph-vscode-<version>.vsix` succeeds in CI's
Linux job and step 5's VS Code test passes against the installed
package.

**Result:** *(to be filled in)*

### 11. Manual verification

**Goal:** the extension is checked by hand where the automated tests
cannot reach.

**Deliverables (recorded here and in DESIGN.md's status line):** VS Code
stable on Linux, Windows and macOS: `docs/examples.md` copied into a
Markdown file previews every example; typing in a block feels fluid
(no visible stall of the editor); light, dark and high-contrast themes;
scroll sync; live panel on two blocks at once; Remote-SSH or WSL; a
restricted (untrusted) workspace; VSCodium or Cursor from Open VSX if
published.

**Tests:** none automated.

**Result:** *(to be filled in)*

## Completion Criteria

1. Steps 2-9 tests pass in CI together with every existing test, which
   pass unmodified.
2. In VS Code with default settings, a Markdown file with `nymph` fences
   previews them as charts in the editor's theme, broken blocks show an
   error panel and an editor diagnostic at the right line, and "Open
   live" gives an interactive chart that follows edits.
3. No document can make the extension read a file, make a request or run
   code; the extension works in a restricted workspace.
4. The `.vsix` is a release file; `README.md`, `docs/vscode.md`,
   `docs/building.md`, `docs/releasing.md`, `CHANGELOG.md` and
   `spec/INDEX.md` are current; DESIGN.md's *unverified* marks are
   resolved or restated as known limits.
