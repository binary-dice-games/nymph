# nymph — Gemini Web Chatbot: Implementation Plan

Design: [DESIGN.md](DESIGN.md). Builds on the Chrome extension
([spec/05_browser_extension](../05_browser_extension/DESIGN.md)) and the
agent text of [spec/06_claude_integration](../06_claude_integration/DESIGN.md).

Steps go from what could invalidate the design (step 1), to pure pieces
(2-3), to the page logic (4-6), to the user's loops and the primer
(7-8), to docs and manual verification (9-10). Each step is a reviewable
deliverable and ends with the full existing native, Node and browser
tests passing unmodified. Extension tests live in
`extension/tests/node/run.mjs` and `extension/tests/browser/run.mjs`
(registered in CTest as the existing `nymph_extension_*` tests). Steps
with user-visible behaviour (4 onwards) add a `CHANGELOG.md` entry. Steps
that touch gemini.google.com record the date and what the page looked
like (the browser and its version).

## Steps

### 1. Feasibility check

**Goal:** the facts DESIGN.md marks *unverified* are answered with
evidence from the real gemini.google.com, and DESIGN.md is updated before
code depends on them.

**Deliverables:** a throwaway content script (not committed; load it
unpacked in Chrome) that logs, and the answers below recorded in this
step's **Result** and folded into DESIGN.md sections 3, 4.3, 4.4 and 9.
Also a **sanitized snapshot of a Gemini answer's DOM** (a message with a
code block, streaming and finished; the composer; the stop button),
committed under `extension/tests/browser/fixtures/gemini/` with text
replaced, to build the fixture page from.

1. Where is a code block in the DOM: `<pre>`, a custom element, inside an
   open or closed shadow root? Is its text reachable from a content
   script (`textContent`, or the "Copy code" button's data)? Its language
   label?
2. How does a streaming answer look in the DOM: does the block's element
   exist from the first token and grow, or is it replaced when finished?
   What marks a pending message (a stop button, an attribute)?
3. The composer: element type; does `execCommand('insertText')`, or a
   synthetic `beforeinput` / `input`, put text in so that the send button
   enables? Does either submit anything on its own (it must not)?
4. Does the extension's current content script run there at all
   (`document_idle`, the page's CSP, Trusted Types)? Does `canRunInline()`
   say no (frame mode expected)? Does the viewer start inside the page in
   frame mode?
5. Is the Gem instruction field long enough for the primer (DESIGN 4.5,
   about 75 lines plus four examples)? And "Instructions for Gemini"?
6. A manual trial of the idea: a Gem with the primer text (draft) and ten
   requests; how often does Gemini answer with one `nymph` block that
   `nymph check` accepts? Record the failures (wrong keys, prose).
7. Images, for the record (the existing extension path): upload a nymph
   PNG to a chat; does the displayed image keep its `nymph` chunk
   (fetch the `currentSrc` and run `nymph` on it)? Does the extension
   replace it?
8. Part 2 probe (optional, needs a US personal account and a public
   URL, e.g. a tunnel to a throwaway MCP server in any language with one
   tool returning a PNG `image` block and a text block): does the Gemini
   app accept the server; does the user see the image; does the model see
   it; does it draw an MCP App (`_meta.ui`)? Otherwise record "not
   verified".

**Tests:** none automated; each answer says how it was checked.

**Go / no-go:** if item 1 or 3 fails (blocks unreachable, composer
unwritable), DESIGN 9's clipboard fallback replaces steps 5 and 7 (a
"draw from clipboard" side panel) and the plan is revised before step 2.

**Result (partly done, 2026-10-19, Chrome with the probe content
script on a real gemini.google.com chat):**
1. *Code blocks (item 1): reachable.* A response holds `<code-block>`
   custom elements in the light DOM (no shadow root), each wrapping
   `div.code-block > div.formatted-code-block-internal-container >
   div.animated-opacity > pre`. `pre.textContent` gives the whole source
   (473 characters for the test block); the header's first line is the
   language label (`YAML` for a block the model wrote as `nymph`/`yaml`).
   The wrappers sit under `response-element > div.markdown >
   message-content > structured-content-container.model-response-text`.
   The Angular class names (`ng-tns-c...`) are generated; the adapter
   relies on `code-block`, `pre` and `.formatted-code-block-internal-container`.
2. *Streaming marker (item 2): not yet seen.* The probe found no button with
   "stop" in its label while idle; `adapters/gemini.js` guesses
   `button[aria-label*="Stop" i]`, and `chat.js` waits 3.6 s instead of 0.6 s
   while it matches, so a wrong guess costs a delay, not correctness.
3. *Composer (item 3): works.* `rich-textarea` with a Quill editor
   (`div.ql-editor`). Focusing it and calling
   `document.execCommand('insertText', false, text)` put the text in the
   box and the round send button turned blue (enabled); nothing was sent.
   A synthetic `beforeinput` event with `inputType: 'insertText'` did
   **not** add text. So the adapter uses `execCommand` first and the
   clipboard as the fallback, and the synthetic-event route of DESIGN 4.4
   is dropped. Not yet checked: inserting after text already in the box,
   and a multi-line text.
4. *Page world (item 4):* `WebAssembly.Module` compiles in the content
   script on gemini.google.com (inline mode is possible; frame mode
   remains the fallback and is tested). `window.trustedTypes` exists; the
   extension builds its DOM with DOM methods and `innerHTML` only inside its
   own shadow root, and the viewer drew charts in the test fixtures under
   both modes. Not yet run on the real page.
5. to 8. *Not yet checked:* Gem instruction length, how reliably Gemini
   follows the primer, whether a nymph PNG keeps its chunk in a chat,
   the hosted-server probe.

Go / no-go: items 1, 3 and 4 are favourable; the plan goes ahead. Items 2
and 5-8 are still open.

**Confirmed on the real page (2026-10-20):** with the extension built from
this repository, a YAML block Gemini wrote (the `bar_groups` example, pasted
into a prompt) is replaced by the live chart, in the message's column.
(The first attempt showed nothing because Chrome was running an older copy
loaded from a WSL checkout; `document.documentElement.dataset.nymphLoader`
and `nymphStatus` now say whether the script ran and what each mode did.)
Not yet checked on the real page: inline or frame mode, the Code and Copy
buttons, a streaming answer, dark theme.


### 2. Source detection

**Goal:** a pure function decides whether text may be a nymph source.

**Deliverables:** `extension/src/content/source-detect.js`:
`looksLikeNymph(text, label)` per DESIGN 3.1 rules 1-2 (label in
`nymph`/`yaml`/`yml`/none; at most 256 KiB; first top-level key is a
nymph key and a top-level `root:` exists),
`hashText(text)` for the cache key, and `fixButtonWanted(text, label)`
(DESIGN 3.4: a `nymph` label or `root:` first).

**Tests** (Node): every `examples/*.yaml` is accepted with each allowed
label; accepted with a leading comment and a document marker `---`;
rejected: a docker-compose file, a GitHub workflow, JSON, an empty
string, a label `python`, 300 KiB of valid-looking text; CRLF line
endings accepted; `hashText` is stable and differs for one changed
byte.

**Result (done):** `extension/src/content/source-detect.js` and four
tests in `extension/tests/node/run.mjs` (20 of 20 Node tests pass). The
plan's first draft required `root:` as the first key; every example
starts with `nymph: 1` (optional in `docs/format.md`), so the rule became
"first key is a nymph key and a top-level `root:` exists" (DESIGN 3.1
updated). Step 1 (the real gemini.google.com) is not done: it needs a
browser session, see `scratchpad/gemini-probe/`.

### 3. `Replacement` with an anchor

**Goal:** `Replacement` mounts a viewer for a source in front of a code
block, and an image still behaves exactly as before.

**Deliverables:** `extension/src/content/replacer.js`: the anchor
interface of DESIGN 4.2; `ImageAnchor` (today's code moved, no change in
behaviour); `CodeAnchor` (host before the block's container, collapse
and restore, sizing of DESIGN 3.2, `data-nymph-ext="source"`, no cover,
badge labels "Chart" / "Code" and "Copy YAML"). `main.js` is changed
only to build an `ImageAnchor`.

**Tests:** all existing browser tests unchanged. New browser tests on a
plain `<pre><code>` fixture: the viewer appears before the block at the
column width, the code collapses to its header and expands with the
badge, copy puts the YAML on the clipboard, turning the extension off
restores the block and removes the host and the attribute, a source the
viewer rejects leaves the block untouched; in inline mode and with
`?csp=github` (frame mode).

**Result (done, together with step 4, which it needs to be tested):**
the plan's "anchor interface" became a smaller change: `replacer.js` now
has a base class `ChartHost` (the viewer inline or in a frame, `yaml()`,
`update()`, `release()`) that `Replacement` (images, behaviour unchanged)
and the new `CodeReplacement` (`code-replacer.js`) extend. The page-level
rule is shared and reference-counted (`injectPageStyle()` /
`removePageStyle()`), so turning one mode off keeps the other's marks.
Frames report the chart's layout size (`rendered` carries `layout`), and
answer a new `set` message with `updated`, so a revised source keeps the
viewer. The host's first shape comes from `sourceSize()` (a regex over
the `image` key, default 800 x 500) and is replaced by the viewer's layout
size once drawn. A chart has no cover (no image to hide); the code stays
visible until `rendered()` resolves, then its body is hidden. DESIGN 4.2
updated. The step's browser tests are in step 4's list below.


### 4. Chat mode: scanning and streaming

**Goal:** on a listed site, finished nymph blocks become viewers and
unfinished ones do not.

**Deliverables:**
- `extension/src/content/chat.js`: entries and states of DESIGN 4.1,
  the observers and idle scheduling shared with `main.js`, the
  completeness rule of DESIGN 3.3 (600 ms debounce plus the adapter's
  `isStreaming`), `setSource()` on a revised block, parking and
  remounting, pool shared with images.
- `extension/src/content/adapters/generic.js` (`pre > code`) and
  `adapters/gemini.js` (`codeBlocks`, `isStreaming`, `theme`, written
  against step 1's snapshot).
- `loader.js` imports `chat.js` on listed sites; `shared/settings.js`
  gains `chatSites` and `chatTheme` (DESIGN 5.2); the toolbar badge
  counts chat charts.
- `CHANGELOG.md`: "Added: Chrome extension draws nymph sources written in
  a chat's code blocks as live charts (Gemini)."

**Tests:**
- Node: the streaming state machine with fake timers (a growing block is
  never considered before it settles; the adapter flag overrides the
  debounce; a block considered and rejected is retried only when its text
  changes); the Gemini adapter's functions over the committed DOM
  snapshot; settings defaults.
- Browser: a fixture page that appends tokens to a code block, shows a
  stop button, then finishes: no viewer during streaming, a viewer within
  1 s of the end; a revised block updates in place keeping a zoom the
  test applied; the message removed and re-added gives the chart back
  from the kept YAML; 40 messages respect the pool cap; a site not in
  `chatSites` is untouched and loads no viewer; plain `yaml` blocks stay;
  inline and frame mode.

**Result (done):** `chat.js` (states `watching -> ready -> mounting ->
live <-> parked`, `plain`), `code-replacer.js`, `count.js` (one toolbar
count for both modes), `adapters/` (`generic` for `pre > code`, `gemini`,
`adapterFor(origin)`), settings `chatSites` (default
`https://gemini.google.com`) and `chatTheme`, `loader.js` starting chat mode
on listed sites and following the settings live. A block is considered 600
ms after its text stops changing (3.6 s while the adapter says the answer
is streaming), by one timer per block, not by polling. Tests: 21 Node
tests (including `sourceSize`) and, in Chromium, for inline and frame mode:
a finished block is drawn and other code left alone; the Code / Chart
badge and Copy; rejected sources stay code; a streaming fixture shows no
chart until the last token; and, once each: a revised block updates in
place, a re-rendered message gets its chart again, an unlisted site is
untouched, and turning the site off restores everything. The whole
browser suite: 30 of 33 pass on this machine; the two failures are the
image-path pixel-tolerance tests (0.34 % of pixels beyond tolerance, 0.5 %
allowed), which fail identically before this work, and the third was a
typo in a new test, since fixed. **Not done from the plan:** the pool is per
mode, not shared with images (each mode caps itself at 32 inline / 12
frames); a 40-message pool test; the Gemini adapter against a DOM snapshot
(it ran only on the generic adapter, as the fixture origin is not
gemini.google.com).


### 5. The composer and the error loop

**Goal:** a rejected source offers a one-click fix prompt that never
submits.

**Deliverables:** `adapters/gemini.js` `composer()` and `insertText()`
with the route of DESIGN 4.4 (`execCommand('insertText')`, else the clipboard),
a `view`-independent function `fixPrompt(error)` (DESIGN 3.4 text), the
error line and **"Ask Gemini to fix"** button in `CodeReplacement`, shown only
when `fixButtonWanted`. `CHANGELOG.md`: "Added: errors in a chat chart
can be sent back to the chat in one click."

**Tests:** Node: `fixPrompt` text for positioned errors; `insertText`
falls back to the clipboard when `execCommand` returns false or throws
(fake composer). Browser: a `contenteditable` composer in the fixture whose
send button enables on input: the click fills it, the send handler is
never called, the existing text in the box is kept (text is appended
after a blank line); with a composer that rejects `execCommand`
the text goes to the clipboard and the button says so; no error UI for a
plain YAML block.

**Result (done, 2026-10-20):** a block is **claimed** (label `nymph`, or
first key `nymph:` or `root:`; `claimsBlock()`) and then always replaced:
the viewer rejecting it makes `CodeReplacement` show an error panel
(`describeError()`: position first, then the viewer's message) instead of
the chart, with "Code", "Copy error" and **"Ask Gemini to fix"**; the fix
button uses `insertIntoComposer()` (`execCommand`, else the clipboard) and
never sends. The plan said the error would appear only for claimed blocks
that the viewer rejected and that unclaimed ones stay silent: both hold.
`chat.js` has a state `error` (a host with no viewer, outside the pool);
a changed block is drawn again from scratch. Found on the real page: Gemini
wrote `xs: $month` with month names (`'Jan' is not a number`); the primer
now says xs and ys are numbers and category names go in `x_tick_labels`
(the corrected chart validates). Tests: `claimsBlock` (Node, 22 of 22) and
browser checks, inline and frame: three invalid claimed blocks get panels
with the detail and the position, an unclaimed block gets no host, the Code
button shows the code and hides the error, the fix button fills the prompt
box and sends nothing (39 of 39). The step's "no error UI for plain YAML"
test is the unclaimed `description:` block.


### 6. Theme and sizing

**Goal:** chat charts follow the chat's theme and column.

**Deliverables:** adapter `theme()`, `chatTheme` option, `fit: 'width'`
host sizing with `max-width` from the source's `image.width`, a resize
observer on the column so a window resize reflows without recreating the
viewer.

**Tests:** browser: the fixture toggles a dark class on the page: the
chart theme follows (pixel check of the background, as the existing theme
test does); `chatTheme: source` keeps the source's theme; the host is
never wider than the column, nor wider than `image.width`; resizing the
window re-fits without a new viewer.

**Result:** *(to be filled in)*

### 7. Sending the view back

**Goal:** the user can put the chart's current view into the prompt box
with one click.

**Deliverables:** the badge's "Send view to Gemini" (shown after a
500 ms-debounced `view` event that changed the `view` key text), the
message text of Claude DESIGN 4.4 built by a function shared with the
MCP App if it can be (otherwise a second implementation tested against
the same golden strings), insertion through `insertText`. Shared code
between `claude/app/app.js` and the extension is only attempted if it
needs no build step; otherwise the two copies are tied by a golden-text
test. `CHANGELOG.md`: "Added: a chat chart's zoom can be sent back to
the chat."

**Tests:** Node: the message text for a view with one entry, a view and a
click, a click only, and the unchanged chart ("The view is the chart as
written"), equal to the Claude App's golden strings. Browser: a drag
makes the button appear, an untouched chart never shows it, the click
fills the composer and does not submit.

**Result:** *(to be filled in)*

### 8. The primer

**Goal:** the user can give Gemini the instructions in two clicks.

**Deliverables:**
- `claude/skill/nymph/SKILL.md`: `<!-- gemini:skip -->` markers around
  the server-only parts (tools, output paths, "look at the PNG"); the
  Claude consumers' outputs (server `instructions`, plugin and Skill
  copies) must stay byte-identical, so the markers are removed by every
  consumer (the existing generator for `instructions` and the plugin
  sync get the same strip rule).
- `extension/src/gemini/primer.tail.md` (the Gemini tail and four short
  examples, each checked by `nymph check` in a test).
- `cmake/NymphExtensionPrimer.cmake`, target `nymph-primer-sync`:
  writes `extension/src/shared/primer.js`; committed.
- The options page: a section with "Copy the instructions" (copy button,
  link to the Gem creation page, three lines of steps), the chat site list
  (add / remove), and the theme setting. "Give this chat the nymph
  instructions" in the toolbar button's context menu puts the primer into
  the composer, unsent.
- `CHANGELOG.md`: "Added: instructions that teach a Gemini Gem to draw
  nymph charts."

**Tests:** `primer_sync` (CMake script test, like `plugin_sync_versions`):
regenerating reproduces `primer.js` byte for byte; the primer contains
no mention of `render_chart`, `output_path` or `.png`; the existing
plugin / server-instruction tests prove the Claude outputs did not change;
every example in the tail passes `nymph check` (the native build's
executable runs it); the primer is under the Gem instruction limit step 1
found. Browser: the options page copies the text, adds and removes a
site, and the new setting takes effect live without reload.

**Result (done):** the plan's marker approach was replaced: the primer
has its own template, `extension/gemini/primer.md.in`, and
`cmake/NymphExtensionPrimer.cmake` inlines `examples/{line,pie,flowchart,
uml_class}.yaml` (6,429 characters). The Claude consumers of `SKILL.md` are
untouched. The options page (`src/options/`) gained a section for chat sites
(add, remove, address checked), the chat theme, and the instructions with
a copy button, the Gem steps and a link; the "Teach this chat" action is the
toolbar button's context menu (`contextMenus` permission, `teach()` in
`chat.js`, `composer()` and `insertIntoComposer()` in the adapters, a toast
on the page), not a badge button. `nymph_gemini_primer` (CTest) and nine new
browser checks pass (37 of 37 in the browser suite, 21 of 21 Node tests).
The tests found a real bug: Chrome's URL parser accepts "not a site at
all" as a host (percent-encoding the spaces), so the options page stored it;
addresses are now checked for a valid host name. The two image-path pixel
tests that failed on the first runs passed in this run and the cause is not
known. Not checked: a Gem's instruction length, how well Gemini follows the
primer (step 10), and `execCommand` with a 6 KB multi-line text in Gemini's
editor.

**Found on the real page (2026-10-20):** a Gem following the primer wrote a
valid line chart, but also leaked its reasoning and repeated the source as
text (the primer now says: one block, one short sentence, never repeat or
recap), and wrote an invalid `SankeyDiagram` (a made-up `data:` list of
objects; nymph: "SankeyDiagram has no field 'data'"), which stays code with
no message to the user. The primer now has seven examples (adds Sankey,
Gantt, sequence; 9.7 KB, limit in the check raised to 10,000) and says to
draw a `Flowchart` for any type without an example. Every example in it
passes `nymph check`. The silent failure is what step 5 is for.

**Web examples (2026-10-20):** to keep Gemini from inventing fields the
primer also gives the address of the raw example sources
(`https://raw.githubusercontent.com/binary-dice-games/nymph/refs/heads/main/examples/NAME.yaml`,
the names listed by the generator from `examples/`) and tells it to open
the one for a type it has no example of, never `docs/elements.md` (326 KB,
cut off by a fetch tool a third of the way through). Five examples stay
inline (line, pie, flowchart, class, Sankey, Gantt: 9.4 KB) in case a Gem
cannot open links; sequence moved to the web list. The repository must stay
public for this to work (it was readable without a login on this date).
**Not yet checked:** whether a Gem opens the link at all, and how long it
takes. A check (`nymph_gemini_primer`) fails when an example is not named.


### 9. Documentation

**Goal:** a reader sets up nymph in Gemini from the docs alone.

**Deliverables:** `docs/gemini.md` (what you get, the three steps:
install the extension, make a Gem, ask for a chart; the loops; limits:
Gemini web only, the user clicks to send; how it differs from Claude;
privacy statement of DESIGN 9), `docs/extension.md` (chat mode, the site
list, the new badge buttons), `docs/claude.md` (a link), `README.md`
(one line in the intro and the structure row, under about 200 lines),
`spec/INDEX.md` (status).

**Tests:** existing doc checks, a read-through.

**Result:** *(to be filled in)*

### 10. Manual verification and the Part 2 decision

**Goal:** the design is checked on the real site, and the follow-on is
decided.

**Deliverables (recorded here and in DESIGN.md's status line):**
- gemini.google.com with the built extension and a Gem carrying the
  primer, in Chrome (version): the ten requests of DESIGN 7 item 3; for
  each: did Gemini answer with a usable block, did it draw, how many
  error-loop rounds were needed; the zoom loop; reload; switching
  between chats; dark and light theme; a long conversation (pool).
- The same without the Gem, with the badge's per-chat primer.
- A decision, using step 1 item 8: start `spec/11_...` (hosted `nymph mcp`
  as a Gemini custom app) or not.

**Tests:** none automated.

**Result:** *(to be filled in)*

## Completion Criteria

1. Steps 2-8 tests pass in CI together with every existing test, which
   pass unmodified.
2. On gemini.google.com, with the extension and a Gem carrying the
   primer, a chart Gemini writes is drawn live in the conversation, in the
   chat's theme and width, in at least 8 of the 10 trial requests within
   two error-loop rounds.
3. The extension never submits a prompt, never reads a site that is not
   in `chatSites`, and turning it off restores every code block.
4. `README.md`, `docs/gemini.md`, `docs/extension.md`, `CHANGELOG.md` and
   `spec/INDEX.md` are current; DESIGN.md's *unverified* marks are
   resolved or restated as known limits.
