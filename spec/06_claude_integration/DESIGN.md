# nymph — Claude Integration: Design

**Status: in progress; PLAN step 1 done except the checks that need a
real Claude client.** [PLAN.md](PLAN.md) is the
ordered implementation plan. This document describes only what this
project adds or changes; the rest of the architecture is
[spec/01_core_features/DESIGN.md](../01_core_features/DESIGN.md)
(*core DESIGN*), [spec/02_opengl_rasterizer/DESIGN.md](../02_opengl_rasterizer/DESIGN.md)
(*GL DESIGN*) and [spec/04_web_viewer/DESIGN.md](../04_web_viewer/DESIGN.md)
(*viewer DESIGN*).

## 1. Purpose / Scope

nymph is meant for AI agents, but today an agent finds it only if someone
built the binary, put it on `PATH` and told the agent to read
`docs/agent-guide.md`, and the user sees the chart only if their client
shows local images. This project makes nymph a **tool Claude can use
with one install step**. Claude renders charts and sees them, the user
sees them in the chat, and where the client can show interactive
widgets the chart is the live web viewer, which the user can pan, zoom
and hover, and whose view Claude can read back.

Hard requirements, from the request:

1. **Frictionless**: one install action per Claude client, no runtime to
   install (no Node, Python or package manager), no settings to edit.
2. **The agent renders PNGs and sees them**, and the user sees them in
   the chat.
3. **Interactive in the chat** where the client allows it: the viewer of
   viewer DESIGN, embedded in the conversation.

In scope:

- `nymph mcp`: an MCP server (stdio) inside the `nymph` executable, with
  tools to render, check, extract, describe the schema and list fonts
  (section 5).
- **The nymph MCP App**: an HTML view (MCP Apps extension, `ui://`
  resource) that shows the result of `render_chart` in the live viewer
  and reports the user's view to Claude.
- Packaging for each client (section 3): a **Claude Code plugin** (this
  repository is its marketplace), an **MCP bundle** (`.mcpb`) for Claude
  Desktop, and a **Skill zip** for claude.ai.
- Release builds: a static Linux executable (x86-64, arm64) and a
  Windows executable, with SHA-256 sums, which the packages carry or
  download.

Out of scope: a hosted remote MCP server (streamable HTTP) for claude.ai
and mobile (section 10 says how it would fit); macOS builds (section 10,
an open question for the owner); any change to the source format or to
the viewer's JavaScript API beyond section 4.3; other agents' clients
(the server is plain MCP, so they work if they speak it, but nothing
here is tested with them).

New and changed files:

| Path | What |
|---|---|
| `src/mcp/` | The `nymph_mcp` library: JSON-RPC over stdio, the MCP session, the tools, the embedded App resource |
| `src/cli/cli.cpp` | The `mcp` command |
| `claude/app/` | The MCP App: `viewer.html.in` and `app.js` (the host bridge) |
| `claude/plugin/` | The Claude Code plugin: manifest, `.mcp.json`, skill, launcher |
| `.claude-plugin/marketplace.json` | Makes this repository a plugin marketplace |
| `claude/mcpb/` | `manifest.json.in` for the Claude Desktop bundle |
| `claude/skill/` | The claude.ai Skill: `SKILL.md` and its reference file |
| `cmake/NymphRelease.cmake` | Static build flags and the package targets |
| `extern/json` | nlohmann/json (new submodule) |
| `docs/claude.md` | Installing and using nymph from Claude |

### Feasibility (measured before this design)

- A static Linux build (`-DNYMPH_OPENGL=OFF -DCMAKE_EXE_LINKER_FLAGS=-static`,
  MinSizeRel, every font embedded) is one 10.8 MB file stripped,
  **3.7 MB gzip**, with no shared library dependency, and renders
  `examples/dashboard.yaml` in 67 ms.
- The MCP Apps extension (stable since 2026-01-26) renders a tool's
  `ui://` resource in a sandboxed iframe in claude.ai and Claude Desktop
  (chat and the Code tab). The Claude Code terminal shows no widgets.
- The spec's iframe Content Security Policy has no way to allow
  WebAssembly (`'wasm-unsafe-eval'`); the proposal to add one
  (modelcontextprotocol/ext-apps issue 605) is open, and under the
  spec's default policy the viewer cannot start. **Claude's hosts allow
  it anyway**: the policy users quote from claude.ai, Claude Desktop
  (macOS) and the iOS app (anthropics/claude-ai-mcp issue 40, July to
  August 2026) has `script-src 'self' 'unsafe-inline' 'unsafe-eval'
  blob: data:`, and under that policy, in Chromium, the single-file
  viewer draws `dashboard.yaml` (first frame 218 ms, no violation).
  Nested frames are refused (`frame-src 'self' blob: data:`, by
  Anthropic's choice). PLAN step 1 records the details; confirmation in
  a real Claude client is still open there.

## 2. Design Goals

1. **One file does everything.** The `nymph` executable is the CLI, the
   MCP server and the carrier of the App's HTML. A package is that file
   plus manifests and text; nothing else is installed or downloaded at
   run time beyond the executable itself.
2. **The PNG always works.** Every client gets the rendered PNG as
   image content; the App is an addition the client may ignore. A
   client without MCP Apps loses nothing that worked before.
3. **Errors are for the agent to fix.** A problem in the source comes
   back as a tool result marked `isError` with the CLI's exact message
   (`<line>:<column>: ...`), never as a protocol error, so Claude reads
   it and corrects the YAML, as with the CLI (core DESIGN section 7).
4. **The user's view reaches Claude.** Pan, zoom, rotation and hidden
   series made in the App reach the model as the source's `view` key
   (viewer DESIGN section 3, "Saved view"), so "zoom on March and add a
   trend line" works without the user describing the zoom.
5. **No surprise on the user's machine.** The server writes a file only
   when a tool call names it, writes only `.png` and `.yaml` files, and
   the launcher runs only a binary whose SHA-256 the plugin pins.

## 3. Clients and Packages

| Client | Package | Install | What the user gets |
|---|---|---|---|
| Claude Code (terminal) | Plugin | `/plugin marketplace add binary-dice-games/nymph`, `/plugin install nymph@nymph` | Claude renders and sees PNGs, and writes them into the project. The terminal shows no image; the user opens the file. |
| Claude Code (desktop app, Code tab) | Same plugin | Same | Also the App in the conversation. |
| Claude Desktop (chat) | `nymph-<version>-<os>-<arch>.mcpb` | Double-click, or Settings > Extensions | PNG and App in the conversation. |
| claude.ai (web, mobile) | `nymph-skill-<version>.zip` | Upload in claude.ai's Skills settings (code execution on) | Claude runs the static Linux binary in its code sandbox and shares the PNG as a file. No App (no local MCP server there). |

The four packages carry the same executable and the same agent text
(section 4.5), generated from one source so they cannot drift.

## 4. Key Abstractions

### 4.1 `nymph mcp` (`src/mcp/`)

A stdio MCP server: newline-delimited JSON-RPC 2.0 on standard input and
output, logs on standard error. It is a library, `nymph_mcp`, on top of
`nymph_cli` (viewer DESIGN section 6), whose helpers its tools share, so
tests drive it with string streams. Because of that layering the
executable (`app/main.cpp`), not `run_cli`, dispatches `nymph mcp
[--rasterizer R]` to `run_mcp_cli`; `run_cli` only lists the command in
its usage and, in a build without the server, refuses it (exit 2).

- `json_rpc` — reads one message per line, writes one per line, and
  turns malformed input into JSON-RPC errors (`-32700`, `-32600`,
  `-32601`, `-32602`). Batches are rejected (MCP removed them).
- `mcp_session` — `initialize` (protocol version negotiation, server
  info, capabilities `tools` and `resources`, the server instructions;
  revisions `2025-11-25`, `2025-06-18` and `2025-03-26`),
  `notifications/initialized`, `ping`, `tools/list`, `tools/call`,
  `resources/list`, `resources/read`. Requests are handled in arrival
  order on one thread. Claude Code 2.1.302 first sends `server/discover`
  (the stateless `2026-07-28` revision) and falls back to `initialize`
  on `-32601` (PLAN step 1); the session answers it with `-32601` until
  that revision is implemented (section 10).
- `tool_registry` — the tools of section 5.1, each a name, a JSON input
  schema, an output schema where it returns structured content, its
  `_meta` (section 4.2) and a handler.
- One `bdg::nymph::renderer` for the session (GL DESIGN): created with
  `--rasterizer` (default `opengl`, falling back silently as in the CLI;
  the static release builds have only `software`).

Invariants: standard output carries only protocol messages (one stray
byte breaks the client); a handler's exception becomes an `isError`
result (section 7); the server exits 0 when standard input closes.

### 4.2 The MCP App (`claude/app/`, resource `ui://nymph/viewer`)

One self-contained HTML page: `viewer.html.in` (layout, CSS reading the
host's style variables with fallbacks) with `app.js` and the viewer's
`nymph-viewer.single.js` (viewer DESIGN section 6) inlined at build
time. CMake writes it to `<build>/claude/viewer.html`, and the native
build embeds it in `nymph_mcp` when the cache variable
`NYMPH_MCP_VIEWER` names that file (section 6). Without it the server
offers no resource and no `_meta.ui`, and works as a PNG-only server.

`render_chart` declares `_meta.ui.resourceUri: "ui://nymph/viewer"`
(visibility `["model", "app"]`) for every client, whether or not its
`initialize` lists the `io.modelcontextprotocol/ui` extension (the
Claude Code terminal does not): clients ignore `_meta` they do not
know, and one tool list is simpler to test. The resource is served as
`text/html;profile=mcp-app` with `_meta.ui.csp` empty (no network: the
page needs none) and `prefersBorder: true`. The page relies on
`'unsafe-eval'` (or `'wasm-unsafe-eval'`) in the host's `script-src`
for WebAssembly, which Claude's hosts grant (section 1, feasibility).

`app.js` is a hand-written host bridge (postMessage JSON-RPC, about 200
lines), not the `@modelcontextprotocol/ext-apps` SDK (section 8):

1. `ui/initialize` with `appCapabilities.availableDisplayModes:
   ["inline", "fullscreen"]`; reads `hostContext.theme` and the
   container dimensions.
2. On `ui/notifications/tool-result`: takes `structuredContent.source`
   and calls `createViewer(box, {source, fit: "width", theme, menu,
   fetchFont})`, then reports its height with
   `ui/notifications/size-changed`.
3. `fetchFont(name)` calls the app-only tool `viewer_font` through
   `tools/call` and decodes the base64 bytes; the viewer checks them
   against the catalog as for any fetch (viewer DESIGN, "Fonts on
   demand").
4. On the viewer's `view` and `click` events (debounced 500 ms): sends
   `ui/update-model-context` with the text of section 4.4.
5. On `ui/notifications/host-context-changed`: `setTheme()`.
6. A fullscreen button sends `ui/request-display-mode`; the viewer
   refits on resize.
7. The right-click menu stays (Save as PNG / YAML) where the sandbox
   allows downloads; otherwise its items are hidden (`menu: false`) and
   the user asks Claude to save.

A render that fails shows nothing in the App (the host shows the error
result); a viewer error (a font the tool cannot provide) shows the
viewer's own error text and the PNG from the result's image content
instead.

### 4.3 Viewer change

None to the API; `fetchFont(name)` (viewer DESIGN section 5) already
lets the embedder supply font bytes. One fix: `createViewer()` resolves
the default `fontBaseUrl` against `import.meta.url` even when
`fetchFont` is given, and inside an inline page (`about:srcdoc`, how a
host may load the App) `new URL('./fonts/', 'about:srcdoc')` throws
`Invalid URL` (PLAN step 1). The default base is resolved only when the
default fetcher is used, and a base that cannot be resolved fails only
the first font fetch, with the viewer's usual `NymphError`.

### 4.4 The context sent back to Claude

The App never sends the whole `toYaml()` (it repeats the data Claude
wrote). It sends, as one text content block that replaces the previous
one:

```
nymph chart "<description or title>": the user changed the view.
view:
  root/children/0: {x_min: 12, x_max: 18}
Last clicked: root/children/0 (PlotLine "revenue") x=2026-03 y=15
To keep this view, render the source with this view key.
```

The `view` lines are the `view` key of `toYaml()` (only what the user
changed); "Last clicked" appears after a click. The host decides when
the model sees it (MCP Apps: the latest update before the next user
message).

### 4.5 Agent text (`claude/skill/`)

`docs/agent-guide.md` stays the reference. From it, one short text
(about 60 lines: the workflow, the source skeleton, the error format,
when to call `chart_schema`) is written once as
`claude/skill/nymph/SKILL.md` and used three ways: the MCP server's
`instructions` (embedded at build time, front matter stripped), the
plugin's skill and the claude.ai Skill. The full `agent-guide.md`
travels as `reference.md` beside the skill and as the resource
`nymph://docs/agent-guide`.

### 4.6 The Claude Code plugin (`claude/plugin/`)

```
.claude-plugin/marketplace.json     (repository root) lists ./claude/plugin
claude/plugin/
  .claude-plugin/plugin.json        name "nymph", version = nymph version
  .mcp.json                         server "nymph": ${CLAUDE_PLUGIN_ROOT}/bin/nymph-launch mcp
  skills/nymph/SKILL.md, reference.md   copies made by the build, committed
  bin/nymph-launch, bin/nymph-launch.cmd
  bin/checksums.txt                 SHA-256 of each release binary of this version
```

The launcher finds the executable in this order: `$NYMPH_BIN`; a cached
download in `$NYMPH_CACHE/<version>/`, which `.mcp.json` sets to
`${CLAUDE_PLUGIN_DATA}` (the plugin's persistent data directory, which
Claude Code expands; PLAN step 1); else
it downloads the release asset for this OS and architecture, checks it
against `checksums.txt`, and caches it. A mismatch or a failed download
prints one line on standard error and exits 1 (the client shows the
server as failed). It never uses an unpinned `nymph` from `PATH`: a
different version's tools may not match the skill. Because the Bash
tool also exists in Claude Code, the skill tells Claude it may run
`nymph-launch` as the CLI for anything the tools do not cover.

### 4.7 The MCP bundle (`claude/mcpb/`)

`manifest.json.in` (MCPB manifest, `server.type: "binary"`,
`entry_point: "bin/nymph"`, `args: ["mcp"]`, the tool list, no user
configuration). `cmake --build . --target nymph-mcpb` zips it with the
executable into `nymph-<version>-<os>-<arch>.mcpb`. The executable is
inside, so the bundle downloads nothing.

### 4.8 The claude.ai Skill (`claude/skill/`)

`nymph-skill-<version>.zip`: `nymph/SKILL.md`, `nymph/reference.md`,
`nymph/bin/nymph` (static Linux x86-64 only: the code execution
sandbox is x86-64; the executable is 10.8 MB, under the 30 MB upload
limit; PLAN step 1). The zip stores the executable with mode 755. `SKILL.md` tells Claude to run
`bin/nymph` with `--rasterizer software`, look at the PNG it wrote, and
share it as an output file. claude.ai's code sandbox has no network, so
the binary must be inside the zip.

## 5. Public Contract

### 5.1 Tools

| Tool | Input | Result |
|---|---|---|
| `render_chart` | `source` (YAML text) or `input_path` (a `.yaml` or nymph `.png` file); optional `output_path` (`.png`); optional `rasterizer` | Content: the PNG as `image` (`image/png`, base64) and one `text` line (`Rendered 800x600 PNG` and `, written to <path>` when written). `structuredContent`: `{source, width, height, path?}`, `source` being the YAML rendered (from the file when `input_path`). `_meta.ui` as in section 4.2. |
| `check_chart` | `source` or `input_path` | `text`: `valid`, or `isError` with the message. |
| `extract_chart` | `input_path` (`.png`) or `png_base64`; optional `output_path` (`.yaml`) | `text`: the YAML the PNG carries; `isError` for a PNG without one. |
| `chart_schema` | optional `type` | `text`: `nymph schema [type]` output; an unknown type is `isError` with suggestions. |
| `list_fonts` | none | `text`: `nymph fonts` output. |
| `viewer_font` (visibility `["app"]`) | `name` | `structuredContent`: `{name, base64}` of an embedded catalog font; `isError` for any other name. |

Rules for every tool:

- Exactly one of `source` / `input_path` (and of `input_path` /
  `png_base64`); otherwise `isError` naming the fields.
- Relative paths resolve against the server's working directory (the
  project directory in Claude Code). `output_path` must end in the
  extension listed; it is written as the CLI writes (atomic replace,
  core DESIGN section 7), so a failed call changes no file.
- Errors in the source carry the CLI's text with `<input>` as the file
  name, `source` for inline text: `source:9:56: unknown column
  'revenu' (did you mean 'revenue'?) (columns: month, revenue, cost)`.
- Results larger than 8 MB of base64 (very large images) return the
  `text` and `structuredContent` without the image and say so.

### 5.2 Resources

| URI | MIME type | What |
|---|---|---|
| `ui://nymph/viewer` | `text/html;profile=mcp-app` | The MCP App (only when built with `NYMPH_MCP_VIEWER`) |
| `nymph://docs/agent-guide` | `text/markdown` | `docs/agent-guide.md` |
| `nymph://docs/elements` | `text/markdown` | `docs/elements.md` |

### 5.3 Command line

`nymph mcp [--rasterizer software|opengl]`: runs the server until
standard input closes. Exit 0 on end of input, 1 if standard output
fails, 2 for bad arguments (`docs/cli.md` gains the row).

## 6. Build

- New option `NYMPH_MCP` (default ON; forced OFF with `NYMPH_WEB`):
  builds `nymph_mcp`, links it into `nymph-cli`, and calls
  `nymph_ensure_submodules(extern/json)`.
- New cache variable `NYMPH_MCP_VIEWER` (default empty): path to a
  `viewer.html` built by the `NYMPH_WEB` build. When set, it is embedded
  as a byte array like the fonts (`cmake/NymphFonts.cmake` style). The
  native build cannot make it itself (it needs Emscripten), so a release
  configures the web build first and passes its output.
- `cmake/NymphRelease.cmake` (option `NYMPH_RELEASE`, default OFF):
  MinSizeRel, `NYMPH_OPENGL=OFF` and `-static` on Linux (section 8), `/MT`
  on MSVC, stripped; package targets `nymph-mcpb`, `nymph-skill`,
  `nymph-plugin-sync` (copies `SKILL.md`, `reference.md` and writes
  `checksums.txt` into `claude/plugin/`), and `nymph-dist` (all of them
  plus `SHA256SUMS`).
- The web build gains a `nymph-claude-app` target writing
  `claude/viewer.html` from `claude/app/` and `nymph-viewer.single.js`.

## 7. Constraints and Invariants

- Standard output in `nymph mcp` is the protocol only. Nothing in the
  library prints; the server sends warnings (an OpenGL fallback with an
  explicit `--rasterizer opengl`) as `notifications/message`.
- Every handler catches all exceptions: `nymph::error` and
  `std::runtime_error` become `isError` results with their text;
  `std::bad_alloc` and anything else become `isError` "internal error:
  <what>". The server never exits because of a request.
- One request at a time on one thread: `renderer` and ImGui contexts are
  not thread-safe (GL DESIGN). `notifications/cancelled` for a request
  already running is ignored (renders take milliseconds).
- The App page makes no network request and needs no CSP entry; its
  only inputs are the tool result, `viewer_font` and the host context.
- The App never calls a model-visible tool and never sends `ui/message`
  (no message in the user's name).
- Platforms: the server code is portable C++ with no `#ifdef` beyond
  what `nymph_cli` has; stdio must be switched to binary mode on Windows
  (`_setmode`), the one narrow guard.
- The launcher and the packages never run a binary that is not the
  pinned release (section 4.6), except `$NYMPH_BIN`, which the user
  sets.

## 8. Design Decisions

- **Server inside the `nymph` executable**, not a Node or Python wrapper
  around the CLI. Rejected wrappers: every client would need a runtime,
  breaking goal 1, and a wrapper adds a second version to keep in step.
  The protocol surface is small (section 4.1).
- **nlohmann/json as a submodule**, not libyaml as a JSON parser. YAML
  1.1 parsers differ from JSON on surrogate-pair escapes, tabs and
  number forms; base64 payloads of megabytes must round-trip exactly.
  It is header-only and widely used.
- **stdio only.** Every local client (Claude Code, Claude Desktop) uses
  it; streamable HTTP is needed only for a hosted server (section 10).
  The session layer is kept separate from the transport so a stateless
  protocol revision or HTTP is a new front end, not a rewrite.
- **Image content plus structured source**, not a file path only. The
  image lets Claude check its own chart (agent-guide step 4) on every
  client; the source in `structuredContent` feeds the App without
  re-parsing the PNG.
- **Hand-written App bridge**, not the ext-apps SDK. The SDK needs npm
  and a bundler in the build, which the repository has avoided (viewer
  DESIGN section 6); the App uses seven messages.
- **Only the `view` key goes back to Claude** (section 4.4), not the
  whole YAML: Claude already has the source; resending data wastes
  context and invites Claude to "fix" values it did not change.
- **Static Linux release with OpenGL off.** A fully static glibc binary
  cannot `dlopen` libEGL reliably, and the software rasterizer is fast
  enough (67 ms for the dashboard). Dynamic builds from source keep
  OpenGL.
- **Plugin downloads its binary**, rather than committing binaries to
  git. Plugins install from the git repository; 10 MB per platform per
  version in history is not acceptable. The pinned SHA-256 keeps the
  download as trustworthy as the plugin itself. While the repository is
  private, release assets need authentication: the launcher then
  depends on `$NYMPH_BIN` (open question in section 10).
- **The normal WebAssembly viewer in the App** (PLAN step 1). Claude's
  hosts put `'unsafe-eval'` in the App's `script-src`, which permits
  WebAssembly, so the App inlines the existing single-file viewer
  (2.4 MB, 815 KB gzip). Rejected:
  1. a `-sWASM=0` viewer build (wasm2js). It works under the spec's
     strict default policy (first frame 456 ms against 218 ms; built
     with `-fexceptions` in place of `-fwasm-exceptions`; 4.6 MB,
     1.17 MB gzip), but Emscripten 6.0.12 marks `WASM=0` deprecated and
     "under consideration for removal" (emscripten issue 27608), and
     Claude does not need it. It stays the answer for a stricter host
     if that ever matters;
  2. a nested iframe (`frameDomains`) to a hosted viewer: claude.ai
     refuses nested frames (anthropics/claude-ai-mcp issue 40).
  If a real client still blocks WebAssembly, the App falls back to the
  PNG of the result (section 4.2) and the decision is reopened.

## 9. Testing Strategy

- Native GoogleTest (`test_mcp`): the session driven through string
  streams; golden JSON for `initialize`, `tools/list`, `resources/list`;
  each tool's success and error results; malformed JSON, unknown
  methods, wrong arguments; standard output stays valid JSON lines;
  `output_path` atomicity (reuse `test_cli`'s helpers); files named per
  test (CLAUDE.md, parallel ctest).
- A process test (`test_mcp_process`): launches the built `nymph mcp`,
  performs a handshake and a render over pipes, closes input, expects
  exit 0.
- Browser test in the web build (Playwright, as `web/tests/browser/`):
  a fixture host page that implements the host side of MCP Apps in an
  iframe sandboxed and CSP'd like the spec's default (plus
  `'wasm-unsafe-eval'` or not, per step 1), sends a tool result, and
  checks the chart draws, `viewer_font` is called for a named font,
  `size-changed` arrives, and a drag produces an
  `ui/update-model-context` with a `view` key.
- Package checks: the `.mcpb` and Skill zips contain their files and
  version; the plugin's `checksums.txt` matches the release binaries;
  the launcher's verification rejects a tampered file.
- Manual, recorded in the plan: Claude Code terminal, Claude Desktop
  chat and Code tab, claude.ai with the Skill.

## 10. Open Questions and Extension Points

- **macOS.** Most Claude Desktop users are on macOS, which nymph does
  not build for (CLAUDE.md, Platform Support). The code is portable and
  a software-only build is likely small work, but adding a platform is
  the owner's decision. Until then the plugin and `.mcpb` cover Linux
  and Windows.
- **Private repository.** Plugin installation from a private repository
  works for users with access; release downloads by the launcher do not
  without a token. Either the repository (or a releases-only mirror)
  becomes public, or the launcher documents `$NYMPH_BIN`.
- **The `2026-07-28` protocol revision** (stateless, `server/discover`).
  Claude Code already tries it first; supporting it is a new front end
  on the same tools, worth doing when Claude Desktop sends it too.
- **Hosted server.** For an App on claude.ai and mobile, `nymph mcp
  --http <port>` (streamable HTTP) behind a public URL, added as a
  custom connector. Needs hosting, authentication and abuse limits; a
  project of its own.
- **Interactive editing.** An app-only `render_chart` call from the App
  (for example a "reset to source" or a data-range picker) fits the
  existing bridge; not needed for the first version.
