# GanttChart (nymph element reference)

Part of the [nymph element reference](../elements.md), which has the
conventions these tables follow.

## GanttChart

A Gantt chart: tasks as bars on a time axis, one row each, top to bottom, in sections with a colour each. Children: GanttSection, GanttTask.

Goes in: root, Child, CollapsingHeader, Disabled, Group, HorizontalLayout, Indent, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, VerticalLayout, Window. Children: GanttTask, GanttSection.

| Field | Type | Default | Description |
|---|---|---|---|
| `title` | text | `""` | Title above the chart; empty for none. |
| `width` | number | `0` | Width in pixels; 0 (default) is 600 in a HorizontalLayout and fills elsewhere. In a HorizontalLayout a negative value is a stretch weight. (-1000 to 8192) |
| `height` | number | `0` | Height in pixels; 0 (default) is the natural height (enough for every row) in a VerticalLayout, and fills elsewhere. In a VerticalLayout a negative value is a stretch weight. (-1000 to 8192) |
| `x_label` | text | `""` | Label under the time axis; empty for none. |
| `today` | text | `""` | Draws a red line marked "today" at this time: a date (YYYY-MM-DD, or YYYY-MM-DD HH:MM in UTC) or a number, like the tasks'. Empty for none. |
| `colormap` | name | `Default` | Colormap for this plot: the colours series take in turn when they set none, and the scale of heatmaps and surfaces. Deep (the usual default), Dark, Pastel, Paired, Viridis, Plasma, Hot, Cool, Pink, Jet, Twilight, RdBu, BrBG, PiYG, Spectral or Greys. Default leaves the current colormap. (one of: Default, Deep, Dark, Pastel, Paired, Viridis, Plasma, Hot, Cool, Pink, Jet, Twilight, RdBu, BrBG, PiYG, Spectral, Greys) |
| `bar_height` | number | `0.6` | Height of a bar as a fraction of its row. (0.1 to 1) |
| `show_dependencies` | true/false | `true` | When true, draws an arrow from each task in after. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## GanttSection

A group of tasks (a phase, a team) sharing a colour and a legend entry. Children: GanttTask.

Goes in: GanttChart. Children: GanttTask.

| Field | Type | Default | Description |
|---|---|---|---|
| `name` | text | `""` | Section name, shown in the legend. |
| `color` | colour | `""` | Colour of its tasks; empty takes the next colour of the chart's colormap. |

## GanttTask

A task (a bar from start to end) or a milestone of a GanttChart, on its own row.

Goes in: GanttChart, GanttSection.

| Field | Type | Default | Description |
|---|---|---|---|
| `id` | text | `""` | Id that other tasks' after names; optional unless named. |
| `name` | text | `""` | Name shown on the task's row; empty shows the id. |
| `start` | text | `""` | When the task starts: a date (YYYY-MM-DD, or YYYY-MM-DD HH:MM in UTC) or a number. Leave empty and give after instead. |
| `end` | text | `""` | When the task ends, in the same form as start (a date means its midnight: 2026-03-02 to 2026-03-05 is three days). Leave empty and give duration instead. |
| `duration` | number | `0` | Length of the task instead of end: days when start is a date, else start's units. (0 to 1e+09) |
| `after` | list of texts | `[]` | Ids of the tasks this one waits for: it starts when the last of them ends, with an arrow from each (mermaid's after). |
| `milestone` | true/false | `false` | When true, the task is a moment, drawn as a diamond at its start. |
| `status` | name | `None` | Done (pale), Active (outlined), Critical (red outline) or None. (one of: None, Done, Active, Critical) |
| `color` | colour | `""` | Bar colour; empty uses its section's colour. |
