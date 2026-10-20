# nymph — Gemini Web Chatbot: Design

**Status: design only; nothing is built.** [PLAN.md](PLAN.md) is the
ordered implementation plan; its step 1 checks the facts marked
*unverified* before code depends on them. This document describes only
what this project adds or changes. It builds on
[spec/05_browser_extension/DESIGN.md](../05_browser_extension/DESIGN.md)
(*extension DESIGN*: the Chrome extension and how it hosts the viewer on
pages we do not own),
[spec/06_claude_integration/DESIGN.md](../06_claude_integration/DESIGN.md)
(*Claude DESIGN*: the `nymph mcp` server, agent text) and
[spec/04_web_viewer/DESIGN.md](../04_web_viewer/DESIGN.md) (*viewer
DESIGN*).

## 1. Purpose / Scope

The user chats with **Gemini in the browser (gemini.google.com)**. They
want Gemini to draw nymph charts and diagrams and the user to see them,
live (pan, zoom, values on hover), inside the conversation.

Why the Claude route does not carry over:

| Claude route | Gemini web |
|---|---|
| Local `nymph mcp` over stdio (plugin, `.mcpb`) | Gemini's custom apps take a **server URL** that Google's servers call, so a local stdio server is unreachable. They need a personal Google account, age 18+, US, English, Keep Activity on (rolling out since 2026-06-29). |
| MCP App widget in the chat | Documented for Gemini Enterprise only; for the consumer app nothing says tool results are drawn as widgets or even as images. *Unverified.* |
| Skill / plugin teaches the model | There is no install step that adds instructions to Gemini; the user's route is a **Gem** (custom assistant with instructions) or the "Instructions for Gemini" setting. |

What Gemini web *can* do is write text. Gemini writes a nymph YAML source
in a fenced code block; the **Chrome extension** (extension DESIGN) sees
the block and shows a live viewer in the chat, as it already does for
nymph PNGs. No server, no account restrictions, no hosting.

Hard requirements, from the request:

1. **Gemini uses nymph** to produce charts and diagrams.
2. **The user sees them in the conversation, live.**
3. Reuses the existing nymph pieces: the viewer, the Chrome extension,
   and where it helps `nymph mcp`.

In scope:

- **Chat-source mode** for the Chrome extension (section 4): finds nymph
  sources in a chat's code blocks and replaces them with viewers;
  streaming-aware; Gemini adapter first.
- **The primer** (section 4.5): the instructions Gemini needs, as one
  compact text, with ways to give it to Gemini (a Gem, the settings, a
  button that puts it in the prompt box).
- **The loop with the user** (section 4.6): errors and the user's view
  come back to Gemini through the prompt box, with the user's click.
- **Part 2, gated** (section 9): a hosted streamable-HTTP `nymph mcp` as
  a Gemini custom app, if step 1 shows Gemini displays or reads its
  results.

Out of scope: Firefox / Safari; the Gemini mobile app and Gemini
Enterprise (no extension runs there; Enterprise would be Part 2); other
chatbots (the adapter list makes ChatGPT and claude.ai a small later
addition, section 4.3); uploading charts as images to Gemini; any change
to the source format or the viewer's JavaScript API.

New and changed files:

| Path | What |
|---|---|
| `extension/src/content/chat.js` | Chat-source mode: scanning, streaming, entries |
| `extension/src/content/source-detect.js` | `looksLikeNymph(text)` and `languageOf(block)` (pure) |
| `extension/src/content/adapters/gemini.js` | The Gemini adapter: code blocks, anchors, composer |
| `extension/src/content/replacer.js`, `code-replacer.js`, `count.js` | `ChartHost` base class, `CodeReplacement` (section 4.2), one chart count for both modes |
| `extension/src/shared/primer.js` | The primer text (generated), `settings.js` gains `chat` keys |
| `extension/src/options/` | The primer page and chat settings |
| `cmake/NymphExtensionPrimer.cmake` | `nymph-primer-sync`: generates `primer.js` from `claude/skill/nymph/SKILL.md` |
| `docs/gemini.md`, `docs/extension.md` | Using nymph with Gemini; chat mode |

## 2. Design Goals

1. **Nothing to host.** Part 1 needs the extension and a Gem; no server,
   no OAuth, no US-only account feature.
2. **Indistinguishable from the chat.** The viewer takes the code block's
   place in the message, at the message's width, in the chat's light or
   dark theme; the chat's own scrolling, copy and "show code" keep
   working. (Extension DESIGN goals 1 and 2 for sources.)
3. **Never break the page, never show half a chart.** Any failure leaves
   the code block as Gemini wrote it. A streaming answer is not drawn
   until the block is complete and valid.
4. **Cheap.** A chat page without nymph sources costs one pre-check per
   finished code block and loads no WebAssembly (extension DESIGN
   goal 4).
5. **The user stays in control.** The extension reads the chat's code
   blocks, which images never needed; it does so only on the sites the
   user listed (section 5.2), reads only block text, sends nothing
   anywhere, and never types into the prompt box or presses send itself.
6. **Same sources, same viewer.** A chart Gemini writes is the YAML that
   `nymph chart.yaml` draws; the user can copy it out of the block.

## 3. Behaviour

### 3.1 What is a nymph source

A **code block** (`<pre>` with a `<code>` child, or the adapter's
equivalent) is a candidate when it is complete (section 3.3) and:

1. its language label is `nymph`, `yaml` or `yml`, or it has none; and
2. its text is at most 256 KiB, its first top-level key (after blank
   lines, comments and `---`) is one of `nymph`, `description`, `image`,
   `data`, `source` or `root` (`docs/format.md`), and it has a top-level
   `root:` key; and
3. the viewer accepts it (`setSource()` resolves `true`).

Rules 1 and 2 are the cheap pre-check (`looksLikeNymph`); rule 3 is the
parser, so the format has one reader (extension DESIGN section 8, "Sniff
in JavaScript, parse in C++"). A plain `yaml` block that is not nymph
fails rule 2 or 3 and is left alone, unless it is *claimed* (section 3.4), in
which case it gets an error panel.

### 3.2 The result in the chat

The viewer goes into a **host element** inserted right before the code
block's container (the chart reads before its code, as in a notebook),
with a shadow root as in extension DESIGN section 3.4. The code block
stays, collapsed to its header line ("nymph source, 24 lines - show") so
the message does not double in length; the badge (extension DESIGN 3.3)
has "Chart" / "Code" and "Copy YAML" (the block's text, or the viewer's
`toYaml()` with the user's view when it differs).

Sizing: the host fills the message column up to the source's own
`image.width`; the chart uses the viewer's `fit: 'width'` (the layout
follows the column) so a narrow window re-flows rather than shrinks. A
source with `image.width` smaller than the column is shown at that size
(like the PNG would be). The theme follows the page (`'auto'`), not the
source, because a chat is dark or light by the user's choice; a setting
returns to the source's theme.

### 3.3 Streaming

Gemini writes its answer token by token, so a code block grows in the DOM
for seconds. Rules:

- A block is **complete** when it has not changed for 600 ms **and** the
  adapter says its message is not streaming (a "stop" button in the
  composer, or the adapter's marker for a pending message), whichever the
  adapter can read; the debounce alone is the fallback. Only complete
  blocks are considered.
- A block that is considered and rejected (rule 3 of 3.1) is marked
  `plain` for that text; if its text changes (the answer was edited, or
  regenerated in place) it is considered again.
- A completed block whose text changes (the model "revised" its answer)
  updates its viewer with `setSource()`, keeping the user's zoom where
  the chart allows (Claude DESIGN 4.2).
- A message the page removes or replaces takes its viewers with it; a
  re-render of the message puts a viewer back from the kept YAML (the
  parking of extension DESIGN 3.5), at most three times.

### 3.4 Errors

A block that is plainly meant as a nymph source is **claimed**: its label
is `nymph`, or its first key is `nymph:` or `root:`. A claimed block is
always replaced by the host, so the user never sees a bare code block and
wonders why no chart came. When the viewer rejects it, the host shows an
error panel instead of the chart: "nymph could not draw this chart" and the
viewer's message, with its position and what is wrong (for example
`line 18, column 3: column 'month', row 1: 'Jan' is not a number`). The
code is one click away ("Code"), "Copy error" copies the message, and
**"Ask Gemini to fix"** puts

```
The nymph chart failed to draw: <error text>
Fix the YAML and send the whole chart again.
```

into the prompt box, without sending it (section 4.4). This is the loop
Claude gets from `isError` results (Claude DESIGN goal 3), with the user's
click in place of the tool call. A block that is not claimed (plain `yaml`
whose first key is something else, such as `description:`) is still tried
by the viewer, but when it fails it is left alone and nothing is said, so
other YAML is never flagged. When the block's text changes (the answer was
revised, or finished streaming), the panel is replaced by a chart or a new
panel.

### 3.5 The user's view goes back

After the user pans or zooms and stops (500 ms), the badge shows
"Send view to Gemini"; clicking puts the text of Claude DESIGN 4.4
(`nymph chart "<title>": the user changed the view. view: ...`, with
the `view` key and the last click) into the prompt box, unsent, so the
next prompt ("add a trend line to this part") carries the zoom. Nothing is
sent without a click, and nothing on its own.

## 4. Key Abstractions

All in `extension/src/`, plain ES modules (extension DESIGN section 4).

### 4.1 `content/chat.js`

The chat-source analogue of `main.js`: observes the page, keeps an entry
per code block, moves it through `watching -> complete -> live <-> parked`
or `plain`, and shares the viewer pool and the idle-callback scheduling
with `main.js`. It starts only on sites the settings list (5.2), from the
same `loader.js`. It reuses `pool.js`, `cache.js` (keyed by text hash),
and `Replacement`. It never runs work in an observer callback.

### 4.2 `CodeReplacement` and the shared `ChartHost`

`replacer.js` holds `ChartHost`: a host element and a viewer running
inline or in an extension frame (extension DESIGN 3.9), with `yaml()`
(the source with the user's view), `update(source)` (another source in the
same viewer, keeping the view where the chart allows) and `release()`.
`Replacement` (images, unchanged) and `CodeReplacement`
(`code-replacer.js`) extend it. `CodeReplacement` takes a code block
`{container, body, text, label}` from the adapter: the host goes before
`container`, and `body` is the part hidden once the chart is drawn (with
`data-nymph-ext="source"`, and `source-shown` while the user looks at
the code), so a site's own header and copy button stay. The host's first
shape comes from the source's `image` size (`sourceSize()`, default
800 x 500) and is replaced by the viewer's layout size when drawn. A chart
needs no cover (there is no image to hide): the code shows until
`rendered()` resolves. An earlier draft of this design had an "anchor"
interface for both cases; a base class changes less of the image code.

### 4.3 Adapters

A site-specific adapter is a small module returning, for a document:

| Method | Gemini |
|---|---|
| `codeBlocks(root)` | The elements that wrap a code block in a response, with their language label and text. Written against the DOM step 1 captures; custom elements and shadow roots are the risk (extension DESIGN scopes shadow roots out). |
| `isStreaming(block)` | Whether the block's message is still being written. |
| `composer()` | The prompt box (a rich-text editor, probably `contenteditable`), or `null`. |
| `insertText(text)` | Puts text into the composer through the editor's own input events (`beforeinput` / `insertText`), so its framework sees a user edit; never submits. |
| `theme()` | `'light'` or `'dark'` from the page. |

`adapters/gemini.js` is the only one in this project. Other sites add an
adapter and a site-list entry; the generic `pre > code` adapter (default,
used when a site has none) covers plain markdown renderers. The sites
list (5.2) ships with `gemini.google.com`.

### 4.4 Writing into the prompt box

Text insertion into a framework-controlled editor is the fragile part. The
adapter focuses the composer and calls `document.execCommand('insertText')`,
which Gemini's Quill editor accepts as typing (measured 2026-10-19: the text
appears and the send button enables). A synthetic `beforeinput` event does
not work there, so it is not used. When `execCommand` returns false or the
composer is not found, the text is copied to the clipboard and the badge
says "Copied; paste it into the box". It never calls anything that submits.
Every path is behind one function so a site change breaks that one function
and the badge falls back to the clipboard.

### 4.5 The primer

Gemini knows nothing about nymph's format. The **primer** is a text of about
6,400 characters, written for a chat model with no tools:

- how to answer: **one fenced block labelled `nymph`** and at most two
  sentences around it; no other way of drawing charts; no invented numbers;
  the whole corrected source when the user pastes an error; the `view:` key
  of a note about the user's zoom copied into the next source;
- the source: element names by family, `$column` references, the three rules
  behind most errors (spaces, quoted colours, `data: |`), what makes a good
  chart;
- four examples: a line plot, a pie chart, a flowchart and a class diagram.

It is generated, not copied from the Claude skill: that text is about tools
(`render_chart`, `output_path`, "look at the PNG") and cutting it into
Gemini and Claude parts would mean marking up a file four Claude packages
consume byte for byte. `extension/gemini/primer.md.in` is the template and
`cmake/NymphExtensionPrimer.cmake` replaces each `{{example:NAME}}` with
`examples/NAME.yaml`, so the examples in the primer are the ones the test
suite already draws; the result, `extension/src/gemini/primer.txt`, is
committed and shipped. A test (`nymph_gemini_primer`) fails when it is
stale, names a tool Gemini lacks, does not hold an example verbatim, or is
over 8,000 characters. Line endings do not matter to the test or to the
readers (CMake on Windows writes CRLF). Whether a Gem's instruction field
holds 8,000 characters is still to be checked (PLAN step 1, item 5).

Ways to give it to Gemini, in order of effort:

1. **A Gem** (recommended): the options page shows the instructions, with
   "Copy the instructions", the steps and a link to Gemini's new Gem page;
   the user pastes them once.
2. **Instructions for Gemini** (the settings): the same text; it affects
   all chats.
3. **Per chat**: the toolbar button's context menu has "Give this chat the
   nymph instructions"; the page's content script puts the text into the
   prompt box (`execCommand`, section 4.4), unsent, and says so in a short
   message at the bottom of the page. With no usable prompt box the text
   goes to the clipboard. A menu item rather than a button on the page keeps
   the page clean when there is no chart.

### 4.6 What is unchanged

The viewer, the sniffer, the image path, the toolbar button and badge
count (chat charts count too), the pool and the frame mode, and
`nymph mcp` are untouched.

## 5. Contracts

### 5.1 Manifest

No new permission: `<all_urls>` and `storage` already cover it. The
content script (`loader.js`) imports `chat.js` only on listed sites.
`web_accessible_resources` gains `src/content/adapters/*`. Nothing for
`clipboardWrite` is needed beyond a user gesture (the copy buttons).

### 5.2 Settings (`chrome.storage.sync`)

| Key | Type | Default | Meaning |
|---|---|---|---|
| `chatSites` | array of origins | `["https://gemini.google.com"]` | Sites where code blocks are read. Empty turns chat mode off. |
| `chatTheme` | `"auto"` \| `"source"` | `"auto"` | Viewer theme for chat charts. |

The site list is explicit here, unlike images ("everywhere, off by
exception", extension DESIGN section 8): reading a page's text is more
than reading its images, so chat mode is on only where the user listed
it, with the options page adding or removing sites. The existing per-site
toggle (`off`) still turns everything off for a site.

### 5.3 DOM markers

As extension DESIGN 5.4, plus the host element `<nymph-ext-chart>` before
a code block, and the attribute `data-nymph-ext="source"` on the block's
container (collapsed). Turning the extension off removes them.

## 6. Build and Packaging

`nymph-primer-sync` writes `extension/src/shared/primer.js` (a committed,
generated module exporting the text). The extension zip and unpacked
folder are produced as before. `docs/extension.md` gains chat mode.

## 7. Testing Strategy

1. **Node** (`extension/tests/node/`): `looksLikeNymph` over the example
   sources (all accepted), non-nymph YAML (rejected), a 300 KiB block
   (rejected), comment-first files; the streaming state machine (debounce,
   adapter flag) with fake timers; the Gemini adapter's pure parts over a
   saved DOM fixture; `primer.js` against `SKILL.md`.
2. **Browser** (`extension/tests/browser/`): a fixture page that mimics
   Gemini's DOM captured in step 1 (a message list that appends tokens
   into a code block, a stop button, a `contenteditable` composer), in
   inline and frame mode (`?csp=github`): a finished block becomes a
   viewer; a half-streamed block does not; a revised block updates; a
   rejected block shows the error and the fix button, which fills the
   composer and does not submit; a plain YAML block stays; the badge
   toggles chart and code and copies YAML; send-view fills the composer
   after a drag; a message removed and re-added restores the chart; a
   site not in `chatSites` is untouched.
3. **Manual** (PLAN step 8): the real gemini.google.com with a Gem
   carrying the primer: ten varied requests (plot from pasted data,
   flowchart, class diagram, Gantt, UI mockup, a request Gemini answers
   with the wrong block), the error loop, the zoom loop, a page reload,
   switching chats, light and dark theme.

## 8. Design Decisions

- **The extension, not a hosted server, for Part 1.** The Gemini app's
  custom apps need a public HTTPS endpoint, OAuth and US / personal-account
  eligibility, and nothing documents the result being drawn. The
  extension works for every Gemini web user, today. Rejected as the first
  step: hosting (cost, auth, abuse); making a Gemini app of the MCP App
  (no evidence the consumer app renders MCP Apps).
- **The source in a code block, not an image.** The image route needs
  Gemini to produce a nymph PNG, which only a tool can. A code block is
  something Gemini does well and the extension reads reliably. The YAML is
  also what the user wants to keep and edit.
- **The user closes the loop.** Without a tool, Gemini cannot check its
  chart. The error button and the send-view button make that one click
  each, and never send for the user (goal 5). Rejected: auto-submitting
  (a script typing as the user into a chat is exactly what a page and its
  user would not expect).
- **A generalised `Replacement`, not a second viewer path.** The inline /
  frame choice, pooling, parking and theming are the hard parts and are
  already tested; only the anchor differs.
- **Site list for chat mode.** Reading text is a larger permission in
  spirit than fetching images; the list keeps the extension's "never
  surprising" promise. The cost is a settings entry for each new chat
  site.
- **Hide nothing the chat needs.** The code block collapses but stays in
  the DOM, so copy, find-in-page and the chat's own "Copy code" keep
  working, and removing the extension changes nothing in the history.
- **Primer as text the user pastes.** There is no install hook into
  Gemini; the Gem is the product's own mechanism. Rejected: the extension
  silently prepending instructions to every prompt (invisible, and it
  would change what the user sends).

## 9. Constraints, Risks, Open Questions

- **Gemini's DOM.** Code blocks may live in custom elements or shadow
  roots the content script cannot reach, the composer may reject
  synthetic input, and the markup changes without notice. The adapter is
  one file; the fallbacks (clipboard for text, `pre > code` for blocks)
  keep the feature degraded rather than broken. Step 1 decides whether
  Part 1 is feasible at all; if the code blocks are unreachable
  (closed shadow roots, canvas), Part 1 falls back to the clipboard route:
  a toolbar button "Draw nymph chart from clipboard" that opens the
  viewer in a side panel (`chrome.sidePanel`) or a tab.
- **Gemini's compliance.** The model may answer with prose, a different
  language label, or truncated YAML. The pre-check accepts an unlabelled
  or `yaml` block, and the error button covers broken sources; a Gem
  with the primer is expected to be reliable but is measured in step 8.
- **Trusted Types / CSP.** gemini.google.com sets a strict CSP; the
  frame mode (extension DESIGN 3.9) exists for this and is the expected
  mode there. Step 1 confirms.
- **Terms.** The extension reads the page the user is already viewing and
  inserts text the user will review; it does not call Gemini, scrape
  histories or automate sending. Stated in `docs/gemini.md`.
- **Part 2: a hosted `nymph mcp` as a Gemini custom app.** `nymph mcp
  --http` (streamable HTTP, OAuth 2.1 with dynamic client registration,
  the transport Claude DESIGN section 10 names), behind a public URL,
  which would give Gemini real `render_chart` / `check_chart` tool calls
  and close the loop without the user. Worth starting only if step 1
  shows the consumer app (a) accepts the server, (b) shows or reads the
  returned image, and ideally (c) draws an MCP App. It is a project of
  its own (`spec/11_...`) with hosting, auth and abuse limits, and it
  would also serve Gemini Enterprise, which renders MCP Apps.
- **Other chatbots.** The adapter interface is general; ChatGPT and
  claude.ai pages would each need an adapter and a site entry. Not
  planned.

## Sources (read 2026-10-19)

- Gemini app custom apps (eligibility, URL entry, no mention of widgets):
  support.google.com/gemini/answer/17209137.
- MCP Apps in Gemini Enterprise (not the consumer app):
  cloud.google.com/blog/topics/developers-practitioners/devs-guide-to-mcp-apps-in-gemini.
- Third-party summary of where Gemini MCP works:
  tactiq.io/learn/google-gemini-mcp-support.
- Gem instructions and the "Instructions for Gemini" setting: Gemini
  Apps Help (step 1 records the limits).
