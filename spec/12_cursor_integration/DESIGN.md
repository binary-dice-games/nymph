# nymph — Cursor Integration: Design

**Status: built and tested as files; not run in Cursor.** Plan steps 2-5
are done; step 1 and every manual check need a real Cursor, which was not
available, so they are open ([PLAN.md](PLAN.md) records each step's
result). Cursor's documentation settled some questions of section 9 (marked
there); the rest stay unverified. This document
describes only what this project adds or changes. It is the Cursor
counterpart of [spec/06_claude_integration/DESIGN.md](../06_claude_integration/DESIGN.md)
(*Claude DESIGN*), whose server, App, agent text and release builds it
reuses unchanged; the rest of the architecture is
[spec/01_core_features/DESIGN.md](../01_core_features/DESIGN.md) and
[spec/04_web_viewer/DESIGN.md](../04_web_viewer/DESIGN.md).

## 1. Purpose / Scope

Make nymph a **tool Cursor's agent can use with one install step**: the
agent renders charts and sees them, the user sees them in the chat, and
where Cursor can show MCP Apps the chart is the live web viewer. Same
three requirements as Claude DESIGN section 1: frictionless (no runtime to
install, no settings to edit), the agent sees the PNG, interactive where
the client allows.

What already works, with no change: `nymph mcp` is a plain stdio MCP
server, and Cursor documents support for stdio servers, image results
(attached to the chat for image-capable models), `resources`, and the MCP
Apps extension (progressive enhancement). So the project is mostly
**packaging, agent text and verification**, not new server code.

In scope:

- A **Cursor plugin** (`cursor/plugin/`) bundling the MCP server entry,
  the nymph skill and a rule, installable from Cursor's Customize panel,
  and a repository-level `.cursor-plugin/marketplace.json`.
- A **no-plugin path** for every Cursor: a documented `mcp.json` entry, an
  "Add to Cursor" install link, and a project rule / `AGENTS.md` snippet.
- Windows launch (section 4.3), since Cursor's users are mostly on
  Windows and macOS, unlike Claude Code's.
- Verification that the MCP App runs in Cursor (section 4.4).

Out of scope: a hosted remote server (Claude DESIGN section 10 applies
unchanged); Cursor's cloud / background agents, which run in a remote VM
where a local stdio server does not exist (the plain `nymph` command from
the install script, or the skill, is what they would use; not tested);
publishing to the Cursor Marketplace (a gated step, section 8); any change
to the server, tools, source format or viewer.

New and changed files:

| Path | What |
|---|---|
| `cursor/plugin/` | The plugin: `.cursor-plugin/plugin.json`, `mcp.json`, `skills/nymph/`, `rules/nymph.mdc`, `logo.png`, launcher |
| `cursor/plugin.json.in`, `cursor/rule/nymph.mdc.in` | Manifest and rule templates (outside the plugin) |
| `.cursor-plugin/marketplace.json` | Repository as a Cursor plugin marketplace |
| `cmake/NymphPluginSync.cmake` | Also writes the Cursor plugin's copies and checksums |
| `docs/cursor.md` | Installing and using nymph in Cursor |

## 2. Design Goals

1. **One executable, one text.** The Cursor plugin carries the same
   pinned executable reference and the same agent text as the Claude
   packages, generated from `claude/skill/nymph/SKILL.md` by the existing
   sync target. Nothing is written twice by hand, so the two cannot drift.
2. **The PNG always works.** As Claude DESIGN goal 2: the App is an
   addition Cursor may ignore.
3. **No new trust.** The launcher runs only the SHA-256-pinned release
   (Claude DESIGN section 4.6); the `mcp.json` path runs only what the user
   names.
4. **Works on Windows first.** The primary Cursor install must not need
   `sh`, Git for Windows or any runtime (section 4.3).
5. **Cheap to keep.** Cursor's plugin format moves quickly; everything
   Cursor-specific is a thin manifest layer a test can validate.

## 3. Install Paths

| Path | Install | What the user gets |
|---|---|---|
| Plugin | Copy to `~/.cursor/plugins/local/nymph` and reload (any user); or install from a team marketplace (Teams / Enterprise: Import Repo) | Server, skill and rule in one step |
| Install script + `mcp.json` | `install.sh` / `install.ps1` (spec 07) puts `nymph` on `PATH`; one `mcp.json` entry, or the "Add to Cursor" link | Server (and the `nymph` command) with no `sh`; the agent text from the rule |
| Manual | Release executable, `mcp.json` with its path | Same |

The plugin is the recommended path where `sh` exists (macOS, Linux, and
Windows if step 1 confirms section 4.3); the install-script path is the
recommended one on Windows otherwise. `docs/cursor.md` states which.

## 4. Key Abstractions

### 4.1 The plugin (`cursor/plugin/`)

```
.cursor-plugin/marketplace.json     (repository root) marketplace "nymph", lists ./cursor/plugin
cursor/plugin.json.in               manifest template
cursor/plugin/
  .cursor-plugin/plugin.json        name "nymph", version = nymph version
  mcp.json                          server "nymph": the launcher, args ["mcp"]
  skills/nymph/SKILL.md, reference.md    copies of the Claude skill and docs/agent-guide.md
  rules/nymph.mdc                   the short rule (section 4.2)
  logo.png                          copy of resources/icon.png
  bin/nymph                         the launcher (section 4.3)
  bin/checksums.txt                 same format and content as the Claude plugin's
```

Cursor detects the format from the manifest and finds components in
default directories. Cursor's skills format is the Agent Skills format
(`SKILL.md` with `name` matching the folder and `description`), which the
Claude skill already is, so the copy is byte-identical; a test checks it.
`nymph-plugin-sync` writes the Cursor copies in the same pass as the
Claude plugin's and `nymph_plugin_in_sync` covers both.

`bin/` and `checksums.txt` are shared in content with `claude/plugin/bin/`.
They are copied, not symlinked (a plugin installs from a git checkout and
Windows checkouts do not reliably keep symlinks).

### 4.2 Agent text: skill and rule

The skill (copy of `SKILL.md`) teaches the workflow. Cursor decides whether
a skill applies from its `description`, which can miss a request such as
"plot this CSV". The rule closes that gap: `rules/nymph.mdc` is a few lines
with `alwaysApply: false` and a `description` ("Use when the user asks for
a chart, plot, diagram, flowchart, Gantt chart, Sankey diagram or UI
mockup; draw it with the nymph tools"), pointing at the `nymph` skill and
the MCP server's own instructions. An always-applied rule would cost
context in every chat, so it is not used. The rule's text is a template
(`cursor/rule/nymph.mdc.in`), copied by the sync target; a test checks its
front matter and that it names the tools.

For the no-plugin path, `docs/cursor.md` gives the same rule as a file to
drop into `.cursor/rules/` and a two-line `AGENTS.md` snippet. The MCP
server's `instructions` (Claude DESIGN section 4.5) already carry the full
workflow; whether Cursor passes them to the model is checked in step 1,
and the rule exists because it may not.

### 4.3 The launcher and Windows

The Claude plugin's launcher is one POSIX `sh` script run as `sh
${CLAUDE_PLUGIN_ROOT}/bin/nymph`. Cursor's plugin reference says it expands `${CURSOR_PLUGIN_ROOT}` (and
`${CLAUDE_PLUGIN_ROOT}`) in `command`, `args`, `env` and `cwd`, and shows a
plugin pointing at its own `bin/` executable. So the plugin-root variable
exists and the Claude launcher is reused as is: `mcp.json` runs `sh
${CURSOR_PLUGIN_ROOT}/bin/nymph mcp`. Whether a stock Windows Cursor finds
a `sh` is **unknown** (step 1, not run), and no PowerShell launcher is
built, since without a way to select it per platform (also unknown) it
could not be wired in. Decision, by outcome:

- If `sh` runs on a stock Windows Cursor: the launcher is reused as is
  (the built state).
- If a plugin-root variable exists but there is no `sh` on Windows: the
  plugin adds a PowerShell launcher (`bin/nymph.ps1`, same steps and
  checks) and the manifest selects by platform if Cursor allows it, else
  the plugin is documented as macOS / Linux and Windows uses the install
  script path.
- (Not the case: the plugin-root variable exists.) The no-plugin path
  uses `nymph` from `PATH` (install script) or `${env:NYMPH_BIN}`.

The install-script path needs no launcher at all: `install.ps1` copies
the checksum-verified release executable to a folder on `PATH`, and the
entry is `{"command": "nymph", "args": ["mcp"]}`. It is therefore the
**fallback that always works**, and what the "Add to Cursor" link installs.
It means a Windows user runs one PowerShell line (spec 07) before the
link; the docs say so.

### 4.4 The MCP App in Cursor

Cursor lists MCP Apps as supported but documents neither where `ui://`
views render, their CSP or sandbox, nor a minimum version. The App needs
WebAssembly (Claude DESIGN section 1, feasibility: `'unsafe-eval'` or
`'wasm-unsafe-eval'`), and Cursor is an Electron app that may apply the
MCP Apps default policy, under which the viewer cannot start. So the App
is an **unverified addition**, as it was for Claude before its step 1. The
step 1 probe (the existing `spec/06_claude_integration/probe/` server, run
from Cursor) answers: does the widget appear, do WebAssembly, WebGL2 and a
blob worker run, how large a `ui://` resource is accepted, does
`ui/update-model-context` reach the model, what `hostContext` and display
modes are offered.

Outcomes: (a) it works: the Claude App is reused byte for byte and the
docs say so; (b) the host's CSP blocks WebAssembly: Cursor gets the PNG
(which `render_chart` always returns) and this design reopens the
`-sWASM=0` viewer build rejected in Claude DESIGN section 8, as the answer
for a strict host, in a follow-up project; (c) widgets do not render in
chat at all: same as (b) without the CSP work. A host that blocks the App
needs no server change: it ignores `_meta.ui`.

If the host's bridge differs from the one `app.js` implements (for
example a different `hostContext` field or no `ui/update-model-context`),
the difference is handled inside `app.js` (feature checks, already how it
treats display modes), never by forking the App.

## 5. Public Contract

No new tools, resources or commands. `nymph mcp` and its tools are as in
Claude DESIGN section 5. Added:

- Manifest `.cursor-plugin/plugin.json`: `name: "nymph"`, `description`,
  `version` equal to the pinned release's (0.0.0 until one is pinned, as
  the Claude plugin's), `author`, `logo` and the component paths `rules/`,
  `skills/`, `mcp.json` (a component path replaces folder discovery). `nymph_plugin_in_sync` fails on a stale
  version.
- `.cursor-plugin/marketplace.json`: `name`, `owner`, `metadata`, and the
  plugin entry with `source: "cursor/plugin"` (a plain relative path, as
  Cursor's own marketplace writes it; the Claude one uses `./`).
- The "Add to Cursor" link and `mcp.json` snippet in `docs/cursor.md`, by
  hand: the entry names no version, so nothing in it can go stale. The link
  format is the one Cursor documents (`cursor://anysphere.cursor-deeplink/mcp/install?name=...&config=<base64>`); not yet tried.

## 6. Build

No new CMake option and no new executable. `NymphPluginSync.cmake` gains
a second output directory (`cursor/plugin/`) and the `plugin.json.in`
substitution. The release workflow's plugin-pin job (spec 07) updates
`cursor/plugin/bin/checksums.txt` together with the Claude plugin's, in
the same pull request.

## 7. Constraints and Invariants

- Nothing under `src/` changes; the Cursor project adds no `#ifdef`.
- The Cursor plugin's `checksums.txt` and `SKILL.md` are byte-identical to
  the Claude plugin's; one test asserts it.
- A launcher never runs an executable whose SHA-256 differs from the
  pinned one (Claude DESIGN section 7), whichever shell runs it.
- The rule is `alwaysApply: false`; no always-on rule is added to the
  user's projects by the plugin.
- Files Cursor treats as user configuration (`.cursor/mcp.json`,
  `.cursor/rules/`) are never written by nymph; the docs show what to add.

## 8. Design Decisions

- **Plugin plus a no-plugin path**, not the plugin alone. Cursor's plugin
  format is new and its Windows behavior unconfirmed; the install script
  and `mcp.json` need only features Cursor documents today.
- **Reuse the server and App unchanged**, not a Cursor-specific build.
  The protocol is the same, Cursor documents progressive enhancement, and a
  fork would add the second version Claude DESIGN section 8 rejects.
- **Rule with a description, not always-on.** Cost in every chat for a
  tool few chats use; the description trigger matches how Cursor selects
  rules, and the skill covers the details.
- **Copy, do not link, shared files** between `claude/plugin/` and
  `cursor/plugin/`, generated and tested, so each installs standalone.
- **Do not publish to the Cursor Marketplace in this project.** Listing is
  outward-facing, reviewed by Cursor, and waits on the owner and on a
  published release (the repository is private; spec 07). The plan ends at
  a plugin installable from the repository as a team marketplace.
- **Rejected: a Cursor extension (VS Code `.vsix`)** wrapping the
  executable. It would add an extension runtime and a second release
  artifact for what `mcp.json` already does.

## 9. Open Questions (answered in PLAN step 1)

1. Where do `ui://` views render in Cursor, under what CSP, and does
   WebAssembly run? (section 4.4) *Open: the docs say nothing.*
2. Does Cursor pass the server's `initialize` `instructions` to the
   model? Does it show image content to the user as well as the model?
3. The plugin manifest and `marketplace.json` schemas, the plugin-root
   variable for `mcp.json`, and whether a plugin may carry `bin/`.
   *Answered from https://cursor.com/docs/reference/plugins (2026-10-20):
   schemas as in section 5, `${CURSOR_PLUGIN_ROOT}`, a local executable is
   shown in an example; not yet confirmed by installing.*
4. Does `sh` run on a stock Windows Cursor, and does `mcp.json` allow
   per-platform commands?
5. Minimum Cursor version for each feature, and the exact "Add to Cursor"
   link format.
6. Protocol revision and capabilities Cursor sends in `initialize`.

## 10. Testing Strategy

- `nymph_plugin_in_sync` extended: Cursor copies, manifest version and
  checksums match their sources; `SKILL.md` and `checksums.txt` equal the
  Claude plugin's.
- Manifest checks: `plugin.json`, `marketplace.json` and `mcp.json` parse
  and contain the fields step 1 lists; the rule's front matter is valid
  and `alwaysApply` is false.
- Launcher: the `nymph_plugin_launcher` script is run against the Cursor
  copy (`nymph_cursor_plugin_launcher`).
- Manual, recorded with Cursor's version per step: stdio server connects;
  the agent renders and sees a chart; the plugin installs; the App
  (section 4.4); the Windows paths.
