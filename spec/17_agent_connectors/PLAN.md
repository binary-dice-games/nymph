# nymph — Agent Connectors: Plan

Design: [DESIGN.md](DESIGN.md). Each step is a reviewable deliverable; the
next starts after its tests pass. Native tests go in `tests/` with
`nymph_add_test(<name> <sources>)`; web tests extend `web/tests/node/` and
`web/tests/browser/` (Playwright with the preinstalled Chromium, skipped
with 77 when Playwright is missing), serve the built site from a local
server on port 0, and wait on conditions, never fixed sleeps. No test
calls a real model API: the provider APIs are local stub servers. A
`CHANGELOG.md` entry is added with each user-visible step (4 to 11).

## 1. Risk check: what the browsers, providers and clients really do

**Goal:** every *unverified* fact in DESIGN.md is checked, and the design
is corrected where one is false, before anything is built.

**Deliverables:** throwaway pages under the scratch build (not committed)
and the results written into this step's **Result**, item by item:

1. `@anthropic-ai/sdk` and `@google/genai`: does each package's published
   ES module build import in Chromium, Firefox and Safari from
   same-origin files, with no bundler, under the app's CSP? Size (gzip)
   of what a page fetches. If one does not, DESIGN.md section 6.4 switches
   that connector to `fetch`.
2. Claude from a page on a public origin: a streamed Messages request
   with tools, `dangerouslyAllowBrowser`, adaptive thinking, `effort`,
   `fallbacks: "default"`, `cache_control`, context editing, and an image
   inside a `tool_result`; check `usage.cache_read_input_tokens` on the
   second step.
3. Gemini from a page: CORS with an API key, streamed function calling
   with three of our schemas through `toGeminiSchema()`, an image returned
   with a function response (in the response part, or only as a separate
   part).
4. WebMCP: which Chrome version and flag or origin trial exposes
   `navigator.modelContext`; the registration API's exact shape; whether
   Chrome's own agent (Gemini in Chrome) or Claude in Chrome calls a
   page's tools.
5. `ws://127.0.0.1:<port>` from an https page on `*.github.io`: Chrome
   (Local Network Access prompt for WebSockets, wording, one-time or
   per-site), Firefox, Safari (mixed content).
6. `notifications/tools/list_changed`: do Claude Code, Cursor and Gemini
   CLI re-read `tools/list` from a stdio server mid-session?
7. Clipboard write and download from inside a confirmation dialog's
   Allow handler after an `await` of a promise (the registry's path).

**Tests:** none committed; each item records what was run, on which
version, and the outcome.

## 2. Registry: the agent tool list and origins

**Goal:** the registry gives every connector one stable, guided tool list
and owns the origin rules.

**Deliverables:** `commands.js`: `agent` field, `agentTools()`,
`callAgent(name, args, connector)` building `agent:<connector>` (rejects
anything outside `claude`, `gemini`, `webmcp`, `mcp:<name>`), guidance
results for unavailable commands (DESIGN.md section 4.2), "Allow for this
session" in the confirmation dialog. `main.js`: `agent: false` on the
eight shell-only commands, `app_read_guide`, `app_example_source`.
`web/app/editor-tools.json` (generated from the editor module by a small
Node script, `web/tools/dump-editor-tools.mjs`, committed). `device.js`:
`assistant` from `config.json`'s `agents`.

**Tests:** Node: `editor-tools.json` equals the editor module's
`toolDefinitions()`; every `agentTools()` schema is valid JSON Schema.
Browser: `agentTools()` is the same in View and Edit, has the editor
tools on desktop and none on an emulated phone, and no `agent: false`
command; `editor_set_fields` called by `callAgent` in View mode returns
the "call app_set_mode" guidance; `callAgent` with connector `user` or
`agent:x` throws; an `external` command with "Allow for this session"
ticked runs a second time without a dialog, and asks again after the
document closes; `app_read_guide` returns `agent-guide.md` and refuses
`../index.html`; `app_example_source("line")` returns the example.

## 3. `editor_model`: another origin ends the group

**Goal:** a user's change during an agent's turn is never undone with it.

**Deliverables:** `editor_model::commit` (and every change that goes
through it) ends the open group when the change's origin differs from the
group's; `spec/15_web_app/DESIGN.md` section 5.1 notes it.

**Tests** (`test_editor_attribution`): agent group, two agent commits, a
user commit, an agent commit after `begin_group` again: three undo
entries, labelled agent, user, agent; undo once restores only the last
agent change; a commit with the group's own origin still joins it; the
existing attribution tests pass.

## 4. Assistant core with a scripted adapter

**Goal:** a provider-neutral session runs turns against the registry,
with one undo step per turn, undo by snapshot, Stop and the step limit.

**Deliverables:** `assistant/session.js` (DESIGN.md sections 5.1, 5.2),
`assistant/turns.js` (section 4.5), `assistant/instructions.md` (section
5.3); a test-only scripted adapter (`web/tests/browser/fake-adapter.js`)
taking a list of responses.

**Tests:** Node: the instructions name only `agentTools()` commands and
are under 8,000 characters. Browser (scripted adapter): a turn with two
`editor_set_fields` calls in Edit mode is one undo step labelled with the
turn; in View mode a `document_set_source` turn is undone by the turn's
Undo (snapshot); a user edit between two agent writes of a turn survives
the turn's Undo, and Undo then asks before restoring; Stop during a turn
ends it with no further tool call; the 26th step is not requested; a tool
error goes back to the adapter as an error result; `app_set_mode` from
the agent switches modes and Undo still works by snapshot; the first
request carries the state note, later ones do not; the history passed to
the adapter only ever grows.

## 5. Chat panel

**Goal:** the user opens the Assistant, picks a connector, enters a key,
chats, sees each change and undoes it, on a desktop and on a phone.

**Deliverables:** `assistant/chat.js`, `assistant/markdown.js`,
`assistant/settings.js` (key in memory, "Remember on this device" in
IndexedDB, consent per connector), the Assistant button and
`Ctrl+Shift+A`, dock content and bottom sheet (`app.css`), "Apply to
chart" on YAML blocks, the activity list for external connectors (used
from step 8 on); `docs/web-app.md` section on the assistant.

**Tests:** Node: `markdown.js` turns `<script>`, `<img src=x
onerror=…>`, `[a](javascript:…)` and an image link into inert text, and
renders lists, code and http(s) links. Browser (scripted adapter): the
Assistant button is absent when `config.json` enables no connector;
consent shows once per connector and Cancel sends nothing; a remembered
key survives a reload, an unremembered one does not, Forget removes it;
streamed text appears as it comes; a tool chip expands to its result;
"Changed by … · Undo" undoes; "Apply to chart" sets the source with
origin `user`; Escape stops a turn; on an emulated phone the panel is a
bottom sheet and the chart stays visible; an axe-core scan of the open
panel has no serious violations; no request leaves the local server
before the user sends a message.

## 6. Claude connector

**Goal:** a user with an Anthropic API key edits charts with Claude in the
app.

**Deliverables:** `web/app/vendor/anthropic-sdk/` (pinned, licence,
`web/app/vendor/update.sh`) or the `fetch` client if step 1 said so;
`assistant/connectors/claude.js` (DESIGN.md section 6.2: models from
`config.json`, streaming, tools with `strict`, `effort`, caching
breakpoints, fallbacks, context editing, images in tool results, typed
errors); `web/app/agents.json`; a test-only `baseURL` override read from
`config.json` in test builds only.

**Tests** (browser, a local stub of the Messages API replaying recorded
streams): a turn with a tool call, its result (an image included) sent
back in the next request's `tool_result`, then text; the request carries
the browser-access header, the model chosen, `effort`, `strict` tools and
the `cache_control` breakpoints, and the second step's prefix is the first
step's byte for byte; a `refusal` stop runs no tool and shows the
message; 401 opens the key field; 429 shows Retry and Retry resends the
same request; the SDK files are fetched only on the first send. By hand:
a real key on the deployed site (recorded in the Result): create a
flowchart in View mode, switch to Edit, change a node's label, undo the
turn.

## 7. Gemini connector

**Goal:** a user with a Gemini API key does the same with Gemini.

**Deliverables:** `web/app/vendor/google-genai/` or `fetch`;
`assistant/connectors/gemini.js` (DESIGN.md section 6.3);
`toGeminiSchema()`.

**Tests:** Node: `toGeminiSchema()` over every `agentTools()` schema
gives only supported keywords and keeps `required` and `enum`. Browser
(stub API): a function call turn, its response (with the image, in the
form step 1 found) and the final text; key and quota errors as for
Claude. By hand: a real key on the deployed site.

## 8. WebMCP

**Goal:** a browser agent that supports WebMCP uses the app's tools on the
open chart.

**Deliverables:** `assistant/webmcp.js` (DESIGN.md section 7.1, in the
API shape step 1 recorded), registration on the document screen,
removal on the start screen, the settings switch, the activity list
entries.

**Tests** (browser, a stubbed `navigator.modelContext` recording
registrations): tools are registered when a document opens and removed
when it closes; a stubbed agent call to `editor_set_fields` changes the
chart with origin `agent:webmcp` and shows in the activity list with Undo;
an `external` call waits for the dialog; the switch off unregisters; with
no `navigator.modelContext`, nothing happens. By hand: a Chrome that has
WebMCP, if step 1 found one.

## 9. Native transport: loopback TCP and WebSocket

**Goal:** `nymph` can serve a WebSocket on 127.0.0.1 on every platform.

**Deliverables:** `src/ipc/loopback.hpp/.cpp` (listener on 127.0.0.1 port
0, accept, read, write, close, `#if defined(_WIN32)` guards as
`local_socket`); `src/mcp/sha1.*`; `src/mcp/websocket.hpp/.cpp` (DESIGN.md
section 7.2: upgrade with `Host` and `Origin` callbacks, frames, 16 MB
limit, ping, close); built into `nymph_mcp` only.

**Tests** (`test_websocket`): SHA-1 test vectors; the RFC 6455 example
key gives the example accept value; a client (in the test) sends masked,
fragmented and control frames and gets the messages back in order; an
unmasked client frame, an oversized message, a bad upgrade and a wrong
`Host` or `Origin` are refused and closed; close is answered; all
threads join on destruction. CI builds and runs it on Linux, macOS, MSVC
and MSYS2.

## 10. `nymph mcp` web bridge

**Goal:** an agent that runs `nymph mcp` pairs with the open web app and
uses its tools.

**Deliverables:** `src/mcp/web_bridge.hpp/.cpp` (pairing codes and
resume tokens, allowed origins with `NYMPH_WEB_APP_ORIGINS`, page links,
requests with a 5-minute wait); `web_app_pair` in `tools.cpp`;
`editor_bridge` listing paired pages (`web:<name>`, focus); the page's
`document_*` / `app_*` tools added to `nymph mcp`'s list while paired,
with `tools.listChanged: true` and `notifications/tools/list_changed`;
`json_rpc` writer mutex; the client name as `_meta["nymph/origin"]`;
the server instructions and `claude/skill/nymph/SKILL.md` mention
`web_app_pair`. If step 1 found a client that ignores `list_changed`,
also a generic `web_app_call {name, arguments}` tool, per the Result.

**Tests** (`test_web_bridge`, a C++ test page client over the WebSocket):
`web_app_pair` returns a code and a link with the port; the page pairs
and receives a resume token; a second use of the code, a wrong code, and
three wrong codes (listener closed) are refused; a reconnect with the
resume token works without a code; `tools/list` on the client gains the
page's `document_set_source` after pairing and a `list_changed`
notification is written to the client's stream; a client call of
`document_set_source` reaches the page with `_meta["nymph/origin"]` and
its result comes back; `editor_set_fields` with no `editor` goes to the
page focused last, `editor: "web:flow.yaml"` to that page, a native editor
still works beside it; a page that does not answer fails the call after
the wait (shortened by a test option); a closed page fails waiting calls
and drops its tools; without `web_app_pair` the session's tool list and
behaviour equal today's (existing `test_mcp*` tests pass).

## 11. Page side of the bridge

**Goal:** the user connects Claude Code, Cursor or Gemini CLI to the open
app with a code or a link.

**Deliverables:** `assistant/bridge.js` (DESIGN.md section 7.2:
Connect an agent dialog, `#pair=` route, the page as MCP server over the
WebSocket, `callAgent(…, "mcp:<client>")`, resume through
`sessionStorage`, `nymph/focus`, disconnect, the browser's refusal
explained); CSP `connect-src` loopback WebSockets; `docs/web-app.md`,
`docs/claude.md`, `docs/cursor.md`, new `docs/gemini-cli.md` (pairing,
per client), `docs/agent-guide.md`.

**Tests:** browser, with a native `nymph` from the build when the web job
has one (else skipped with 77): a fake MCP client drives `nymph mcp` on
stdin, calls `web_app_pair`; the page opens `#pair=<code>`; the client's
`editor_set_fields` (after `app_set_mode` edit) changes the title on the
page, the activity list shows `mcp:test-client` with Undo; a reload of the
tab reconnects by resume token; a page served from another origin is
refused; a wrong code shows the dialog's error. By hand: Claude Code and
Cursor pairing on the deployed site in Chrome and Firefox (Safari's result
from step 1 noted in the docs).

## 12. Documentation and status

**Goal:** users and the owner can find, use and configure the connectors.

**Deliverables:** `README.md` (one line on the assistant and pairing);
`docs/web-app.md` (assistant, keys and privacy, connectors, owner's
`NYMPH_AGENT_CONNECTORS`); `CHANGELOG.md`; `spec/INDEX.md` status.

**Tests:** the docs' links resolve; a deploy from `main` serves the
Assistant button and `config.json` with the `agents` section.

## Completion Criteria

- Every test above passes in CI (native on Linux, macOS, MSVC and MSYS2;
  web tests in the web job and `pages.yml`); the viewer and editor modules
  and their APIs unchanged and within their size budgets; `nymph mcp`
  without pairing unchanged.
- On the deployed site, by hand: with a real Claude key and a real Gemini
  key, create a chart in View mode, edit it in Edit mode, undo a turn,
  have a download confirmed; on a phone, edit a chart through the
  assistant in View mode; Claude Code and Cursor paired through `nymph
  mcp` change the open chart in Chrome and Firefox; WebMCP where a browser
  offers it.
- Every agent change is attributed, undoable per turn or per write, and
  every `external` action from an agent was confirmed; no request to a
  provider or a loopback socket happens for a user who never opens the
  assistant or pairs.
- `spec/INDEX.md`, `README.md`, `docs/web-app.md`, the client docs and
  `CHANGELOG.md` describe what was built.
