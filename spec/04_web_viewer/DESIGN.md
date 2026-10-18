# nymph — Interactive Web Viewer: Design

**Status: designed, not started.** [PLAN.md](PLAN.md) is the ordered
implementation plan. This document describes only what this project adds
or changes; the rest of the architecture is
[spec/01_core_features/DESIGN.md](../01_core_features/DESIGN.md)
(*core DESIGN*), [spec/02_opengl_rasterizer/DESIGN.md](../02_opengl_rasterizer/DESIGN.md)
(*GL DESIGN*) and [spec/03_uml_diagrams/DESIGN.md](../03_uml_diagrams/DESIGN.md)
(*UML DESIGN*).

## 1. Purpose / Scope

nymph turns a YAML source into a still PNG. This project adds a **viewer**:
it takes a nymph YAML source, or a nymph PNG (which carries its YAML),
and draws the chart live, redrawing on every input, so the user can
interact with it:

- pan and zoom plots, rotate 3D plots, pan and zoom diagrams;
- hover a data point, bar, cell, task, flow or diagram node to see its
  values in a tooltip, and report it to the host page;
- update the data frequently (a live feed) without losing the user's
  zoom.

The viewer is meant to be embedded in other tools, which are mostly web
based: markdown viewers, chatbots, notebooks, documentation sites. So the
deliverable is a **WebAssembly build of nymph with a small JavaScript
API**, drawing with **WebGL2** (OpenGL ES 3.0), which is real-time,
GPU-accelerated and supported by every current browser.

```html
<script type="module" src="nymph-viewer.js"></script>
<nymph-viewer src="chart.yaml"></nymph-viewer>
<nymph-viewer src="chart.png" fit="width" theme="auto"></nymph-viewer>
```

```js
import { createViewer } from "./nymph-viewer.js";
const v = await createViewer(document.getElementById("box"), { source: yamlText });
v.on("hover", (pick) => console.log(pick.label, pick.x, pick.y));
setInterval(() => v.setData({ revenue: nextValues() }), 250);
```

Hard requirements, from the request:

1. **Input**: a nymph YAML source or a nymph PNG.
2. **Real time**: drawn with OpenGL (WebGL2), redrawn on every input or
   data change.
3. **Interaction** with the mouse (and touch): pan, zoom, data point
   values.
4. **Lightweight and embeddable**: one ES module and one `.wasm` file, no
   framework, no server, any number of viewers on a page.

In scope: a platform-neutral live view in the library (persistent
contexts, input, picking), the interaction behaviour of each element
family, the Emscripten build, a WebGL2 presenter with a software
fallback, the JavaScript API and the `<nymph-viewer>` custom element.

Out of scope: a native desktop viewer window (section 9 says how it would
fit), editing the source from the viewer, keyboard navigation of data
points (section 9), WebGPU, and any change to the source format beyond
what section 3 lists.

### Feasibility (measured before this design)

The library, unchanged, was built with Emscripten 4 (`emcmake cmake
-DNYMPH_OPENGL=OFF`, `-fwasm-exceptions`) and run in Node:

- every example rendered; `dashboard.yaml` gave **the same bytes** as the
  committed PNG; `uml_class`, `sankey` and `surface` differed in 1 to 4
  pixels by one colour level (libm float differences);
- 0.15 to 0.45 s per command, including module start-up;
- `nymph.wasm` (CLI included) 4.8 MB, **2.1 MB gzip**; 2.8 MB of it is
  the embedded fonts.

Dear ImGui 1.92.9's `imgui_impl_opengl3` supports WebGL2 under Emscripten,
including the dynamic font atlas (`ImGuiBackendFlags_RendererHasTextures`).

## 2. Design Goals

1. **Same picture as the PNG.** Before any input, the viewer shows what
   `nymph chart.yaml` would draw at that size: same frame driver, layout,
   elements and theme. Only the rasterizer differs (GL DESIGN goal 1
   tolerance).
2. **PNG output does not change.** Everything interactive is off unless a
   live view turns it on. `render_png()` and the CLI keep their bytes
   (core DESIGN goal 3); every existing test passes unmodified.
3. **Testable without a browser.** The interaction logic lives in the
   library (`live_view`), driven by synthetic input and checked natively
   with GoogleTest and the software rasterizer. The web layer only maps
   browser events and presents pixels.
4. **Cheap when idle.** A page with twenty charts uses no CPU or GPU while
   nobody touches them: frames run only after input, a data change or a
   resize.
5. **No I/O in the module.** The wasm module never reads files or the
   network; the host passes source text or PNG bytes. Sources are
   untrusted (often written by an AI) and keep the library's limits.
6. **Small.** One shared module per page; the gzip size of
   `nymph-viewer.wasm` stays at or below 2.5 MB.

## 3. Behaviour

### Interaction per element family

| Element | Pointer interaction | Hover / pick payload |
|---|---|---|
| `Plot` | ImPlot's own: drag pans, wheel zooms (over an axis: that axis only), right-drag box-zooms, double-click fits, legend click hides a series. | Nearest point of any series within 8 px: series label, index, x, y. |
| `PlotLine`, `PlotScatter`, `PlotStairs`, `PlotStems`, `PlotShaded`, `PlotDigital`, `PlotErrorBars` | (inside `Plot`) | x, y of the nearest point (error bars: also the error). |
| `PlotBars`, `PlotBarsH`, `PlotBarGroups`, `PlotHistogram` | | The bar under the pointer: group / item label, value (histogram: bin range and count). |
| `PlotHeatmap`, `PlotHistogram2D` | | The cell: row, column, value. |
| `PlotPieChart` | | The slice: label, value, share. |
| `Plot3D` | ImPlot3D's own: drag rotates, wheel zooms, double-click resets. | None in this project (section 9). |
| `GanttChart` | Drag / wheel pans and zooms the time axis only. | The task: name, start, end, section. |
| `SankeyDiagram` | None. | The node (name, total) or flow (from, to, value). |
| UML diagrams, `Flowchart` | Drag pans, wheel zooms around the pointer, double-click fits (new in ImUml). | The node or edge: its `id`, kind, label. |
| Layouts, `Label`, separators, `ColorScale` | None. | None. |

ImPlot's right-click context menus are left as the plot's `flags` field
says (`NoMenus` turns them off), since authors may already set it; the
viewer adds no menu of its own.

A tooltip is drawn by nymph itself (an ImGui tooltip window), so it looks
the same in every host. The host also receives the pick (section 5,
events) and may show its own UI instead; `tooltips: false` turns nymph's
off.

### Axis limits: the one source-format consequence

`Plot` and `GanttChart` set limits written in the source with
`ImPlotCond_Always` (`src/elements/plot2d.cpp`, `gantt.cpp`), which
re-applies them every frame and would undo every pan and zoom. In a live
view they are applied with `ImPlotCond_Once` instead, and again for one
frame after `resetView()`. Off a live view nothing changes. No YAML field
is added: a source that must not move sets `NoInputs` in `flags`, which
already exists.

Plots without limits are fitted by ImPlot on their first frame, as today.
After that, new data outside the visible range is not followed unless the
axis has the `AutoFit` flag (also existing) or the user double-clicks.
This is ImPlot's behaviour and is documented, not changed.

### Sizing

The viewer has a CSS size (its box on the page) and a device pixel ratio.
`fit` decides how the source's `image.width` / `height` relate to it:

- `source` (default): the layout runs at `image.width` x `height`, as the
  PNG. The canvas keeps that aspect ratio and shrinks with CSS when its
  box is narrower; pointer positions are mapped back to layout pixels.
- `width`: the layout width follows the box width; the height is
  `image.height` scaled by the same factor (the aspect ratio is kept).
- `fill`: the layout fills the box in both directions.

In `width` and `fill`, a box smaller than the layout needs (an overflow,
core DESIGN section 4, layout engine) is handled by laying out at the
source size and shrinking with CSS, as `source` does. `image.scale` is
ignored: the framebuffer scale is the device pixel ratio, capped at 3 and
by the 8192 px limit of `render_frames()`.

### Theme

`theme` is `source` (default: `image.theme`), `light`, `dark`, `classic`,
or `auto` (light or dark from the page's `prefers-color-scheme`, followed
live). It overrides `image.theme` only; colours written in the source
stay.

### Updates

- `setSource(yaml)` replaces the chart. The view state (zoom, hidden
  series, diagram pan) is kept for elements whose ImGui ID path is
  unchanged: ImPlot and ImUml key their state by ID, and nymph's IDs are
  the element's title and child index (core DESIGN section 4,
  `draw_context::draw_children`). `{ resetView: true }` drops it.
- `setData(tables)` replaces the source's top-level `data` value with
  `tables` (a JSON object, which is YAML) and binds again; everything else
  in the source stays. Binding is milliseconds for typical charts, so a
  feed of several updates per second is fine; section 9 lists an append
  path for larger data.
- A source that fails to bind throws a `NymphError` (`line`, `column`,
  `message`, the library's error text) and **keeps the previous chart on
  screen**. If there is no previous chart, the viewer shows the error as
  text in its box.

### PNG input

`setPng(bytes)` calls `extract_source()`. A nymph PNG behaves like its
YAML. A PNG without a nymph chunk is drawn as a plain image (by the
browser, through `createImageBitmap`) with no interaction, and `info()`
says so. A corrupt PNG throws.

### Accessibility

The canvas gets `role="img"` and an `aria-label` from the source's
`description` (core DESIGN section 3), so screen readers read what the
chart shows.

## 4. Key Abstractions

```
 host page ──► nymph-viewer.js ──────────────► nymph-viewer.wasm (one per page)
               (API, custom element,            │
                pointer/wheel/touch events,     ├─ nymph_web.cpp: C exports, viewer table
                ResizeObserver, frame           │
                scheduling, blit to canvas)     ├─ live_view ── bind_source() ── figure
                     ▲                          │   (persistent ImGui / ImPlot / ImPlot3D /
                     │ picks, view, errors      │    ImUml contexts, input, picks)
                     └──────────────────────────┤        │ ImDrawData
                                                └─ presenter: webgl_presenter (shared WebGL2
                                                   context, imgui_impl_opengl3) or
                                                   software_presenter (soft_rasterizer → RGBA)
```

### Frame setup (`src/render/frame_setup.hpp/.cpp`)

The context scope, `configure()`, `load_fonts()`, the theme table and
`set_frame_inputs()` move out of `figure_renderer.cpp` into this internal
file, unchanged, so `render_frames()` and `live_view` share them. Two
additions: `configure()` takes the renderer flags to set (a WebGL
presenter without vertex offsets, below), and the frame inputs take the
mouse state instead of always `-FLT_MAX`.

### `live_view` (`src/view/live_view.hpp/.cpp`)

The heart of the project, platform-neutral, built in every configuration
and unit-tested natively.

```cpp
namespace bdg::nymph {
struct view_size { float width, height, pixel_ratio; };  // CSS px, DPR
enum class view_fit { source, width, fill };
struct view_options { view_fit fit; std::string theme; bool tooltips; };

struct pointer_input {           // one event, in CSS px of the view
  enum kind { move, down, up, wheel, leave } type;
  float x, y; int button; float wheel_x, wheel_y; bool touch;
};

struct pick {                    // what is under the pointer
  std::string element;           // element path, e.g. "root/children/1/children/0"
  std::string type;              // "PlotLine"
  std::string label;             // series label or diagram id
  int index;                     // point / bar / cell / node index, -1 if none
  std::vector<std::pair<std::string, std::string>> values; // "x" -> "3", "y" -> "15.2"
};

struct axis_range { std::string element; int axis; double min, max; };

class live_view {
 public:
  explicit live_view(view_options options);
  void set_source(std::string_view source, bool reset_view = false); // throws nymph::error
  void set_png(std::span<const uint8_t> png);   // throws; no nymph chunk -> has_figure() false
  void set_data(std::string_view data_yaml);    // throws nymph::error
  void resize(view_size size);
  void input(const pointer_input& event);
  void reset_view();

  /// Runs one frame if one is due; returns its draw data or nullptr.
  const ImDrawData* frame(double now_seconds);
  bool wants_frame() const;     // input, data or settling frames pending
  ImU32 clear_color() const;

  std::optional<pick> hovered() const;
  std::optional<pick> take_click();             // last click, once
  std::vector<axis_range> view_state() const;   // every plot axis, after the last frame
  bool has_figure() const;
  const std::string& description() const;
  std::string_view source() const;              // with set_data() applied
  const texture_store& textures() const;        // for the software presenter (below)
};
}
```

Rules:

- **Persistent contexts.** The live view owns one ImGui, ImPlot,
  ImPlot3D and ImUml context for its lifetime (with its own font atlas)
  and makes them current only inside its own calls, restoring the
  previous ones afterwards, like `render_frames()`. Several live views
  can exist on one thread; none may be used from two threads at once.
- **Frames on demand.** A frame is due after input, `set_*`, `resize`,
  `reset_view`, and for a few settling frames after any of them (ImPlot
  fits one frame late; tooltips and hover fades need a frame or two).
  ImGui is told the real time between frames (`DeltaTime`), clamped.
- **Static parity.** After `set_source()` and three frames with the
  pointer outside the view, `fit: source`, pixel ratio = `image.scale`,
  the draw data equals `render_figure()`'s, so the software rasterizer
  gives the same bytes. This is the test that keeps goal 1 honest.
- **Picks** come from `draw_context` (below), recomputed each frame
  while the pointer is over the view.
- **Textures.** The live view keeps a `texture_store`
  for the software presenter and leaves the `ImTextureData` requests in
  place for a GPU presenter, which answers them itself during its render
  (ImGui 1.92's protocol, core DESIGN section 4).

### Interactive drawing (`draw_context` additions)

Elements learn they are live through the `draw_context`, so the PNG path
pays nothing:

- `bool interactive() const` — false in `render_frames()`.
- `ImPlotCond limits_cond() const` — `Always` unless interactive; in a
  live view `Once`, or `Always` for the frame after `reset_view()`.
- `pick_tracker& picks()` — a per-frame candidate list. A series, while
  its plot is hovered (`ImPlot::IsPlotHovered()`), offers its nearest
  point as `{distance in px, pick}`; bars, cells and slices offer a hit
  with distance 0. The plot keeps the best candidate within 8 px after
  its children, draws the tooltip and a highlight ring, and hands the pick
  to the live view. Draw-list elements (Sankey, Gantt bars, diagram
  nodes) offer hits from their own rectangles. All of it is skipped
  unless `interactive()`.
- Hit testing reads the node's bound data (literal after binding, core
  DESIGN section 4, `figure`); it is O(points) per hovered series per
  frame, and only while the pointer moves.

### ImUml view transform (`imuml/`)

ImUml fits each diagram to its rectangle (UML DESIGN, fit-to-rectangle
zoom). It gains a per-diagram view (`Pan`, `ZoomFactor`) kept in the
diagram's pool entry next to the layout cache, applied on top of the fit:
wheel zooms around the pointer (clamped to 0.25x to 8x of the fit), drag
pans, double-click resets. It is on only with a new
`ImUmlDiagramFlags_Interactive`, which nymph passes when
`dc.interactive()`; without it drawing is unchanged. `ImUmlDiagramInfo`
gains the hovered node / edge id. This is an ImGui-style API addition,
following ImUml's naming (CLAUDE.md, `imuml/` exception).

### Web module (`web/`)

- `web/nymph_web.cpp` — `extern "C"` exports over a table of
  `live_view`s keyed by an integer handle: create / destroy, set source /
  PNG / data (returning 0 or an error that `nymph_last_error()` returns as
  JSON), resize, pointer events, `frame()`, picks and view state as JSON,
  `to_png()`. Every export catches all exceptions (CLAUDE.md, C ABI
  boundaries). This is internal: the JavaScript module is the public
  contract.
- `web/webgl_presenter.cpp` — draws a live view's `ImDrawData` with
  Dear ImGui's `imgui_impl_opengl3` into **one WebGL2 context shared by
  every viewer** (a hidden canvas), then the JavaScript side copies the
  frame into the viewer's visible canvas (`drawImage` on a 2D context, or
  `transferToImageBitmap` where available). Each ImGui context initialises
  the backend once; the backend keeps its data per context, so viewers do
  not share GL textures by accident.
- `web/software_presenter.cpp` — the existing `soft_rasterizer`, returning
  RGBA that JavaScript puts in the canvas with `putImageData`. Used when
  WebGL2 is unavailable or the context is lost, and in Node tests.
- `web/js/nymph-viewer.js` — the ES module: loads the wasm once (on the
  first viewer), `createViewer()`, the `NymphViewer` class, the
  `<nymph-viewer>` element, event mapping, frame scheduling.

## 5. Public Contract: the JavaScript API

```ts
createViewer(container: HTMLElement, options?: {
  source?: string;            // YAML text
  png?: Uint8Array;           // or a nymph PNG
  fit?: "source" | "width" | "fill";
  theme?: "source" | "light" | "dark" | "classic" | "auto";
  tooltips?: boolean;         // default true
  rasterizer?: "auto" | "webgl" | "software"; // default auto
  wasmUrl?: string;           // default: next to nymph-viewer.js
}): Promise<NymphViewer>;

class NymphViewer {
  setSource(yaml: string, opts?: { resetView?: boolean }): void; // throws NymphError
  setPng(bytes: Uint8Array): void;
  setData(tables: object): void;                                 // throws NymphError
  resetView(): void;
  on(event: "hover" | "click" | "view" | "error", cb: (detail) => void): () => void;
  info(): { rasterizer: "webgl" | "software"; hasFigure: boolean;
            description: string; size: { width: number; height: number } };
  toPng(): Uint8Array;        // render_png() of the current source and data: YAML embedded
  destroy(): void;
}

class NymphError extends Error { line: number; column: number; }
```

- **Events.** `hover` fires when the pick changes (`null` when it ends);
  `click` on a primary-button click with a pick; `view` after a pan or
  zoom settles, with every plot axis range; `error` when a deferred update
  fails. The custom element re-dispatches them as DOM `CustomEvent`s
  (`nymph-hover`, ...).
- **`<nymph-viewer>`** attributes: `src` (URL of a `.yaml` or `.png`,
  fetched by the element, not the wasm), `fit`, `theme`, `tooltips`;
  without `src`, the element's text content is the YAML, so a markdown
  renderer can turn a ```` ```nymph ```` fenced block into
  `<nymph-viewer>…</nymph-viewer>`. Its `viewer` property is the
  `NymphViewer`.
- **Input mapping.** Pointer events (mouse, pen, touch) with pointer
  capture during drags; `wheel` with `preventDefault()` only while the
  pointer is over a plot or diagram that takes input, so page scrolling is
  not hijacked over labels and margins; a two-finger pinch becomes wheel
  zoom at its centre; a double tap is a double click.
- **Scheduling.** `requestAnimationFrame` only while some viewer
  `wants_frame()`; `ResizeObserver` for size and pixel-ratio changes;
  `IntersectionObserver` pauses viewers off screen.
- **Hosting.** Two files, `nymph-viewer.js` and `nymph-viewer.wasm`,
  served together (or a single-file build with the wasm inlined, about a
  third larger, for hosts that cannot serve a second file). A page's
  Content Security Policy must allow `'wasm-unsafe-eval'`.

## 6. Build

- New option `NYMPH_WEB` (default OFF; only meaningful with the
  Emscripten toolchain, an error otherwise). It adds `web/` and turns
  `NYMPH_OPENGL` off (the EGL / WGL loader does not apply; WebGL comes
  from Emscripten's GL library, which is linked, unlike the native rule of
  GL DESIGN section 1, because a browser always provides it).
- The CLI sources (`src/cli/`) move from the `nymph` library to a
  `nymph_cli` library used by `app/` and `test_cli`, so the web module
  does not link `std::filesystem` and the CLI.
- Flags: `-fwasm-exceptions` (native wasm exception handling, in every
  current browser), `-sMODULARIZE -sEXPORT_ES6`, `-sALLOW_MEMORY_GROWTH`,
  `-sFILESYSTEM=0`, `-sMAX_WEBGL_VERSION=2 -sMIN_WEBGL_VERSION=2`,
  `-Oz` with `--closure 1` for the JS glue.
- `ImDrawIdx` is `unsigned int` in the web build (a public compile
  definition on `nymph_imgui`): WebGL2 has no `glDrawElementsBaseVertex`,
  so `imgui_impl_opengl3` cannot set `RendererHasVtxOffset` there, and
  plots above 65,535 vertices would otherwise fail; WebGL2 draws 32-bit
  indices natively. The native build keeps 16-bit indices.
- Output in `build/web/`: `nymph-viewer.js`, `nymph-viewer.wasm`,
  `nymph-viewer.single.js`, and `demo/index.html`.

## 7. Testing Strategy

1. **Native GoogleTest** (every configuration, `ctest`): `live_view`
   static parity with `render_figure()`; synthetic drags and wheels change
   `view_state()` and `reset_view()` restores it; `set_data()` keeps the
   view; picks for every element family at known pixel positions; PNG
   bytes of every example unchanged (the existing tests); ImUml pan and
   zoom.
2. **Node** (when the Emscripten toolchain is configured): the built
   module with the software presenter renders every example within the
   GL DESIGN tolerance of the committed PNGs, reports bind errors with
   positions, and returns picks for synthetic input.
3. **Browser** (Playwright with the preinstalled Chromium, WebGL2 through
   SwiftShader): the WebGL presenter matches the software presenter within
   tolerance; dragging a plot fires `view`; hovering a point fires `hover`
   with its values and shows the tooltip; `<nymph-viewer src>` loads YAML
   and PNG; a bad source shows the error; twenty viewers on one page use
   one WebGL context and go idle.

## 8. Design Decisions

- **Live view in the library, not in the web glue.** All interaction
  logic is plain C++ over ImGui, so it is tested natively, in milliseconds,
  without a browser, and a native viewer (section 9) reuses it. The web
  layer stays a thin adapter.
- **WebGL2 over WebGPU.** The request prefers OpenGL; WebGL2 is OpenGL ES
  3.0, is available in all current browsers, and ImGui's OpenGL3 backend
  supports it. WebGPU support is still incomplete across browsers and
  would add a second GPU code path.
- **ImGui's `imgui_impl_opengl3`, not nymph's `gl_backend`.** nymph's
  OpenGL rasterizer draws offscreen and reads pixels back, uses desktop
  GL 3.3 and `glDrawElementsBaseVertex`, and loads GL at run time. A live
  viewer presents to the screen, needs GLSL ES 3.00 and gets GL from the
  browser. The stock backend already does exactly that and is maintained
  upstream.
- **One shared WebGL context, frames copied to each canvas.** Browsers
  cap live WebGL contexts per page (about 16), and a chat transcript can
  hold more charts than that. One context draws every viewer in turn into
  a hidden canvas; a copy to a 2D canvas costs microseconds at chart
  sizes and only happens on the frames that run.
- **Re-bind on `setData`, no incremental data path (yet).** Binding is
  the single validator of content (core DESIGN section 4, `figure`), so
  updates stay checked and simple. It costs milliseconds for typical
  data; an append path is an extension, not a requirement.
- **`ImPlotCond_Once` only in live views.** Keeps the PNG contract and
  the source format unchanged; the existing `NoInputs` flag covers charts
  that must not move.
- **No I/O in wasm.** The element fetches `src` in JavaScript, under the
  page's own origin rules; the module only sees bytes. This keeps the
  library's "no file access in `bind()`" property and the attack surface
  of untrusted sources unchanged.
- **Alternatives rejected.** SVG output with JavaScript interaction (a
  second renderer that would not match the PNG); a JavaScript chart
  library fed from nymph's YAML (duplicates every element); rendering on
  a server and streaming images (needs a server, not real time).

## 9. Extension Points

- **Native viewer window**: `nymph view chart.yaml` with a GLFW or SDL
  window and `imgui_impl_opengl3` around the same `live_view`.
- **Worker rendering**: run the module in a Web Worker with
  `OffscreenCanvas`, so heavy layout never blocks the host page.
- **Fonts on demand**: embed only the default font and fetch the others
  by name the first time a source uses them (about 2.5 MB less to load).
- **Appending data**: `appendData(table, rows)` that extends bound
  columns in place, for feeds of large series.
- **3D picking** through ImPlot3D's projection, **keyboard navigation**
  of data points, and an accessible data-table view.
