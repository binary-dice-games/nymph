# nymph — Examples

Each example is a source in [`examples/`](../examples) with the PNG nymph
renders from it. To render one:

```sh
nymph examples/line.yaml          # writes examples/line.png
```

Every PNG carries its source: `nymph examples/line.png` prints
`examples/line.yaml` back byte for byte (a test checks this for all of
them). See [format.md](format.md) for the format and
[agent-guide.md](agent-guide.md) for recipes.

## Line chart

Two series with fixed colours, a thicker line and markers, a fixed 0-100 Y range.

![Line chart](../examples/line.png)

[`examples/line.yaml`](../examples/line.yaml):

```yaml
nymph: 1
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
    - type: PlotLine
      label: cpu
      xs: $minute
      ys: $cpu
      color: "#C0392B"
      line_weight: 2
      marker: Circle
    - type: PlotLine
      label: memory
      xs: $minute
      ys: $mem
      color: "#2471A3"
      line_weight: 2
data: |
  minute, cpu, mem
  0, 12, 40
  1, 35, 42
  2, 71, 47
  3, 88, 55
  4, 64, 58
  5, 30, 57
```

## Bar chart

Bars with an `x_min`/`x_max` half a step beyond the outer bars, and a horizontal target line (`PlotInfLines`).

![Bar chart](../examples/bars.png)

[`examples/bars.yaml`](../examples/bars.yaml):

```yaml
nymph: 1
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
    - type: PlotBars
      label: closed
      xs: $team
      ys: $closed
      bar_size: 0.6
    - type: PlotInfLines
      label: target
      values: [30]
      horizontal: true
      color: "#C0392B"
      line_weight: 2
data: |
  team, closed
  1, 31
  2, 24
  3, 37
  4, 18
```

## Grouped bars

Two series side by side per quarter with `PlotBarGroups`: `values: [$north, $south]` gives one row per group; the Plot's `x_ticks` and `x_tick_labels` (here a text column) name the groups, and `y_format` adds the unit.

![Grouped bars](../examples/bar_groups.png)

[`examples/bar_groups.yaml`](../examples/bar_groups.yaml):

```yaml
nymph: 1
description: Revenue per quarter in two regions, in millions.
image: {width: 640, height: 400}
root:
  type: Plot
  title: Revenue by quarter
  y_label: revenue
  y_format: "%.0fM"
  x_ticks: [0, 1, 2, 3]
  x_tick_labels: $quarter
  y_min: 0
  y_max: 30
  legend_location: NorthWest
  children:
    - type: PlotBarGroups
      labels: [north, south]
      values: [$north, $south]
data: |
  quarter, north, south
  Q1, 12, 9
  Q2, 18, 14
  Q3, 15, 17
  Q4, 22, 19
```

## Error bars, annotations and tags

Asymmetric error bars (`neg`, `pos`) that take the colour of the line before them, a callout on the outlier with `PlotAnnotation`, and a `PlotTag` naming the budget line on the Y axis.

![Error bars, annotations and tags](../examples/error_bars.png)

[`examples/error_bars.yaml`](../examples/error_bars.yaml):

```yaml
nymph: 1
description: Median request latency per build, with the 10th to 90th percentile spread, against a 40 ms budget.
image: {width: 640, height: 400}
root:
  type: Plot
  title: Latency per build
  x_label: build
  y_label: latency
  y_format: "%.0f ms"
  x_ticks: [1, 2, 3, 4, 5, 6]
  y_min: 0
  y_max: 60
  children:
    - type: PlotLine
      label: median
      xs: $build
      ys: $median
      marker: Circle
      line_weight: 2
    - type: PlotErrorBars
      xs: $build
      ys: $median
      neg: $below
      pos: $above
    - type: PlotAnnotation
      x: 4
      y: 34
      text: cache disabled
    - type: PlotInfLines
      label: budget
      values: [40]
      horizontal: true
      color: "#C0392B"
      line_weight: 2
    - type: PlotTag
      value: 40
      text: budget
data: |
  build, median, below, above
  1, 21, 4, 9
  2, 22, 5, 8
  3, 20, 4, 10
  4, 34, 7, 15
  5, 23, 5, 9
  6, 19, 3, 7
```

## Time and log axes, second Y axis

`x_scale: Time` reads UNIX seconds and labels them in UTC; `y_scale: Log10` fits values from 60 to 21000; the error rate uses `y_axis: Y2`, the axis on the right, with its own range and format.

![Time and log axes, second Y axis](../examples/time_axes.png)

[`examples/time_axes.yaml`](../examples/time_axes.yaml):

```yaml
nymph: 1
description: Requests per hour (log scale, left) and error rate (right) over one day, times in UTC.
image: {width: 720, height: 400}
root:
  type: Plot
  title: Traffic on 2026-03-02
  x_scale: Time
  y_scale: Log10
  y_label: requests per hour
  y2_label: error rate
  y2_format: "%.1f%%"
  y2_min: 0
  y2_max: 5
  children:
    - type: PlotLine
      label: requests
      xs: $time
      ys: $requests
      line_weight: 2
    - type: PlotLine
      label: errors
      xs: $time
      ys: $errors
      y_axis: Y2
      marker: Circle
data: |
  time, requests, errors
  1772409600, 120, 0.4
  1772416800, 60, 0.3
  1772424000, 900, 0.6
  1772431200, 8500, 1.1
  1772438400, 21000, 1.8
  1772445600, 18000, 3.9
  1772452800, 9000, 1.2
  1772460000, 2500, 0.7
  1772467200, 400, 0.5
```

## Histogram

Raw samples binned by nymph; the data is given as a column list instead of CSV.

![Histogram](../examples/histogram.png)

[`examples/histogram.yaml`](../examples/histogram.yaml):

```yaml
nymph: 1
description: Distribution of 40 API response times, in milliseconds.
image: {width: 640, height: 400}
root:
  type: Plot
  title: Response time
  x_label: ms
  y_label: requests
  children:
    - type: PlotHistogram
      label: latency
      values: $ms
      bins: 8
data:
  ms: [118, 131, 96, 143, 110, 125, 88, 138, 121, 114,
       102, 127, 119, 133, 109, 98, 122, 117, 141, 129,
       112, 105, 124, 136, 115, 120, 93, 126, 111, 130,
       107, 123, 116, 150, 101, 128, 113, 134, 119, 104]
```

## Heatmap

A 4 x 5 grid built from five CSV columns with `[$mon, $tue, ...]`, on the `Viridis` scale. `x_ticks` / `y_ticks` with tick labels name the cells (the heatmap spans 0 to 1, so the cell centres are at 0.1, 0.3, ...), and a `ColorScale` beside the plot shows what the colours mean.

![Heatmap](../examples/heatmap.png)

[`examples/heatmap.yaml`](../examples/heatmap.yaml):

```yaml
nymph: 1
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
          label: activity
          rows: 4
          cols: 5
          values: [$mon, $tue, $wed, $thu, $fri]
          scale_min: 0
          scale_max: 10
          format: "%.0f"
    - type: ColorScale
      label: events
      height: -1
      scale_min: 0
      scale_max: 10
      colormap: Viridis
data: |
  slot, mon, tue, wed, thu, fri
  morning, 2, 4, 6, 3, 1
  noon, 5, 7, 9, 6, 4
  evening, 8, 9, 10, 7, 6
  night, 1, 2, 3, 2, 5
```

## Pie chart

Kept round by `Equal` and a fixed X range; legend outside on the right.

![Pie chart](../examples/pie.png)

[`examples/pie.yaml`](../examples/pie.yaml):

```yaml
nymph: 1
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
    - type: PlotPieChart
      labels: $browser
      values: $share
      normalize: true
data: |
  browser, share
  Chrome, 63
  Firefox, 6
  Safari, 20
  Other, 11
```

## Scatter

Two groups with different markers and a fixed range so no point sits on the edge.

![Scatter](../examples/scatter.png)

[`examples/scatter.yaml`](../examples/scatter.yaml):

```yaml
nymph: 1
description: Height against weight for two groups of 8 people.
image: {width: 640, height: 440}
root:
  type: Plot
  title: Height and weight
  x_label: height (cm)
  y_label: weight (kg)
  legend_location: SouthEast
  x_min: 155
  x_max: 195
  y_min: 50
  y_max: 95
  children:
    - type: PlotScatter
      label: group A
      xs: $height_a
      ys: $weight_a
      marker: Circle
      marker_size: 5
    - type: PlotScatter
      label: group B
      xs: $height_b
      ys: $weight_b
      marker: Square
      marker_size: 5
data: |
  height_a, weight_a, height_b, weight_b
  158, 54, 166, 63
  162, 57, 171, 68
  165, 61, 174, 72
  168, 60, 177, 75
  171, 66, 180, 79
  174, 69, 183, 82
  177, 71, 186, 88
  181, 76, 190, 91
```

## 3D surface

A 9 x 9 grid listed row by row, coloured by `Viridis`.

![3D surface](../examples/surface.png)

[`examples/surface.yaml`](../examples/surface.yaml):

```yaml
nymph: 1
description: A bump, z = exp(-(x^2 + y^2) / 2) * cos(x), on a 9 by 9 grid.
image: {width: 640, height: 520}
root:
  type: Plot3D
  title: Surface
  x_label: x
  y_label: y
  z_label: z
  colormap: Viridis
  flags: NoLegend
  children:
    - type: Plot3DSurface
      label: z
      xs: $x
      ys: $y
      zs: $z
      x_count: 9
      y_count: 9
data: |
  x, y, z
  -2, -2, -0.01
  -1.5, -2, 0.00
  -1, -2, 0.04
  -0.5, -2, 0.10
  0, -2, 0.14
  0.5, -2, 0.10
  1, -2, 0.04
  1.5, -2, 0.00
  2, -2, -0.01
  -2, -1.5, -0.02
  -1.5, -1.5, 0.01
  -1, -1.5, 0.11
  -0.5, -1.5, 0.25
  0, -1.5, 0.32
  0.5, -1.5, 0.25
  1, -1.5, 0.11
  1.5, -1.5, 0.01
  2, -1.5, -0.02
  -2, -1, -0.03
  -1.5, -1, 0.01
  -1, -1, 0.20
  -0.5, -1, 0.47
  0, -1, 0.61
  0.5, -1, 0.47
  1, -1, 0.20
  1.5, -1, 0.01
  2, -1, -0.03
  -2, -0.5, -0.05
  -1.5, -0.5, 0.02
  -1, -0.5, 0.29
  -0.5, -0.5, 0.68
  0, -0.5, 0.88
  0.5, -0.5, 0.68
  1, -0.5, 0.29
  1.5, -0.5, 0.02
  2, -0.5, -0.05
  -2, 0, -0.06
  -1.5, 0, 0.02
  -1, 0, 0.33
  -0.5, 0, 0.77
  0, 0, 1.00
  0.5, 0, 0.77
  1, 0, 0.33
  1.5, 0, 0.02
  2, 0, -0.06
  -2, 0.5, -0.05
  -1.5, 0.5, 0.02
  -1, 0.5, 0.29
  -0.5, 0.5, 0.68
  0, 0.5, 0.88
  0.5, 0.5, 0.68
  1, 0.5, 0.29
  1.5, 0.5, 0.02
  2, 0.5, -0.05
  -2, 1, -0.03
  -1.5, 1, 0.01
  -1, 1, 0.20
  -0.5, 1, 0.47
  0, 1, 0.61
  0.5, 1, 0.47
  1, 1, 0.20
  1.5, 1, 0.01
  2, 1, -0.03
  -2, 1.5, -0.02
  -1.5, 1.5, 0.01
  -1, 1.5, 0.11
  -0.5, 1.5, 0.25
  0, 1.5, 0.32
  0.5, 1.5, 0.25
  1, 1.5, 0.11
  1.5, 1.5, 0.01
  2, 1.5, -0.02
  -2, 2, -0.01
  -1.5, 2, 0.00
  -1, 2, 0.04
  -0.5, 2, 0.10
  0, 2, 0.14
  0.5, 2, 0.10
  1, 2, 0.04
  1.5, 2, 0.00
  2, 2, -0.01
```

## Two named tables

A monthly series and a per-region summary of different lengths, as two named tables under `data` referenced with `$table:column`; `source` records the prompt and code that produced the numbers.

![Two named tables](../examples/tables.png)

[`examples/tables.yaml`](../examples/tables.yaml):

```yaml
nymph: 1
description: >-
  Monthly revenue in 2025 next to each region's share of the year's total,
  from two tables of different lengths.
source:
  prompt: Chart 2025 revenue by month, and each region's share of the total.
  code: |
    import pandas as pd
    df = pd.read_csv("orders_2025.csv", parse_dates=["date"])
    monthly = df.groupby(df.date.dt.month).revenue.sum() / 1000
    regions = df.groupby("region").revenue.sum()
    share = (100 * regions / regions.sum()).round()
  language: python
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
      x_max: 12.5
      y_min: 0
      y_max: 160
      flags: NoLegend
      children:
        - {type: PlotBars, label: revenue, xs: $monthly:month, ys: $monthly:revenue, bar_size: 0.7}
    - type: Plot
      title: Share by region
      width: -1
      height: -1
      flags: Equal
      legend_location: South
      legend_flags: Horizontal
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
    5, 118
    6, 126
    7, 121
    8, 115
    9, 130
    10, 138
    11, 149
    12, 155
  regions:
    name: [north, south, east, west]
    share: [34, 21, 28, 17]
```

## Fonts and text colour

Embedded fonts by file name: Fira Sans for the whole image, a PT Serif heading, an italic note, Fira Mono and a blue text colour on the cost chart, a bold red `PlotText`.

![Fonts and text colour](../examples/fonts.png)

[`examples/fonts.yaml`](../examples/fonts.yaml):

```yaml
nymph: 1
description: >-
  Quarterly revenue and costs in 2025, styled with embedded fonts: a serif
  heading, an italic note, and monospaced numbers in the cost chart.
image: {width: 760, height: 460, font: FiraSans-Regular.ttf, font_size: 15}
root:
  type: VerticalLayout
  spacing: 6
  children:
    - {type: Label, text: Quarterly report 2025, font: PT_Serif-Web-Bold.ttf, font_size: 28, text_color: "#1F3A5F"}
    - type: Label
      text: "All figures in kUSD. Source: finance export, 3 January 2026."
      font: FiraSans-Italic.ttf
      text_color_light: "#666666"
      text_color_dark: "#AAAAAA"
    - type: HorizontalLayout
      spacing: 8
      height: -1
      children:
        - type: Plot
          title: Revenue
          width: -1
          height: -1
          x_label: quarter
          x_min: 0.5
          x_max: 4.5
          y_min: 0
          y_max: 200
          flags: NoLegend
          children:
            - {type: PlotLine, label: revenue, xs: $quarter, ys: $revenue, line_weight: 2, marker: Circle}
            - {type: PlotText, text: record, x: 4, y: 182, font: FiraSans-Bold.ttf, text_color: "#C0392B"}
        - type: Plot
          title: Costs
          width: -1
          height: -1
          x_label: quarter
          x_min: 0.5
          x_max: 4.5
          y_min: 0
          y_max: 200
          flags: NoLegend
          font: FiraMono-Regular.ttf
          text_color: "#2471A3"
          children:
            - {type: PlotBars, label: costs, xs: $quarter, ys: $cost, bar_size: 0.6}
    - {type: SeparatorText, label: Fonts, font: FiraSans-Bold.ttf, font_size: 13}
    - {type: Label, text: "PT Serif Bold, Fira Sans (regular, italic, bold) and Fira Mono, embedded in nymph.", font_size: 13}
data: |
  quarter, revenue, cost
  1, 121, 96
  2, 138, 101
  3, 152, 108
  4, 171, 112
```

## Several plots with labels

A heading, a separator, two stacked plots next to a pie, and a source note: nested layouts with stretch sizes (`height: -1`, `width: -2`).

![Several plots with labels](../examples/dashboard.png)

[`examples/dashboard.yaml`](../examples/dashboard.yaml):

```yaml
nymph: 1
description: >-
  Greenhouse sensors on 12 March: temperature and humidity through the day,
  next to the share of time each vent was open.
image: {width: 900, height: 560}
root:
  type: VerticalLayout
  spacing: 6
  children:
    - type: Label
      text: Greenhouse sensors, 12 March
    - type: Separator
    - type: HorizontalLayout
      spacing: 8
      height: -1
      children:
        - type: VerticalLayout
          spacing: 6
          width: -2
          children:
            - type: Plot
              title: Temperature
              height: -1
              y_label: C
              flags: NoLegend
              children:
                - {type: PlotLine, label: temp, xs: $hour, ys: $temp, color: "#C0392B", line_weight: 2}
            - type: Plot
              title: Humidity
              height: -1
              x_label: hour
              y_label: "%"
              flags: NoLegend
              children:
                - {type: PlotShaded, label: humidity, xs: $hour, ys: $hum, color: "#2471A3", fill_alpha: 0.25}
                - {type: PlotLine, label: humidity, xs: $hour, ys: $hum, color: "#2471A3", line_weight: 2}
        - type: Plot
          title: Vent open time
          width: -1
          height: -1
          flags: Equal
          legend_location: South
          legend_flags: Horizontal
          x_flags: NoDecorations
          y_flags: NoDecorations
          x_min: 0
          x_max: 1
          children:
            - {type: PlotPieChart, labels: "north\nsouth\nroof", values: [45, 35, 20], label_fmt: "%.0f%%"}
    - type: Label
      text: "Source: station 14. Readings every 2 hours."
      text_color_light: "#666666"
      text_color_dark: "#AAAAAA"
data: |
  hour, temp, hum
  0, 12.8, 73
  2, 12.1, 75
  4, 11.6, 77
  6, 14.9, 66
  8, 18.4, 58
  10, 21.7, 50
  12, 23.2, 47
  14, 24.0, 45
  16, 22.8, 48
  18, 21.0, 52
  20, 17.3, 61
  22, 14.6, 68
```

## UML class diagram

Classes with attribute and method compartments, an abstract class (italic, from the embedded italic font), inheritance, an aggregation with multiplicities, and a note. Laid out automatically, parents above.

![UML class diagram](../examples/uml_class.png)

[`examples/uml_class.yaml`](../examples/uml_class.yaml):

```yaml
nymph: 1
description: UML class diagram of animals, an abstract Animal and three subclasses, and a Pond aggregating ducks.
image: {width: 760, height: 520, font: FiraSans-Regular.ttf, font_size: 15}
root:
  type: ClassDiagram
  title: Animals
  children:
    - type: Class
      id: Animal
      kind: Abstract
      attributes: ["+name: string", "+age: int"]
      methods: ["+makeSound()*", "+isMammal() bool"]
    - {type: Class, id: Duck, attributes: ["+beakColor: string"], methods: ["+swim()", "+quack()"]}
    - {type: Class, id: Fish, attributes: ["-sizeInFeet: int"], methods: ["-canEat()"]}
    - {type: Class, id: Zebra, attributes: ["+is_wild: bool"], methods: ["+run()", "+count()$"]}
    - {type: Class, id: Pond}
    - {type: ClassRelation, from: Duck, to: Animal, kind: Inheritance}
    - {type: ClassRelation, from: Fish, to: Animal, kind: Inheritance}
    - {type: ClassRelation, from: Zebra, to: Animal, kind: Inheritance}
    - {type: ClassRelation, from: Pond, to: Duck, kind: Aggregation, from_label: "1", to_label: "*", label: has}
    - {type: Note, text: "Ducks can fly too", attach: [Duck]}
```

## UML sequence diagram

Participants (an actor, a box, a database), numbered messages, activation bars, an `alt` fragment with an `Else`, a self message and a note over two lifelines.

![UML sequence diagram](../examples/uml_sequence.png)

[`examples/uml_sequence.yaml`](../examples/uml_sequence.yaml):

```yaml
nymph: 1
description: UML sequence diagram of a login between a user, web app and database, with an alt fragment for a wrong password.
image: {width: 720, height: 560, font: FiraSans-Regular.ttf, font_size: 15}
root:
  type: SequenceDiagram
  autonumber: true
  children:
    - {type: Participant, id: user, name: User, kind: Actor}
    - {type: Participant, id: web, name: Web app}
    - {type: Participant, id: db, name: Database, kind: Database}
    - {type: Message, from: user, to: web, text: "POST /login", activate: true}
    - type: Fragment
      kind: Alt
      label: valid password
      children:
        - {type: Message, from: web, to: db, text: load user, activate: true}
        - {type: Message, from: db, to: web, text: row, kind: Reply, deactivate: true}
        - {type: Else, label: otherwise}
        - {type: Message, from: web, to: web, text: log failure}
    - {type: Message, from: web, to: user, text: "200 OK", kind: Reply, deactivate: true}
    - {type: Note, text: session cookie set, over: [user, web]}
```

## UML state diagram

Initial and final pseudo-states, a composite state laid out inside its parent, and a note attached to an inner state.

![UML state diagram](../examples/uml_state.png)

[`examples/uml_state.yaml`](../examples/uml_state.yaml):

```yaml
nymph: 1
description: UML state diagram of a media player, with a composite Active state.
image: {width: 640, height: 600, font: FiraSans-Regular.ttf, font_size: 15}
root:
  type: StateDiagram
  title: Media player
  children:
    - {type: State, id: start, kind: Initial}
    - {type: State, id: stopped, name: Stopped}
    - type: State
      id: active
      name: Active
      children:
        - {type: State, id: a0, kind: Initial}
        - {type: State, id: playing, name: Playing}
        - {type: State, id: paused, name: Paused}
        - {type: Transition, from: a0, to: playing}
        - {type: Transition, from: playing, to: paused, label: pause}
        - {type: Transition, from: paused, to: playing, label: play}
    - {type: State, id: end, kind: Final}
    - {type: Transition, from: start, to: stopped}
    - {type: Transition, from: stopped, to: playing, label: play}
    - {type: Transition, from: paused, to: stopped, label: stop}
    - {type: Transition, from: stopped, to: end, label: eject}
    - {type: Note, text: "remembers the position", attach: [paused]}
```

## UML activity diagram

An initial node, actions (one a send signal), a decision with guards, a fork and join, a merge and a final node.

![UML activity diagram](../examples/uml_activity.png)

[`examples/uml_activity.yaml`](../examples/uml_activity.yaml):

```yaml
nymph: 1
description: UML activity diagram of order handling, with a decision, a fork and a join.
image: {width: 560, height: 680, font: FiraSans-Regular.ttf, font_size: 15}
root:
  type: ActivityDiagram
  title: Order handling
  children:
    - {type: ActivityControl, id: start, kind: Initial}
    - {type: Action, id: receive, text: Receive order}
    - {type: ActivityControl, id: stock, kind: Decision, label: in stock?}
    - {type: Action, id: ship, text: Ship}
    - {type: Action, id: bill, text: Send invoice, kind: Send}
    - {type: Action, id: cancel, text: Cancel order}
    - {type: ActivityControl, id: fork, kind: Fork}
    - {type: ActivityControl, id: join, kind: Join}
    - {type: ActivityControl, id: merge, kind: Merge}
    - {type: ActivityControl, id: end, kind: Final}
    - {type: Flow, from: start, to: receive}
    - {type: Flow, from: receive, to: stock}
    - {type: Flow, from: stock, to: fork, guard: yes}
    - {type: Flow, from: stock, to: cancel, guard: no}
    - {type: Flow, from: fork, to: ship}
    - {type: Flow, from: fork, to: bill}
    - {type: Flow, from: ship, to: join}
    - {type: Flow, from: bill, to: join}
    - {type: Flow, from: join, to: merge}
    - {type: Flow, from: cancel, to: merge}
    - {type: Flow, from: merge, to: end}
```

## UML use case diagram

Actors on both sides of a system boundary, associations, and dashed `«include»` / `«extend»` relations.

![UML use case diagram](../examples/uml_usecase.png)

[`examples/uml_usecase.yaml`](../examples/uml_usecase.yaml):

```yaml
nymph: 1
description: UML use case diagram of an online shop, with a customer and a bank around the shop's use cases.
image: {width: 760, height: 420, font: FiraSans-Regular.ttf, font_size: 15}
root:
  type: UseCaseDiagram
  system: Online shop
  children:
    - {type: Actor, id: customer, name: Customer}
    - {type: Actor, id: bank, name: Bank, side: Right}
    - {type: UseCase, id: browse, name: Browse catalog}
    - {type: UseCase, id: buy, name: Buy item}
    - {type: UseCase, id: pay, name: Pay}
    - {type: UseCase, id: coupon, name: Apply coupon}
    - {type: UseCaseRelation, from: customer, to: browse}
    - {type: UseCaseRelation, from: customer, to: buy}
    - {type: UseCaseRelation, from: buy, to: pay, kind: Include}
    - {type: UseCaseRelation, from: coupon, to: pay, kind: Extend}
    - {type: UseCaseRelation, from: pay, to: bank}
```

## Flowchart

Every node shape, a `FlowGroup` around the deploy steps that links end at, labelled decision branches, a thick, a dotted, a plain and a two-headed link, and a note.

![Flowchart](../examples/flowchart.png)

[`examples/flowchart.yaml`](../examples/flowchart.yaml):

```yaml
nymph: 1
description: How a push to main reaches production, with the deploy steps grouped.
image: {width: 720, height: 560}
root:
  type: Flowchart
  title: Deploy pipeline
  children:
    - {type: FlowNode, id: start, text: Push to main, shape: Stadium}
    - {type: FlowNode, id: build, text: Build}
    - {type: FlowNode, id: tests, text: Tests pass?, shape: Diamond}
    - {type: FlowNode, id: fix, text: "Fix and\npush again", shape: Rounded}
    - type: FlowGroup
      id: prod
      title: Production
      children:
        - {type: FlowNode, id: deploy, text: Deploy, shape: Subroutine}
        - {type: FlowNode, id: db, text: Migrate DB, shape: Cylinder}
        - {type: FlowLink, from: deploy, to: db, line: Dotted}
    - {type: FlowNode, id: notify, text: Notify team, shape: Parallelogram}
    - {type: FlowNode, id: prep, text: Prepare, shape: Hexagon}
    - {type: FlowNode, id: done, text: Done, shape: Circle}
    - {type: FlowLink, from: start, to: build, line: Thick}
    - {type: FlowLink, from: build, to: tests}
    - {type: FlowLink, from: tests, to: prep, label: "yes"}
    - {type: FlowLink, from: prep, to: prod}
    - {type: FlowLink, from: tests, to: fix, label: "no"}
    - {type: FlowLink, from: fix, to: build, arrow: None}
    - {type: FlowLink, from: prod, to: notify, arrow: Both}
    - {type: FlowLink, from: notify, to: done}
    - {type: Note, text: runs on every push, attach: [build]}
```

## Gantt chart

Three sections with their own colours, tasks placed by `after` (drawn as arrows), `Done` / `Active` / `Critical` statuses, two milestones and a `today` line.

![Gantt chart](../examples/gantt.png)

[`examples/gantt.yaml`](../examples/gantt.yaml):

```yaml
nymph: 1
description: Release 2.0 schedule in three phases, with dependencies, a code-freeze milestone and today marked.
image: {width: 760, height: 420}
root:
  type: GanttChart
  title: Release 2.0
  today: 2026-03-12
  children:
    - type: GanttSection
      name: Design
      children:
        - {type: GanttTask, id: spec, name: Write spec, start: 2026-03-02, duration: 4, status: Done}
        - {type: GanttTask, id: review, name: Review, after: [spec], duration: 2, status: Done}
    - type: GanttSection
      name: Build
      children:
        - {type: GanttTask, id: api, name: API, after: [review], duration: 6, status: Active}
        - {type: GanttTask, id: ui, name: UI, after: [review], duration: 8, status: Critical}
        - {type: GanttTask, id: freeze, name: Code freeze, after: [api, ui], milestone: true}
    - type: GanttSection
      name: Ship
      children:
        - {type: GanttTask, id: qa, name: QA, after: [freeze], end: 2026-03-27}
        - {type: GanttTask, name: Release, after: [qa], milestone: true}
```

## Sankey diagram

Flows from a CSV table mapped on with `from: $from`, `to: $to`, `values: $twh`; nodes are placed in columns automatically, and a `SankeyNode` colours the losses grey.

![Sankey diagram](../examples/sankey.png)

[`examples/sankey.yaml`](../examples/sankey.yaml):

```yaml
nymph: 1
description: Where the energy of a small grid comes from and where it goes, in TWh per year.
image: {width: 760, height: 460}
root:
  type: SankeyDiagram
  title: Energy flow (TWh)
  from: $from
  to: $to
  values: $twh
  value_format: "%.0f"
  children:
    - {type: SankeyNode, id: Losses, color: "#4D4D4D"}
data: |
  from, to, twh
  Coal, Electricity, 120
  Gas, Electricity, 90
  Gas, Heating, 60
  Solar, Electricity, 40
  Wind, Electricity, 55
  Electricity, Homes, 110
  Electricity, Industry, 95
  Electricity, Losses, 100
  Heating, Homes, 45
  Heating, Losses, 15
```
