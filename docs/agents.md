# nymph in any local agent

Any AI agent that runs MCP servers on your computer (Claude Code, Codex
CLI, Gemini CLI, Cursor, VS Code, Windsurf and others) can use nymph
through `nymph mcp`. The npm package
[`@binary-dice-games/nymph`](https://www.npmjs.com/package/@binary-dice-games/nymph)
starts it with one command, the same on every system:

```
npx -y @binary-dice-games/nymph mcp
```

Only Node.js 18 or later is needed. The first time it runs, the package
downloads the nymph executable for your system from the GitHub release of
its version, checks it against the SHA-256 the package pins, and keeps it
in `~/.cache/nymph/<version>/` (shared with the Claude Code plugin). A
file that does not match is never run. Linux, macOS (x86-64, arm64) and
Windows (x86-64). The package is also listed in the
[MCP Registry](https://registry.modelcontextprotocol.io) as
`io.github.binary-dice-games/nymph`, so clients that browse the registry
offer it there.

Agents with their own nymph package are simpler to set up that way:
Claude Code and Claude Desktop ([claude.md](claude.md)), Cursor
([cursor.md](cursor.md)).

## Setup per agent

| Agent | Command or configuration |
|---|---|
| Claude Code | `claude mcp add nymph -- npx -y @binary-dice-games/nymph mcp` |
| Codex CLI | `codex mcp add nymph -- npx -y @binary-dice-games/nymph mcp` |
| Gemini CLI | `~/.gemini/settings.json`: the `mcpServers` entry below |
| Cursor | `~/.cursor/mcp.json`: the `mcpServers` entry below |
| Windsurf | `~/.codeium/windsurf/mcp_config.json`: the `mcpServers` entry below |
| VS Code (Copilot agent mode) | `.vscode/mcp.json`: the `servers` entry below |

Most clients take the `mcpServers` form:

```json
{
  "mcpServers": {
    "nymph": { "command": "npx", "args": ["-y", "@binary-dice-games/nymph", "mcp"] }
  }
}
```

VS Code names the key `servers` and the transport:

```json
{
  "servers": {
    "nymph": { "type": "stdio", "command": "npx", "args": ["-y", "@binary-dice-games/nymph", "mcp"] }
  }
}
```

Codex CLI's `~/.codex/config.toml`, written by hand:

```toml
[mcp_servers.nymph]
command = "npx"
args = ["-y", "@binary-dice-games/nymph", "mcp"]
```

With nymph already installed ([README](../README.md#install)), use
`"command": "nymph", "args": ["mcp"]` instead: nothing is downloaded.

## Using it

Ask for a chart: the agent calls `render_chart` and gets the PNG back. The
tools are listed in [claude.md](claude.md#tools); the server's
instructions and [agent-guide.md](agent-guide.md) (the resource
`nymph://docs/agent-guide`) teach the format. The agent can also work on
the chart open in `nymph view --edit` or in the
[web app](web-app.md#agents).

`npm install -g @binary-dice-games/nymph` puts the `nymph` command on your
`PATH` the same way, for the command line ([cli.md](cli.md)).

## Environment

| Variable | Effect |
|---|---|
| `NYMPH_BIN` | Run this nymph executable instead; nothing is downloaded |
| `NYMPH_CACHE` | Where downloaded executables are kept (default `~/.cache/nymph`) |
| `NYMPH_RELEASE_URL` | Where to download the release files from (a mirror, or `file://` a folder) |

## Limits

- On Windows, a client that starts the command without a shell does not
  find `npx` (it is `npx.cmd`); use `"command": "cmd", "args": ["/c",
  "npx", "-y", "@binary-dice-games/nymph", "mcp"]`.
- The first start downloads the executable, which can take longer than a
  client waits for a server. Run `npx -y @binary-dice-games/nymph --version`
  once first.
- Behind an HTTP proxy, Node's downloads follow `HTTPS_PROXY` only with
  `NODE_USE_ENV_PROXY=1` (on Node releases that support it). Otherwise,
  install nymph another way and set `NYMPH_BIN`.
- Web-based agents (claude.ai, ChatGPT) cannot start a local server; see
  [claude.md](claude.md#claudeai) for claude.ai.
