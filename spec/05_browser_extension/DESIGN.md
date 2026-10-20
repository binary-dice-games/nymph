# nymph — Browser Extension: Design

**Status: implemented (plan steps 1-8 and 10); verified in Chromium 141
through Playwright on local pages, including one with GitHub's Content
Security Policy. Not yet verified on github.com itself or in Google Docs
(plan steps 8 and 9 list the manual checks).** [PLAN.md](PLAN.md) is the
ordered implementation plan and records each step's result. This document
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
2. **Google Docs** (`docs.google.com/document/...`): see section 3.7;
   only partly possible, and not yet checked.

In scope: the extension (`extension/`): manifest, content script,
service worker, the toolbar button (section 3.8), an options page,
packaging into an unpacked folder and a
zip from the `NYMPH_WEB` build; one viewer change, `fit: 'scale'`
(section 5.3); tests with fixture pages in Chromium.

Out of scope: Firefox and Safari (a later project,
[spec/11_cross_browser_extension](../11_cross_browser_extension/DESIGN.md)); images inside shadow
roots, iframes (the content script runs in the main frame only),
`<canvas>` and CSS background images (so Google Docs' editor, section
3.7);
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
   image is downloaded twice in full until a nymph PNG is found (the
   page gets one storage read and about 25 KB of script); deciding that
   an image is not a nymph PNG reads at most its first chunks, usually
   from the HTTP cache.
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
is connected, loaded, its `currentSrc` is
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
  `width` / `height` attributes (a px value the image is not shown at is
  ignored: a stylesheet overrides it, as claude.ai's `width="0"` with
  `width: auto`) and computed `width`, `max-width`,
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

1. In one task: the host goes in after the image with its final sizing,
   holding the viewer's box and, on top of it, a **cover**: a copy of
   the image (same URL, already decoded); the image gets the attribute
   `data-nymph-ext="hidden"` (an injected stylesheet hides
   `img[data-nymph-ext="hidden"]` with `display: none !important`). The
   page looks the same.
2. The viewer starts under the cover. When `rendered()` resolves, the
   cover is removed and the chart shows. If the viewer fails, the host
   is removed and the image shown again.
3. **Back to the image**: a badge in the host's top-right corner, shown
   while the pointer is over the chart, has two buttons: "Image" /
   "Chart" switches this chart between the chart and its image, and
   "Open" opens the image's link (or the image) in a new tab. Turning
   the extension or the site off restores every image (section 3.5).

The viewer is never hidden while it starts (an earlier version kept the
host invisible until the first frame): Chrome does not run animation
frames in an extension frame it considers hidden, so in frame mode
(section 3.9) the chart would never draw.

Hiding by attribute and stylesheet, not by an inline style or by removing
the image, leaves the node to the page's framework: React, Turbo and
similar libraries keep reconciling the element they own, and if they
replace it the new element is simply a new candidate (and a cache hit).

### 3.4 Living with the host page

- **Links.** Images are often inside a link (GitHub wraps every markdown
  image in a link to itself). The host stops `click`, `auxclick` and
  `dragstart` from bubbling out of the chart and prevents their default,
  so panning does not navigate; the badge's "Open" keeps the link's use.
  (In frame mode, input inside the frame never reaches the page.)
- **Wheel and touch**: the viewer's own rules (viewer DESIGN section 5,
  input mapping) already scroll the page except over a plot or diagram.
- **Styles.** The viewer lives in a shadow root inside the host, so page
  CSS (`.markdown-body img`, `canvas { ... }`) cannot reach it and the
  extension's CSS cannot leak out. The root is open: it holds nothing a
  page should not see, and tests reach the badge through it. The only
  page-level CSS is the rule of section 3.3 (and, inline, the viewer's
  own right-click menu style).
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
- **Viewer pool.** At most 32 live viewers per page inline, 12 in frame
  mode (each frame has its own module and WebGL context; browsers allow
  about 16 contexts per process). When a new one is needed, the viewer
  farthest from the viewport that is outside the `IntersectionObserver`
  margin is **parked**: its source with the user's view (`toYaml()`) is
  kept, it is destroyed and its image is shown again. When the image
  comes near again it gets a viewer from the kept YAML, so the user's
  zoom and hidden series come back. A host the page removes (a
  re-render of the image's parent) is put back the same way, at most
  three times per image.
- Turning on and off applies live (section 3.8): turning on starts the
  scan; turning off destroys every viewer, removes every host and the
  stylesheet and restores every image, without reloading the page.

### 3.6 GitHub

Generic `<img>` handling covers GitHub's markdown; nothing in the code is
specific to GitHub:

- GitHub's Content Security Policy has no `'wasm-unsafe-eval'`, so the
  charts run in frame mode (section 3.9) there;
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

Not yet checked (it needs a signed-in browser): that GitHub serves
uploaded and proxied PNGs byte for byte (the `iTXt` chunk survives).
GitHub documents Camo as a proxy that does not transform images.

### 3.7 Google Docs

Google Docs draws document pages on `<canvas>` elements (since 2021)
rather than as HTML, so there may be no `<img>` to replace, and the
service may re-encode inserted images, which drops the chunk (README:
"services that re-encode images drop it"). The feasibility step could
not answer these questions without a Google account; they stay open:

1. Does an image inserted into a Doc keep its `nymph` chunk at the URL
   the editor loads it from?
2. Does the editor's DOM expose each image's URL and box (an `<img>`, an
   SVG `<image>` in Docs' accessibility / annotation layer, or anything
   else readable from an isolated content script)?
3. Do the read-only views (`/preview`, published `/pub`) render images as
   `<img>`?

What is built: generic `<img>` handling, which covers any Docs view that
shows images as `<img>` with their original bytes (question 3, if the
bytes are kept). The editor is not supported. Depending on the answers,
a later change could add:

| Outcome | What the extension does in Docs |
|---|---|
| Bytes kept, images in the DOM | An adapter maps each image to a URL and a box; the host is an absolutely positioned overlay over the canvas, re-positioned on scroll and resize (the canvas is not ours to replace). |
| Bytes kept, images only in the canvas | Nothing more: read-only views only, as built. |
| Bytes re-encoded | **Linked source**: an image whose link (Docs lets users link an image) points to a nymph PNG or a `.yaml` / `.yml` nymph source is replaced with that source, wherever the image's box can be found (row 1 or 2). |

The linked-source rule is general (any site): it applies only when the
image itself is not a nymph PNG, its enclosing `<a href>` points to a URL
ending in `.png`, `.yaml` or `.yml` on a host the extension may fetch,
and that URL's content is a nymph PNG or a source the viewer accepts.
It is not built.

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

### 3.9 Inline and frame mode

The feasibility step found that a page's Content Security Policy governs
WebAssembly in content scripts: on a page without `'wasm-unsafe-eval'`
(GitHub, and most large sites), compiling the module in the content
script fails. So the viewer runs in one of two ways, chosen once per
page by compiling an empty module (`canRunInline()`):

| | Inline | Frame |
|---|---|---|
| Where | In the content script, in the host's shadow root | In an extension page (`src/frame/frame.html`) in an `<iframe>` in the host's shadow root, under the extension's own policy |
| Cost | One module and one WebGL context for the page | One of each per chart |
| Pool cap | 32 | 12 |
| Bytes | Passed to `createViewer()` | Sent over a `MessageChannel` port handed to the frame with its first message |
| Parking | `toYaml()` | The frame sends `toYaml()` after the first frame and every `view` event; the last one is kept |
| Start | At once | When the frame is on screen: Chrome pauses animation frames in an off-screen extension frame, so a chart below the viewport swaps in when the user scrolls to it (the image shows until then) |

The frame is sized by the host (`aspect-ratio`), never by messages. The
right-click menu and About panel open inside the frame in frame mode.

## 4. Key Abstractions

All in `extension/src/`, plain ES modules, no framework, no build step
beyond copying.

| Module | Role | Invariants |
|---|---|---|
| `content/loader.js` | The declared content script, on every page (classic script, as MV3 requires): reads the settings and, when this site is on, `import()`s `content/main.js`; follows setting changes (`storage.onChanged`) with `start()` / `stop()`. | Does nothing else: on a site turned off it costs one storage read. |
| `content/main.js` | The observers and states of section 3.5: each image's entry moves watching, reading, live, parked or plain; the pool. | Never runs work inside an observer callback; never touches nodes it did not create except the one attribute of section 3.3. |
| `content/sniff.js` | `sniffStream(stream) -> {nymph, bytes?, reason?}`: checks the PNG signature, walks chunk headers up to the first `IDAT`, and answers whether an `iTXt` chunk with keyword `nymph` is among them; on yes, reads the rest (up to 32 MiB) and returns all bytes; on no, cancels the stream. `PngSniffer` is the incremental walker. | Pure over bytes (tested in Node); reads at most the chunk headers before `IDAT` and the keyword of each `iTXt`; never trusts a chunk length beyond the caps. |
| `content/fetcher.js` | `readImage(url)`: `data:`, `blob:` and same-origin URLs with `fetch(url, {cache: 'force-cache', credentials: 'same-origin'})` here (a failure there, as a redirect to a host without CORS, falls back to the worker); cross-origin URLs through the service worker. At most 4 reads at a time. | Reads only the `currentSrc` of a candidate; never a URL taken from inside a chart's YAML. Never rejects. |
| `content/replacer.js` | `Replacement`: the host (section 3.2), shadow root, cover, viewer inline or in a frame (section 3.9), badge, link handling; `unmount()` restores the image. | The image is hidden only while the host and its cover stand in its place; every failure path shows the image again. |
| `content/sizing.js`, `pool.js`, `cache.js` | The host's CSS from the image's (`hostStyle()`); which viewer to park (`pickToPark()`); the per-URL LRU cache. | Pure (tested in Node). `pickToPark()` never picks a viewer under the pointer or within the observer margin. |
| `frame/frame.html`, `frame.js` | One chart in an extension frame (section 3.9). | Takes the first `MessagePort` it is sent and listens only to it. |
| `background/service-worker.js` | Cross-origin reads for content scripts (host permissions lift CORS there), with `credentials: 'omit'`; sniffs first and returns only nymph PNG bytes (base64: messages are JSON) or "not nymph". Owns the toolbar button (section 3.8): toggles, per-tab icon, title and badge from the counts content scripts report (kept in `storage.session`, as the worker sleeps). | Answers only messages from its own content scripts; enforces the sniffer's caps. |
| `shared/settings.js` | The settings of section 5.2: read, `isOn(origin)`, `setSite()`, `runnableOrigin(url)`. | The one place that knows the setting keys. |
| `options/` | The options page (from the button's context menu): theme (`source` or `auto`), the list of sites turned off (each can be turned on), and the pinning hint shown on install. | Writes only `chrome.storage.sync`; content scripts follow its changes live. |

The bundled viewer (`viewer/`: `nymph-viewer.js`, `nymph-viewer-core.js`,
`.wasm`, `fonts/`) is the output of the `NYMPH_WEB` build, loaded on the
first nymph image of a page (inline) or by each frame, finding its
`.wasm` and fonts next to itself in the extension. One viewer change was
needed besides `fit: 'scale'`: `customElements` is `null` (not
undefined) in content scripts, and the module now checks for that
before defining `<nymph-viewer>`.

## 5. Contracts

### 5.1 Manifest and permissions

- Manifest V3; `minimum_chrome_version` 116 (`'wasm-unsafe-eval'` in
  extension pages and module service workers; verified only with
  Chromium 141).
- `permissions`: `storage`.
- `host_permissions`: `<all_urls>`, granted on install: the content
  script runs everywhere, and the service worker can read cross-origin
  images on any site.
- `content_scripts`: `content/loader.js` on `<all_urls>`,
  `run_at: document_idle`, `all_frames: false` (main frame only).
- `action`: the toolbar button, no `default_popup` (a click toggles,
  section 3.8), icons for on and off.
- `options_ui`: `options/options.html`.
- `web_accessible_resources`: `src/content/*`, `src/frame/*`, `viewer/*`
  and `viewer/fonts/*` for `<all_urls>` (the content script imports its
  modules and the viewer from the extension, and frames load from it).
  A page can therefore tell that the extension is installed;
  `use_dynamic_url` would hide it but is not used yet (untested with
  module imports and frames).
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

The marks the extension leaves in the page: the attribute
`data-nymph-ext="hidden"` on hidden images, the host element
`<nymph-ext-chart>` (an unregistered custom element name, so the page's
`<nymph-viewer>` definition, if any, is never involved; its classes are
`nymph-ext-starting` and `nymph-ext-showing-image`), and one `<style>`
element with `id="nymph-ext-style"`. Turning the extension off removes
them all. Inline, the viewer's right-click menu adds its own style
(`nymph-viewer-style`) and menu elements to the page when used.

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
   below 1, the framebuffer cap, pointer mapping at scale 2) in
   `tests/test_live_view.cpp`.
2. **Node** (`extension/tests/node/run.mjs`, CTest `nymph_extension_node`):
   the sniffer over every committed example PNG, the same PNGs without
   the chunk (and their image data never read), a chunk after `IDAT`,
   another keyword, truncated and malformed streams, the header and size
   caps, non-PNG bytes; the URL cache; the host sizing rules; the
   pool's parking order; fetch routes; settings.
3. **Browser** (`extension/tests/browser/run.mjs`, CTest
   `nymph_extension_browser`; Playwright with the preinstalled Chromium
   and `--load-extension`): fixture pages served on two origins
   (`127.0.0.1` and `localhost`, one port from the OS), several of them
   in both modes (`?csp=github` serves a GitHub-like policy, which
   forces frame mode). Each asserts what the user would see:
   - only nymph PNGs become charts, in the image's box (to the pixel)
     and looking like it (the viewer DESIGN browser tolerance), for
     images at natural size, with a `width` attribute and scaled by CSS;
     a plain PNG and an icon-sized image stay images;
   - dragging and clicking a chart inside a link does not follow it;
   - `srcset`, `<picture>` and `loading="lazy"` images; a feed that
     appends images on scroll; a framework that replaces image nodes and
     strips the extension's attribute; an image whose `src` changes or
     that is removed;
   - 48 charts: at most 32 (inline) or 12 (frame) viewers live, the
     first is parked and comes back with its zoom;
   - corrupt and non-nymph sources stay images; a cross-origin image
     without CORS is read by the worker without cookies; a page without
     nymph PNGs loads no viewer;
   - the button turns a site off in every tab of it (page restored, badge
     empty, title), stays off after a reload, turns on again (badge
     count), leaves another site on, and is disabled on pages the
     extension cannot see; the badge switches a chart to its image and
     back and opens it; `theme: auto` follows a dark colour scheme.
4. **Manual** (PLAN.md steps 8 and 9): github.com and Google Docs.

## 8. Design Decisions

- **Inline where allowed, a frame per chart where not.** Inline, the
  viewer runs in the content script's isolated world: one WebAssembly
  instance and one shared WebGL context for all charts on the page
  (viewer DESIGN section 8), and no page script can call into it. But
  the page's Content Security Policy governs WebAssembly there, and
  GitHub's forbids it, so pages like GitHub get one extension frame per
  chart, which runs under the extension's own policy. Rejected:
  frames everywhere (a module instance and a WebGL context per chart on
  every page); one hidden extension frame drawing every chart and
  sending frames to the page (a remote version of the whole viewer API,
  input and menus included); injecting into the page's main world (the
  page could reach the viewer, and its CSP still applies).
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
- **Cover, then reveal.** The host takes the image's place at once with
  a copy of the image on top, and the copy goes when `rendered()`
  resolves, so there is never a blank box while the module or a font
  loads. Rejected: building the viewer in a hidden host and swapping
  after the first frame (Chrome never runs a hidden extension frame).
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
- No fetch of a URL that did not come from a candidate's `currentSrc`;
  fonts come only from the extension.
- Sources are untrusted: the viewer's limits apply unchanged (viewer
  DESIGN goal 5); byte caps: 32 MiB per PNG, the header walk stops at
  the first `IDAT` or after 1 MiB.
- No timers polling the DOM; observers and idle callbacks only.
- All extension code follows CLAUDE.md where it applies (MIT header,
  JSDoc on exported functions, 2-space indentation).

## 10. Integration Boundaries

- **Depends on** the web viewer's public JavaScript API (viewer DESIGN
  section 5: `loadNymph`, `createViewer`, `setPng`, `setSource`,
  `rendered`, `toYaml`, `destroy`, the `theme` and `menu` options) and
  its build outputs; on the PNG layout of core DESIGN section 4 (`png`)
  only through the sniffer's check of the chunk keyword.
- **Changes** the viewer by adding `fit: 'scale'` (section 5.3); viewer
  DESIGN and `docs/viewer.md` gain it.
- **Couples to** pages' Content Security Policies (section 3.9) and,
  for GitHub and Google Docs, to how they host images, which can change
  without notice. Nothing in the code is site-specific; a site adapter
  (section 3.7) would be the first.
