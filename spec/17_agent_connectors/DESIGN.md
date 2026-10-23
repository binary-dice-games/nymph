# nymph — Agent Connectors: Design

**Status: designed, not built.** [PLAN.md](PLAN.md) is the implementation
plan; its step 1 checks the outside facts this design depends on (marked
*unverified* below) before anything is built. This document describes only
what this project adds or changes. It builds on the web app,
[15_web_app](../15_web_app/DESIGN.md) (*app DESIGN*), whose section 11
prepared for it, on the editor tools and the editor bridge of
[14_editor_workspace](../14_editor_workspace/DESIGN.md) (*workspace
DESIGN*), and on `nymph mcp` from
[06_claude_integration](../06_claude_integration/DESIGN.md) (*MCP DESIGN*).

## 1. Purpose / Scope

The web app opens, views and edits nymph charts. This project lets an AI
agent help the user do it, in the app, on the chart the user has open:
"add a node for billing and connect it to orders", "make a Gantt chart
from this table", "why does this not load?". The user sees every change
the agent makes, can undo it, and is asked before anything leaves the tab.

Two kinds of connector (app DESIGN section 11.1):

| Kind | What it is | Built here |
|---|---|---|
| **In-app** | The app calls a model API from the page with the user's key, gives the model the command registry as tools, and runs its tool calls. A chat panel in the dock. | **Claude** (Messages API), **Gemini** (Gemini API) |
| **External** | An agent the user already runs reaches the open app through MCP. | **WebMCP** (browser agents, where the browser has it); **`nymph mcp` bridge** (Claude Code, Claude Desktop, Cursor, Gemini CLI, any MCP client that runs `nymph mcp`) |

Cursor has no public model API, so it is external only (through `nymph
mcp`, 12_cursor_integration). Claude in Chrome and Gemini in Chrome are
external through WebMCP when the browser exposes the page's tools to them
(*unverified*; without it they see only an unreadable canvas).

In scope:

- The **assistant core** (`web/app/app/assistant/`): a provider-neutral
  session that runs a conversation's tool loop against the command
  registry, with attribution, undo per turn, Stop, and the confirmation
  rule for `external` commands.
- The **chat panel** in the document screen's dock (DOM), on desktop and,
  as a bottom sheet, on phones.
- **Model adapters** for Claude and Gemini: keys, model choice,
  streaming, tool calls, images, errors.
- **WebMCP registration** of the agent tool list.
- The **`nymph mcp` web bridge**: a loopback WebSocket opened by `nymph
  mcp` on request, pairing with a code, and forwarding of the app's tools
  (`src/mcp/web_bridge.*`, `src/ipc/loopback.*`, `src/mcp/websocket.*`).
- **Registry changes** the connectors need (section 4): which commands
  agents see, a stable tool list, two new read commands, origin rules.
- One **`editor_model`** change: a change by another origin closes an open
  undo group (section 4.4).
- The assistant's **instructions** text and the documentation.

Not in scope: a hosted model proxy or any server (keys stay with the user,
app DESIGN goal 4); OAuth sign-in to a model provider (none is offered to
third-party web apps today; section 10); storing conversations (a chat
lives as long as its document is open); agents in `nymph view --edit`'s
own window (it is already reached through `nymph mcp`, workspace DESIGN);
the viewer module, the editor module and their JavaScript APIs, which do
not change; Firefox's and Safari's own assistants; the Chrome extension
and the MCP App.

Files owned or changed:

| Path | What |
|---|---|
| `web/app/app/assistant/` | new: `session.js`, `turns.js` (undo per turn), `chat.js` (panel), `markdown.js` (safe renderer), `settings.js` (keys, consent), `connectors/claude.js`, `connectors/gemini.js`, `webmcp.js`, `bridge.js` (page side of the `nymph mcp` bridge), `instructions.md` |
| `web/app/vendor/` | new: the vendored provider SDKs (section 6.4), with their licences |
| `web/app/app/commands.js` | `agent` field, `agentTools()`, origin rules, `unavailable` guidance |
| `web/app/app/main.js` | `agent: false` on the shell-only commands, `app_read_guide`, `app_example_source`, the dock wiring |
| `web/app/app/device.js` | `assistant` capability |
| `web/app/editor-tools.json` | new, generated and committed: the editor tool definitions, so agents see them before the editor module loads |
| `web/app/index.html`, `app.css` | dock content, `ws://127.0.0.1:*` in `connect-src` |
| `cmake/NymphSite.cmake` | `config.json` `agents` section from cache variables; copies the instructions and vendor files |
| `src/edit/editor_model.*` | a commit from another origin ends the open group |
| `src/ipc/loopback.*` | new: TCP listener and stream on 127.0.0.1 |
| `src/mcp/websocket.*`, `src/mcp/sha1.*` | new: the server side of RFC 6455, what the bridge needs and no more |
| `src/mcp/web_bridge.*` | new: pairing, page links, forwarding |
| `src/mcp/editor_bridge.*`, `mcp_session.*`, `json_rpc.*`, `tools.cpp` | pages listed as editors; server-to-client notifications; `web_app_pair` |
| `docs/web-app.md`, `docs/agent-guide.md`, `docs/claude.md`, `docs/cursor.md`, `docs/gemini-cli.md` (new), `claude/skill/nymph/SKILL.md` | using the assistant, pairing an external agent |

## 2. Design Goals

1. **The user stays in charge.** Every change an agent makes is shown,
   named with who made it, and undoable as one step per turn; anything that
   leaves the tab waits for the user's Allow; the user's own typing always
   wins over an agent's write.
2. **One tool surface for every agent.** In-app connectors, WebMCP and the
   `nymph mcp` bridge all see the same tools (the registry's agent list),
   validated, attributed and confirmed by the registry, never by a
   connector (app DESIGN section 8).
3. **No server, no stored secrets by default.** The page talks to the
   model provider directly with the user's key; the key lives in memory
   unless the user asks to remember it on this device. Nothing about
   agents loads until the user opens the assistant.
4. **Works without the editor.** An in-app agent creates and edits charts
   in View mode and on phones through the `document` commands (app DESIGN
   section 11.2, item 2); in Edit mode it also gets the fine-grained
   editor tools.
5. **Local means local.** The `nymph mcp` bridge listens on 127.0.0.1 only,
   only after an agent asks for it, accepts only the app's origin, and
   only a page that typed the one-time code.
6. **Nothing existing changes for people who do not use it.** No module
   grows, no request is made, no control appears beyond the Assistant
   button; `nymph mcp` without pairing behaves exactly as before.

## 3. How It Fits

```
 the app's page
 ┌─────────────────────────────────────────────────────────────────────┐
 │ chat.js ─► session.js ─► connectors/claude.js ─────────────────────────► Anthropic API
 │            (turns, undo,  connectors/gemini.js ─────────────────────────► Gemini API
 │             Stop)  │                                                   │
 │                    ▼                                                   │
 │ webmcp.js ───► commands.callAgent(name, args, connector) ◄── bridge.js ◄─── ws://127.0.0.1 ── nymph mcp ◄─stdio── Claude Code,
 │   ▲            │ checks args, confirms external,           (MCP server)│    (web_bridge)               Cursor, Gemini CLI,
 │   │            │ snapshots, attribution                                │                               Claude Desktop
 │   │            ▼                                                       │
 │   │      viewer (View)  or  editor tool() (Edit)                       │
 └───┼─────────────────────────────────────────────────────────────────────┘
     └── browser agent (WebMCP)
```

Every route ends in `commands.callAgent(name, args, connector)`, which
gives the call an `agent:` origin. The registry checks availability and arguments, asks the user for
`external` commands, and runs the command in whichever module is loaded.
The connectors differ only in how tool calls reach the page and how
results go back.

## 4. The Agent Tool List

### 4.1 Which commands agents see

A command gains `agent: true | false` (default `true`). Shell-only
commands that open a picker or a dialog for the user are `false`, since an
agent cannot use them and the browser would refuse most of them without a
user gesture: `app_open_file`, `app_open_url_dialog`, `app_open_drive`,
`app_new_dialog`, `app_save_as`, `app_choose_theme`, `app_about`,
`app_shortcuts`. Everything else is listed, `external` ones included (they
are confirmed, section 4.3).

Two new `app` read commands give an agent the format without growing
either module:

| Command | Arguments | Result |
|---|---|---|
| `app_read_guide` | `name`: `agent-guide`, `format`, `elements`, or `elements/<Type>` | the text of that file in the site's `agent/` folder (app DESIGN section 11.2, item 7) |
| `app_example_source` | `name` | an example's YAML, without opening it |

### 4.2 A stable list

`commands.agentTools()` returns the agent list for this device, **the same
for the page's whole life**, whatever the mode: every agent command, plus
the editor tools on a device with `directEditing`, even while the editor
is not loaded. Their definitions come from `web/app/editor-tools.json`,
generated from the native `editor_tool_definitions()` and committed; a test
fails when it differs from the editor module's `toolDefinitions()`. A call
to a command that is not available now fails with guidance instead of
"unknown": an `editor_*` tool in View mode says "the editor is not open:
call app_set_mode with mode edit, or use document_set_source"; a `document`
command with no document says to open or create one.

Why stable: a model provider caches the tool list with the system prompt,
and a list that changes mid-conversation invalidates that cache and, for
current Claude models, the thinking kept from earlier turns. A stable list
also tells the model up front what Edit mode would give it.

`commands.list()` (what is available now) stays as it is for the shell and
the tests.

### 4.3 Origins

- An origin is `user` or `agent:<connector>` with `<connector>` one of
  `claude`, `gemini`, `webmcp`, or `mcp:<client>` (the name `nymph mcp`'s
  client gave in `initialize`, e.g. `mcp:claude-code`, reduced to
  `[a-z0-9-]`, at most 32 characters).
- The registry refuses `user` from any route but the shell: connectors
  call `commands.callAgent(name, args, connector)`, which builds the
  origin; only the shell's controls call `commands.call(…, "user")`. No
  external input can choose its origin.
- `external` commands from an agent origin wait for the existing
  confirmation dialog (app DESIGN section 8). Its Allow click is the user
  gesture that downloads and clipboard writes need (*unverified* for the
  clipboard after an `await`; plan step 1).
- A per-connector "Allow for this session" checkbox in the dialog skips the
  dialog for the same command from the same connector until the document
  closes. Never remembered across sessions.

### 4.4 Undo groups

The in-app session opens a group (`beginGroup(label, origin)`, app DESIGN
section 5.2) before the first write of a turn and closes it when the turn
ends, so a turn is one undo step in Edit mode. Today a change made while a
group is open joins it, whoever makes it; a user's edit during an agent's
turn would then be undone with the agent's. **Change:** `editor_model`'s
commit ends the open group first when the change's origin differs from the
group's. The session reopens a group (same label) before its next write if
`in_group()` is false; the turn may then be two undo entries, which is
correct, since the user's change sits between them.

External connectors (WebMCP, the bridge) have no turns: each of their
writes is its own undo step, labelled `<tool> (<connector>)`, as `nymph
mcp`'s edits are today.

### 4.5 Undo per turn and in View mode

View mode has no undo history (the viewer holds a source, app DESIGN
section 3.3). `turns.js` keeps, for every turn or external write that
changed the document, the source before and after it. The chat shows
**Undo** on each such turn (and the dock's activity list on each external
write):

- Edit mode, and the turn's group is still the editor's last undo entry
  (`last_label()` / `last_origin()`): `editor.undo()`.
- Otherwise, when the current source equals the turn's "after": the
  "before" source through `document_set_source` (origin `user`), one undo
  step in Edit mode.
- Otherwise (the user or another agent changed it since): "Restore the
  chart from before this turn? Later changes are lost." with Cancel.

Snapshots are kept for the open document only, at most 50, in memory.

## 5. The Assistant Core (`web/app/app/assistant/`)

### 5.1 Session

```js
class AssistantSession {
  constructor({ commands, adapter, instructions, device, onEvent })
  send(text: string, attachments?: { image?: Uint8Array }[]): Promise<void>  // one turn
  stop(): void                     // aborts the request; running tool calls finish
  reset(): void                    // new conversation
  get busy(): boolean
}
// onEvent: {type: "text", delta} | {type: "tool", name, args, result, label}
//          | {type: "turn-end", changed, usage} | {type: "error", message, retry?}
```

A turn: the user's text goes to the adapter with the instructions and
`agentTools()`; each tool call the model returns runs through
`commands.callAgent` (in the order given; calls of one response run one
after another, since they act on one document); the results go back; the
loop ends when the model answers without tool calls, on Stop, on an error,
or after **25** model requests in one turn (then "the assistant stopped
after 25 steps", Continue). Text streams into the chat as it comes.

- **Context.** The first request of a conversation carries, after the
  user's text, a short state note: `document_state`'s result and the
  device (`directEditing`). The model reads the source and the image itself
  through tools; the note is not repeated each turn (it would change the
  cached prefix), and later turns rely on `document_state`.
- **History is append-only.** Messages are never edited or removed once
  sent: current Claude models keep their reasoning only over an unchanged
  history. Long conversations rely on the provider's own context
  management (section 6.2).
- **Busy.** While a turn runs the input is disabled, Stop is shown, and
  the chat says which tool runs. The user can still edit the chart; an
  agent write that then fails on `expected_version` or on the source pane
  being ahead goes back to the model as the tool's error, which tells it
  to read the state again (workspace DESIGN section 6).
- **Mode switch.** `app_set_mode` from an agent works like the user's.
  The editor's undo does not survive a switch, so the turn's snapshot is
  what Undo uses afterwards.
- **Document closed or replaced** (`app_new_document`, open from URL):
  the conversation continues; the snapshots of the old document are
  dropped and the state note of the next request says a new document is
  open.

### 5.2 Adapter interface

```js
interface ModelAdapter {
  id: "claude" | "gemini";  label: string;
  models(): Promise<{ id, label }[]>;  defaultModel: string;
  configured(): boolean;               // a key is set
  turn({ system, tools, history, signal }): AsyncIterable<
    { type: "text", delta } | { type: "tool_calls", calls: [{ id, name, args }] }
    | { type: "end", stop: "done" | "max_tokens" | "refusal", usage }>;
  addToolResults(history, calls, results): void;  // provider's own message shape
  addUserMessage(history, text, attachments): void;
}
```

The history is the provider's own message list, held by the session but
written only through the adapter, so provider fields (thinking blocks,
signatures) survive unchanged. Tool results are MCP `CallToolResult`s; the
adapter converts text, structured content and images.

### 5.3 Instructions (`assistant/instructions.md`)

About 80 lines, written for an agent with the app's tools, not copied from
the Claude skill (that text is about `render_chart` and files, as the
Gemini primer found, 10_gemini_extension section 4.5):

- where it is: the user's open chart in the nymph web app; View and Edit
  modes, what each allows; mobile has no Edit;
- the workflow: `document_state` first; in Edit mode prefer the `editor_*`
  tools (one field or element at a time, with `expected_version`); in View
  mode `document_check` then `document_set_source`; look at the result
  with `document_image` after a change; read `app_read_guide` before
  writing an element type it has not used in this conversation;
- the source format essentials (skeleton, `$column`, the three rules behind
  most errors), from the skill text;
- conduct: make the change asked, nothing else; say in one or two
  sentences what changed; never invent data; ask before replacing a
  chart wholesale; `external` actions are confirmed by the user, so ask for
  them only when the user wants them.

A test fails when the text names a command that is not in
`agentTools()`, or when it is over 8,000 characters.

## 6. In-App Connectors

### 6.1 Keys and consent (`assistant/settings.js`)

- The user picks a connector in the chat panel's header and pastes an API
  key; a link opens the provider's key page. The key is kept **in memory**;
  "Remember on this device" stores it in IndexedDB (`nymph-app`,
  `settings`), with a line saying anyone using this browser profile can
  use it. "Forget key" removes it.
- The first use of a connector in this browser shows a consent dialog:
  "Your messages, the chart's source and pictures of it are sent to
  Anthropic / Google, under your API key and their terms." Accept is stored
  per connector; there is no default connector.
- A connector that `config.json` disables is not offered.

### 6.2 Claude (`connectors/claude.js`)

- The Messages API with the official TypeScript SDK (`@anthropic-ai/sdk`)
  in its browser build, vendored (section 6.4), constructed with the
  user's key and `dangerouslyAllowBrowser: true` (which sends the
  `anthropic-dangerous-direct-browser-access` header that lets the API
  answer a page's CORS request). Streaming (`messages.stream`,
  `finalMessage()` for each step).
- Models: Claude Opus 5.5 (`claude-opus-5-5`, default), Claude Sonnet 5.5
  (`claude-sonnet-5-5`), Claude Haiku 5.5 (`claude-haiku-5-5`); the list
  lives in `config.json` so the site can update it without code.
- Request: `max_tokens` 64000; adaptive thinking (the default on these
  models) with `output_config.effort` set explicitly (`medium`; a setting
  in the panel offers `low` and `high`); tools from `agentTools()` with
  `strict: true` where the schema allows it; the instructions as `system`;
  `cache_control` breakpoints on the system prompt and on the last message,
  so each step re-reads the conversation from cache.
- **Refusals.** Server-side fallbacks on (`fallbacks: "default"`, beta
  `server-side-fallback-2026-07-01`); a `refusal` stop that remains is
  shown as "Claude declined this request", and the turn's tool calls are
  not run.
- **Long chats.** Server-side clearing of old tool results (context
  editing, `clear_tool_uses`) keeps old images and sources from filling the
  window, without editing the history on the client.
- Images: `document_image`'s PNG goes back as an image block inside the
  `tool_result`.
- Errors by the SDK's typed errors: 401 "the key was refused" (opens the
  key field), 429 and overloaded "busy, try again" with Retry, network
  "cannot reach Anthropic". No automatic retry beyond the SDK's own.

### 6.3 Gemini (`connectors/gemini.js`)

- The Gemini API (`generativelanguage.googleapis.com`) with Google's
  official JavaScript SDK (`@google/genai`), vendored likewise, with the
  user's key; streamed generation with function calling.
- Models from `config.json` (Gemini's current Pro and Flash models at
  release); default the Pro model.
- Tool schemas: Gemini accepts a subset of JSON Schema; `toGeminiSchema()`
  maps ours (drops `additionalProperties`, keeps `type`, `properties`,
  `required`, `enum`, `items`, `description`); a Node test runs it over
  every agent tool.
- Images in function responses (*unverified* for the current models):
  as a part of the function response where the API takes it, else as an
  inline image part in the same user turn after the responses.
- Errors: 400 with a key error, 403, 429, network, as for Claude.

### 6.4 Vendored SDKs

Each SDK's published browser-usable ES module build is copied into
`web/app/vendor/<name>/` at a pinned version, with its licence, by a
script run by hand (`web/app/vendor/update.sh <name> <version>`), and
committed. No npm in the site build (app DESIGN section 8), no CDN (the
CSP allows `'self'` scripts only). They are imported dynamically when a
connector is first used, so they are never fetched by a user who does not
use the assistant; the service worker caches them on that fetch, not at
install. If an SDK's build cannot be imported as plain ES modules without
a bundler (plan step 1), that connector calls the REST API with `fetch`
instead, behind the same adapter, and this section is updated.

### 6.5 The chat panel (`assistant/chat.js`)

- An **Assistant** button in the top bar (a capability, section 8) opens
  the dock with the panel: header (connector and model, settings, New
  chat, close), the conversation, an input with Send and Stop. On a phone
  the dock is a bottom sheet over the viewer (half height, draggable to
  full).
- Messages: the user's text; the model's text rendered by
  `markdown.js`; each tool call as a one-line chip (`Set fields: title`,
  its result on expand); a turn that changed the chart ends with
  "Changed by Claude · Undo".
- `markdown.js` renders a small subset (paragraphs, emphasis, lists,
  inline code, fenced code, links) **as DOM nodes with `textContent`**,
  never `innerHTML`. Links are http(s) only, shown with their full URL,
  opened with `rel="noopener noreferrer"`. **No images** are ever loaded
  from model output: an image URL could carry the chart's data to a third
  party (prompt injection from an opened file, section 9).
- A fenced `yaml` / `nymph` block in the model's text gets "Apply to
  chart", which calls `document_set_source` with origin `user`.
- Keyboard: `Ctrl+Shift+A` toggles the panel; Enter sends, Shift+Enter is
  a new line, Escape stops a running turn. The panel is a labelled
  `complementary` region; new text is announced politely.

## 7. External Connectors

### 7.1 WebMCP (`assistant/webmcp.js`)

Where `navigator.modelContext` exists (*unverified*: the API is a draft
of the W3C Web Machine Learning group; plan step 1 records the shape
Chrome ships), the app registers `agentTools()` as the page's tools when a
document screen is shown, each `execute` calling `commands.callAgent(name,
args, "webmcp")`, and unregisters them on the start screen. It is on by
default: a browser agent the user invoked can already read and click the
page, the tools only make it able to do so correctly, and `external`
commands are still confirmed. The dock's activity list shows each call.
One switch in the assistant settings turns it off for this browser.

### 7.2 The `nymph mcp` bridge

**From the agent's side.** `nymph mcp` gains a tool, `web_app_pair`
(always listed). The agent calls it when the user wants it to work on the
chart open in the web app. `nymph mcp` then opens a listener on
`127.0.0.1` (a free port) if it has none, and returns a **pairing code**
and a **pairing link**:

```
code:  48213-K7QM-2XPD          (port, then 40 random bits in Crockford base32)
link:  https://binary-dice-games.github.io/nymph/#pair=48213-K7QM-2XPD
```

The agent shows both to the user: the code is typed into the open app's
Assistant ▸ Connect an agent dialog; the link opens the app in a new tab,
paired, for a user with nothing open.

**From the page's side** (`assistant/bridge.js`). The page opens
`ws://127.0.0.1:<port>/nymph`, sends `{"pair": "<code>"}` first, and then
serves MCP over the socket as a server: `initialize`, `tools/list`
(`agentTools()`), `tools/call` (through `commands.callAgent(…, "mcp:<client>")`),
and it sends `notifications/tools/list_changed` when its list changes
(never, today, by section 4.2; kept for later) and `nymph/focus` when its
tab gains focus. On success `nymph mcp` answers with a **resume token**
(128 random bits) that the page keeps in `sessionStorage`, so a reload of
the same tab reconnects without a new code while that `nymph mcp` runs.

**In `nymph mcp`** (`src/mcp/web_bridge.*`):

- The listener accepts only: a `GET` upgrade to `/nymph` with `Host`
  `127.0.0.1:<port>` or `localhost:<port>` (DNS rebinding), and an
  `Origin` in the allowed list: the site's origin
  (`https://binary-dice-games.github.io`), plus those in
  `NYMPH_WEB_APP_ORIGINS` (self-hosted copies, `http://localhost:<port>`
  for a local `nymph-site` preview). Anything else is closed without a
  body.
- A code is valid once, for 5 minutes, and three wrong codes in a row
  close the listener (a new `web_app_pair` opens another). Comparison is
  constant-time. The listener stays open, without a code, only for
  resume tokens; it closes when `nymph mcp` exits.
- A paired page is listed by `editor_bridge` as one more editor, beside
  native `nymph view --edit` windows: its `editor` name is `web:<file
  name>`, its focus time from `nymph/focus`. The editor tools route to it
  as to a native editor (the same tools, workspace DESIGN section 6).
- Its other tools (`document_*`, `app_*`) are added to `nymph mcp`'s tool
  list while a page is paired, each with the optional `editor` argument,
  and `nymph mcp` sends `notifications/tools/list_changed` to its client
  when that set changes (declaring `tools.listChanged: true`). The app's
  names do not collide with `nymph mcp`'s chart tools (`render_chart`,
  `check_chart`, ...).
- A call to the page waits up to 5 minutes (a confirmation dialog may be
  open), then fails with "the web app did not answer". A closed socket
  fails waiting calls with "the web app was closed".
- The client name from `initialize` (`clientInfo.name`) goes with each
  call as `_meta["nymph/origin"]`; the page turns it into
  `mcp:<client>` (section 4.3).

**Transport code.** `src/ipc/loopback.*`: a TCP listener and stream bound
to 127.0.0.1, beside `local_socket` and with the same `#if
defined(_WIN32)` guards. `src/mcp/websocket.*`: the server side of RFC
6455 that the bridge needs: the upgrade (`Sec-WebSocket-Accept`, hence
`sha1.*`), masked client frames, unmasked server frames, text messages
up to 16 MB (fragmented ones reassembled), ping / pong, close. No
extensions, no binary messages, no TLS (loopback only). One thread per
page reads messages; calls from the session thread wait on a promise per
request id, as `editor_bridge` does. Writes to `nymph mcp`'s standard
output now come from two threads (the session and a page's reader, for
`list_changed`), so `json_rpc`'s writer takes a mutex.

**Browser requirements.** A public site reaching `ws://127.0.0.1` needs,
in Chrome, the user's Local Network Access permission, asked at the first
connection (*unverified* for WebSockets in the shipping version); Firefox
treats loopback as secure (*unverified*); Safari may block it as mixed
content (*unverified*). Plan step 1 records each; where it cannot work,
the Connect dialog says so and points to the in-app connectors.

## 8. Capabilities and Configuration

- `device.assistant` (app DESIGN section 3.10) becomes `true` on every
  device; the Assistant button shows when at least one connector is
  enabled in `config.json`. On a phone only the in-app connectors and
  WebMCP are offered (no `nymph mcp` there), and the tool list has no
  editor tools (section 4.2).
- `config.json`'s `agents` section, from CMake cache variables
  (`NYMPH_AGENT_CONNECTORS`, default `claude;gemini;webmcp;bridge`) and a
  committed `web/app/agents.json` for models:

```json
"agents": {
  "claude": { "models": [{"id": "claude-opus-5-5", "label": "Claude Opus 5.5"}, ...], "default": "claude-opus-5-5" },
  "gemini": { "models": [...], "default": "..." },
  "webmcp": {}, "bridge": { "pairUrl": "https://binary-dice-games.github.io/nymph/" }
}
```

- The CSP gains `ws://127.0.0.1:* ws://localhost:*` in `connect-src`;
  `https:` already covers both providers' APIs.

## 9. Design Decisions

- **Keys in the page, not a proxy.** A proxy would see every chart and
  every key and needs a server (app DESIGN goals 4 and 5). Cost: the key is
  readable by any script on the page; the page runs only its own scripts
  (CSP `'self'`, Google's only during a Drive action) and never inserts
  file or model content as HTML. Remembering the key is opt-in.
- **One registry, every route.** Connectors only translate. A second path
  (a connector calling the viewer or editor directly) would skip the
  confirmation, the attribution or the snapshot, which goal 1 forbids.
- **A stable tool list** over "what is available now": caching and the
  kept reasoning of current models both need an unchanged prefix, and the
  model plans better knowing the editor tools exist. Rejected: listing
  only available tools and restarting the conversation on a mode switch.
- **The editor tool definitions committed as JSON**, not read from the
  editor module, so View mode and phones (where that module is never
  fetched, app DESIGN section 3.10) list them without a 1 MB download. A
  staleness test keeps them true.
- **Undo by snapshot in View mode**, not an undo history in the viewer. The
  viewer holds one source; snapshots of a few chart sources per
  conversation cost nothing and also cover a turn that switched modes.
- **A change by someone else ends an agent's undo group.** Otherwise
  undoing the agent's turn also undoes the user's edit made during it.
- **The page as the MCP server, `nymph mcp` as the listener.** A page
  cannot accept connections; `nymph mcp` is already the process every
  external agent runs, so it is the natural client, and its existing
  editor bridge already routes editor tools to "the editor the user has
  open". Rejected: a separate `nymph serve` daemon (one more thing to
  start); a fixed port (collisions, and a page could probe it without a
  code); WebRTC (needs signalling).
- **A typed code, not a confirmation on the agent side.** The page must
  prove it is the page the user means; the agent side is a CLI or chat
  with no reliable way to show a prompt. The code also carries the port.
- **Pairing on request, listener closed by default.** Goal 5: no port is
  open for people who never pair.
- **Each external write is one undo step.** WebMCP and MCP have no turn
  boundary; guessing one by idle time would group unrelated edits.
- **WebMCP on by default.** It gives the browser's own agent no power the
  page's DOM does not already give it, and `external` stays confirmed.
- **No stored conversations.** Chats hold the user's data and the model's
  output; keeping them adds a privacy surface for little gain. Revisit
  with a "save chat" action (section 11).
- **Vendored official SDKs** over hand-written REST clients, for streaming,
  typed errors and tool-call parsing maintained by the providers; loaded
  only on use, so the app shell does not grow.

## 10. Constraints and Invariants

- No model API, socket or vendored script is requested until the user
  opens the assistant (or, for WebMCP, until a document screen shows on a
  browser that has `navigator.modelContext`, which makes no request).
- A connector never calls the viewer, the editor or `app` internals;
  only `commands.callAgent`. No connector can produce the origin `user`.
- `external` commands from an agent always pass the confirmation (or the
  session's "Allow for this session" for that connector and command).
- Model text and tool arguments are never inserted as HTML; no remote
  image from model output is ever loaded.
- Opened files are untrusted (they may contain text written to steer the
  model): their contents reach the model only as tool results, never in
  the system prompt; nothing a tool returns can widen what the model may
  do without the user.
- History sent to a provider is append-only within a conversation.
- `nymph mcp`: the listener binds 127.0.0.1 only, checks `Host` and
  `Origin`, accepts a code once, closes after three wrong codes; only the
  session thread touches the tool registry; reader threads only parse,
  fulfil promises and queue notifications; shutdown closes sockets and
  joins every thread. Without `web_app_pair`, no socket, no thread, no
  change to the tool list.
- The web build compiles none of `src/ipc/` and `src/mcp/web_bridge.*`.
- Tests use port 0 and test-named files (CLAUDE.md, Tests); no test calls
  a real model API.
- Code style: JavaScript as `web/app/` (ES modules, no framework, no build
  step); C++ as nymph.

## 11. Testing Strategy

1. **Node**: `markdown.js` (every input becomes text nodes; a `<script>`,
   an `<img>` and a `javascript:` link come out inert), `toGeminiSchema()`
   over every agent tool, the instructions test, `editor-tools.json`
   against the editor module's `toolDefinitions()`.
2. **Browser** (Playwright, built site): the session with a **scripted
   adapter** (a list of responses): a turn's writes are one undo step and
   the chat's Undo reverts them in Edit and in View; a user edit during a
   turn is not undone with it; Stop ends a turn; an `external` call waits
   for the dialog and Cancel goes back as the tool's error; an `editor_*`
   call in View mode returns the guidance; 25 steps stop the turn. The
   Claude and Gemini adapters against **local stub servers** that replay
   recorded streaming responses (tool calls, images, refusal, 401, 429);
   the consent dialog and key storage; the chat renders a hostile message
   inertly; WebMCP with a stubbed `navigator.modelContext`; on an emulated
   phone the panel is a sheet and lists no editor tools.
3. **Native** (GoogleTest): `sha1` vectors; WebSocket handshake and
   framing (masked, fragmented, oversized, close); the listener refuses a
   wrong `Host` or `Origin`, a reused or wrong code, and closes after three;
   resume tokens; `web_app_pair` and forwarding with a C++ test client
   acting as the page (tools list, a call, a timeout, a closed page,
   `list_changed` reaching the client); `editor_bridge` picks a page or a
   native editor by focus; `editor_model`: a commit with another origin
   ends the group.
4. **End to end** (browser, when a native `nymph` is available to the
   web job, else skipped with 77): `nymph mcp` with a fake client on
   stdin calls `web_app_pair`, the page types the code, the client calls
   `editor_set_fields` and the page's chart changes.
5. **By hand**, recorded in the plan: a real Claude key and a real Gemini
   key on the deployed site; Claude Code and Cursor pairing through `nymph
   mcp` in Chrome and Firefox; WebMCP in a Chrome that has it; a phone.

## 12. Integration Boundaries

Depends on: app DESIGN (registry, dock, documents, `device.js`, editor
module API, `agent/` folder), workspace DESIGN (editor tools, versions,
`editor_bridge`, discovery), MCP DESIGN (`nymph mcp`, session, skill
text), [12_cursor_integration](../12_cursor_integration/DESIGN.md)
(Cursor runs `nymph mcp`).

Changes others see: `nymph mcp` has `web_app_pair`, may list `document_*`
/ `app_*` tools and send `list_changed`; `editor_bridge` lists web pages;
`editor_model` ends a group on another origin's change; the registry has
`agent`, `agentTools()` and `callAgent`; the app's CSP allows loopback
WebSockets.

Risks, checked first (plan step 1): the SDKs as plain ES modules; the
Messages API and Gemini API from a page (CORS, streaming, images in tool
results); WebMCP's shape; loopback WebSockets from the public site in each
browser; whether Claude Code, Cursor and Gemini CLI follow
`tools/list_changed`.

## 13. Extension Points

- OAuth sign-in for a provider, if one is offered to third-party web
  apps (replaces the key field, same adapter).
- More in-app connectors (any model API with tool calls) behind the same
  adapter interface.
- Saving a chat with its document; a "save chat" action.
- Voice input on phones.
- `nymph view --edit`'s own window gaining the chat panel (ImGui or a
  webview), reusing the session's rules natively.
- `resources/subscribe` from the bridge, so an external agent is told
  when the user changes the chart instead of re-reading it.
