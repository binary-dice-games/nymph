# nymph — Extension on Edge, Firefox and Safari: Design

**Status: partly built (plan steps 2-4: the `ext` layer, per-browser
manifests and packages, the site-access state); not yet run in Firefox,
Edge or Safari.** [PLAN.md](PLAN.md) is the
ordered implementation plan; its step 1 checks the facts marked
*unverified* in real browsers before code depends on them. This document
describes only what this project adds or changes. It builds on
[spec/05_browser_extension/DESIGN.md](../05_browser_extension/DESIGN.md)
(*extension DESIGN*: the Chrome extension), on
[spec/10_gemini_extension/DESIGN.md](../10_gemini_extension/DESIGN.md)
(*chat DESIGN*: chat mode, which the other browsers inherit) and on the
release pipeline of
[spec/07_release_pipeline/DESIGN.md](../07_release_pipeline/DESIGN.md)
section 4.7 (*release DESIGN*: the Chrome Web Store job).

## 1. Purpose / Scope

The extension works in Chrome only. This project makes the same source
tree run in **Microsoft Edge**, **Mozilla Firefox** and, if the checks of
PLAN step 1 allow it, **Apple Safari**, each distributed through its own
store, with the same behaviour as extension DESIGN describes: nymph PNGs
(and, in chat mode, nymph sources in code blocks) become live viewers,
the toolbar button toggles the site, the options page holds the settings.

Where each browser stands (what it takes, from the documentation of each
vendor; *unverified* until PLAN step 1):

| Browser | Package | Distribution | Effort |
|---|---|---|---|
| Edge | The Chrome package, unchanged (Chromium, Manifest V3, same `chrome.*` APIs) | Microsoft Edge Add-ons (also installs Chrome Web Store items) | A listing, a publish script, tests |
| Firefox | Manifest V3 with an event page instead of a service worker, a `browser_specific_settings` block, host access that the user may withhold | addons.mozilla.org (AMO); signing is mandatory for release builds | Manifest variant, a host-access state, a test driver, store job |
| Safari | The same files wrapped by Xcode (`xcrun safari-web-extension-converter`) in a macOS app that carries the extension | App Store, or a developer-signed app | A Mac, Xcode, an Apple Developer account; fewer APIs; hand testing |

In scope: a browser abstraction layer in the extension code; a manifest
per browser generated from the one template; build outputs and zips per
browser; the host-access state; test drivers for Firefox (automated) and
Edge (as far as it can be automated); publish jobs for Edge Add-ons and
AMO; the Safari converter script and an unsigned CI build; docs.

Out of scope:

- Mobile: Firefox for Android and Safari on iOS / iPadOS (the converter
  can emit an iOS target and Firefox for Android takes the same package,
  but touch behaviour, memory limits and the stores' review of mobile
  extensions are a separate project; the manifest keeps room for them,
  decision 7).
- Opera, Brave, Vivaldi and other Chromium browsers: they install the
  Chromium package as is (from the Chrome Web Store or unpacked); nothing is
  tested or documented for them beyond that sentence.
- Features that differ per browser: the three browsers get one feature
  set, minus the exceptions of section 3.3.
- Notarised distribution of Safari outside the App Store, and iOS.

## 2. Design Goals

1. **One source tree.** The JavaScript, CSS, HTML, icons and the viewer are
   shared byte for byte by every browser's package. Per-browser code is
   limited to the manifest and one module (`shared/api.js`, section 4).
   Reason: a fork of the content script would have to follow every
   extension and chat-mode change.
2. **Chromium behaviour does not change.** Chrome's package keeps its
   manifest, its name and its tests; the Chrome Web Store job is untouched.
3. **Feature detection, not browser sniffing.** Code asks whether an API
   exists (`ext.contextMenus`, `ext.permissions`), never what the browser
   is. A browser that adds an API gains the feature with no change.
4. **The same promise as before: never break the page.** Each browser's
   quirks (a frame its page's policy blocks, an API that is missing) end in
   the original image staying in place, as extension DESIGN section 9
   requires.
5. **The user understands what is missing.** Where a browser withholds
   host access until the user grants it (Firefox, Safari), the button and
   the options page say so and offer the grant, instead of an extension
   that silently does nothing.
6. **A released package is the one that was tested.** Each package is
   built by the same CMake target, tested in its browser by CTest where
   the browser can be driven (Chromium, Firefox), and uploaded to its
   store by CI from the release's artifact.
7. **Private as before.** No new permission beyond what Chrome has; no
   network traffic added; the Firefox data-collection declaration says
   "none".

## 3. Behaviour

### 3.1 Support matrix

Minimum versions are set by the APIs the code uses (`storage.session`,
`action`, MV3 modules, `'wasm-unsafe-eval'`, WebGL2):

| | Chrome | Edge | Firefox | Safari |
|---|---|---|---|---|
| Minimum | 116 (as now) | 116 (Chromium) | 140 (ESR; `data_collection_permissions` needs 140) | 17 (macOS 14), *unverified* |
| Background | module service worker | module service worker | module event page (`background.scripts`) | non-persistent background, form *unverified* |
| Content scripts, viewer inline | yes | yes | *unverified* (section 3.2) | *unverified* |
| Frame mode | yes | yes | *unverified* (section 3.2) | *unverified* |
| Toolbar button, badge | yes | yes | yes | yes, with a Safari-specific permission state |
| Button context menu ("teach") | yes | yes | yes | no `contextMenus` in the toolbar button: alternative in section 3.3 |
| Settings (`storage.sync`) | synced | synced | synced with Firefox Sync | stored per Mac, not synced |
| Chat mode (spec/10) | yes | yes | yes | yes, once content scripts run |
| Store | Chrome Web Store | Edge Add-ons | AMO | App Store |

### 3.2 What differs, and how the code absorbs it

Each row is a difference found in the vendors' documentation, with the
plan for it. Rows marked *check* are PLAN step 1 questions: the design
holds either way, but the answer chooses the branch.

| Difference | Handling |
|---|---|
| Firefox has no extension service workers; its MV3 background is an **event page** (`background.scripts`, `"type": "module"`). | The Firefox manifest declares `scripts`. `service-worker.js` already keeps all state in `storage.session` and registers its listeners at top level, as an event page needs; it is loaded unchanged. Its test hooks (`globalThis.nymphToggleTab`) are reached from an extension page through `runtime.getBackgroundPage()` (section 7). *check*: `getBackgroundPage()` on an MV3 event page. |
| `chrome.*` and `browser.*`. All four browsers expose `chrome.*` in MV3, with promises; Firefox and Safari also expose `browser.*`. | `shared/api.js` exports `ext = globalThis.browser ?? globalThis.chrome`; modules use `ext` instead of `chrome`. The classic `loader.js` repeats the one-line alias (it cannot import). *check*: that every call the code uses returns a promise on `chrome.*` in Firefox and Safari; otherwise `ext` resolves to `browser` first, which is what the line above does. |
| **Host access.** Chrome grants `host_permissions` on install. Firefox treats them as user-controlled permissions (prompted at install since Firefox 127, revocable in `about:addons`); Safari asks the user per site or for "all websites", and tab URLs are hidden on sites not granted. | A new **access state**: the service worker computes `ext.permissions.contains({origins: ['<all_urls>']})`. When false, the button shows the off icon with the title "nymph needs access to sites. Click to allow." and a click calls `permissions.request()` (a click is the user gesture it needs; Safari shows its own prompt). The options page shows the same banner with a button. When the tab's URL is unreadable (Safari without access), the title says so instead of claiming the page is off limits. |
| **Dynamic `import()` of extension modules from a content script** (`loader.js` imports `main.js`, `chat.js`). Chrome allows it for web-accessible resources. | *check* in Firefox and Safari. Fallback if one refuses: a build step concatenates `src/content/*` into one classic file for that browser (CMake `file(READ)` plus an import-stripping step; no bundler dependency). The module sources stay the single source of truth. |
| **WebAssembly in a content script.** Chrome applies the *page's* policy (extension DESIGN section 3.9). Firefox and Safari may apply the extension's or the page's. | Nothing to change: `canRunInline()` decides at run time by compiling an empty module, in every browser. If a browser runs inline everywhere, frame mode is simply never used there. |
| **Extension frames in pages with a strict `frame-src`** (frame mode, GitHub). Chrome lets an extension's web-accessible page load whatever the page's policy says. Firefox and Safari may enforce the page's `frame-src`. | *check*, and the highest-risk row (section 9). If a browser blocks the frame **and** refuses inline WebAssembly on that page, charts cannot run there and the images stay (goal 4). Possible remedies, in order: `MessageChannel`-less frame via `blob:` / `moz-extension:` allow-list; a Firefox-only content script `world`; accept the limit and document the pages. |
| **Animation frames in an off-screen extension frame** (frame mode starts a chart "when the frame is on screen", extension DESIGN section 3.9). The cause is Chrome's throttling. | Kept as is for all browsers; in a browser that does not throttle, the start rule only delays a chart until it is on screen. |
| `minimum_chrome_version`, `use_dynamic_url`, `background.service_worker` are Chromium-only. | Only in the Chromium manifest. Firefox warns about unknown keys; the manifest generator emits exactly the keys each browser knows (section 5.1). |
| Firefox requires an add-on **id** and, for new listings, a `data_collection_permissions` declaration. | In the Firefox block of the manifest (section 5.1). |
| Messages: Chrome clones JSON; Firefox and Safari clone structured data. | No change. Bytes stay base64 in worker replies (works in all). |
| Toolbar icon and badge APIs (`action.setIcon`, `setBadgeText`, `setTitle`, `enable`, `disable`) | Used by all four; Safari draws the badge in its own style. *check* that `action.disable(tabId)` greys the button in Safari. |
| `runnableOrigin()` excludes `chromewebstore.google.com`. | Gains each browser's own gallery origins (`addons.mozilla.org`, `microsoftedge.microsoft.com/addons`) and `about:` / `moz-extension:` URLs are already excluded by the `http(s)` / `file` rule. Firefox forbids extensions on `addons.mozilla.org` by default (`extensions.webextensions.restrictedDomains`). |
| `file:` pages. | Chrome needs "Allow access to file URLs"; Firefox runs on `file:` when host access covers it; Safari does not allow it. Not guaranteed on every browser; the docs say so. |

### 3.3 Exceptions to "one feature set"

1. **Safari has no button context menu.** The "Give this chat the nymph
   instructions" command (chat DESIGN section 4.5) is also offered as a
   button on the options page ("Copy the instructions"), which copies the
   primer to the clipboard and works in every browser. In Safari it is the
   only route; elsewhere it is the second. The options page is reached
   from the extension's entry in Safari's settings.
2. **Safari settings are not synced** (`storage.sync` behaves as local
   storage there); the options page says nothing about it, the docs do.
3. **Firefox's first-run tab** pins the button with different words
   ("Extensions" menu, "Pin to toolbar"); the welcome page picks the text
   by feature test (`ext.runtime.getBrowserInfo` exists only in Firefox).
   Browser identity is used for wording only, never to branch behaviour.

### 3.4 Edge

Edge loads the Chromium package unchanged. Nothing in the code or
manifest is Edge-specific. What exists is a store listing, a publish job
and a test run in Edge when the tests can load an extension into it
(section 7). The `-edge` in artifact names is not needed: the Chromium
zip is uploaded to both stores.

## 4. Key Abstractions

All under `extension/`. New or changed modules:

| Module | Role | Invariants |
|---|---|---|
| `src/shared/api.js` (new) | `export const ext = globalThis.browser ?? globalThis.chrome;` plus `hasHostAccess()` (`ext.permissions.contains`, `true` when the API is absent) and `canRequestAccess()`. | The one place that chooses the namespace. Pure over its global (tested in Node with a fake). |
| `src/shared/settings.js`, `background/service-worker.js`, `options/options.js`, `content/*.js` | Use `ext` instead of `chrome`. The service worker gains the access state of section 3.2. | No behaviour change on Chromium; the Node and browser tests pass unmodified. |
| `content/loader.js` | Classic script; `const ext = globalThis.browser ?? globalThis.chrome;` on its first line. | Still costs one storage read on a site that is off. |
| `extension/manifest.json.in` | Gains substitution points for the keys that differ (section 5.1). | The Chromium output is byte-identical to today's manifest (a Node test compares). |
| `extension/cmake/` (new) | The per-browser variables and the Safari conversion step. | Reads no network, runs no package manager. |
| `extension/tests/browser/` | A small **driver** interface (open a page, evaluate in the page, call a background hook), a Chromium driver (Playwright, as now) and a Firefox driver; the scenarios are written once against the interface (section 7). | A scenario never imports Playwright or Selenium. |
| `packaging/edge/`, `packaging/firefox/`, `packaging/safari/` (new) | `publish.sh` for Edge Add-ons and AMO (mirroring `packaging/chrome/publish.sh`), the Safari converter script. | Never print a credential; endpoints settable for stub tests. |

## 5. Contracts

### 5.1 One manifest template, three outputs

`manifest.json.in` keeps today's content with these keys turned into
substitution points, set per browser by CMake (`configure_file ... @ONLY`,
no JSON tooling needed, so the project's CMake 3.14 floor holds):

| Key | Chromium (Chrome, Edge) | Firefox | Safari |
|---|---|---|---|
| `minimum_chrome_version` | `"116"` | omitted | omitted |
| `background` | `{"service_worker": "src/background/service-worker.js", "type": "module"}` | `{"scripts": ["src/background/service-worker.js"], "type": "module"}` | as Firefox (the converter adapts it; *check*) |
| `browser_specific_settings` | omitted | `{"gecko": {"id": "<id>", "strict_min_version": "140.0", "data_collection_permissions": {"required": ["none"]}}}` | omitted (the Xcode project carries the identity) |
| `permissions` | `["storage", "contextMenus"]` | same | `["storage"]` (no `contextMenus`) |
| everything else (icons, `action`, `options_ui`, `content_scripts`, `host_permissions`, `web_accessible_resources`, `content_security_policy`) | as now | as now | as now |

The Firefox add-on **id** is `nymph@binarydicegames.com` (the CMake
cache variable `NYMPH_FIREFOX_ID`). It cannot change without losing
users' settings and the listing.

`web_accessible_resources` uses `matches: ["<all_urls>"]` in every
browser. `use_dynamic_url` stays unused (extension DESIGN section 5.1).

### 5.2 Settings and messages

Unchanged: the keys of extension DESIGN section 5.2 and chat DESIGN, the
message types (`nymph-sniff`, `nymph-count`, `nymph-teach`, the frame's
port messages). One addition, in the options page and service worker only:
the **access state** (`granted` / `needed`), derived from
`permissions.contains`, never stored.

### 5.3 Build outputs

In `<build>/extension/` (the `NYMPH_WEB` build):

| Output | Contents | Goes to |
|---|---|---|
| `nymph/`, `nymph-extension-<v>.zip` | The Chromium package (name and place unchanged) | Chrome Web Store, Edge Add-ons, "Load unpacked" in Chrome and Edge |
| `nymph-firefox/`, `nymph-extension-firefox-<v>.zip` | The Firefox package | AMO; `about:debugging` "Load Temporary Add-on" |
| `nymph-safari/` | The Safari web extension folder, then (macOS only, target `nymph-extension-safari`) the generated Xcode project and the built unsigned `nymph.app` | Hand testing; the App Store after signing |

The three folders differ only in `manifest.json`; a Node test asserts it.

## 6. Build and Packaging

- `extension/CMakeLists.txt` factors the assembly of section 6 of
  extension DESIGN into a function `nymph_extension_package(<browser>)`;
  the Chromium call keeps today's output names.
- The Safari target exists only on macOS (`if(APPLE)`), runs
  `xcrun safari-web-extension-converter --no-open --no-prompt --macos-only`
  on `nymph-safari/` and then `xcodebuild` for an unsigned build. On other
  systems the Safari folder is still produced (it is just files) so the
  tests can check it.
- AMO needs the **source** of any generated or compiled code, and the
  WebAssembly module is compiled from C++: the Firefox upload carries
  `nymph-<v>-src.tar.gz` (release DESIGN section 4.3, which already builds
  offline) and the review notes point to `docs/building.md`. *Check* in
  PLAN step 8 that the tarball builds the web viewer with Emscripten; if
  it does not, that is fixed there.
- Releases: `release.yml` gains `edge` and `firefox` jobs after the
  release is published, each skipping with a warning when its
  credentials are not set, as the `chrome` job does. Safari has no job
  until PLAN step 10.

## 7. Testing Strategy

1. **Node** (`nymph_extension_node`): `ext` and `hasHostAccess()` with fake
   namespaces (`browser` only, `chrome` only, both; promise and callback
   styles); the access-state logic; `runnableOrigin()` with the new gallery
   origins; **manifests**: generate the three, parse them, assert the
   Chromium one equals today's, that each lists only keys its browser
   knows, that every module `loader.js` imports and every file a frame
   loads is covered by `web_accessible_resources`, that the three
   folders' other files are identical.
2. **Chromium** (`nymph_extension_browser`): unchanged, except that the
   scenarios shared with Firefox move into the driver-based list and run
   here through the Chromium driver; nothing is lost.
3. **Edge**: the same run with `channel: 'msedge'` when Edge is installed
   and accepts `--load-extension` (branded Chrome removed the switch in
   version 137, and Edge may follow); otherwise skipped (77) and Edge is
   verified by hand per release. The package is the Chromium one, so the
   engine under test is the same.
4. **Firefox** (`nymph_extension_firefox`, skipped (77) without Firefox,
   `geckodriver` and `selenium-webdriver`, as the browser test is without
   Playwright): Playwright cannot load WebExtensions into Firefox, so the
   Firefox driver uses WebDriver (`installAddon(path, temporary)`). The
   shared scenarios: a nymph PNG becomes a chart in the image's box; a plain
   PNG and a corrupt one stay; a page with a GitHub-like policy (frame or
   inline, whichever the browser does, the chart appears); the button
   toggles a site and the page is restored; parking and the return of the
   user's zoom; a chat code block (the Gemini fixture); the access state
   when host access is withheld. Background hooks are called from an
   extension page via `runtime.getBackgroundPage()`.
5. **Safari**: not automatable (an unsigned extension must be enabled by
   hand in Safari's settings, and `safaridriver` does not load it). CI
   checks that the converter and `xcodebuild` succeed; a **hand checklist**
   in PLAN step 9 records what was verified, on which macOS and Safari.
6. **Packaging scripts**: stub-server tests for the Edge and AMO publish
   scripts, in the style of `tests/chrome_publish_test.mjs`.
7. Native tests are not affected.

## 8. Design Decisions

1. **One tree with a manifest template, not three forks or a bundler.**
   Everything but the manifest is shared, so the content script's and chat
   mode's evolution (this repository's busiest area) is paid once. A
   WebExtension polyfill or bundler would add a dependency at build time
   for what is one line (`globalThis.browser ?? globalThis.chrome`) here,
   because all four browsers expose promise-returning `chrome.*` in MV3.
   Rejected: `webextension-polyfill` (a dependency for callback-era APIs
   the code does not use).
2. **MV3 everywhere, event page for Firefox.** Firefox supports MV3 with
   event pages and no extension service workers; the service worker is
   already written as an event page would be (state in storage, listeners
   at top level). Rejected: MV2 for Firefox (a second manifest model, and
   being phased out in the stores).
3. **Edge ships the Chrome package.** No Edge-specific code or artifact; a
   listing, a script and a test run. Rejected: an Edge manifest variant
   (nothing to put in it).
4. **Detect, don't branch.** A browser check in code would rot when
   browsers add APIs. The one browser-identity use is wording on the
   welcome page (section 3.3).
5. **Host access is a first-class state.** Firefox and Safari users can
   withhold it, and "installed but does nothing" is the worst outcome;
   the button and options page report and fix it. Chrome never enters the
   state (`<all_urls>` is granted on install), so its behaviour is
   unchanged.
6. **Safari through the converter, unsigned in CI, signed by the owner.**
   Apple offers no other route, and signing needs the owner's developer
   account (release DESIGN already treats accounts as owner setup). Safari
   is therefore the one browser whose "done" can be *package builds and
   hand-verified*, not *published*; PLAN step 10 is gated on the owner.
   Rejected: promising a store release before a Mac check shows the viewer
   runs (WebGL2 and WebAssembly are expected to, *unverified*).
7. **Desktop first; mobile keeps room.** The manifest and code avoid
   desktop-only assumptions where free (no window APIs, no hover-only
   controls added), but nothing mobile is built or tested.
8. **Firefox minimum 140.** It is the ESR whose manifest accepts the
   data-collection declaration AMO now asks of new listings; supporting
   older versions would mean a different manifest for no users who matter
   (ESR is the long-tail floor). Revisit if AMO relaxes the requirement.
9. **A driver interface for the browser tests, not a Firefox copy.** The
   existing 1,000-line Playwright test is the specification of behaviour;
   copying it would let the two drift. The shared scenarios are the
   behaviour every browser must show; Chromium-only internals (frame
   throttling, `--load-extension` quirks) stay in the Chromium file.

## 9. Constraints, Invariants and Risks

Invariants (extension DESIGN section 9 still applies unchanged):

- The Chromium package is byte-identical in behaviour; no Chrome test is
  edited to pass.
- No browser's package grants more than Chrome's: `storage`,
  `contextMenus` (not Safari), `<all_urls>` hosts; Firefox adds only the
  `none` data declaration.
- A page is never left with a hidden image and no chart in any browser.

Risks, in the order PLAN step 1 retires them:

1. **Frame mode on strict pages in Firefox / Safari** (section 3.2): if a
   page's `frame-src` blocks the extension frame and the page's policy
   forbids WebAssembly in the content script, charts cannot run on such
   sites (GitHub-like) in that browser. Medium likelihood, high impact on
   the main use case; remedies listed in the table.
2. **Dynamic `import()` in content scripts** (Firefox, Safari): fallback
   is the concatenation step, so low impact.
3. **Safari's feature gaps and permission model**: contextMenus is
   handled; the per-site permission flow is the largest uncertainty.
   Safari may force the design to "works on sites the user allowed" and
   reduced counts of tabs it can read.
4. **WebGL2 / memory limits in Safari** with several live viewers; the
   pool caps (32 inline, 12 frames) may need a lower Safari value, set
   from the measured behaviour, through one constant per mode chosen by
   feature test or by a measured limit (not the browser name).
5. **Review times and policies**: AMO's reviewers read generated code
   (source upload covers it); `<all_urls>` means an in-depth review on
   AMO and Edge as on Chrome; Apple's review of an extension that reads
   every site. Delays, not design changes.
6. **Developer machines**: Safari work needs a Mac and Xcode; the author
   works on Windows, so Safari steps run on CI and on the owner's Mac.

## 10. Integration Boundaries

- **Depends on** the Chrome extension (extension DESIGN), chat mode (chat
  DESIGN), the web viewer's outputs (viewer DESIGN), the source tarball
  (release DESIGN section 4.3) and the release workflow's artifact layout.
- **Changes** `extension/manifest.json.in`, `extension/CMakeLists.txt`,
  every extension module's use of `chrome` (renamed to `ext`), the
  service worker (access state), the options page (access banner, "Copy
  the instructions"), `runnableOrigin()`, the test layout, `release.yml`
  (two jobs), `docs/extension.md`, `docs/releasing.md`.
- **Changes extension DESIGN** only where it says Firefox and Safari are
  out of scope (section 1): it gets a pointer to this project.
- **Couples to** each browser's store policies and WebExtension
  implementation; every `check` row above records what was observed and
  in which version.
- **Owner setup** (docs/releasing.md, one time): the Edge Add-ons
  listing and API keys; the AMO listing, the add-on id and API keys; for
  Safari an Apple Developer account, bundle identifier and signing
  certificates.
