# Cursor checks: what to run and record

The short list. The commands, paths and expected output are in
[RUNBOOK.md](RUNBOOK.md) (Windows, PowerShell).

For [PLAN.md](PLAN.md) steps 1, 2 and 4, which need a real Cursor. Do the
parts in order; each says what to write down. Paste the results into the
plan's step results and fix [DESIGN.md](DESIGN.md) sections 4.3, 4.4 and 9
where an answer differs. Record the **Cursor version** (Help > About) and
the OS with every answer.

Needs: Cursor on Windows (repeat parts B and C on macOS or Linux if you
have one), Python 3 (part A only), and a nymph executable with the App
embedded: a release build (`docs/building.md`, "Release executables") or
a native build configured with `NYMPH_MCP_VIEWER`. Put its folder on
`PATH`, or use its full path below. The MCP log is in Cursor's Output
panel (Ctrl+Shift+U), channel "MCP Logs".

## A. Probe: the App, instructions and protocol (step 1, questions 1, 2, 6)

1. Build the web viewer (`docs/building.md`, web viewer) to get
   `<build-web>/web/nymph-viewer.single.js`.
2. In `~/.cursor/mcp.json` add (paths absolute, forward slashes):

   ```json
   { "mcpServers": { "nymph-probe": { "type": "stdio", "command": "python", "args": [
     "<repo>/spec/06_claude_integration/probe/probe_server.py",
     "--log", "<somewhere>/probe.log",
     "--png", "<repo>/examples/dashboard.png",
     "--yaml", "<repo>/examples/dashboard.yaml",
     "--viewer", "<build-web>/web/nymph-viewer.single.js"] } } }
   ```
3. Restart Cursor, approve the server, and ask the agent: "Run
   probe_sandbox, then probe_viewer."
4. Record, from the chat and `probe.log` (`"kind": "report"` lines):
   - [ ] Does a widget appear in the chat? Where (inline, side panel)?
   - [ ] `wasm_compile`, `webgl2` and the blob-worker results; `violations`
         (the CSP in force).
   - [ ] Does the viewer draw `dashboard.yaml` and move on drag? Time to
         first frame, if the log gives it.
   - [ ] Is the PNG shown to you in the chat, and does the agent describe
         it correctly?
   - [ ] The `initialize` message in `probe.log`: `protocolVersion`, and
         whether `capabilities` lists `io.modelcontextprotocol/ui`.
   - [ ] The host context the page gets (theme, display modes) and whether
         the page's `ui/update-model-context` reaches the agent (ask it
         "what did I change in the chart?" after a drag).
   - [ ] Question 2 (do `instructions` reach the model?) is answered in
         part B, since the probe's instructions are one short sentence.

   **Outcome:** App works (reuse as is), WebAssembly blocked (PNG only;
   reopen the `-sWASM=0` build), or no widgets (PNG only).

## B. The no-plugin path (step 2)

Remove the probe from `mcp.json`. Install nymph (`install.ps1`, or copy the
executable and set `NYMPH_BIN`), then add the entry from
[docs/cursor.md](../../docs/cursor.md#install-script-and-mcpjson).

- [ ] The server shows as connected in Cursor's MCP settings; the tool list
      has `render_chart`, `check_chart`, `extract_chart`, `chart_schema`,
      `list_fonts` and not `viewer_font`.
- [ ] Ask: "plot the numbers 3, 5, 4, 9 as a bar chart". The agent calls
      `render_chart` and describes the image it got.
- [ ] Provoke an error: "draw a PlotLinee chart". The agent reads the
      `<line>:<column>` message, fixes the source and tries again.
- [ ] "Save it as bars.png": the file exists and `nymph bars.png` prints
      the YAML back.
- [ ] Question 2: with **no** rule and no skill, ask "what rules does the
      nymph server give you for writing a chart?". If it quotes SKILL.md's
      rules (the server's `instructions`), Cursor passes them on; if not,
      it does not, and the rule matters more.
- [ ] With **no** rule and no skill: does the agent reach for nymph on
      "make me a chart" unprompted? Then add the rule from
      `docs/cursor.md` as `.cursor/rules/nymph.mdc` and ask again.
- [ ] The "Add to Cursor" link in `docs/cursor.md`: does it open Cursor
      and add the entry? If not, correct or remove the link.

## C. The plugin (step 1 questions 3, 4; step 4)

The release is not published, so serve a local one. Build a release
executable, make `SHA256SUMS`, and run `nymph-plugin-sync` with
`NYMPH_PLUGIN_SUMS` (`docs/building.md`) on a **scratch copy** of the
repository; do not commit that pin. Set the environment variable
`NYMPH_RELEASE_URL=file:///<folder with the release files>` before
starting Cursor. If the pin is awkward, set `NYMPH_BIN` instead and skip
the download rows.

1. Push the scratch copy to a repository you own, and add it in Cursor as
   a team marketplace (Dashboard > Plugins & MCPs > Add Marketplace >
   Import Repo), or use whatever local install Cursor offers. Record
   which route worked.
2. Customize panel: install `nymph`.
   - [ ] The plugin appears and installs. Marketplace and plugin manifest
         accepted (any warning text: copy it).
   - [ ] The `nymph` server starts: on macOS or Linux; **on Windows**,
         does `sh` resolve? If not, copy the MCP log line. (That decides
         DESIGN.md section 4.3: Windows then uses part B's path.)
   - [ ] On first start the launcher downloaded, verified and cached the
         executable in `~/.cache/nymph/<version>/` (on Windows, under
         Git Bash's home).
   - [ ] The skill and the rule are listed in Customize.
3. Repeat part B's chart, error and save rows with the plugin.
4. If the App ran in part A, ask for a chart, drag-zoom it, then say "add a
   trend line to what I am looking at". Record whether the new chart keeps
   the zoom.
5. Questions 3 and 5 of DESIGN.md section 9:
   - [ ] Was `logo.png` used? Any manifest field Cursor rejected or ignored?
   - [ ] The minimum Cursor version for plugins and for MCP Apps (release
         notes or the version where it first worked).

## Reporting

For each unchecked or surprising row, write the exact text of the message
or screenshot. Then update PLAN.md steps 1, 2 and 4 (results), DESIGN.md
section 9 (mark each answer), `docs/cursor.md` (drop *unverified* where
confirmed, correct where not), and the completion criteria. Delete nothing
from the probe folder; the Claude project still lists it.
