# nymph in Gemini CLI

[Gemini CLI](https://github.com/google-gemini/gemini-cli) runs MCP servers
on your computer, so it uses nymph the way Claude Code and Cursor do: the
`nymph mcp` server draws charts and returns PNGs, and works on the chart
open in `nymph view --edit` or in the [web app](web-app.md#agents).

## Setup

Install nymph ([README](../README.md#install)), then add the server to
`~/.gemini/settings.json` (or a project's `.gemini/settings.json`):

```json
{
  "mcpServers": {
    "nymph": { "command": "nymph", "args": ["mcp"] }
  }
}
```

Use the executable's full path if it is not on your `PATH`. `/mcp` in
Gemini CLI lists nymph's tools once it has started.

## Charts

Ask for a chart: Gemini calls `render_chart` and gets the PNG; with
`output_path` the image is also written to a file. The server's
instructions and [agent-guide.md](agent-guide.md) (the resource
`nymph://docs/agent-guide`) teach the format.

## The web app

To edit a chart in the browser with Gemini's help, open it in the
[web app](https://binary-dice-games.github.io/nymph/) and ask Gemini to
"connect to the nymph web app". It calls `web_app_pair` and shows a code
like `48213-K7QM-2XPD`: type it in the app under **Agents > Connect an
agent** (or open the link it shows). Gemini then reads and changes the
open chart; every change is listed in the app with Undo, and saving or
downloading asks you first.

Whether Gemini CLI re-reads the tool list when nymph adds the app's tools
(`tools/list_changed`) is *unverified*; if not, it reaches them through
`web_app_call`.
