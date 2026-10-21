# nymph in Cursor

nymph works as a tool Cursor's agent uses by itself: the agent writes the
chart's YAML, nymph draws it, the agent looks at the PNG and fixes what is
unclear, and the image is attached to the chat. `nymph mcp` is a standard
MCP server, which Cursor runs over standard input and output. Design:
[spec/12_cursor_integration](../spec/12_cursor_integration/DESIGN.md).

**Status:** the files below are built and tested for their contents; they
have not yet been run in Cursor (nobody has tried them there). The parts
that need a real Cursor are marked *unverified*.

| Path | Install | What you get |
|---|---|---|
| Plugin (macOS, Linux; Windows *unverified*) | Copy `cursor/plugin/` to `~/.cursor/plugins/local/nymph`, reload Cursor; or, on a Teams / Enterprise plan, import this repository as a team marketplace | The nymph tools, the `nymph` command, the nymph skill and a rule, in one step. |
| Install script and `mcp.json` (every system) | Install `nymph` ([README](../README.md#install)), then add the entry below | The nymph tools and the `nymph` command. |

Both need a nymph release to download or install; until one is published
(or while GitHub is out of reach) build nymph
([building.md](building.md)) and point `NYMPH_BIN` at the executable.

## Plugin

The plugin (`cursor/plugin/`) is listed by `.cursor-plugin/marketplace.json`
at the root of this repository. Cursor installs plugins from a team
marketplace (a Teams / Enterprise feature: Dashboard > Plugins & MCPs >
Add Marketplace > Import Repo, then Customize in the sidebar) or from a
local folder, which any user can use: copy `cursor/plugin/` to
`~/.cursor/plugins/local/nymph` (`%USERPROFILE%\.cursor\plugins\local\nymph`
on Windows; a symlink works only if its target is inside that folder), then
run **Developer: Reload Window** and look for `nymph` in Customize. The
plugin needs nothing else installed: the first time its server starts, the launcher downloads
the nymph executable for your system from the release the plugin's version
names, checks it against the SHA-256 the plugin pins, and keeps it in
`~/.cache/nymph/<version>/`; a file that does not match is never run. To
use a nymph you built or installed yourself, set `NYMPH_BIN` to its path
before starting Cursor.

The launcher is a `sh` script, run as `sh ${CURSOR_PLUGIN_ROOT}/bin/nymph
mcp`. On Windows that needs a `sh` on `PATH` (Git for Windows has one);
whether Cursor finds it there is *unverified*. If the server does not
start on Windows, use the next path.

## Install script and mcp.json

Install nymph as the README says (`install.ps1` on Windows puts
`nymph.exe` on your `PATH`; it needs no `sh`). Then add the server to
`~/.cursor/mcp.json` (every project) or `.cursor/mcp.json` (this project):

```json
{ "mcpServers": { "nymph": { "type": "stdio", "command": "nymph", "args": ["mcp"] } } }
```

For an executable at a path you choose, use `"command": "${env:NYMPH_BIN}"`
or the full path. Cursor documents this one-click link format; not yet tried here:
`cursor://anysphere.cursor-deeplink/mcp/install?name=nymph&config=eyJjb21tYW5kIjoibnltcGgiLCJhcmdzIjpbIm1jcCJdfQ%3D%3D`.

Cursor asks you to approve tool calls the first time, as for any MCP tool.
The tools are the ones in [claude.md](claude.md#tools).

## Teaching the agent

The MCP server sends its instructions when it connects; whether Cursor
passes them to the model is *unverified*. The plugin adds a rule
(`rules/nymph.mdc`, applied when the agent judges the request is about a
chart) and the nymph skill. Without the plugin, put the rule in your
project as `.cursor/rules/nymph.mdc`:

```
---
description: Use when the user asks for a chart, plot, graph, diagram, flowchart, UML, Gantt chart, Sankey diagram, dashboard or user-interface mockup. Draw it with the nymph MCP tools (render_chart) and look at the image.
alwaysApply: false
---

Draw charts and diagrams with nymph: write a short YAML source, call the
`render_chart` tool, look at the PNG it returns and fix what is unclear.
Call `chart_schema` for the fields of an element type. To keep a chart,
pass `output_path` (a `.png`) and keep the YAML.
```

or, in `AGENTS.md`:

```
For charts and diagrams use the nymph MCP tools (`render_chart`); read
docs/agent-guide.md of nymph, or call `chart_schema`, for the YAML format.
```

## The live chart

Cursor lists MCP Apps as supported, so `render_chart`'s live viewer may
appear in the chat as it does in Claude ([claude.md](claude.md#the-live-chart)).
Whether it does, and whether Cursor lets it run WebAssembly, is
*unverified*; where it does not, you get the PNG, which every result
carries.

## Limits

- Cursor's cloud and background agents run on a remote machine with no
  local MCP server; nymph is not set up for them.
- The YAML stays inside the PNG only while the file is copied byte for
  byte, so keep the YAML too.
- See [claude.md](claude.md#limits) for the rest, which is the same.
