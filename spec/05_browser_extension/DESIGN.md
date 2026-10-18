# nymph — Browser Extension: Design

**Status: design; not started.** [PLAN.md](PLAN.md) is the ordered
implementation plan; its step 1 is a feasibility check whose results may
change sections 3.6, 3.7 and 8 of this document. This document
describes only what this project adds or changes; the viewer it embeds
is [spec/04_web_viewer/DESIGN.md](../04_web_viewer/DESIGN.md)
(*viewer DESIGN*), the PNG format is
[spec/01_core_features/DESIGN.md](../01_core_features/DESIGN.md)
section 4, `png` (*core DESIGN*).

## 1. Purpose / Scope

A nymph PNG carries its YAML source (an `iTXt` chunk with keyword
`nymph`). The web viewer can draw such a PNG live, but only on pages
whose authors embed it. This project adds a **Chrome extension** (Manifest
V3) that does it on any page the user reads: a content script finds the
nymph PNGs on the page and replaces each with a nymph viewer of the same
size, so every chart becomes interactive (pan, zoom, values on hover,
save as PNG or YAML). Plain PNGs and every other image are left alone.

Pages change after they load (comment threads, previews, feeds that load
more content as the user scrolls), so the extension **follows the page
live**: images added, lazily loaded, re-rendered or changed later are
handled as they appear, and viewers whose image goes away are freed.

The main use cases, which get site-specific care and manual verification:

1. **GitHub**: markdown rendered in READMEs, files, issues, pull
   requests, discussions, wikis and the comment preview; repository files
   (`github.com/<o>/<r>/raw/...`), uploads (`user-attachments`,
   `private-user-images.githubusercontent.com`) and external images
   through GitHub's image proxy (`camo.githubusercontent.com`).
2. **Google Docs** (`docs.google.com/document/...`): see section 3.7; how
   far this is possible is decided by the feasibility check.

In scope: the extension (`extension/`): manifest, content script,
service worker, the toolbar button (section 3.8), an options page,
packaging into an unpacked folder and a
zip from the `NYMPH_WEB` build; one viewer change, `fit: 'scale'`
(section 5.3); tests with fixture pages in Chromium.

Out of scope: Firefox and Safari (the code avoids Chrome-only APIs where
an equivalent exists, so a port stays small); images inside shadow
roots, cross-origin iframes the extension is not granted, `<canvas>` and
CSS background images (except as section 3.7 allows for Google Docs);
editing charts; publishing to the Chrome Web Store (a later release
task; running on every site needs `<all_urls>`, which the store
listing must justify).

## 2. Design Goals

1. **Indistinguishable until touched.** A replaced image keeps its place,
   its size and its look: the viewer shows what the PNG shows (same
   layout, same theme) in the same box, and appears only once its first
   frame is drawn, so nothing flickers or reflows.
2. **Never break the page.** Plain images, links, scrolling, the page's
   framework and its layout keep working. Any failure (fetch, a corrupt
   or foreign PNG, a source the viewer rejects, a newer nymph format)
   leaves the original image in place.
3. **Live.** Images that appear, load, change or leave after the first
   scan are handled within about 200 ms of becoming visible, with no
   polling.
4. **Cheap on pages without charts.** No WebAssembly is loaded and no
   image is downloaded twice in full until a nymph PNG is found; deciding
   that an image is not a nymph PNG reads at most its first chunks,
   usually from the HTTP cache.
5. **Bounded on pages with many charts.** Only images near the viewport
   become viewers, and the number of live viewers is capped; the user's
   zoom survives a viewer being freed and recreated.
6. **Everywhere, one click to turn off.** The extension runs on every
   site, with no list of allowed sites; the user turns it off (and on
   again) for the site in the current tab with one click on its button
   next to the address bar (section 3.8).
7. **Private.** No data leaves the browser; the extension fetches only
   image URLs the page already shows.

## 3. Behaviour

### 3.1 What is replaced

An `<img>` (including one inside `<picture>`) is a **candidate** when it
is connected, rendered (not `display: none`), its `currentSrc` is
`http(s):`, `data:` or `blob:`, its natural size is at least 32 x 32 and
its rendered box at least 24 x 24 CSS px (skips icons, avatars and
tracking pixels). A candidate is a **nymph image** when its bytes are a
PNG with a `nymph` `iTXt` chunk before the first `IDAT` (section 4,
sniffer) and the viewer accepts it (`setPng()` resolves `true`). wish
nymph images use the same keyword but another layout; the viewer rejects
them and they stay images.

### 3.2 Sizing: the same box

The viewer goes into a **host element** inserted right after the image,
which mirrors the image's sizing rules rather than a snapshot of its
pixels, so it reflows as the image would:

- `display` as the image (`inline-block` for inline images), the image's
  `width` / `height` attributes and computed `width`, `max-width`,
  `max-height`, `margin`, `vertical-align` and `float`, and
  `aspect-ratio` = natural width / natural height;
- when the image has no width rule, `width` = its natural width in CSS
  px, as the browser shows an image without one (so a 2x PNG on GitHub
  appears twice as large, as it does today).

The viewer uses the new `fit: 'scale'` (section 5.3): the layout runs at
the source's `image.width` x `height`, exactly as the PNG was drawn, and
is scaled up or down to the host's width, the framebuffer following the
host's size times the device pixel ratio so text stays sharp. The chart
therefore looks like the image, sharper when the image was scaled up.

The theme is the source's (`theme: 'source'`), so the chart looks like
the PNG; a setting lets the user follow the page instead (`'auto'`).

### 3.3 Swapping in and out

1. The viewer is created inside the host while the host is hidden
   (`visibility: hidden`, positioned absolutely over the image's box), so
   the viewer has its size while the image is still the one shown.
2. When `rendered()` resolves, in one task: the image gets the attribute
   `data-nymph-ext="hidden"` (an injected stylesheet hides
   `img[data-nymph-ext="hidden"]` with `display: none !important`) and
   the host becomes visible in its normal flow position.
3. **Back to the image**: a small badge in the host's top-right corner,
   shown while the pointer is over the chart, switches between the chart
   and the original image for that chart ("Show image" / "Show chart").
   Turning the extension or the site off restores every image (section
   3.5).

Hiding by attribute and stylesheet, not by an inline style or by removing
the image, leaves the node to the page's framework: React, Turbo and
similar libraries keep reconciling the element they own, and if they
replace it the new element is simply a new candidate (and a cache hit).

### 3.4 Living with the host page

- **Links.** Images are often inside a link (GitHub wraps every markdown
  image in a link to itself). The host stops `click` and `auxclick` from
  bubbling out of the chart and prevents their default, so panning does
  not navigate; the badge offers "Open image" to keep the link's use.
- **Wheel and touch**: the viewer's own rules (viewer DESIGN section 5,
  input mapping) already scroll the page except over a plot or diagram.
- **Styles.** The viewer lives in a closed shadow root inside the host,
  so page CSS (`.markdown-body img`, `canvas { ... }`) cannot reach it
  and the extension's CSS cannot leak out. The only page-level CSS is
  the one rule of section 3.3.
- **Accessibility.** The canvas's `aria-label` is the source's
  `description`, or the image's `alt` when the source has none.
- **Keyboard and the page's own handlers** are untouched: the extension
  adds listeners only on its host and one capture-phase `load` listener
  on the document (section 3.5).

### 3.5 Following the page live

- A `MutationObserver` on the document element (`childList`, `subtree`,
  `attributes` filtered to `src`, `srcset`, `sizes` and
  `data-nymph-ext`) collects added images (also inside added subtrees),
  changed images and removed nodes into a pending set.
- A capture-phase `load` listener on the document catches images whose
  `currentSrc` changes without an attribute change (`srcset` picking
  another candidate on resize, lazy loading).
- Pending work runs in batches from `requestIdleCallback` (timeout
  200 ms), never inside the observer callback.
- Candidates are registered with an `IntersectionObserver`
  (`rootMargin` one viewport height above and below); only intersecting
  ones are sniffed and turned into viewers, so a feed with hundreds of
  images costs only what the user scrolls near.
- An image that leaves the document takes its viewer with it (the host
  is removed and the viewer destroyed). An image whose `currentSrc`
  changes goes back to the image and is processed again. A replaced
  image whose `data-nymph-ext` attribute is removed by the page is
  hidden again if its host is still there, else processed again.
- Results are cached per URL for the page's lifetime (nymph or not, up
  to 4,000 URLs, least recently used dropped), so re-rendered and
  repeated images (avatars in a feed) cost one lookup.
- **Viewer pool.** At most 32 live viewers per page. When a new one is
  needed, the viewer farthest from the viewport that is outside the
  `IntersectionObserver` margin is **parked**: its `toYaml()` is kept,
  it is destroyed and its image is shown again. When the image comes
  near again it gets a viewer from the kept YAML (`setSource()`), so the
  user's zoom and hidden series come back.
- Turning on and off applies live (section 3.8): turning on starts the
  scan; turning off destroys every viewer, removes every host and the
  stylesheet and restores every image, without reloading the page.

### 3.6 GitHub

Generic `<img>` handling covers GitHub's markdown; the GitHub adapter
adds only:

- `github.com/<o>/<r>/raw/...` URLs are fetched from the content script
  (same origin, with the user's cookies, so private repositories work);
  the redirect to `raw.githubusercontent.com` carries its own token and
  is CORS-enabled;
- `camo` and `private-user-images` URLs are fetched by the service
  worker (section 4, fetcher) without credentials; the latter are signed
  and short-lived, which is fine because the content script fetches
  `currentSrc` as soon as the image is near the viewport;
- `<picture>` elements with `#gh-dark-mode-only` / `#gh-light-mode-only`
  sources work through `currentSrc`;
- navigation without reloads (Turbo, React routes) needs nothing more:
  replaced subtrees are mutations like any other.

Feasibility step 1 checks that GitHub serves uploaded and proxied PNGs
byte for byte (the `iTXt` chunk survives).

### 3.7 Google Docs

Google Docs draws document pages on `<canvas>` elements (since 2021)
rather than as HTML, so there may be no `<img>` to replace, and the
service may re-encode inserted images, which drops the chunk (README:
"services that re-encode images drop it"). Feasibility step 1 answers:

1. Does an image inserted into a Doc keep its `nymph` chunk at the URL
   the editor loads it from?
2. Does the editor's DOM expose each image's URL and box (an `<img>`, an
   SVG `<image>` in Docs' accessibility / annotation layer, or anything
   else readable from an isolated content script)?
3. Do the read-only views (`/preview`, published `/pub`) render images as
   `<img>`?

The **Docs adapter** implements the outcome:

| Outcome | What the extension does in Docs |
|---|---|
| Bytes kept, images in the DOM | An adapter maps each image to a URL and a box; the host is an absolutely positioned overlay over the canvas, re-positioned on scroll and resize (the canvas is not ours to replace). |
| Bytes kept, images only in the canvas | Read-only views only (`/preview`, `/pub`) through generic `<img>` handling; the editor is not supported. |
| Bytes re-encoded | **Linked source**: an image whose link (Docs lets users link an image) points to a nymph PNG or a `.yaml` / `.yml` nymph source is replaced with that source, wherever the image's box can be found (row 1 or 2). |

The linked-source rule is general (any site): it applies only when the
image itself is not a nymph PNG, its enclosing `<a href>` points to a URL
ending in `.png`, `.yaml` or `.yml` on a host the extension may fetch,
and that URL's content is a nymph PNG or a source the viewer accepts.
It is built only if step 1 shows Docs needs it.

### 3.8 The toolbar button

The extension's button (the `action`, pinned next to the address bar)
shows and toggles its state for the current tab's site, like the
Translate icon does for translation:

- **Icon**: the nymph icon in colour when the extension is on for the
  tab's site, greyed out when off. The **badge** shows the number of
  charts replaced on the tab (empty when none). The **title** (tooltip)
  says "nymph: on for <site>, 3 charts. Click to turn off." or "nymph:
  off for <site>. Click to turn on."
- **Click**: toggles the site (the tab's origin) on or off. The choice
  is stored (section 5.2) and applies at once to every open tab of that
  site, without reloading: off restores every image, on scans the page
  (section 3.5).
- **Context menu** (right click on the button): "Options" (theme, and
  the list of sites turned off, each removable).
- Pages where extensions cannot run (`chrome://`, the Web Store, PDF
  viewer) show the button disabled, with a title saying so.
- On install, an options tab opens once and asks the user to pin the
  button (Chrome does not let an extension pin itself).

Every site is on until the user turns it off: there is no list of
allowed sites.

## 4. Key Abstractions

All in `extension/src/`, plain ES modules, no framework, no build step
beyond copying.

| Module | Role | Invariants |
|---|---|---|
| `content/loader.js` | The declared content script, on every page (classic script, as MV3 requires): reads the settings for this site and, when on, `import()`s `content/main.js` from the extension; listens for the toggle message (section 3.8). | Does nothing else: on a site turned off, and until a page has images, it costs one storage read. |
| `content/scanner.js` | The observers of section 3.5; turns DOM changes into candidates for the active site adapter. | Never runs work inside an observer callback; never touches nodes it did not create except the one attribute of section 3.3. |
| `content/sniff.js` | `sniffPng(reader) -> {nymph: boolean, bytes?}`: reads a byte stream, checks the PNG signature, walks chunk headers (length, type) up to the first `IDAT`, and answers whether a `iTXt` chunk with keyword `nymph` is among them; on yes, reads the rest of the stream (up to 32 MiB) and returns all bytes; on no, cancels the stream. | Pure over bytes (tested in Node); reads at most the chunk headers before `IDAT` and the keyword of each `iTXt`; never trusts a chunk length beyond the cap. |
| `content/fetcher.js` | Gets an image's byte stream: `data:` decoded in place; same-origin and `blob:` URLs with `fetch(url, {cache: 'force-cache', credentials: 'same-origin'})` in the content script; cross-origin URLs through the service worker. At most 4 fetches at a time. | Fetches only the `currentSrc` of a candidate (or a linked source, section 3.7); never a URL taken from inside a chart's YAML. |
| `background/service-worker.js` | Cross-origin fetches for content scripts (host permissions lift CORS there), with `credentials: 'omit'`; sniffs first and returns only nymph PNG bytes (base64 over `chrome.runtime` messaging) or "not nymph". Also owns the toolbar button (section 3.8): toggles, per-tab icon, title and badge. | Answers only messages from its own content scripts; enforces the same caps as the sniffer. |
| `content/replacer.js` | Creates the host (section 3.2), the shadow root and the viewer (`createViewer` from the bundled `nymph-viewer.js`), swaps in after `rendered()`, the badge, link handling; restores the image. | The image is hidden only after the chart is drawn; every failure path restores the image and records the URL as "not nymph" for this page. |
| `content/pool.js` | The viewer pool of section 3.5: cap, parking by distance to the viewport, parked YAML. | Never parks a viewer under the pointer or within the observer margin. |
| `content/sites/*.js` | Site adapters: `generic` (images), `github` (fetch routing), `gdocs` (per section 3.7). An adapter answers `candidates(root)`, `anchor(candidate)` (where the host goes and how it is sized: in flow after the image, or an overlay) and `fetchRoute(url)`. | The generic adapter is the default for every site; adapters only narrow or extend it. |
| `options/` | The options page (from the button's context menu): theme (`source` or `auto`) and the list of sites turned off, each removable. | Writes only `chrome.storage.sync`; content scripts follow its changes live. |

The bundled viewer (`viewer/`: `nymph-viewer.js`, `nymph-viewer-core.js`,
`.wasm`, `fonts/`) is the unchanged output of the `NYMPH_WEB` build,
loaded once per page on the first nymph image with
`loadNymph({wasmUrl, fontBaseUrl})` pointing into the extension.

## 5. Contracts

### 5.1 Manifest and permissions

- Manifest V3; `minimum_chrome_version` 116 (WebAssembly in content
  scripts and the `wasm-unsafe-eval` CSP keyword; confirmed by step 1).
- `permissions`: `storage`.
- `host_permissions`: `<all_urls>`, granted on install: the content
  script runs everywhere, and the service worker can read cross-origin
  images on any site.
- `content_scripts`: `content/loader.js` on `<all_urls>`,
  `run_at: document_idle`, `all_frames: false` (main frame only).
- `action`: the toolbar button, no `default_popup` (a click toggles,
  section 3.8), icons for on and off.
- `options_ui`: `options/options.html`.
- `web_accessible_resources`: `content/*`, `viewer/*` for `<all_urls>`,
  with `use_dynamic_url: true` so pages cannot use the resources to
  detect the extension.
- `content_security_policy.extension_pages`:
  `script-src 'self' 'wasm-unsafe-eval'; object-src 'self'`.

### 5.2 Settings (`chrome.storage.sync`)

| Key | Type | Default | Meaning |
|---|---|---|---|
| `off` | array of origins | `[]` | Sites the user turned off with the button; every other site is on. |
| `theme` | `"source"` \| `"auto"` | `"source"` | The viewers' theme (section 3.2). |

### 5.3 Viewer change: `fit: 'scale'`

A fourth fit mode for the viewer, useful beyond the extension (a chart
in a slide, a thumbnail):

- `live_view` gains `view_fit::scale`: the layout runs at the source's
  `image.width` x `height` (as `source`), and `geometry.css_scale` is
  `box width / layout width` with no upper bound of 1 (`source` caps it
  at 1, viewer DESIGN section 3, sizing); the CSS height follows the
  aspect ratio; `framebuffer_scale` stays `css_scale x ratio` under the
  8192 px cap.
- JavaScript: `fit: 'scale'` in `createViewer()` and the `fit` attribute
  of `<nymph-viewer>`; `docs/viewer.md` documents it.
- Pointer mapping already divides by `css_scale` and needs no change.

### 5.4 DOM markers

The only marks the extension leaves in the page: the attribute
`data-nymph-ext="hidden"` on hidden images, the host element
`<nymph-ext-chart>` (an unregistered custom element name, so the page's
`<nymph-viewer>` definition, if any, is never involved), and one
`<style>` element with `id="nymph-ext-style"`. Turning the extension off
removes all three.

## 6. Build and Packaging

- `extension/CMakeLists.txt`, added by the `NYMPH_WEB` build after
  `web/`: assembles `build-web/extension/nymph/` (the unpacked
  extension: `manifest.json` with the project version, `src/`, `icons/`,
  and `viewer/` copied from `build-web/web/`), and
  `build-web/extension/nymph-extension-<version>.zip` with
  `cmake -E tar --format=zip`.
- Icons (16, 32, 48, 128 px) are committed PNGs made once from
  `resources/Nymph.png`.
- No bundler, transpiler or npm dependency at run time; the extension's
  JavaScript is the shipped source.

## 7. Testing Strategy

1. **Native GoogleTest**: `fit: scale` geometry (`css_scale` above and
   below 1, framebuffer cap, pointer mapping at scale 2) in the
   `live_view` tests.
2. **Node** (`extension/tests/node/`): the sniffer over every committed
   example PNG (nymph), PNGs re-encoded without the chunk, a wish-style
   chunk, a chunk after `IDAT`, truncated and oversized lengths, non-PNG
   bytes, and a stream that must be cancelled after the headers; the
   URL cache; the pool's parking order; the host sizing rules from a
   computed-style object.
3. **Browser** (`extension/tests/browser/`, Playwright with the
   preinstalled Chromium and `--load-extension`): fixture pages served
   on two local origins (`127.0.0.1` and `localhost`, ports from the OS)
   each asserting what the user would see:
   - a markdown-like page with nymph and plain PNGs: only the nymph ones
     become viewers, with the image's box (within 1 px) and pixels close
     to the PNG (the viewer DESIGN browser tolerance);
   - images in links (no navigation on drag or click), in `<picture>`,
     with `srcset`, `loading="lazy"`, scaled by CSS, a 2x PNG;
   - a feed that appends images on scroll, a framework-like page that
     replaces and re-renders image nodes, an image whose `src` changes,
     an image removed: viewers appear, follow and are freed;
   - 100 nymph images: at most 32 viewers live, a parked chart comes
     back with its zoom;
   - a strict CSP page (GitHub's policy headers) and a cross-origin
     image without CORS (through the service worker);
   - corrupt and wish-style PNGs stay images; clicking the button turns
     the site off in every open tab of it (every image restored, grey
     icon, empty badge) and on again (charts back, badge count), and
     the choice survives a reload; another site stays on;
   - a page without nymph images loads no `.wasm`.
4. **Manual**, recorded in PLAN.md results: GitHub (README, issue,
   pull request comment with an uploaded PNG, comment preview, dark
   mode, private repository) and Google Docs (per step 1's outcome).

## 8. Design Decisions

- **Content script world, one module per page.** The viewer runs in the
  content script's isolated world: one WebAssembly instance and one
  shared WebGL context for all charts on the page (viewer DESIGN section
  8), direct DOM access for sizing, and no page script can call into
  it. Rejected: one extension `<iframe>` per chart (a module instance
  and a WebGL context each, which hits the browser's context cap on a
  long thread, and frames are hard to size to an inline image);
  injecting into the page's main world (the page could reach the
  viewer, and the page's CSP would govern WebAssembly). If step 1 finds
  WebAssembly blocked in content scripts on a target site, the
  per-chart iframe becomes that site's fallback.
- **Re-fetch the bytes; never decode pixels.** An `<img>` exposes only
  decoded pixels, which have lost the chunk, and `webRequest` cannot
  read response bodies in MV3. Re-fetching with `force-cache` usually
  hits the HTTP cache, and the sniffer stops after the chunk headers.
- **Sniff in JavaScript, parse in C++.** The sniffer only reads chunk
  headers and one keyword, so pages without charts never load the 630 KB
  module; the source itself is read by the library's `read_source()`
  (CRCs, decompression limits) through `setPng()`, so the format has one
  parser. Rejected: extracting the YAML in JavaScript (a second
  implementation of the chunk format).
- **Chunk before `IDAT` only.** nymph writes the chunk right after
  `IHDR`; looking further would mean reading every image in full.
  Tools that move it after the image data lose the chart, which is
  acceptable.
- **Hide the image, don't remove it.** The page's framework owns the
  node; removing it breaks reconciliation, and an inline style is
  overwritten on the next render. An attribute plus one stylesheet rule
  survives re-renders, and the original is there to go back to.
- **Mirror sizing rules, not pixel sizes.** A snapshot of the image's
  box breaks on resize and responsive layouts; copying the image's
  sizing rules and aspect ratio reflows like the image did.
- **`fit: 'scale'` rather than relayout.** Laying out at the box size
  (`fit: 'fill'`) would change line breaks and font proportions
  compared to the image; scaling the source-size layout keeps the chart
  identical to the PNG, only sharper.
- **Swap after the first frame.** Creating the viewer hidden and
  swapping when `rendered()` resolves avoids a blank box while the
  module or a font loads.
- **Everywhere, off by exception; no list of allowed sites.** Charts
  are pasted anywhere (wikis, forums, chat logs, internal tools), and a
  list to maintain would hide them on every site nobody added. The cost
  is the `<all_urls>` permission and a loader on every page, which goal
  4 keeps to one storage read until an image is near the viewport.
  Rejected: default sites plus opt-in through optional permissions (the
  earlier version of this design).
- **A toolbar button, not an address bar icon.** Chrome gives
  extensions no icon inside the address bar: Manifest V3 removed page
  actions, and the Translate icon is a browser feature. The action
  button, pinned next to the address bar, is the closest; it shows the
  tab's state with its icon, as Translate does, and a click toggles.
  Rejected: a popup with switches (two clicks for the common action).
- **Off per site, remembered.** A click turns the extension off for the
  tab's origin, for every tab and future visit, as users expect from
  "this site doesn't need it"; a per-tab, forgotten toggle would bring
  the charts back on every reload.
- **Cross-origin fetches without credentials.** Images the page loaded
  from another origin are re-fetched without cookies, so the extension
  never sends a user's cookies to a third party; images that need them
  (rare: signed URLs carry their token) stay images.

## 9. Constraints and Invariants

- The page is never left with a hidden image and no visible chart: every
  path that destroys a viewer shows its image first.
- No fetch of a URL that did not come from a candidate's `currentSrc` or
  (section 3.7) its enclosing link; fonts come only from the extension.
- Sources are untrusted: the viewer's limits apply unchanged (viewer
  DESIGN goal 5); byte caps: 32 MiB per PNG, the header walk stops at
  the first `IDAT` or after 1 MiB.
- No timers polling the DOM; observers and idle callbacks only.
- All extension code follows CLAUDE.md where it applies (MIT header,
  JSDoc on exported functions, 2-space indentation, ASCII).

## 10. Integration Boundaries

- **Depends on** the web viewer's public JavaScript API (viewer DESIGN
  section 5: `loadNymph`, `createViewer`, `setPng`, `setSource`,
  `rendered`, `toYaml`, `destroy`, the `theme` and `menu` options) and
  its build outputs; on the PNG layout of core DESIGN section 4 (`png`)
  only through the sniffer's check of the chunk keyword.
- **Changes** the viewer by adding `fit: 'scale'` (section 5.3); viewer
  DESIGN and `docs/viewer.md` gain it.
- **Couples to** GitHub's and Google Docs' markup and image hosting,
  which change without notice: everything site-specific stays in
  `content/sites/`, and the generic image path must keep working when an
  adapter's assumptions break.
