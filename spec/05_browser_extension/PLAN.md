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

**Goal:** the build produces a loadable extension that runs a content
script on its default hosts and does nothing else yet.

**Deliverables:**
- `extension/manifest.json.in` (DESIGN.md section 5.1), `extension/icons/`,
  `extension/src/content/loader.js` (settings read, `import()` of
  `main.js`), `extension/src/content/main.js` (empty start),
  `extension/src/background/service-worker.js` (message skeleton).
- `extension/CMakeLists.txt` (DESIGN.md section 6), added from the root
  `CMakeLists.txt` under `NYMPH_WEB`.
- `extension/tests/browser/harness.mjs`: launches Chromium with the
  extension and a test manifest granting `<all_urls>`, and serves
  fixture pages on two local origins with OS-assigned ports.

**Tests:**
- Browser: on a fixture page the content script runs once and sets a
  test-visible marker; a page on a disabled site gets no `main.js`.
- The zip contains the manifest with the project version and every
  file of the unpacked folder.

### 4. Sniffer and fetcher

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

### 7. Living with the host page, settings and popup

**Goal:** the user can use the chart without breaking the page, switch
back to the image, and turn the extension on and off per site.

**Deliverables:**
- Link handling and the badge (Show image / Show chart / Open image) in
  `replacer.js`; `aria-label` fallback to `alt`.
- `extension/src/popup/` and the settings of DESIGN.md section 5.2;
  per-site permission request and `registerContentScripts`; the
  toolbar badge count.
- Live response to setting changes in `main.js`.

**Tests (browser):**
- Dragging and clicking a chart inside a link does not navigate;
  "Open image" opens the link's target.
- Show image / Show chart toggle one chart only.
- Turning the site off restores every image and removes every host and
  the stylesheet (the page's DOM equals a page loaded with the extension
  off, ignoring the page's own changes); turning it on replaces them
  again.
- `theme: 'auto'` follows an emulated dark colour scheme.

### 8. GitHub

**Goal:** nymph PNGs on GitHub are interactive wherever GitHub shows
them.

**Deliverables:**
- `extension/src/content/sites/github.js` (fetch routing of DESIGN.md
  section 3.6, as adjusted by step 1).
- A fixture page built from a saved GitHub markdown rendering (markup
  and CSP headers, images served locally under the same path shapes).

**Tests:**
- Browser: the GitHub fixture's images are replaced, in light and dark
  mode, with images inside GitHub's self-links.
- Manual, recorded in the result: a README, a markdown file view, an
  issue and a pull request comment with an uploaded PNG, the comment
  preview, a raw PNG file page, dark mode, a private repository, and
  navigation between pages without reload.

### 9. Google Docs

**Goal:** what step 1 found possible in Google Docs works.

**Deliverables:** fixed by step 1's outcome (DESIGN.md section 3.7);
this step is rewritten after step 1, before it starts. Expected: one of
an overlay adapter `sites/gdocs.js` for the editor, generic handling of
`/preview` and `/pub` only, or the linked-source rule.

**Tests:** a fixture reproducing the relevant Docs markup in the
browser suite, and a manual check in a real Doc, recorded in the result.

### 10. Documentation and close

**Goal:** the extension ships as documented files, and every document
matches the code.

**Deliverables:**
- `docs/extension.md`: install (unpacked folder, zip), what is replaced,
  supported sites, enabling other sites, settings, privacy (no data
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

- [ ] All native, Node and browser tests pass under `ctest -j`; every
      committed example PNG is still reproduced byte for byte.
- [ ] On fixture pages, nymph PNGs (and only they) become viewers in the
      image's box, look like the PNG, follow dynamic content, and are
      freed with their images; at most 32 viewers are live.
- [ ] Pages without nymph PNGs load no WebAssembly; non-nymph images are
      read no further than their chunk headers.
- [ ] Every failure leaves the original image; turning a site off
      restores the page.
- [ ] GitHub verified manually in the places step 8 lists.
- [ ] Google Docs works as far as step 1 found possible, and the docs
      say exactly how far.
- [ ] README, `docs/`, CHANGELOG, the DESIGN.md files and
      `spec/INDEX.md` match the code.
