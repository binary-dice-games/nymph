# Chart themes

A theme gives a chart its look in one line, like a slide theme does for a
presentation: background, text colour and font, the colours series take in
turn, and the colours of diagram nodes and lines. A chart picks one with
`image.theme`:

```yaml
image: {width: 800, height: 500, theme: ocean}
```

A theme only fills in what the chart leaves out. Any value the chart
writes wins, so you can start from a theme and override single fields.

## Built-in themes

`nymph themes` lists them; `nymph themes <name>` prints one's file.

| Theme | Colours | Look |
|---|---|---|
| `light` (default) | light | Dark text on white, ImPlot's Deep series colours. |
| `dark` | dark | Light text on near-black, ImGui's dark colours. |
| `classic` | dark | ImGui's classic blue-grey colours. |
| `forest` | light | Pale sage paper, greens and earth tones, moss-green diagram lines. |
| `midnight` | dark | Ink-blue night, bright neon series, violet diagram lines. |
| `ocean` | dark | Deep navy, sea-green and coral series, soft blue diagram lines. |
| `paper` | light | Off-white page, serif type (PT Serif), muted ink colours. |
| `pastel` | light | Airy white, soft pastel series, periwinkle diagram lines. |
| `slate` | dark | Charcoal grey, cool blue and teal series, steel diagram lines. |
| `sunset` | light | Warm cream, orange, rose and plum series, terracotta diagram lines. |

Choosing one:

- **In the file:** `image.theme: <name>`.
- **Command line:** `nymph render chart.yaml --theme ocean` (the PNG's
  source gets `image.theme: ocean`; the input file is left as written),
  `nymph check --theme`, `nymph view --theme` (shows it, the file keeps
  its own).
- **Editor** (`nymph view --edit`, the web app's Edit): the panel's
  **Theme** button lists every theme with a swatch; a click writes
  `image.theme` (one undo step). The web app's **⋯ > Theme…** does the same
  in Edit, and previews a theme in View without changing the file.
- **Web pages:** `viewer.setTheme(name)` and `editor.setTheme(name)` show a
  theme without changing the source; `themes()` lists them with their
  swatch colours ([viewer.md](viewer.md)).

## Which value a field gets

For each field of each element, in order:

1. The value the element writes.
2. Nothing from the theme when an enclosing element (or `image`, for
   `font`, `font_size` and `text_color`) writes that field: the element
   inherits it as it always did. A `Flowchart` with `fill_color` fills its
   nodes whatever the theme says.
3. The theme's most specific rule for the field (below).
4. The field's default.

## Theme files

Built-in themes are YAML files in `resources/themes/`, compiled into nymph
like the fonts and shapes. A theme has five keys, all optional:

```yaml
name: Ocean                      # shown in pickers
description: Deep navy background, sea-green and coral series.
base: dark                       # light, dark or classic: the ImGui, ImPlot and ImUml colours under it
image:                           # image options the chart leaves out
  background: "#0f2233"
  text_color: "#dce6f0"
  font: FiraSans-Regular.ttf
  palette: ["#4cc9b0", "#ff8c69", "#7fb3ff", "#ffd166"]
fields:                          # field values, by rule
  figure:line_color: "#5fa8d3"
  figure:fill_color: "#16324a"
  Note:fill_color: "#24405a"
  plot2d.item:line_weight: 2
```

`image` takes `padding`, `font`, `font_size`, `text_color`, `background`
and `palette` (not the size). `background` also colours plot areas (a faint
step toward the text colour) and legends; `palette` is the list of colours
series take in turn when they set none, the plots' default colormap. Both
are also image options a chart can write itself.

### Rules

A key of `fields` is `[qualifier:]field`. The most specific rule that
covers the element wins:

| Key | Covers | Example |
|---|---|---|
| `Type:field` | elements of that type (any letter case) | `FlowNode:fill_color`, `plotline:line_weight` |
| `slot:field` | elements whose slot is `slot` or starts with `slot.`; a longer slot is more specific | `uml.flow.item:color` (flowchart items), `uml:color` (every diagram item), `plot2d.item:line_weight` (2D series), `figure:fill_color` (charts and diagrams themselves) |
| `field` | every element with that field | `rounding: 6` |

Slots are what `nymph schema` calls an element's place: `figure` (root and
layout children: plots, diagrams, shapes), `plot2d.item`, `plot3d.item`,
`uml.<diagram>.item`, `ui.widget`, ... A bare `font`, `font_size` or
`text_color` is the image's own (`image.text_color`), so every element
inherits it.

Diagram items take their diagram's `line_color` and `fill_color` when they
set none, so `figure:line_color` and `figure:fill_color` style every
diagram's nodes and edges while letting a diagram that sets its own pass it
on.

### Adding a theme

Add `resources/themes/<name>.yaml` (lower-case name: letters, digits, `_`)
and rebuild; the theme test checks every rule against the element types
(a qualifier that matches nothing, a field no covered type has, or a value
that does not fit fails the build's tests).
