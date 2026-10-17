# Changelog

All notable user-facing changes to nymph are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

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
