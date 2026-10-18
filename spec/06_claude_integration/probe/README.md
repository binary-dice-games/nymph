# Step 1 probe

Throwaway tools for [PLAN.md](../PLAN.md) step 1. Delete this folder when
the step's manual questions are answered.

| File | What |
|---|---|
| `probe_server.py` | MCP server (Python, standard library): MCP App pages that test the sandbox and draw a chart with the viewer; logs every message and every page report to `--log` |
| `fake_host.mjs` | Plays the host side of MCP Apps in Chromium (Playwright) under a chosen CSP |
| `skill/nymph-probe/` | A claude.ai test Skill; zip it with a static Linux x86-64 `nymph` as `nymph-probe/bin/nymph` |

## Running in Claude Desktop

Build the web viewer (`docs/building.md`, web viewer), then add to
`claude_desktop_config.json` (Settings > Developer > Edit Config):

```json
{"mcpServers": {"nymph-probe": {"command": "python3", "args": [
  "<repo>/spec/06_claude_integration/probe/probe_server.py",
  "--log", "<somewhere>/probe.log",
  "--png", "<repo>/examples/dashboard.png",
  "--yaml", "<repo>/examples/dashboard.yaml",
  "--viewer", "<build-web>/web/nymph-viewer.single.js"]}}}
```

Restart Claude Desktop and ask, in the chat and in the Code tab: "Run
probe_sandbox, then probe_viewer." Each widget writes its report to the
log (`"kind": "report"`). Record: the widget appeared (or not), the
chart drew and moves on drag, the PNG was also shown, the
`wasm_compile` / `webgl2` results, the policy in `violations`, the
`initialize` message's `capabilities` (does it list
`io.modelcontextprotocol/ui`?) and whether `download_anchor` saved a file.

## Running the fake host

```sh
node fake_host.mjs --log /tmp/probe.log --viewer <build-web>/web/nymph-viewer.single.js \
  [--viewer-js <wasm0-build>/web/nymph-viewer.single.js] [--wasm-csp | --claude-csp]
```
