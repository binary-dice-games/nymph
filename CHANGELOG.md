# Changelog

All notable user-facing changes to nymph are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- `Plot` axis fields: `x_scale` / `y_scale` (`Log10`, `SymLog`, `Time` in UTC), `x_format` / `y_format` tick formats, `x_ticks` / `y_ticks` with `x_tick_labels` / `y_tick_labels`, and a second Y axis (`y2_` fields, `y_axis: Y2` on any 2D series).
- `PlotErrorBars`, `PlotBarGroups` (grouped or stacked bars), `PlotAnnotation` (callouts) and `PlotTag` (axis tags) 2D series.
- `ColorScale` element: a colour bar to place beside a heatmap or surface.
- `Flowchart` diagram (`FlowNode` in nine shapes, `FlowGroup` boxes, `FlowLink` with labels, dotted / thick lines and arrowheads), laid out automatically like the UML diagrams.
- `GanttChart` (`GanttSection`, `GanttTask`): tasks on a date or number axis, with `after` dependencies drawn as arrows, milestones, statuses and a `today` line.
- UML diagrams, described in YAML and laid out automatically: `ClassDiagram` (`Class`, `ClassRelation`), `SequenceDiagram` (`Participant`, `Message`, `Fragment`, `Else`), `StateDiagram` (`State`, composite states, `Transition`), `ActivityDiagram` (`Action`, `ActivityControl`, `Flow`), `UseCaseDiagram` (`Actor`, `UseCase`, `UseCaseRelation`), and `Note` for all of them. Graph diagrams use a Graphviz dot-style layered layout steered by `direction`, `rank`, `weight`, `min_length` and `constraint`; diagrams scale to fit down to `min_zoom`.
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
