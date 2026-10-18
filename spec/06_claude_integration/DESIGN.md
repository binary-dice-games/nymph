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
| Claude Code (terminal) | Plugin | `/plugin install nymph --marketplace binary-dice-games/nymph` (Claude Code 2.1.275 or later; before: `/plugin marketplace add binary-dice-games/nymph`, then `/plugin install nymph@nymph`) | Claude renders and sees PNGs, and writes them into the project. The terminal shows no image; the user opens the file. |
| Claude Code (desktop app, Code tab) | Same plugin | Same | Also the App in the conversation. |
| Claude Desktop (chat) | `nymph-<version>-windows-x86_64.mcpb` (macOS once built) | Double-click, or Settings > Extensions | PNG and App in the conversation. |
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
`NYMPH_MCP_VIEWER` names that file (section 6). `nymph mcp` hands it to
the session as `server_options::viewer_html` (tests pass their own page).
Without it the server offers no resource, no `_meta.ui` and no
`viewer_font`, and works as a PNG-only server.

The page is about 2.4 MB, nearly all of it the viewer, whose WebAssembly
travels as a string of raw characters, NUL included: Emscripten's decoder
maps the U+FFFD the HTML parser puts in place of NUL back to 0, and
escapes CR, so the module survives inline in a `<script>`. CMake strings
cannot hold NUL, so `cmake/NymphClaudeApp.cmake` joins the template's
pieces and the two scripts byte for byte with `cmake -E cat`, and fails
the build if a script contains `</script`, `<script` or `<!--`.

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
   ["inline", "fullscreen"]`; reads `hostContext.theme` and
   `availableDisplayModes`. The chart takes the host's `light` or `dark`
   theme so it matches the conversation, and the source's own theme when
   the host gives none (the PNG Claude sees keeps the source's).
2. On `ui/notifications/tool-result`: takes `structuredContent.source`
   and calls `createViewer(box, {source, fit: "width", theme, menu,
   fetchFont})`, then reports its height with
   `ui/notifications/size-changed`.
3. `fetchFont(name)` calls the app-only tool `viewer_font` through
   `tools/call` and decodes the base64 bytes; the viewer checks them
   against the catalog as for any fetch (viewer DESIGN, "Fonts on
   demand").
4. On the viewer's `view` and `click` events (debounced 500 ms): sends
   `ui/update-model-context` with the text of section 4.4, only when that
   text changed since the last one sent. The viewer also fires `view`
   when the frame resizes and a plot refits; the `view` key lists only
   what the user changed, so a refit changes no text and sends nothing,
   and an untouched chart sends nothing at all.
5. On `ui/notifications/host-context-changed`: `setTheme()`.
6. A fullscreen button, shown only when the host offers `fullscreen`,
   sends `ui/request-display-mode` and follows the mode the host
   returns; the viewer (`fit: "width"`) refits on resize.
7. No right-click menu (`menu: false`). A sandboxed frame without
   `allow-downloads` cannot save files, and the sandbox attributes of
   Claude's hosts are not known (PLAN step 1); the user asks Claude to
   save, which renders the chart with the `view` key it was sent. Kept
   off unless a real client shows that downloads work.

A render that fails shows nothing in the App (the host shows the error
result); a viewer error (a font the tool cannot provide) shows the
viewer's own error text and the PNG from the result's image content
instead. A later tool result in the same frame goes through
`setSource()`, keeping the user's zoom where the chart allows. The page
exposes `window.nymphApp` (`viewer`, `host`, `displayMode`, `error`) for
tests.

### 4.3 Viewer change

None to the API; `fetchFont(name)` (viewer DESIGN section 5) already
lets the embedder supply font bytes. Two fixes. The `click` event never fired: `state()` hands out the
click once, and `sizeCanvas()` read `state()` in every drawn frame
before `afterFrame()` could emit it; `state()` now keeps the click for
the event (PLAN step 6). And `createViewer()` resolves
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
changed); "Last clicked" appears after a click. When only a click
happened, or the user went back to the chart as written, the `view`
block is replaced by "The view is the chart as written (no zoom, pan or
hidden series)." and the last line is left out. The same message carries
`structuredContent: {view, lastClicked}` (the block's text and the
pick), for hosts that pass structured context on. The host decides when
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
.claude-plugin/marketplace.json     (repository root) marketplace "nymph", lists ./claude/plugin
claude/plugin.json.in               the manifest's template (outside the plugin)
claude/plugin/
  .claude-plugin/plugin.json        name "nymph", version = nymph version
  .mcp.json                         server "nymph": sh ${CLAUDE_PLUGIN_ROOT}/bin/nymph mcp
  skills/nymph/SKILL.md, reference.md   copies of claude/skill/nymph/SKILL.md, docs/agent-guide.md
  bin/nymph                         the launcher (POSIX sh)
  bin/checksums.txt                 "<sha256>  nymph-<version>-<os>-<arch>[.exe]" per release executable
```

Everything but `bin/nymph` is written by the `nymph-plugin-sync` target
(`cmake/NymphPluginSync.cmake`) and committed, since the plugin installs
from the repository; a test fails when the copies or the version are
stale. `checksums.txt` is filled from the release's `SHA256SUMS` files
(`NYMPH_PLUGIN_SUMS`) when a release is published; until then it names
no executable.

`bin/` is on the Bash tool's `PATH` while the plugin is enabled, so the
launcher is also the `nymph` command Claude runs for anything the tools
do not cover (the skill says `nymph`). It runs, in order: `$NYMPH_BIN`;
the release for this OS and CPU named in `checksums.txt`, from
`$NYMPH_CACHE` (default `~/.cache/nymph/<version>/`) once its SHA-256
matches; else that release downloaded from `$NYMPH_RELEASE_URL` (default
the GitHub release `v<version>`), checked, then cached (atomic rename).
The version comes from the file name in `checksums.txt`, so a release
changes only that file and the manifest. A mismatch, a failed download
or a missing release prints one line on standard error and exits 1 (the
client shows the server as failed); a file whose SHA-256 differs is
never run or kept. It never uses an unpinned `nymph` from `PATH`: a
different version's tools may not match the skill. `NYMPH_LAUNCHER_INFO=1`
prints the release name and cache path (tests).

Why the user cache and not `${CLAUDE_PLUGIN_DATA}`: Claude Code exports
that variable to MCP servers but not to Bash-tool commands, so the
server and the `nymph` command would download the binary twice into
different places. Why `sh` and one script: `.mcp.json` has no
per-platform command; Claude Code on Windows requires Git for Windows,
whose `sh` runs the same script (not yet verified there, PLAN step 8).
Plugins with a `bin/` folder are refused by claude.ai and Cowork; this
plugin is for Claude Code, and claude.ai gets the Skill (section 4.8).

### 4.7 The MCP bundle (`claude/mcpb/`)

`manifest.json.in`: MCPB manifest version 0.3 (the version the spec
calls current; nothing of 0.4 is needed), `server.type: "binary"`,
`entry_point: "server/nymph"` (`server/nymph.exe` on Windows), command
`${__dirname}/server/nymph` (Claude Desktop appends `.exe` on Windows),
`args: ["mcp"]`, the tools the server offers the model, one
`compatibility.platforms` entry (`linux`, `win32` or `darwin`), no user
configuration, and `icon.png` (`claude/mcpb/icon.png`, 512 x 512, the
dragonfly of `resources/Nymph.png`). In release builds the `nymph-mcpb`
target (`cmake/NymphMcpb.cmake`) zips the manifest, the icon and the
executable (mode 755) into `dist/nymph-<version>-<os>-<arch>.mcpb`
before `SHA256SUMS` is written. The zip holds files only: `mcpb unpack`
(2.1.2) fails on a directory entry. The executable is inside, so the
bundle downloads nothing. The manifest cannot name a CPU, so the arm64
and x86-64 bundles of one OS differ only by file name. Claude Desktop
runs on macOS and Windows; until nymph builds for macOS the Windows
bundle is the one for most users, and the Linux bundles serve unofficial
Linux builds of Claude Desktop.

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
  `png_base64`); otherwise `isError` naming the fields. An argument a
  tool does not take is `isError` with the closest name ("unknown
  argument 'sorce' (did you mean 'source'?)"); `input_path` may not be
  `-` (standard input is the protocol). An unknown tool name, or
  `arguments` that is not an object, is a protocol error (`-32602`), as
  MCP asks.
- Relative paths resolve against the server's working directory (the
  project directory in Claude Code). `output_path` must end in the
  extension listed; it is written as the CLI writes (atomic replace,
  core DESIGN section 7), so a failed call changes no file.
- Errors in the source carry the CLI's text with `<input>` as the file
  name, `source` for inline text: `source:9:56: unknown column
  'revenu' (did you mean 'revenue'?) (columns: month, revenue, cost)`.
- Results larger than 8 MB of base64 (very large images) return the
  `text` and `structuredContent` without the image and say so.
- An OpenGL fallback when OpenGL was asked for explicitly (`--rasterizer
  opengl` or the tool's `rasterizer`) adds a text block `warning: OpenGL
  unavailable (<reason>); used the software rasterizer`, the CLI's
  warning; the default choice falls back silently.

### 5.2 Resources

| URI | MIME type | What |
|---|---|---|
| `ui://nymph/viewer` | `text/html;profile=mcp-app` | The MCP App (only when built with `NYMPH_MCP_VIEWER`) |
| `nymph://docs/agent-guide` | `text/markdown` | `docs/agent-guide.md` |
| `nymph://docs/elements` | `text/markdown` | `docs/elements.md` |

`resources/read` of any other URI is the MCP error `-32002` (resource
not found).

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
  with `cmake/NymphEmbedFile.cmake`, like the fonts, and `nymph_mcp` gets
  `NYMPH_MCP_HAS_VIEWER`. The native build cannot make it itself (it
  needs Emscripten), so a release configures the web build first and
  passes its output; a missing file is a configure error.
- `cmake/NymphRelease.cmake` (option `NYMPH_RELEASE`, default OFF):
  MinSizeRel, `NYMPH_OPENGL=OFF` and `-static` on Linux (section 8),
  `-static` with MinGW, `/MT` on MSVC (chosen in the root
  `CMakeLists.txt` before `project()`), stripped (`-s`). `nymph-dist`
  copies the executable to `<build>/dist/nymph-<version>-<os>-<arch>`
  (`.exe` on Windows; `os` linux / windows / macos, `arch` x86_64 /
  arm64; `NYMPH_DIST_NAME` overrides) and writes `<build>/dist/SHA256SUMS`
  in `sha256sum` format (`cmake/NymphChecksums.cmake`), over every file
  of `dist/`. Byte-identical output holds per build: arm64 differs from
  x86-64 by a colour level at some pixels (PLAN step 7), so the release
  test of the examples is exact on x86-64 only. The package targets of
  steps 8 to 10 (`nymph-mcpb`,
  `nymph-skill`, `nymph-plugin-sync`, which copies `SKILL.md` and
  `reference.md` and writes `checksums.txt` into `claude/plugin/`) put
  their files in `dist/` too. A release without `NYMPH_MCP_VIEWER` warns.
- Cross builds: `cmake/toolchains/mingw-w64-x86_64.cmake` and
  `cmake/toolchains/aarch64-linux-gnu.cmake`, whose
  `CMAKE_CROSSCOMPILING_EMULATOR` (Wine, QEMU) runs every test, the
  script tests included (they take it as `RUNNER`).
- The web build gains a `nymph-claude-app` target writing
  `claude/viewer.html` from `claude/app/` and `nymph-viewer.single.js`.

## 7. Constraints and Invariants

- Standard output in `nymph mcp` is the protocol only. Nothing in the
  library prints. Warnings go in the tool result (section 5.1), where
  the agent reads them, not in `notifications/message`, which clients
  log out of the model's sight (so the server declares no `logging`
  capability).
- Every tool call catches all exceptions: problems with the input or
  the source (`nymph::error`, `cli_failure`, a rendering or encoding
  `std::runtime_error`) become `isError` results with the CLI's text;
  anything else becomes `isError` "internal error: <what>". The server
  never exits because of a request.
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
