# nymph — Interactive Web Viewer: Design

**Status: done (plan steps 1-12; see [PLAN.md](PLAN.md) for each step's
result). Verified on Linux with Emscripten 6.0.12, Node 22 and Chromium 141
(Playwright, WebGL2 through SwiftShader); native MSVC / MSYS2 builds, a
hardware GPU and other browsers are not yet verified.** [PLAN.md](PLAN.md) is the ordered
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
  zoom;
- load only the fonts a chart uses, when it first uses them, so the
  download stays small.

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
family, fonts on demand (a font catalog, a build option choosing which
fonts are embedded, run-time registration), the Emscripten build, a WebGL2 presenter with a software
fallback, the JavaScript API and the `<nymph-viewer>` custom element.

Added after the first release: the same viewer in a native window,
`nymph view` (section 10).

Out of scope: editing the source from the viewer, keyboard navigation of
data points (section 9), WebGPU, and any change to the source format
beyond what section 3 lists.

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
6. **Small.** One shared module per page. The web build embeds no font
   file (ImGui's built-in font is always there); each other font is
   fetched once, the first time a chart names it. `nymph-viewer-core.wasm`
   stays at or below 1.5 MB gzip.
7. **A source means the same everywhere.** Every build knows the same
   font names; a build that does not embed a font still accepts the name
   and asks for the bytes, so a source valid for the CLI is valid in the
   viewer, and the same font bytes give the same pixels.

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
| `GanttChart` | Drag / wheel pans and zooms the time axis only; a legend click hides a section's rows. | The task: name, start, end, section. |
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
view they are applied with `ImPlotCond_Always` only while fresh contexts
settle (the first three frames, exactly as the PNG path: ImPlot treats an
`Always` axis as locked, and an `Equal` plot such as a pie chart settles
differently when it is not), then with `ImPlotCond_Once`. `resetView()`
recreates the contexts, so the source limits apply again. Off a live view nothing changes. No YAML field
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
- `scale` (added by [spec/05_browser_extension](../05_browser_extension/DESIGN.md)
  section 5.3): the layout runs at the source size, as `source`, and the
  CSS scale is the box width over the layout width, above 1 too, so a
  chart replacing an image shown larger than its pixels looks the same.

In `width` and `fill`, a box smaller than the layout needs (an overflow,
core DESIGN section 4, layout engine) is handled by laying out at the
source size and shrinking with CSS, as `source` does. `image.scale` is
ignored: the framebuffer scale is the device pixel ratio, capped at 3 and
by the 8192 px limit of `render_frames()`.

### Theme

`theme` is `source` (default: `image.theme`), any chart theme id
(`light`, `dark`, `ocean`, ...; spec/01_core_features/DESIGN.md section 3,
"Themes"), or `auto` (light or dark from the page's `prefers-color-scheme`,
followed live). It overrides `image.theme` only (the source is bound again
with it); values written in the source stay.

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

### Fonts on demand

Sources name fonts by file name (`font: FiraSans-Regular.ttf`, core
DESIGN section 3, "Text style"). Every build carries a **font catalog**:
the name, size and SHA-256 of every font in `resources/fonts/`, and the bytes of
the ones it embeds. The native build embeds them all, as today; the web
build embeds none.

1. Binding accepts any catalog name, embedded or not (the error for an
   unknown name and its suggestions are unchanged).
2. Before drawing, the live view lists the fonts the figure uses
   (`image.font`, every element's `font`, and, for diagrams, the
   `-Italic` sibling ImUml uses for abstract names) and checks they are
   available. If some are not, `set_source()` throws a
   `fonts_needed` error naming them and keeps the previous chart.
3. The JavaScript module catches it, fetches each missing font from
   `<fontBaseUrl>/<name>` (default: a `fonts/` folder next to
   `nymph-viewer.js`), registers the bytes, and retries. Fetches are
   shared by every viewer on the page and made once per font.
4. Registration checks the bytes against the catalog's size and SHA-256,
   so a wrong or corrupt file is rejected, and only catalog names are
   ever fetched: a source cannot make the viewer request an arbitrary
   URL.
5. A font that cannot be fetched fails that `setSource()` with a
   `NymphError` ("font 'X' could not be loaded from <url>: 404"); the
   previous chart stays.

Registered fonts belong to the process (the module instance) and are
added to every live view's ImGui atlas on its next update; ImGui 1.92
bakes glyphs lazily, so an added font costs only its header until used.

The native library and CLI gain the same mechanism: built with
`NYMPH_EMBED_FONTS` limited to a subset, `render_png()` of a source
naming a font that is neither embedded nor registered throws a
`nymph::error` at the `font` field ("font 'X' is not embedded in this
build; register it with register_font()"), and `nymph fonts` marks such
fonts `(not embedded)`.

### PNG input

`setPng(bytes)` calls `extract_source()`. A nymph PNG behaves like its
YAML. A PNG without a nymph chunk is drawn as a plain image (by the
browser, through `createImageBitmap`) with no interaction, and `info()`
says so. A corrupt PNG throws.

### Saved view (added after the first release)

The view a user leaves (zoom, pan, 3D rotation, series hidden from the
legend) is saved in a new top-level `view` key, so a chart saved from the
viewer reopens the same way, in the viewer and as a PNG
([docs/format.md](../../docs/format.md#view) has the format):

```yaml
view:
  root/children/0: {x: [2, 6.5], y: [10, 40], hidden: [memory]}
  root/children/1: {zoom: 1.5, center: [310, 220]}
```

- **Keyed by element path**, the path picks already use. A separate key
  rather than rewriting `x_min` / `x_max`: the author's limits stay (reset
  view still means "as the source describes it"), the YAML is rewritten
  in one place (`replace_top_level()`, as `setData`), and elements with no
  such fields (diagrams, 3D rotation) fit the same scheme.
- **What a kind saves** (`element_class::viewable()`): `Plot` `x`, `y`,
  `y2`, `hidden`; `GanttChart` `x`, `hidden` (sections); `Plot3D` `x`, `y`, `z`, `rotation`,
  `hidden`; diagrams `zoom` (on top of the fit) and `center` (content
  point at the centre, at zoom 1, so it does not depend on the box size).
- **Applied like source limits**: always for PNGs, in a live view while
  fresh contexts settle (`draw_context::apply_view()`), so PNG and viewer
  agree byte for byte. Hidden entries are switched off right after their
  item is plotted (hiding before would make ImPlot skip the item's colour
  assignment); both paths run several frames first.
- **Only what the user changed is saved.** The live view compares each
  element's state before and after the frames that follow an input; such
  elements are "touched" and `source_with_view()` saves their state, the
  source's own entries staying for the others. An element the user never
  touched keeps following its data (new data from an agent is not frozen
  into a view). Values are rounded to six significant digits of the span
  (centres to 0.01, zoom to 1e-4) so the YAML stays readable. Nothing
  changed: the source comes back byte for byte.
- **Lenient on paths, strict on content**: an entry naming an element
  that no longer exists is ignored (the source changed since the view was
  saved); a malformed entry of an existing element is a positioned error
  with suggestions. A `setSource()` whose view differs from the chart's
  starts from fresh contexts so the new view shows.

### Menu

A right click that did not drag opens a DOM menu: Save as PNG (the image
on screen: `render_png()` of `source_with_view()`), Save as YAML
(`toYaml()`), About (version, rasterizer, controls, licences). It opens
on release, not on `contextmenu`, because Chrome fires that on press on
Linux and macOS, and the press may start a right-drag box zoom. ImPlot's
own right-click menus are off in live views (`NoMenus`; they draw
nothing in PNGs). `menu: false` / `menu="false"` leaves the right click
to the host.

### Accessibility

The canvas gets `role="img"` and an `aria-label` from the source's
`description` (core DESIGN section 3), so screen readers read what the
chart shows.

## 4. Key Abstractions

```
 host page ──► nymph-viewer.js ──────────────► nymph-viewer-core.wasm (one per page)
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

### Font catalog (`src/fonts/`)

```cpp
namespace bdg::nymph {
struct font_entry {
  const char* name;            // "FiraSans-Regular.ttf"
  std::size_t size;            // file size
  uint32_t crc32;              // of the file bytes
  const unsigned char* data;   // embedded bytes, or nullptr when not embedded
};
std::span<const font_entry> font_catalog();         // every font in resources/fonts/, sorted

/// Makes a non-embedded catalog font available (process-wide, thread-safe).
/// @throws std::invalid_argument for an unknown name, or bytes whose size
///         or SHA-256 differ from the catalog's.
void register_font(std::string_view name, std::span<const uint8_t> bytes);

/// The bytes of a catalog font if embedded or registered, else empty.
std::span<const uint8_t> font_data(std::string_view name);

/// Every font `f` draws with, sorted; and those of them without bytes.
std::vector<std::string> fonts_used(const figure& f);
std::vector<std::string> fonts_missing(const figure& f);
}
```

`cmake/NymphFonts.cmake` generates the catalog: every font in
`NYMPH_FONTS_DIR` gets an entry; only those selected by
`NYMPH_EMBED_FONTS` get their bytes compiled in, the others are copied
to `<build>/fonts/` (with the licence files) for hosting. Licence texts
stay embedded in every build (a few KB). `embedded_fonts()` and
`find_font()` become views over the catalog; `load_fonts()` adds every
font with bytes. Registered bytes are copied and never freed, so ImGui
can keep pointing at them (`FontDataOwnedByAtlas = false`, as for
embedded data).

### `live_view` (`src/view/live_view.hpp/.cpp`)

The heart of the project, platform-neutral, built in every configuration
and unit-tested natively.

```cpp
namespace bdg::nymph {
struct view_size { float width, height, pixel_ratio; };  // CSS px, DPR
enum class view_fit { source, width, fill };
struct view_options { view_fit fit; bool cpu_textures; bool tooltips; };
struct view_geometry {           // of the last frame
  float layout_width, layout_height;   // logical px (ImGui's DisplaySize)
  float css_scale, framebuffer_scale;  // CSS px and device px per logical px
  int pixel_width, pixel_height; bool fit_fallback;
};

struct pointer_event {           // one event, in CSS px of the view
  enum class kind { move, down, up, wheel, leave } type;
  float x, y; int button; float wheel_x, wheel_y; bool touch;
};

struct view_pick {               // what is under the pointer
  std::string element;           // element path, e.g. "root/children/1/children/0"
  std::string type;              // "PlotLine"
  std::string label;             // series label or diagram id
  int index;                     // point / bar / cell / node index, -1 if none
  std::vector<std::pair<std::string, std::string>> values; // "x" -> "3", "y" -> "15.2"
};

struct plot_view {               // a 2D plot or Gantt chart after the last frame
  std::string element;
  float x, y, width, height;     // plot area, CSS px
  double x_min, x_max, y_min, y_max; bool has_y2; double y2_min, y2_max;
};

class live_view {
 public:
  explicit live_view(view_options options);
  void set_source(std::string_view source, bool reset_view = false); // throws nymph::error, fonts_needed
  void set_png(std::span<const uint8_t> png);   // throws; no nymph chunk -> has_figure() false
  void set_data(std::string_view data_yaml);    // throws nymph::error
  void set_theme(std::string theme);           // "" = the source's image.theme
  void resize(view_size size);
  void input(const pointer_event& event);
  void reset_view();

  /// Runs one frame if one is due; returns its draw data or nullptr.
  const ImDrawData* frame(double now_seconds);
  bool wants_frame() const;     // input, data or settling frames pending
  ImU32 clear_color() const;

  const std::optional<view_pick>& hovered() const;
  std::optional<view_pick> take_click();        // last click, once
  std::vector<plot_view> view_state() const;    // every plot, after the last frame
  view_geometry geometry() const;
  bool has_figure() const;
  const std::string& description() const;
  std::string_view source() const;              // with set_data() applied
  const texture_store& textures() const;        // for the software presenter (below)
};
}
```

Rules:

- **Persistent contexts.** The live view owns one ImGui, ImPlot,
  ImPlot3D and ImUml context for its lifetime (with its own font atlas;
  `imgui_contexts` in `frame_setup`) and makes them current only inside
  its own calls (`context_switch`), restoring the previous ones
  afterwards, like `render_frames()`. Several live views
  can exist on one thread; none may be used from two threads at once.
- **Frames on demand.** A frame is due after input, `set_*`, `resize`,
  `reset_view`, and for a few settling frames after any of them (ImPlot
  fits one frame late; tooltips and hover fades need a frame or two).
  ImGui is told the real time between frames (`DeltaTime`), clamped.
- **Fonts.** `set_source()`, `set_png()` and `set_data()` bind, then
  throw `fonts_needed` (a `std::runtime_error` with the missing names)
  before replacing the figure if `fonts_missing()` is not empty. Fonts
  registered since the last update are added to the atlas first.
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
- `ImPlotCond limits_cond() const` — `Always` unless interactive or while
  a live view's fresh contexts settle (`set_lock_limits()`), else `Once`.
- `record_current_plot(node, with_y2)` / `plot_views()` /
  `previous_plot_view(node)` — each plot's area and limits, read back as
  `view_state()`; the Gantt chart computes its date ticks from the
  previous frame's range so they follow pan and zoom.
- `offer_pick()` / `take_pick_candidate()` / `set_hovered()` (types
  `pick`, `pick_candidate`; helpers in `src/elements/plot_pick`) — a
  per-frame candidate. A series, while
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

(Later: `ImUml::SetNextDiagramView(zoom, center)` sets the view of the
next diagram drawn, with or without the flag, for the saved view; the
info reports `ViewCenter`.)


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
  fit?: "source" | "width" | "fill" | "scale";
  theme?: "source" | "auto" | string; // a chart theme id (docs/themes.md)
  tooltips?: boolean;         // default true
  rasterizer?: "auto" | "webgl" | "software"; // default auto
  menu?: boolean;             // the right-click menu, default true
  fileName?: string;          // for saving; default from src, or "chart"
  wasmUrl?: string;           // default: next to nymph-viewer.js
  fontBaseUrl?: string;       // default: "fonts/" next to nymph-viewer.js
}): Promise<NymphViewer>;

class NymphViewer {
  // Each resolves once the chart is shown (after fetching any fonts it
  // needs) and rejects with NymphError, keeping the previous chart.
  setSource(yaml: string, opts?: { resetView?: boolean }): Promise<void>;
  setPng(bytes: Uint8Array): Promise<void>;
  setData(tables: object): Promise<void>;
  resetView(): void;
  on(event: "hover" | "click" | "view" | "error", cb: (detail) => void): () => void;
  setTheme(theme: string): Promise<void>; // a chart theme id, "" = the source's; fetches its fonts
  themes(): ThemeInfo[];
  rendered(): Promise<void>;  // every update so far applied and drawn, nothing due
  state(): object;            // geometry, hovered, plot views (as the events report them)
  source(): string;           // with setData applied
  info(): { rasterizer: "webgl" | "software"; hasFigure: boolean;
            description: string; size: { width: number; height: number } };
  toYaml(): string;           // the source, with data and the user's view (`view` key)
  toPng(): Uint8Array;        // the image on screen: render_png() of toYaml(), which it embeds
  save(kind: "png" | "yaml"): void; // downloads one of them (the menu's items)
  destroy(): void;
}

class NymphError extends Error { line: number; column: number; }
```

- **Ordering.** Updates are applied in call order: a `setData()` made
  while an earlier `setSource()` waits for a font is applied after it.
  When several `setData()` calls queue behind a font fetch, only the
  last is bound.
- **Events.** `hover` fires when the pick changes (`null` when it ends);
  `click` on a primary-button click with a pick; `view` after a pan or
  zoom settles, with every plot axis range; `error` when a deferred update
  fails. The custom element re-dispatches them as DOM `CustomEvent`s
  (`nymph-hover`, ...).
- **`<nymph-viewer>`** attributes: `src` (URL of a `.yaml` or `.png`,
  fetched by the element, not the wasm), `fit`, `theme`, `tooltips`
  (and `nymph-load` once shown, with `info()`);
  without `src`, the element's text content is the YAML, so a markdown
  renderer can turn a ```` ```nymph ```` fenced block into
  `<nymph-viewer>…</nymph-viewer>`. Its `viewer` property is the
  `NymphViewer`.
- **Input mapping.** Pointer events (mouse, pen, touch) with pointer
  capture during drags; `wheel` with `preventDefault()` only while the
  pointer is over a plot or diagram that takes input
  (`live_view::takes_wheel()`), so page scrolling is not hijacked over
  labels and margins; a two-finger pinch becomes wheel zoom at its centre
  (about 1.2 per notch); a double tap is a double click. The canvas has
  `touch-action: pan-y`: a vertical one-finger swipe scrolls the page, a
  horizontal one pans the chart.
- **Scheduling.** `requestAnimationFrame` only while some viewer
  `wants_frame()`; `ResizeObserver` for size and pixel-ratio changes;
  `IntersectionObserver` pauses viewers off screen.
- **Hosting.** `nymph-viewer.js`, `nymph-viewer-core.wasm` and the `fonts/`
  folder (the font files and their licences), served together; or a
  single-file build (`nymph-viewer.single.js`: Emscripten's
  `SINGLE_FILE` loader with the API appended to the same module by
  `--extern-post-js`; about 10 % larger gzip) for hosts that cannot serve
  a second file, which then needs `fontBaseUrl`
  or only uses the built-in font. A page's
  Content Security Policy must allow `'wasm-unsafe-eval'`.

## 6. Build

- New option `NYMPH_WEB` (default OFF; only meaningful with the
  Emscripten toolchain, an error otherwise). It adds `web/` and turns
  `NYMPH_OPENGL` off (the EGL / WGL loader does not apply; WebGL comes
  from Emscripten's GL library, which is linked, unlike the native rule of
  GL DESIGN section 1, because a browser always provides it).
- New cache variable `NYMPH_EMBED_FONTS`: `all` (default), `none`, or a
  `;`-separated list of font file names to embed. The web build defaults
  to `none`. Fonts not embedded are copied to `<build>/fonts/` with the
  licence texts.
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
- Output in `build/web/`: `nymph-viewer.js` (the API), `nymph-viewer-core.js`
  (Emscripten's loader, which it imports), `nymph-viewer-core.wasm`,
  `nymph-viewer.single.js`, `fonts/`, and `demo/index.html`.

Native window (section 10): `NYMPH_VIEW` (default ON on Windows and Linux;
ON on macOS is a configure error; always OFF with `NYMPH_WEB` and
`NYMPH_RELEASE`) adds the `nymph_window` library and the `view` command to
the executable. It adds no dependency and no file to ship.
`shared_library` is always built now (it was part of the OpenGL
rasterizer).

## 7. Testing Strategy

1. **Native GoogleTest** (every configuration, `ctest`): `live_view`
   static parity with `render_figure()`; synthetic drags and wheels change
   `view_state()` and `reset_view()` restores it; `set_data()` keeps the
   view; picks for every element family at known pixel positions; PNG
   bytes of every example unchanged (the existing tests); ImUml pan and
   zoom; the font catalog, `register_font()` checks, `fonts_used()` and
   `fonts_missing()`, and a `NYMPH_EMBED_FONTS=none` build rendering an
   example once its fonts are registered, with the same bytes as the
   full build.
2. **Node** (when the Emscripten toolchain is configured): the built
   module with the software presenter renders every example within the
   GL DESIGN tolerance of the committed PNGs, fetching their fonts from
   a local folder, reports bind errors with positions, and returns picks
   for synthetic input.
3. **Browser** (Playwright with the preinstalled Chromium, WebGL2 through
   SwiftShader): the WebGL presenter matches the software presenter within
   a browser tolerance (at least 99.5 % of pixels within 16, at most 0.5 %
   beyond 64, mean below 0.5; looser than GL DESIGN's because ImGui's
   stock backend samples glyphs with the GPU's filtering and, through
   ANGLE, breaks pixel-centre ties on the other side of an edge); dragging a plot fires `view`; hovering a point fires `hover`
   with its values and shows the tooltip; `<nymph-viewer src>` loads YAML
   and PNG; a bad source shows the error; twenty viewers on one page use
   one WebGL context and go idle.

4. **Native window** (`NYMPH_VIEW` builds): GoogleTest on what needs no
   window: argument parsing and usage errors, source errors and missing
   files reported before any window opens, the initial window size from the
   layout (and its clamp to the screen), the save file names. The window
   itself is checked by hand (it needs a display and an OpenGL driver).

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
- **Font names stay valid when bytes are absent.** Rejecting a name the
  build does not embed would make a source's validity depend on the
  build. Keeping one catalog in every build, and failing only when bytes
  are missing at draw time, keeps sources portable and the error precise.
- **Fetch in JavaScript, verify in C++.** The module stays free of I/O
  (goal 5); the catalog's size and SHA-256 stop a wrong file from changing
  how a chart looks.
- **A plain native window over the system's OpenGL, all loaded at run
  time** (section 10). Like OpenGL (GL DESIGN section 1), the window
  libraries (`user32`, `gdi32`, `opengl32`; `libX11.so.6`, `libGL.so.1`)
  are never linked: `shared_library` opens them and every function is
  looked up by name (the X11 and GLX types are declared in the source, so
  no X or GL header or development package is needed to build), so the
  executable's imports do not change and users need nothing installed or
  shipped beside it. A windowing library (GLFW was tried first) would mean
  a DLL to build and distribute, or a static link of the OS libraries. The
  price: one small window implementation per platform behind
  `window_host.hpp`: Win32 and X11 exist (Wayland runs through XWayland);
  Cocoa is future work.
- **Alternatives rejected.** SVG output with JavaScript interaction (a
  second renderer that would not match the PNG); a JavaScript chart
  library fed from nymph's YAML (duplicates every element); rendering on
  a server and streaming images (needs a server, not real time).

## 9. Extension Points

- **Reload on change** in `nymph view`: watch the input file and call
  `set_source()` (keeping the view) when it changes.
- **Worker rendering**: run the module in a Web Worker with
  `OffscreenCanvas`, so heavy layout never blocks the host page.
- **Appending data**: `appendData(table, rows)` that extends bound
  columns in place, for feeds of large series.
- **3D picking** through ImPlot3D's projection, **keyboard navigation**
  of data points, and an accessible data-table view.

## 10. Native Window: `nymph view`

(Added after the first release; [PLAN.md](PLAN.md) step 14.)

`nymph view <in> [--theme light|dark|classic] [--fit source|width|fill]`
opens a window on a nymph YAML source or PNG with the viewer of the
previous sections: the very same `live_view`, so pan, zoom, picks,
tooltips, saved-view state and the picture itself are the web viewer's.
Only the host layer is new, in the `nymph_window` library
(`src/window/`), linked into the executable like `nymph mcp`. The viewer
(`viewer_window.cpp`) is platform neutral; it talks to a `native_window`
(`window_host.hpp`: context, swap, size, scale, title, an event pump and the
events it reports) made by `window_win32.cpp` or `window_x11.cpp`:

```
 nymph view ──► view_command (arguments, input, checks) ──► viewer_window
                                                              │
   native_window ─ events ──► pointer_event ──► live_view ◄───┤ frame(now)
   (OpenGL 3 context)                              │ ImDrawData
                       imgui_impl_opengl3 ◄────────┘ (view_options::cpu_textures = false)
```

- **Checks before any window.** The input is read and bound by a throwaway
  `live_view` first, so a bad source reports its line and column on
  standard error (exit 1), as `nymph render` does, and a font the build
  lacks is named; no window flashes up for a source that cannot be drawn.
  The same view gives the layout size, which is the initial window size
  (clamped to the primary monitor's work area; the layout then follows the
  window).
- **Window to view.** The box is the window's size in screen coordinates;
  the pixel ratio is the framebuffer width over it. `fit` defaults to
  `fill` (the web default, `source`, would leave the rest of a resizable
  window empty); a window too small for the layout makes the view lay out
  at the source size (`geometry().fit_fallback`), and the window is then
  enlarged to that size once. Cursor movement, buttons (primary, secondary,
  middle) and scrolling become `pointer_event`s one to one; the cursor
  leaving the window is `leave`.
- **Drawing.** As in the web build: ImGui's `imgui_impl_opengl3` is
  installed through `set_context_hooks()` and draws the frame's
  `ImDrawData` straight to the window's back buffer (no hidden canvas, no
  copy), cleared to `clear_color()`. OpenGL is loaded by that backend at
  run time, as the rest of nymph does; the window, its device context and
  an OpenGL 3.3 core context (as `gl_device_win.cpp` makes one) come from
  user32, gdi32 and opengl32 (Windows: WGL), or libX11 and libGL (Linux:
  GLX, `glXCreateContextAttribsARB`), through function tables filled by
  `shared_library::symbol()`. On Windows per-monitor DPI awareness is
  requested when the system has it and the pixel ratio is the window's DPI
  over 96; the X11 window uses a pixel ratio of 1. The wheel is X11's
  button 4 / 5 (6 / 7 sideways). A machine with no display or no OpenGL
  3.3 context gets an error (exit 1) that names `nymph render`, which
  needs none; there is no CPU path.
- **Idle.** The loop waits for events and draws only while `wants_frame()`
  (goal 4); an exposed or resized window is redrawn from the last frame's
  draw data, also inside the operating system's resize loop.
- **Keys.** `Esc` or `Q` closes; `R` resets the view; `S` writes the
  image on screen (`render_png()` of `source_with_view()`, which embeds the
  view) to `<input>.view.png` and `Y` writes the YAML to `<input>.view.yaml`,
  next to the input and never over it; the window title says what was
  written. The title is `nymph - <file name>`.
- **Icon.** The window shows nymph's icon (`resources/icon.png`, compiled
  in by `src/CMakeLists.txt`, decoded once and scaled by
  `window/window_icon.cpp`), not the system's generic one: X11 gets
  `_NET_WM_ICON` at 16 to 128 px; Windows gets `WM_SETICON` at the
  `SM_CXICON` / `SM_CXSMICON` sizes of the window's DPI, again when the
  DPI changes. On Windows `nymph.exe` also carries the icon as its first
  icon resource (`app/nymph.rc.in`, `resources/icon.ico`), which Explorer
  and shortcuts show; the `.ico` is committed, made from `icon.png` by
  `packaging/windows/make_ico.py` (no image tool at build time).
- **Not in the window**: the right-click menu and About panel (the keys
  replace them), live data feeds (`setData`), PNGs that carry no nymph
  source (an error, as for `nymph render`).
