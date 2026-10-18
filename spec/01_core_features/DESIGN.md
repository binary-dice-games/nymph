# nymph — Architecture & Design

**Status: plan steps 1-17 done; builds on MSYS2 and MSVC are not yet
verified.** [PLAN.md](PLAN.md) is the ordered
implementation plan.

## 1. Purpose / Scope

`nymph` is a command-line tool that turns a YAML file describing a chart
into a PNG image, and turns a nymph PNG back into the YAML that made it.
The YAML is stored inside the PNG as metadata, so every nymph image is its
own source file. Its main users are AI agents that need to show
information as a chart: they write YAML, run one command, and reference the
PNG from markdown or a reply.

It started as the `bdg/dev/nymph` module of wish
(`wish/modules/bdg/dev/nymph/`), extracted into a standalone program. It
is not compatible with wish: it neither reads wish nymph's three-part
sources nor promises to keep wish's schema. Compared with wish nymph:

- **No wish, no bison.** The element schema, the YAML binding and the
  plot drawing that wish provides through its widget registry are
  reimplemented here, small and specific to charts.
- **No GPU needed, no window, no display.** Frames are drawn by Dear
  ImGui and rasterized on the CPU by nymph's own software rasterizer
  (`src/render/soft_rasterizer`), written for ImGui 1.92 with emilk's
  [imgui_software_renderer](https://github.com/emilk/imgui_software_renderer)
  as the reference design. No SDL, no platform backend, and no OpenGL
  linked. (A later project,
  [spec/02_opengl_rasterizer](../02_opengl_rasterizer/DESIGN.md), adds an
  optional OpenGL rasterizer loaded at run time, used by the CLI when a
  GPU is available; the software rasterizer stays the default of the
  library and the fallback everywhere.)
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
4. **A familiar schema, not a compatible one.** The `image` options,
   element types, field names, defaults and `$column` references start
   from wish nymph's, which were tried with agents. They are nymph's own
   now: they may change where nymph benefits, and wish nymph sources and
   images are not read.
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
| `data` | string or mapping | no | The data the tree refers to: one table, or several named tables, below. |
| `source` | any | no | How the data was made (the prompt, query, command or code that produced it), for whoever edits the chart next. Kept verbatim, never interpreted. |

### `image`

wish nymph's keys and defaults, plus the text style:

| Key | Type | Default | Meaning |
|---|---|---|---|
| `width` / `height` | int | `800` / `500` | Image size in pixels, 16–4096. |
| `scale` | int | `1` | Supersampling factor 1–4: the PNG is `width*scale` x `height*scale`, same layout, sharper. |
| `theme` | string | `light` | `light`, `dark` or `classic` (ImGui + ImPlot built-in styles). wish's fourth theme, `wish`, is not ported (section 10). |
| `padding` | int | `8` | Pixels between the image edge and `root`. |
| `font` | string | `default` | Font of all text: an embedded font file name (`FiraSans-Regular.ttf`) or `default`, ImGui's built-in font. |
| `font_size` | number | `16` | Text size in logical pixels, 1–96. |
| `text_color` | colour | theme | Colour of all text. |

### Text style

Fonts are compiled into the binary from the `fonts/` folder (section 8)
and named by file name. Every `figure` element that shows or contains
text (`VerticalLayout`, `HorizontalLayout`, `Plot`, `Plot3D`, `Label`,
`SeparatorText`) and the text series `PlotText` / `Plot3DText` take three
fields, `font`, `font_size` and `text_color` (`Label` keeps its own
`text_color` and theme variants and gains the other two). Empty or 0
inherits:

```
image.font / font_size / text_color      (the outermost style)
  └ VerticalLayout font: PT_Serif-Web-Bold.ttf
      ├ Label                            → PT Serif Bold, image size and colour
      └ Plot font_size: 20               → PT Serif Bold at 20 px: title, ticks, legend
          └ PlotText text_color: "#C0392B" → also red
```

A plot's style covers everything ImPlot / ImPlot3D draws for it: title,
axis labels, tick labels, legend and text inside the plot. `text_color`
does not change grid and tick-mark colours, which ImPlot otherwise
derives from the text colour. An unknown font name is an error that lists
the fonts, with a suggestion. `font: default` returns to the built-in
font inside a subtree that set another one.

### `root` and elements

An element is a mapping with a `type`, any of that type's fields, and an
optional `children` list of elements. Allowed types are exactly the
registered element classes (section 4). In the first release:

- `Plot`, with children `PlotLine`, `PlotScatter`, `PlotStairs`,
  `PlotStems`, `PlotShaded`, `PlotBars`, `PlotBarsH`, `PlotHistogram`,
  `PlotHistogram2D`, `PlotHeatmap`, `PlotPieChart`, `PlotDigital`,
  `PlotText`, `PlotInfLines`, and (added after the first release, not
  in wish) `PlotBarGroups`, `PlotErrorBars`, `PlotAnnotation`,
  `PlotTag`.
- `Plot3D`, with children `Plot3DLine`, `Plot3DScatter`, `Plot3DSurface`,
  `Plot3DTriangle`, `Plot3DQuad`, `Plot3DMesh`, `Plot3DText`.
- `VerticalLayout` and `HorizontalLayout`, with any of the elements in
  this list except series as children; `Spring`, `Label`, `Separator`,
  `SeparatorText`; `ColorScale`, `GanttChart` and `SankeyDiagram`
  (added later, see "Axes and annotations", "Gantt charts" and "Sankey
  diagrams" below).

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

Layouts use wish's size hints:

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
  height (`HorizontalLayout`). So a `Plot` in a row keeps its default
  height of 300 unless it sets `height: -1` to fill the row, as in wish.
  `HorizontalLayout.align: right` pushes the row against the right edge
  when it has no stretch child.
- **Natural sizes**, measured with the width the element will get (so a
  wrapped label in a column knows its height). `Label`: its text in the
  image font, wrapped at that width when `wrap` is true. `Separator`: 1 px
  plus the style's item spacing above and below. `SeparatorText`: its
  text plus the style's separator-text padding. A layout: the sum of its
  children along its axis (stretch children count 0) plus `spacing`, the
  largest across, plus `2 * padding` when `card` is true. `Plot` /
  `Plot3D` (with a 0 hint): 400 wide, 300 / 400 high.
- **Root.** `root` is placed in the image minus `image.padding`: a
  positive `width` / `height` is its size (an error if larger than the
  space), anything else fills. So a `Plot` / `Plot3D` used as `root` with
  no `height` fills the image, as in wish nymph, and one with `height: 200`
  is 200 high.
- **Plots draw at their rectangle**: the layout engine has already applied
  their `width` and `height`.
- **Overflow is an error.** If the fixed and natural sizes along an axis
  do not fit, rendering fails with the layout's position and the numbers
  (`needs 620 px of height, has 484`), rather than clipping something an
  agent cannot see.
- **Card.** `card: true` draws the layout's children on a rounded panel
  (the theme's `FrameBg`, bordered with `Border`), inset by `padding`,
  corner radius `rounding` (wish's defaults 12 and 10).
- **Label colours.** `text_color`, else `text_color_light` or
  `text_color_dark` by whether `image.theme` is light or dark, else the
  theme's text colour.

wish layout fields that need a live window (`scroll`) are not part of the
schema.

Differences from wish's 2D elements, all stricter or additive (where
wish drew something silently wrong, nymph reports an error):

- Rules between fields are errors: an empty series, `xs` and `ys` (or
  `ys2`) of different lengths, `PlotHeatmap.values` not `rows x cols`
  long, a `PlotPieChart` without one label per value, an empty
  `PlotText.text`, histogram `bins` of 0. wish drew the shorter length or
  nothing.
- A series with `ys` and no `xs` is drawn against 0, 1, 2, ... (wish drew
  nothing, except for bars, which keep ImPlot's values overload).
- `PlotHeatmap.format` and `PlotPieChart.label_fmt` reach printf inside
  ImPlot, so they must hold at most one `%f`/`%e`/`%g` conversion (plus
  `%%`, width and precision up to 99); anything else is an error. In wish
  an untrusted `%s` or `%n` reached printf.
- `PlotHistogram2D` passes its data range to ImPlot explicitly: ImPlot
  v1.0 computes a missing X range wrongly and draws every bin at count 0
  (one flat colour), in wish too.
- The `bins` description follows ImPlot (-1 square root, -2 Sturges, -3
  Rice, -4 Scott); wish's text had the names in the wrong order. The
  default (-1) is unchanged.
- 3D: `xs`, `ys` and `zs` must be equally long; `Plot3DTriangle` /
  `Plot3DQuad` vertex counts must be multiples of 3 / 4 and
  `Plot3DSurface` arrays exactly `x_count x y_count` (wish truncated or
  drew nothing); `Plot3DMesh.indices` must be a non-zero multiple of 3,
  each within the vertex count. wish passed the indices to ImPlot3D
  unchecked, so an out-of-range index read past the vertex arrays.

**Gantt charts** (added after the first release, not in wish):
`GanttChart` (a `figure` element, in `src/elements/gantt.cpp`) with
`GanttTask` children, directly or in `GanttSection`s.

- Drawn with ImPlot, not ImUml: a schedule is a plot (a time axis, rows,
  a legend) rather than a graph, and ImPlot already gives the axes, the
  theme and the text style. Rows are Y ticks (inverted, task names as
  labels); each section is a legend item made with `ImPlot::BeginItem` /
  `EndItem` (internal API: `PlotDummy` cannot take a colour) whose colour
  its bars take; bars, milestone diamonds and dependency elbows are drawn
  on the plot's draw list.
- Times are ISO dates (`src/core/dates.hpp`: pure arithmetic, UTC, no
  locale) or plain numbers, one kind per chart. A task starts at `start`
  or when the last task in `after` ends (mermaid's `after`), and ends at
  `end` or after `duration` (days for dates). `schedule_of()` resolves
  every task in dependency order and is the one place errors come from
  (missing or doubled start / end, unknown or circular `after`, end
  before start, mixed kinds), so `check()` and drawing cannot disagree.
- Date axes get nymph's own ticks (whole hours, days, Mondays, months or
  years, labelled `2026-03-08`, `2026-03`, ...) chosen to fit the width,
  instead of ImPlot's Time scale, whose ISO day labels read `--03-08`.
- Natural height: 1.8 text lines per row plus title, axis and legend, so
  a chart in a `VerticalLayout` grows with its tasks; as root it fills.
- At most `kMaxGanttTasks` (500) tasks.

**Sankey diagrams** (added after the first release, not in wish):
`SankeyDiagram` (a `figure` element, `src/elements/sankey.cpp`) with
optional `SankeyFlow` and `SankeyNode` children.

- Flows come from the diagram's `from` / `to` / `values` lists first,
  then from `SankeyFlow` children: a table of one row per flow maps on
  with `$column`, the way agents hold such data. Nodes are created by the
  flows that name them, as in mermaid's sankey (unlike the UML diagrams,
  where an undeclared id is an error: requiring declarations would make
  the table form unusable). A `SankeyNode` that no flow names is an
  error, which catches the common misspelling.
- The layout (`src/layout/sankey_layout.cpp`, pure, tested on its own)
  is d3-sankey's, reimplemented: columns by longest path with sinks
  justified to the last column, one value scale fitting the fullest
  column, 32 passes of relaxation towards the value-weighted centre of
  the linked nodes with collisions resolved after each column, then
  links stacked in each node by the other end's position. Every tie
  follows submission order, so it is deterministic.
- Drawn directly on the window's draw list, not through ImPlot (no axes)
  or ImUml (not a graph of boxes): bands are filled polygons of constant
  vertical thickness along d3's link curve (`AddConcavePolyFilled`; a
  thick stroked curve would overlap itself on steep bends and darken
  there), nodes are bars, labels sit beside them towards the middle.
- Errors: list lengths, values not above 0, a flow to itself, a cycle
  (reported at the first flow on it), an unused `SankeyNode`, no flows,
  more than `kMaxSankeyNodes` / `kMaxSankeyFlows`.

**Axes and annotations** (added after the first release; ImPlot v1.0
features wish did not expose):

- `Plot` has, per axis, `_scale` (`Linear`, `Time`, `Log10`, `SymLog`),
  `_format` (a checked printf format), `_ticks` and `_tick_labels`, and
  a second Y axis through the `y2_` fields. Flat fields rather than a
  nested axis mapping keep the schema one level deep like the rest of
  it. A tick label count other than the tick count, a fixed limit at or
  below 0 on a `Log10` axis and a format on a `Time` axis are errors.
- `Time` axes read UNIX seconds and are labelled in UTC, ISO 8601 and a
  24-hour clock (`ImPlotStyle::UseLocalTime` stays false), so an image
  does not depend on the machine's time zone.
- Every 2D series has `y_axis` (`Y1` / `Y2`, its last field). The `Plot`
  enables Y2 (on the right, ImPlot's `AuxDefault`) when a `y2_` field is
  set or a child uses it, since ImPlot asserts on selecting a disabled
  axis.
- `PlotBarGroups.values` is one row per group, so `[$a, $b]` (columns
  interleaved by row) gives it directly from a table of one column per
  series; it is transposed to ImPlot's one row per item.
- `PlotErrorBars`, `PlotAnnotation` and `PlotTag` without a `color` take
  the previous item's colour (`ImPlot::GetLastItemColor()`), since they
  describe the series written before them. ImPlot v1.0 gives a new
  error-bar item the next colormap colour although it draws it in its
  line colour; nymph restores the colormap index and sets the item's
  colour to the drawn one (through `implot_internal.h`), so error bars
  do not shift the colours of later series.
- `ColorScale` is a `figure` element (ImPlot's `ColormapScale`) rather
  than a `Plot` option: it is placed by the layout engine beside any plot,
  2D or 3D. Its default height (300) matches a `Plot`'s, so the two line
  up in a row; as root it fills the image.
- Candlestick (OHLC) charts are not available: ImPlot v1.0 has no such
  item.

Scalar typing follows the registered field type, not YAML's guess: a value
assigned to a text field stays text (`title: 2025`); a quoted value
assigned to a number field is an error. Colours are `"#RRGGBB"` or
`"#RRGGBBAA"`; an unquoted `#` starts a YAML comment, so an empty colour
caused by one is reported as an error rather than read as "automatic".

### `data`

`data` holds one table or several named tables. One table, two forms:

- **CSV text** (usually a `|` block scalar): comma-separated, header first,
  RFC 4180 quoting, blank lines skipped, whitespace around unquoted cells
  trimmed, every row as long as the header. Exactly wish nymph's rules.
  Error positions inside the CSV are mapped back to lines of the YAML file.
- **Columns as a mapping**: `data: {month: [1, 2, 3], revenue: [12, 15, 11]}`.
  Each key is a column, each value a list of scalars; all lists the same
  length. Position references (`$2`) follow key order. An unquoted empty
  value, `~` or `null` is an empty cell (a gap); quoted `'null'` is text.
- A folded block (`data: >`) is an error: folding joins the CSV lines.

Both forms produce the same `table` (section 4); nothing after parsing
knows which form was used.

**Named tables.** When every value of the `data` mapping is CSV text or a
mapping of columns (rather than a list), each key is a table name and
each value one table in either form:

```yaml
data:
  sales: |
    month, revenue
    1, 12
    2, 15
  costs:
    month: [1, 2, 3]
    cost: [9, 9.5, 10]
```

The kind of the values decides: all lists is one table of columns, all
text or mappings is named tables, and a mix is an error that says which
value is the odd one. Table names are non-empty and contain no `:` (the
reference separator); tables may have different row counts and share
column names. A table nested inside a named table is an error (a named
table's columns must be lists).

### `source`

Free-form provenance: the prompt, query, command or code that produced
the data, so an agent that later reads the PNG back can regenerate or
update it. Any YAML value; the convention documented for agents is text,
or a mapping of text such as `{prompt: ..., code: ..., language: python}`.
nymph checks only that it is valid YAML (inside the usual size and depth
limits), keeps it in the embedded YAML like everything else, and never
renders or interprets it. A `$` in it is not a column reference.

### Column references

As in wish nymph: a string value starting with `$` names a column by header
(`$revenue`, `"$unit price"`) or by 1-based position (`$2`); `$$` is a
literal `$`. With named tables, `$table:column` (`$sales:revenue`,
`$sales:2`) names a column of one table:

- If the text before the first `:` is a table name, the rest names a
  column of that table (by header, else position).
- Otherwise the whole text names a column of *the* table: the single
  table, named or not (so a header containing `:` still works with one
  table). With several named tables a bare `$column` is an error that
  lists the tables having that column (`write $sales:revenue`), rather
  than a guess that would change meaning when a table is added.
- A prefix that is not a table, with no such column either, is reported
  as an unknown table, with a suggestion.

Resolution then depends on the field's registered type:

| Field type | Value | Result |
|---|---|---|
| number array / integer array | `$col` | The column, one number per row. |
| number array / integer array | `[$a, $b, $c]` | Columns interleaved by row: the row-major grid `PlotHeatmap.values` and `Plot3DSurface.zs` expect. |
| text | `$col` | The cells joined with `\n` (`PlotPieChart.labels`). |
| anything else | `$col` | Error. |

A list mixing references and numbers is an error. `[$a, $b]` may take
columns from different tables when they have the same number of rows;
otherwise it is an error naming both counts. In a column used as numbers
an empty cell is NaN (a gap) and any other non-number is an error naming
its position. Columns and tables nothing refers to are never checked.

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

`parse_yaml(text) -> yaml_node`, over libyaml's document API, the only
place libyaml is used. A `yaml_node` is a scalar (`text`, `style`: plain,
single- or double-quoted, literal, folded), a sequence (`children`) or a
mapping (`keys` and `children` as parallel vectors, in source order), with
a 1-based `line` and `column` (columns count characters, as libyaml does).
A literal or folded block scalar also records `content_line` /
`content_column`, where its text starts, so a CSV error inside `data: |`
can point at the right line of the file. Aliases are expanded into copies.

Errors (all `nymph::error` with a position): libyaml's syntax errors, with
the start of the enclosing construct added (libyaml notices a problem
after its cause: "found unexpected end of stream (while scanning a quoted
scalar that started at line 1, column 4)"); tab indentation, with its own
message; a duplicate key, at the second one (wish nymph let the last one
win; an agent should hear about it); a non-scalar key; a second document;
an empty document; an alias inside the node it refers to (`a: &x [1,
*x]`, a cycle libyaml accepts), reported at the anchored node; nesting
deeper than 200; more than 4,000,000 nodes
after alias expansion (a few nested aliases would otherwise expand
exponentially).

`src/core/error.hpp` defines `nymph::error : std::runtime_error` with
`line()` and `column()` (0 when the error has no position).

### `table` (`src/document/table`)

Column names plus cells kept as strings (`cell{text, line, column}`),
column-major, each with its position in the YAML file, so a data error
points at its cell. `parse_data(const yaml_node&)` picks the form: a
mapping goes to `table_from_mapping()`, a literal block scalar to
`parse_csv(text, content_line, content_column)`, a single-line quoted or
plain scalar to `parse_csv()` at its own position; a folded (`>`) scalar
and a sequence are errors. `numbers(column)` converts one column on
demand during binding (`std::from_chars`, so no locale; an empty cell is
NaN); `find_column()` resolves a name, else a 1-based position.

`data_set` is what `data` holds: `tables` with their `names` (one entry
with an empty name for the single-table forms, none without `data`).
`parse_data_set(const yaml_node&)` classifies the `data` value (section
3) and calls `parse_data()` once per table, so each table's positions are
its own place in the file. `parse_data()` keeps its single-table meaning.

### Element schema (`src/schema/`)

```cpp
enum class field_type { boolean, integer, number, text, color,
                        enumeration, flags, number_array, integer_array };

// value = variant<bool, int64_t, double, std::string,
//                 std::vector<double>, std::vector<int64_t>>
struct field_desc {
  std::string name;
  field_type type;
  value default_value;
  std::string description;           // shown by `nymph schema`
  const name_table* names;           // enumeration / flags names, or null
  std::optional<value_range> limits; // integer / number
};
// Builders keep registrations short: bool_field, int_field, number_field,
// text_field, color_field, enum_field, flags_field, numbers_field,
// integers_field.

struct node {                        // one bound element
  const element_class* cls;
  std::vector<value> values;         // values[i] is field i
  std::vector<node> children;
  int line, column;
  template <class T> const T& get(size_t i) const;
};

class element_class {
 public:
  element_class(name, slot, accepts, fields, description);
  std::string_view name() const;          // "PlotLine"
  std::string_view slot() const;          // what it is: "plot2d.item"
  std::span<const std::string> accepts() const;  // child slots
  std::span<const field_desc> fields() const;
  int field_index(std::string_view) const;
  node make_node() const;                 // every field at its default
  // Rules between fields, run by bind_source() on every node; errors
  // through fail_at(node, field, message), positioned at the field
  // (node.field_pos) or at the element.
  virtual void check(const node& n) const;
  virtual void adjust_root(node& n, const std::vector<bool>& set) const;
  virtual void draw(const node& n, draw_context& dc) const = 0;
  // Added with the layout engine (plan step 11): measure(), hints(),
  // arrange() for `figure` elements.
};
```

`name_table` holds an enumeration's or flag set's names in order:
`parse_flags("A|B")`, `format_enum()`, `format_flags()` (single bits are
preferred, so tables list them before composites, as wish's do).

`convert_value(yaml_node, field_desc, "Type.field") -> value` applies
section 3's typing rules: booleans are unquoted `true/false`, `yes/no`,
`on/off`; integers are unquoted whole numbers in the 32-bit range; numbers
are unquoted and parsed with `std::from_chars` (no locale; NaN and
infinity are refused); a quoted number or boolean is an error whose
message says "remove the quotes"; text keeps any scalar as written and
`$$` at its start stands for `$`; an enumeration takes a name or one of
its values, never an arbitrary integer; flags take `A|B` or any integer;
an unquoted empty, `~` or `null` item in a number list is NaN (a gap).
Limits are checked for integers and numbers. Every error is positioned at
the value, or at the list item, and names `Type.field`.

- **`registry`**: name → `element_class`; `add()` throws
  `std::logic_error` for a duplicate class or field name. Built explicitly by
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
  (`rect_of(node)`, `figure` slot only; `render_figure()` gives the root
  the content rectangle), `draw_children(node)` to draw the children in
  order, each under its own ImGui ID so same-labelled series or plots do
  not collide, and the image scale. Plots open their scope
  (`BeginPlot` at the rectangle, with its size), call `draw_children`,
  then close it (`EndPlot`). Layouts only draw their card, if any, and
  their children.

### Layout engine (`src/layout/`)

Runs inside each frame, after `NewFrame()` and before any drawing, because
measuring text needs the frame's font. The rectangles are kept in
`draw_context` (`set_rect` / `rect_of`), keyed by node.

1. **Place the root** (`layout_root`): the content rectangle, sized by the
   root's hints (section 3, "Root").
2. **Arrange**, top-down: a container's `arrange()` (the layouts call
   `arrange_linear()`) measures each child that has a 0 hint with
   `measure(node, dc, width)` at the width it will get, shares the axis
   with `distribute()`, rounds boundaries to whole pixels, stores each
   child's rectangle and arranges it in turn. `distribute()` is the one
   pure function holding the axis arithmetic (fixed, natural, stretch
   weights, spacing); the overflow check compares what it needs with
   what is available (0.5 px tolerance).
3. **Draw**: `root`'s `draw()`, each node at its stored rectangle.

Element hooks: `hints()` (default: the `width` / `height` number fields),
`measure()`, `arrange()`. `layout_figure(figure)` runs one frame with only
the layout pass and returns the rectangles; `check_source()` uses it, so
an overflow is found without rendering, and tests use it to check exact
rectangles.

Sizes are known before anything is drawn, so unlike wish (where an auto row
learns its height from the previous frame) the first frame is already
correctly laid out.

### `figure` (`src/document/bind`)

`bind_source(source, registry) -> figure` (called `bind()` in this
document; not named `bind` in code because, with a `std::string`
argument, argument-dependent lookup picks `std::bind`). `figure =
{image_options, description, node root}`. It reads the top-level keys
(`source` is accepted as any value and not looked at),
parses `data` into a `data_set`, walks the element tree, checks every type, slot, field,
type conversion, name, range and colour, and replaces every column
reference with literal data. Throws `nymph::error{line, column, message}`
(1-based positions in the whole file). It is pure: no file access, no
ImGui. It is the only validator of content; `render_figure()` trusts its
result.

Details:

- `children` is a list of elements, or a mapping of name to element
  (wish's named-children form; the names are dropped). An element whose
  class accepts no slot cannot have `children`.
- Errors suggest the closest name within two edits for an unknown type,
  field, top-level key, `image` option or column ("unknown column
  'revenu' (did you mean 'revenue'?) (columns: month, revenue, cost)"),
  and say where a misplaced element goes ("'PlotLine' cannot be placed in
  VerticalLayout; it goes inside: Plot").
- A `$column` into an integer list rounds each value (as wish does); an
  empty cell there is an error.
- `image` options are checked with the same limits as `render_frames()`;
  the "width or height times scale above 8192" error is reported at
  `scale`, the padding error at `padding`.
- After the root element is bound, its class's `adjust_root(node,
  explicitly_set)` hook runs. `Plot` and `Plot3D` use it for wish's rule
  that a root plot without `height` fills the image; `bind_source()`
  itself never names an element type.

### `render_figure()` (`src/render/figure_renderer`)

`render_figure(const figure&) -> image{width, height, rgba}`, built on
`render_frames(const image_options&, draw_fn) -> image`, the frame driver
that knows nothing about elements (`draw_fn` receives a `draw_context`).
For one call it owns a private ImGui context (with its own font atlas), an
ImPlot context and an ImPlot3D context, and destroys them on return,
including on exception; the contexts current before the call are current
again afterwards, so it can run inside another open ImGui frame.
`render_frames` validates `image_options` itself (`std::invalid_argument`)
as a second line behind `bind()`. Per call:

- `io.DisplaySize = {width, height}`, `io.DisplayFramebufferScale =
  {scale, scale}`, `ImGuiBackendFlags_RendererHasTextures` set,
  `IniFilename` / `LogFilename` null, fixed `DeltaTime`, mouse at
  `-FLT_MAX`. No platform or renderer backend is installed; nymph plays
  both roles (`BackendPlatformName` `nymph`, `BackendRendererName`
  `nymph_soft_rasterizer`, `PlatformIO.DrawCallback_ResetRenderState` set
  to the rasterizer's no-op).
- Fonts: `AddFontDefaultVector()` (registered as `default`), then every
  embedded font with `AddFontFromMemoryTTF` (`FontDataOwnedByAtlas =
  false`: the data is static), each registered by file name in the
  `draw_context`. ImGui 1.92's atlas bakes glyphs on first use, per size
  and at the framebuffer scale, so loading every font costs only their
  headers (about 20 ms for eight) and text stays sharp at `scale: 2`.
- Text style: each frame's drawing runs inside the `image` style; then
  `draw_context::draw_node()` / `measure_node()` / `arrange_node()`,
  through which every element is drawn, measured and arranged, wrap the
  call in a `text_style_scope` built from the node's `font`, `font_size`
  and `text_color` fields (`src/render/text_style`). The scope pushes
  `PushFont(font, size)` and the ImGui, ImPlot and ImPlot3D text colours
  (pinning grid and tick colours first) and pops them afterwards, so
  inheritance is ImGui's own push stack and measuring sees the same font
  as drawing.
- Style: the `image.theme` ImGui, ImPlot and ImPlot3D styles
  (`StyleColorsLight` / `Dark` / `Classic`), with
  `AntiAliasedLinesUseTex = false` (see section 7) and window rounding,
  border and padding 0. The image is cleared to the theme's `WindowBg`,
  made opaque.
- One window covering the display (no decoration, no saved settings, no
  padding) in which the layout engine runs and `root` is drawn.
- Three frames are run and the third is kept: ImPlot fits axes one frame
  late, and text sizes settle after the first frame. After each frame the
  `ImTextureData` requests are applied to a `texture_store`; after the
  third, its `ImDrawData` goes to a `raster_backend`: `soft_rasterizer`
  by default, or the OpenGL rasterizer of
  [spec/02_opengl_rasterizer](../02_opengl_rasterizer/DESIGN.md)
  section 3.

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
  Reset-render-state callbacks are ignored (the legacy
  `ImDrawCallback_ResetRenderState` value, and nymph's own
  `reset_render_state_callback`, which the frame driver installs as
  `ImGuiPlatformIO::DrawCallback_ResetRenderState`, the 1.92.8 mechanism);
  any other callback is an error (neither ImGui, ImPlot nor ImPlot3D emits
  one). Vertices are clamped to +-2^19 px so edge functions fit in 64
  bits, and a triangle with a non-finite vertex is skipped.
- **Coverage**: edge functions in 64-bit fixed point (8 sub-pixel bits),
  sampled at pixel centres, with the top-left fill rule, so two triangles
  sharing an edge never both cover a pixel. This matters more than it
  looks: ImGui's anti-aliasing fringes are thin triangles that share every
  edge, and a double-covered edge shows as a darker seam.
- **Attributes**: colour and UV interpolated with barycentric weights
  (affine, in double; ImGui geometry is 2D) and colour rounded to 8 bits.
  An attribute equal at all three vertices is used as is, never
  interpolated: this is a rule of the general path, not an optimisation,
  because it is what makes a solid rectangle give the same bytes through
  either path. Anti-aliasing comes from ImGui's own
  fringe geometry (vertex alpha fading to 0), so no multisampling is
  needed; `image.scale` is the supersampling knob.
- **Texture sampling**: bilinear, clamp to edge, texel centres at
  `(i + 0.5) / size`. The sample position is rounded to 1/256 texel and
  the four texels weighted in integers, so a sample within 1/512 texel of
  a texel centre returns that texel exactly whatever the float error in
  the interpolated UV. Vertex colour and texel are multiplied in integers
  (`(c * t + 127) / 255`). Alpha8 texels are white with that alpha, as
  ImGui's GPU backends treat them. Solid fills sample the atlas's white
  pixel (`ImFontAtlas::TexUvWhitePixel`).
- **Blending**: ImGui's `SRC_ALPHA, ONE_MINUS_SRC_ALPHA` on colour and
  `ONE, ONE_MINUS_SRC_ALPHA` on alpha, in 8-bit integer arithmetic with
  one fixed rounding rule. The caller clears the target to the theme's
  window background, opaque, so the PNG is opaque.
- **Fast paths** (taken from imgui_sw's design; each produces exactly the
  bytes of the general path, which every rasterizer test checks by
  rendering with `raster_options::fast_paths` on and off). Both quad paths
  apply to six indices in ImGui's `PrimRect` order, `(a, b, c), (a, c,
  d)`, forming an axis-aligned rectangle of one colour; they cover the
  pixels whose centres lie in `[x0, x1) x [y0, y1)`, which is exactly what
  the top-left rule gives the two triangles.
  1. Solid rectangles: one UV at all four corners; one texel sample, then
     a row-by-row fill.
  2. Text quads: UV separable (u follows x, v follows y), the left/top
     edge on a pixel boundary, and a 1:1 texel mapping aligned to texel
     boundaries within 1/4096 texel; texels are read directly. Anything
     else (a scaled image, a glyph at a fractional position) falls back to
     the triangle path.
  3. A one-entry cache of the last (destination, source) blend, since long
     runs blend the same colour over the same background.
  The triangle path also stops a row at the first pixel outside the
  triangle once it has been inside (triangles are convex), as imgui_sw
  does.

What is deliberately not taken from imgui_sw: float edge tests (coverage
would depend on the compiler), a single global font texture, a
`round(uv * (size - 1))` texel lookup (off by up to half a texel), the
white pixel assumed at texel (0, 0) (no longer true in 1.92 atlases),
ignoring `VtxOffset`, calling draw callbacks, and its global statistics
and ImGui option windows.

### `png` (`src/png/`)

`encode_png(image, source, description) -> std::vector<uint8_t>` and
`read_source(std::span<const uint8_t>) -> optional<string>`, as in wish
nymph: pixels encoded by `stb_image_write` (deterministic, compression
level 8); the source in an `iTXt` chunk with keyword `nymph` directly after
`IHDR`, uncompressed up to 64 KiB (readable with `strings`) and
zlib-compressed above (`stbi_zlib_compress`); the description in a second
`iTXt` chunk with the standard keyword `Description`, omitted when empty.
CRC-32 is nymph's own table-driven `crc32()`. `read_source` checks the
signature and every chunk CRC and never decodes pixels; it returns
`nullopt` for "not a PNG" and "no `nymph` chunk", and throws for a
truncated file, a bad CRC or an undecompressible chunk, so the CLI can tell
"not a nymph image" from "corrupt PNG". Decompression goes into a fixed
16 MiB buffer (`stbi_zlib_decode_buffer`), so a crafted chunk cannot make
it allocate without limit. stb is compiled into `png_meta.cpp` with
internal linkage (`STB_IMAGE_WRITE_STATIC`, `STB_IMAGE_STATIC`), so a
program linking nymph can have its own stb. Keeping wish nymph's chunk
layout means each tool can read the other's images; PIL and ImageMagick
read nymph's chunks too.

### Library API (`src/nymph.hpp`)

```cpp
namespace bdg::nymph {
struct error : std::runtime_error { int line; int column; };
/// Validate and render a source; throws error.
std::vector<uint8_t> render_png(std::string_view source);
/// Validate only; throws error.
void check_source(std::string_view source);
/// The source a nymph PNG carries, byte for byte; nullopt if bytes is
/// not a nymph PNG.
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
5. Write `<out>.nymph-tmp`, rename over `<out>`. On any failure nothing
   is written and an existing output file is untouched. A symbolic link
   is followed (the file it points to is replaced, not the link); an
   existing non-regular file (`/dev/null`, a pipe) is written directly,
   since a rename would replace the device itself.

**PNG → YAML** (`nymph chart.png`):

1. Read the file; `read_source()`. No `nymph` chunk → "not a nymph image".
2. Write the text verbatim to stdout, or to `-o` through the same
   temp-and-rename.

## 6. Public Contract: the command line

| Command | Behavior |
|---|---|
| `nymph <in> [-o <out>]` | If `<in>` starts with the PNG signature: extract its YAML to `<out>` or stdout. Otherwise: render it to `<out>`, default `<in>` with its extension replaced by `.png`. |
| `nymph render <in> [-o <out.png>]` | Render; `<in>` may be `-` (stdin, then `-o` is required) or a nymph PNG (re-render its embedded source). |
| `nymph extract <in.png> [-o <out.yaml>]` | Extract the embedded YAML. |
| `nymph check <in>` | Validate only: bind and lay out, no rasterizing. Exit 0 and no output when valid. |
| `nymph fonts [--licenses]` | List the names `font:` accepts (`default` and every embedded font), or print the embedded licence texts. |
| `nymph schema [<Type>] [--markdown]` | Print the element reference from the registry: every type, its slot, fields, types, defaults, names and descriptions. `--markdown` prints the form kept in `docs/elements.md`. |
| `nymph --version`, `nymph --help` | Version, usage. |

`-o -` writes to standard output (for pipes). `check` and `render` also
take a nymph PNG and use the YAML it carries. Argument errors are reported
before any file is read. User documentation: [docs/cli.md](../../docs/cli.md).

Exit code 0 on success, 1 on failure with one line on stderr:
`nymph: <file>:<line>:<column>: <message>` for a bad source,
`nymph: <file>: <message>` otherwise; 2 with the usage text for bad
arguments.

**wish nymph images** use the same chunk keyword but hold a three-part
text (description, `--`, format YAML, `--`, CSV). nymph does not convert
them: `extract` prints that text as it is, and `render` / `check` report
it as an invalid source.

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
  under `data: |`.
- **Named tables inside `data`, told apart by value kind.** Charts often
  combine data of different shapes (a time series and a summary), which
  one rectangular table forces into padding with empty cells. Keeping
  them under `data` (not a new `tables` key) leaves one place to look for
  data, and the value kind (list = column, text or mapping = table) is
  unambiguous, so every existing source keeps its meaning. `$table:column`
  reads naturally and needs no quoting in YAML (libyaml takes `$a:b` as a
  plain scalar in block and flow style, since the `:` is not followed by
  a space). A bare `$column` across several tables is an error rather
  than a search: a search would silently change meaning when a later
  table adds a column of the same name.
- **`source` is free-form and unchecked.** Its readers are agents, not
  nymph; any structure nymph imposed would be a convention nymph cannot
  use, and an error in it would block rendering for no visual reason.
- **A small typed schema of nymph's own, not a port of bison `dynamic`.**
  nymph needs field names, types, defaults, names for flags and
  descriptions, and that is all; a fixed `std::vector<value>` per node
  indexed by field position is enough and keeps `draw()` free of map
  lookups. The schema data (names, defaults, descriptions, flag tables)
  started as a copy of wish's registrations (Goal 4).
- **Slots, not a fixed allow list.** wish nymph had an allow list because
  wish's registry contains widgets that read files. Here the registry
  contains only what nymph registers, so the registry is the allow list;
  slots carry the placement rule that wish expressed in code.
- **Explicit registration functions, not static self-registration.**
  Self-registering objects in a static library are dropped by the linker
  when nothing references their translation unit, and their order is
  unspecified; `register_builtin_elements()` lists the families in one
  place.
- **Fonts embedded as generated C++ arrays.** The binary must not depend
  on font files next to it (Goal 6), on every compiler nymph targets.
  C++26 `#embed` is not available everywhere yet; `ld -b binary`, `.incbin`
  and resource files differ per toolchain. A byte array in generated C++
  works with GCC, Clang and MSVC alike (32 bytes per line, because MSVC
  limits line length), and one file per font keeps rebuilds incremental.
  The cost is a few seconds of compile time for 2.8 MB of fonts.
- **Fonts named by file name.** It is what the person adding a font
  already has, it is unique within the folder, and it makes the source say
  exactly which file draws it (weight and style are part of the name, no
  family / weight matching to explain). `nymph fonts` and the schema list
  the names, and an unknown one gets a suggestion.
- **Text style inherited through ImGui's own stacks.** Pushing a node's
  font and colours around its draw, measure and arrange calls, in one
  place (`draw_context`), gives CSS-like inheritance without passing a
  style through every element: ImPlot and ImPlot3D already read the
  current font and colours. The built-in font stays the default, so
  sources without a `font` look exactly as before.
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
- No source text reaches printf unchecked: printf-format fields are
  declared with `double_format_field()`, whose check allows one
  floating-point conversion.
- The source never names a file: nymph reads only the input path and writes
  only the output path given on the command line. A `font` names an
  embedded font, never a file on disk.
- Metadata survives only byte-exact copies; services that re-encode images
  drop it. The README says so.
- No OpenGL, EGL or GLX library is linked; the optional OpenGL rasterizer
  opens them at run time (spec/02_opengl_rasterizer/DESIGN.md section 8),
  and the `nymph_no_gl_linkage` test checks the executable. Goal 3's "same
  bytes" holds for the software rasterizer (`--rasterizer software`, and
  the library's `render_png()`); OpenGL output is repeatable per device,
  not across GPUs.

Dependencies (git submodules in `extern/`, built from source by
`cmake/NymphDependencies.cmake`, initialised lazily by
`nymph_ensure_submodules()`):

| Submodule | Why | Pin |
|---|---|---|
| `imgui` | Frame generation, fonts | `v1.92.9b` (the version wish uses, non-docking branch) |
| `implot` | 2D plots | wish's `extern/implot` commit |
| `implot3d` | 3D plots | wish's `extern/implot3d` commit |
| `libyaml` | YAML parsing with positions | `0.2.5` |
| `stb` | `stb_image_write` (PNG), zlib for compressed `iTXt` | wish's `extern/stb` commit |
| `googletest` | Tests only | `v1.15.2` |

libyaml is compiled from its C files rather than through its own
`CMakeLists.txt` (CMake 3.0 minimum, rejected by CMake 4; CTest and install
rules). ImGui is built with `IMGUI_DISABLE_WIN32_FUNCTIONS` and
`IMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS` (no clipboard, IME or shell calls)
but not `IMGUI_DISABLE_OBSOLETE_FUNCTIONS`, which ImPlot v1.0 still needs.

**Fonts** are not a submodule: `fonts/` holds unmodified TTF files from
the Google Fonts repository (Fira Sans, Fira Mono, PT Serif; SIL Open Font
License 1.1) with their licence texts. `cmake/NymphFonts.cmake`
(`nymph_embed_fonts(nymph)`) globs `NYMPH_FONTS_DIR` (default `fonts/`)
for `*.ttf` / `*.otf` and `*.txt` at configure time (`CONFIGURE_DEPENDS`)
and adds one custom command per file that runs
`cmake/NymphEmbedFile.cmake` in script mode, writing a `.cpp` with the
bytes as an `unsigned char` array (32 per line, a trailing 0 byte) and its
size; a generated table (`embedded_fonts()`, `embedded_licenses()`,
sorted by name) refers to them. A changed font rebuilds only its own
file; an added or removed one re-runs the configure step. The OFL asks for
the licence to travel with the fonts: the texts are embedded too and
printed by `nymph fonts --licenses`.

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
- **More themes**: `image.theme` maps to functions applied in
  `render_figure()`; the rasterizer is unaffected.
- **More fonts**: a file dropped into `fonts/` (with its licence text) is
  embedded on the next build and accepted by every `font` field; nothing
  else changes. Another set can be built in with `-DNYMPH_FONTS_DIR`.
- **Text style on a new element**: append `text_style_fields()` to its
  fields (`with_text_style()`); `draw_context` applies them.

## 10. Not Ported from wish nymph

| wish nymph | Here | Why |
|---|---|---|
| `edit`, `view` modes | Not present | They need a window. |
| Three-part `.nymph` text | YAML document; converted on `extract` | Section 7. |
| `wish` theme | Not present | It is wish's own palette; can be added as data later. |
| Layout `scroll`, `Splitter`, other wish widgets | Not present | They need a live window or are not chart elements. |
| Schema completion in an editor | `nymph schema` | No editor; agents read the printed schema. |
