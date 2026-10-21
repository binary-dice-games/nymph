# Cursor checks: step-by-step instructions

The detailed version of [CHECKLIST.md](CHECKLIST.md), for PLAN steps 1, 2
and 4. Written for Windows 11 and PowerShell, from the repository root
(`c:\github\nymph`), because that is the first target. For macOS or Linux
the same steps apply with `sh` commands and `~` paths; the Windows-only
risks are marked **W**.

Time: about 1 hour, most of it the first build. Nothing here publishes
anything or touches the repository: scratch files live in
`C:\nymph-test`, and the only files outside it that change are
`%USERPROFILE%\.cursor\mcp.json` and
`%USERPROFILE%\.cursor\plugins\local\`, both undone in "Cleaning up".

Write each answer down as you go (the table in "Reporting" lists them).

## 0. Prepare

### 0.1 Facts to record first

- Cursor version: Help > About. Plan (Free, Pro, Teams): Settings > Account.
- Windows version: `winver`.

### 0.2 A nymph executable with the App embedded

`build-web\claude\viewer.html` already exists in this checkout. If it does
not, build it first (Emscripten, `docs/building.md`, "Web viewer").

```powershell
$repo = 'c:\github\nymph'
Set-Location $repo
cmake -S . -B build-cursor -DNYMPH_RELEASE=ON "-DNYMPH_MCP_VIEWER=$repo\build-web\claude\viewer.html"
cmake --build build-cursor --config MinSizeRel
Get-ChildItem build-cursor\dist
```

Expect `nymph-0.1.0-windows-x86_64.exe` and `SHA256SUMS` in
`build-cursor\dist`. (A native MSVC release build has not been verified;
if it fails to configure or link, build with MSYS2 instead, or configure
`build` with `-DNYMPH_MCP_VIEWER=...` and use its `Debug` executable;
only `NYMPH_BIN` below changes.)

Install it and check it:

```powershell
New-Item -ItemType Directory -Force C:\nymph-test\bin, C:\nymph-test\release | Out-Null
Copy-Item build-cursor\dist\* C:\nymph-test\release\
Copy-Item build-cursor\dist\nymph-0.1.0-windows-x86_64.exe C:\nymph-test\bin\nymph.exe
C:\nymph-test\bin\nymph.exe --version
```

Prove the server and the App work before Cursor is involved. This sends
`initialize` and `tools/list` and prints the reply lines:

```powershell
$msgs = '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-11-25","capabilities":{},"clientInfo":{"name":"t","version":"0"}}}',
        '{"jsonrpc":"2.0","method":"notifications/initialized"}',
        '{"jsonrpc":"2.0","id":2,"method":"resources/list"}'
$msgs | C:\nymph-test\bin\nymph.exe mcp
```

Pass: two JSON lines; the second lists `ui://nymph/viewer`. If it does
not, the build lacks the App (check `NYMPH_MCP_VIEWER`); part A still
works, but part B shows only the PNG.

### 0.3 Cursor's MCP file

The file is `%USERPROFILE%\.cursor\mcp.json` (user level) or
`<project>\.cursor\mcp.json`. If it exists, copy it first:

```powershell
$mcp = "$env:USERPROFILE\.cursor\mcp.json"
if (Test-Path $mcp) { Copy-Item $mcp "$mcp.bak" }
```

Edit it with an editor; use forward slashes in paths inside JSON. After
each edit, **fully quit Cursor** (tray icon too) and start it again, or
use Settings > Tools & MCP to toggle the server. MCP output is in the
Output panel (Ctrl+Shift+U), channel **MCP Logs**.

### 0.4 A test project

```powershell
New-Item -ItemType Directory -Force C:\nymph-test\project | Out-Null
```

Open `C:\nymph-test\project` as the Cursor workspace for all parts.

## A. Probe: the App and the protocol (step 1)

**Question:** does an MCP App render in Cursor, and may it run
WebAssembly?

Python is needed. In PowerShell run `python --version`: if it opens the
Microsoft Store or prints nothing, use `py -3` or the full path of a real
Python in `command` below (the `WindowsApps` entry on `PATH` is a stub).

1. Edit `mcp.json`:

   ```json
   {
     "mcpServers": {
       "nymph-probe": {
         "type": "stdio",
         "command": "python",
         "args": [
           "c:/github/nymph/spec/06_claude_integration/probe/probe_server.py",
           "--log", "C:/nymph-test/probe.log",
           "--png", "c:/github/nymph/examples/dashboard.png",
           "--yaml", "c:/github/nymph/examples/dashboard.yaml",
           "--viewer", "c:/github/nymph/build-web/web/nymph-viewer.single.js"
         ]
       }
     }
   }
   ```
2. Restart Cursor. In Settings > Tools & MCP the server `nymph-probe`
   should show a green dot and its tools (`probe_sandbox`,
   `probe_viewer`, ...). If red, read MCP Logs.
3. In Agent chat: **"Run probe_sandbox, then probe_viewer."** Approve each
   tool call.
4. Look at the chat. Record: widget visible? (yes/no), where; does the
   dashboard chart draw, and does dragging inside it pan it?
5. Read the log:

   ```powershell
   Get-Content C:\nymph-test\probe.log | ForEach-Object { $_ | ConvertFrom-Json } |
     Where-Object { $_.kind -eq 'report' } | ConvertTo-Json -Depth 6
   ```

   Record `wasm_compile`, `webgl2`, the blob-worker result and
   `violations`. Find the `initialize` line (`Select-String initialize
   C:\nymph-test\probe.log`) and record `protocolVersion`, the
   `capabilities` object (does it contain `io.modelcontextprotocol/ui`?)
   and `clientInfo`.
6. Ask the agent: "Did the probe_viewer chart show host context such as
   theme or display modes?" and read the widget's own text; record what
   the host offered.
7. Decide the outcome and write it down: **(a)** the chart draws:
   the App is reused as is; **(b)** the widget appears but WebAssembly or
   WebGL2 fails: PNG only, and the `-sWASM=0` viewer is the follow-up;
   **(c)** no widget: PNG only.

Then remove `nymph-probe` from `mcp.json` (or restore `mcp.json.bak`).

## B. The no-plugin path (step 2)

1. Edit `mcp.json` (the entry from `docs/cursor.md`, with `NYMPH_BIN`
   style full path, so no `PATH` change is needed):

   ```json
   {
     "mcpServers": {
       "nymph": { "type": "stdio", "command": "C:/nymph-test/bin/nymph.exe", "args": ["mcp"] }
     }
   }
   ```
2. Restart Cursor. Pass: Settings > Tools & MCP shows `nymph`, green,
   with exactly five tools: `render_chart`, `check_chart`,
   `extract_chart`, `chart_schema`, `list_fonts` (`viewer_font` is
   app-only and must not be listed). Record the tool count and any
   warning in MCP Logs.
3. **Chart.** Chat: "plot the numbers 3, 5, 4, 9 as a bar chart".
   Pass: the agent calls `render_chart`, a PNG is attached to the chat,
   and the agent describes it correctly. Record: did you see the image?
   Did a live widget appear (part A's outcome, now with the real App)?
4. **Error.** "draw a PlotLinee chart of 1, 2, 3". Pass: the agent gets an
   error naming `source:<line>:<column>` and a suggestion, corrects the
   element name and renders. Record the number of attempts.
5. **Save.** "save it as bars.png in this project". Pass:

   ```powershell
   Set-Location C:\nymph-test\project
   Get-Item bars.png
   C:\nymph-test\bin\nymph.exe bars.png     # prints the YAML back
   ```
6. **Instructions reach the model?** Start a new chat. Ask: "What rules
   does the nymph server give you for writing a chart? Quote one." If it
   quotes the SKILL.md rules (for example about quoting colours or
   `description`), Cursor passes `instructions` on. If it says it has
   none, it does not: record "no".
7. **Rule needed?** In a new chat, ask only: "make me a line chart of
   monthly revenue 12, 15, 11, 18". Record whether the agent uses nymph
   unprompted. Then create `C:\nymph-test\project\.cursor\rules\nymph.mdc`
   with the rule text from `docs/cursor.md` ("Teaching the agent"), open
   a new chat and ask again. Record whether it now does.
8. **Install link.** Paste into the browser address bar (or Win+R) the
   link of `docs/cursor.md`:
   `cursor://anysphere.cursor-deeplink/mcp/install?name=nymph&config=eyJjb21tYW5kIjoibnltcGgiLCJhcmdzIjpbIm1jcCJdfQ%3D%3D`
   Pass: Cursor opens and offers to add `nymph`. Do not accept if you
   already have the entry. This entry names `nymph` on `PATH`, so it
   works only after `C:\nymph-test\bin` is on `PATH` (optional:
   `$env:Path += ';C:\nymph-test\bin'` before starting Cursor from that
   same shell). Record whether the link opens Cursor at all.

Then remove the `nymph` entry (or restore `mcp.json.bak`) before part C.

## C. The plugin (steps 1 and 4)

Cursor installs a plugin from a team marketplace (Teams / Enterprise:
Import Repo) or from a local folder. Local is used here. The plugin
downloads its executable on first start, so set up a local "release".

### C.1 Serve a local release

```powershell
Start-Process python -ArgumentList '-m','http.server','8000','--bind','127.0.0.1','--directory','C:\nymph-test\release'
(Invoke-WebRequest http://127.0.0.1:8000/SHA256SUMS -UseBasicParsing).StatusCode   # 200
```

### C.2 Make a pinned scratch copy of the plugin

Pinning edits the plugin's checksums and version; do it on a copy, never
in the repository.

```powershell
$ver = '0.1.0'
$scratch = 'C:/nymph-test/scratch'
Remove-Item -Recurse -Force $scratch -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force "$scratch/docs", "$scratch/cmake" | Out-Null
robocopy c:\github\nymph\claude  C:\nymph-test\scratch\claude  /E /NFL /NDL /NJH /NJS | Out-Null
robocopy c:\github\nymph\cursor  C:\nymph-test\scratch\cursor  /E /NFL /NDL /NJH /NJS | Out-Null
Copy-Item c:\github\nymph\docs\agent-guide.md  C:\nymph-test\scratch\docs\
Copy-Item c:\github\nymph\cmake\NymphPluginSync.cmake C:\nymph-test\scratch\cmake\
cmake "-DSOURCE_DIR=$scratch" "-DVERSION=$ver" "-DSUMS=C:/nymph-test/release/SHA256SUMS" -P C:/nymph-test/scratch/cmake/NymphPluginSync.cmake
Get-Content C:\nymph-test\scratch\cursor\plugin\bin\checksums.txt
(Get-Content C:\nymph-test\scratch\cursor\plugin\.cursor-plugin\plugin.json) -match '"version":"0.1.0"'
```

(`robocopy` exit codes below 8 are success; ignore its status.) Pass:
`checksums.txt` has one line, `nymph-0.1.0-windows-x86_64.exe`, and the
manifest says version 0.1.0.

### C.3 Check the launcher by hand (**W**)

Cursor will run `sh <plugin>/bin/nymph mcp`. Find out what `sh` is:

```powershell
where.exe sh
```

- **No result**: Cursor will most likely fail to start the server, since
  Git for Windows by default puts only `Git\cmd` on `PATH`, not
  `Git\usr\bin` where `sh.exe` lives. Record this: it is the answer to
  question 4, and the Windows plugin is then not usable without
  changing `PATH`. You can still continue to see the rest: for this
  session only, add it with
  `[Environment]::SetEnvironmentVariable('Path', "$env:Path;C:\Program Files\Git\usr\bin", 'User')`
  (undo in "Cleaning up"), restart Cursor, and note that it then works.
- **A result** (Git, MSYS2 or WSL's `sh`): note which. A WSL `sh.exe`
  would not work with Windows paths; record if so.

Run the launcher outside Cursor with the release server on:

```powershell
$env:NYMPH_RELEASE_URL = 'http://127.0.0.1:8000'
sh C:/nymph-test/scratch/cursor/plugin/bin/nymph --version
```

Pass: prints `nymph 0.1.0` after downloading and verifying. To see where
it cached: `$env:NYMPH_LAUNCHER_INFO=1; sh C:/nymph-test/scratch/cursor/plugin/bin/nymph`
(prints the file name and cache path; unset it afterwards:
`Remove-Item Env:NYMPH_LAUNCHER_INFO`). If this fails the problem is the
launcher or `sh`, not Cursor: copy the one error line.

### C.4 Install into Cursor and start it

```powershell
[Environment]::SetEnvironmentVariable('NYMPH_RELEASE_URL', 'http://127.0.0.1:8000', 'User')
$dest = "$env:USERPROFILE\.cursor\plugins\local\nymph"
New-Item -ItemType Directory -Force $dest | Out-Null
robocopy C:\nymph-test\scratch\cursor\plugin $dest /E /NFL /NDL /NJH /NJS | Out-Null
Get-ChildItem $dest -Force
```

(Copy, do not symlink: a link pointing outside the local folder is
skipped.) Fully quit Cursor and start it again so it sees the new
environment variable, open `C:\nymph-test\project`, and run **Developer:
Reload Window** (Ctrl+Shift+P).

Record:

1. Customize (sidebar): does `nymph` appear, with its skill and rule
   listed? Any warning about the manifest? Is the logo shown?
2. Settings > Tools & MCP: is a `nymph` server from the plugin listed and
   green? If red, MCP Logs: copy the line (a missing `sh`, a download or
   checksum error each have their own one-line message from the
   launcher).
3. The cache: the path from C.3 now holds the executable
   (`Get-ChildItem` it).
4. Repeat part B steps 3 to 5 (chart, error, save) with the plugin.
5. Skill and rule: new chat, ask "make a Gantt chart of three tasks".
   Without naming nymph: does the agent use it? (This is the rule and
   skill working.)
6. If part A's outcome was (a): ask for a chart, drag to zoom, then say
   "add a trend line to what I am looking at". Record whether the new
   chart keeps the zoom.
7. Tamper test (the launcher refuses a bad file): quit Cursor, then
   `Add-Content "$env:USERPROFILE\.cache\nymph\0.1.0\nymph-0.1.0-windows-x86_64.exe" 'x'`
   (use the path C.3 printed), start Cursor. Pass: the server starts
   anyway (the launcher replaces the changed file after checking) and
   the file's hash matches `SHA256SUMS` again.
8. Minimum versions: if anything above failed, note Cursor's version and
   look in its release notes for MCP Apps and plugins support; record the
   first version that mentions each.

## Cleaning up

```powershell
Remove-Item -Recurse -Force "$env:USERPROFILE\.cursor\plugins\local\nymph"
[Environment]::SetEnvironmentVariable('NYMPH_RELEASE_URL', $null, 'User')
if (Test-Path "$env:USERPROFILE\.cursor\mcp.json.bak") { Move-Item -Force "$env:USERPROFILE\.cursor\mcp.json.bak" "$env:USERPROFILE\.cursor\mcp.json" }
# Only if you changed it in C.3:
# remove C:\Program Files\Git\usr\bin from your user Path (Settings > System > About > Advanced system settings > Environment Variables)
Get-Process python -ErrorAction SilentlyContinue | Where-Object { $_.CommandLine -like '*http.server*' } | Stop-Process
Remove-Item -Recurse -Force C:\nymph-test, c:\github\nymph\build-cursor
```

Restart Cursor.

## Reporting

Fill this in (answers to [DESIGN.md](DESIGN.md) section 9) and paste it
into PLAN.md's step 1, 2 and 4 results:

| # | Question | Answer | Evidence |
|---|---|---|---|
| 1 | Widget in chat? CSP? WebAssembly, WebGL2? | | part A steps 4, 5 |
| 2 | `instructions` reach the model? Rule needed? | | B 6, 7 |
| 3 | Plugin, manifest, `${CURSOR_PLUGIN_ROOT}`, `bin/` accepted? | | C.4 1, 2, 3 |
| 4 | `sh` on Windows? | | C.3 |
| 5 | Minimum versions; install link works? | | C.4 8, B 8 |
| 6 | Protocol version, capabilities | | A 5 |

Then: update the steps' results in PLAN.md, DESIGN.md sections 4.3, 4.4,
9; remove *unverified* from `docs/cursor.md` where confirmed and correct
it where not (especially the Windows plugin row); tick the completion
criteria.
