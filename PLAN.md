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

**Goal:** ImGui, ImPlot, ImPlot3D, libyaml and stb build from submodules
and link into the `nymph` library with no system packages.

**Deliverables:**
- `extern/imgui` (`v1.92.9b`), `extern/implot`, `extern/implot3d`,
  `extern/stb` (the commits wish pins), `extern/libyaml` (`0.2.5`) as
  submodules with `branch` entries in `.gitmodules`.
- `cmake/NymphDependencies.cmake`: `nymph_ensure_submodules()` for each;
  static targets `nymph_imgui` (imgui core + implot + implot3d sources, no
  backends) and `yaml` (libyaml's own CMake, tests and install off); an
  interface target `nymph_stb` for the stb include path. All in the
  `extern` IDE folder.
- Root `CMakeLists.txt` includes it; `src/CMakeLists.txt` links `nymph`
  to them.
- `docs/building.md`: dependency table.

**Tests:**
- `tests/test_dependencies.cpp`: creates and destroys an ImGui, ImPlot and
  ImPlot3D context; runs one `NewFrame()` / `Render()` with a fixed
  display size and an Alpha8 atlas built; parses a one-line YAML string
  with libyaml.
- A fresh clone without `--recursive` configures and builds (manual check,
  recorded in the step's result).

### 2. Software rasterizer (adapted imgui_sw)

**Goal:** `ImDrawData` from ImGui 1.92 becomes correct, repeatable RGBA8
pixels on the CPU, using an adapted copy of emilk's
imgui_software_renderer.

**Deliverables:**
- `src/render/imgui_sw.hpp`/`.cpp`: copied from
  `emilk/imgui_software_renderer` commit `b5ae63a` (`src/imgui_sw.*`), with
  a header stating the origin, its public-domain licence, and the changes
  of DESIGN.md section 4 (`imgui_sw`), items 1-7. First commit the copy
  as is plus only the formatting, then the changes, so the diff against
  upstream stays reviewable.
- `src/render/texture_store.hpp`/`.cpp`: Alpha8 textures by id;
  `apply(ImTextureData&)` for `WantCreate` / `WantUpdates` /
  `WantDestroy`, setting `TexID` and status.
- `src/render/image.hpp`: `struct image {int width, height;
  std::vector<uint8_t> rgba;}`.
- `THIRD_PARTY.md` (repository root): imgui_sw's origin and licence text.

**Tests** (`tests/test_imgui_sw.cpp`, hand-built `ImDrawList`s with a
small Alpha8 test texture whose white pixel is not at (0, 0), no ImGui
frame):
- An opaque axis-aligned rectangle covers exactly its pixels, with its
  colour, and nothing else changes.
- Two triangles forming a square with alpha 0.5 give one uniform colour:
  no pixel along the shared diagonal is blended twice.
- A triangle partly outside its clip rectangle changes no pixel outside it.
- Blending a known colour at known alpha over a known background gives the
  expected bytes (within 1 per channel).
- A textured quad mapped 1:1 onto the texture reproduces its texels at the
  right pixels (catches the texel-lookup fix).
- A solid rectangle that samples the moved white pixel is painted solid.
- A draw list over 65,536 vertices with `VtxOffset` set draws the same
  pixels as the same shapes split into small lists.
- Two textures in one frame are each sampled from their own pixels.
- `texture_store` applies `WantCreate`, then a `WantUpdates` rectangle,
  then `WantDestroy`, with the expected pixels and statuses.
- A display of 10x10 painted into a 20x20 target draws a rectangle twice
  as large.
- A `ResetRenderState` callback is ignored; any other callback throws.
- Painting the same draw data twice gives identical bytes.

### 3. Offscreen frame driver

**Goal:** a drawing function rendered through a private ImGui / ImPlot /
ImPlot3D context comes out as an image, with no window or display.

**Deliverables:**
- `src/render/figure_renderer.hpp`/`.cpp`, first form:
  `render_frames(const image_options&, std::function<void(draw_context&)>)
  -> image`. Context trio created and destroyed per call (RAII guard),
  `AddFontDefaultVector()` at 16 px, Alpha8 atlas, theme (`light`, `dark`,
  `classic`) with `AntiAliasedLinesUseTex = false`, the full-display
  window with `padding`, fixed `DeltaTime`, mouse at `-FLT_MAX`, three
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
- Non-PNG bytes, a PNG without the chunk, and a PNG with one corrupted CRC
  all give `nullopt`.
- The pixels decode back (with `stb_image`, test-only) to the input image.

### 5. YAML document with positions

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

**Goal:** `Plot` and every 2D series from wish render with wish's fields
and defaults.

**Deliverables:**
- `src/elements/plot2d/`: `Plot` and the 14 series classes, their field
  tables, flag/enum tables and descriptions copied from wish's
  registrations, and `draw()` ported from `imgui_plot_renderer.cpp` and
  `imgui_plot_style.hpp`; `register_plot2d()`.
- `src/elements/register_builtin.cpp`: `register_builtin_elements()`.
- `src/nymph.hpp`: `render_png()` using the built-in registry.
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

### 11. Command line

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

### 12. wish nymph images

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
- `chart.nymph` (it uses `VerticalLayout`) converts, and rendering it
  reports the unknown type at its line, until layouts exist.

### 13. Documentation and examples

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
  surface) and `docs/examples.md` showing each with its PNG.
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
- [ ] Every 2D and 3D element type and field of wish nymph is accepted
      with the same name and default.
- [ ] README, `docs/`, CHANGELOG and DESIGN.md match the code.
