---
name: nymph
description: Make charts and diagrams as PNG images from a short YAML description with nymph - line, bar, scatter, pie, heatmap, histogram and 3D plots, dashboards of several charts, UML, ER, network and data-flow diagrams, flowcharts, Gantt and Sankey charts. Use whenever a chart or diagram would explain data or structure better than text or a table.
---

# nymph: charts from YAML

You write a small YAML source; nymph draws it as a PNG. The PNG carries
its source, so it can be read back and changed later.

## How to run it

- With the nymph MCP tools: `render_chart` (returns the image; give
  `output_path` to also save a `.png`), `check_chart`, `chart_schema`
  (fields of an element type), `extract_chart` (the source of a nymph
  PNG), `list_fonts`.
- Without them: the `nymph` command. `nymph chart.yaml` writes
  `chart.png`; `nymph check chart.yaml` validates; `nymph schema
  PlotLine` lists fields; `nymph chart.png` prints the source back. Exit
  code 0 is success; on 1, one line on standard error says why.

## Workflow

1. Write the source (below). If you computed or fetched the data, record
   how in `source` (the prompt, query or code).
2. Render it. An error names `<line>:<column>` in the source and usually
   suggests the right name: fix that spot and render again.
3. Look at the image and improve it if it is unclear, then show it.
4. To change a nymph PNG, read its source back first (its `view` key
   keeps the zoom the user chose in the viewer: keep it unless asked).
5. Where the chart is shown live (the nymph viewer in the conversation),
   the user can pan, zoom, hide series and click points. You then get a
   note starting `nymph chart "...": the user changed the view.` with a
   `view:` block and the last clicked point. "This", "here" or "what I'm
   looking at" mean that view and point. When you render the chart
   again, put that `view:` block in the source (replacing any `view`
   key) so the user keeps their zoom; leave it out to show the whole
   chart again.

## Source

```yaml
description: Monthly revenue against cost, 2025.   # the alt text
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

- `root` is one element. One chart: a `Plot` (2D) or `Plot3D`. Several:
  a `VerticalLayout` / `HorizontalLayout` whose `children` are plots,
  `Label`s and separators. Diagrams: `ClassDiagram`, `SequenceDiagram`,
  `StateDiagram`, `ActivityDiagram`, `UseCaseDiagram`, `ERDiagram`,
  `NetworkDiagram`, `DataFlowDiagram`, `Flowchart`; charts:
  `GanttChart`, `SankeyDiagram`.
- 2D series go in a `Plot`'s `children`: `PlotLine`, `PlotScatter`,
  `PlotBars`, `PlotBarsH`, `PlotBarGroups`, `PlotShaded`, `PlotStairs`,
  `PlotHistogram`, `PlotHeatmap`, `PlotPieChart`, `PlotErrorBars`,
  `PlotInfLines`, `PlotAnnotation`, `PlotText`.
- `$name` takes a column from `data` (CSV in a `|` block, or
  `{name: [values]}`); `$table:name` from one of several named tables.
- Names are exact: an unknown type, field or flag is an error. Ask the
  schema when unsure.

Rules that cause most errors: indent with spaces; quote colours
(`color: "#C0392B"`, an unquoted `#` starts a comment); never quote
numbers; quote text containing `: `; CSV in `data: |`, never `>`.

## A good chart

Give it a title, axis labels with units, a label per series and a real
`description`. Category names go on ticks (`x_ticks: [0, 1, 2]`,
`x_tick_labels: [Q1, Q2, Q3]`); dates are UNIX seconds with `x_scale:
Time`. Start bar charts at 0 (`y_min: 0` with `y_max`). Keep pies round
with `flags: Equal`, `x_min: 0` and `x_max: 1` on the plot. Keep to about
five series per plot. Use `image: {scale: 2}` for images shown large.
Before a chart type you have not drawn yet, read its recipe in the full
guide: it shows the fields that make it look right.

The full guide, with recipes for every chart and diagram type and a
table of common errors, is `reference.md` next to this file (the MCP
resource `nymph://docs/agent-guide`); every field of every element is in
`nymph://docs/elements` or `nymph schema`.
