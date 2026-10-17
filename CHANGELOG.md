# Changelog

All notable user-facing changes to nymph are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Initial project: CMake build of the `nymph` library and `nymph` executable, with a GoogleTest suite.
- `nymph` command: `nymph <in>` renders YAML to PNG or extracts the YAML from a nymph PNG; `render`, `extract`, `check` and `schema [<Type>] [--markdown]` commands; `-` / `-o -` for standard input and output; one-line positioned errors and exit codes 0 / 1 / 2; outputs written atomically.
- Library API: `render_png()` renders a YAML chart source to a PNG that embeds the source, `check_source()` validates one, `extract_source()` reads it back; CPU rendering with no GPU or window.
- 2D chart elements: `Plot` and `PlotLine`, `PlotScatter`, `PlotStairs`, `PlotStems`, `PlotShaded`, `PlotBars`, `PlotBarsH`, `PlotHistogram`, `PlotHistogram2D`, `PlotHeatmap`, `PlotPieChart`, `PlotDigital`, `PlotText`, `PlotInfLines`, with wish's fields.
- 3D chart elements: `Plot3D` and `Plot3DLine`, `Plot3DScatter`, `Plot3DSurface`, `Plot3DTriangle`, `Plot3DQuad`, `Plot3DMesh`, `Plot3DText`, with wish's fields.
- Layout and text elements: `VerticalLayout`, `HorizontalLayout` (spacing, size hints with stretch weights, card panel, right alignment), `Spring`, `Label` (colours per theme, wrapping), `Separator`, `SeparatorText`. A layout that does not fit the image is an error.
