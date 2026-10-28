# nymph — Command Line

```sh
nymph chart.yaml              # render: writes chart.png
nymph chart.png               # extract: prints the YAML the image carries
nymph check chart.yaml        # validate only; prints nothing when valid
nymph diagram.mmd             # a mermaid diagram: writes diagram.png
nymph convert diagram.mmd -o diagram.yaml   # its nymph YAML
nymph schema PlotLine         # what PlotLine accepts
nymph rasterizers             # can this machine draw with the GPU?
```

`nymph` needs no display, window or GPU (only `nymph view` opens a window). It draws with OpenGL when a GPU
is available and on the CPU otherwise ([Rasterizers](#rasterizers)).

## Commands

| Command | What it does |
|---|---|
| `nymph <in> [-o <out>]` | If `<in>` is a PNG, prints its YAML (or writes it to `<out>`). Otherwise renders `<in>` (YAML or [mermaid](mermaid.md)) to `<out>`, by default `<in>` with its extension replaced by `.png`; an `<out>` ending in `.yaml` / `.yml` converts instead (`convert`). |
| `nymph render <in> [-o <out>] [--theme <name>]` | Renders. `<in>` may be a nymph PNG (its YAML is rendered again, in place unless `-o`), a mermaid diagram, or `-` for standard input, which needs `-o`. `--theme` renders with that [chart theme](themes.md) and writes `image.theme: <name>` into the PNG's YAML (the input file is not changed). |
| `nymph extract <in.png> [-o <out>]` | Prints the YAML a nymph PNG carries, or writes it to `<out>`. |
| `nymph convert <in> [-o <out>] [--theme <name>]` | Prints the nymph YAML of `<in>`, or writes it to `<out>` (which must end in `.yaml` or `.yml`): a mermaid diagram converted, a nymph PNG's source, or a YAML source checked and passed through; `--theme` writes that theme into it. See [mermaid.md](mermaid.md). |
| `nymph check <in> [--theme <name>]` | Validates a YAML file, a mermaid diagram (converted) or a nymph PNG's YAML: everything `render` checks, including whether a layout fits, without drawing; `--theme` checks it with that theme. |
| `nymph schema` | Prints every element type with its fields, types, defaults and allowed values. |
| `nymph schema <Type>` | The same for one type. |
| `nymph schema --markdown` | The reference as Markdown, in one piece. |
| `nymph schema --markdown --paged <dir>` | The reference as Markdown in pages: `<dir>/elements.md` (conventions and a table of pages) and `<dir>/elements/<Type>.md`, one per kind of figure with the types that go inside it. [elements.md](elements.md) and `elements/` are generated this way; building `nymph` refreshes them (`-DNYMPH_UPDATE_DOCS=OFF` stops that). |
| `nymph fonts` | The font names `font:` accepts, one per line: `default` and every font file of the catalog; fonts whose bytes this build does not embed (`NYMPH_EMBED_FONTS`) are marked `(not embedded)`. |
| `nymph fonts --licenses` | The licence texts of the embedded fonts. |
| `nymph themes` | The [chart themes](themes.md) `image.theme` accepts, one per line: name, display name, light or dark, description. |
| `nymph themes <name>` | The YAML file of one theme. |
| `nymph view <in> [--theme <t>] [--fit <f>] [--edit [--no-mcp]]` | Opens a YAML file, a mermaid diagram (converted; the editor saves it as `<name>.yaml` next to it, never over the mermaid) or a nymph PNG in a window with the [web viewer](viewer.md)'s interaction: drag to pan, wheel to zoom, hover for values, double-click to fit. Keys: `R` resets the view, `S` saves the image on screen as `<in>.view.png`, `Y` its YAML as `<in>.view.yaml` (the zoom and pan go in the `view` key; never over `<in>`), `Esc` / `Q` closes; while a text field of a [user interface](format.md#user-interfaces) has the keyboard, keys go to it. `--theme <name>` shows the chart in another [chart theme](themes.md) (the file keeps its own); `--fit source\|width\|fill` (default `fill`) says how the layout follows the window. `--edit` (or the `E` key) opens the [editor panel](viewer.md#editing-in-the-window): click a text or element and change its text, font, size, colour or any other field, right-click to add, move or delete elements, and edit the `data` tables in tabs; AI agents using `nymph mcp` can work on the chart through its `editor_*` tools (`--no-mcp` turns that off). Source errors are reported before any window opens. Needs OpenGL 3.3 and, on Linux, an X display (XWayland is enough); both are the system's, loaded at run time. Windows and Linux; built with `NYMPH_VIEW` (on by default, off on macOS). |
| `nymph mcp [--rasterizer <r>]` | Runs an MCP server for AI agents: JSON-RPC 2.0 messages, one per line, on standard input and output, until standard input closes (exit 0; 1 if standard output fails). Tools: `render_chart` (returns the PNG; `output_path` also writes it), `check_chart`, `convert_chart` (mermaid to nymph YAML), `extract_chart`, `chart_schema`, `list_fonts`, `list_shapes`, `list_themes`; the `editor_*` tools of an open editor; `web_app_pair` and `web_app_call` for the chart open in the [web app](web-app.md#agents) (then a 127.0.0.1 port is opened, for the app's origin and `NYMPH_WEB_APP_ORIGINS`); `render_chart` and `check_chart` take mermaid too; source errors read as on the command line. Resources: `nymph://docs/agent-guide` and `nymph://docs/elements`. Logs go to standard error. Built with `NYMPH_MCP` (see [spec/06_claude_integration](../spec/06_claude_integration/DESIGN.md)). |
| `nymph rasterizers` | Whether each rasterizer can be used here: `software: available`, then `opengl: available (<device>)` or `opengl: unavailable (<reason>)`. Exit 0 either way. |
| `nymph --version`, `nymph --help` | Version, usage. |

`-o -` writes to standard output, so nymph works in a pipe:

```sh
generate-yaml | nymph render - -o - > chart.png
```

## Rasterizers

`render` and the implicit render of `nymph <in>` take
`--rasterizer software|opengl`:

| Value | Draws with |
|---|---|
| `opengl` (default) | The GPU, through OpenGL 3.3 loaded at run time (EGL on Linux, needing no display server; `opengl32.dll` on Windows). If OpenGL cannot be used (no library, no GPU driver, only a CPU OpenGL such as Mesa llvmpipe, an image larger than the GPU allows, a driver error), the image is drawn by the software rasterizer instead and the command still succeeds. |
| `software` | nymph's own CPU rasterizer. Never touches OpenGL, and gives the same bytes for the same source and nymph build on every machine. |

Without the option, a fallback is silent. With an explicit
`--rasterizer opengl`, a fallback prints one line on standard error and
still exits 0:

```
nymph: warning: OpenGL unavailable (libEGL.so.1: cannot open shared object file: No such file or directory); used the software rasterizer
```

The two rasterizers draw the same picture; their pixels can differ
slightly at anti-aliased edges, and OpenGL images can differ between GPUs.
Use `--rasterizer software` when images must compare byte for byte.
`--rasterizer` on any other command, or an unknown value, is a usage error
(exit 2).

## Exit codes and errors

| Code | Meaning |
|---|---|
| 0 | Success. |
| 1 | Failure; one line on standard error. |
| 2 | Bad arguments; the message and the usage on standard error. |

A problem in the source names its line and column (columns count
characters):

```
nymph: chart.yaml:9:56: unknown column 'revenu' (did you mean 'revenue'?) (columns: month, revenue, cost)
```

Anything else names the file:

```
nymph: photo.png: not a nymph image (the PNG carries no nymph source)
nymph: missing.yaml: no such file
```

On failure nothing is written: output goes to a temporary file that is
renamed over the target only once it is complete, so an existing output
file is left as it was. A symbolic link as `-o` is followed, and a
device or pipe (`-o /dev/null`) is written directly.

A mermaid input reports its problems at its own lines and columns, and
prints one line per thing nymph dropped, without failing:

```
nymph: diagram.mmd:7:3: warning: click is ignored (a nymph figure has no links or callbacks)
```

## Metadata survives only exact copies

The YAML is stored in the PNG as a text chunk. Tools that re-encode images
(many chat apps and image hosts) drop it; keep the YAML file too when the
image goes through one.
