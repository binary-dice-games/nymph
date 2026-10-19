# Changelog

All notable user-facing changes to nymph are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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

- An unknown name in an enumeration or flags field now suggests the closest one.
- `Separator` and `SeparatorText` also go in user-interface windows, as ImGui's own.
- The Claude Code plugin's version is now the version of the nymph release it pins (`0.0.0` until a release is pinned), so Claude Code offers an update exactly when the pin changes.
- `nymph schema <Type>` suggests the closest type name for an unknown one.
- `nymph fonts` marks fonts whose bytes the build does not embed `(not embedded)`.
- The command line moved out of the `nymph` library into a `nymph_cli` library; programs using only the library no longer link it.

### Fixed

- Gantt charts: switching a section off in the legend (live views, `nymph view`) now hides its rows; the `view` key saves hidden sections as `hidden`.
- `nymph view` on Linux: the X11 window now sets `WM_CLASS` (`nymph`), so window managers and WSLg name and group it.
- Chrome extension: a chart whose image has `width` / `height` attributes overridden by the page's CSS (claude.ai's file preview) now takes the image's shown size instead of collapsing to zero height.
- `nymph view` on 32-bit Windows no longer aborts with a stack-pointer (ESP) check failure; OpenGL functions are now called with the `__stdcall` convention.
- Web viewer: the `click` event (and `nymph-click`) now fires when a data point is clicked; it never did.
- Web viewer: a viewer given `fetchFont` in a page without a base URL (an inline `srcdoc` frame) no longer fails to start; the default font folder is resolved only when a font is first fetched.
