# nymph — Claude Integration: Implementation Plan

See [DESIGN.md](DESIGN.md) for the design this plan implements, and
[spec/04_web_viewer/DESIGN.md](../04_web_viewer/DESIGN.md) section 5 for
the viewer API the MCP App embeds.

Steps go from what could invalidate the design (step 1), to the server
that every package needs (steps 2-4), to the App (steps 5-6), to the
packages (steps 7-10). Every step leaves the existing native, Node and
browser tests passing unmodified. Server steps need only the native
build; App steps need the `NYMPH_WEB` build (Emscripten). Each step that
touches a Claude client records the client and its version.

## Steps

### 1. Feasibility check

*Result: done, partly.* Checked with Emscripten 6.0.12, Playwright
1.56's Chromium 141 (SwiftShader WebGL2) and Claude Code 2.1.302, using
the probe in [probe/](probe/) (kept, not throwaway, until the open
answers are in). Each page ran in a fake host iframe (`sandbox=
"allow-scripts"`, the policy as a `<meta>` CSP). Answers:
1. **Yes, by evidence short of a real client.** Under the spec's default
   policy WebAssembly and `eval` are refused and the viewer aborts
   (`CompileError`). Users quote Claude's real policy (claude.ai,
   Claude Desktop 1.34493.1 on macOS, iOS 1.260716.0;
   anthropics/claude-ai-mcp issue 40): `script-src 'self'
   'unsafe-inline' 'unsafe-eval' blob: data: https://assets.claude.ai`,
   `frame-src 'self' blob: data:`, `connect-src 'self'`. Under it
   WebAssembly, `eval`, WebGL2 and a blob worker running WebAssembly
   all work, and the single-file viewer draws `dashboard.yaml`: first
   frame 218 ms, redraw (resize) 50 ms, no violation. The `-sWASM=0`
   build (`-fexceptions`) works even under the default policy: 4.6 MB
   (1.17 MB gzip) against 2.4 MB (815 KB gzip), first frame 456 ms,
   redraw 50 ms; Emscripten warns `WASM=0` is deprecated. Anthropic has
   said nested frames stay refused on claude.ai. Also found: the viewer
   throws `Invalid URL` in an `about:srcdoc` page (default `fontBaseUrl`
   resolved against `import.meta.url`); step 5 fixes it. **Open**: a
   run in Claude Desktop (chat and Code tab), on macOS and Windows.
2. **Partly.** A 2.4 MB inline page loads in the fake host; the size
   Claude accepts and whether its sandbox saves downloads are **open**
   (a real client).
3. **Partly.** In Claude Code the model receives the tool's image
   content (`claude -p` described the dashboard correctly). Whether
   Claude Desktop shows that image to the user beside the App is
   **open**.
4. **Claude Code 2.1.302** sends `server/discover` with
   `io.modelcontextprotocol/protocolVersion: 2026-07-28` first; on
   `-32601` it sends `initialize` with `2025-11-25`, capabilities
   `roots` and `elicitation`, and **no** `io.modelcontextprotocol/ui`
   extension (the terminal shows no App). It hides app-only tools
   (`visibility: ["app"]`) from the model. Claude Desktop: **open**.
5. **Partly.** The code execution sandbox is Linux x86-64, without
   network, 5 GiB memory; Skill uploads are limited to 30 MB (the API
   docs; the claude.ai docs do not say whether compressed). The static
   x86-64 executable is 10.8 MB (3.7 MB zipped). Whether it runs on
   claude.ai and how the PNG reaches the user is **open**: the test
   Skill in `probe/skill/` answers it once uploaded.
6. **`${CLAUDE_PLUGIN_DATA}`**: Claude Code 2.1.302 expands it for
   plugins (a persistent data folder), besides `${CLAUDE_PLUGIN_ROOT}`.

DESIGN.md sections 1, 4.1, 4.2, 4.3, 4.6, 4.8, 8 and 10 were updated
with these.

**Goal:** the questions DESIGN.md leaves open are answered with
evidence, and DESIGN.md is updated before any code depends on them.

**Deliverables:**
- A throwaway MCP server (not committed; any language) with one tool
  whose `_meta.ui` resource is a page that instantiates a 1 KB
  WebAssembly module and creates a WebGL2 context, and one whose page
  inlines `nymph-viewer.single.js` and draws `examples/dashboard.yaml`.
- Answers, recorded in this step's result and folded into DESIGN.md
  sections 1 (feasibility), 3, 4.8 and 8:
  1. Do Claude Desktop (chat and Code tab) and claude.ai run
     WebAssembly and WebGL2 in an MCP App? If not, the `-sWASM=0`
     viewer build: does it build with `-fexceptions`, its size gzip,
     and the time to draw the dashboard.
  2. The largest `ui://` resource the hosts accept (the single-file
     viewer with no fonts embedded, about 3 MB), and whether the
     sandbox allows downloads (the right-click menu).
  3. Is a `render_chart`-style image content shown to the user in the
     chat in Claude Desktop and in the Code tab, alongside an App?
  4. The MCP protocol versions Claude Code and Claude Desktop send in
     `initialize`.
  5. claude.ai Skills: the upload size limit, the sandbox CPU
     architecture, whether a bundled executable runs, and how a PNG
     written there reaches the user.
  6. The data directory a Claude Code plugin may write to, if any.

**Tests:** none automated; the result lists each answer with how it was
checked (client version, screenshot or log).

### 2. JSON-RPC and the MCP session

*Result: done.* `nymph_mcp` sits on `nymph_cli` (step 3 shares its
helpers), so `app/main.cpp` dispatches `nymph mcp` and `run_cli` refuses
it in a `NYMPH_MCP=OFF` build; binary stdio on Windows was already set
in `main.cpp`. Revisions `2025-11-25`, `2025-06-18`, `2025-03-26`;
`server/discover` gets `-32601`. 22 tests in `test_mcp` (also: requests
before `initialize`, a second `initialize`, invalid UTF-8, ids, CRLF and
blank lines, an unwritable output); 452 tests pass. Claude Code 2.1.302
connects (`claude mcp list`: Connected).

**Goal:** `nymph mcp` completes the MCP handshake and answers `ping`,
`tools/list` (empty) and `resources/list` (empty) over stdio.

**Deliverables:**
- `extern/json` submodule (nlohmann/json, a release tag);
  `NYMPH_MCP` option and `nymph_ensure_submodules(extern/json)` in
  `CMakeLists.txt` (DESIGN.md section 6).
- `src/mcp/json_rpc.hpp/.cpp`, `src/mcp/mcp_session.hpp/.cpp`,
  `src/mcp/mcp_server.hpp/.cpp` (`int run_mcp(std::istream&,
  std::ostream&, std::ostream& log, options)`), the `nymph_mcp` library
  in `src/CMakeLists.txt`.
- `src/cli/cli.cpp`: the `mcp` command and its arguments; binary stdio
  on Windows.
- `docs/cli.md`: the `nymph mcp` row.

**Tests (`test_mcp`):**
- `initialize` with each supported protocol version returns it; an
  unsupported one returns the newest supported; server name and
  `version()` match.
- `ping` returns `{}`; an unknown method returns `-32601`; a malformed
  line returns `-32700` and the next line still works; a batch array
  returns `-32600`; notifications get no response.
- Every line written to output parses as one JSON object; end of input
  returns 0.
- `nymph mcp --bogus` exits 2 with the usage.

### 3. The tools

*Result: done.* File I/O, `source_of()` and the schema and font texts
moved from `cli.cpp` to `src/cli/cli_support.hpp/.cpp`; the only CLI
change is that `nymph schema <Type>` now suggests the closest type.
Warnings go in the tool result instead of `notifications/message`
(DESIGN.md section 7: the agent must see them). Unknown arguments are
refused with a suggestion. `tools/list` is checked by structure (names
in order, schema properties, output schema, annotations) rather than a
golden file, which every description edit would break. 17 tests in
`test_mcp_tools` (also base64); 469 tests pass. End to end, Claude Code
2.1.302 with only these tools drew and saved a bar chart, correcting a
wrong element name from `chart_schema`'s suggestion.

**Goal:** every tool of DESIGN.md section 5.1 except `viewer_font`
works, with errors as `isError` results.

**Deliverables:**
- `src/mcp/tools.hpp/.cpp`: the registry and the handlers for
  `render_chart`, `check_chart`, `extract_chart`, `chart_schema`,
  `list_fonts`, sharing `src/cli/` code for schema text, font lists and
  atomic writes (move a shared helper out of `cli.cpp` if needed, no
  behaviour change).
- The session's renderer and `--rasterizer`; OpenGL fallback warnings
  in the tool result (DESIGN.md section 7).

**Tests (`test_mcp_tools`):**
- `tools/list` matches a golden JSON (names, input schemas, output
  schema of `render_chart`).
- `render_chart` with `examples/bars.yaml` text: the image decodes to
  the same bytes as `render_png()` of that text (software rasterizer);
  `structuredContent` has the source, width and height.
- `render_chart` with `input_path` of a nymph PNG renders its source;
  with `output_path` writes the file; with a bad source returns
  `isError` text `source:<l>:<c>: ...` equal to the CLI's message and
  leaves an existing output file unchanged.
- Both or neither of `source` / `input_path`, an `output_path` with the
  wrong extension, and a missing file are `isError`.
- `extract_chart` round-trips a rendered PNG (path and base64); a plain
  PNG is `isError`. `chart_schema` for `PlotLine` equals `nymph schema
  PlotLine`; an unknown type suggests the nearest. `list_fonts` equals
  `nymph fonts`.
- A handler that throws (a test hook) gives `isError` and the next
  request works.

### 4. Instructions, documents and the process test

**Goal:** a client that connects learns how to use nymph from the
server itself.

**Deliverables:**
- `claude/skill/nymph/SKILL.md` (DESIGN.md section 4.5), written from
  `docs/agent-guide.md`.
- Build-time embedding of `SKILL.md` (front matter stripped) as the
  `initialize` instructions, and of `docs/agent-guide.md` and
  `docs/elements.md` as resources (DESIGN.md section 5.2).

**Tests:**
- `test_mcp`: `instructions` is non-empty and contains no front
  matter; `resources/read` of each `nymph://docs/` URI returns the
  file's bytes; an unknown URI is an error.
- `test_mcp_process`: the built executable, run as `nymph mcp` with
  pipes, answers `initialize` and a `render_chart`, and exits 0 when
  its input closes.
- Manual: `claude mcp add nymph -- <build>/app/nymph mcp` in Claude
  Code; asked for a chart, Claude calls `render_chart`, fixes a
  provoked error from the message, and describes the image. Record the
  Claude Code version.

### 5. The App page

**Goal:** `ui://nymph/viewer` shows the result of `render_chart` live in
a host that implements MCP Apps.

**Deliverables:**
- `claude/app/viewer.html.in`, `claude/app/app.js` (DESIGN.md section
  4.2, steps 1-3, 5 and 6).
- `web/js/nymph-viewer.js`: the default `fontBaseUrl` resolved only for
  the default fetcher (DESIGN.md section 4.3), with a browser test
  creating a viewer with `fetchFont` in an `about:srcdoc` frame.
- `web/CMakeLists.txt`: the `nymph-claude-app` target writing
  `<build-web>/claude/viewer.html` (the WebAssembly single-file viewer).
- `NYMPH_MCP_VIEWER` in the native build; `resources/list` and
  `render_chart`'s `_meta.ui` only when set; `viewer_font`.
- `web/tests/browser/mcp_host.html` and its test: a fixture host that
  loads `viewer.html` in an iframe sandboxed with the spec's attributes
  and CSP, and plays the host side of the protocol.

**Tests:**
- Native: without `NYMPH_MCP_VIEWER`, no resource and no `_meta.ui`;
  with a small fixture file, `resources/read` returns it with
  `text/html;profile=mcp-app` and the tool carries
  `_meta.ui.resourceUri`. `viewer_font` returns an embedded font's
  bytes and is `isError` for an unknown name; `tools/list` marks it
  app-only.
- Browser: after `ui/initialize` and a tool result, the canvas is not
  blank and matches the PNG within the viewer's tolerance (viewer PLAN
  step 10); a source naming a font produces one `viewer_font` call;
  `size-changed` reports the chart's height; a `host-context-changed`
  to dark changes the background; the page makes no network request.

### 6. The view back to Claude

**Goal:** the user's pan, zoom and clicks reach the model as DESIGN.md
section 4.4 describes.

**Deliverables:**
- `app.js`: step 4 of DESIGN.md section 4.2 (debounced
  `ui/update-model-context`), the fallback to the result's PNG on a
  viewer error, the menu rule of step 7 (from step 1's answer).
- `claude/skill/nymph/SKILL.md`: how to use the context (keep the view
  key when re-rendering).

**Tests:**
- Browser: a drag on a plot produces, after the debounce, one
  `ui/update-model-context` whose text contains a `view:` block equal to
  the `view` key of `toYaml()`; a click adds a "Last clicked" line; no
  update is sent before the user touches the chart; a source whose font
  cannot be fetched shows the PNG.
- Manual: in Claude Desktop, zoom a chart, then ask "add a trend line
  to what I'm looking at"; Claude's new chart keeps the zoom. Record
  the client version.

### 7. Release builds

**Goal:** one command produces the static executables with the App
embedded and their SHA-256 sums.

**Deliverables:**
- `cmake/NymphRelease.cmake` and the `NYMPH_RELEASE` option (DESIGN.md
  section 6).
- `docs/building.md`: the release build (web build first, then native
  with `NYMPH_MCP_VIEWER`), and cross builds for Linux arm64 and
  Windows (MSVC; MinGW under Wine as a check, as GL PLAN did).

**Tests:**
- The Linux release binary is static (`file` reports it; a CTest test
  in release builds checks the ELF has no `PT_INTERP`), and reproduces
  every example PNG with `--rasterizer software`.
- `nymph mcp` of the release binary lists the `ui://` resource.
- The Windows binary passes `test_mcp_process` (under Wine at least);
  native MSVC is recorded as verified or not.

### 8. The Claude Code plugin

**Goal:** `/plugin marketplace add binary-dice-games/nymph` and
`/plugin install nymph@nymph` give Claude the nymph tools with nothing
else to do.

**Deliverables:**
- `.claude-plugin/marketplace.json`, `claude/plugin/` (DESIGN.md
  section 4.6), the launcher for POSIX shells and Windows `cmd`.
- The `nymph-plugin-sync` target; its outputs committed.

**Tests:**
- A shell test (CTest, POSIX only) runs the launcher with `$NYMPH_BIN`
  unset and a local file served as the release asset: it downloads,
  verifies, caches and execs `nymph mcp`; a tampered asset makes it
  exit 1 with one line on standard error and leaves no cached file; a
  second run uses the cache without downloading.
- `claude plugin validate` (or the current equivalent) accepts the
  manifests.
- Manual: a fresh Claude Code installs the plugin from the repository
  and draws a chart into the project. Record the version, and the
  result of the private-repository question (DESIGN.md section 10).

### 9. The Claude Desktop bundle

**Goal:** double-clicking `nymph-<version>-<os>-<arch>.mcpb` installs
nymph in Claude Desktop, App included.

**Deliverables:**
- `claude/mcpb/manifest.json.in`, the `nymph-mcpb` target (DESIGN.md
  section 4.7).

**Tests:**
- The `mcpb` CLI's validation (`mcpb validate`) accepts the manifest,
  if available in the container; otherwise a test checks the manifest
  against the fields DESIGN.md lists.
- The zip contains the manifest with the project version and the
  executable, which runs `nymph mcp` from the extracted folder.
- Manual: Claude Desktop on Linux or Windows installs it and shows a
  live chart. Record the version.

### 10. The claude.ai Skill, documentation and close

**Goal:** claude.ai users can upload one zip, and every document
matches the code.

**Deliverables:**
- `claude/skill/` packaging and the `nymph-skill` target (DESIGN.md
  section 4.8), shaped by step 1's answers.
- `docs/claude.md`: install and use per client (the table of DESIGN.md
  section 3), what the user sees, limits; a row in README's "Further
  Documentation", and one line in the README overview.
- `docs/agent-guide.md`: a pointer to the MCP tools.
- `CHANGELOG.md`: `nymph mcp`, the App, the plugin, the bundle and the
  Skill under `[Unreleased]`.
- DESIGN.md status and any change made during implementation;
  `spec/INDEX.md` status and topics.

**Tests:**
- The Skill zip contains `SKILL.md`, `reference.md` and the executable
  for each architecture step 1 required.
- Manual: in claude.ai, with the Skill uploaded, Claude renders a chart
  and the user receives the PNG.
- `ctest -j` passes three times in a row natively and in the
  `NYMPH_WEB` build.

## Completion Criteria

- [ ] All native, Node and browser tests pass under `ctest -j`; every
      committed example PNG is still reproduced byte for byte.
- [ ] `nymph mcp` passes the session, tool and process tests; source
      errors come back as `isError` with the CLI's message.
- [ ] In Claude Code (terminal), the plugin installs with the two
      commands of DESIGN.md section 3 and Claude renders, sees and
      saves charts.
- [ ] In Claude Desktop, the `.mcpb` installs with a double-click and
      the chart is live in the conversation, or DESIGN.md says why not
      (step 1) and the PNG is shown instead.
- [ ] A user's zoom in the App reaches Claude and survives the next
      render.
- [ ] In claude.ai, the Skill renders a chart the user receives.
- [ ] README, `docs/`, CHANGELOG, the DESIGN.md files and
      `spec/INDEX.md` match the code.
