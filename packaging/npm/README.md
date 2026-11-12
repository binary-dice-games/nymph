# nymph

nymph turns a short YAML (or mermaid) source into a chart or diagram PNG:
2D and 3D plots, dashboards, UML, flowcharts, Gantt and Sankey charts,
user-interface mockups. Its MCP server gives any AI agent tools to draw,
check and convert charts.

This package is a small launcher: the first time it runs, it downloads
the nymph executable for your system from the GitHub release of this
version, checks it against the SHA-256 the package pins, and keeps it in
`~/.cache/nymph/<version>/`. A file that does not match is never run.
Linux, macOS (x86-64, arm64) and Windows (x86-64).

## MCP server

Add it to your agent's MCP configuration:

```json
{ "mcpServers": { "nymph": { "command": "npx", "args": ["-y", "@binary-dice-games/nymph", "mcp"] } } }
```

or, with Claude Code: `claude mcp add nymph -- npx -y @binary-dice-games/nymph mcp`.
Per-agent setup: [docs/agents.md](https://github.com/binary-dice-games/nymph/blob/main/docs/agents.md).

## Command line

```sh
npm install -g @binary-dice-games/nymph
nymph chart.yaml        # writes chart.png
```

## Environment

| Variable | Effect |
|---|---|
| `NYMPH_BIN` | Run this nymph executable instead; nothing is downloaded |
| `NYMPH_CACHE` | Where downloaded executables are kept (default `~/.cache/nymph`) |
| `NYMPH_RELEASE_URL` | Where to download the release files from (a mirror) |

MIT License. Source, documentation and other installs:
https://github.com/binary-dice-games/nymph
