# nymph — Implementation Plan

See [DESIGN.md](DESIGN.md) for the architecture this plan implements. The
reference implementation is wish's `modules/bdg/dev/nymph/` module and the
plot code it uses (`src/ui/plot_elements/`, `src/ui/plot3d_elements/`,
`src/imgui/imgui_plot_renderer.cpp`, `src/imgui/imgui_plot3d_renderer.cpp`,
`src/imgui/imgui_plot_style.hpp`, `tests/test_nymph_*.cpp`).

Steps are ordered so the riskiest part, the software rasterizer, is proven
first, and every step leaves something that passes its own tests. Every
test is a GoogleTest file in `tests/`, added with `nymph_add_test()`.

## Steps

### 1. Dependencies and build wiring

*Result: done.* Departures from the plan as first written: libyaml is
compiled from its eight C files in `cmake/NymphDependencies.cmake`, not
through its own `CMakeLists.txt` (that file requires CMake 3.0, which
CMake 4 rejects, and adds CTest tests and install/export rules);
`IMGUI_DISABLE_OBSOLETE_FUNCTIONS` is not set because ImPlot v1.0 still
calls `ImDrawList` overloads it deletes; the test runs two frames, since
ImGui hides a new auto-sized window on its first. Fresh-clone check done:
a clone without `--recursive` checks out the pinned commits on configure,
builds, and passes. Lesson: `git submodule add` records the remote's
default branch; the pinned commit must be staged (`git add extern/<name>`)
or the configure-time `git submodule update` moves it back.

**Goal:** ImGui, ImPlot, ImPlot3D, libyaml and stb build from submodules
and link into the `nymph` library with no system packages.

**Deliverables:**
- `extern/imgui` (`v1.92.9b`), `extern/implot`, `extern/implot3d`,
  `extern/stb` (the commits wish pins), `extern/libyaml` (`0.2.5`) as
  submodules with `branch` entries in `.gitmodules`.
- `cmake/NymphDependencies.cmake`: `nymph_ensure_submodules()` for each;
  static targets `nymph_imgui` (imgui core + implot + implot3d sources, no
  backends) and `yaml` (libyaml's C sources, no tests or install); an
  interface target `nymph_stb` for the stb include path. All in the
  `extern` IDE folder.
- Root `CMakeLists.txt` includes it; `src/CMakeLists.txt` links `nymph`
  to them.
- `docs/building.md`: dependency table.

**Tests:**
- `tests/test_dependencies.cpp`: creates and destroys an ImGui, ImPlot and
  ImPlot3D context; runs `NewFrame()` / `Render()` with a fixed
  display size, checking that ImGui requests the font atlas as an Alpha8
  texture (`ImTextureStatus_WantCreate`) and that text and plots produce
  geometry; parses a YAML string with libyaml and checks node positions;
  encodes a PNG with stb.
- A fresh clone without `--recursive` configures and builds (manual check,
  recorded in the step's result).

### 2. Software rasterizer

*Result: done.* `src/render/soft_rasterizer`, `texture_store`, `image`;
`tests/test_soft_rasterizer.cpp` (16 tests). Refinements recorded in
DESIGN.md: constant attributes are never interpolated (a rule, not a fast
path, so the old fast path 3 is gone), sampling and colour modulation are
integer after rounding the sample position to 1/256 texel, and draw
callbacks follow 1.92.8's `DrawCallback_ResetRenderState`. A temporary
counter confirmed the fast-path tests really take the fast paths (2 solid
rectangles and 6 glyph quads in the mixed scene; the scaled quad falls
back). No `THIRD_PARTY.md`: no code was copied, the header credits
imgui_sw.

**Goal:** `ImDrawData` from ImGui 1.92 becomes correct, repeatable RGBA8
pixels on the CPU, from nymph's own rasterizer.

Reference: emilk's imgui_software_renderer (`src/imgui_sw.cpp`, commit
`b5ae63a`). Read it before writing this step, for the overall structure
(per-list, per-command, per-triangle loop), the fast-path detection and
the blend cache; DESIGN.md section 4 (`soft_rasterizer`) lists what to
take from it and what to do differently. Do not copy its float edge test,
texel lookup or white-pixel assumption.

**Deliverables:**
- `src/render/image.hpp`: `struct image {int width, height;
  std::vector<uint8_t> rgba;}`.
- `src/render/texture_store.hpp`/`.cpp`: Alpha8 and RGBA32 textures by
  id; `apply(ImTextureData&)` for `WantCreate` / `WantUpdates` /
  `WantDestroy`, setting `TexID` and status.
- `src/render/soft_rasterizer.hpp`/`.cpp`: `rasterize(const ImDrawData&,
  const texture_store&, ImU32 clear_color, raster_options = {}) ->
  image`. General path first (fixed-point edges, top-left rule, affine interpolation, bilinear
  sampling, integer blending, clip rectangles, `IdxOffset` / `VtxOffset`,
  `FramebufferScale`), then the fast paths. The header credits
  imgui_sw as the reference.
- `raster_options{bool fast_paths = true}`, an optional `rasterize()`
  argument, so tests can compare both paths.
- `reset_render_state_callback`, the no-op callback the frame driver
  installs as `ImGuiPlatformIO::DrawCallback_ResetRenderState`.

**Tests** (`tests/test_soft_rasterizer.cpp`, hand-built `ImDrawList`s
with a small Alpha8 texture whose white pixel is not at (0, 0), no ImGui
frame):
- An opaque axis-aligned rectangle covers exactly its pixels, with its
  colour, and nothing else changes.
- Two triangles forming a square with alpha 0.5 give one uniform colour:
  no pixel along the shared diagonal is blended twice. Same for a fan of
  eight triangles around a centre point.
- A triangle partly outside its clip rectangle changes no pixel outside
  it.
- Blending a known colour at known alpha over a known background gives the
  exact hand-computed bytes (three cases, including alpha 0 and 255).
- A textured quad mapped 1:1 onto the texture reproduces its texels at
  the right pixels; a quad sampling halfway between two texels gets their
  average.
- A solid rectangle that samples the moved white pixel is painted solid.
- A draw list over 65,536 vertices using `VtxOffset` draws the same
  pixels as the same shapes split into small lists.
- Two textures in one frame are each sampled from their own pixels.
- `texture_store` applies `WantCreate`, then a `WantUpdates` rectangle,
  then `WantDestroy`, with the expected pixels and statuses.
- `FramebufferScale = 2` with a 10x10 display gives a 20x20 image and a
  rectangle twice as large.
- A `ResetRenderState` callback is ignored; any other callback throws.
- For every test above, the fast paths on and off give identical bytes.
- Rasterizing the same draw data twice gives identical bytes.

### 3. Offscreen frame driver

*Result: done.* `src/render/figure_renderer`, `image_options`,
`draw_context` (with `content_rect()`); `tests/test_figure_renderer.cpp`
(8 tests). Departures: the previous contexts are saved and restored, not
assumed null, with a test rendering inside another open frame and one
where drawing throws; options are validated here too. Visual check of a
2D and a 3D plot in both themes at scale 1 and 2: text sharp, lines even,
bars solid. Text at scale 2 is re-baked, not stretched: fully inked
pixels are 37% of text pixels at 2x against 5.6% at 1x, and the test
checks that ratio. Four 640x420 images (two at 2x) render in about 0.5 s.

**Goal:** a drawing function rendered through a private ImGui / ImPlot /
ImPlot3D context comes out as an image, with no window or display.

**Deliverables:**
- `src/render/figure_renderer.hpp`/`.cpp`, first form:
  `render_frames(const image_options&, std::function<void(draw_context&)>)
  -> image`. Context trio created and destroyed per call (RAII guard),
  `AddFontDefaultVector()` at 16 px, Alpha8 atlas, theme (`light`, `dark`,
  `classic`) with `AntiAliasedLinesUseTex = false`, the full-display
  window without padding, fixed `DeltaTime`, mouse at `-FLT_MAX`, three
  frames with texture requests applied after each, rasterize the third.
- `src/render/draw_context.hpp`: what `draw()` receives (section 4), with
  `draw_children` left to step 7.

**Tests** (`tests/test_figure_renderer.cpp`):
- An `ImPlot::BeginPlot` / `PlotLine` / `EndPlot` lambda at 400x300 gives
  a 400x300 image with more than two distinct colours inside the plot
  area, and the corner pixel equal to the theme's window background.
- `scale = 2` gives 800x600; the plot title's text pixels cover about four
  times the area they cover at `scale = 1` (text is baked at the higher
  density, not stretched).
- A `Plot3D` with one `Plot3DLine` gives a non-uniform image.
- `dark` and `light` give different corner pixels.
- Two calls give identical bytes; after each, `ImGui::GetCurrentContext()`
  is null.

If text renders visibly wrong (blurred or misplaced glyphs), stop and fix
the sampling or atlas handling here before going on.

### 4. PNG encoding and metadata

*Result: done.* `src/png/png_meta`, `src/core/limits.hpp`;
`tests/test_png_meta.cpp` (14 tests). Bytes are `std::vector<uint8_t>` /
`std::span<const uint8_t>` rather than wish's `std::string`; `crc32()` is
public so a test checks it against the standard check value and against
stb's IHDR CRC. Outside the test suite, PNGs from `encode_png` were read
by Python's PIL (inline and compressed source, description) and by
ImageMagick (`identify -verbose`, no warnings).

**Goal:** an image plus a source text becomes a PNG that gives the text
back.

**Deliverables:**
- `src/png/png_meta.hpp`/`.cpp`: `encode_png(image, source, description)`,
  `read_source(bytes) -> optional<string>`, CRC-32, `iTXt` writing and
  reading as in DESIGN.md section 4 (port of wish's `nymph_png_meta`, with
  stb in place of miniz).

**Tests** (`tests/test_png_meta.cpp`, ported from wish's
`test_nymph_png_meta.cpp`):
- Round trip for: an empty source, LF and CRLF text, non-ASCII UTF-8, a
  source of exactly 64 KiB, and one of 1 MiB (compressed).
- A source up to 64 KiB appears verbatim in the PNG bytes; the `nymph`
  chunk directly follows `IHDR`.
- The description is in an `iTXt` chunk with keyword `Description`.
- Non-PNG bytes and a PNG without the chunk give `nullopt`; a PNG with
  one corrupted CRC, a truncated PNG and a corrupt compressed chunk throw
  `std::runtime_error` (as wish nymph does), so the CLI can say "corrupt
  PNG" instead of "not a nymph image".
- The pixels decode back (with `stb_image`, test-only) to the input image.

### 5. YAML document with positions

*Result: done.* `src/core/error.hpp`, `src/document/yaml_doc`;
`tests/test_yaml_doc.cpp` (18 tests). Additions beyond the plan: block
scalars record where their text starts (`content_line` /
`content_column`, needed by step 6); syntax errors name where the
enclosing construct started; non-scalar keys, nesting over 200 levels and
alias expansion beyond 4,000,000 nodes are errors. Mapping entries are two
parallel vectors (`keys`, `children`) rather than a vector of pairs of an
incomplete type, which MSVC handles less reliably.

**Goal:** the source text becomes a positioned, immutable node tree.

**Deliverables:**
- `src/core/error.hpp`: `nymph::error{line, column, message}`.
- `src/document/yaml_doc.hpp`/`.cpp`: `parse_yaml(std::string_view) ->
  yaml_doc` over libyaml's document API; scalar text, quoted flag,
  1-based line/column per node; aliases resolved by libyaml.

**Tests** (`tests/test_yaml_doc.cpp`):
- Line and column of nested mapping keys, sequence items, flow-style
  values and a block scalar's content.
- Quoted vs plain scalars are told apart (`"2025"` vs `2025`).
- Errors with positions for: a syntax error, a duplicate key (position of
  the second one), two documents, an empty document, a tab indentation.

### 6. Data tables

*Result: done.* `src/document/table`; `tests/test_table.cpp` (17 tests).
Beyond the plan: every cell carries its own line and column (wish
reported column 1 of the row); `parse_data()` chooses the form and maps a
literal block's text back to file positions; in the mapping form `~` /
`null` / an empty value is a gap; `data: >` and a list are errors. Also
found here: libyaml accepts an alias inside its own anchor (`a: &x [1,
*x]`), which the step 5 code only stopped through its depth limit with a
misleading message; `parse_yaml()` now detects the cycle and says so
(test in `test_yaml_doc.cpp`).

**Goal:** both forms of `data` become the same column table with
positions.

**Deliverables:**
- `src/document/table.hpp`/`.cpp`: `table`, `parse_csv(text,
  first_line, column_offset)`, `table_from_mapping(const yaml_node&)`,
  `number_column(i) -> vector<double>` (empty cell → NaN, otherwise a
  positioned error), column lookup by name or 1-based position.

**Tests** (`tests/test_table.cpp`, CSV cases ported from wish's
`test_nymph_bind.cpp`):
- RFC 4180 quoting, escaped quotes, quoted commas and newlines, trimming,
  blank lines, CRLF.
- A short row is an error at its line in the YAML file (block scalar
  offset applied).
- A non-number in a number column is an error naming row and column; an
  empty cell is NaN.
- The mapping form: unequal lengths and a non-scalar item are errors;
  `$2` is the second key.
- The same data in both forms gives equal tables.

### 7. Element schema and registry

*Result: done.* `src/schema/` (`value`, `name_table`, `field_desc` with
builder functions, `node`, `element_class`, `registry`, `convert`) and
`draw_context::draw_children`; `tests/test_schema.cpp` (16 tests). The
layout hooks shown in DESIGN.md (`measure`, `hints`, `arrange`) are left
to step 11, where they are first used. Typing rules settled here: yes/no
and on/off are booleans; an enumeration accepts only its own values; NaN
and infinity are not numbers; a quoted number gets a "remove the quotes"
hint.

**Goal:** element classes can be declared and looked up, and YAML values
convert to field values by field type.

**Deliverables:**
- `src/schema/value.hpp`, `field_desc.hpp`, `element_class.hpp`,
  `registry.hpp`/`.cpp`, `name_table.hpp` (enumeration and `A|B` flags
  parsing and printing).
- `src/schema/convert.hpp`/`.cpp`: `yaml_node` + `field_desc` → `value`,
  with DESIGN.md section 3's typing rules, ranges and colour check.
- `draw_context::draw_children` implemented.

**Tests** (`tests/test_schema.cpp`, with fake classes registered in the
test):
- Registering two classes with one name throws.
- Each field type: a valid value converts; a wrong YAML kind, a quoted
  number for a number field, an out-of-range value and an unknown name
  are errors at the value's position.
- `flags: A|B` and `flags: 3` give the same value; `title: 2025` stays
  text.
- `color: "#11223344"` converts; `color: #112233` (empty after YAML drops
  the comment) and `"#12345"` are errors.

### 8. Binding

*Result: done.* `src/document/bind` (`bind_source()`, `figure`);
`tests/test_bind.cpp` (20 tests) with fake element classes. Departures:
the function is `bind_source`, not `bind` (argument-dependent lookup on a
`std::string` argument chose `std::bind`, which surfaced as a compile
error in the test); the root-fill rule is an `element_class::adjust_root`
hook instead of a check for `Plot` in the binder; the theme list moved to
`image_options.hpp` so binding needs no renderer; errors carry "did you
mean" suggestions. The public `check_source()` in `src/nymph.hpp` moves
to step 9, where the built-in registry it needs is created.

**Goal:** `bind(source, registry)` produces a fully checked `figure` or a
positioned error.

**Deliverables:**
- `src/document/bind.hpp`/`.cpp`: top-level keys, `image` options and
  limits, the element walk with slot checks, column references,
  `figure` / `node` types.
- `src/nymph.hpp`: `check_source()` (registry from step 9 not yet
  needed; tests use fake classes).

**Tests** (`tests/test_bind.cpp`, ported from wish's
`test_nymph_bind.cpp` and adapted to the YAML form):
- The DESIGN.md example (with fake `Plot` / `PlotLine` / `PlotBars`
  classes) binds to the expected tree and arrays.
- Errors with exact `line:column` for: missing `root`, unknown top-level
  key, `nymph: 2`, unknown type, unknown field, a series in the wrong
  slot, an unknown column, a reference in a non-array field, a list
  mixing references and numbers, each `image` limit.
- `[$a, $b]` interleaves by row; `$col` into a text field joins with
  `\n`; `$$x` is the literal `$x`.
- A root `Plot` without `height` gets the fill value.
- 16 MiB + 1 byte and 1,000,001 data rows are rejected before parsing
  them.

### 9. 2D plot elements

*Result: done.* `src/elements/` (`plot_style`, `plot2d`,
`register_builtin`), `render_figure()`, the public `render_png()`,
`check_source()` and `extract_source()` in `src/nymph.hpp`;
`tests/test_plot2d.cpp` (13 tests) and 14 samples in
`tests/data/plot2d/`, all looked at. Additions, recorded in DESIGN.md
section 3: an `element_class::check()` hook with per-field source
positions (`node::field_pos`) for rules between fields; `ys` without `xs`
drawn against the index; printf-format fields validated (a security fix
over wish); a per-child ImGui ID in `draw_children`. Found while looking
at the images: ImPlot v1.0's `PlotHistogram2D` drew every bin at count 0
(it fills the X range wrongly when none is given; wish has the same bug),
worked around by passing the range; and wish's `bins` description had
the binning methods in the wrong order. `PlotDigital` draws a 4-pixel
logic trace at the bottom of the plot, which is ImPlot's design; its
description now says so. Element rectangles live in `draw_context`
(`set_rect` / `rect_of`) rather than a separate `layout_context`.

**Goal:** `Plot` and every 2D series from wish render with wish's fields
and defaults.

**Deliverables:**
- `src/elements/plot2d/`: `Plot` and the 14 series classes, their field
  tables, flag/enum tables and descriptions copied from wish's
  registrations, and `draw()` ported from `imgui_plot_renderer.cpp` and
  `imgui_plot_style.hpp`; `register_plot2d()`.
- `src/elements/register_builtin.cpp`: `register_builtin_elements()`.
- `src/layout/layout_context.hpp`/`.cpp`, first form: rectangles per
  node; `root` gets the image minus `image.padding`. The axis arithmetic
  comes in step 11.
- `src/nymph.hpp`: `render_png()` and `check_source()` using the built-in
  registry (`check_source` moved here from step 8).
- `tests/data/`: one YAML source per 2D series.

**Tests** (`tests/test_plot2d.cpp`):
- Each class's field names and defaults equal a table in the test, copied
  from wish's registrations (catches a dropped field).
- Each test source renders, and its image differs from the same `Plot`
  with no series.
- A series with `color: "#FF0000"` and `line_weight: 3` has pure-red
  pixels; without `color` it has none.
- `x_min` / `x_max` change the image; `flags: NoLegend` removes the legend
  (pixel difference in the legend corner).
- `render_png` twice gives identical bytes.

### 10. 3D plot elements

*Result: done.* `src/elements/plot3d`; `tests/test_plot3d.cpp` (10 tests)
and 7 samples in `tests/data/plot3d/`, all looked at. `surface.yaml` is
wish's `testdata/surface3d.nymph` converted by hand to the YAML form (the
conversion step 13 automates), at 480x360. Added checks as in step 9;
the notable one is `Plot3DMesh.indices` bounds checking: wish passed
indices to ImPlot3D unchecked, so a bad index read out of bounds.

**Goal:** `Plot3D` and every 3D series render with wish's fields and
defaults.

**Deliverables:**
- `src/elements/plot3d/`: `Plot3D` and the 7 series classes, ported from
  wish's registrations and `imgui_plot3d_renderer.cpp`;
  `register_plot3d()`.
- `tests/data/`: one YAML source per 3D series (the surface source ported
  from wish's `testdata/surface3d.nymph`).

**Tests** (`tests/test_plot3d.cpp`): the step 9 tests for the 3D classes,
plus: a `Plot3D` inside a `Plot` is a slot error.

### 11. Layout and text elements

*Result: done.* `src/layout/` (`distribute`, `arrange_linear`,
`measure_linear`, `layout_root`), `src/elements/layout_elements`
(the files are `layout_elements.*`, not `elements/layout/`),
`element_class::hints / measure / arrange`, `layout_figure()`;
`tests/test_layout.cpp` (6 tests), `tests/test_layout_elements.cpp`
(15 tests) and 5 samples in `tests/data/layout/`, all looked at.
Decisions: natural sizes are measured at the width the child will get,
during arrange, rather than in a separate bottom-up pass (a wrapped
label's height depends on it); a root's positive width/height is honoured
and a larger one is an overflow error; plots now always draw at their
rectangle; wish's descriptions for the layout fields were rewritten for
nymph's engine. A `Plot` in a row keeps its fixed default height unless
it sets `height: -1` (wish's rule); the agent guide must say so.

**Goal:** several plots, labels and separators can be arranged in one
image with wish's layout hints.

**Deliverables:**
- `src/layout/`: measure and arrange passes, the shared axis function
  (fixed, natural, stretch weights, `spacing`, overflow error with
  position and pixel counts), as in DESIGN.md sections 3 ("Layout") and 4
  ("Layout engine").
- `src/elements/layout/`: `VerticalLayout`, `HorizontalLayout` (`spacing`,
  `width`, `height`, `card`, `padding`, `rounding`; `align` on the
  horizontal one), `Spring` (`weight`), `Label` (`text`, `text_color`,
  `text_color_light`, `text_color_dark`, `wrap`), `Separator`,
  `SeparatorText` (`label`), with wish's defaults and descriptions;
  `register_layout()`.
- `Plot` / `Plot3D`: `hints()` from their `width` / `height` (negative
  height now allowed as a stretch weight; positive values keep wish's
  range); a root plot with no `height` fills.
- `check_source()` runs the layout pass (no rasterizing).
- `layout_figure(figure) -> rectangles` in `figure_renderer`: one frame,
  layout only. Used by `check_source()` and by the tests, which check
  rectangles exactly instead of looking for plot borders in pixels.
- `tests/data/`: the DESIGN.md two-plot example, a centred label (two
  Springs), a card, a horizontal row of two plots.

**Tests:**
- `tests/test_layout.cpp`, the axis function alone (no ImGui): fixed,
  natural and stretch children with spacing get the expected sizes;
  weights 1 and 3 split 400 px as 100 / 300; a Spring with weight 0
  collapses; `align: right` offsets the row; fixed + natural sizes larger
  than the space give the overflow error with both numbers.
- `tests/test_layout_elements.cpp` (rendered):
  - Two plots with `height: -1` in a `VerticalLayout` get equal heights
    (their rectangles); `-1` and `-3` split 1:3.
  - A `Label` between two Springs is horizontally centred (its rectangle
    centred within 1 px, and its text pixels inside the rectangle).
  - A wrapped `Label` is taller than the same text unwrapped and fits the
    width.
  - `text_color` shows in the text pixels; with `theme: dark`,
    `text_color_dark` wins over `text_color_light`.
  - `card: true` draws a panel whose border pixels sit `padding` outside
    the children.
  - A `Separator` draws a one-pixel-high line across the layout.
  - A layout whose fixed heights exceed the image fails with the
    layout's line and column, and `check` reports the same error.
  - A series directly inside a `VerticalLayout` is a slot error.
  - The schema of each class matches a table copied from wish.

### 12. Command line

**Goal:** the `nymph` executable implements DESIGN.md section 6.

**Deliverables:**
- `src/cli/cli.hpp`/`.cpp`: `run_cli(args, in, out, err)` — auto-detect,
  `render`, `extract`, `check`, `schema [--markdown]`, `--version`,
  `--help`, default output names, `-` for stdin, temp-file-and-rename
  writes, exit codes and message formats.
- `app/main.cpp` forwards to `run_cli`, with stdin/stdout in binary mode
  on Windows.
- `docs/cli.md`.

**Tests** (`tests/test_cli.cpp`, files in a per-test temp directory named
after the gtest test):
- `nymph in.yaml` writes `in.png`; `nymph in.png` prints the YAML; both
  commands give byte-identical round trips.
- `render - -o out.png` reads stdin; `render -` without `-o` exits 2.
- A bad source: exit 1, exactly one stderr line in the
  `nymph: <file>:<line>:<column>: <message>` form, no output file, an
  existing output file unchanged.
- `extract` of a non-nymph PNG: exit 1, "not a nymph image".
- `check` prints nothing on a valid source.
- `schema PlotLine` lists `xs`, `ys` and `color`; `schema Nope` exits 1.

### 13. wish nymph images

**Goal:** `extract` turns a wish nymph three-part source into equivalent
nymph YAML.

**Deliverables:**
- `src/document/legacy.hpp`/`.cpp`: detection and conversion (DESIGN.md
  section 6), used by `extract_source()`.
- `tests/data/legacy/`: wish's `chart.nymph`, `pie_heatmap.nymph`,
  `surface3d.nymph`.

**Tests** (`tests/test_legacy.cpp`):
- Each legacy file wrapped in a PNG by `encode_png` extracts to YAML that
  binds, with the same description, root tree and data as the original.
- A YAML source containing a `--` line inside a block scalar is not
  mistaken for a legacy source.
- `chart.nymph` (a `VerticalLayout` with a `Label` and two plots)
  converts and renders, and its image shows a label above two plots.

### 14. Documentation and examples

**Goal:** a person or an agent can use nymph from the docs alone.

**Deliverables:**
- `README.md`: overview, quick build, the one-command quick start, a short
  example, the metadata caveat; rows for every new `docs/` file.
- `docs/format.md`: the source format reference (DESIGN.md section 3 in
  user terms).
- `docs/elements.md`: the output of `nymph schema --markdown`.
- `docs/agent-guide.md`: self-contained instructions for an AI agent,
  ported from wish's `AGENT_GUIDE.md` to the YAML form and this CLI
  (recipes, good-chart advice, limits, common errors).
- `examples/*.yaml` (line, bars, histogram, heatmap, pie, scatter, 3D
  surface, several plots with labels in a layout) and `docs/examples.md`
  showing each with its PNG.
- `CHANGELOG.md` `### Added` entries; DESIGN.md status line updated.

**Tests** (`tests/test_docs.cpp`):
- `docs/elements.md` equals `run_cli({"schema", "--markdown"})` output.
- Every `examples/*.yaml` renders, and extracting its PNG gives the file
  back byte for byte.

## Completion Criteria

- [ ] All tests pass under `ctest -j` three times in a row.
- [ ] Builds and tests pass on Linux (GCC), MSYS2 and native Windows
      (MSVC).
- [ ] `nymph <example>.yaml` works with no display: `DISPLAY` and
      `WAYLAND_DISPLAY` unset, no GPU driver.
- [ ] For every example, `extract(render(text)) == text`, and rendering
      twice gives identical PNG bytes.
- [ ] A bad source exits 1 with one positioned line on stderr and writes
      nothing.
- [ ] The `nymph` binary links no SDL, OpenGL, wish or bison symbols
      (`nm` / `dumpbin` check).
- [ ] Every element type and field wish nymph accepted (2D and 3D plots,
      layouts, `Spring`, `Label`, `Separator`) is accepted with the same
      name and default, and wish's three test charts render after
      `extract` converts them.
- [ ] README, `docs/`, CHANGELOG and DESIGN.md match the code.
