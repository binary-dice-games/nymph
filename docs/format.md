# nymph — Source Format

A nymph source is one YAML document. It describes what to draw (`root`)
and the numbers to draw (`data`). `nymph chart.yaml` turns it into
`chart.png`, and the PNG keeps a copy of the text.

```yaml
nymph: 1
description: Monthly revenue against cost, 2025.
image: {width: 800, height: 450}
root:
  type: Plot
  title: Revenue vs cost
  x_label: month
  y_label: kUSD
  children:
    - {type: PlotLine, label: revenue, xs: $month, ys: $revenue}
    - {type: PlotBars, label: cost, xs: $month, ys: $cost}
data: |
  month, revenue, cost
  1, 12, 9
  2, 15, 9.5
  3, 11, 10
```

Block style (`key: value` on indented lines) and flow style
(`{key: value}`, `[a, b]`) can be mixed freely. JSON input is not
supported.

## Top-level keys

| Key | Required | Meaning |
|---|---|---|
| `nymph` | no | Format version. Only `1` exists. |
| `description` | no | Plain text saying what the chart shows. Stored in the PNG as its description (alt text). |
| `image` | no | Size and look of the PNG, below. |
| `root` | yes | The element to draw. |
| `data` | no | The table, or named tables, that fields refer to with `$column` / `$table:column`. |
| `source` | no | How the data was made: the prompt, query or code. Kept in the PNG, never drawn. |
| `view` | no | Zoom, pan, rotation and hidden series per element, as the [web viewer](viewer.md) saves them. Below. |

Any other key is an error.

## `image`

| Key | Default | Meaning |
|---|---|---|
| `width`, `height` | `800`, `500` | Size in pixels, 16 to 4096. |
| `scale` | `1` | 1 to 4. The PNG is `width*scale` x `height*scale` pixels with the same layout, only sharper. |
| `theme` | `light` | `light`, `dark` or `classic`. |
| `padding` | `8` | Empty pixels between the image edge and `root`. |
| `font` | `default` | Font of all text, by file name (see "Fonts and text style"). |
| `font_size` | `16` | Text size in pixels, 1 to 96. |
| `text_color` | theme | Colour of all text, `"#RRGGBB"`. |

## Elements

An element is a mapping with a `type`, any fields of that type, and, for
containers, a `children` list of elements:

- `Plot` holds 2D series (`PlotLine`, `PlotBars`, `PlotScatter`, ...).
- `Plot3D` holds 3D series (`Plot3DSurface`, `Plot3DLine`, ...).
- `VerticalLayout` and `HorizontalLayout` hold plots, diagrams, other
  layouts, `Label`, `Separator`, `SeparatorText` and `Spring`.
- UML diagrams hold their nodes and edges: `ClassDiagram`,
  `SequenceDiagram`, `StateDiagram`, `ActivityDiagram`, `UseCaseDiagram`,
  `ERDiagram` for database structure, `DataFlowDiagram` and
  `NetworkDiagram` (see [UML diagrams](#uml-diagrams)).

A series goes directly inside its kind of plot; anything else can be
`root`. [elements.md](elements.md) lists every type with its fields,
types, defaults and allowed values (also printed by `nymph schema` and
`nymph schema <Type>`).

### Field values

| Field type | Written as | Notes |
|---|---|---|
| number | `0.5`, `-3`, `1e6` | Not in quotes: `"3"` is an error. |
| integer | `4` | Whole numbers only. |
| true/false | `true`, `false` | `yes`/`no`, `on`/`off` work too. |
| text | `Revenue`, `"a: b"` | Quote text containing `: `, ` #`, or starting with `[`, `{`, `*`, `&`, `!`, `%`, `@`. A number written in a text field stays text (`title: 2025`). |
| colour | `"#RRGGBB"`, `"#RRGGBBAA"` | **Always in quotes**: an unquoted `#` starts a YAML comment. Names like `red` are not accepted. |
| name | `Circle` | One of the listed names. |
| flags | `NoLegend\|Equal` | Names joined with `\|`; a number also works. |
| list of numbers | `[1, 2.5, 4]` or `$column` | See column references. |

Names are case-sensitive. An unknown type, field, name or key is an
error, with a "did you mean" suggestion when one is close.

## Fonts and text style

nymph has fonts built in, so it needs no font files. Pick one by its
file name:

| Font | Files |
|---|---|
| built-in (ImGui's) | `default` |
| Fira Sans | `FiraSans-Regular.ttf`, `FiraSans-Bold.ttf`, `FiraSans-Italic.ttf` |
| Fira Mono | `FiraMono-Regular.ttf`, `FiraMono-Bold.ttf` |
| PT Serif | `PT_Serif-Web-Regular.ttf`, `PT_Serif-Web-Bold.ttf`, `PT_Serif-Web-Italic.ttf` |

`nymph fonts` prints the list of the binary you have (a build can embed
other fonts, see [building.md](building.md)).

Three fields set the text style: `font`, `font_size` (pixels) and
`text_color`. They go on `image` for the whole figure, and on
`VerticalLayout`, `HorizontalLayout`, `Plot`, `Plot3D`, `Label`,
`SeparatorText`, `PlotText` and `Plot3DText`. An element passes its style
to everything inside it; an empty value (or a size of 0) inherits.

```yaml
image: {font: FiraSans-Regular.ttf, font_size: 15}
root:
  type: VerticalLayout
  children:
    - {type: Label, text: Quarterly report, font: PT_Serif-Web-Bold.ttf, font_size: 28}
    - type: Plot
      title: Costs
      font: FiraMono-Regular.ttf      # title, ticks and legend in Fira Mono
      text_color: "#2471A3"
      children:
        - {type: PlotBars, ys: [2, 3, 3, 4]}
```

On a plot, the style covers its title, axis and tick labels, legend and
text inside it. `text_color` leaves grid lines alone. `font: default`
switches back to the built-in font inside a subtree that set another.

Every diagram item that draws text (`FlowNode`, `FlowGroup`, `FlowLink`,
a class, a state, a message, a `Note`, ...) takes the three fields too and
styles its own text; unset, it follows the diagram's.

```yaml
- {type: FlowNode, id: warn, text: Careful, text_color: "#C0392B", font_size: 22}
```

## Layout

A layout places its children one after another: `VerticalLayout` top to
bottom, `HorizontalLayout` left to right, with `spacing` pixels between
them.

Along the layout's direction (`height` in a `VerticalLayout`, `width` in
a `HorizontalLayout`) each child's size is:

| Size | Meaning |
|---|---|
| positive | Fixed pixels. |
| `0` | Natural size: a label's text, a separator's line, a layout's content, 400 x 300 for a `Plot`, 400 x 400 for a `Plot3D`. |
| negative | Stretch: shares the space left over with the other stretch children, in proportion (`-1` and `-3` split it 1:3). |

`Spring` is invisible stretch space (`weight`, default 1); two Springs
around an element centre it.

Across the layout, a positive size is fixed and anything else fills.
A `Plot` has `width: -1` and `height: 300` by default, so in a
`HorizontalLayout` give plots `height: -1` to fill the row, and in a
`VerticalLayout` give them `height: -1` to share the column.

`root` fills the image (minus `image.padding`) unless it sets a positive
`width` or `height`. A `Plot` used as `root` with no `height` fills the
image.

If fixed and natural sizes do not fit, nymph reports an error with the
numbers (`VerticalLayout needs 620 px of height, has 484`) instead of
cutting something off.

`card: true` draws a layout's children on a rounded panel inset by
`padding`. `HorizontalLayout.align: right` pushes a row with no stretch
child to the right edge.

`Label` shows `text` (wrapped to the width with `wrap: true`), coloured
by `text_color`, or `text_color_light` / `text_color_dark` depending on
the theme.

## `data`

`data` holds one table, or several named tables. A table has named
columns and is written in one of two ways.

**CSV text**, in a literal block (`|`):

```yaml
data: |
  month, revenue, "unit price"
  1, 12, 3.5
  2, 15, 3.75
```

- The first row is the header. Names must be unique and not empty.
- Spaces around values are ignored; blank lines are skipped.
- A value containing a comma, a quote or a line break goes in double
  quotes; a quote inside it is written twice (`"said ""hi"""`).
- Every row has as many values as the header.
- Use `|`, not `>`: `>` joins the lines into one.

**Columns**, as a mapping of lists:

```yaml
data:
  month: [1, 2, 3]
  revenue: [12, 15, 11]
```

All lists have the same length. `~`, `null` or nothing is an empty cell.

In both forms an empty cell in a column used as numbers is a gap in the
chart, and any other value that is not a number is an error naming its
row. Columns nothing refers to are not checked, so text columns are fine.

**Named tables.** For data of different shapes (a monthly series and a
per-region summary), give each table a name: a `data` mapping whose
values are tables, in either form.

```yaml
data:
  monthly: |
    month, revenue
    1, 92
    2, 88
    3, 104
  regions:
    name: [north, south]
    share: [60, 40]
```

Tables may have different numbers of rows and the same column names.
Names cannot contain `:`. A `data` mapping is either all columns (lists)
or all tables (text or mappings), never a mix.

## Column references

A value starting with `$` takes data from a column:

| Written as | Meaning |
|---|---|
| `ys: $revenue` | The column named `revenue`. |
| `ys: "$unit price"` | A name with spaces: quote the whole value. |
| `ys: $2` | The second column, counting from 1. |
| `values: [$mon, $tue, $wed]` | Several columns interleaved row by row (row 1 of each, then row 2, ...): the row-major grid `PlotHeatmap.values` and `Plot3DSurface.zs` expect. |
| `labels: $name` | In a text field: the column's cells, one per line (pie slice names). |
| `title: $$5 per unit` | `$$` is a literal `$`. |
| `ys: $monthly:revenue` | Column `revenue` of the table named `monthly`. |
| `ys: $monthly:2` | The second column of `monthly`. |

Only list fields and text fields take references. A list is either all
numbers or all references, never a mix.

With named tables, write `$table:column`. A bare `$column` works only
when there is a single table; with several, nymph asks which one (and
lists the tables that have that column). `[$a:x, $b:y]` may combine
columns from tables with the same number of rows.

## `source`

`source` records how the data was made, so whoever changes the chart
later (often an AI agent reading the PNG back) can regenerate or update
it: the prompt, a query, a command, or the code. nymph keeps it in the
PNG with the rest of the YAML and never draws or checks it, so it can
be any YAML; text or a mapping of text reads best:

```yaml
source:
  prompt: Chart 2025 revenue by month, and each region's share.
  code: |
    df = pd.read_csv("orders_2025.csv", parse_dates=["date"])
    monthly = df.groupby(df.date.dt.month).revenue.sum() / 1000
  language: python
```

A `$` inside `source` is plain text, not a column reference.

## `view`

`view` holds what a user did to the chart in the [web viewer](viewer.md)
(zoom, pan, rotation, series hidden from the legend), so the chart opens
the same way again, in the viewer and as a PNG. The viewer writes it when
the chart is saved (`toYaml()`, `toPng()`, the right-click menu); it can
also be written by hand. Each entry names an element by its path in the
source (`root`, `root/children/1`, `root/children/0/children/2`):

```yaml
view:
  root/children/0: {x: [2, 6.5], y: [10, 40], hidden: [memory]}
  root/children/1: {zoom: 1.5, center: [310, 220]}
  root/children/2: {rotation: [0.11, 0.23, 0.31, 0.92], z: [-1, 1]}
```

| Element | Keys |
|---|---|
| `Plot` | `x`, `y`, `y2`: `[min, max]` ranges, which win over `x_min` / `x_max` ...; `hidden`: legend labels switched off. |
| `GanttChart` | `x`: the time range shown, in the chart's time unit (UNIX seconds for dates); `hidden`: the names of the sections switched off in the legend (their rows close up). |
| `Plot3D` | `x`, `y`, `z`; `rotation`: a quaternion `[x, y, z, w]`; `hidden`. |
| UML diagrams, `Flowchart` | `zoom`: 0.25 to 8 times the fit; `center`: the diagram point at the centre, in diagram units at zoom 1. |

An entry whose path names no such element (the source changed since the
view was saved) is ignored; a malformed entry of an element that has a
view is an error. The ranges and hidden series of a view are where the
chart starts: the user can still pan, zoom and show them again.

## UML diagrams

A diagram is an element like `Plot`: it can be `root` or sit in a layout,
and takes `title`, `width` / `height` hints and the text style. Its
children are its nodes and edges, and nodes are placed automatically: a
layered layout after Graphviz's `dot` for class, state, activity and use
case diagrams, columns and rows for sequence diagrams.

```yaml
root:
  type: ClassDiagram
  children:
    - {type: Class, id: Animal, kind: Abstract, methods: ["+speak()*"]}
    - {type: Class, id: Dog, attributes: ["+name: string"]}
    - {type: ClassRelation, from: Dog, to: Animal, kind: Inheritance}
```

| Diagram | Nodes | Edges and others |
|---|---|---|
| `ClassDiagram` | `Class` | `ClassRelation` |
| `SequenceDiagram` | `Participant` | `Message`, `Fragment` (with `Else`) |
| `StateDiagram` | `State` (composite when it has children) | `Transition` |
| `ActivityDiagram` | `Action`, `ActivityControl` | `Flow` |
| `UseCaseDiagram` | `Actor`, `UseCase` | `UseCaseRelation` |
| `ERDiagram` (database) | `Entity` (a table) | `Relationship` (crow's foot) |
| `DataFlowDiagram` | `ExternalEntity`, `Process`, `DataStore` | `DataFlow`, `TrustBoundary` (groups nodes) |
| `NetworkDiagram` | `Device` (icon by `kind`) | `Link`, `Zone` (groups devices) |
| `Flowchart` | `FlowNode`, `FlowGroup` (a box around nodes, mermaid's subgraph) | `FlowLink` |

`Note` goes in every diagram. `Flowchart` is not UML: it draws any
node-and-link diagram, with mermaid's flowchart shapes and link styles.

- **Ids.** Every node has an `id`, unique in the diagram; `name` (or
  `text`) is what is shown and defaults to the id. Edges name nodes by id
  in `from` / `to`, notes in `attach` / `over`. An unknown id is an error
  with a suggestion; nothing is created by being named.
- **Lists of text.** Members, activities and id lists are YAML lists:
  `attributes: ["+name: string", "+age: int"]`; `$column` gives a
  column's cells. A member ending in `$` is underlined (static), in `*`
  italic (abstract).
- **Steering the layout** (graph diagrams), after dot's attributes:
  `direction` (`TopDown`, `BottomUp`, `LeftRight`, `RightLeft`),
  `node_spacing`, `rank_spacing` and `edge_style` on the diagram; `weight`,
  `min_length` and `constraint` on edges; `rank` on nodes (nodes with the
  same `rank` text share a rank).
- **ER attributes** are written as in mermaid: `"type name"`, then
  optional keys `PK`, `FK`, `UK` (comma-separated) and a `"comment"` in
  double quotes: `attributes: ["int id PK", "int customer_id FK \"buyer\""]`.
  A `Relationship` sets `from_cardinality` / `to_cardinality`
  (`ExactlyOne`, `ZeroOrOne`, `ZeroOrMore`, `OneOrMore`), drawn at each
  end; `identifying: false` draws it dashed.
- **Data flows** join two nodes, one of them a `Process`, never a
  `TrustBoundary`; `bidirectional: true` draws both arrowheads. A
  `TrustBoundary` (or a network `Zone`) is a dashed box around its
  children; links and flows name the nodes inside it.
- **Network links** join two `Device`s. `kind` is `Wired` (solid),
  `Wireless` (dashed) or `Vpn` (long dashes); `from_label` / `to_label`
  name the ports, `directed: true` adds an arrow.
- **Order.** In a sequence diagram the children are time, top to bottom;
  declare every `Participant` first. Elsewhere order only breaks ties.
- **Size.** In a layout a diagram with `width` / `height` 0 takes its
  natural size; as `root` it fills the image. It is scaled down to fit
  its space, but not below `min_zoom` (default 0.5): beyond that,
  rendering fails with the size the diagram needs. `max_zoom` (default 1)
  lets a small diagram grow.
- **UML rules are checked**: nothing enters an initial state or leaves a
  final one, `Else` only inside an `Alt`, `Par` or `Critical` fragment,
  `Include` only between use cases, a `deactivate` needs an open
  activation, and so on.

## Gantt charts

`GanttChart` is a schedule: `GanttTask` children (optionally inside
`GanttSection`s, which give them a colour and a legend entry), one row
each, top to bottom.

```yaml
root:
  type: GanttChart
  today: 2026-03-04
  children:
    - {type: GanttTask, id: a, name: Design, start: 2026-03-02, duration: 3}
    - {type: GanttTask, name: Build, after: [a], end: 2026-03-12}
```

- **Times** are ISO dates (`2026-03-02`, `2026-03-02 14:00`, in UTC) or
  plain numbers; one chart uses one kind. A date `end` is its midnight.
- **Start**: `start`, or `after` (ids of tasks it waits for: it starts
  when the last ends, and an arrow is drawn from each). **End**: `end`,
  or `duration` (days for dates). A `milestone` has a start only.
- **Errors**: a task without a start or an end, both `start` and
  `after`, an unknown or circular `after`, an end before the start,
  dates mixed with numbers.

## Sankey diagrams

`SankeyDiagram` draws flows between nodes as bands as wide as their
value. Flows come from the lists `from`, `to` and `values` (so a table
with one row per flow maps on with `$column`), and from `SankeyFlow`
children; nodes are made by the flows that name them. `SankeyNode`
optionally gives a node a label or colour.

```yaml
root:
  type: SankeyDiagram
  from: [Gas, Gas, Coal]
  to: [Power, Heat, Power]
  values: [30, 20, 50]
```

Values must be above 0, no flow may go from a node to itself, and the
flows may not form a cycle.

## Errors

Every problem is reported on one line with its position in the file:

```
nymph: chart.yaml:9:52: unknown column 'revenu' (did you mean 'revenue'?) (columns: month, revenue, cost)
```

Positions inside CSV data point at the line in the YAML file. Besides
unknown names, nymph checks rules a chart needs: `xs` and `ys` of the
same length, a non-empty series, a heatmap with `rows x cols` values, one
pie label per value, a mesh index within its vertices, and so on.

## Limits

- At most 16 MiB of source and 1,000,000 data rows.
- A UML diagram has at most 500 nodes, 2,000 edges (sequence diagrams:
  2,000 rows) and 8 levels of composite states or fragments.
- Dates on a `Time` axis are UNIX seconds, always shown in UTC.
- The YAML stays in the PNG only while the file is copied byte for byte;
  services that re-encode images drop it. Keep the YAML too.
