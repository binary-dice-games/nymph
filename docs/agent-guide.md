# nymph: making charts from YAML

Instructions for an AI agent. Everything needed to use the tool is in this
file; [elements.md](elements.md) has every field of every element.

## What nymph does

`nymph` turns a small YAML file into a PNG chart. You write the YAML, run
one command, and get an image you can reference from markdown or show to
a user. The YAML is stored inside the PNG, so the image can later be given
back to the tool to change it.

Use it whenever a chart would explain something better than a table or a
sentence: trends over time, comparisons, distributions, proportions.

## The command

If `nymph` is not on `PATH`, use the full path to the binary (in a source
checkout: `build/app/nymph`). It needs no window, display or GPU and
takes well under a second.

| Command | Result |
|---|---|
| `nymph chart.yaml` | Writes `chart.png`. |
| `nymph chart.yaml -o out.png` | Writes `out.png`. |
| `nymph chart.png` | Prints the YAML stored in a nymph PNG (`-o FILE` writes it). |
| `nymph check chart.yaml` | Validates without drawing; prints nothing when valid. |
| `nymph render - -o out.png` | Reads the YAML from standard input. |
| `nymph schema PlotLine` | Lists one element's fields, defaults and allowed values. |
| `nymph fonts` | Lists the font names `font:` accepts. |

**Exit code 0** means success. **Exit code 1** means failure: one line on
standard error says why, and no file is created or changed. **Exit code 2**
means bad arguments. Always check the exit code.

```
nymph: chart.yaml:15:11: unknown column 'revenu' (did you mean 'revenue'?) (columns: month, revenue, cost)
```

The numbers are the line and column in your file. Fix that spot and run
the command again.

## Workflow

1. Write the source file (format below), with the extension `.yaml`.
   If you computed or fetched the data, record how in `source` (the
   prompt, query or code), so the chart can be updated later.
2. Run `nymph chart.yaml`.
3. If the exit code is 1, read the error line, fix the source, run again.
4. Look at the PNG if you can view images, and adjust the source if the
   chart is not clear.
5. Reference the PNG as a normal image: `![Revenue by month](chart.png)`.

To change an existing nymph image when you no longer have its source, run
`nymph chart.png -o chart.yaml`, edit the YAML, and render again. If the
YAML has a `source` key, read it first: it says how the data was made, so
you can rerun that prompt, query or code for fresh data instead of
guessing. Keep `source` up to date when you change the data.

## Source format

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
  3, 11, 10
```

| Key | Content |
|---|---|
| `nymph` | Optional. Always `1`. |
| `description` | Plain text saying what the chart shows. It becomes the image's alt text. |
| `image` | Optional size and look, below. |
| `root` | Required. The element to draw. |
| `data` | The table (or named tables) that `$column` values refer to. Optional if every number is written inline. |
| `source` | Optional. How the data was made: the prompt, query, command or code. Never drawn; kept in the PNG for the next edit. |

Rules that cause most errors:

- YAML indentation is spaces, never tabs.
- Every `type`, field name and flag name must be spelled exactly as in
  [elements.md](elements.md). Unknown names are errors, not ignored.
- Series (`PlotLine`, `PlotBars`, ...) go in the `children` of a `Plot`;
  3D series in the `children` of a `Plot3D`.
- **Colours go in quotes**: `color: "#C0392B"`. Without quotes `#` starts a
  YAML comment.
- Numbers are never quoted: `y_min: 0`, not `y_min: "0"`.
- Quote text that contains `: ` or ` #` (`title: "Cost: 2025"`).
- The CSV goes in a `|` block, never `>`.

### `image`

| Key | Default | Meaning |
|---|---|---|
| `width` | `800` | Width in pixels, 16 to 4096. |
| `height` | `500` | Height in pixels, 16 to 4096. |
| `scale` | `1` | 1 to 4. The PNG has `width*scale` by `height*scale` pixels with the same layout, only sharper. Use `2` for images shown large. |
| `theme` | `light` | `light`, `dark` or `classic`. |
| `padding` | `8` | Empty pixels around the chart. |
| `font` | `default` | Font of all text, by file name; see "Fonts and text style". |
| `font_size` | `16` | Text size in pixels, 1 to 96. |
| `text_color` | theme | Colour of all text, `"#RRGGBB"` in quotes. |

### `root`

`root` is one element: a YAML mapping with a `type`, that type's fields,
and for containers a `children` list. Block style and flow style
(`{type: PlotLine, ys: $v}`) can be mixed.

For one chart, `root` is a `Plot` (2D) or a `Plot3D`. It fills the image.
For several charts, `root` is a `VerticalLayout` or `HorizontalLayout`;
see "Several charts".

### Using the data: `$column`

A value that starts with `$` means "this column of the data".

| Written as | Meaning |
|---|---|
| `ys: $revenue` | The column named `revenue`, one number per row. |
| `ys: "$unit price"` | A name with spaces: quote the whole value. |
| `ys: $2` | The second column, counting from 1. |
| `values: [$a, $b, $c]` | Several columns interleaved row by row: row 1 of a, b, c, then row 2, and so on. This is how a grid is given to `PlotHeatmap` and `Plot3DSurface`. |
| `labels: $name` | On a text field: the column's cells, one per line. Used for pie slice names. |
| `ys: [1, 2.5, 4]` | Numbers written directly, no data needed. |
| `ys: $monthly:revenue` | Column `revenue` of the table named `monthly` (see "Several tables"). |

A list is either all numbers or all `$` references, never a mix. Only
list and text fields take references (`x_min: $a` is an error). To start a
text with a real dollar sign, write `$$` (`title: $$5 per unit`).

### Data

CSV in a `|` block:

- First row is the header. Header names must be unique and not empty.
- Comma-separated. Spaces around values are ignored.
- A value that contains a comma, a quote or a line break goes in double
  quotes; a quote inside it is written twice (`"said ""hi"""`).
- Every row has the same number of values as the header.
- An empty value in a numeric column leaves a gap in the chart.
- Columns you do not reference are not checked, so text columns are fine.

Or columns as lists, handy for generated data:

```yaml
data:
  minute: [0, 1, 2, 3]
  cpu: [12, 35, 71, 88]
```

### Several tables

When the charts need data of different shapes (a monthly series and a
per-region summary), do not pad one table with empty cells: give each
table a name under `data`, in either form, and refer to its columns as
`$table:column`.

```yaml
data:
  monthly: |
    month, revenue
    1, 92
    2, 88
  regions:
    name: [north, south]
    share: [60, 40]
```

- `ys: $monthly:revenue`, `labels: $regions:name`, `ys: $monthly:2`.
- With several tables a bare `$revenue` is an error that tells you which
  tables have that column; with one table it still works.
- Tables can have different numbers of rows. Table names cannot contain
  `:`.
- A `data` mapping holds either columns (lists) or tables (CSV text or
  mappings), never both.

### `source`

Record how the data was made, so you (or another agent) can update the
chart after reading the PNG back. nymph never reads it; any YAML works,
and this shape is the clearest:

```yaml
source:
  prompt: Chart 2025 revenue by month, and each region's share.
  code: |
    df = pd.read_csv("orders_2025.csv", parse_dates=["date"])
    monthly = df.groupby(df.date.dt.month).revenue.sum() / 1000
  language: python
```

Use `prompt` for the request you were answering, `code` (with
`language`) or `query` or `command` for what produced the numbers, and
`notes` for anything else (sources, dates, assumptions). Leave out
secrets such as passwords and API keys: the YAML travels inside the
image.

## Elements

Full reference: [elements.md](elements.md), or `nymph schema <Type>`.

### `Plot` (2D chart container)

| Field | Default | Meaning |
|---|---|---|
| `title` | none | Title above the plot. |
| `x_label`, `y_label` | none | Axis labels. |
| `x_min`, `x_max` | automatic | Fixed X range. Both must be set and different to take effect. |
| `y_min`, `y_max` | automatic | Fixed Y range, same rule. |
| `width` | `-1` | Width in pixels; `-1` fills the available width. |
| `height` | fills the image when the plot is `root`; otherwise `300` | Height in pixels; in a layout a negative value shares the space left. |
| `flags` | none | Any of `NoTitle`, `NoLegend`, `NoFrame`, `Equal` (same scale on both axes), `Crosshairs`, `CanvasOnly`. |
| `x_flags`, `y_flags` | none | Per axis: `NoLabel`, `NoGridLines`, `NoTickMarks`, `NoTickLabels`, `NoDecorations` (all four), `Invert`, `Opposite` (axis on the other side), `AutoFit`. |
| `legend_location` | top left | `Center`, `North`, `South`, `West`, `East`, `NorthWest`, `NorthEast`, `SouthWest`, `SouthEast`. |
| `legend_flags` | none | `Outside` (legend outside the plot area), `Horizontal`, `Sort`, `Reverse`. |
| `colormap` | `Deep` | The colours series take in turn, and the scale of heatmaps. See "Colours and style". |

Several flags are joined with `|`: `flags: NoLegend|NoTitle`.

### 2D series (children of a `Plot`)

Every series has a `label` (its legend name) and the style fields in
"Colours and style".

| Type | Fields besides `label` | Draws |
|---|---|---|
| `PlotLine` | `xs`, `ys` | A connected line. |
| `PlotScatter` | `xs`, `ys` | Markers, not connected. |
| `PlotStairs` | `xs`, `ys` | A step line. |
| `PlotStems` | `xs`, `ys`, `ref` (baseline, default 0) | A stem from the baseline to each point. |
| `PlotShaded` | `xs`, `ys`, and either `ref` (default 0) or `ys2` | A filled area between `ys` and the baseline, or between `ys` and `ys2`. |
| `PlotBars` | `xs` (positions; omit for 0, 1, 2, ...), `ys` (heights), `bar_size` (width, default 0.67) | Vertical bars. |
| `PlotBarsH` | `xs` (lengths), `ys` (positions), `bar_size` | Horizontal bars. |
| `PlotHistogram` | `values`, `bins` (a count; default `-1`, automatic), `cumulative`, `density` (true/false), `range_min`, `range_max` | A histogram of the raw samples in `values`. |
| `PlotHistogram2D` | `xs`, `ys`, `x_bins`, `y_bins` | A 2D frequency map of sample pairs. |
| `PlotHeatmap` | `values` (grid, row by row), `rows`, `cols`, `scale_min`, `scale_max` (default 0 and 1), `format` (cell label, default `"%.1f"`; `""` for none) | A coloured grid. |
| `PlotPieChart` | `labels`, `values`, `x`, `y` (centre, default 0.5), `radius` (default 0.4), `normalize`, `label_fmt` (default `"%.1f%%"`), `angle0` (start angle in degrees, default 90) | A pie chart. |
| `PlotDigital` | `xs`, `ys` (0 or 1) | A digital signal trace. |
| `PlotInfLines` | `values`, `horizontal` (default false) | Lines across the whole plot: vertical at each X value, or horizontal at each Y value. Good for limits and targets. |
| `PlotText` | `text`, `x`, `y`, `offset_x`, `offset_y` (pixels) | A text label at a plot coordinate. |

`xs` and `ys` must have the same length. With `ys` only, the points are
drawn at x = 0, 1, 2, ...

### `Plot3D` and 3D series

`Plot3D` has `title`, `x_label`, `y_label`, `z_label`, `width`, `height`,
`colormap`, `flags` (`NoTitle`, `NoLegend`, `NoClip`, `Equal`,
`CanvasOnly`) and `x_flags`, `y_flags`, `z_flags` (`NoLabel`,
`NoGridLines`, `NoTickMarks`, `NoTickLabels`, `NoDecorations`, `Invert`,
`AutoFit`).

| Type | Fields besides `label` | Draws |
|---|---|---|
| `Plot3DLine` | `xs`, `ys`, `zs` | A connected line through 3D points. |
| `Plot3DScatter` | `xs`, `ys`, `zs` | Markers at 3D points. |
| `Plot3DSurface` | `xs`, `ys`, `zs`, `x_count`, `y_count`, `scale_min`, `scale_max` | A surface over a grid of `x_count` by `y_count` points, listed row by row. All three lists have `x_count * y_count` values. |
| `Plot3DTriangle` | `xs`, `ys`, `zs` | Filled triangles; every 3 points make one. |
| `Plot3DQuad` | `xs`, `ys`, `zs` | Filled quads; every 4 points make one. |
| `Plot3DMesh` | `xs`, `ys`, `zs`, `indices` | A triangle mesh; `indices` lists 3 point numbers (from 0) per triangle. |
| `Plot3DText` | `text`, `x`, `y`, `z`, `angle`, `offset_x`, `offset_y` | A text label at a 3D coordinate. |

### Colours and style

Without any of these, series are coloured automatically in turn from the
plot's `colormap`.

Fields on every series, 2D and 3D:

| Field | Default | Meaning |
|---|---|---|
| `color` | automatic | The series colour, for its lines, fills and markers. |
| `fill_color` | same as `color` | A different colour for filled areas (bar faces, shaded regions). |
| `line_weight` | `1` | Line thickness in pixels. |
| `fill_alpha` | automatic | Opacity of fills, from 0 (transparent) to 1 (solid). |
| `marker` | none on lines, circles on scatters | `None`, `Circle`, `Square`, `Diamond`, `Up`, `Down`, `Left`, `Right`, `Cross`, `Plus`, `Asterisk`. |
| `marker_size` | automatic | Marker radius in pixels. |

**Colours are written `"#RRGGBB"` or `"#RRGGBBAA"`, always in quotes.**
Colour names such as `red` are not accepted.

`PlotHeatmap`, `PlotHistogram2D`, `PlotPieChart`, `PlotText` and
`Plot3DText` ignore these fields. Heatmap cells, pie slices and surfaces
are coloured by the plot's `colormap` instead:

| `colormap` | Kind |
|---|---|
| `Deep` (default), `Dark`, `Pastel`, `Paired` | Distinct colours, for separate series and pie slices. |
| `Viridis`, `Plasma`, `Hot`, `Cool`, `Pink`, `Jet`, `Greys` | A smooth scale from low to high, for heatmaps and surfaces. |
| `RdBu`, `BrBG`, `PiYG`, `Spectral`, `Twilight` | A scale with a distinct middle, for values around a centre such as zero. |

`colormap` is set on the `Plot` or `Plot3D`, not on the series.

### Layout and text elements

| Type | Fields | Use |
|---|---|---|
| `VerticalLayout` | `spacing` (pixels between children), `children`, `card` | Stack elements top to bottom. |
| `HorizontalLayout` | `spacing`, `children`, `card`, `align` | Place elements side by side. |
| `Label` | `text`, `text_color`, `wrap` | A line of text, for example a heading or a source note. |
| `Separator` | none | A horizontal rule. |
| `SeparatorText` | `label` | A rule with a text in it. |
| `Spring` | `weight` | Invisible space that takes what is left; two around an element centre it. |

Sizes inside a layout, along its direction (`height` in a
`VerticalLayout`, `width` in a `HorizontalLayout`):

- a positive value is fixed pixels;
- `0` is the natural size (a label's text, a plot's 400 x 300);
- a negative value shares the space left over: `-1` and `-1` split it
  evenly, `-1` and `-2` split it 1:2.

Across the layout, anything but a positive value fills. A `Plot` keeps
its default `height: 300` unless you change it, so **give plots
`height: -1`** to fill a row or share a column. If fixed sizes do not
fit, nymph reports an error with the numbers instead of cutting
something off.

### Fonts and text style

Fonts are built into nymph; name one by its file name (`nymph fonts`
lists them):

- `default`: ImGui's built-in font (used when nothing is set).
- Fira Sans, a clean sans-serif: `FiraSans-Regular.ttf`,
  `FiraSans-Bold.ttf`, `FiraSans-Italic.ttf`.
- Fira Mono, for numbers and code: `FiraMono-Regular.ttf`,
  `FiraMono-Bold.ttf`.
- PT Serif, for headings and report-style text:
  `PT_Serif-Web-Regular.ttf`, `PT_Serif-Web-Bold.ttf`,
  `PT_Serif-Web-Italic.ttf`.

`font`, `font_size` and `text_color` go on `image` (everything), on
layouts (everything inside), on `Plot` / `Plot3D` (title, ticks, legend),
on `Label` and `SeparatorText`, and on `PlotText` / `Plot3DText`. Empty or
0 inherits from the enclosing element.

```yaml
image: {width: 760, height: 460, font: FiraSans-Regular.ttf, font_size: 15}
root:
  type: VerticalLayout
  spacing: 6
  children:
    - {type: Label, text: Quarterly report 2025, font: PT_Serif-Web-Bold.ttf, font_size: 28}
    - type: Plot
      title: Costs
      height: -1
      font: FiraMono-Regular.ttf
      children:
        - {type: PlotBars, label: costs, ys: [96, 101, 108, 112]}
        - {type: PlotText, text: record, x: 3, y: 116, font: FiraSans-Bold.ttf, text_color: "#C0392B"}
```

Tips: one sans font for the whole image plus a bold or serif heading
reads best. Keep `font_size` between 12 and 18 for chart text; small
images need 12 to 14. File names are exact (case, `.ttf`); a wrong one is
an error that suggests the closest name.

## Recipes

Each is a complete file. Rendered versions of these and more are in
[examples.md](examples.md).

### Line chart, several series

```yaml
description: CPU and memory use during the load test, per minute.
image: {width: 800, height: 420}
root:
  type: Plot
  title: Load test
  x_label: minute
  y_label: "%"
  y_min: 0
  y_max: 100
  children:
    - {type: PlotLine, label: cpu, xs: $minute, ys: $cpu, color: "#C0392B", line_weight: 2, marker: Circle}
    - {type: PlotLine, label: memory, xs: $minute, ys: $mem, color: "#2471A3", line_weight: 2}
data: |
  minute, cpu, mem
  0, 12, 40
  1, 35, 42
  2, 71, 47
  3, 88, 55
  4, 64, 58
  5, 30, 57
```

### Bar chart with a target line

Set `x_min` and `x_max` half a step beyond the first and last bar, or the
outer bars are cut in half.

```yaml
description: Tickets closed per team last sprint, against a target of 30.
image: {width: 640, height: 400}
root:
  type: Plot
  title: Tickets closed
  x_label: team number
  y_label: tickets
  x_min: 0.5
  x_max: 4.5
  y_min: 0
  y_max: 40
  children:
    - {type: PlotBars, label: closed, xs: $team, ys: $closed, bar_size: 0.6}
    - {type: PlotInfLines, label: target, values: [30], horizontal: true, color: "#C0392B", line_weight: 2}
data: |
  team, closed
  1, 31
  2, 24
  3, 37
  4, 18
```

### Histogram

`values` takes the raw samples; nymph does the binning.

```yaml
description: Distribution of response times, in milliseconds.
root:
  type: Plot
  title: Response time
  x_label: ms
  y_label: requests
  children:
    - {type: PlotHistogram, label: latency, values: $ms, bins: 5}
data:
  ms: [118, 131, 96, 143, 110, 125, 88, 138, 121, 114, 102, 127]
```

### Pie chart

`Equal`, a fixed X range from 0 to 1 and no Y range keep the pie round in
any image shape; `NoDecorations` hides the axes. The number on each slice
is the value as given, not a computed share: give percentages (as here)
with the default `label_fmt`, or give counts and set `label_fmt: "%.0f"`.
Put small slices between big ones so their labels do not overlap.

```yaml
description: Share of traffic by browser, in percent.
image: {width: 560, height: 440}
root:
  type: Plot
  title: Browser share
  flags: Equal
  legend_location: East
  legend_flags: Outside
  x_flags: NoDecorations
  y_flags: NoDecorations
  x_min: 0
  x_max: 1
  children:
    - {type: PlotPieChart, labels: $browser, values: $share, normalize: true}
data: |
  browser, share
  Chrome, 63
  Firefox, 6
  Safari, 20
  Other, 11
```

### Heatmap

One CSV column per grid column, one CSV row per grid row. `rows` and
`cols` must match the data, and `scale_min` / `scale_max` should cover its
range. Use a smooth `colormap` such as `Viridis`; `NoLegend` keeps the
legend box off the first cell.

```yaml
description: Activity by weekday (columns) and time of day (rows, morning at the top).
image: {width: 520, height: 360}
root:
  type: Plot
  title: Activity
  colormap: Viridis
  flags: NoLegend
  x_flags: NoDecorations
  y_flags: NoDecorations
  children:
    - type: PlotHeatmap
      rows: 4
      cols: 5
      values: [$mon, $tue, $wed, $thu, $fri]
      scale_min: 0
      scale_max: 10
      format: "%.0f"
data: |
  slot, mon, tue, wed, thu, fri
  morning, 2, 4, 6, 3, 1
  noon, 5, 7, 9, 6, 4
  evening, 8, 9, 10, 7, 6
  night, 1, 2, 3, 2, 5
```

### Several charts

A heading, two stacked plots sharing the height, and a note:

```yaml
description: Temperature and humidity over a day.
image: {width: 640, height: 540}
root:
  type: VerticalLayout
  spacing: 6
  children:
    - {type: Label, text: Greenhouse sensors}
    - type: Plot
      title: Temperature
      height: -1
      y_label: C
      children:
        - {type: PlotLine, label: temp, xs: $hour, ys: $temp}
    - type: Plot
      title: Humidity
      height: -1
      x_label: hour
      y_label: "%"
      children:
        - {type: PlotShaded, label: humidity, xs: $hour, ys: $hum}
    - {type: Label, text: "Source: station 14.", text_color: "#666666"}
data: |
  hour, temp, hum
  0, 12.8, 73
  6, 14.9, 66
  12, 23.2, 47
  18, 21.0, 52
```

Side by side: a `HorizontalLayout` whose plots have `width: -1` (share
the width) and `height: -1` (fill the row's height):

```yaml
description: p95 latency before and after the cache change.
image: {width: 900, height: 380}
root:
  type: HorizontalLayout
  spacing: 8
  children:
    - type: Plot
      title: Before
      width: -1
      height: -1
      y_min: 0
      y_max: 400
      children:
        - {type: PlotLine, label: p95 ms, xs: $hour, ys: $before}
    - type: Plot
      title: After
      width: -1
      height: -1
      y_min: 0
      y_max: 400
      children:
        - {type: PlotLine, label: p95 ms, xs: $hour, ys: $after}
data: |
  hour, before, after
  1, 310, 120
  2, 345, 118
  3, 298, 131
  4, 372, 125
```

### Two tables side by side

A monthly series and a per-region summary have different lengths, so
each gets its own table; `source` says where the numbers came from.

```yaml
description: Monthly revenue in 2025 next to each region's share of the total.
source:
  prompt: Chart 2025 revenue by month, and each region's share of the total.
  query: select month, sum(revenue) from orders group by month
image: {width: 900, height: 420}
root:
  type: HorizontalLayout
  spacing: 8
  children:
    - type: Plot
      title: Revenue per month
      width: -2
      height: -1
      x_label: month
      y_label: kUSD
      x_min: 0.5
      x_max: 4.5
      y_min: 0
      y_max: 120
      flags: NoLegend
      children:
        - {type: PlotBars, label: revenue, xs: $monthly:month, ys: $monthly:revenue}
    - type: Plot
      title: Share by region
      width: -1
      height: -1
      flags: Equal
      legend_location: South
      x_flags: NoDecorations
      y_flags: NoDecorations
      x_min: 0
      x_max: 1
      children:
        - {type: PlotPieChart, labels: $regions:name, values: $regions:share, label_fmt: "%.0f%%"}
data:
  monthly: |
    month, revenue
    1, 92
    2, 88
    3, 104
    4, 111
  regions:
    name: [north, south, east, west]
    share: [34, 21, 28, 17]
```

### 3D surface

The data lists the grid row by row: all X values for the first Y, then
the next Y. Here the grid is 3 by 3.

```yaml
description: Height of the test surface.
image: {width: 640, height: 480}
root:
  type: Plot3D
  title: Surface
  colormap: Viridis
  flags: NoLegend
  children:
    - {type: Plot3DSurface, xs: $x, ys: $y, zs: $z, x_count: 3, y_count: 3}
data: |
  x, y, z
  0, 0, 0.0
  1, 0, 0.5
  2, 0, 0.0
  0, 1, 0.5
  1, 1, 1.0
  2, 1, 0.5
  0, 2, 0.0
  1, 2, 0.5
  2, 2, 0.0
```

## Making a good chart

- Give every chart a `title` and axis labels with units.
- Give every series a `label` when there is more than one.
- Write a real `description`. It is the image's alt text.
- For values that are not numbers on the X axis (names, dates), number the
  rows in a column and plot against that column; say what the numbers mean
  in `x_label` or the description. Axis ticks are always numeric.
- Set `y_min: 0` (with a `y_max`) for bar charts, so bar lengths are honest.
- An automatic range puts the extreme points exactly on the plot edge,
  where they are cut in half. Set `x_min`/`x_max` (and `y_min`/`y_max`) a
  little beyond the data when the end points matter.
- Keep to about five series per plot. Split into several plots beyond that.
- Use `line_weight: 2` for the lines that matter.
- Set colours when they carry meaning (red for a limit, the same colour for
  the same thing across charts). Otherwise leave them automatic.
- Use `scale: 2` when the image will be viewed large.
- Run `nymph check` while iterating on a large source; it is faster than
  rendering.

## Limits

- Pie slices, heatmap cells and surfaces cannot be given individual
  colours; choose a `colormap` for the plot instead.
- Axis ticks are numbers only: no date or category axes.
- The YAML stays in the PNG only while the file is copied unchanged.
  Services that re-encode images remove it. Keep the `.yaml` file when the
  chart may need changing later.
- Maximum source size is 16 MiB and 1,000,000 data rows; images are at
  most 4096 x 4096 before `scale`.

## Common errors

| Message contains | Cause and fix |
|---|---|
| `unknown column 'x'` | `$x` does not match a data header. The message lists the headers and suggests the closest. |
| `needs a table, as there are several` | With named tables write `$table:column`; the message names the tables that have the column. |
| `unknown font 'x'` | Use a name from `nymph fonts` exactly, including `.ttf`; the message suggests the closest. |
| `unknown table 'x'` | The part before `:` is not a table name under `data`. |
| `data mixes columns and named tables` | Under `data`, use either lists (one table of columns) or CSV text / mappings (named tables). |
| `interleaved columns need the same number of rows` | `[$a:x, $b:y]` takes columns from tables of different lengths. |
| `unknown element type 'X'` | `X` is misspelled; the message lists the types allowed there. |
| `'X' cannot be placed in Y; it goes inside: Z` | A series is at the wrong level. Put it in a `Z`'s `children`. |
| `cannot be the root` | `root` is a series. Wrap it in a `Plot` or `Plot3D`. |
| `Plot has no field 'x'` | The field name is misspelled or belongs to another type. |
| `unknown value 'X'` | A flag or name is wrong. The message lists the valid ones. |
| `is not a colour` / `is empty; an unquoted '#' starts a YAML comment` | Write the colour as `"#RRGGBB"`, in quotes. |
| `must be a number, not '3' (remove the quotes)` | A number was quoted. |
| `must be a list of numbers, like [1, 2, 3], or a $column reference` | A list field was given a single number or plain text. |
| `mixes $column references and literal values` | Use only numbers or only references in one list. |
| `cannot take a $column reference` | Only list and text fields take `$column`. |
| `'abc' is not a number` | A referenced column has a non-numeric value at the reported row. |
| `xs has 3 values but ys has 2` | Series lists must be equally long. |
| `is empty: the series has no data` | A series list is empty, or its column has no rows. |
| `rows x cols needs` | `PlotHeatmap.values` does not hold `rows * cols` numbers. |
| `CSV row has N cells, the header has M` | A row is too short or too long, or a value with a comma is not quoted. |
| `'>' joins the CSV lines` | Write `data: |`, not `data: >`. |
| `needs 620 px of height, has 484` | The fixed sizes in a layout do not fit. Use `height: -1` on plots, reduce sizes, or enlarge the image. |
| `tab character in indentation` | Indent with spaces. |
| `mapping values are not allowed`, `could not find expected ':'` | A YAML syntax error at that line: check indentation, colons and quotes. |
| `missing 'root'` | Every source needs a `root`. |
| `not a nymph image` | The PNG was not made by nymph, or lost its embedded YAML. |
