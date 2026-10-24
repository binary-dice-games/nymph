# nymph — Agent Connectors: Design

**Status: designed, not built.** [PLAN.md](PLAN.md) is the implementation
plan; its step 1 checks the outside facts this design depends on (marked
*unverified* below) before anything is built. This document describes only
what this project adds or changes. It builds on the web app,
[15_web_app](../15_web_app/DESIGN.md) (*app DESIGN*), whose section 11
prepared for it, on the editor tools and the editor bridge of
[14_editor_workspace](../14_editor_workspace/DESIGN.md) (*workspace
DESIGN*), and on `nymph mcp` and the MCP App from
[06_claude_integration](../06_claude_integration/DESIGN.md) (*MCP DESIGN*).

## 1. Purpose / Scope

The web app opens, views and edits nymph charts. This project lets the AI
agent **the user already runs** work on the chart open in the app: "add a
node for billing and connect it to orders", "make a Gantt chart from this
table", "why does this not load?". The agent brings its own model, login
and billing; the app only offers tools. The user sees every change the
agent makes, can undo it, and is asked before anything leaves the tab.

App DESIGN section 11.1 listed two kinds of connector: **in-app** (the app
calls a model API with the user's key) and **external** (the user's agent
reaches the app). This project builds **only the external kind** and
drops the in-app kind (section 9): no API key is ever given to nymph.

| Route | Reaches the app from | Agents |
|---|---|---|
| **`nymph mcp` bridge** | an MCP client that runs `nymph mcp` on the user's computer, paired with the open tab | Claude Code, Claude Desktop, Cursor (12_cursor_integration), Gemini CLI, any stdio MCP client |
| **WebMCP** | the browser's own agent, through the page's registered tools (*unverified*: a draft API, section 7.1) | Gemini in Chrome, Claude in Chrome, where the browser exposes page tools to them |
| **"Open in the web app"** from the MCP App | the chart shown in the agent's chat, opened in the app already paired | Claude Desktop, Claude Code, any MCP Apps host running `nymph mcp` |

In scope:

- **Registry changes** the connectors need (section 4): which commands
  agents see, a stable tool list, origin rules, change groups, two read
  commands, a workflow guide for agents.
- One **`editor_model`** change: a change by another origin closes an open
  undo group (section 4.4).
- The **Agents panel** in the document screen's dock: connections, the
  Connect dialog, and the activity list with Undo (section 5).
- **WebMCP registration** of the agent tool list.
- The **`nymph mcp` web bridge**: a loopback WebSocket opened by `nymph
  mcp` on request, pairing with a code, forwarding of the app's tools
  (`src/mcp/web_bridge.*`, `src/ipc/loopback.*`, `src/mcp/websocket.*`).
- **"Open in the web app"** in the MCP App, paired in one click.
- Documentation for each client.

Not in scope: calling any model API from the app, API keys, a chat inside
the app (dropped, section 9); a hosted, remote MCP server (would reach
claude.ai and phone chat apps, but needs a server; a separate project,
section 13); agents in `nymph view --edit`'s own window (already reached
through `nymph mcp`, workspace DESIGN); the viewer module, the editor
module and their JavaScript APIs, which do not change; the Chrome
extension.

Files owned or changed:

| Path | What |
|---|---|
| `web/app/app/agents/` | new: `panel.js` (dock panel, Connect dialog), `activity.js` (activity list, snapshots, Undo), `webmcp.js`, `bridge.js` (page side of the `nymph mcp` bridge) |
| `web/app/app/commands.js` | `agent` field, `agentTools()`, `callAgent()`, origin rules, change groups, guidance for unavailable commands |
| `web/app/app/main.js` | `agent: false` on shell-only commands, `app_read_guide`, `app_example_source`, `document_begin_change` / `document_end_change`, `#pair=` route, dock wiring |
| `web/app/app/device.js` | `agents` capability (replaces `assistant`) |
| `web/app/editor-tools.json` | new, generated and committed: the editor tool definitions, so agents see them before the editor module loads |
| `web/app/index.html`, `app.css` | dock content, `ws://127.0.0.1:* ws://localhost:*` in `connect-src` |
| `docs/agent-web-app.md` | new: the workflow guide agents read in the app (copied to `agent/web-app.md` by `nymph-site`) |
| `cmake/NymphSite.cmake` | `config.json` `agents` section from `NYMPH_AGENT_CONNECTORS` |
| `src/edit/editor_model.*` | a commit from another origin ends the open group |
| `src/ipc/loopback.*` | new: TCP listener and stream on 127.0.0.1 |
| `src/mcp/websocket.*`, `src/mcp/sha1.*` | new: the server side of RFC 6455, what the bridge needs and no more |
| `src/mcp/web_bridge.*` | new: pairing, page links, forwarding |
| `src/mcp/editor_bridge.*`, `mcp_session.*`, `json_rpc.*`, `tools.cpp` | pages listed as editors; server-to-client notifications; `web_app_pair` |
| `claude/app/` | "Open in the web app" in the viewer's menu |
| `claude/skill/nymph/SKILL.md`, `docs/agent-guide.md` | `web_app_pair` and when to use it |
| `docs/web-app.md`, `docs/claude.md`, `docs/cursor.md`, `docs/gemini-cli.md` (new) | connecting each agent |

## 2. Design Goals

1. **No keys, no model calls in the app.** The user's agent keeps its own
   login and billing; nymph never sees a credential and never pays for or
   chooses a model. There is nothing for the user to have to trust nymph
   with beyond the chart itself.
2. **The user stays in charge.** Every change an agent makes is listed,
   named with who made it, and undoable; anything that leaves the tab
   waits for the user's Allow; the user's own typing always wins over an
   agent's write.
3. **One tool surface for every agent.** WebMCP and the `nymph mcp` bridge
   see the same tools (the registry's agent list), validated, attributed
   and confirmed by the registry, never by a connector (app DESIGN
   section 8). An agent sees the same editor tools in the app, in `nymph
   view --edit` and through `nymph mcp`.
4. **Local means local.** The bridge listens on 127.0.0.1 only, only after
   an agent asks for it, accepts only the app's origin, and only a page
   that has the one-time code.
5. **Nothing changes for people who do not use it.** No module grows, no
   request is made, no port is opened; `nymph mcp` without pairing behaves
   exactly as before.

## 3. How It Fits

```
 the app's page
 ┌──────────────────────────────────────────────────────────────────┐
 │ webmcp.js ───► commands.callAgent(name, args, connector) ◄── bridge.js ◄── ws://127.0.0.1 ── nymph mcp ◄─stdio── Claude Code,
 │   ▲            │ checks args, confirms external,      (MCP server)│      (web_bridge)               Cursor, Gemini CLI,
 │   │            │ change groups, attribution                       │                                 Claude Desktop
 │   │            ▼                                                   │                                   │
 │   │      viewer (View)  or  editor tool() (Edit)                   │   MCP App (in the chat) ──────────┘
 │   │            │                                                   │     "Open in the web app": #y=…&pair=…
 │   │            ▼                                                   │
 │   │      activity.js ──► Agents panel (dock): connections, activity, Undo
 └───┼──────────────────────────────────────────────────────────────────┘
     └── browser agent (WebMCP)
```

Every route ends in `commands.callAgent(name, args, connector)`, which
gives the call an `agent:` origin. The registry checks availability and
arguments, asks the user for `external` commands, and runs the command in
whichever module is loaded. The connectors only carry tool calls in and
results out.

## 4. The Agent Tool List

### 4.1 Which commands agents see

A command gains `agent: true | false` (default `true`). Shell-only
commands that open a picker or a dialog for the user are `false`, since an
agent cannot use them and the browser would refuse most of them without a
user gesture: `app_open_file`, `app_open_url_dialog`, `app_open_drive`,
`app_new_dialog`, `app_save_as`, `app_choose_theme`, `app_about`,
`app_shortcuts`. Everything else is listed, `external` ones included (they
are confirmed, section 4.3).

New commands:

| Command | Effect | Arguments | Result |
|---|---|---|---|
| `app_read_guide` | read | `name`: `web-app`, `agent-guide`, `format`, `elements`, or `elements/<Type>` | the text of that file in the site's `agent/` folder (app DESIGN section 11.2, item 7) |
| `app_example_source` | read | `name` | an example's YAML, without opening it |
| `document_begin_change` | read | `label` | starts a change group (section 4.4) |
| `document_end_change` | read | — | ends it |

An external agent has no system prompt the app controls, so the workflow
lives in `agent/web-app.md` (`docs/agent-web-app.md`, about 80 lines),
and `document_state`'s description says to read it first. It covers:
View and Edit modes and what each allows; `document_state` first; in Edit
mode the `editor_*` tools with `expected_version`, in View mode
`document_check` then `document_set_source`; `document_image` to look at
the result; `app_read_guide` for an element type before using it; wrapping
a multi-step change in `document_begin_change` / `document_end_change`;
making only the change asked; `external` actions are confirmed by the
user. A test fails when it names a command not in `agentTools()`.

### 4.2 A stable list

`commands.agentTools()` returns the agent list for this device, **the same
for the page's whole life**, whatever the mode: every agent command, plus
the editor tools on a device with `directEditing`, even while the editor
is not loaded. Their definitions come from `web/app/editor-tools.json`,
generated from the editor module's `toolDefinitions()` and committed; a
test fails when they differ. A call to a command not available now fails
with guidance instead of "unknown": an `editor_*` tool in View mode says
"the editor is not open: call app_set_mode with mode edit, or use
document_set_source"; a `document` command with no document says to open
or create one.

Why stable: MCP clients read `tools/list` once and follow
`list_changed` unevenly (plan step 1 checks the ones we target); WebMCP
registrations are cheap but agents may cache them; and an agent plans
better knowing up front what Edit mode would give it. The bridge's list
then changes only when a page pairs or goes away.

`commands.list()` (what is available now) stays as it is for the shell and
the tests.

### 4.3 Origins and confirmation

- An origin is `user` or `agent:<connector>`, `<connector>` being
  `webmcp` or `mcp:<client>` (the name `nymph mcp`'s client gave in
  `initialize`, e.g. `mcp:claude-code`, reduced to `[a-z0-9-]`, at most 32
  characters).
- Connectors call `commands.callAgent(name, args, connector)`, which
  builds the origin and rejects any other connector name; only the shell's
  controls call `commands.call(…, "user")`. No outside input can choose
  its origin.
- `external` commands from an agent origin wait for the existing
  confirmation dialog (app DESIGN section 8). Its Allow click is the user
  gesture that downloads and clipboard writes need (*unverified* for the
  clipboard after an `await`; plan step 1).
- The dialog has "Allow for this session": the same command from the same
  connector then runs without asking until the document closes. Never
  remembered across sessions.

### 4.4 Change groups

An agent's request often takes several tool calls; undoing half of it is
useless. `document_begin_change(label)` opens a group: in Edit mode
`beginGroup(label, origin)` (app DESIGN section 5.2), so the calls until
`document_end_change` are one undo step; in View mode one activity entry
with one snapshot (section 5). A group also ends when its connector
disconnects, after 60 s without a call from it, or on a mode switch. A
call without a group is its own step, labelled `<tool> (<connector>)`, as
`nymph mcp`'s edits are today.

Today a change made while a group is open joins it, whoever makes it; a
user's edit during an agent's change would then be undone with the
agent's. **Change:** `editor_model`'s commit ends the open group first
when the change's origin differs from the group's. The registry reopens
the group (same label) before the agent's next write while its
`document_begin_change` is still in force; the change is then two undo
entries, correctly, since the user's sits between them. Two connectors'
groups do not nest: a second connector's `document_begin_change` while
another's group is open is refused with "another agent is changing the
chart".

## 5. The Agents Panel (`web/app/app/agents/`)

The dock (app DESIGN section 11.2, item 6) gets an **Agents** panel,
opened from an **Agents** button in the top bar (shown when
`config.json` enables a connector) or `Ctrl+Shift+A`, and shown by
itself the first time an agent calls a tool. DOM, labelled
`complementary` region, keyboard reachable. On a phone it is a bottom
sheet.

- **Connections**: each connected agent with its route (`mcp:claude-code`
  through `nymph mcp`, `webmcp`), Disconnect for bridge connections, and
  **Connect an agent…**: a field for a pairing code, the steps per client
  ("ask your agent to connect to the nymph web app"; links to the client
  docs), and the browser's refusal explained when the socket cannot open
  (section 7.2). Hidden on phones, which run no `nymph mcp`.
- **Activity**: one entry per agent change (a group, or a single write):
  who, its label, when, the tools it called on expand, and **Undo**. Read
  calls are not listed; `external` ones are, with what the user allowed.
- **Undo** (`activity.js`). View mode has no undo history (the viewer
  holds one source, app DESIGN section 3.3), so `activity.js` keeps the
  source before and after each entry (in memory, open document only, at
  most 50):
  - Edit mode, and the entry is still the editor's last undo entry
    (`last_label()` / `last_origin()`): `editor.undo()`.
  - Otherwise, when the current source equals the entry's "after": the
    "before" source through `document_set_source` (origin `user`).
  - Otherwise (changed since): "Restore the chart from before this change?
    Later changes are lost." with Cancel.
- While an agent's group is open, the panel's header says "Claude Code is
  changing the chart" with **Stop**, which ends the group and refuses that
  connector's writes until its next `document_begin_change` or a new
  call after the user dismisses the notice.

## 6. Capabilities and Configuration

- `device.assistant` becomes `device.agents`: the bridge needs a desktop
  (`directEditing`); WebMCP is used wherever the browser has it. On a
  phone without WebMCP no agent can reach the app (section 9).
- `config.json`'s `agents` section, from the CMake cache variable
  `NYMPH_AGENT_CONNECTORS` (default `webmcp;bridge`):
  `{"webmcp": {}, "bridge": {"pairUrl": "<site URL>"}}`. A connector not
  listed is not offered.
- The CSP gains `ws://127.0.0.1:* ws://localhost:*` in `connect-src`.

## 7. Connectors

### 7.1 WebMCP (`agents/webmcp.js`)

Where `navigator.modelContext` exists (*unverified*: the API is a draft of
the W3C Web Machine Learning community group; plan step 1 records the
shape Chrome ships), the app registers `agentTools()` as the page's tools
when a document screen is shown, each `execute` calling
`commands.callAgent(name, args, "webmcp")`, and unregisters them on the
start screen. On by default: a browser agent the user invoked can already
read and click the page, the tools only let it do so correctly, and
`external` commands are still confirmed. A switch in the Agents panel
turns it off for this browser. Browser agents that read only the DOM see
an unreadable canvas; that is a reason to register, not a fallback.

### 7.2 The `nymph mcp` bridge

**From the agent's side.** `nymph mcp` gains a tool, `web_app_pair`
(always listed). The agent calls it when the user wants it to work on the
chart open in the web app. `nymph mcp` opens a listener on `127.0.0.1` (a
free port) if it has none, and returns a **pairing code** and a **pairing
link**:

```
code:  48213-K7QM-2XPD          (port, then 40 random bits in Crockford base32)
link:  https://binary-dice-games.github.io/nymph/#pair=48213-K7QM-2XPD
```

The agent shows both: the code goes into the open tab's Connect an agent
field; the link opens the app in a new tab, paired. Both stay in the URL
fragment, so they never reach a server.

**From the page's side** (`agents/bridge.js`). The page opens
`ws://127.0.0.1:<port>/nymph`, sends `{"pair": "<code>"}` first, and then
serves MCP over the socket as a server: `initialize` (with the
`instructions`: read `app_read_guide("web-app")` first), `tools/list`
(`agentTools()`), `tools/call` (through `commands.callAgent(…,
"mcp:<client>")`), and it sends `nymph/focus` when its tab gains focus. On
success `nymph mcp` answers with a **resume token** (128 random bits) that
the page keeps in `sessionStorage`, so a reload of the same tab reconnects
without a new code while that `nymph mcp` runs.

**In `nymph mcp`** (`src/mcp/web_bridge.*`):

- The listener accepts only a `GET` upgrade to `/nymph` with `Host`
  `127.0.0.1:<port>` or `localhost:<port>` (against DNS rebinding) and an
  `Origin` in the allowed list: the site's origin
  (`https://binary-dice-games.github.io`), plus those in
  `NYMPH_WEB_APP_ORIGINS` (self-hosted copies, `http://localhost:<port>`
  for a local `nymph-site` preview). Anything else is closed without a
  body.
- A code is valid once, for 5 minutes; three wrong codes in a row close
  the listener (a new `web_app_pair` opens another). Comparison is
  constant-time. After pairing, the listener stays open only for resume
  tokens; it closes when `nymph mcp` exits.
- A paired page is listed by `editor_bridge` as one more editor, beside
  native `nymph view --edit` windows: `editor` name `web:<file name>`,
  focus time from `nymph/focus`. The editor tools route to it as to a
  native editor (workspace DESIGN section 6).
- Its other tools (`document_*`, `app_*`) join `nymph mcp`'s tool list
  while a page is paired, each with the optional `editor` argument;
  `nymph mcp` declares `tools.listChanged: true` and sends
  `notifications/tools/list_changed` when that set changes. The app's
  names do not collide with the chart tools (`render_chart`,
  `check_chart`, ...). A client that does not follow `list_changed`
  (plan step 1) gets a generic `web_app_call {name, arguments}` tool
  instead, listed always.
- A call to the page waits up to 5 minutes (a confirmation may be open),
  then fails with "the web app did not answer"; a closed socket fails
  waiting calls with "the web app was closed".
- The client name from `initialize` (`clientInfo.name`) goes with each
  call as `_meta["nymph/origin"]`; the page turns it into `mcp:<client>`.

**Transport code.** `src/ipc/loopback.*`: a TCP listener and stream bound
to 127.0.0.1, beside `local_socket` with the same `#if defined(_WIN32)`
guards. `src/mcp/websocket.*`: the server side of RFC 6455 the bridge
needs: the upgrade (`Sec-WebSocket-Accept`, hence `sha1.*`), masked client
frames, unmasked server frames, text messages up to 16 MB (fragments
reassembled), ping / pong, close. No extensions, no binary messages, no
TLS (loopback only). One thread per page reads messages; calls from the
session thread wait on a promise per request id, as `editor_bridge` does.
Writes to `nymph mcp`'s standard output now come from two threads (the
session, and a page's reader for `list_changed`), so `json_rpc`'s writer
takes a mutex.

**Browser requirements.** A public site reaching `ws://127.0.0.1` needs,
in Chrome, the user's Local Network Access permission at the first
connection (*unverified* for WebSockets in the shipping version); Firefox
treats loopback as secure (*unverified*); Safari may block it as mixed
content (*unverified*). Plan step 1 records each; where it cannot work,
the Connect dialog says so.

### 7.3 "Open in the web app" from the MCP App

The MCP App (MCP DESIGN section 4.2) shows a chart in the agent's chat.
Its menu gains **Open in the web app**: the App calls an app-only tool,
`web_app_pair`'s sibling `web_app_link {source}`, on its own `nymph mcp`;
the result is `<site>/#y=<source>&pair=<code>`, which the App opens with
the host's open-link request (*unverified* per host; plan step 1). The tab
opens the chart and pairs with that same `nymph mcp`, so the agent in the
chat can go on editing what the user now sees in the app. Without a
host's open-link support the item copies the link.

## 8. Security Notes

- Opened files are untrusted and may contain text written to steer an
  agent. They reach agents only as tool results; nothing a tool returns
  widens what an agent may do without the user (`external` stays
  confirmed; there is no tool that fetches arbitrary URLs except
  `app_open_url`, which is `external`).
- The pairing code proves the page is the one the user means; `Origin`
  stops other sites; `Host` stops DNS rebinding; binding 127.0.0.1 stops
  the network; the code's one use, expiry and three-try limit stop local
  guessing. A local process of the same user can always act as that user;
  that is outside this design's threat model.
- The page never inserts tool arguments or agent text as HTML; the
  activity list shows labels and arguments with `textContent`.

## 9. Design Decisions

- **No in-app chat, no API keys.** The first draft of this design had the
  page call Claude and Gemini with a key the user pasted. Dropped: a user
  cannot check that a key handed to a web app is not misused, whatever the
  app promises, and the standard way to join an app and an agent is the
  other way round: the app offers tools (MCP, WebMCP, MCP Apps) and the
  user's agent, with its own login and billing, uses them. It also removes
  provider SDKs, model choice, a chat UI and per-provider upkeep from
  nymph. Cost: phones and users without an MCP-capable agent get no agent
  in the app; a hosted remote MCP server would reach claude.ai and phone
  chat apps, and is a separate project because it needs a server (app
  DESIGN goal 5). Rejected too: provider sign-in in place of a key, which
  Anthropic does not offer to third-party web apps.
- **One registry, every route.** Connectors only translate. A second path
  (a connector calling the viewer or editor directly) would skip the
  confirmation, the attribution or the snapshot, which goal 2 forbids.
- **A stable tool list** over "what is available now" (section 4.2).
- **The editor tool definitions committed as JSON**, not read from the
  editor module, so View mode and WebMCP list them without a 1 MB
  download. A staleness test keeps them true.
- **Explicit change groups** over guessing an agent's request boundaries
  by idle time, which would group unrelated edits; the idle timeout only
  closes a group an agent forgot.
- **A change by someone else ends an agent's group.** Otherwise undoing
  the agent's change also undoes the user's edit made during it.
- **Undo by snapshot in View mode**, not an undo history in the viewer:
  a few sources per document cost nothing and also cover a change that
  switched modes.
- **The page as the MCP server, `nymph mcp` as the listener.** A page
  cannot accept connections; `nymph mcp` is already the process every
  local agent runs, and its editor bridge already routes editor tools to
  "the editor the user has open". Rejected: a separate `nymph serve`
  daemon (one more thing to start); a fixed port (collisions, and a page
  could probe it without a code); WebRTC (needs signalling).
- **A typed code, not a confirmation on the agent side.** The page must
  prove it is the page the user means; a CLI or chat has no reliable way
  to show a prompt. The code also carries the port.
- **Pairing on request.** Goal 5: no port open for people who never pair.
- **WebMCP on by default** (section 7.1).

## 10. Constraints and Invariants

- No loopback socket is opened until the user pairs; WebMCP registration
  makes no request; no other request is added.
- A connector never calls the viewer, the editor or `app` internals; only
  `commands.callAgent`. No connector can produce the origin `user`.
- `external` commands from an agent always pass the confirmation (or the
  session's "Allow for this session" for that connector and command).
- Tool arguments and results are never inserted as HTML.
- `nymph mcp`: the listener binds 127.0.0.1 only, checks `Host` and
  `Origin`, accepts a code once, closes after three wrong codes; only the
  session thread touches the tool registry; reader threads only parse,
  fulfil promises and queue notifications; shutdown closes sockets and
  joins every thread. Without `web_app_pair` or `web_app_link`: no socket,
  no thread, no change to the tool list.
- The web build compiles none of `src/ipc/` and `src/mcp/web_bridge.*`.
- Tests use port 0 and test-named files (CLAUDE.md, Tests).
- Code style: JavaScript as `web/app/` (ES modules, no framework, no build
  step); C++ as nymph.

## 11. Testing Strategy

1. **Node**: `editor-tools.json` against the editor module's
   `toolDefinitions()`; `agent/web-app.md` names only `agentTools()`
   commands; every `agentTools()` schema is valid JSON Schema.
2. **Browser** (Playwright, built site), with a **test connector** that
   calls `callAgent` as a bridge would: a group's writes are one undo
   step and one activity entry, undone in Edit and in View; a user edit
   during a group survives the group's Undo; Stop ends a group; an
   `external` call waits for the dialog and Cancel returns an error; an
   `editor_*` call in View mode returns the guidance; WebMCP with a
   stubbed `navigator.modelContext`; on an emulated phone the panel is a
   sheet without Connect.
3. **Native** (GoogleTest): `sha1` vectors; WebSocket handshake and
   framing; the listener refuses a wrong `Host` or `Origin`, a reused or
   wrong code, and closes after three; resume tokens; `web_app_pair`,
   `web_app_link` and forwarding with a C++ test client acting as the
   page; `list_changed`; `editor_bridge` picks a page or a native editor
   by focus; `editor_model`: a commit with another origin ends the group.
4. **End to end** (browser, when a native `nymph` is available to the web
   job, else skipped with 77): a fake MCP client drives `nymph mcp`, calls
   `web_app_pair`, the page pairs, the client's edit changes the page's
   chart.
5. **By hand**, recorded in the plan: Claude Code, Cursor and Gemini CLI
   paired in Chrome and Firefox on the deployed site; "Open in the web
   app" from Claude Desktop; WebMCP in a Chrome that has it.

## 12. Integration Boundaries

Depends on: app DESIGN (registry, dock, documents, `device.js`, editor
module API, `agent/` folder), workspace DESIGN (editor tools, versions,
`editor_bridge`, discovery), MCP DESIGN (`nymph mcp`, session, MCP App,
skill text), [12_cursor_integration](../12_cursor_integration/DESIGN.md)
(Cursor runs `nymph mcp`).

Changes others see: `nymph mcp` has `web_app_pair` (and the app-only
`web_app_link`), may list `document_*` / `app_*` tools and send
`list_changed`; `editor_bridge` lists web pages; `editor_model` ends a
group on another origin's change; the registry has `agent`,
`agentTools()`, `callAgent()` and change groups; the MCP App has "Open in
the web app"; the app's CSP allows loopback WebSockets.

Risks, checked first (plan step 1): loopback WebSockets from the public
site in each browser; WebMCP's shape and which agents call it; whether
Claude Code, Cursor and Gemini CLI follow `tools/list_changed`; the MCP
Apps hosts' open-link request; clipboard and download after the
confirmation.

## 13. Extension Points

- A **hosted remote MCP server** (streamable HTTP, OAuth) for claude.ai,
  ChatGPT and phone chat apps, relaying to a paired tab: its own project,
  since it needs a server.
- `resources/subscribe` from the bridge, so an external agent is told when
  the user changes the chart instead of re-reading it.
- The Agents panel in `nymph view --edit`'s window (an activity list over
  the existing editor server).
