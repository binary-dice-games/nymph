# nymph — Architecture & Design

**Status: designed, not implemented.** [PLAN.md](PLAN.md) is the ordered
implementation plan.

## 1. Purpose / Scope

`nymph` is a command-line tool that turns a YAML file describing a chart
into a PNG image, and turns a nymph PNG back into the YAML that made it.
The YAML is stored inside the PNG as metadata, so every nymph image is its
own source file. Its main users are AI agents that need to show
information as a chart: they write YAML, run one command, and reference the
PNG from markdown or a reply.

It is the `bdg/dev/nymph` module of wish (`wish/modules/bdg/dev/nymph/`),
extracted into a standalone program:

- **No wish, no bison.** The element schema, the YAML binding and the
  plot drawing that wish provides through its widget registry are
  reimplemented here, small and specific to charts.
- **No GPU, no window, no display.** Frames are drawn by Dear ImGui and
  rasterized on the CPU by nymph's own software rasterizer
  (`src/render/soft_rasterizer`), written for ImGui 1.92 with emilk's
  [imgui_software_renderer](https://github.com/emilk/imgui_software_renderer)
  as the reference design. No SDL, no OpenGL, no platform backend.
- **Only the silent mode.** wish nymph's `render` and `extract` commands
  exist here; its interactive `edit` and `view` modes do not (they need a
  window).

First release scope: the 2D (`Plot` + series) and 3D (`Plot3D` + series)
chart elements of wish nymph, plus the layout and text elements wish nymph
allowed for combining them (`VerticalLayout`, `HorizontalLayout`,
`Spring`, `Label`, `Separator`, and `SeparatorText`), all with wish's
fields. The design is built to grow two ways later (section 9): more
element types and direct drawing commands on an `ImDrawList`.

## 2. Design Goals

1. **Round trip.** `extract(render(text)) == text`, byte for byte.
2. **Usable blind.** An agent runs one command with no display. Every input
   error is one line, `nymph: <file>:<line>:<column>: <message>`, and exit
   code 1. Nothing is written on failure.
3. **Deterministic.** The same YAML and the same build give the same PNG
   bytes: no clock, no input, no threads in the rasterizer. Which pixels a
   triangle covers is decided in integer arithmetic, so it does not vary
   between compilers either.
4. **Compatible with wish nymph.** The `image` options, element types,
   field names, defaults and `$column` references are wish nymph's, so a
   wish nymph chart moves to this tool by changing only its outer
   structure (section 3), and wish nymph images can be read back (section
   6, `extract`).
5. **Strict.** An unknown key, field, type, enum name or column is an
   error, never silently ignored: an agent cannot see a wrong image.
6. **Small and self-contained.** One static executable built from
   source with vendored dependencies (section 8); no system packages.
7. **Extensible by adding files.** A new element type is one new source
   file that registers a class; nothing else in the pipeline names
   element types.

## 3. Source Format

The source is a single UTF-8 YAML document (block or flow style). JSON is
not an accepted input format, even though JSON is mostly valid YAML: the
files nymph writes, documents and accepts are YAML.

```yaml
nymph: 1
description: Monthly revenue against cost, 2025.
image:
  width: 800
  height: 450
root:
  type: Plot
  title: Revenue vs cost
  x_label: month
  y_label: kUSD
  children:
    - type: PlotLine
      label: revenue
      xs: $month
      ys: $revenue
    - type: PlotBars
      label: cost
      xs: $month
      ys: $cost
data: |
  month, revenue, cost
  1, 12, 9
  2, 15, 9.5
```

Top-level keys (any other key is an error):

| Key | Type | Required | Meaning |
|---|---|---|---|
| `nymph` | int | no | Format version. Only `1` exists; anything else is an error. Lets a later format change stay detectable. |
| `description` | string | no | Plain text. Stored as the PNG `Description` chunk (alt text). Never parsed. |
| `image` | mapping | no | Output options, below. |
| `root` | mapping | yes | The element tree to draw. |
| `data` | string or mapping | no | The data the tree refers to, below. |

### `image`

Same keys and defaults as wish nymph:

| Key | Type | Default | Meaning |
|---|---|---|---|
| `width` / `height` | int | `800` / `500` | Image size in pixels, 16–4096. |
| `scale` | int | `1` | Supersampling factor 1–4: the PNG is `width*scale` x `height*scale`, same layout, sharper. |
| `theme` | string | `light` | `light`, `dark` or `classic` (ImGui + ImPlot built-in styles). wish's fourth theme, `wish`, is not ported (section 10). |
| `padding` | int | `8` | Pixels between the image edge and `root`. |

### `root` and elements

An element is a mapping with a `type`, any of that type's fields, and an
optional `children` list of elements. Allowed types are exactly the
registered element classes (section 4). In the first release:

- `Plot`, with children `PlotLine`, `PlotScatter`, `PlotStairs`,
  `PlotStems`, `PlotShaded`, `PlotBars`, `PlotBarsH`, `PlotHistogram`,
  `PlotHistogram2D`, `PlotHeatmap`, `PlotPieChart`, `PlotDigital`,
  `PlotText`, `PlotInfLines`.
- `Plot3D`, with children `Plot3DLine`, `Plot3DScatter`, `Plot3DSurface`,
  `Plot3DTriangle`, `Plot3DQuad`, `Plot3DMesh`, `Plot3DText`.
- `VerticalLayout` and `HorizontalLayout`, with any of the elements in
  this list except series as children; `Spring`, `Label`, `Separator`,
  `SeparatorText`.

`root` is any element except a series. A series must be a direct child of
its kind of plot (section 4, slots). Fields, defaults and enum/flag names
are those of the wish registrations (`wish/src/ui/plot_elements/`,
`wish/src/ui/plot3d_elements/`, `plot_style_fields.hpp`,
`wish/src/ui/ui_elements/{layout,spring,label,separator}.cpp`), including
the style fields `color`, `fill_color`, `line_weight`, `fill_alpha`,
`marker`, `marker_size` and `colormap`. Integer fields with named values
take names or numbers (`flags: NoLegend|NoTitle`).

Two plots and a caption in one image:

```yaml
image: {width: 640, height: 560}
root:
  type: VerticalLayout
  spacing: 6
  children:
    - {type: Label, text: "Temperature and humidity, 12 March", text_color: "#555555"}
    - type: Plot
      title: Temperature
      height: -1
      children:
        - {type: PlotLine, label: temp, xs: $hour, ys: $temp}
    - type: Plot
      title: Humidity
      height: -1
      children:
        - {type: PlotShaded, label: humidity, xs: $hour, ys: $hum}
    - {type: Separator}
    - {type: Label, text: "Source: station 14.", wrap: true}
data: |
  hour, temp, hum
  0, 12.8, 73
  12, 23.2, 47
```

### Layout

Layouts follow wish's size hints, so wish nymph sources keep their look:

- **Hints.** Every element in a layout has a `width` and a `height`
  (layouts and plots have the fields; `Label`, `Separator`,
  `SeparatorText` and `Spring` behave as if both were 0). Along the
  layout's axis — `height` in a `VerticalLayout`, `width` in a
  `HorizontalLayout` — a positive value is fixed pixels, `0` is the
  element's natural size, and a negative value is a stretch weight: the
  space left after fixed sizes, natural sizes and `spacing` is shared by
  the stretch children in proportion to the magnitude. A `Spring` is a
  stretch child with weight `weight` (0 collapses it); two Springs around
  an element centre it.
- **Cross axis.** A positive hint is fixed pixels (clamped to the
  layout); anything else fills the layout's width (`VerticalLayout`) or
  height (`HorizontalLayout`). `HorizontalLayout.align: right` pushes the
  row against the right edge when the children do not fill it.
- **Natural sizes.** `Label`: its text measured with the image font,
  wrapped at the available width when `wrap` is true. `Separator`,
  `SeparatorText`: the height ImGui gives them. A layout: the sum of its
  children's sizes along its axis (stretch children count as 0) plus
  `spacing`, plus `2 * padding` when `card` is true. `Plot` / `Plot3D`
  always carry a hint (defaults: width -1, height 300 / 400), so they never
  need a natural size.
- **Root.** `root` gets the whole image minus `image.padding`. A `Plot` /
  `Plot3D` used as `root` with no `height` fills it, as in wish nymph.
- **Overflow is an error.** If the fixed and natural sizes along an axis
  do not fit, rendering fails with the layout's position and the numbers
  (`needs 620 px of height, has 484`), rather than clipping something an
  agent cannot see.
- **Card.** `card: true` draws the layout's children on a rounded,
  bordered panel in theme colours, inset by `padding`, corner radius
  `rounding` (wish's defaults 12 and 10).
- **Label colours.** `text_color`, else `text_color_light` or
  `text_color_dark` by whether `image.theme` is light or dark, else the
  theme's text colour.

wish layout fields that need a live window (`scroll`) are not part of the
schema.

Scalar typing follows the registered field type, not YAML's guess: a value
assigned to a text field stays text (`title: 2025`); a quoted value
assigned to a number field is an error. Colours are `"#RRGGBB"` or
`"#RRGGBBAA"`; an unquoted `#` starts a YAML comment, so an empty colour
caused by one is reported as an error rather than read as "automatic".

### `data`

Two forms:

- **CSV text** (usually a `|` block scalar): comma-separated, header first,
  RFC 4180 quoting, blank lines skipped, whitespace around unquoted cells
  trimmed, every row as long as the header. Exactly wish nymph's rules.
  Error positions inside the CSV are mapped back to lines of the YAML file.
- **Columns as a mapping**: `data: {month: [1, 2, 3], revenue: [12, 15, 11]}`.
  Each key is a column, each value a list of scalars; all lists the same
  length. Position references (`$2`) follow key order.

Both forms produce the same `table` (section 4); nothing after parsing
knows which form was used.

### Column references

As in wish nymph: a string value starting with `$` names a column by header
(`$revenue`, `"$unit price"`) or by 1-based position (`$2`); `$$` is a
literal `$`. Resolution depends on the field's registered type:

| Field type | Value | Result |
|---|---|---|
| number array / integer array | `$col` | The column, one number per row. |
| number array / integer array | `[$a, $b, $c]` | Columns interleaved by row: the row-major grid `PlotHeatmap.values` and `Plot3DSurface.zs` expect. |
| text | `$col` | The cells joined with `\n` (`PlotPieChart.labels`). |
| anything else | `$col` | Error. |

A list mixing references and numbers is an error. In a column used as
numbers an empty cell is NaN (a gap) and any other non-number is an error
naming its position. Columns nothing refers to are never checked.

## 4. Key Abstractions

```
 source text ──► yaml_doc ──► bind() ──► figure ──► render_figure() ──► image ──► encode_png() ──► PNG bytes
                (libyaml,     │  ▲        (image_options    │                          ▲
                 positions)   │  │         + node tree)     ▼                          │
                              │  registry             ImGui/ImPlot/ImPlot3D frame      source text
                              ▼                        (layout, then draw)
                            table (CSV or columns)     ──► ImDrawData ──► soft_rasterizer
```

### `yaml_doc` (`src/document/yaml_doc`)

The source parsed with libyaml's document API into an immutable tree of
`yaml_node {kind (scalar/sequence/mapping), text, quoted, line, column,
entries}`. It is the only place libyaml is used. Duplicate mapping keys,
multiple documents and an empty document are errors here (wish nymph let
the last duplicate win; an agent should hear about it).

### `table` (`src/document/table`)

Column names plus cells kept as strings, each with its source position.
Built by `parse_csv()` or `table_from_mapping()`. Number conversion happens
per column, on demand, during binding.

### Element schema (`src/schema/`)

```cpp
enum class field_type { boolean, integer, number, text, color,
                        enumeration, flags, number_array, integer_array };

struct field_desc {
  std::string name;
  field_type type;
  value default_value;            // value = variant<bool, int64_t, double,
                                  //   std::string, vector<double>, vector<int64_t>>
  std::string description;        // shown by `nymph schema`
  const name_table* names;        // enumeration / flags names, or null
  std::optional<range> limits;
};

class element_class {
 public:
  std::string_view name() const;          // "PlotLine"
  std::string_view slot() const;          // what it is: "plot2d.item"
  std::span<const std::string_view> accepts() const;  // child slots, e.g. {"plot2d.item"}
  std::span<const field_desc> fields() const;
  /// Natural size (`figure` slot only): the size of a 0 hint.
  virtual ImVec2 measure(const node& n, layout_context& lc,
                         float avail_width) const;
  /// Size hints (`figure` slot only); default reads `width` / `height`
  /// fields if the class has them, else {0, 0}.
  virtual size_hints hints(const node& n) const;
  /// Arrange the children inside `rect` (containers only).
  virtual void arrange(const node& n, layout_context& lc, ImRect rect) const;
  virtual void draw(const node& n, draw_context& dc) const = 0;
};
```

- **`registry`**: name → `element_class`. Built explicitly by
  `register_builtin_elements(registry&)`, which calls
  `register_plot2d(registry&)`, `register_plot3d(registry&)` and
  `register_layout(registry&)`. A plain
  object passed by reference, not a global singleton, so tests can build
  one with fake classes.
- **Slots** replace wish's hard-coded placement rules. `Plot` has slot
  `figure` and accepts `plot2d.item`; every 2D series has slot
  `plot2d.item` and accepts nothing; likewise `Plot3D` / `plot3d.item`.
  `VerticalLayout` and `HorizontalLayout` have slot `figure` and accept
  `figure`; `Spring`, `Label`, `Separator` and `SeparatorText` have slot
  `figure` and accept nothing. `root` must have slot `figure`. A child
  whose slot its parent does not accept is an error. A future element,
  such as a canvas accepting `draw.command`, is added without touching
  `bind()`.
- **Field order is fixed** at registration; a `node` stores its values in a
  `std::vector<value>` indexed by field position, so `draw()` reads fields
  through index constants defined next to the class, with no string
  lookup per frame.
- **`draw_context`**: what `draw()` may use — the current ImGui window
  (ImGui, ImPlot and ImPlot3D calls), the node's arranged rectangle
  (`rect(node)`, `figure` slot only), `draw_children(node)` to draw the
  children in order, and the image scale. Plots open their scope
  (`BeginPlot` at the rectangle, with its size), call `draw_children`,
  then close it (`EndPlot`). Layouts only draw their card, if any, and
  their children.

### Layout engine (`src/layout/`)

Runs inside each frame, after `NewFrame()` and before any drawing, because
measuring text needs the frame's font. `layout_context` holds a rectangle
per `figure` node.

1. **Measure**, bottom-up: each `figure` node's natural size through
   `measure()`; containers combine their children's hints and natural
   sizes as in section 3, "Layout".
2. **Arrange**, top-down: `root` gets the image rectangle minus
   `image.padding`; each container's `arrange()` divides its rectangle
   among its children and stores theirs. The axis arithmetic (fixed,
   natural, stretch weights, spacing, overflow check) is one shared
   function used by both layout directions.
3. **Draw**: `root`'s `draw()`, each node at its stored rectangle
   (`SetCursorScreenPos` + the element's own size, clipped to the
   rectangle).

Sizes are known before anything is drawn, so unlike wish (where an auto row
learns its height from the previous frame) the first frame is already
correctly laid out.

### `figure` (`src/document/bind`)

`bind(source, registry) -> figure` where `figure = {image_options,
description, node root}` and `node = {const element_class*, values,
children, position}`. It reads the top-level keys, parses `data`, walks
the element tree, checks every type, slot, field, type conversion, name,
range and colour, and replaces every column reference with literal data.
Throws `nymph::error{line, column, message}` (1-based positions in the
whole file). It is pure: no file access, no ImGui. It is the only
validator of content; `render_figure()` trusts its result.

### `render_figure()` (`src/render/figure_renderer`)

`render_figure(const figure&) -> image{width, height, rgba}`. For one call
it owns a private ImGui context (with its own font atlas), an ImPlot
context and an ImPlot3D context, and destroys them on return, including on
exception. Per call:

- `io.DisplaySize = {width, height}`, `io.DisplayFramebufferScale =
  {scale, scale}`, `ImGuiBackendFlags_RendererHasTextures` set,
  `IniFilename` / `LogFilename` null, fixed `DeltaTime`, mouse at
  `-FLT_MAX`. No platform or renderer backend is installed; nymph plays
  both roles.
- Font: `AddFontDefaultVector()` at 16 px (the embedded scalable font;
  wish uses ImGui's default at 16 px too). ImGui bakes glyphs at the
  framebuffer scale, so text stays sharp at `scale: 2`.
- Style: the `image.theme` ImGui and ImPlot styles, with
  `AntiAliasedLinesUseTex = false` (see section 7).
- One window covering the display (no decoration, no saved settings, no
  padding) in which the layout engine runs and `root` is drawn.
- Three frames are run and the third is kept: ImPlot fits axes one frame
  late, and text sizes settle after the first frame. After each frame the
  `ImTextureData` requests are applied to a `texture_store`; after the
  third, its `ImDrawData` goes to `soft_rasterizer`.

ImGui's current-context pointer is a process global, so `render_figure()`
must not run on two threads at once. The CLI renders one figure per
process.

### `soft_rasterizer` (`src/render/soft_rasterizer`)

Turns `ImDrawData` into RGBA8 pixels on the CPU. It replaces the GPU or SDL
backend wish uses. Written for nymph against ImGui 1.92, with emilk's
[imgui_software_renderer](https://github.com/emilk/imgui_software_renderer)
(`src/imgui_sw.cpp`, commit `b5ae63a`, public domain) as the reference
for its structure and fast paths. That project does not compile against
ImGui 1.92, so it is read, not built; the file header credits it.

- **Input**: `ImDrawData` plus a `texture_store` (texture id → pixels,
  Alpha8 or RGBA32). `texture_store::apply(ImTextureData&)` handles the
  `WantCreate` / `WantUpdates` / `WantDestroy` requests that ImGui ≥ 1.92
  issues when the backend sets `ImGuiBackendFlags_RendererHasTextures`;
  that flag is what lets ImGui bake glyphs at the output density, so text
  is sharp at `scale: 2`. The font atlas is requested as Alpha8.
- **Geometry**: for every draw command, its triangles read through
  `IdxOffset` / `VtxOffset` (the backend sets
  `ImGuiBackendFlags_RendererHasVtxOffset`, so meshes over 64K vertices
  work with 16-bit indices); positions mapped with `(pos - DisplayPos) *
  FramebufferScale`; clipped to the command's clip rectangle (mapped the
  same way, rounded to whole pixels) and the target.
  `ImDrawCallback_ResetRenderState` is ignored; any other callback is an
  error (neither ImPlot nor ImPlot3D emits one).
- **Coverage**: edge functions in 64-bit fixed point (8 sub-pixel bits),
  sampled at pixel centres, with the top-left fill rule, so two triangles
  sharing an edge never both cover a pixel. This matters more than it
  looks: ImGui's anti-aliasing fringes are thin triangles that share every
  edge, and a double-covered edge shows as a darker seam.
- **Attributes**: colour and UV interpolated with barycentric weights
  (affine; ImGui geometry is 2D). Anti-aliasing comes from ImGui's own
  fringe geometry (vertex alpha fading to 0), so no multisampling is
  needed; `image.scale` is the supersampling knob.
- **Texture sampling**: bilinear, clamp to edge, texel centres at
  `(i + 0.5) / size`. Alpha8 texels are white with that alpha, as ImGui's
  GPU backends treat them. Solid fills sample the atlas's white pixel
  (`ImFontAtlas::TexUvWhitePixel`).
- **Blending**: ImGui's `SRC_ALPHA, ONE_MINUS_SRC_ALPHA` on colour and
  `ONE, ONE_MINUS_SRC_ALPHA` on alpha, in 8-bit integer arithmetic with
  one fixed rounding rule. The caller clears the target to the theme's
  window background, opaque, so the PNG is opaque.
- **Fast paths** (taken from imgui_sw's design; each must produce exactly
  the bytes of the general path, which a test checks):
  1. Axis-aligned rectangles: two triangles forming an axis-aligned quad
     with one colour and the white-pixel UV are filled row by row, no
     edge functions.
  2. Text quads: an axis-aligned textured quad of one colour whose UV
     size matches its pixel size is a glyph drawn 1:1; texels are read
     directly instead of interpolated and bilinear-sampled.
  3. Uniform-colour triangles skip colour interpolation.
  4. A one-entry cache of the last (destination, source) blend, since long
     runs blend the same colour over the same background.

What is deliberately not taken from imgui_sw: float edge tests (coverage
would depend on the compiler), a single global font texture, a
`round(uv * (size - 1))` texel lookup (off by up to half a texel), the
white pixel assumed at texel (0, 0) (no longer true in 1.92 atlases),
ignoring `VtxOffset`, calling draw callbacks, and its global statistics
and ImGui option windows.

### `png` (`src/png/`)

`encode_png(image, source, description) -> bytes` and
`read_source(bytes) -> optional<string>`, as in wish nymph: pixels encoded
by `stb_image_write`; the source in an `iTXt` chunk with keyword `nymph`
directly after `IHDR`, uncompressed up to 64 KiB (readable with `strings`)
and zlib-compressed above; the description in a second `iTXt` chunk with
the standard keyword `Description`. CRC-32 is nymph's own (a table-driven
function); zlib compression and decompression come from stb
(`stbi_zlib_compress`, `stbi_zlib_decode_malloc_guesssize`).
`read_source` checks the signature and every chunk CRC and never decodes
pixels. Keeping wish nymph's chunk layout means each tool can read the
other's images.

### Library API (`src/nymph.hpp`)

```cpp
namespace bdg::nymph {
struct error : std::runtime_error { int line; int column; };
/// Validate and render a source; throws error.
std::vector<uint8_t> render_png(std::string_view source);
/// Validate only; throws error.
void check_source(std::string_view source);
/// The source a nymph PNG carries (converted to YAML if it is a wish nymph
/// three-part source); nullopt if bytes is not a nymph PNG.
std::optional<std::string> extract_source(std::span<const uint8_t> png);
}
```

The command line (`src/cli/`) is a function
`int run_cli(std::span<const std::string_view> args, std::istream& in,
std::ostream& out, std::ostream& err)` over this API, so it is tested
in-process; `app/main.cpp` only forwards `argv` and the standard streams
(in binary mode on Windows).

## 5. Data Flow

**YAML → PNG** (`nymph chart.yaml`):

1. Read the file (or stdin); reject above 16 MiB.
2. `bind()` → `figure` (errors stop here, before any ImGui work).
3. `render_figure()` → RGBA image.
4. `encode_png(image, source text verbatim, description)`.
5. Write `<out>.tmp`, rename over `<out>`. On any failure nothing is
   written and an existing output file is untouched.

**PNG → YAML** (`nymph chart.png`):

1. Read the file; `read_source()`. No `nymph` chunk → "not a nymph image".
2. If the text is a wish nymph three-part source (section 6), convert it.
3. Write it to stdout, or to `-o` through the same temp-and-rename.

## 6. Public Contract: the command line

| Command | Behavior |
|---|---|
| `nymph <in> [-o <out>]` | If `<in>` starts with the PNG signature: extract its YAML to `<out>` or stdout. Otherwise: render it to `<out>`, default `<in>` with its extension replaced by `.png`. |
| `nymph render <in> [-o <out.png>]` | Render; `<in>` may be `-` (stdin, then `-o` is required) or a nymph PNG (re-render its embedded source). |
| `nymph extract <in.png> [-o <out.yaml>]` | Extract the embedded YAML. |
| `nymph check <in>` | Validate only: bind and lay out, no rasterizing. Exit 0 and no output when valid. |
| `nymph schema [<Type>] [--markdown]` | Print the element reference from the registry: every type, its slot, fields, types, defaults, names and descriptions. `--markdown` prints the form kept in `docs/elements.md`. |
| `nymph --version`, `nymph --help` | Version, usage. |

Exit code 0 on success, 1 on failure with one line on stderr:
`nymph: <file>:<line>:<column>: <message>` for a bad source,
`nymph: <file>: <message>` otherwise; 2 with the usage text for bad
arguments.

**wish nymph images.** wish stores a three-part text (description, `--`,
format YAML, `--`, CSV) in the same chunk. `extract` recognises it (two
separator lines `--`, and the first part is not a YAML mapping) and
converts it mechanically: `nymph: 1`, `description: |` with the first
part, the format part verbatim, `data: |` with the CSV indented. The
result renders with this tool when it uses only the elements nymph has.

## 7. Design Decisions

- **nymph's own rasterizer, designed after imgui_software_renderer.**
  Dear ImGui ships no software renderer: its backends are GPU APIs
  (OpenGL, Vulkan, DirectX, Metal, WebGPU, SDL renderers) plus
  `imgui_impl_null`, which draws nothing. wish nymph used SDL's software
  renderer through `imgui_impl_sdlrenderer3`; that brings in SDL, which
  this tool must not depend on. emilk's imgui_sw is the obvious base but
  was written for ImGui 1.6x and does not compile against 1.92 (the
  texture API changed), and its gaps (single font texture, no
  `VtxOffset`, texel and white-pixel assumptions) touch most of its code.
  Writing the rasterizer fresh for 1.92, with imgui_sw as the reference for
  structure and fast paths, costs about as much as patching it and leaves
  code that follows nymph's conventions. Rejected: SDL's software renderer
  (dependency), patching an imgui_sw copy (most of it would change), and
  other third-party rasterizers (unmaintained, pre-1.92).
- **Fixed-point coverage, integer blending.** Goal 3 asks for repeatable
  bytes; float edge tests can differ across compilers and FMA settings,
  and a wrong edge decision shows as a seam between fringe triangles.
  Colour and UV interpolation stay in float (they do not decide coverage),
  quantized once with an explicit rounding rule.
- **Layout is computed before drawing, not by ImGui's flow layout.** The
  image is static and every size is known, so a measure/arrange pass in
  the same frame gives exact rectangles; wish's flow layout learns auto
  sizes one frame late and needs child windows. The hint convention is
  wish's (positive fixed, 0 natural, negative stretch weight), so wish
  nymph sources lay out the same way.
- **Overflow is an error, not clipping.** An agent cannot see that a plot
  was squeezed to nothing or a label cut off; a positioned error with the
  needed and available pixels tells it what to change.
- **Geometry anti-aliasing, not textured lines**
  (`AntiAliasedLinesUseTex = false`). Learned in wish nymph: textured lines
  depend on GPU-exact bilinear filtering and came out uneven on a
  software renderer. Geometry fringes need no texture at all.
- **The whole source is one YAML document**, not wish nymph's three parts
  separated by `--` lines. A YAML file is what the tool is asked to read;
  any YAML tool or library can read, generate or edit it; and there is one
  parser and one position scheme instead of three. The CSV keeps its
  compact form inside a block scalar, and the columns-as-mapping form lets
  a program emit data without writing CSV. Cost: data lines are indented
  under `data: |`, and wish nymph texts need the conversion in section 6.
- **A small typed schema of nymph's own, not a port of bison `dynamic`.**
  nymph needs field names, types, defaults, names for flags and
  descriptions, and that is all; a fixed `std::vector<value>` per node
  indexed by field position is enough and keeps `draw()` free of map
  lookups. The schema data (names, defaults, descriptions, flag tables)
  is copied from wish's registrations so that Goal 4 holds.
- **Slots, not a fixed allow list.** wish nymph had an allow list because
  wish's registry contains widgets that read files. Here the registry
  contains only what nymph registers, so the registry is the allow list;
  slots carry the placement rule that wish expressed in code.
- **Explicit registration functions, not static self-registration.**
  Self-registering objects in a static library are dropped by the linker
  when nothing references their translation unit, and their order is
  unspecified; `register_builtin_elements()` lists the families in one
  place.
- **Strict YAML handling.** Unknown keys, duplicate keys and unknown flag
  names are errors (Goal 5). wish's importer ignores the first two; wish
  nymph already made unknown fields errors for the same reason.
- **libyaml, not a C++ YAML library.** It is what wish nymph uses (same
  scalar and position behaviour), it is small C with a CMake build, and
  its document API gives every node's line and column. yaml-cpp and
  rapidyaml are larger for no gain here.
- **`extract` writes to stdout by default.** An agent usually wants to read
  the YAML, not create a file next to the image; `-o` writes a file.
  Writing `<in>.yaml` by default could also overwrite a hand-kept source.

## 8. Constraints, Invariants, Dependencies

- `read_source(encode_png(img, t, d)) == t`; line endings, comments and
  whitespace are preserved, never normalised.
- `bind()` is pure and is the only validator of the source's content; the
  layout overflow check (it needs font metrics) is the one error found
  while rendering, and `check` runs the layout pass to find it too.
- The rendered frame depends only on the `figure`. No wall clock, no
  environment variable, no locale (numbers are parsed and printed with
  `std::from_chars` / `std::to_chars`).
- Limits, checked before the allocation they bound: source 16 MiB; output
  `width*scale` and `height*scale` at most 8192; data at most 1,000,000
  rows. Exceeding one is an ordinary `nymph::error`.
- The source never names a file: nymph reads only the input path and writes
  only the output path given on the command line.
- Metadata survives only byte-exact copies; services that re-encode images
  drop it. The README says so.

Dependencies (git submodules in `extern/`, built from source, initialised
lazily by `nymph_ensure_submodules()`):

| Submodule | Why | Pin |
|---|---|---|
| `imgui` | Frame generation, fonts | `v1.92.9b` (the version wish uses, non-docking branch) |
| `implot` | 2D plots | wish's `extern/implot` commit |
| `implot3d` | 3D plots | wish's `extern/implot3d` commit |
| `libyaml` | YAML parsing with positions | `0.2.5` |
| `stb` | `stb_image_write` (PNG), zlib for compressed `iTXt` | wish's `extern/stb` commit |
| `googletest` | Tests only | `v1.15.2` |

ImPlot and ImPlot3D are ImGui extensions, and libyaml and stb are small
vendored C libraries; none of them is a system or GPU dependency. Pinning
ImGui, ImPlot and ImPlot3D to wish's versions keeps the drawing code
portable between the two projects.

## 9. Extension Points

- **New element types** (tables, images, a grid layout, more series):
  one source file under `src/elements/<family>/` that defines
  `element_class` subclasses and a `register_<family>()` called from
  `register_builtin_elements()`. Choosing slots and, for `figure`
  elements, `measure()` / `hints()` are the only integration steps.
- **Direct drawing commands.** A `Canvas` element (slot `figure`, accepts
  `draw.command`) reserves a rectangle and exposes it through
  `draw_context` as a coordinate transform plus `ImGui::GetWindowDrawList()`.
  Draw commands (`Line`, `Rect`, `Circle`, `Polyline`, `Triangle`, `Text`,
  ...) are elements of slot `draw.command` whose `draw()` calls
  `ImDrawList` directly with transformed coordinates. The same commands
  can later be accepted inside `Plot` (slot list `plot2d.item`,
  `draw.command`) with plot coordinates mapped through
  `ImPlot::PlotToPixels` and drawn on `ImPlot::GetPlotDrawList()`; the
  transform lives in `draw_context`, so the command classes do not change.
- **More themes or fonts**: `image.theme` and a future `image.font` map to
  functions applied in `render_figure()`; the rasterizer is unaffected.

## 10. Not Ported from wish nymph

| wish nymph | Here | Why |
|---|---|---|
| `edit`, `view` modes | Not present | They need a window. |
| Three-part `.nymph` text | YAML document; converted on `extract` | Section 7. |
| `wish` theme | Not present | It is wish's own palette; can be added as data later. |
| Layout `scroll`, `Splitter`, other wish widgets | Not present | They need a live window or are not chart elements. |
| Schema completion in an editor | `nymph schema` | No editor; agents read the printed schema. |
