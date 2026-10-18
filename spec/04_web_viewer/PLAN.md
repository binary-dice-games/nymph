# nymph — Interactive Web Viewer: Implementation Plan

See [DESIGN.md](DESIGN.md) for the design this plan implements, and
[spec/01_core_features/DESIGN.md](../01_core_features/DESIGN.md) section 4
(`render_figure()`, `draw_context`, `figure`) for the code it builds on.

Steps are ordered so the library comes first and is tested natively
(nothing changes for PNG users), then interaction element by element, then
the WebAssembly build, then the browser layer. Every step leaves the
existing tests passing unmodified: the software rasterizer's bytes and
every committed example PNG must not change. Native tests are GoogleTest
files in `tests/`, added with `nymph_add_test()`.

Web steps need the Emscripten SDK (`emcmake`); their tests are registered
with CTest only in a `NYMPH_WEB` build and run with Node (step 9 on) and
Playwright with the preinstalled Chromium (steps 10 and 11). Each web
step records the Emscripten version it was verified with.

## Steps

### 1. Split the CLI and share the frame setup

*Result: done.* `src/render/frame_setup.hpp/.cpp` and `nymph_cli`.
`set_frame_inputs()` (fixed clock, no mouse) stayed in
`figure_renderer.cpp`: it is the PNG path's own input, and the live view
feeds ImGui events instead. All 340 existing tests pass unmodified.

**Goal:** `render_frames()` and a future live view share one frame setup,
and the `nymph` library no longer contains the command line; nothing
changes for users.

**Deliverables:**
- `src/render/frame_setup.hpp/.cpp` (internal): the context scope,
  theme table, `check_options()`, `configure()`, `load_fonts()`,
  and `image_text_style()` moved from `figure_renderer.cpp`.
  `configure()` takes the backend flags to set.
- `src/render/figure_renderer.cpp`: uses them; behaviour unchanged.
- `src/CMakeLists.txt`: `src/cli/` sources move to a new static library
  `nymph_cli` (links `nymph`); `app/CMakeLists.txt` and the `test_cli`
  entry link it.

**Tests:**
- All existing tests pass unmodified, including the byte-exact example
  renders in `test_docs`.

### 2. Font catalog and fonts on demand

*Result: done.* SHA-256 instead of CRC-32 (CMake's `file(SHA256)`
computes it at configure time; CMake has no CRC-32), with nymph's own
`src/core/sha256.cpp`. Instead of compiling the library a second time
with no fonts, the tests use a hook (`fonts/font_testing.hpp`,
`hide_embedded_fonts_for_testing()`) that makes the default build behave
like `NYMPH_EMBED_FONTS=none`; a real `none` build was checked by hand
(`nymph fonts` marks every font, the line example renders byte for byte,
the fonts example fails at `image.font`). `register_font()` is also
declared in `nymph.hpp`. 11 tests in `test_fonts_on_demand.cpp`; 351
tests pass.

**Goal:** every build knows every font name, a build can embed a subset
of the fonts, and the missing ones can be registered at run time with
their bytes checked; the default build is unchanged.

**Deliverables:**
- `cmake/NymphFonts.cmake`: cache variable `NYMPH_EMBED_FONTS` (`all`
  default, `none`, or a `;` list of file names; an unknown name is a
  configure error). The generated table holds every font's name, size and
  SHA-256 and, for embedded ones, its bytes; fonts not embedded are copied
  with the licence texts to `<build>/fonts/`.
- `src/fonts/embedded_fonts.hpp`, `fonts.cpp`: `font_entry`,
  `font_catalog()`, `register_font()`, `font_data()`; `find_font()` and
  `check_font_name()` work on the catalog; `embedded_fonts()` returns
  the embedded entries only.
- `src/fonts/fonts_used.hpp/.cpp`: `fonts_used(figure)` (with the
  `-Italic` siblings of fonts used inside diagrams) and
  `fonts_missing(figure)`.
- `src/render/figure_renderer.cpp`: `load_fonts()` adds every font with
  bytes; `render_figure()` throws `nymph::error` at the `font` field
  (or `image.font`) of the first element naming a font without bytes.
- `src/cli/cli.cpp`: `nymph fonts` marks fonts `(not embedded)`.
- `src/fonts/font_testing.hpp`: `hide_embedded_fonts_for_testing()`, so
  tests can run the on-demand path in the default build.
- `docs/building.md`, `fonts/README.md`: the option.

**Tests** (`tests/test_fonts_on_demand.cpp`):
- The catalog lists every file in `fonts/`, sorted, with the right size
  and SHA-256; in the default build every entry has bytes.
- `register_font()` rejects an unknown name, a wrong size and a wrong
  SHA-256, and accepts the right bytes; registering twice is harmless.
- `fonts_used()` of `examples/fonts.yaml` lists its fonts; a class
  diagram drawn in `FiraSans-Regular.ttf` also lists
  `FiraSans-Italic.ttf`.
- With embedded fonts hidden: binding a source naming `FiraSans-Bold.ttf`
  succeeds; `render_png()` throws a positioned `nymph::error` naming the
  font; after registering the file's bytes, it gives the same bytes as
  the committed `examples/fonts.png`. A source using only the default
  font renders without registering anything.
- All existing tests pass unmodified.

### 3. `live_view` with static parity

*Result: done.* `src/view/live_view.hpp/.cpp`; 14 tests in
`test_live_view.cpp`, the parity test covering all 24 examples
byte for byte. Additions to the plan: `set_theme()` (the override,
resolved from `theme: auto` by the JavaScript side), `geometry()` (layout
size, CSS and framebuffer scales, `fit_fallback`), and a layout check at
the source size in `set_source()` (`layout_figure()`, separate contexts,
so a failing update keeps the view state). A fit that overflows at the
box size is found by the frame itself: the contexts are recreated and
the view lays out at the source size until the next resize. 365 tests
pass.

**Goal:** a `live_view` keeps its contexts across frames, runs frames on
demand at a given size and pixel ratio, and with no input draws exactly
what `render_figure()` draws.

**Deliverables:**
- `src/view/live_view.hpp/.cpp`: `view_options`, `view_size`,
  `live_view` with `set_source()`, `resize()`, `frame()`,
  `wants_frame()`, `clear_color()`, `has_figure()`, `description()`,
  `source()`, `textures()` (DESIGN.md section 4). Persistent ImGui,
  ImPlot, ImPlot3D and ImUml contexts, made current only inside its calls.
  `fit` (`source`, `width`, `fill`) and the `theme` override.
- `src/render/draw_context.hpp`: `interactive()` (false by default).
- `src/CMakeLists.txt`: the new files.

**Tests** (`tests/test_live_view.cpp`):
- For every example source: `set_source()`, `resize({width, height,
  scale})`, three `frame()` calls, software-rasterized: the same bytes as
  `render_figure()`.
- `wants_frame()` is true after `set_source()` and `resize()`, and false
  once the settling frames have run; `frame()` then returns nullptr.
- A bind error throws `nymph::error` with its position and keeps the
  previous figure (`has_figure()` stays true, the next frame draws it).
- In the no-fonts build, a source naming an unregistered font throws
  `fonts_needed` listing it and keeps the previous figure; after
  `register_font()` the same `set_source()` succeeds and the font is
  drawn.
- `fit: width` at half the source width lays the root out at half width
  (rectangles from the frame); a box too small for the layout falls back
  to the source size.
- Two live views alternate frames on one thread without disturbing each
  other's output; the contexts current before each call are current
  after it.

### 4. Pointer input, axis limits and view state

*Result: done.* `pointer_event`, `plot_view` (plot area in CSS pixels
plus limits, per element path), `input()`, `reset_view()` (recreates the
contexts), `view_state()`; `draw_context::limits_cond()`,
`record_current_plot()`. Found while testing: with `ImPlotCond_Once` from
the first frame, the `Equal` pie charts of `pie` and `dashboard` settled
differently from the PNG, so source limits stay `Always` while fresh
contexts settle. Gantt date ticks follow the range on screen. 9 tests in
`test_live_view.cpp` (`live_view_input`); 374 tests pass.

**Goal:** plots pan and zoom under synthetic pointer input in a live view,
the view can be reset and read back, and PNG output is unchanged.

**Deliverables:**
- `live_view::input()`, `reset_view()`, `view_state()`; pointer events
  fed to ImGui (`AddMousePosEvent`, `AddMouseButtonEvent`,
  `AddMouseWheelEvent`, `AddMouseSourceEvent` for touch), with real
  `DeltaTime`.
- `draw_context::limits_cond()`; `plot2d.cpp` and `gantt.cpp` use it
  instead of `ImPlotCond_Always`. The Gantt chart locks its Y axis
  (time axis only).
- `view_state()` collects every `Plot` axis range after each frame
  (`ImPlot::GetPlotLimits()` inside the plot, recorded by element path).

**Tests** (`tests/test_live_view.cpp`):
- A drag across a plot moves its X and Y ranges by the dragged amount
  (within a pixel); a wheel step over the plot shrinks both ranges around
  the pointer; over the X axis only X changes.
- A plot with `x_min` / `x_max` keeps the user's pan on later frames,
  and `reset_view()` restores the source limits.
- `set_source()` with the same structure and new data keeps the view;
  with `reset_view = true` it does not.
- A plot with `flags: NoInputs` does not move.
- A Gantt chart pans in time; its rows do not move.
- All PNG tests pass unmodified.

### 5. Picks and tooltips for XY series

*Result: done.* `pick`, `pick_candidate` and the pick calls live in
`render/draw_context.hpp` (elements need them; `src/view/` depends on
`render/`, not the other way), and `src/elements/plot_pick.hpp/.cpp`
holds the plot helpers (`offer_nearest_point()`, `format_axis_value()`:
category tick labels, ISO dates on time axes, the axis format,
`finish_plot_picks()`: ring and tooltip). `live_view` reports
`view_pick` (with the element path) through `hovered()` and
`take_click()`. Found while testing: an input needs three frames, not
two (ImPlot sees the hover a frame late; ImGui hides a new tooltip on its
first frame). Error bars win ties with their line and add the error.
11 tests in `test_pick.cpp`; the test driver moved to
`tests/live_view_driver.hpp`. 385 tests pass.

**Goal:** hovering a data point of a 2D series reports it and shows a
tooltip, in a live view only.

**Deliverables:**
- `src/view/pick.hpp`: `pick`, `pick_tracker` (per-frame candidates,
  best within 8 px).
- `draw_context::picks()`; `xy_series` and `PlotErrorBars` offer their
  nearest point when the plot is hovered; `Plot` resolves the best
  candidate after its children, draws the tooltip (label, x, y, formatted
  as the axes format them, dates on time axes) and a highlight ring.
- `live_view::hovered()`, `take_click()`; `view_options::tooltips`.

**Tests** (`tests/test_pick.cpp`):
- Pointer on the pixel of point 2 of a `PlotLine` → pick with that
  series' label, index 2, its x and y; 20 px away → no pick.
- Two series near the pointer → the closer one wins.
- A time-axis plot formats x as an ISO date in the pick.
- A click fires `take_click()` once.
- With `tooltips: false` the pick is reported and nothing extra is drawn
  (draw data equal to a frame without the pointer).
- A hidden series (legend click) is not picked.

### 6. Picks for bars, cells, slices, Gantt and Sankey

*Result: done.* Bars and bar groups (side by side and stacked) through
`offer_bars()`, histograms from ImPlot's own bin centres and heights
(left in its scratch buffers), heatmap and 2D histogram cells, pie
slices (ImPlot's angle and normalisation rules), Gantt tasks and
milestones (the task element's path, ISO dates, section) and Sankey
nodes and flows (`finish_picks()` for draw-list elements). Rectangle
hits are highlighted with an outline. 10 tests in `test_pick.cpp`
(`pick_shapes`); 395 tests pass.

**Goal:** every 2D element family in DESIGN.md section 3 reports what is
under the pointer.

**Deliverables:**
- Picks in `PlotBars`, `PlotBarsH`, `PlotBarGroups`, `PlotHistogram`,
  `PlotHeatmap`, `PlotHistogram2D`, `PlotPieChart` (`plot2d.cpp`),
  `GanttChart` (`gantt.cpp`), `SankeyDiagram` (`sankey.cpp`, nodes and
  flows from the layout it already computes).

**Tests** (`tests/test_pick.cpp`):
- One test per element: the pointer at the centre of a known bar, cell,
  slice, task, node and flow gives the documented payload; in a gap,
  none.

### 7. Pan, zoom and picks in diagrams

*Result: done.* `ImUmlDiagramFlags_Interactive`; the view (`ViewPan`,
`ViewZoom`) lives on the pooled `ImUmlDiagram`; `FindHovered()` in
`imuml_items.cpp` hit-tests in drawing order (plain nodes, edges along
their flattened paths, composite nodes). nymph's `diagram_class` passes
the flag in live views and maps the hashed ids to its elements
(`offer_hovered()`). Sequence diagrams pan and zoom but report no hover
(their participants are not placed through the node records); left for
later. 6 tests in `test_imuml_interactive.cpp` (real ImGui mouse events
through the `frame_setup` pieces) and 1 in `test_pick.cpp`; 402 tests
pass.

**Goal:** UML diagrams and flowcharts pan, zoom around the pointer and
report the hovered node or edge, only when interactive.

**Deliverables:**
- `imuml/imuml.h`: `ImUmlDiagramFlags_Interactive`; hovered node / edge
  ids in `ImUmlDiagramInfo`.
- `imuml/imuml.cpp`, `imuml_internal.h`, `imuml_render.cpp`: per-diagram
  `Pan` / `ZoomFactor` in the pool entry, applied on top of the fit;
  wheel zoom about the pointer clamped to 0.25x-8x, drag pan, double-click
  reset; hit tests on node rectangles and edge polylines.
- `src/elements/uml/uml_common.cpp`: passes the flag when
  `dc.interactive()`, turns the hovered id into a pick and tooltip.
- `spec/03_uml_diagrams/DESIGN.md`: the view transform and the flag.

**Tests** (`tests/test_imuml_context.cpp`, `tests/test_pick.cpp`):
- Without the flag, draw data is unchanged (all UML tests pass
  unmodified).
- With it: a wheel step at a point keeps that point fixed on screen and
  scales the diagram; a drag moves it by the dragged amount; double-click
  restores the fit; zoom stays within its clamp.
- The pointer over a class box gives its id; over an edge, its two ends.

### 8. Data updates and PNG input

*Result: done.* Instead of a `bind_source()` overload, `replace_data()`
rewrites the source text (the `data` block replaced, every other line
kept; appended when absent; refused for a flow-style top level) and the
live view binds it with `set_source()`, so `source()` is exactly what
`toPng()` will embed. Errors inside the new data are re-positioned in
the caller's text with a "data: " prefix. `set_png()` returns false (and
clears the figure) for a PNG with no nymph chunk. 4 tests in
`test_bind.cpp`, 4 in `test_live_view.cpp`; 410 tests pass.

**Goal:** a live view replaces its data from JSON and opens nymph PNGs.

**Deliverables:**
- `src/document/bind.hpp/.cpp`: `bind_source()` overload taking a
  replacement for the top-level `data` value (YAML / JSON text), checked
  with the same rules and limits; errors point into the replacement
  ("data (from setData) line 1 column 14: ...").
- `live_view::set_data()`, `set_png()`.

**Tests** (`tests/test_live_view.cpp`, `tests/test_bind.cpp`):
- `set_data()` with new columns draws the new values, keeps the view and
  the rest of the source; `source()` returns the source with the new
  `data`.
- A `set_data()` naming a missing column throws and keeps the previous
  data on screen.
- `set_png()` of every example PNG gives the same frame as its YAML; a
  PNG without a nymph chunk leaves `has_figure()` false; a corrupt PNG
  throws.

### 9. Emscripten build with the software presenter

**Goal:** `NYMPH_WEB` builds `nymph-viewer.wasm` and a minimal JS loader,
and Node can drive a viewer end to end on the CPU.

**Deliverables:**
- `CMakeLists.txt`: option `NYMPH_WEB` (error without the Emscripten
  toolchain; forces `NYMPH_OPENGL` off, tests' native GoogleTest off);
  `ImDrawIdx=unsigned int` on `nymph_imgui` in this build.
- `web/CMakeLists.txt`: target `nymph-viewer` with the flags of
  DESIGN.md section 6.
- `web/nymph_web.cpp`: the C exports over a handle table, exceptions
  caught, errors as JSON.
- `web/software_presenter.cpp`.
- `web/js/nymph-viewer.js`: module loading and a `NymphViewer` subset
  working in Node (`setSource`, `setData`, frame to RGBA, picks).
- `docs/building.md`: the web build.

**Tests** (`web/tests/node/`, CTest `nymph_web_node`):
- Every example renders within the GL DESIGN tolerance of its committed
  PNG.
- A bad source throws `NymphError` with the same line, column and message
  as the native library.
- A synthetic hover over a known point returns its pick.
- `examples/fonts.yaml` renders after the loader fetches its fonts from
  `<build>/fonts/` (a file-system fetch in Node), and a font file with
  one byte changed is rejected.
- The gzip size of `nymph-viewer.wasm` is at most 1.5 MB.

### 10. WebGL2 presenter

**Goal:** viewers draw through one shared WebGL2 context and match the
software presenter.

**Deliverables:**
- `web/webgl_presenter.cpp`: `imgui_impl_opengl3` (from
  `extern/imgui/backends`) per ImGui context on one shared context over a
  hidden canvas; context-loss handling (fall back to software, recreate
  on restore).
- `nymph-viewer.js`: copies each finished frame to the viewer's canvas;
  `rasterizer` option; `info().rasterizer`.
- `web/demo/index.html`: every example, side by side with its PNG.

**Tests** (`web/tests/browser/`, Playwright, CTest `nymph_web_browser`):
- Every example in WebGL matches the software presenter within the GL
  DESIGN tolerance.
- A plot with more than 65,535 vertices draws (32-bit indices).
- Twenty viewers on a page create exactly one WebGL context.
- `rasterizer: "software"` and a page without WebGL2 both draw with the
  software presenter.

### 11. Browser input, scheduling and the custom element

**Goal:** a person can pan, zoom and hover in the browser, viewers go idle
when untouched, and `<nymph-viewer>` works from a tag.

**Deliverables:**
- `nymph-viewer.js`: pointer events with capture, wheel (default
  prevented only over interactive elements), pinch and double tap,
  `ResizeObserver` and pixel ratio, `requestAnimationFrame` only while a
  frame is wanted, `IntersectionObserver`, events (`hover`, `click`,
  `view`, `error`), `resetView()`, `toPng()`, `destroy()`, `theme:
  "auto"`, `aria-label`.
- `web/js/nymph-viewer-element.js` (bundled in the module):
  `<nymph-viewer>` with `src`, inline YAML text, `fit`, `theme`,
  `tooltips`, DOM events, error text in the box.
- Font fetching: `fontBaseUrl`, one fetch per font shared by all
  viewers, retry of the update after registration, queued updates in
  call order.

**Tests** (`web/tests/browser/`):
- Dragging across a plot fires `view` with moved ranges; the wheel over
  the plot zooms and does not scroll the page; the wheel over a label
  scrolls the page.
- Hovering a known point fires `hover` with its values and the canvas
  shows the tooltip (pixels change near the point).
- After input stops, no animation frame is requested within one second.
- `<nymph-viewer src="line.yaml">`, `src="line.png"` and inline YAML show
  the chart; a plain PNG shows as an image; a bad source shows its error.
- `toPng()` returns a PNG whose `extract_source()` equals the current
  source with its data.
- `theme="auto"` follows an emulated `prefers-color-scheme` change.
- Two viewers using the same font make one request for it; a 404 font
  rejects `setSource()` with the URL and keeps the previous chart.
- A viewer resized from 800 to 400 px wide with `fit="width"` lays out
  again at 400.

### 12. Packaging, documentation and close

**Goal:** the viewer ships as documented files, and every document
matches the code.

**Deliverables:**
- Single-file build `nymph-viewer.single.js`.
- `docs/viewer.md`: embedding (tag, module, markdown fenced blocks,
  chatbots), the API, events, interaction per element, CSP and hosting,
  limits; a row in README's "Further Documentation".
- `README.md`: the viewer in the overview and layout table (`web/`).
- `CHANGELOG.md`: the viewer and the `live_view` API under
  `[Unreleased]`.
- `spec/04_web_viewer/DESIGN.md`: status and any change made during
  implementation; `spec/01_core_features/DESIGN.md` section 4 points to
  `frame_setup`, `draw_context::interactive()` and `nymph_cli`.
- `spec/INDEX.md`: status and topics.

**Tests:**
- `ctest -j` passes three times in a row natively and in the `NYMPH_WEB`
  build.

## Completion Criteria

- [ ] All native tests pass under `ctest -j`, and every committed example
      PNG is reproduced byte for byte by the CLI and `render_png()`.
- [ ] A live view with no input draws the same bytes as
      `render_figure()` for every example (software rasterizer).
- [ ] Plots, Gantt charts and diagrams pan and zoom; 3D plots rotate;
      every 2D element family and diagrams report picks, natively and in
      the browser.
- [ ] `nymph-viewer.wasm` builds with Emscripten, embeds no font file,
      is at most 1.5 MB gzip, fetches only the fonts a chart uses, and its
      Node and Playwright tests pass.
- [ ] WebGL2 output matches the software presenter within tolerance;
      a page with twenty viewers uses one WebGL context and no frames
      while idle.
- [ ] `<nymph-viewer>` works from a `src` URL (YAML and PNG) and from
      inline YAML.
- [ ] Native builds still pass on Linux (GCC); MSYS2 and MSVC are not
      affected by `NYMPH_WEB` (the option is off and `web/` is not
      added).
- [ ] README, `docs/`, CHANGELOG, the DESIGN.md files and
      `spec/INDEX.md` match the code.
