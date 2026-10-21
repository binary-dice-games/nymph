# nymph — Cursor Integration: Implementation Plan

See [DESIGN.md](DESIGN.md) for the design this plan implements. The
server, App and release builds are from
[spec/06_claude_integration](../06_claude_integration/DESIGN.md) and need
no change; the install scripts and release workflow are
[spec/07_release_pipeline](../07_release_pipeline/DESIGN.md).

Steps go from what could invalidate the design (step 1), to the paths that
need no plugin (step 2), to the plugin (steps 3-4), to the documents
(step 5). Every step leaves the existing tests passing unmodified. Each
step that touches Cursor records its version. Nothing is published
anywhere by this plan.

## Steps

### 1. Feasibility check in a real Cursor

**Goal:** the questions of DESIGN.md section 9 are answered with evidence,
and DESIGN.md is updated before any file depends on them.

**Deliverables:**
- Using the existing [probe](../06_claude_integration/probe/) as a custom
  stdio server (`.cursor/mcp.json`) and a release or build of `nymph mcp`
  (with the App embedded), on Windows and, if available, macOS or Linux:
  answers to DESIGN.md section 9, items 1 to 6, each with how it was
  checked (Cursor version, screenshot, MCP log from the Output panel).
- A scratch plugin (not committed) with a skill, a rule and an `mcp.json`
  entry, installed from a local team-marketplace import or the Customize
  panel, to find the manifest schemas, the plugin-root variable and
  whether `bin/` is kept.
- The results written into this step and folded into DESIGN.md sections 3,
  4.3, 4.4 and 9 (which launcher variant, which App outcome).

**Tests:** none automated; the result lists each answer with its evidence.

*Result: not done; needs a real Cursor (none here).* Answered from
Cursor's documentation only, 2026-10-20 (cursor.com/docs/context/mcp,
/context/rules, /skills, /plugins, /reference/plugins): stdio servers,
image results, resources and MCP Apps are supported; `mcp.json` has
`${workspaceFolder}`, `${userHome}`, `${env:NAME}`; plugins use
`.cursor-plugin/plugin.json` (only `name` required, component paths
replace discovery) and `.cursor-plugin/marketplace.json` (`name`, `owner`,
`plugins`), expand `${CURSOR_PLUGIN_ROOT}`, and the docs show a plugin
running a local `bin/` executable; skills are Agent Skills (`name`
matching the folder, `description`); rules are `.mdc` with `description`,
`alwaysApply`, `globs`. **Still open, for a run in Cursor:** where `ui://`
views render and whether WebAssembly runs; whether `instructions` reach
the model; whether `sh` runs on Windows; the deeplink format; minimum
versions; the protocol Cursor sends. A second read of cursor.com/docs/plugins (2026-10-20) added: local plugins load from `~/.cursor/plugins/local/<name>` (any user; reload the window), "Import Repo" marketplaces are a Teams / Enterprise feature, and the MCP install deeplink format is documented. DESIGN.md sections 4.3, 5 and 9 were
updated with the answered ones.

### 2. The no-plugin path

**Goal:** a Cursor user without the plugin gets working tools from the
install script and one `mcp.json` entry, on Windows as well.

**Deliverables:**
- `docs/cursor.md` (first version): the `mcp.json` entry for `nymph` on
  `PATH` and for `${env:NYMPH_BIN}`, the "Add to Cursor" link, the rule
  and `AGENTS.md` snippets (DESIGN.md sections 3, 4.2).
- `cursor/rule/nymph.mdc` template and its generation from
  `claude/skill/nymph/SKILL.md`'s description, if step 1 shows the
  instructions do not reach the model (otherwise the rule stays a
  shorter pointer).

**Tests:**
- A test generates the rule and checks its front matter (`description`,
  `alwaysApply: false`, no `globs`) and that it names the skill's trigger
  words.
- Manual (Cursor, recorded): with the entry, the agent calls
  `render_chart`, fixes a provoked source error from the message, and
  describes the image; on Windows with `install.ps1`.

*Result: done except the manual check, not run (no Cursor).*
`docs/cursor.md` has the entry, the link (format unverified), the rule and
the `AGENTS.md` snippet. The rule is a fixed template,
`cursor/rule/nymph.mdc.in`, a short pointer written whatever step 1 finds
about `instructions`; `nymph_cursor_plugin_valid` checks its front matter
(`description`, `alwaysApply: false`, no `globs`) and that it names
`render_chart` and `chart_schema`.

### 3. The plugin files and sync

**Goal:** `nymph-plugin-sync` writes a complete, valid Cursor plugin that
cannot drift from the Claude one.

**Deliverables:**
- `cursor/plugin.json.in`, `.cursor-plugin/marketplace.json`,
  `cursor/plugin/` (DESIGN.md section 4.1) with the launcher variant
  step 1 chose; `NymphPluginSync.cmake` writing the Cursor copies,
  version and checksums.
- `docs/building.md`: the sync target now covers both plugins (one line).

**Tests:**
- `nymph_plugin_in_sync` fails on a stale Cursor copy, version or
  checksum; a test asserts `SKILL.md` and `checksums.txt` equal the Claude
  plugin's.
- Manifest parse tests (DESIGN.md section 10).
- `nymph_plugin_launcher` runs on the Cursor launcher copy: download from
  a local file, verify, cache, exec; a tampered asset exits 1 with one
  line and leaves no cached file.

*Result: done.* `NymphPluginSync.cmake` writes the Cursor plugin after the
Claude one, from the same pin; the launcher, `SKILL.md`, `reference.md`
and `checksums.txt` are copies, the manifest and rule come from
`cursor/plugin.json.in` and `cursor/rule/nymph.mdc.in`, `logo.png` is
`resources/icon.png`. The launcher is the Claude one, run through
`${CURSOR_PLUGIN_ROOT}`; no PowerShell launcher (DESIGN.md section 4.3).
The release workflow's pin pull request now commits the Cursor plugin's
manifest and `checksums.txt` too. Tests: `nymph_plugin_in_sync` and
`nymph_plugin_sync_versions` cover both plugins, `nymph_cursor_plugin_valid`
(manifest, `mcp.json`, marketplace, rule, skill),
`nymph_cursor_plugin_launcher` (13 checks; run under MSYS2 here, all pass;
the same script fails 5 of them under Git Bash on Windows for the Claude
launcher too, a path problem of that shell, not of the plugin).

### 4. Install the plugin in Cursor

**Goal:** the plugin installs from the repository and the agent draws a
chart.

**Deliverables:**
- Fixes found while installing (manifest, paths, launcher).
- The App outcome of step 1 implemented: reused as is, or recorded as
  PNG-only in `docs/cursor.md` with the reopened `-sWASM=0` question noted
  in DESIGN.md section 4.4. Any `app.js` feature check needed for
  Cursor's host (DESIGN.md section 4.4) with a browser test in the
  existing fixture host.

**Tests:**
- Browser (only if `app.js` changed): the fixture host omits the
  capability and the App still draws and sends no error.
- Manual (Cursor, recorded per OS): installed from a team-marketplace
  import of the repository, with a scratch pinned local release
  (`NYMPH_RELEASE_URL=file://...`); the server connects; the agent draws
  and saves a chart into the project; the live chart appears or the PNG
  does, as step 1 predicted; a zoom reaches the model if the App runs.

*Result: not done; needs a real Cursor (none here).* No fixes from an
install could be found, and the App outcome is unknown, so `app.js` is
unchanged and `docs/cursor.md` says the live chart is unverified and that
the PNG is what every result carries. The two manual checks (install from
a team-marketplace import with a local release; the App and a zoom) are
open.

### 5. Documentation and close

**Goal:** every document matches the code.

**Deliverables:**
- `docs/cursor.md` final: install per path, what the user sees, limits
  (cloud agents, App status, Windows path).
- README: one line in the overview and a row for `docs/cursor.md` in
  "Further Documentation", and a mention in the `claude/` / plugin row
  of the layout table if the files live elsewhere; `docs/claude.md`: a
  pointer to `docs/cursor.md` for other agents.
- `CHANGELOG.md` under `[Unreleased]`, `### Added`: the Cursor plugin and
  its install paths.
- DESIGN.md status and any change made during implementation;
  `spec/INDEX.md` status and topics.

**Tests:**
- `ctest -j` passes three times in a row natively.
- Links in `docs/cursor.md` resolve (the docs link check, if one exists).

*Result: done, except the repeated test run.* `docs/cursor.md` is written
(install per path, the rule, limits, each unverified claim marked); README
has the overview line, the `cursor/` layout row and two documentation
rows; `docs/claude.md` points to it; `docs/building.md` and
`docs/releasing.md` say the sync and the pin pull request cover both
plugins; CHANGELOG has the entry; `spec/INDEX.md` has the project's row.
`ctest` ran once in the existing Windows (MSVC, Debug) build: the new and
changed plugin tests pass; 13 other tests fail (fonts, examples, the
element pages) because that build predates the move of the fonts into
`resources/` (its libraries are from 2026-10-18), and 2 more
(`nymph_plugin_validate_*`) cannot start Claude Code's CLI from CTest on
Windows. A rebuild and three runs in a row are not done.

## Completion Criteria

- [ ] Step 1's answers are recorded and DESIGN.md matches them. *From
      Cursor's documentation only; the run in Cursor is open.*
- [ ] With the install script and one `mcp.json` entry, Cursor's agent
      renders, sees and saves charts, on Windows and one other OS.
- [ ] The plugin installs in Cursor from the repository and does the
      same; or DESIGN.md says why not and the no-plugin path is the
      documented one.
- [ ] The App runs in Cursor, or DESIGN.md says why not and the PNG is
      what the user sees.
- [x] The Cursor plugin's copies, version and pins are tested against
      their sources; all existing tests pass unmodified. *Plugin tests
      pass; the full suite needs a rebuild first (step 5).*
- [x] README, `docs/`, CHANGELOG, DESIGN.md and `spec/INDEX.md` match the
      code. (Cursor Marketplace listing is not a criterion: it is the
      owner's step.)
