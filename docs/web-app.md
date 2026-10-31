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
| **New chart** | A template of each family, each card with a small picture of its chart type: plots, a dashboard, a flowchart, class and sequence diagrams, a Gantt chart, a Sankey diagram, a UI mockup. |
| **Import CSV or mermaid…** | A new chart from a CSV table or a mermaid diagram, through a wizard (below). Dropping a `.csv` or `.mmd` file opens it too. |
| **Recent** | The last 20 charts, kept in this browser (**Clear** forgets them). |
| **Examples** | Every example of the repository. |

Links open a chart directly: `?url=<url>`, `?example=<name>`,
`?template=<name>`, `?drive=<file id>`, and `#y=…` (a shared chart, see
below); add `&mode=edit` to open it in the editor.

A file that does not load shows the loader's message with its line and
column; **Fix in source** opens it in the editor with the YAML at the error
(only that pane edits until the text loads).

## Importing a CSV table or a mermaid diagram

**Import CSV or mermaid…** asks for a file and opens a wizard, with a live
preview of the chart beside each step:

| Step | CSV table | Mermaid diagram |
|------|-----------|-----------------|
| 1 | **Chart type**: line, area, bar, stacked bars, horizontal bars, scatter, pie or histogram (the best fit is picked). | **Diagram**: the text, to fix what does not convert. |
| 2 | **Data**: the column for the X axis or categories, the series (each with its legend name), the axis labels. | |
| 3 / 2 | **Look**: title, subtitle and footnote; fonts (Editorial: PT Serif heading; Modern: Fira Sans; Technical: Fira Mono); colour theme; heading as a text box, a title over a rule, or plain; size; legend place; summary cards with each series' headline number (CSV only). | |

**Create chart** opens the result as a new chart (in Edit on a computer).
It is ordinary nymph YAML: a `VerticalLayout` with the heading, the plot or
diagram, a `Separator` and the footnote, and the CSV as its `data` (numbers
like `$1,200` or `45 %` cleaned; semicolon files with decimal commas read).

## Viewing and editing

**View** is the [web viewer](viewer.md): drag to pan, wheel or pinch to
zoom, hover for values, double-click to fit.

**Edit** (or the `E` key) loads the editor, the same as
[`nymph view --edit`](viewer.md#editing-in-the-window): click a text or an
element to select it, change it in the panel or the floating toolbar,
right-click to add, move or delete elements, and edit the data tables in
their tabs. `Ctrl+Z` / `Ctrl+Y` undo and redo; going back to View keeps the chart and its zoom but
not the undo history.

**Theme** (the panel's button, or **⋯ > Theme…**) gives the chart a
[chart theme](themes.md): background, fonts, series colours and diagram
colours for everything it does not set itself. In Edit it is written into
the file (`image.theme`, undoable); in View it is a preview that leaves the
file alone.

The editor panel takes the app's light or dark look (the **⋯** menu, or the
system's) and its green accent.

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

## Live collaboration

**Collaborate** (`Ctrl+Shift+L`) shares the open chart live: people with
the link see it as you edit it and draw on a whiteboard over it (cursors,
ink, notes, stickers), browser to browser, with no server. The drawings
are never saved in the file; **Download PNG with drawings** gives a
picture with them. Details, privacy and what to do when someone cannot
connect: [collaboration.md](collaboration.md).

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
| `Ctrl+Z`, `Ctrl+Y` | Undo, redo |
| `Delete`, `Ctrl+D`, `Alt+↑/↓` | Delete, duplicate, move the selected element |
| `Ctrl+Shift+A` | The Agents panel |
| `Ctrl+Shift+L` | Share live, or the Live session panel |
| `?` | All shortcuts |

## Agents

An AI agent you already use can work on the chart open in the app: Claude
Code, Claude Desktop, Cursor and Gemini CLI through `nymph mcp`, and the
browser's own assistant through WebMCP where the browser has it. The agent
brings its own model, login and billing: **the app never asks for an API
key and calls no model.** Your chart goes to the agent you connect, and
nowhere else.

**Connect an agent.** Ask it to "connect to the nymph web app". It calls
`nymph mcp`'s `web_app_pair` and gives you a code like `48213-K7QM-2XPD`
and a link. Type the code under **Agents** (`Ctrl+Shift+A`) > **Connect an
agent**, or open the link (it opens the app, connected). The code works
once, for 5 minutes. `nymph mcp` then listens on `127.0.0.1` only, for
this site only; reloading the tab reconnects. Chrome may ask to let the
site reach apps on this device: allow it. Per client:
[claude.md](claude.md#the-web-app), [cursor.md](cursor.md#the-web-app),
[gemini-cli.md](gemini-cli.md). From a chart shown in Claude's chat,
**Open in the web app** does both steps at once.

**What you see.** The **Agents** panel lists who is connected and every
change an agent made, newest first, each with **Undo** (in Edit mode the
editor's own undo; in View mode the chart from before the change). An agent
groups the steps of one request, so Undo takes them back together; while
it works the panel says so, with **Stop**. Your own edits always win: an
agent's write that would overwrite them is refused. Anything that leaves
the tab (saving, downloading, copying, opening a URL) asks you first;
"Allow ... without asking" lasts until the chart closes.

**What agents see.** The app's commands, as MCP tools: `document_state`,
`document_set_source`, `document_image`, `app_set_mode`, the editor's
`editor_*` tools (in Edit mode), and `app_read_guide`, which serves the
agent guide, the element reference and a workflow guide for the app
(`agent/web-app.md`). The list is
`window.nymphApp.commands.agentTools()`; the shell's own controls use the
same commands.

**Browser assistants (WebMCP).** Where the browser offers WebMCP
(`navigator.modelContext`), the app registers its tools while a chart is
open; a switch in the Agents panel turns that off. On phones this is the
only way an agent reaches the app.

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
| `collab/` | The live collaboration packages (`collab-session.js`, `collab-whiteboard.js`, a demo), loaded only for a session ([collaboration.md](collaboration.md)) |
| `examples/`, `templates/`, `demo/`, `agent/` | Examples, templates, the demo page, agent docs |
| `version.json`, `config.json` | The build's commit; the provider settings |

`config.json`'s `collab` section has the trackers and STUN / TURN servers
of live sessions ([collaboration.md](collaboration.md#hosting-and-setup)).
`config.json`'s `agents` section lists the agent connectors offered:
`NYMPH_AGENT_CONNECTORS` (CMake, default `webmcp;bridge`) sets it; an empty
value hides the Agents button. A copy of the app on another address must
be added to the origins `nymph mcp` accepts: `NYMPH_WEB_APP_ORIGINS`
(comma-separated; an entry ending in `:*` accepts any port, as
`http://localhost:*` for a local preview).

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
