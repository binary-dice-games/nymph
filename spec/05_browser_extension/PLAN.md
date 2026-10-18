# nymph — Browser Extension: Implementation Plan

See [DESIGN.md](DESIGN.md) for the design this plan implements, and
[spec/04_web_viewer/DESIGN.md](../04_web_viewer/DESIGN.md) section 5 for
the viewer API it embeds.

Steps go from what could invalidate the design (step 1), to the one
viewer change, to the extension from the inside out: bytes, then one
static page, then live pages, then the user's controls, then the two
target sites. Every step leaves the existing native, Node and browser
tests passing unmodified. Extension steps need the `NYMPH_WEB` build
(Emscripten); their tests are registered with CTest only there, run
with Node and with Playwright driving the preinstalled Chromium with the
extension loaded. Each step records the Chrome and Emscripten versions
it was verified with.

## Steps

### 1. Feasibility check

*Result: done, partly.* Verified with Emscripten 6.0.12 and Playwright
1.56's Chromium 141 (headless, `channel: 'chromium'`). Answers:
1. **No** on pages whose Content Security Policy lacks
   `'wasm-unsafe-eval'`: compiling the module in a content script fails
   (`CompileError`) under a GitHub-like policy, so the design gained
   frame mode (DESIGN.md section 3.9). In an extension frame the module
   and WebGL2 work under such a page, `frame-src 'self'` included. Also
   found: `customElements` is `null` in content scripts (the viewer now
   checks for it), and Chrome runs no animation frames in an extension
   frame that is hidden or not yet on screen (DESIGN.md section 3.3).
   Not checked on github.com or docs.google.com themselves.
2. **Yes**: a persistent context with `--load-extension` works headless.
3. **Not answered**: the repository is private and the container has no
   signed-in browser, so GitHub's serving of PNGs could not be checked.
4. **Not answered**: no Google account in the container.
DESIGN.md sections 3.6, 3.7, 3.9, 5.1 and 8 were updated with these.

**Goal:** the questions DESIGN.md leaves open are answered with
evidence, and DESIGN.md is updated before any code depends on them.

**Deliverables:**
- A throwaway unpacked extension (not committed) whose content script
  loads the existing `nymph-viewer.js` with `import()` and draws one
  chart.
- Answers, recorded in this step's result and folded into DESIGN.md
  sections 3.6, 3.7, 5.1 and 8:
  1. WebAssembly instantiates and WebGL2 works in a content script on
     `github.com` (strict CSP) and `docs.google.com`, and the minimum
     Chrome version for it.
  2. Playwright's Chromium loads an unpacked extension (`--load-extension`
     with a persistent context) in this container, headless.
  3. GitHub serves byte-identical PNGs (the `nymph` chunk kept) for a
     committed file (`/raw/`), an upload (`user-attachments`,
     `private-user-images`) and an external image (`camo`); which of
     them are CORS-enabled.
  4. Google Docs: the three questions of DESIGN.md section 3.7 (chunk
     kept at the editor's image URL; images' URL and box readable from
     the editor's DOM; `<img>` in `/preview` and `/pub`), and the host
     its images come from.
- DESIGN.md: the Docs outcome row chosen, the fallback of section 8
  dropped or kept, the host permissions completed.

**Tests:** none automated; the result lists each answer with how it was
checked (a downloaded file's chunks, a DOM snapshot).

### 2. `fit: 'scale'` in the viewer

*Result: done.* `view_fit::scale` (`source_geometry()` lets `css_scale`
exceed 1 only in this mode), `fit: 'scale'` in the JS API and element.
4 native tests (scale 2 and 0.5, the framebuffer cap, `source` never
grows, pointer mapping at scale 2) and 1 browser test (CSS width,
device pixels and a hover at scale 2). 430 native tests pass.

**Goal:** a viewer can show a chart laid out at its source size and
scaled to any box width, up or down.

**Deliverables:**
- `src/view/live_view.hpp/.cpp`: `view_fit::scale` (DESIGN.md section
  5.3).
- `web/nymph_web.cpp`, `web/js/nymph-viewer.js`: `fit: 'scale'` and the
  element's `fit="scale"`.
- `docs/viewer.md`, `spec/04_web_viewer/DESIGN.md` section 3 (sizing):
  the new mode.

**Tests:**
- Native (`test_live_view`): at a box of twice the layout width,
  `css_scale` is 2 and the framebuffer is twice the layout at ratio 1;
  at half, 0.5; the 8192 px cap holds; a pointer at box (2x, 2y) maps to
  layout (x, y); `source` still caps at 1.
- Browser: a `fit: 'scale'` viewer in a box twice the PNG's width has a
  canvas of that CSS width, and a hover at a doubled position reports
  the same pick as at scale 1.

### 3. Extension skeleton and packaging

*Result: done.* The harness lives in `extension/tests/browser/run.mjs`
(no separate `harness.mjs`): one server on `127.0.0.1` and `localhost`
with an OS port. The zip (2.1 MB) is built with `cmake -E tar`; its
contents are not checked by a test. The content-script marker test was
folded into the later tests (every one needs the script to run; the
"off" site is covered by step 7's reload check).

**Goal:** the build produces a loadable extension that runs a content
script on every page and does nothing else yet.

**Deliverables:**
- `extension/manifest.json.in` (DESIGN.md section 5.1), `extension/icons/`,
  `extension/src/content/loader.js` (settings read, `import()` of
  `main.js`), `extension/src/content/main.js` (empty start),
  `extension/src/background/service-worker.js` (message skeleton).
- `extension/CMakeLists.txt` (DESIGN.md section 6), added from the root
  `CMakeLists.txt` under `NYMPH_WEB`.
- `extension/tests/browser/harness.mjs`: launches Chromium with the
  extension, and serves
  fixture pages on two local origins with OS-assigned ports.

**Tests:**
- Browser: on a fixture page the content script runs once and sets a
  test-visible marker; a page on a site in the `off` setting gets no
  `main.js`.
- The zip contains the manifest with the project version and every
  file of the unpacked folder.

### 4. Sniffer and fetcher

*Result: done.* 8 Node tests on the sniffer, 1 on the cache, 1 on fetch
routes; the browser test of a cross-origin image without CORS also
checks that the worker's request carries no cookie.

**Goal:** given an image URL, the extension knows whether it is a nymph
PNG, reading only its chunk headers when it is not.

**Deliverables:**
- `extension/src/content/sniff.js`, `fetcher.js`, the URL cache;
  the service worker's fetch-and-sniff handler (DESIGN.md section 4).

**Tests:**
- Node: every committed example PNG is nymph and returned whole; the
  same images re-encoded without the chunk, a wish-style chunk, a chunk
  after `IDAT`, non-PNG bytes, a truncated stream and a chunk length
  over the cap are not nymph; a non-nymph stream is cancelled before
  its `IDAT` data is read; the cache drops the least recently used
  entry at its limit.
- Browser: a same-origin and a cross-origin (no CORS headers) nymph PNG
  are both read; a cross-origin request carries no cookie.

### 5. Replacing images on a static page

*Result: done, with changes.* No `sites/` adapters: nothing needed one
(DESIGN.md section 3.6). The shadow root is open, and the host is no
longer hidden while the viewer starts: it covers the viewer with a copy
of the image instead (DESIGN.md section 3.3), because a hidden frame
never draws. Frame mode was added here (DESIGN.md section 3.9), and the
static-page tests run in both modes. Not tested: a 2x PNG (the sizing
rule is the same as for any image without width rules).

**Goal:** on a page that does not change, every nymph PNG becomes a
viewer in the same box, and nothing else changes.

**Deliverables:**
- `extension/src/content/replacer.js` (host element, closed shadow
  root, sizing rules of DESIGN.md section 3.2, hidden creation and swap
  after `rendered()`, restore on failure), `sites/generic.js`, the
  injected stylesheet, `loadNymph()` pointed into the extension.

**Tests (browser, fixture pages):**
- Only nymph images are replaced; each host's box equals the image's
  box within 1 px, for a natural-size image, a 2x PNG, an image scaled
  by `width`, by `max-width: 100%` in a narrow column, and inside
  `<picture>` and with `srcset`; the canvas matches the PNG within the
  viewer DESIGN browser tolerance.
- Resizing the window reflows the host as it reflowed the image.
- A corrupt PNG, a wish-style PNG and a source the viewer rejects stay
  images; a page without nymph PNGs never requests the `.wasm`.

### 6. Following the page live, and the viewer pool

*Result: done.* The observers live in `main.js` (no `scanner.js`). The
pool test uses 48 charts (enough to fill both caps), in both modes: at
most 32 viewers inline and 12 in frames. In frame mode a chart starts
only when it is on screen, so the tests scroll through the page.

**Goal:** charts appear, follow and go away with the page's content,
with bounded cost.

**Deliverables:**
- `extension/src/content/scanner.js` (observers, idle batches,
  `IntersectionObserver`), `pool.js` (cap 32, parking with `toYaml()`).

**Tests (browser):**
- A feed fixture appending nymph images on scroll: each appears as a
  viewer within 500 ms of entering the viewport margin.
- A framework-like fixture that replaces image nodes and rewrites their
  attributes on every "render": the chart stays, no duplicate host
  appears, a removed image's viewer is destroyed (the pool's count
  drops).
- An image whose `src` changes to a plain PNG goes back to the image;
  one that changes to another nymph PNG shows the new chart.
- 100 nymph images: never more than 32 live viewers; a chart zoomed,
  scrolled far away and back shows the same zoom.
- Node: the pool's parking order (farthest first, never under the
  pointer or inside the margin).

### 7. Living with the host page, the toolbar button and options

*Result: done.* The button is driven in tests through the service
worker's `nymphToggleTab()` (Playwright cannot click the toolbar). The
badge's buttons are "Image" / "Chart" and "Open". 22 browser tests in
all, `ctest` of the web build passes (4 tests: web Node and browser,
extension Node and browser).

**Goal:** the user can use the chart without breaking the page, switch
back to the image, and turn the extension on and off for a site with
one click on its button.

**Deliverables:**
- Link handling and the badge (Show image / Show chart / Open image) in
  `replacer.js`; `aria-label` fallback to `alt`.
- The toolbar button of DESIGN.md section 3.8 in the service worker
  (click toggle, per-tab icon, title and badge, disabled on restricted
  pages); `extension/src/options/` and the settings of section 5.2;
  the first-run tab asking to pin the button.
- Live response to setting changes in `loader.js` and `main.js`.

**Tests (browser):**
- Dragging and clicking a chart inside a link does not navigate;
  "Open image" opens the link's target.
- Show image / Show chart toggle one chart only.
- Clicking the button turns the site off: every image restored, every
  host and the stylesheet removed (the page's DOM equals a page loaded
  with the extension off, ignoring the page's own changes), in every
  open tab of the site, the icon grey and the badge empty; clicking
  again replaces them and the badge counts the charts; the choice
  survives a reload; a second site stays on throughout.
- Removing a site from the options page's list turns it back on.
- `theme: 'auto'` follows an emulated dark colour scheme.

### 8. GitHub

*Result: automated part done; manual check pending.* No GitHub-specific
code was needed: fetch routing is generic, and GitHub's policy selects
frame mode. The fixtures run with a GitHub-like policy and markdown-like
markup (images in self-links) instead of a saved GitHub page. The manual
check below needs a signed-in Chrome and is not done.

**Goal:** nymph PNGs on GitHub are interactive wherever GitHub shows
them.

**Deliverables:**
- Fixture pages with GitHub's policy shape and markdown markup.

**Tests:**
- Browser: the fixtures' images are replaced in frame mode, inside
  self-links.
- Manual, recorded here: a README, a markdown file view, an issue and a
  pull request comment with an uploaded PNG, the comment preview, a raw
  PNG file page, dark mode, a private repository, and navigation between
  pages without reload.

### 9. Google Docs

*Result: open.* Step 1 could not answer the Docs questions, so only
generic `<img>` handling exists (DESIGN.md section 3.7).

**Goal:** find what Google Docs allows, then make it work.

**Deliverables:** the answers to DESIGN.md section 3.7's three
questions, from a real Doc in a signed-in Chrome with the extension
loaded (does the image URL the editor loads keep the `nymph` chunk; does
the editor's DOM expose image URLs and boxes; do `/preview` and `/pub`
use `<img>`); then, by the outcome table there, nothing more, an overlay
adapter for the editor, or the linked-source rule.

**Tests:** a fixture reproducing the relevant Docs markup in the
browser suite, and a manual check in a real Doc, recorded here.

### 10. Documentation and close

*Result: done* (`docs/extension.md`, README, `docs/building.md`,
`docs/viewer.md`, the web viewer DESIGN, CHANGELOG, `spec/INDEX.md`).

**Goal:** the extension ships as documented files, and every document
matches the code.

**Deliverables:**
- `docs/extension.md`: install (unpacked folder, zip), what is replaced,
  sites given special care, pinning and using the toolbar button,
  options, privacy (no data
  leaves the browser), limits (re-encoding services, shadow roots,
  canvases); a row in README's "Further Documentation".
- `README.md`: the extension in the overview and the layout table
  (`extension/`); `docs/building.md`: the extension outputs.
- `CHANGELOG.md`: the extension and `fit: 'scale'` under
  `[Unreleased]`.
- DESIGN.md status and any change made during implementation;
  `spec/INDEX.md` status and topics.

**Tests:** `ctest -j` passes three times in a row in the `NYMPH_WEB`
build, and natively.

## Completion Criteria

- [x] All native, Node and browser tests pass under `ctest -j`; every
      committed example PNG is still reproduced byte for byte.
      *430 native tests; 4 CTest tests in the web build.*
- [x] On fixture pages, nymph PNGs (and only they) become viewers in the
      image's box, look like the PNG, follow dynamic content, and are
      freed with their images; at most 32 viewers are live (12 in frame
      mode).
- [x] Pages without nymph PNGs load no WebAssembly; non-nymph images are
      read no further than their chunk headers.
- [x] Every failure leaves the original image; turning a site off
      restores the page.
- [ ] GitHub verified manually in the places step 8 lists.
- [ ] Google Docs works as far as possible, and the docs say exactly
      how far. *The docs say what is known; the questions are open.*
- [x] README, `docs/`, CHANGELOG, the DESIGN.md files and
      `spec/INDEX.md` match the code.
