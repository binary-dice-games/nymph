# nymph in Claude

nymph works as a tool Claude uses by itself: Claude writes the chart's
YAML, nymph draws it, Claude looks at the PNG and fixes what is unclear,
and you see the chart in the conversation. Where Claude can show
interactive content, the chart is live: drag to pan, wheel or pinch to
zoom, hover for values, and Claude follows the view you leave. Design:
[spec/06_claude_integration](../spec/06_claude_integration/DESIGN.md).

| Claude | Install | What you get |
|---|---|---|
| Claude Code | `/plugin install nymph --marketplace binary-dice-games/nymph` | The nymph tools, the `nymph` command and the nymph skill. In the terminal, charts are files in your project; in the desktop app's Code tab they are also live in the conversation. |
| Claude Desktop (macOS, Windows) | Double-click `nymph-<version>-macos-universal.mcpb` or `nymph-<version>-windows-x86_64.mcpb` | The nymph tools; charts live in the conversation. |
| claude.ai (web, mobile) | Upload `nymph-skill-<version>.zip` as a skill | Claude runs nymph in its code sandbox and gives you the PNG. No live chart. |
| Any MCP client | Run `nymph mcp` (below) | The tools; live charts where the client shows MCP Apps. |

The files come from a GitHub release of nymph, or from a release build
([building.md](building.md#release-executables)).

## Claude Code

```
/plugin install nymph --marketplace binary-dice-games/nymph
```

(Before Claude Code 2.1.275: `/plugin marketplace add
binary-dice-games/nymph`, then `/plugin install nymph@nymph`.)

The plugin needs nothing else installed. The first time it starts, it
downloads the nymph executable for your system from the release its
version names, checks it against the SHA-256 the plugin pins, and keeps
it in `~/.cache/nymph/<version>/`; a file that does not match is never
run. The plugin runs on Linux and macOS (x86-64 and arm64) and Windows
(x86-64). Each nymph release updates the plugin's pin and version
(`/plugin marketplace update nymph` fetches it). To use a nymph you built
or installed yourself, set `NYMPH_BIN` to its path before starting Claude
Code. While no release is published, or when GitHub is out of reach,
`NYMPH_BIN` is required.

Then ask for a chart ("plot the error rate of the last week from
logs.csv"). Claude calls `render_chart`, sees the image, and writes the
PNG into the project when asked to save it. The plugin's `nymph` command
is also on Claude's `PATH` for anything else (`nymph chart.png` prints the
source a nymph PNG carries).

## Claude Desktop

Download `nymph-<version>-macos-universal.mcpb` (any Mac: Apple silicon
or Intel) or `nymph-<version>-windows-x86_64.mcpb` and open it: Claude
Desktop shows what it installs and asks to confirm. The executable is
inside: nothing else is downloaded or installed. It is listed, and
removed, in Claude Desktop's extension settings. The Linux bundles are
for unofficial Linux builds of Claude Desktop.

## claude.ai

Turn on code execution, then upload `nymph-skill-<version>.zip` in the
Skills settings. The skill holds the instructions, the full guide and the
nymph executable for Claude's sandbox (Linux x86-64), which has no
network: nothing is fetched. Ask for a chart; Claude draws it in the
sandbox and gives you the PNG (and its YAML) as files.

## Other MCP clients

`nymph mcp` is a standard MCP server on standard input and output. With
Claude Code, without the plugin:

```sh
claude mcp add nymph -- /path/to/nymph mcp
```

Or in a client's JSON configuration (Claude Desktop's
`claude_desktop_config.json`, for example):

```json
{ "mcpServers": { "nymph": { "command": "/path/to/nymph", "args": ["mcp"] } } }
```

The live chart is served only by executables built with the web viewer's
MCP App embedded (`NYMPH_MCP_VIEWER`, as the release ones are).

## The live chart

| You do | The chart does |
|---|---|
| Drag, wheel, pinch | Pans and zooms plots, Gantt charts and diagrams; rotates 3D plots |
| Hover, click | Shows the values of a point, bar, slice, task, flow or node |
| Click a legend entry | Hides or shows that series |
| Full screen (top right) | Asks the host for the whole window, where it offers it |

When you stop, Claude receives the view you left (the source's `view`
key) and the last point you clicked, so "add a trend line to what I am
looking at" keeps your zoom. The chart follows Claude's light or dark
theme. To keep a chart, ask Claude to save it: the live chart has no
download menu, since chat frames usually cannot save files.

## Tools

| Tool | What it does |
|---|---|
| `render_chart` | YAML (text or a file) to a PNG, returned to Claude; `output_path` also writes it |
| `check_chart` | Validates a source without drawing it |
| `extract_chart` | The YAML a nymph PNG carries |
| `chart_schema` | The fields of one element type, or of all |
| `example_chart` | A complete, working source using an element type (or an example by name), to copy and adapt; without `type`, the list of examples |
| `list_fonts` | The names a chart's `font` accepts |
| `list_shapes` | The shape library (the names a node's `shape` accepts); with `name`, the details of one shape |

Errors name the line and column of the source, as on the command line
([cli.md](cli.md)). The server also offers [agent-guide.md](agent-guide.md)
and [elements.md](elements.md) as resources.

## Other agents

Cursor has its own plugin and `mcp.json` entry: [cursor.md](cursor.md).

## Limits

- The YAML stays inside the PNG only while the file is copied byte for
  byte; chat apps that re-encode images drop it, so keep the YAML too.
- The live chart needs a client that shows MCP Apps and lets them run
  WebAssembly (Claude's do); elsewhere you get the PNG.
- On Windows, the Claude Code plugin's launcher runs through the `sh` of
  Git for Windows, which Claude Code requires; this is not yet verified.
- PNGs are identical byte for byte on every machine for one nymph build;
  the arm64 and x86-64 builds differ by a colour level here and there.
