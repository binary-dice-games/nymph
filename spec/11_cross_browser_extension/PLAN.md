# nymph — Extension on Edge, Firefox and Safari: Implementation Plan

Design: [DESIGN.md](DESIGN.md). Builds on the Chrome extension
([spec/05_browser_extension](../05_browser_extension/DESIGN.md)), chat mode
([spec/10_gemini_extension](../10_gemini_extension/DESIGN.md)) and the
release pipeline ([spec/07_release_pipeline](../07_release_pipeline/DESIGN.md)).

Steps go from what could invalidate the design (step 1), to changes that
leave Chrome as it is (2-3), to Firefox (4-6, 8), Edge (7), Safari (9-10)
and the final check (11). Each step is a reviewable deliverable and ends
with the full existing native, Node and browser tests passing
unmodified. Extension tests live in `extension/tests/node/run.mjs` and
`extension/tests/browser/` (CTest `nymph_extension_*`). Steps with
user-visible behaviour add a `CHANGELOG.md` entry; steps that touch a real
browser record its name, version and the date in their **Result**.
Docs (`docs/extension.md`, `README.md`, `docs/releasing.md`) are updated in
the step that changes what they describe.

## Steps

### 1. Feasibility check in Firefox, Edge and Safari

**Goal:** the facts DESIGN.md marks *check* are answered with evidence
from real browsers, and DESIGN.md is updated before code depends on them.

**Deliverables:** a throwaway probe extension (not committed; the
current extension plus logging, loaded as a temporary add-on in Firefox,
unpacked in Edge, and through the Safari converter on a Mac) and the
answers below in this step's **Result**, folded into DESIGN.md sections
3.2, 5.1 and 9.

1. **Firefox 140 ESR and current release**
   1. Does the current package load with the Firefox manifest of DESIGN
      5.1 (event page, `browser_specific_settings`), and does `about:debugging`
      show warnings?
   2. Is host access granted at install, and what does the button do when
      the user revokes it?
   3. Does `loader.js` run at `document_idle`, and does `import()` of
      `main.js` and `chat.js` from the content script work?
   4. `canRunInline()` on a page without a policy and on a page with a
      GitHub-like policy (the existing `?csp=github` fixture): which
      answers? In frame mode, does the extension frame load under that
      policy, and does a chart draw (WebGL2, `MessageChannel` port into the
      frame)?
   5. Does `runtime.getBackgroundPage()` return the event page, so that
      `nymphToggleTab` can be called from the options page?
   6. `contextMenus` with `contexts: ['action']`, `storage.session`,
      `action.setIcon` with a tab id, `setBadgeText`, `disable(tabId)`.
   7. Does a cross-origin read through the event page work with
      `credentials: 'omit'` (the CORS lift of host permissions)?
2. **Edge (current stable)**: the Chromium package loads unpacked; one
   page with a nymph PNG and one with the GitHub-like policy behave as in
   Chrome; does Edge stable accept `--load-extension` under Playwright
   (`channel: 'msedge'`)?
3. **Safari (macOS 14+, current release), on a Mac with Xcode**
   1. Does `xcrun safari-web-extension-converter` accept the Firefox-style
      manifest, and what does it warn about (background form, `contextMenus`)?
   2. After "Allow unsigned extensions" and enabling the extension: do
      content scripts run, and on which sites (per-site permission flow)?
   3. Items 1.3, 1.4, 1.6 and 1.7 above.
   4. WebGL2 and WebAssembly in the viewer; how many live viewers before
      Safari drops a context or the tab reloads for memory.
   5. What `tabs.query` returns for `url` on a site not yet granted.
4. **Stores**: for each (Edge Add-ons, AMO) the current requirements for a
   listing with `<all_urls>`, a WebAssembly module, and the API for
   automated uploads (names and base URLs of the endpoints, auth
   scheme, whether source upload is available through the API).

**Tests:** none automated; each answer says how it was checked.

**Go / no-go:**
- If item 1.4 says Firefox runs neither inline nor in a frame on a
  strict-policy page, Firefox is still shipped (charts work on sites that
  allow them) and the limit is documented; the plan continues, and a
  separate remedy project is listed in DESIGN 9 risk 1.
- If item 3.2 shows content scripts cannot run on sites without a
  per-site prompt that the extension cannot improve, or WebGL2 /
  WebAssembly fail in Safari, steps 9-10 shrink to "documented as not
  supported, with the finding", and the rest proceeds.

**Result:** _not done_: needs real Firefox, Edge and Safari (and a Mac); nothing below was run in them.

### 2. The `ext` namespace layer

**Goal:** every extension module reaches the browser API through one
shared alias, with no change in behaviour in Chrome.

**Deliverables:**
- `extension/src/shared/api.js`: `ext`, `hasHostAccess()`,
  `canRequestAccess()`, documented (DESIGN 4).
- `settings.js`, `service-worker.js`, `options.js`, `fetcher.js`,
  `count.js`, `chat.js`, `replacer.js` use `ext`; `loader.js` begins with
  the one-line alias.
- Node tests: `ext` with a fake `browser`-only, `chrome`-only and both;
  `hasHostAccess()` with and without `permissions`.

**Tests:** the new Node tests; `grep -rn "chrome\." extension/src` finds
only `api.js` and `loader.js` (asserted by a Node test over the source);
all existing Node and browser tests pass unmodified.

**Result:** done (2026-10-20). `src/shared/api.js`; every module uses `ext`; `loader.js` has the alias. Node tests pass (26). The Chromium browser test could not run here (no Playwright): it is unverified.

### 3. Manifest template and per-browser outputs

**Goal:** the build produces the Chromium, Firefox and Safari folders and
the two zips of DESIGN 5.3 from one template, Chrome's output unchanged.

**Deliverables:**
- `manifest.json.in` with the substitution points of DESIGN 5.1;
  `extension/CMakeLists.txt` with `nymph_extension_package(<browser>)`,
  variables per browser in `extension/cmake/`, the Firefox id as a cache
  variable `NYMPH_FIREFOX_ID` (default `nymph@binarydicegames.com`).
- Outputs: `nymph/` and `nymph-extension-<v>.zip` (as before),
  `nymph-firefox/` and `nymph-extension-firefox-<v>.zip`, `nymph-safari/`.
- Node test: the manifest checks of DESIGN 7 item 1.
- `docs/building.md`: the new outputs.

**Tests:** the three manifests parse; the Chromium one equals the one the
previous commit produced (fixture); the Firefox one has `scripts`, no
`service_worker`, no `minimum_chrome_version`, the gecko block; the
Safari one has no `contextMenus`; the other files are identical across
folders; every module the content script imports is web-accessible.

**Result:** done (2026-10-20). `nymph_extension_package()` builds `nymph/`, `nymph-firefox/` (+ zips) and `nymph-safari/`; the manifest template has the substitution points. The Node manifest test reads the generated manifests (checks the Chromium keys, Firefox event page and gecko block, Safari permissions, shared keys, web-accessible modules). Deviation: the Chromium manifest is equal in content to the old one, not byte-identical (key order). `release-assemble.sh` includes the Firefox zip. `NYMPH_FIREFOX_ID` is `nymph@binarydicegames.com`.

### 4. The access state

**Goal:** when a browser withholds host access, the button and the
options page say so and can request it; Chrome never shows the state.

**Deliverables:**
- `service-worker.js`: `showTab()` handles `hasHostAccess() === false`
  (off icon, title "nymph needs access to sites. Click to allow.");
  the click handler calls `permissions.request({origins: ['<all_urls>']})`
  when access is missing, else toggles as now; it refreshes all tabs on
  `permissions.onAdded` / `onRemoved`.
- `options.html` / `options.js`: a banner with an "Allow access" button
  while access is missing; the welcome text by feature test (DESIGN 3.3
  item 3); a "Copy the instructions" button (the primer text from
  `src/gemini/primer.txt`, to the clipboard).
- `runnableOrigin()`: the added gallery origins.
- Node tests with a fake `permissions`: all combinations of
  access / on / off / unrunnable origin give the expected title and icon.

**Tests:** Node tests above; the browser test confirms Chrome's titles,
icons and options page are unchanged (existing assertions); a new
Chromium case revokes `<all_urls>` through the test hook and checks the
banner (skipped if the Chromium driver cannot revoke).

**Result:** done in code (2026-10-20): access state in the service worker (button title, request on click, `permissions` events), banner and "Allow access" in the options page, wording by feature test, `contextMenus` optional, gallery origins in `runnableOrigin()`; Node tests cover `hasHostAccess`/`canRequestAccess` and origins. The button-title combinations are not unit-tested (the worker is not importable in Node) and a Chromium case that revokes access was not written. CHANGELOG entry added. CHANGELOG: Added: "Firefox" is not logged until
step 6; this step logs nothing user-visible on Chrome.

### 5. A driver interface for the browser tests

**Goal:** the behaviour scenarios are written once and run on any
browser that has a driver; Chromium runs them exactly as before.

**Deliverables:**
- `extension/tests/browser/driver.mjs` (the interface: `openPage(url)`,
  `evaluate(page, fn)`, `screenshotBox`, `callBackground(name, args)`,
  `extensionPage(path)`, `close`), `driver-chromium.mjs` (Playwright, the
  code that is now in the head of `run.mjs`), `scenarios.mjs` (the
  scenarios of DESIGN 7 item 4, moved from `run.mjs` and rewritten against
  the interface), `fixtures.mjs` (the fixture server, shared).
- `run.mjs` keeps the Chromium-only checks and runs the shared scenarios
  through the Chromium driver.

**Tests:** `nymph_extension_browser` passes with the same number of
assertions covering the same cases; a Node test checks that
`scenarios.mjs` imports neither Playwright nor Selenium.

**Result:** _not done_.

### 6. Firefox running and tested

**Goal:** the Firefox package works in Firefox 140+ with the shared
scenarios passing in CI, on the sites step 1 found workable.

**Deliverables:**
- Fixes step 1 and the first run require (the table of DESIGN 3.2), in
  the smallest form each: if `import()` fails, the concatenation step of
  DESIGN 3.2 for the Firefox package; if frames are blocked, the remedy
  chosen in step 1.
- `driver-firefox.mjs` (selenium-webdriver, `installAddon`, background
  hooks through the options page) and `firefox/run.mjs`; CTest
  `nymph_extension_firefox` (skip 77 without Firefox, geckodriver or the
  npm package, with `TIMEOUT 600`).
- CI: a job in `ci.yml` that installs Firefox, geckodriver and
  selenium-webdriver on `ubuntu-24.04` and runs the test (`NYMPH_WEB`
  build of the Emscripten job reused).
- `docs/extension.md`: Firefox install (from AMO once published,
  temporary load before that), the access prompt, the pinning words,
  Firefox's limits.
- CHANGELOG: Added "Firefox support for the browser extension".

**Tests:** the shared scenarios pass in Firefox; the access-state case
passes with `permissions` revoked through the driver; the Firefox package
passes `web-ext lint` with no errors (run in CI where `npx` exists).

**Result:** _not done_.

### 7. Edge

**Goal:** Edge users can install the extension from Edge Add-ons, and the
Chromium package is checked in Edge.

**Deliverables:**
- `packaging/edge/publish.sh` (Edge Add-ons publish API: upload the
  Chromium zip, poll the operation, publish; endpoints settable; refuses
  nothing but a real error; treats "version exists" as success), and
  `tests/edge_publish_test.mjs` against a stub server, registered in
  `tests/CMakeLists.txt` like `chrome_publish_test.mjs`.
- `release.yml`: an `edge` job after the release is published (skips
  with a warning when `EDGE_*` variables or secrets are unset).
- `driver-chromium.mjs` takes `NYMPH_EXT_BROWSER_CHANNEL=msedge`; a CTest
  `nymph_extension_edge` (skip 77 when Edge is missing or refuses the
  switch).
- `docs/extension.md`: Edge install; `docs/releasing.md`: the listing and
  the API keys the owner sets up.
- CHANGELOG: Added "Microsoft Edge: the extension installs from Edge
  Add-ons" (when the first release publishes there).

**Tests:** the stub test (success, version already there, upload
rejected, credentials never printed); the Edge run when Edge is present;
by hand, one page with a chart in Edge (recorded in Result).

**Result:** _not done_.

### 8. Firefox on AMO

**Goal:** a release uploads the Firefox package and the source to
addons.mozilla.org for review.

**Deliverables:**
- `packaging/firefox/publish.sh` (or `web-ext sign --channel listed`
  pinned in CI, whichever step 1 item 4 finds supported for listed
  uploads with source), with the source tarball attached and review notes
  (build instructions pointing to `docs/building.md`); stub-server test
  as in step 7 when the script talks to the API directly.
- A check that `nymph-<v>-src.tar.gz` builds `NYMPH_WEB` offline with
  Emscripten and yields a byte-identical `.wasm` (or documents the
  difference reviewers need); fix in the tarball recipe if it does not.
- `release.yml`: a `firefox` job (skips with a warning when `AMO_*` are
  unset).
- `docs/releasing.md`: the AMO listing, the add-on id, the keys;
  `docs/extension.md`: the store link.

**Tests:** the stub test; the source-tarball build check (a CTest or CI
step on Linux with Emscripten); `web-ext lint` on the release artifact.

**Result:** _not done_ (the first real upload waits for the owner's AMO
account).

### 9. Safari package and hand verification

**Goal:** an unsigned `nymph.app` with the extension builds on CI and has
been used by hand in Safari on a Mac; DESIGN 3.1 is updated with what
was seen.

**Deliverables:**
- `packaging/safari/convert.sh` (runs the converter with the bundle
  identifier from a variable, patches nothing in the shared files) and
  the `nymph-extension-safari` CMake target on macOS (DESIGN 6).
- Safari adaptations step 1 requires: the permissions state wording, the
  "Copy the instructions" route in place of the context menu, a lower
  pool cap if step 1 item 3.4 measured one (a constant chosen by feature
  test or measured limit).
- CI: a `macos-15` job that builds the web viewer (or reuses its
  artifact), runs the converter and `xcodebuild` unsigned; uploads the
  `.app` as an artifact.
- `docs/extension.md`: a Safari section (build, "Allow unsigned
  extensions", enabling, per-site access, limits, no sync).
- A hand checklist in this Result: macOS and Safari versions; a chart on a
  local page, on a strict-policy page, the button, the options page, the
  chat code block (a saved Gemini fixture page), memory with 12 and 32
  charts.

**Tests:** CI builds the app; the Node manifest test covers the Safari
folder; the checklist is the evidence for the rest.

**Result:** _not done_.

### 10. Safari distribution (gated on the owner)

**Goal:** a signed Safari build is available to users, by the route the
owner chooses.

**Deliverables (needs the owner's Apple Developer account):**
- Decision recorded here: App Store (review, a macOS app that only
  carries the extension) or developer-signed and notarised download.
- Signing and notarisation in CI with the account's secrets;
  `xcodebuild archive` and export; for the App Store the upload with the
  App Store Connect API key; Release DESIGN section 4.8's signing notes
  reused.
- `release.yml`: a `safari` job, skipping with a warning when the secrets
  are unset.
- `docs/extension.md`, `docs/releasing.md`, README table updated.
- CHANGELOG: Added "Safari" when the first signed build ships.

**Tests:** a signed build installs on a second Mac without the unsigned
setting (by hand, recorded in Result).

**Result:** _not done_; may stay open indefinitely without blocking the
project (see Completion Criteria).

### 11. Documentation and the final check

**Goal:** the docs describe all browsers as shipped, and the manual
matrix has been run.

**Deliverables:**
- `docs/extension.md` titled for all browsers; README line, the
  `extension/` row of the structure table and the Further Documentation
  table; extension DESIGN section 1 points to this project instead of
  "out of scope"; `spec/INDEX.md` status and topics updated.
- A manual matrix in this Result: for each of Chrome, Edge, Firefox
  (ESR and release) and Safari, with version and date: GitHub README with
  a nymph PNG, a page with a strict policy, button toggle, options page,
  Gemini chat block.

**Tests:** all CTest tests pass on Linux; the macOS CI job passes; the
matrix is filled in or states what was not verified.

**Result:** _not done_.

## Completion Criteria

Done when:

1. `nymph_extension_node`, `nymph_extension_browser` (Chromium) and
   `nymph_extension_firefox` pass; the Chrome package, its tests and its
   store job are unchanged in behaviour.
2. The Firefox package passes the shared scenarios, `web-ext lint`, and a
   release uploads it (and the source) to AMO for review when the owner's
   credentials exist.
3. The Chromium package is published to Edge Add-ons by the release when
   credentials exist, and has been checked by hand in Edge.
4. Safari: either (a) the app builds on CI and was verified by hand, with
   the findings in DESIGN 3.1, or (b) step 1 showed it cannot work and the
   docs say why. Signed distribution (step 10) is a follow-up gated on the
   owner's Apple account and does not block "Done".
5. Docs and `spec/INDEX.md` match what shipped.
