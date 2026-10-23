# The web app

<https://binary-dice-games.github.io/nymph/> opens, views and edits nymph
charts in the browser, with nothing to install. Your files stay in the
browser: nothing is uploaded unless you save to Google Drive, and there are
no analytics. Design: [spec/15_web_app/DESIGN.md](../spec/15_web_app/DESIGN.md).

## Opening a chart

From the start page:

| How | What |
|-----|------|
| **Open file…** (or `Ctrl+O`) | A `.yaml` / `.yml` file, or a PNG nymph wrote (it carries its YAML). In Chrome and Edge the file can then be saved in place. |
| Drag and drop | Anywhere on the page. |
| Paste | YAML text, or an image (clipboard images are usually re-encoded, which drops the YAML a nymph PNG carries). |
| **From URL…** | Any `https` file the server lets other sites read. GitHub file pages (`…/blob/…`) and gists open their raw file. |
| **Google Drive** | When the site is set up for it (below). |
| **New chart** | A template of each family: plots, a dashboard, a flowchart, class and sequence diagrams, a Gantt chart, a Sankey diagram, a UI mockup. |
| **Recent** | The last 20 charts, kept in this browser (**Clear** forgets them). |
| **Examples** | Every example of the repository. |

Links open a chart directly: `?url=<url>`, `?example=<name>`,
`?template=<name>`, `?drive=<file id>`, and `#y=…` (a shared chart, see
below); add `&mode=edit` to open it in the editor.

A file that does not load shows the loader's message with its line and
column; **Fix in source** opens it in the editor with the YAML at the error
(only that pane edits until the text loads).

## Viewing and editing

**View** is the [web viewer](viewer.md): drag to pan, wheel or pinch to
zoom, hover for values, double-click to fit.

**Edit** (or the `E` key) loads the editor, the same as
[`nymph view --edit`](viewer.md#editing-in-the-window): click a text or an
element to select it, change it in the panel or the floating toolbar,
right-click to add, move or delete elements, and edit the data tables in
their tabs. **Source** (or `Ctrl+U`) shows the YAML beside the chart, kept
in step both ways; `Ctrl+F` / `Ctrl+H` find and replace in it. `Ctrl+Z` /
`Ctrl+Y` undo and redo; going back to View keeps the chart and its zoom but
not the undo history.

The editor needs a mouse, a keyboard and WebGL2: on phones and tablets the
app is a viewer only (open, view, share, download).

## Saving

The dot after the file name means unsaved changes (zoom and pan alone do not
count, though a save keeps them in the `view` key).

| Save menu | What |
|-----------|------|
| **Save** (`Ctrl+S`) | Back to the file it came from (a file opened with **Open file…** in Chrome or Edge, or Google Drive); otherwise a download. A PNG is saved as a PNG that carries the edited YAML. |
| **Save as…** | A new file on this computer (Chrome, Edge). |
| **Save to Google Drive…** | A new file in a Drive folder you pick. |
| **Download YAML / PNG** | Always available. |

Unsaved changes are kept in the browser as a draft: if the tab closes,
reopening the chart offers to restore them.

## Sharing

**Share > Copy link** gives a link that reopens the chart: the URL, example
or Drive file it came from when unchanged, otherwise the chart itself in the
link's `#y=` part (compressed; never sent to a server). Very large charts
make long links that some chat apps cut; the app warns and offers the file
instead. A Drive link works for people the file is shared with.
**Copy embed code** gives HTML that shows the chart live in any page.

## Installing and offline

In Chrome or Edge, install the app from the address bar. Installed, it can
open `.yaml` files from the file manager ("Open with"). After one visit the
app works offline; a new version installs in the background and the app
offers **Reload** (it never reloads by itself).

## Keyboard

| Keys | What |
|------|------|
| `Ctrl+O`, `Ctrl+S` | Open, save |
| `E` | Edit (in View) |
| `Ctrl+U` | The YAML source |
| `Ctrl+Z`, `Ctrl+Y` | Undo, redo |
| `Delete`, `Ctrl+D`, `Alt+↑/↓` | Delete, duplicate, move the selected element |
| `?` | All shortcuts |

## For agents

Every action of the app is a command shaped as an MCP tool (name,
description, JSON Schema), listed by `window.nymphApp.commands.list()` and
run by `commands.call(name, args, origin)`; in Edit mode they include the
editor's `editor_*` tools. Changes record who made them. A command that
leaves the browser tab (opening a URL, saving, downloading, copying) asks
the user first when an agent calls it. Agent connectors (Claude, Cursor,
Gemini) are planned on this (design section 11). The agent guide and the
element reference are served under `agent/`.

## Hosting and setup

The site is built by the `nymph-site` target of the web build into
`build-web/web/site/` ([building.md](building.md#web-viewer)); preview it
with `python3 -m http.server -d build-web/web/site`. The `pages` workflow
builds it, runs the web tests and deploys it on each push to `main` (Settings
> Pages > Source: GitHub Actions).

| Path | What |
|------|------|
| `index.html`, `app/`, `app.css`, `sw.js`, `manifest.webmanifest` | The app |
| `nymph-viewer.js`, `nymph-viewer-core.*`, `nymph-viewer.single.js`, `fonts/` | The viewer, at the paths embedders use |
| `editor/` | The editor module (`nymph-editor.js`, about 1.2 MB gzip), loaded only to edit |
| `examples/`, `templates/`, `demo/`, `agent/` | Examples, templates, the demo page, agent docs |
| `version.json`, `config.json` | The build's commit; the provider settings |

**Google Drive** (optional): in the Google Cloud console create a project,
enable the Google Drive API and the Google Picker API, configure the OAuth
consent screen (scope `drive.file` only, which needs no security review),
create an OAuth client ID of type *Web application* with the site's origin
(`https://binary-dice-games.github.io`) as an authorised JavaScript origin,
and a browser API key restricted to that origin and the Picker API. Set the
repository variables `NYMPH_GOOGLE_CLIENT_ID`, `NYMPH_GOOGLE_API_KEY` and
`NYMPH_GOOGLE_APP_ID` (the project number); the next deploy shows the Drive
buttons. These values are public by design. A local build takes them as the
CMake cache variables of the same names.
