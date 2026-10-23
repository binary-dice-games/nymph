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

With the nymph MCP tools (`nymph mcp`, the Claude Code plugin or the
Claude Desktop extension: [claude.md](claude.md)), call `render_chart`
with the YAML instead of running the command: it returns the PNG, and
errors read the same. Everything else in this guide applies unchanged.

If `nymph` is not on `PATH`, use the full path to the binary (in a source
checkout: `build/app/nymph`). It needs no window, display or GPU and
takes well under a second. It uses the GPU through OpenGL when there is
one and falls back to the CPU silently; nothing to configure. Add
`--rasterizer software` when two images must compare byte for byte (for
example across machines): OpenGL output can differ slightly between GPUs.

| Command | Result |
|---|---|
| `nymph chart.yaml` | Writes `chart.png`. |
| `nymph chart.yaml -o out.png` | Writes `out.png`. |
| `nymph chart.png` | Prints the YAML stored in a nymph PNG (`-o FILE` writes it). |
| `nymph check chart.yaml` | Validates without drawing; prints nothing when valid. |
| `nymph diagram.mmd` | Draws a mermaid diagram (converted to nymph YAML inside the PNG). |
| `nymph convert diagram.mmd -o diagram.yaml` | Writes the nymph YAML of a mermaid diagram (or of any input). |
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

## Starting from mermaid

Diagrams (flowchart, sequence, class, state, ER, Gantt, pie, mind map,
Sankey, xychart) may be drafted in mermaid instead of YAML: nymph reads
mermaid wherever it reads YAML, detected by its content, converts it and
draws the result. It never writes mermaid.

1. Write the mermaid and draw it: `render_chart` with it as `source` (or
   `nymph diagram.mmd`). Warnings name what nymph dropped (`click`,
   icons, ...); errors give the line and column in the mermaid.
2. Revise the mermaid until the user is satisfied (`editor_set_source`
   takes mermaid for a chart open in the editor).
3. Convert: `convert_chart` (`output_path` ending in `.yaml`) or `nymph
   convert diagram.mmd -o diagram.yaml`. Keep and edit that YAML from now
   on: it declares every node explicitly, and nymph's fields (colours,
   fonts, shapes, layout hints, sizes) refine the look.

A PNG drawn from mermaid carries the converted YAML. Plots, dashboards
and user interfaces have no mermaid form: write them in YAML.
[mermaid.md](mermaid.md) lists what each mermaid kind becomes and what is
dropped.

## The chart open in the editor

When the user has a chart open in the nymph editor (`nymph view --edit
chart.yaml`), `nymph mcp` also offers `editor_*` tools that work on that
chart as the user would: every change is checked like any source, shows at
once and can be undone in the editor.

1. `editor_state`: the file, a `version`, every element by path (with its
   type, label and `id`), the **selection** (its path, written fields and
   the child types it takes), the data tables and the view on screen.
   "This", "the selected node" mean the selection.
2. Change it: `editor_add_element` (`parent`, `type`, `fields`, e.g. a
   `FlowLink` with `{"from": "cart", "to": "purchases"}`),
   `editor_set_fields` (`path`, `fields`; `null` clears one, `"$col"` is a
   column reference), `editor_remove_element` (`with_references` also
   removes the links that name it), `editor_move_element`,
   `editor_set_cells` / `editor_set_table` for data. Pass the
   `expected_version` from `editor_state`: if the user changed the chart
   meanwhile the change is refused, so read the state again.
3. `editor_view_image` shows what the user sees.

A refused change names the problem as the loader does and changes nothing.
With several editors open, `editor` (a file name or process id) chooses
one; without an open editor the tools say so: use `render_chart` instead.

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
| `view` | Written by the web viewer: the zoom, pan and hidden series the user left. Keep it when you edit the chart (it is ignored for elements that no longer exist); remove it to show the chart as you describe it. |

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
| `x_scale`, `y_scale` | `Linear` | `Log10` for values spanning several orders of magnitude (fixed limits must be above 0), `SymLog` for log-like with 0 and negatives, `Time` for UNIX seconds labelled as UTC dates and times. |
| `x_format`, `y_format` | `"%g"` | Tick label format with one `%f`, `%e` or `%g`: `"%.0f ms"`, `"%.1f%%"`. Not on a `Time` axis. |
| `x_ticks`, `y_ticks` | automatic | Tick positions, in data units. |
| `x_tick_labels`, `y_tick_labels` | the values | Text for each tick, such as category names: `[Q1, Q2, Q3]` or a text column `$quarter`. Needs one label per tick. |
| `y2_label`, `y2_min`, `y2_max`, `y2_scale`, `y2_format`, `y2_ticks`, `y2_tick_labels`, `y2_flags` | no second axis | A second Y axis, on the right, for a series in other units (`y_axis: Y2` on the series). Any of these, or such a series, shows it. |

Several flags are joined with `|`: `flags: NoLegend|NoTitle`.

### 2D series (children of a `Plot`)

Every series has a `label` (its legend name) and the style fields in
"Colours and style" that it is drawn with, except the ones that draw
texts or several legend entries (`PlotText`, `PlotAnnotation`, `PlotTag`,
`PlotPieChart`, `PlotBarGroups`), which have no `label`.

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
| `PlotBarGroups` | `labels` (one per series), `values` (one row per group: `[$a, $b]`), `group_size` (default 0.67), `shift`, `horizontal`, `stacked` | Groups of bars side by side (or stacked), at 0, 1, 2, ...; name the groups with `x_ticks` and `x_tick_labels`. |
| `PlotErrorBars` | `xs`, `ys`, and either `err` (symmetric) or `neg` and `pos`, `horizontal`, `cap_size` (pixels, default 4) | Error bars around each point. Write it after the line or bars it belongs to. |
| `PlotAnnotation` | `text` (empty shows the coordinates), `x`, `y`, `offset_x`, `offset_y` (pixels, default 10 and -10), `clamp` | A callout box next to a point, to name an event or an outlier. |
| `PlotTag` | `value`, `axis` (`Y`, default, or `X`), `text` (empty shows the value) | A coloured label on the axis at a value, such as the name of a limit drawn with `PlotInfLines`. |

`xs` and `ys` must have the same length. With `ys` only, the points are
drawn at x = 0, 1, 2, ...

Every 2D series also has `y_axis`: `Y1` (default, left) or `Y2`, the
plot's second Y axis on the right (see the `y2_` fields of `Plot`).

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
| `Plot3DText` | `text`, `x`, `y`, `z`, `angle`, `offset_x`, `offset_y` (no `label`) | A text label at a 3D coordinate. |

### Colours and style

Without any of these, series are coloured automatically in turn from the
plot's `colormap`.

Fields of the series, 2D and 3D, that draw lines, fills or markers:

| Field | Default | Meaning |
|---|---|---|
| `color` | automatic | The series colour, for its lines, fills and markers. |
| `fill_color` | same as `color` | A different colour for filled areas (bar faces, shaded regions) and marker faces. |
| `line_weight` | `1` | Line thickness in pixels. |
| `fill_alpha` | automatic | Opacity of fills, from 0 (transparent) to 1 (solid). |
| `marker` | none on lines, circles on scatters | `None`, `Circle`, `Square`, `Diamond`, `Up`, `Down`, `Left`, `Right`, `Cross`, `Plus`, `Asterisk`. |
| `marker_size` | automatic | Marker radius in pixels. |

**Colours are written `"#RRGGBB"` or `"#RRGGBBAA"`, always in quotes.**
Colour names such as `red` are not accepted.

A series takes only the fields it is drawn with; any other is an error.
`PlotLine`, `PlotScatter`, `PlotStairs`, `PlotStems` and every 3D series
but `Plot3DText` take them all. `PlotBars`, `PlotBarsH`, `PlotHistogram`
and `PlotDigital` take all but `marker` and `marker_size`; `PlotShaded`
takes `color`, `fill_color` and `fill_alpha`; `PlotErrorBars` and
`PlotInfLines` take `color` and `line_weight`; `PlotAnnotation` and
`PlotTag` take `color`; `PlotBarGroups` takes `fill_alpha`.

`PlotHeatmap`, `PlotHistogram2D`, `PlotPieChart`, `PlotText` and
`Plot3DText` take none of them (`PlotText` has `text_color`). Heatmap cells, pie
slices, bar groups and surfaces are coloured by the plot's `colormap`
instead (a `ColorScale` shows what the colours of a heatmap mean):

| `colormap` | Kind |
|---|---|
| `Deep` (default), `Dark`, `Pastel`, `Paired` | Distinct colours, for separate series and pie slices. |
| `Viridis`, `Plasma`, `Hot`, `Cool`, `Pink`, `Jet`, `Greys` | A smooth scale from low to high, for heatmaps and surfaces. |
| `RdBu`, `BrBG`, `PiYG`, `Spectral`, `Twilight` | A scale with a distinct middle, for values around a centre such as zero. |

`colormap` is set on the `Plot` or `Plot3D`, not on the series.

`PlotErrorBars`, `PlotAnnotation` and `PlotTag` without a `color` take
the colour of the series written just before them in the plot.

### Layout and text elements

| Type | Fields | Use |
|---|---|---|
| `VerticalLayout` | `spacing` (pixels between children), `children`, `card` | Stack elements top to bottom. |
| `HorizontalLayout` | `spacing`, `children`, `card`, `align` | Place elements side by side. |
| `Label` | `text`, `text_color`, `wrap` | A line of text, for example a heading or a source note. |
| `Separator` | none | A horizontal rule. |
| `SeparatorText` | `label` | A rule with a text in it. |
| `Spring` | `weight` | Invisible space that takes what is left; two around an element centre it. |
| `ColorScale` | `label`, `scale_min`, `scale_max`, `colormap`, `format`, `height` (default 300, like a `Plot`) | A colour bar with ticks. Put it next to a heatmap's `Plot` in a `HorizontalLayout`, with the same `colormap`, `scale_min` and `scale_max`. |

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

### UML diagrams

Diagram elements draw UML, database, data flow and network diagrams from a list of nodes and edges, like
mermaid; positions are computed (Graphviz `dot`-style layers), never
written. Each can be `root` or a layout child.

| Diagram | Children |
|---|---|
| `ClassDiagram` | `Class` (`id`, `name`, `kind`: Class/Abstract/Interface/Enum, `attributes`, `methods`, `stereotype`), `ClassRelation` (`from`, `to`, `kind`: Association, DirectedAssociation, Inheritance, Realization, Dependency, Aggregation, Composition, `label`, `from_label`, `to_label`) |
| `SequenceDiagram` | `Participant` (`id`, `name`, `kind`: Participant/Actor/Boundary/Control/Entity/Database, `created`) first, then in time order `Message` (`from`, `to`, `text`, `kind`: Sync/Async/Reply/Create/Destroy, `activate`, `deactivate`), `Fragment` (`kind`: Loop/Alt/Opt/Par/Critical/Break/Group, `label`, children) and `Else` (`label`, inside Alt/Par/Critical) |
| `StateDiagram` | `State` (`id`, `name`, `kind`: Simple/Initial/Final/Choice/Fork/Join/History/DeepHistory, `activities`; children make it composite), `Transition` (`from`, `to`, `label`) |
| `ActivityDiagram` | `Action` (`id`, `text`, `kind`: Action/Send/Receive), `ActivityControl` (`id`, `kind`: Initial/Final/FlowFinal/Decision/Merge/Fork/Join, `label`), `Flow` (`from`, `to`, `guard`) |
| `UseCaseDiagram` (`system`: boundary title) | `Actor` (`id`, `name`, `side`: Left/Right), `UseCase` (`id`, `name`), `UseCaseRelation` (`from`, `to`, `kind`: Association/Include/Extend/Generalization) |
| `ERDiagram` (database structure) | `Entity` (`id`, `name`, `attributes`: `"type name PK, FK \"comment\""`), `Relationship` (`from`, `to`, `from_cardinality` / `to_cardinality`: ExactlyOne/ZeroOrOne/ZeroOrMore/OneOrMore, `label`, `identifying`) |
| `DataFlowDiagram` | `ExternalEntity`, `Process`, `DataStore` (`id`, `name`, `number`), `DataFlow` (`from`, `to`, `label`, `bidirectional`; one end a Process), `TrustBoundary` (`id`, `name`, children) |
| `NetworkDiagram` | `Device` (`id`, `name`, `kind`: Server/Database/Storage/Router/Switch/Firewall/LoadBalancer/AccessPoint/Cloud/Internet/Workstation/Laptop/Mobile/User, `address`), `Link` (`from`, `to`, `kind`: Wired/Wireless/Vpn, `label`, `from_label`, `to_label`, `directed`), `Zone` (`id`, `name`, children) |
| `ComponentDiagram` | `Component` (`id`, `name`, `stereotype`, `provides` / `requires`: lists of interface names, `shape`; children make a frame), `ComponentLink` (`from`, `to`, `kind`: Dependency/Association/Delegation/Realization, `label`). Interfaces wire themselves: a name one component provides and another requires is a ball-and-socket; two providers of a name are an error |
| `DeploymentDiagram` | `DeploymentNode` (`id`, `name`, `kind`: Node/Device/ExecutionEnvironment, children are deployed on it), `Artifact` (`id`, `name`), `Component`, `DeploymentLink` (`from`, `to`, `kind`: Communication between nodes/Deploy/Manifest, `label`) |
| `PackageDiagram` | `Package` (`id`, `name`, children), `PackageItem` (`id`, `name`, `kind`: Class/Interface/Enum/Component/Other), `PackageRelation` (`from`, `to`, `kind`: Dependency/Import/Access/Merge, `label`) |
| `ObjectDiagram` | `Object` (`id`, `name`, `class`, `slots`: `["total = 42"]`), `ObjectLink` (`from`, `to`, `kind`: Association/DirectedAssociation/Aggregation/Composition/Dependency, `label`, `from_label`, `to_label`) |
| `TreeDiagram` (`direction`, `edge_style`) | one `TreeItem` (`text`, `shape`, `collapsed`, `children`: more `TreeItem`s nested inside it) |
| `MindMap` | one `MindNode` (`text`, `side`: Left/Right on the root's children, `color`, `collapsed`, nested `MindNode`s), `MindLink` (`from`, `to`, `label`) |
| `OrgChart` | one `Person` (`name`, `title`, `department`, `image`: `person`/`people`/..., `children_layout`: Horizontal/Hanging, `staff`, `vacant`, nested `Person`s), `OrgLink` (`from`, `to`, `label`) |
| `Shape` (not a diagram: a layout element) | `shape` (any name from `list_shapes` / `nymph shapes`), `text`, `width`, `height`, `color`, `fill_color`; diagram nodes take the same names in `shape` (`symbol` on `FlowNode`) |

`Note` (`text`, `attach: [ids]`; in sequence diagrams `over: [a, b]` or
`attach: [a]` with `side`) works in all of them.

- Every node needs a unique `id`; edges name ids. A typo is an error with
  a suggestion.
- Lists of text are YAML lists: `attributes: ["+name: string"]`.
- Steer the layout with `direction: LeftRight` (and `node_spacing`,
  `rank_spacing`) on the diagram, `rank: same-text` on nodes to line them
  up, and `min_length`, `weight`, `constraint: false` on edges.
- A diagram shrinks to fit, down to `min_zoom` (0.5). If it still does not
  fit, the error gives the size it needs: enlarge the image, lower
  `min_zoom`, or split the diagram.

### Flowcharts

`Flowchart` draws any node-and-link diagram: a process, a decision tree,
a dependency or architecture graph (mermaid's `flowchart`). It is laid
out like the UML diagrams and takes the same diagram fields (`direction`,
spacing, zoom, colours) and `Note`.

| Type | Fields | Draws |
|---|---|---|
| `FlowNode` | `id`, `text` (empty shows the id; `"a\nb"` for two lines), `shape`, `rank`, `color`, `fill_color` | A node. `shape`: `Box` (a step, default), `Rounded`, `Stadium` (start / end), `Diamond` (decision), `Circle`, `Parallelogram` (input / output), `Hexagon`, `Cylinder` (database), `Subroutine`. |
| `FlowGroup` | `id`, `title`, `color`, `fill_color`, `children` (nodes, groups, links, notes) | A titled box around its nodes (mermaid's subgraph): a service, a team, a phase. Links may name the group to end at its box. |
| `FlowLink` | `from`, `to` (node or group ids), `label`, `line` (`Solid`, `Dotted`, `Thick`), `arrow` (`End`, `None`, `Both`), `weight`, `min_length`, `constraint`, `color` | A link; `label` sits in its middle (a decision's answer). |

### Gantt charts

`GanttChart` draws a schedule: one row per task, top to bottom, bars on a
time axis. It can be `root` or a layout child.

| Type | Fields | Draws |
|---|---|---|
| `GanttChart` | `title`, `x_label`, `today` (a red line), `colormap`, `bar_height` (0.6 of a row), `show_dependencies` (true), `width`, `height` | The chart. Children: `GanttSection`, `GanttTask`. |
| `GanttSection` | `name` (legend entry), `color`, `children` (tasks) | A phase or team: its tasks share a colour. |
| `GanttTask` | `id`, `name`, `start` or `after: [ids]`, `end` or `duration`, `milestone`, `status` (`Done` pale, `Active` outlined, `Critical` red outline), `color` | A bar from start to end, or a diamond for a milestone. `after` starts the task when the last of those tasks ends, with an arrow from each. |

- Times are dates, `2026-03-02` or `2026-03-02 14:00` (UTC), or plain
  numbers (days, sprints) for a relative plan; one chart uses one kind.
  `duration` counts days for dates.
- An `end` date means its midnight: `start: 2026-03-02, end: 2026-03-05`
  is three days.
- With no `height`, a chart in a `VerticalLayout` is as tall as its rows
  need; as `root` it fills the image.

### Sankey diagrams

`SankeyDiagram` shows quantities flowing between nodes (energy, budget,
traffic, a funnel) as bands as wide as the amount. Nodes are placed in
columns automatically, from sources on the left to sinks on the right.

| Type | Fields | Draws |
|---|---|---|
| `SankeyDiagram` | `from`, `to`, `values` (one entry per flow: `$source`, `$target`, `$amount` from a table), `title`, `colormap`, `link_color` (`Source`, `Target`, `Gray`), `show_values` (true), `value_format` (`"%g"`), `node_width`, `node_spacing`, `width`, `height` | The diagram. Children: `SankeyFlow`, `SankeyNode`. |
| `SankeyFlow` | `from`, `to`, `value`, `color` | One more flow, instead of (or besides) the lists. |
| `SankeyNode` | `id` (the name flows use), `name` (label), `color` | Optional: a node's label or colour; nodes are listed in this order. |

- Nodes are created by the flows that name them (as in mermaid); a
  misspelt name makes a separate node, so check the labels.
- Values must be above 0, a flow cannot go from a node to itself, and
  flows must not loop back (`makes a cycle`).
- Each label shows the node's total (the larger of what enters and what
  leaves) unless `show_values: false`.

### User interfaces (mockups)

To design or mock a user interface, draw it with Dear ImGui's widgets: a
`Screen` (as `root`) holds `Window`s, each holding widgets. Types are
ImGui's function names (`Button`, `Checkbox`, `SliderFloat3`,
`InputTextWithHint`, `ColorEdit4`, `TreeNode`, `TabBar` / `TabItem`,
`Table`, `Menu`, `PopupModal`, ...), fields are their parameters and
flags are ImGui's names without the prefix (`flags: NoResize|NoCollapse`).
`nymph schema Window` and `nymph schema SliderFloat` list the fields.

| Type | Key fields | Draws |
|---|---|---|
| `Screen` | `window_rounding`, `frame_rounding`, `background` | The area the windows sit on. Children: `Window`, `MainMenuBar`, `Popup`, `PopupModal`, `WindowTransition`. |
| `Window` | `title`, `x`, `y`, `width`, `height` (0 fits the content, -1 reaches the edge), `id`, `flags`, `collapsed` | An ImGui window. Can also be `root` alone. |
| Widgets | `label`, `value` (the state shown), `width`, `disabled`, `tooltip`, `opens` | `Text`, `Button`, `Checkbox`, `RadioButton`, sliders, drags, inputs, colours, `Combo` / `ListBox` (`items`, `selected`), `Selectable`, `ProgressBar`, `Image`. |
| Containers | `open`, `selected`, `children` | `TreeNode`, `CollapsingHeader`, `TabBar` > `TabItem`, `Child`, `Group`, `Indent`, `Disabled`, `Table` > `TableColumn` (`cells`) / `TableRow`. |
| Layout | | `SameLine` (next widget on the same line), `Separator`, `SeparatorText`, `Spacing`, `NewLine`, `Dummy`. |
| `Image` | `image` (`person`, `people`, `landscape`, `product`, `logo`, `video`, `map`, `chart`, `document`, `icon`, `building`, `cross`), `width`, `height`, `tint`, `background`, `rounding`, `label` | A built-in placeholder picture: a portrait silhouette, a photo, a logo. `rounding` at half the size makes a round avatar. |
| `WindowTransition` | `from`, `to` (window `id`s), `label`, `dashed` | A labelled arrow between windows: the UI flow ("ok", "cancel", "finished loading"). |

- Windows need room: place them with gaps (arrows and labels go in the
  gaps) and inside the image; one that does not fit is an error.
- Labels may repeat (`OK` in two dialogs); `##` hides the rest of a label.
- A plot or layout can go inside a window, with its own `height`.
- A `Popup` / `PopupModal` (child of the Screen, with an `id`) shows with
  `open: true`, or when a widget with `opens: <id>` is clicked in a live
  view. Only one popup or menu can be open at a time.
- In the live viewer the user can use the widgets; you get a note with
  the `view:` block (what they changed, by element path) and the last
  widget clicked.

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
legend box off the first cell. The grid spans 0 to 1 on both axes, so
ticks at the cell centres (0.1, 0.3, ... for 5 columns) name the rows and
columns. A `ColorScale` with the same colormap and range shows the
values of the colours.

```yaml
description: Activity by weekday (columns) and time of day (rows, morning at the top).
image: {width: 520, height: 360}
root:
  type: HorizontalLayout
  spacing: 8
  children:
    - type: Plot
      title: Activity
      width: -1
      height: -1
      colormap: Viridis
      flags: NoLegend
      x_flags: NoGridLines|NoTickMarks
      y_flags: NoGridLines|NoTickMarks
      x_ticks: [0.1, 0.3, 0.5, 0.7, 0.9]
      x_tick_labels: [Mon, Tue, Wed, Thu, Fri]
      y_ticks: [0.875, 0.625, 0.375, 0.125]
      y_tick_labels: $slot
      children:
        - type: PlotHeatmap
          rows: 4
          cols: 5
          values: [$mon, $tue, $wed, $thu, $fri]
          scale_min: 0
          scale_max: 10
          format: "%.0f"
    - {type: ColorScale, label: events, height: -1, scale_min: 0, scale_max: 10, colormap: Viridis}
data: |
  slot, mon, tue, wed, thu, fri
  morning, 2, 4, 6, 3, 1
  noon, 5, 7, 9, 6, 4
  evening, 8, 9, 10, 7, 6
  night, 1, 2, 3, 2, 5
```

### Grouped bars with category names

`values: [$north, $south]` gives one row per group, one value per label.
The groups sit at 0, 1, 2, ...; `x_ticks` and `x_tick_labels` put the
category names under them.

```yaml
description: Revenue per quarter in two regions, in millions.
root:
  type: Plot
  title: Revenue by quarter
  y_format: "%.0fM"
  x_ticks: [0, 1, 2, 3]
  x_tick_labels: $quarter
  y_min: 0
  y_max: 30
  children:
    - {type: PlotBarGroups, labels: [north, south], values: [$north, $south]}
data: |
  quarter, north, south
  Q1, 12, 9
  Q2, 18, 14
  Q3, 15, 17
  Q4, 22, 19
```

### Error bars and callouts

Write the error bars right after their line so they take its colour.
Give `err` for a symmetric error, or `neg` and `pos`.

```yaml
description: Median latency per build with its spread, against a 40 ms budget.
root:
  type: Plot
  title: Latency per build
  y_format: "%.0f ms"
  y_min: 0
  y_max: 60
  children:
    - {type: PlotLine, label: median, xs: $build, ys: $median, marker: Circle}
    - {type: PlotErrorBars, xs: $build, ys: $median, neg: $below, pos: $above}
    - {type: PlotAnnotation, x: 4, y: 34, text: cache disabled}
    - {type: PlotInfLines, label: budget, values: [40], horizontal: true, color: "#C0392B", line_weight: 2}
    - {type: PlotTag, value: 40, text: budget}
data: |
  build, median, below, above
  1, 21, 4, 9
  2, 22, 5, 8
  3, 20, 4, 10
  4, 34, 7, 15
```

### Dates, a log axis and a second Y axis

With `x_scale: Time`, `xs` are UNIX seconds (UTC); the ticks show dates
and times. `y_scale: Log10` suits values from tens to tens of thousands.
A series with `y_axis: Y2` uses the axis on the right.

```yaml
description: Requests per hour (log scale) and error rate over one day, in UTC.
root:
  type: Plot
  title: Traffic
  x_scale: Time
  y_scale: Log10
  y_label: requests per hour
  y2_label: error rate
  y2_format: "%.1f%%"
  children:
    - {type: PlotLine, label: requests, xs: $time, ys: $requests}
    - {type: PlotLine, label: errors, xs: $time, ys: $errors, y_axis: Y2}
data: |
  time, requests, errors
  1772409600, 120, 0.4
  1772424000, 900, 0.6
  1772438400, 21000, 1.8
  1772452800, 9000, 1.2
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

### Flowchart

```yaml
description: How a pull request gets merged.
root:
  type: Flowchart
  direction: LeftRight
  children:
    - {type: FlowNode, id: open, text: PR opened, shape: Stadium}
    - {type: FlowNode, id: ci, text: CI green?, shape: Diamond}
    - {type: FlowNode, id: fix, text: Fix}
    - type: FlowGroup
      id: review
      title: Review
      children:
        - {type: FlowNode, id: read, text: Read the diff}
        - {type: FlowNode, id: ok, text: Approve, shape: Rounded}
        - {type: FlowLink, from: read, to: ok}
    - {type: FlowNode, id: merge, text: Merge, shape: Stadium}
    - {type: FlowLink, from: open, to: ci}
    - {type: FlowLink, from: ci, to: fix, label: "no"}
    - {type: FlowLink, from: fix, to: ci, line: Dotted}
    - {type: FlowLink, from: ci, to: review, label: "yes"}
    - {type: FlowLink, from: review, to: merge, line: Thick}
```

### Gantt chart

```yaml
description: Website relaunch plan.
image: {width: 760, height: 360}
root:
  type: GanttChart
  title: Relaunch
  today: 2026-03-10
  children:
    - type: GanttSection
      name: Design
      children:
        - {type: GanttTask, id: wire, name: Wireframes, start: 2026-03-02, duration: 5, status: Done}
        - {type: GanttTask, id: look, name: Visual design, after: [wire], duration: 4, status: Active}
    - type: GanttSection
      name: Build
      children:
        - {type: GanttTask, id: dev, name: Development, after: [look], duration: 10}
        - {type: GanttTask, name: Launch, after: [dev], milestone: true}
```

### Sankey diagram

```yaml
description: Monthly budget, in euros.
root:
  type: SankeyDiagram
  title: Budget
  from: $from
  to: $to
  values: $eur
  value_format: "%.0f"
data: |
  from, to, eur
  Salary, Budget, 3000
  Freelance, Budget, 800
  Budget, Rent, 1200
  Budget, Food, 600
  Budget, Savings, 1000
  Budget, Other, 1000
```

### Class diagram

```yaml
description: Payment classes.
image: {width: 640, height: 420, font: FiraSans-Regular.ttf}
root:
  type: ClassDiagram
  children:
    - {type: Class, id: Payment, kind: Interface, methods: ["+pay(amount) bool"]}
    - {type: Class, id: Card, attributes: ["-number: string"], methods: ["+pay(amount) bool"]}
    - {type: Class, id: Wallet, attributes: ["-balance: float"], methods: ["+pay(amount) bool"]}
    - {type: Class, id: Order, attributes: ["+total: float"]}
    - {type: ClassRelation, from: Card, to: Payment, kind: Realization}
    - {type: ClassRelation, from: Wallet, to: Payment, kind: Realization}
    - {type: ClassRelation, from: Order, to: Payment, kind: DirectedAssociation, label: paid by, to_label: "1"}
```

### Sequence diagram

```yaml
description: Checkout calls.
image: {width: 640, height: 420}
root:
  type: SequenceDiagram
  autonumber: true
  children:
    - {type: Participant, id: app, name: App}
    - {type: Participant, id: api, name: API}
    - {type: Participant, id: bank, name: Bank}
    - {type: Message, from: app, to: api, text: checkout, activate: true}
    - type: Fragment
      kind: Alt
      label: card accepted
      children:
        - {type: Message, from: api, to: bank, text: charge}
        - {type: Message, from: bank, to: api, text: ok, kind: Reply}
        - {type: Else, label: declined}
        - {type: Message, from: api, to: app, text: error, kind: Reply}
    - {type: Message, from: api, to: app, text: receipt, kind: Reply, deactivate: true}
```

### Database tables (ER diagram)

```yaml
description: Blog database.
image: {width: 640, height: 420}
root:
  type: ERDiagram
  children:
    - {type: Entity, id: user, name: USER, attributes: ["int id PK", "string email UK"]}
    - {type: Entity, id: post, name: POST, attributes: ["int id PK", "int author_id FK", "string title"]}
    - {type: Relationship, from: user, to: post, from_cardinality: ExactlyOne, to_cardinality: ZeroOrMore, label: writes}
```

### Network diagram

```yaml
description: Home network.
image: {width: 520, height: 420}
root:
  type: NetworkDiagram
  children:
    - {type: Device, id: isp, name: Internet, kind: Internet}
    - {type: Device, id: router, name: Router, kind: Router, address: 192.168.1.1}
    - type: Zone
      id: home
      name: Home LAN
      children:
        - {type: Device, id: nas, name: NAS, kind: Storage}
        - {type: Device, id: phone, name: Phone, kind: Mobile}
    - {type: Link, from: isp, to: router}
    - {type: Link, from: router, to: nas}
    - {type: Link, from: router, to: phone, kind: Wireless}
```

State, activity, use case and data flow diagrams follow the same pattern: see
[examples.md](examples.md).

### A user interface mockup

```yaml
description: Sign-in and inbox screens of a mail app, and how one leads to the other.
image: {width: 760, height: 360}
root:
  type: Screen
  window_rounding: 6
  frame_rounding: 4
  children:
    - type: Window
      id: sign_in
      title: Sign in
      x: 16
      y: 60
      width: 260
      height: 210
      flags: NoResize|NoCollapse
      children:
        - {type: Image, image: logo, width: -1, height: 48, tint: "#2F80ED", background: "#00000000"}
        - {type: InputTextWithHint, label: "##email", hint: Email, width: -1}
        - {type: InputText, label: "##password", value: secret, flags: Password, width: -1}
        - {type: Checkbox, label: Remember me, value: true}
        - {type: Button, label: Sign in, size: [118, 0]}
        - {type: SameLine}
        - {type: Button, label: Cancel, size: [118, 0]}
    - type: Window
      id: inbox
      title: Inbox
      x: 470
      y: 30
      width: 270
      height: 300
      children:
        - {type: Image, image: person, width: 40, height: 40, rounding: 20}
        - {type: SameLine}
        - {type: Text, text: Ada Lovelace}
        - {type: Separator}
        - {type: Selectable, label: Engine notes, selected: true}
        - {type: Selectable, label: Tea on Sunday?}
        - {type: Button, label: Sign out}
    - {type: WindowTransition, from: sign_in, to: inbox, label: sign in}
    - {type: WindowTransition, from: inbox, to: sign_in, label: sign out}
```

## Making a good chart

- Give every chart a `title` and axis labels with units.
- Give every series a `label` when there is more than one.
- Write a real `description`. It is the image's alt text.
- For names on the X axis (categories), plot against 0, 1, 2, ... and
  name the ticks with `x_ticks` and `x_tick_labels`. For dates and times,
  give UNIX seconds and set `x_scale: Time`.
- Use `y_scale: Log10` when values span several orders of magnitude, and
  `y_format` to put the unit on the ticks (`"%.0f ms"`).
- Show uncertainty with `PlotErrorBars` or a `PlotShaded` band; point out
  events with `PlotAnnotation` instead of explaining them only in text.
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
- Dates are UNIX seconds and are always shown in UTC.
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
| `unknown id 'x'` | An edge or note names an id no node has; the message suggests the closest and lists the ids. |
| `id 'x' is already used` | Two nodes of one diagram share an id. |
| `ClassDiagram needs W x H px (at min_zoom ...)` | The diagram does not fit even scaled down: enlarge the image or its space, lower `min_zoom`, or split it. |
| `only the keys PK, FK and UK` / `needs a type and a name` | An ER attribute is `"type name"`, then optional `PK`, `FK`, `UK` and a `"comment"`. |
| `one end of a data flow must be a Process` | A `DataFlow` joins a Process to anything; entity-to-store flows go through a Process. |
| `must name a Device, not a Zone` / `names a TrustBoundary` | Edges name the nodes inside a group, not the group. |
| `declared after the first message` | In a sequence diagram, list every `Participant` before the messages. |
| `'abc' is not a number` | A referenced column has a non-numeric value at the reported row. |
| `xs has 3 values but ys has 2` | Series lists must be equally long. |
| `is empty: the series has no data` | A series list is empty, or its column has no rows. |
| `rows x cols needs` | `PlotHeatmap.values` does not hold `rows * cols` numbers. |
| `x_tick_labels has 3 labels but x_ticks has 2 values` | Give one label per tick position. |
| `must be above 0 on a Log10 axis` | A fixed limit of a `Log10` axis is 0 or negative; raise it or use `SymLog`. |
| `does not apply to a Time axis` | Remove the `_format` of a `Time` axis; its ticks show dates. |
| `not a whole number of groups` | `PlotBarGroups.values` needs one value per label in each group. |
| `give err, or neg and pos` | `PlotErrorBars` needs `err`, or both `neg` and `pos`, as long as `ys`. |
| `joins a FlowGroup with a node inside it` | A link from a group to its own node; link the inner nodes instead. |
| `GanttTask.start is empty: give start, or after` | Every task needs a `start`, or `after: [ids]` of the tasks it waits for. |
| `GanttTask.end is empty: give end, or a duration` | Give `end` or `duration` (or set `milestone: true`). |
| `is not a date (YYYY-MM-DD ...` | Write dates as `2026-03-02` or `2026-03-02 14:00`, or use plain numbers throughout. |
| `uses dates; a chart uses one or the other` | Mixed dates and numbers in one `GanttChart`. |
| `makes a cycle` | Tasks wait for each other in a loop through `after`, or Sankey flows lead back to where they started. |
| `has value 0; a flow's value must be above 0` | A Sankey flow of 0, negative or missing; leave such rows out. |
| `is in no flow` | A `SankeyNode` whose `id` no flow names (often a spelling difference). |
| `CSV row has N cells, the header has M` | A row is too short or too long, or a value with a comma is not quoted. |
| `'>' joins the CSV lines` | Write `data: |`, not `data: >`. |
| `needs 620 px of height, has 484` | The fixed sizes in a layout do not fit. Use `height: -1` on plots, reduce sizes, or enlarge the image. |
| `puts the window's right edge at x = 940, the Screen is 900 wide` | A `Window` does not fit its Screen: move it, shrink it, or enlarge the image. |
| `names no Window id` / `names no Popup or PopupModal id` | A `WindowTransition`'s `from` / `to`, or a widget's `opens`, does not match an `id` on the Screen. |
| `ImGui shows one open popup or menu at a time` | Two popups or menus have `open: true`; keep one. |
| `cannot be placed in Window; it goes inside: TabBar` | A `TabItem` goes in a `TabBar`; `TableColumn` / `TableRow` in a `Table`; `MenuItem` in a `Menu`. |
| `tab character in indentation` | Indent with spaces. |
| `mapping values are not allowed`, `could not find expected ':'` | A YAML syntax error at that line: check indentation, colons and quotes. |
| `missing 'root'` | Every source needs a `root`. |
| `not a nymph image` | The PNG was not made by nymph, or lost its embedded YAML. |
