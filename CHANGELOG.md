# Changelog

All notable user-facing changes to nymph are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- The web app at https://binary-dice-games.github.io/nymph/: open nymph YAML and PNG files from the computer (file picker, drag and drop, paste), a URL, Google Drive (when the site is set up for it), a shared link, Recent or a template; view them live; edit them with the editor of `nymph view --edit`; save back to the file (Chrome, Edge) or Drive, or download YAML or PNG; copy a link that reopens the chart or HTML that embeds it. Unsaved changes are kept as drafts; the app installs, opens `.yaml` files from the system and works offline. Phones and tablets get the viewer only.
- The editor module for web pages: `editor/nymph-editor.js` (`createEditor()`), a second WebAssembly module next to the viewer's, with the editor tools as a JavaScript API (`tool()`), attributed changes and undo groups.
- `nymph view --edit` and the web editor: a source pane (`Ctrl+U`, or the **Source** tab button) shows the YAML beside the figure, highlighted, with line numbers, folding and find / replace, in step with the panel both ways; a file that does not load opens at its error there instead of failing.
- `nymph view --edit`: the undo and redo buttons name the change they undo or redo; `Ctrl+S` asks to save.
- Chart themes: `image.theme` picks one of ten built-in themes (`light`, `dark`, `classic`, `forest`, `midnight`, `ocean`, `paper`, `pastel`, `slate`, `sunset`) that fills every field the chart leaves out, by element type (`FlowNode:fill_color`), slot (`uml:color`) or field name; values the chart writes, on an element or an enclosing one, win.
- `image.background` and `image.palette` (the colours series take in turn).
- `nymph themes [<name>]`; `--theme <name>` on `nymph render` and `nymph check` (render writes it into the PNG's source).
- Editor (`nymph view --edit`, web app): a **Theme** picker with swatches that writes `image.theme`; the web app's **Theme…** dialog previews themes in View.
- Web viewer and editor modules: `themes()`; `setTheme()` takes any chart theme and fetches its fonts.
- Mermaid input: `nymph`, `render`, `check`, `nymph view` and the MCP tools `render_chart` / `check_chart` / `editor_set_source` read mermaid diagrams (flowchart, sequence, class, state, ER, gantt, pie, mindmap, sankey, xychart) and convert them to nymph YAML; errors and warnings point at the mermaid lines. Outputs stay PNG or YAML.
- `nymph convert` and the MCP tool `convert_chart`: the nymph YAML of a mermaid diagram, a nymph PNG or a YAML source; `nymph diagram.mmd -o diagram.yaml` converts too.
- `nymph view --edit` on a mermaid file saves the converted chart as `<name>.yaml` next to it.
- Web viewer: `setSource()` and `<nymph-viewer src="x.mmd">` take mermaid; `importMermaid()` converts without drawing.

- `nymph view --edit`: a floating toolbar next to the selected element changes its shape, fill, line and text colours, font, text size and line style, copies its style to the next element clicked, clears its style, duplicates and deletes it; plots get a colour scheme picker (pie charts and heatmaps change their plot's), series a marker picker and marker size.
- `nymph view --edit`: clicking a legend entry selects its series (a pie chart's slice selects the chart) instead of hiding it.
- `nymph view --edit`: the divider between the figure and the editor panel can be dragged.
- `nymph view --edit`: right-click an element in the figure or the element tree to add a child, insert, duplicate, move or delete elements; only the types that fit are offered, and a chart can be added next to the root.
- `nymph view --edit`: tabs over the figure show each `data` table as an editable grid, with CSV import, export and paste, and a `+` tab to add tables.
- `nymph view --edit`: drag elements in the element tree to reorder them or move them into another parent that takes them.
- `nymph view --edit`: **Info** button opens a window to edit the figure's `description` and `source` note; the divider under the element tree can be dragged to resize it.
- `nymph view --edit`: list fields (`xs`, `ys`, `values`, ...) are editable as text, and a **columns** button points a field at any column of the data.
- `nymph view --edit` is an MCP server for AI agents: `nymph mcp` has `editor_*` tools that read and change the chart open in the editor (elements, selection, tables, an image of the view); `--no-mcp` turns it off.

### Changed
- `nymph view --theme`, the viewer element's `theme` attribute and `setTheme()` accept every chart theme, not only `light`, `dark` and `classic`.
- `nymph view --edit`: number fields are boxes that take a typed value after a click, or change by dragging sideways (Shift finer, Ctrl coarser); a text size of 0 shows as "auto".
- `nymph view --edit` and the web editor: a flat, web-app style panel (Fira Sans text, light or dark palette, green accent, primary Save button, section headers, thin scrollbars); the selection outline uses the accent. The web editor follows the page's light / dark theme and accent (`setAppearance()`, `appearance` option of `createEditor()`).

- `nymph view --edit`: the element list is a tree with collapsible nodes and an icon per element type; errors show in their own panel at the bottom of the editor.

- The Windows release executable is built with MSVC.
- The Linux release executable links glibc dynamically (libstdc++ stays inside) so `nymph view` works; it needs glibc 2.39 or newer.
- Plot series take only the style fields they are drawn with; a field that had no effect is now an error. `PlotHeatmap`, `PlotHistogram2D`, `PlotPieChart`, `PlotText` and `Plot3DText` take none, bars and digital traces have no `marker` / `marker_size`, `PlotShaded` has no `line_weight` or markers, `PlotErrorBars` and `PlotInfLines` take `color` and `line_weight`, `PlotAnnotation` and `PlotTag` take `color`, `PlotBarGroups` takes `fill_alpha`; `PlotText`, `PlotAnnotation`, `PlotTag`, `PlotPieChart`, `PlotBarGroups` and `Plot3DText` have no `label`.
- `fill_color` also colours marker faces of lines, scatters, stairs and stems (2D and 3D).
- `nymph view --edit`: choosing a colour for a field that was automatic opens the colour picker at once.

### Fixed
- `nymph view --edit` no longer redraws the editor panel continuously while nothing changes.
- `nymph view --edit`: adding a child from an element's right-click menu no longer crashes.
- `nymph view` and the web viewer: a series colour removed by an edit, an undo or a new source goes back to its automatic colour instead of keeping the old one; automatic colours and markers match a fresh render.
- The library builds with GCC again (a stray carriage return in `src/edit/tree_edit.cpp`).
- `nymph view --edit`: an edit that makes the layout overflow the image (such as adding an Image to a full layout) is refused with the layout error instead of leaving the editor out of sync.
- `nymph mcp`: a failed `output_path` write names the absolute path and suggests giving one.

- The Windows and Linux release executables include `nymph view`.
- `nymph view --edit`: clicking a diagram's shapes and arrows (nodes, links, messages, participants and their lifelines, notes, fragments) selects their element instead of the whole diagram.

## [0.1.0] - 2026-10-21

### Fixed

- Source archives include the Cursor plugin's launcher (`cursor/plugin/bin/`).
- The MCP bundle manifest lists the `example_chart` tool.

### Added

- winget: `winget install BinaryDiceGames.Nymph` (Windows), updated by each release once the package is accepted into the winget community repository.
- `nymph example [<Type or name>]` and the MCP tool `example_chart`: a complete, working source that uses an element type (or an example by name), to start from; the nymph skill and Cursor rule now tell the agent to start from it and look up fields with `chart_schema` instead of trial and error.

- Shape library: about 100 editable SVG shapes in `resources/shapes/` (basic geometry, flowchart symbols, arrows, callouts, UML and network icons, containers). The `Shape` element draws one in a layout; `FlowNode` takes any of them in its new `symbol` field; `nymph shapes [<name>]` and the MCP tool `list_shapes` list them, and `docs/shapes.md` shows them.
- UML component, deployment, package and object diagrams (`ComponentDiagram`, `DeploymentDiagram`, `PackageDiagram`, `ObjectDiagram`); component interfaces are wired from their names.
- `TreeDiagram`, `MindMap` and `OrgChart`: hierarchies written by nesting, laid out as tidy trees (a mind map grows to both sides, an org chart has portraits, hanging teams, assistants and vacancies).
- Cursor plugin (`cursor/plugin/`, listed by `.cursor-plugin/marketplace.json`): the nymph tools, skill and a rule for Cursor's agent; `docs/cursor.md` also gives the `mcp.json` entry for the `nymph mcp` server. Not yet run in Cursor.

- Browser extension for Firefox 140 or later: a release file `nymph-extension-firefox-<version>.zip` (load it as a temporary add-on; not yet on addons.mozilla.org). Where the browser withholds site access (Firefox, Safari), the toolbar button and the options page say so and ask for it. The "Copy the instructions" button of the options page works in every browser; the toolbar button's menu is skipped where the browser has none.
- `nymph schema --markdown --paged <dir>` writes the element reference in pages: `elements.md` (conventions and a table of pages) and `elements/<Type>.md`, one per kind of figure with the types that go inside it, so an AI agent reads only the pages it needs. `docs/elements.md` is now the root page, and building `nymph` refreshes the pages (`NYMPH_UPDATE_DOCS`).
- Chrome extension: nymph sources written in a chat's code blocks (Gemini, gemini.google.com) become live charts, drawn once the answer has finished and updated when it is revised; the code stays behind a "Code" button. Chat mode runs only on the sites listed in the new `chatSites` setting.
- Chrome extension: instructions that teach Gemini nymph's format, to paste into a Gem (options page) or to put in an open chat's prompt box from the toolbar button's menu ("Give this chat the nymph instructions"); the options page also lists, adds and removes the chat sites. New `contextMenus` permission.
- Chrome extension: a chat code block that starts like a nymph source (`nymph: 1`, `root:` or a `nymph` label) is always replaced by the viewer; when the YAML is invalid the viewer area shows the error with its line and column, a "Copy error" button and an "Ask Gemini to fix" button that puts the request in the prompt box.
- `nymph view`: the window shows nymph's dragonfly icon (title bar, taskbar, window switcher) on Windows and Linux instead of the system's generic icon.
- Windows: `nymph.exe` carries nymph's icon, shown by Explorer and shortcuts.

### Changed

- The fonts and placeholder images moved into `resources/` (`resources/fonts/`, `resources/placeholders/`); `NYMPH_FONTS_DIR` now defaults to `resources/fonts/`. The MCP bundle's icon is now `resources/icon.png`.

### Added

- User-interface mockups: a `Screen` of Dear ImGui `Window`s holding ImGui widgets by their ImGui names (`Text`, `Button`, `Checkbox`, `SliderFloat`..`SliderInt4`, drags, inputs, colour editors, `Combo`, `ListBox`, `Selectable`, `TreeNode`, `CollapsingHeader`, `TabBar` / `TabItem`, `Child`, `Group`, `Table`, `MainMenuBar`, `MenuBar`, `Menu`, `MenuItem`, `Popup`, `PopupModal`, `Tooltip`, `SameLine`, ...), drawn into the PNG in the state the source gives; plots and layouts can go inside windows; labels need not be unique. In `nymph view` and the web viewer the widgets work (pointer, keyboard, text); what the user changed is saved in the `view` key, and widget clicks are reported. See `docs/format.md`, "User interfaces".
- `WindowTransition`: labelled arrows between the windows of a `Screen`, for the flow between interfaces ("ok", "cancel", "finished loading").
- `Image` and `ImageButton` show built-in placeholder images by name (`person`, `people`, `landscape`, `product`, `logo`, `video`, `map`, `chart`, `document`, `icon`, `building`, `cross`), with `tint`, `background`, `rounding`, `fit` and a caption; `Image` also works in layouts.
- Library: `live_view::input_key()`, `input_text()`, `wants_keyboard()`, `set_clipboard_hooks()` and `ui_revision()`.

- `nymph view --edit` (and the `E` key): an editor panel docked to the view window. Click a plot title, axis label or legend, a label, or a diagram node, group or link text to select it, then change its text, font, size, colour or any other field; every change edits the YAML and redraws, with undo and redo. Texts that come from data are shown read-only. **Save** asks for confirmation and overwrites the input (a PNG stays a PNG, a YAML stays YAML); closing the window with unsaved changes asks first. Clicking the figure's background (or **Image** in the panel's list) edits the `image:` options: width, height, scale, theme, padding and the figure's text style.
- Every UML diagram and flowchart item (nodes, groups, links, notes, messages, transitions, ...) takes optional `font`, `font_size` and `text_color`; unset, it follows the diagram's.
- macOS (Apple silicon and Intel, macOS 13.3 or later) is a supported platform: release executables `nymph-<version>-macos-arm64` / `-macos-x86_64`, and `nymph-<version>-macos-universal.mcpb` for Claude Desktop on any Mac. macOS builds have no OpenGL rasterizer (`NYMPH_OPENGL` defaults to `OFF` there).
- `cmake --install` installs `nymph`, its documentation and licences, and puts `<prefix>/bin` on the user's `PATH` (a marked block in the shell start-up files; the user `Path` on Windows); `NYMPH_INSTALL` and `NYMPH_INSTALL_UPDATE_PATH` CMake options. On Windows the default prefix is `%LOCALAPPDATA%\Programs\nymph`.
- Install scripts in every release, `install.sh` (Linux, macOS) and `install.ps1` (Windows): download the executable for the system, check its SHA-256, install it and put it on `PATH`; `--uninstall` / `-Uninstall` undoes both.
- Release source tarball `nymph-<version>-src.tar.gz` with every submodule: builds, tests and installs with no network (`nymph-source-dist` target). Configuring a tree without the submodules and without git now fails with a message saying how to get them.
- Homebrew: `brew install binary-dice-games/nymph/nymph` (macOS, Linux), updated by each release.
- Releases are published by GitHub Actions from a `v<version>` tag: GitHub release with every file and one `SHA256SUMS`, the Chrome extension submitted to the Chrome Web Store, the Claude Code plugin pinned to the release.
- `NYMPH_CODESIGN_IDENTITY` CMake option: the macOS release executable's signing identity (ad hoc by default).
- `nymph view <in>`: opens a YAML file or nymph PNG in a window with the web viewer's pan, zoom and hover values; `--theme`, `--fit`, keys to reset the view and save the image or YAML on screen. Windows (Win32) and Linux (X11, also under Wayland through XWayland); the window libraries and the system's OpenGL 3.3 are loaded at run time, so `nymph` gains no imports and needs no extra files or development packages. `NYMPH_VIEW` CMake option (default `ON`; macOS has no window yet).

- `nymph mcp`: an MCP server on stdin / stdout for AI agents, with tools to render a chart (returning the PNG, optionally writing it), check it, extract a PNG's source, show the element schema and list fonts; source errors come back with their line and column. It gives clients usage instructions, and the agent guide and element reference as resources (`nymph://docs/agent-guide`, `nymph://docs/elements`). Built with `NYMPH_MCP_VIEWER` (the web build's `claude/viewer.html`), it also serves an MCP App that shows the chart live in the conversation: pan, zoom and hover, the chat's light or dark theme, full screen; what the user zooms to and clicks is passed to Claude, so the next render keeps their view. `NYMPH_MCP` CMake option (default `ON`); new submodule `extern/json`.
- `NYMPH_RELEASE` CMake option: static, stripped executables (`dist/nymph-<version>-<os>-<arch>`) with `SHA256SUMS`; toolchain files for Windows (MinGW-w64) and Linux arm64 cross builds in `cmake/toolchains/`.
- Claude Code plugin (`claude/plugin/`; this repository is its marketplace): `/plugin install nymph --marketplace binary-dice-games/nymph` gives Claude the nymph MCP tools and the `nymph` command; its launcher downloads the release executable for the platform, checks its pinned SHA-256 and caches it in `~/.cache/nymph` (`NYMPH_BIN` uses your own).
- Claude Desktop extension: release builds also write `dist/nymph-<version>-<os>-<arch>.mcpb`, an MCP bundle with the executable inside, installed with a double-click.
- claude.ai skill: Linux x86-64 release builds also write `dist/nymph-skill-<version>.zip` (the skill instructions, the agent guide and the executable) to upload in claude.ai's skills settings.
- Chrome extension (built with `NYMPH_WEB`, in `build-web/extension/`): turns nymph PNGs on any page into live charts of the same size, following pages that change (comments, previews, feeds); a toolbar button turns it off or on per site and counts the charts; options for the theme and the sites turned off. See `docs/extension.md`.
- Web viewer: `fit: 'scale'` (and `fit="scale"`) lays the chart out at its source size and scales it to the box width, up or down.

- Web viewer (`NYMPH_WEB`, built with Emscripten): the library compiled to WebAssembly (630 KB gzip) with a JavaScript API (`nymph-viewer.js`, or one file, `nymph-viewer.single.js`) and a `<nymph-viewer>` element that show a nymph YAML source or PNG live in a web page with WebGL2 (or on the CPU): pan, zoom and pinch, hover and click values, hover / click / view / error events, data updates, themes following the page, fonts fetched on demand, and PNG export of the chart on screen. See `docs/viewer.md`.
- `view` top-level key: the zoom, pan, 3D rotation and hidden series of plots, Gantt charts, 3D plots and diagrams, by element path; PNGs and the viewer open with it. The web viewer writes it.
- Web viewer: `toYaml()` returns the chart's YAML as it is on screen (data and view included); `toPng()` saves the image on screen; a right-click menu saves PNG or YAML and shows an About panel (`menu: false` / `menu="false"` turns it off).
- Library: `bdg::nymph::live_view` keeps a figure on screen frame after frame, with pointer input (pan, zoom, picks), data updates (`set_data`), nymph PNG input and view state.
- ImUml: `ImUmlDiagramFlags_Interactive` (wheel zoom around the mouse, drag pan, double-click fit, hovered node or edge).
- `NYMPH_EMBED_FONTS` CMake option (`all`, `none` or a list of file names) to compile in only some fonts; fonts not embedded are copied to `build/fonts/` and registered at run time with `bdg::nymph::register_font()`, checked against their size and SHA-256.

- `Plot` axis fields: `x_scale` / `y_scale` (`Log10`, `SymLog`, `Time` in UTC), `x_format` / `y_format` tick formats, `x_ticks` / `y_ticks` with `x_tick_labels` / `y_tick_labels`, and a second Y axis (`y2_` fields, `y_axis: Y2` on any 2D series).
- `PlotErrorBars`, `PlotBarGroups` (grouped or stacked bars), `PlotAnnotation` (callouts) and `PlotTag` (axis tags) 2D series.
- `ColorScale` element: a colour bar to place beside a heatmap or surface.
- `Flowchart` diagram (`FlowNode` in nine shapes, `FlowGroup` boxes, `FlowLink` with labels, dotted / thick lines and arrowheads), laid out automatically like the UML diagrams.
- `SankeyDiagram` (`SankeyFlow`, `SankeyNode`): flows between nodes as bands proportional to their value, from `from` / `to` / `values` lists (table columns) or child elements, laid out in columns automatically.
- `GanttChart` (`GanttSection`, `GanttTask`): tasks on a date or number axis, with `after` dependencies drawn as arrows, milestones, statuses and a `today` line.
- UML diagrams, described in YAML and laid out automatically: `ClassDiagram` (`Class`, `ClassRelation`), `SequenceDiagram` (`Participant`, `Message`, `Fragment`, `Else`), `StateDiagram` (`State`, composite states, `Transition`), `ActivityDiagram` (`Action`, `ActivityControl`, `Flow`), `UseCaseDiagram` (`Actor`, `UseCase`, `UseCaseRelation`), and `Note` for all of them. Graph diagrams use a Graphviz dot-style layered layout steered by `direction`, `rank`, `weight`, `min_length` and `constraint`; diagrams scale to fit down to `min_zoom`.
- Data flow diagrams: `DataFlowDiagram` with `ExternalEntity`, `Process`, `DataStore`, `DataFlow` and `TrustBoundary` groups.
- Network diagrams: `NetworkDiagram` with `Device` (14 icon kinds, address), `Link` (wired, wireless, VPN; port labels) and `Zone` groups.
- ER diagrams of database structure: `ERDiagram` with `Entity` (tables with typed attributes and PK / FK / UK keys) and `Relationship` (crow's foot cardinalities, identifying or not).
- ImUml: a UML diagram library for Dear ImGui in the style of ImPlot (`imuml/`, CMake target `imuml`).
- Fields can hold lists of text (`attributes: ["+id: int"]`), also from a `$column`.
- UML example diagrams in `examples/`.

- OpenGL rasterizer: `nymph` renders on the GPU when one is available (OpenGL 3.3, loaded at run time with no OpenGL library linked; headless EGL on Linux, WGL on Windows) and falls back to the software rasterizer otherwise. `--rasterizer software|opengl` chooses (default `opengl`); an explicit `--rasterizer opengl` warns on fallback.
- `nymph rasterizers` reports whether OpenGL can be used and why not.
- Library: `bdg::nymph::renderer` renders with a chosen `rasterizer` and reports fallbacks; `probe_opengl()`. The free `render_png()` still uses the software rasterizer.
- `NYMPH_OPENGL` CMake option (default `ON`) to build without the OpenGL rasterizer.

- Text style: `font`, `font_size` and `text_color` on `image`, layouts, plots, `Label`, `SeparatorText`, `PlotText` and `Plot3DText`, inherited by nested elements.
- Embedded fonts (Fira Sans, Fira Mono, PT Serif from Google Fonts), selected by file name; `NYMPH_FONTS_DIR` embeds another folder of fonts at build time.
- `nymph fonts` lists the font names; `nymph fonts --licenses` prints their licences.
- Named data tables: `data` can map table names to tables, referenced as `$table:column`.
- Optional `source` key recording how the data was made (prompt, query or code); kept in the PNG, never drawn.
- Initial project: CMake build of the `nymph` library and `nymph` executable, with a GoogleTest suite.
- `nymph` command: `nymph <in>` renders YAML to PNG or extracts the YAML from a nymph PNG; `render`, `extract`, `check` and `schema [<Type>] [--markdown]` commands; `-` / `-o -` for standard input and output; one-line positioned errors and exit codes 0 / 1 / 2; outputs written atomically (a symlinked output is followed, a device such as `/dev/null` written directly).
- Library API: `render_png()` renders a YAML chart source to a PNG that embeds the source, `check_source()` validates one, `extract_source()` reads it back; CPU rendering with no GPU or window.
- 2D chart elements: `Plot` and `PlotLine`, `PlotScatter`, `PlotStairs`, `PlotStems`, `PlotShaded`, `PlotBars`, `PlotBarsH`, `PlotHistogram`, `PlotHistogram2D`, `PlotHeatmap`, `PlotPieChart`, `PlotDigital`, `PlotText`, `PlotInfLines`.
- 3D chart elements: `Plot3D` and `Plot3DLine`, `Plot3DScatter`, `Plot3DSurface`, `Plot3DTriangle`, `Plot3DQuad`, `Plot3DMesh`, `Plot3DText`.
- Example charts in `examples/` (line, bars, histogram, heatmap, pie, scatter, 3D surface, named tables, fonts, a multi-plot layout) with their images.
- Layout and text elements: `VerticalLayout`, `HorizontalLayout` (spacing, size hints with stretch weights, card panel, right alignment), `Spring`, `Label` (colours per theme, wrapping), `Separator`, `SeparatorText`. A layout that does not fit the image is an error.

### Changed

- Plot lines are now anti-aliased in the software and OpenGL rasterizers, in PNGs and live views; they were jagged. The example PNGs are redrawn.
- An unknown name in an enumeration or flags field now suggests the closest one.
- `Separator` and `SeparatorText` also go in user-interface windows, as ImGui's own.
- The Claude Code plugin's version is now the version of the nymph release it pins (`0.0.0` until a release is pinned), so Claude Code offers an update exactly when the pin changes.
- `nymph schema <Type>` suggests the closest type name for an unknown one.
- `nymph fonts` marks fonts whose bytes the build does not embed `(not embedded)`.
- The command line moved out of the `nymph` library into a `nymph_cli` library; programs using only the library no longer link it.

### Fixed

- The Claude Code plugin launcher and `install.sh` no longer reject a correct download on Windows (Git Bash, MSYS2): the SHA-256 check failed when the file path contained a backslash.
- Building with MSYS2 (mingw-w64 GCC) no longer fails on a missing `<cstdint>` include.
- Gantt charts: switching a section off in the legend (live views, `nymph view`) now hides its rows; the `view` key saves hidden sections as `hidden`.
- `nymph view` on Linux: the X11 window now sets `WM_CLASS` (`nymph`), so window managers and WSLg name and group it.
- Chrome extension: a chart whose image has `width` / `height` attributes overridden by the page's CSS (claude.ai's file preview) now takes the image's shown size instead of collapsing to zero height.
- `nymph view` on 32-bit Windows no longer aborts with a stack-pointer (ESP) check failure; OpenGL functions are now called with the `__stdcall` convention.
- Web viewer: the `click` event (and `nymph-click`) now fires when a data point is clicked; it never did.
- Web viewer: a viewer given `fetchFont` in a page without a base URL (an inline `srcdoc` frame) no longer fails to start; the default font folder is resolved only when a font is first fetched.
