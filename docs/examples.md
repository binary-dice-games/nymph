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

A 4 x 5 grid built from five CSV columns with `[$mon, $tue, ...]`, on the `Viridis` scale.

![Heatmap](../examples/heatmap.png)

[`examples/heatmap.yaml`](../examples/heatmap.yaml):

```yaml
nymph: 1
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
      label: activity
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
