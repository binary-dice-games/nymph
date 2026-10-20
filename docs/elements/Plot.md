# Plot (nymph element reference)

Part of the [nymph element reference](../elements.md), which has the
conventions these tables follow.

## Plot

A 2D plot (ImPlot). Children are 2D series: PlotLine, PlotBars, PlotScatter, ...

Goes in: root, Child, CollapsingHeader, Disabled, Group, HorizontalLayout, Indent, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, VerticalLayout, Window. Children: PlotAnnotation, PlotBarGroups, PlotBars, PlotBarsH, PlotDigital, PlotErrorBars, PlotHeatmap, PlotHistogram, PlotHistogram2D, PlotInfLines, PlotLine, PlotPieChart, PlotScatter, PlotShaded, PlotStairs, PlotStems, PlotTag, PlotText.

| Field | Type | Default | Description |
|---|---|---|---|
| `title` | text | `"##plot"` | Plot title shown at the top of the frame; prefix ## to make the ID hidden (no visible title). |
| `x_label` | text | `""` | X axis label; empty string = no label. |
| `y_label` | text | `""` | Y axis label; empty string = no label. |
| `width` | number | `-1` | Plot width in pixels; -1 (default) fills the available width. In a HorizontalLayout a negative value is a stretch weight (-1 and -3 split the space left 1:3) and 0 the natural width. (-1000 to 8192) |
| `height` | number | `300` | Plot height in pixels. A Plot used as root with no height fills the image. In a VerticalLayout a negative value is a stretch weight (-1 and -3 split the space left 1:3) and 0 the natural height (300). (-1000 to 8192) |
| `flags` | flags | `None` | ImPlotFlags bitmask (combine names with '\|'; see implot.h). (combine with '\|': None, NoTitle, NoLegend, NoMouseText, NoInputs, NoMenus, NoBoxSelect, NoFrame, Equal, Crosshairs, CanvasOnly) |
| `x_flags` | flags | `None` | ImPlotAxisFlags for the primary X axis (combine names with '\|'). Use NoDecorations for pie charts. (combine with '\|': None, NoLabel, NoGridLines, NoTickMarks, NoTickLabels, NoInitialFit, NoMenus, NoSideSwitch, NoHighlight, Opposite, Foreground, Invert, AutoFit, RangeFit, PanStretch, LockMin, LockMax, Lock, NoDecorations) |
| `y_flags` | flags | `None` | ImPlotAxisFlags for the primary Y axis (combine names with '\|'). Use NoDecorations for pie charts. (combine with '\|': None, NoLabel, NoGridLines, NoTickMarks, NoTickLabels, NoInitialFit, NoMenus, NoSideSwitch, NoHighlight, Opposite, Foreground, Invert, AutoFit, RangeFit, PanStretch, LockMin, LockMax, Lock, NoDecorations) |
| `x_min` | number | `0` | Fixed lower X-axis limit, locked every frame.  Ignored (axis auto-fits normally) when equal to x_max. |
| `x_max` | number | `0` | Fixed upper X-axis limit, locked every frame.  Ignored (axis auto-fits normally) when equal to x_min. |
| `y_min` | number | `0` | Fixed lower Y-axis limit, locked every frame.  Ignored (axis auto-fits normally) when equal to y_max. |
| `y_max` | number | `0` | Fixed upper Y-axis limit, locked every frame.  Ignored (axis auto-fits normally) when equal to y_min. |
| `legend_location` | flags | `-1` | Where the legend sits (ImPlotLocation: North/South/West/East and the corner combinations, or Center). -1 (default) keeps ImPlot's own default of top-left inside the plot. Combine with legend_flags Outside to move the legend out of the plot area entirely. (combine with '\|': Center, North, South, West, East, NorthWest, NorthEast, SouthWest, SouthEast) |
| `legend_flags` | flags | `None` | ImPlotLegendFlags bitmask (combine names with '\|'): Outside renders the legend outside the plot frame, Horizontal lays entries in a row, plus NoButtons / NoMenus / Sort / Reverse. (combine with '\|': None, NoButtons, NoHighlightItem, NoHighlightAxis, NoMenus, Outside, Horizontal, Sort, Reverse) |
| `colormap` | name | `Default` | Colormap for this plot: the colours series take in turn when they set none, and the scale of heatmaps and surfaces. Deep (the usual default), Dark, Pastel, Paired, Viridis, Plasma, Hot, Cool, Pink, Jet, Twilight, RdBu, BrBG, PiYG, Spectral or Greys. Default leaves the current colormap. (one of: Default, Deep, Dark, Pastel, Paired, Viridis, Plasma, Hot, Cool, Pink, Jet, Twilight, RdBu, BrBG, PiYG, Spectral, Greys) |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |
| `x_scale` | name | `Linear` | Scale of the X axis: Linear (default); Log10, powers of ten, for values spanning several orders of magnitude (fixed limits must be above 0); SymLog, log-like but allowing 0 and negative values; or Time, values in UNIX seconds labelled as UTC dates and times. (one of: Linear, Time, Log10, SymLog) |
| `y_scale` | name | `Linear` | Scale of the Y axis: Linear (default); Log10, powers of ten, for values spanning several orders of magnitude (fixed limits must be above 0); SymLog, log-like but allowing 0 and negative values; or Time, values in UNIX seconds labelled as UTC dates and times. (one of: Linear, Time, Log10, SymLog) |
| `x_format` | text | `""` | printf format of the X tick labels, with one %f, %e or %g conversion ("%.0f ms", "%.1f%%"). Empty (default) is "%g". Not allowed on a Time axis. |
| `y_format` | text | `""` | printf format of the Y tick labels, with one %f, %e or %g conversion ("%.0f ms", "%.1f%%"). Empty (default) is "%g". Not allowed on a Time axis. |
| `x_ticks` | list of numbers | `[]` | Positions of the X ticks, in data units. Empty (default) places them automatically. |
| `x_tick_labels` | list of texts | `[]` | Text of each tick in x_ticks, in order, such as category names under bars. Empty (default) labels the ticks with their values. |
| `y_ticks` | list of numbers | `[]` | Positions of the Y ticks, in data units. Empty (default) places them automatically. |
| `y_tick_labels` | list of texts | `[]` | Text of each tick in y_ticks, in order, such as category names under bars. Empty (default) labels the ticks with their values. |
| `y2_label` | text | `""` | Label of the second Y axis, on the right. Setting any y2_ field, or giving a series y_axis: Y2, shows the axis. |
| `y2_flags` | flags | `None` | ImPlotAxisFlags for the second Y axis. (combine with '\|': None, NoLabel, NoGridLines, NoTickMarks, NoTickLabels, NoInitialFit, NoMenus, NoSideSwitch, NoHighlight, Opposite, Foreground, Invert, AutoFit, RangeFit, PanStretch, LockMin, LockMax, Lock, NoDecorations) |
| `y2_min` | number | `0` | Fixed lower limit of the second Y axis.  Ignored (axis auto-fits to its series) when equal to y2_max. |
| `y2_max` | number | `0` | Fixed upper limit of the second Y axis.  Ignored (axis auto-fits to its series) when equal to y2_min. |
| `y2_scale` | name | `Linear` | Scale of the second Y axis: Linear (default); Log10, powers of ten, for values spanning several orders of magnitude (fixed limits must be above 0); SymLog, log-like but allowing 0 and negative values; or Time, values in UNIX seconds labelled as UTC dates and times. (one of: Linear, Time, Log10, SymLog) |
| `y2_format` | text | `""` | printf format of the second Y tick labels, with one %f, %e or %g conversion ("%.0f ms", "%.1f%%"). Empty (default) is "%g". Not allowed on a Time axis. |
| `y2_ticks` | list of numbers | `[]` | Positions of the second Y ticks, in data units. Empty (default) places them automatically. |
| `y2_tick_labels` | list of texts | `[]` | Text of each tick in y2_ticks, in order, such as category names under bars. Empty (default) labels the ticks with their values. |

## PlotAnnotation

Draws a callout box with text next to a point, to point out an event or a value. Without a color it takes the colour of the series drawn just before it (no background if none); its text is black or white, whichever reads better.  Must be a child of a Plot element.

Goes in: Plot.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Series name shown in the plot legend. |
| `color` | colour | `""` | Series colour as "#RRGGBB" or "#RRGGBBAA", used for its lines, fills and markers. Empty (default) takes the next colour of the plot's colormap. |
| `fill_color` | colour | `""` | Colour of filled areas (bar faces, shaded regions, surfaces) as "#RRGGBB" or "#RRGGBBAA". Empty (default) uses color. |
| `line_weight` | number | `1` | Line thickness in pixels (lines, bar edges, marker edges). (0 to 32) |
| `fill_alpha` | number | `-1` | Opacity of fills from 0 (transparent) to 1 (opaque). -1 (default) keeps the series type's own default. (-1 to 1) |
| `marker` | name | `Default` | Point marker: None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus or Asterisk. Default keeps the series type's own choice (markers on a scatter, none on a line). (one of: Default, None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus, Asterisk) |
| `marker_size` | number | `0` | Marker radius in pixels. 0 (default) keeps the automatic size. (0 to 64) |
| `text` | text | `""` | Text in the callout box. Empty (default) shows the point's coordinates. |
| `x` | number | `0` | X coordinate of the point the callout marks, in plot (data) space. |
| `y` | number | `0` | Y coordinate of the point the callout marks, in plot (data) space. |
| `offset_x` | number | `10` | Horizontal position of the box from the point, in pixels (positive is right). |
| `offset_y` | number | `-10` | Vertical position of the box from the point, in pixels (negative is up). |
| `clamp` | true/false | `true` | When true, keeps the box inside the plot area. |
| `y_axis` | name | `Y1` | Y axis the series is drawn against: Y1 (default, left) or Y2, the plot's second Y axis on the right, for a quantity in other units. Y2's range, scale and ticks are the Plot's y2_ fields. (one of: Y1, Y2) |

## PlotBarGroups

Draws groups of bars, one bar per label in each group, coloured in turn from the plot's colormap (color and fill_color are ignored). Groups sit at 0, 1, 2, ...: name them with the Plot's x_ticks and x_tick_labels (y_ when horizontal).  Must be a child of a Plot element.

Goes in: Plot.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Series name shown in the plot legend. |
| `color` | colour | `""` | Series colour as "#RRGGBB" or "#RRGGBBAA", used for its lines, fills and markers. Empty (default) takes the next colour of the plot's colormap. |
| `fill_color` | colour | `""` | Colour of filled areas (bar faces, shaded regions, surfaces) as "#RRGGBB" or "#RRGGBBAA". Empty (default) uses color. |
| `line_weight` | number | `1` | Line thickness in pixels (lines, bar edges, marker edges). (0 to 32) |
| `fill_alpha` | number | `-1` | Opacity of fills from 0 (transparent) to 1 (opaque). -1 (default) keeps the series type's own default. (-1 to 1) |
| `marker` | name | `Default` | Point marker: None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus or Asterisk. Default keeps the series type's own choice (markers on a scatter, none on a line). (one of: Default, None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus, Asterisk) |
| `marker_size` | number | `0` | Marker radius in pixels. 0 (default) keeps the automatic size. (0 to 64) |
| `labels` | list of texts | `[]` | Name of each series of bars (its legend entry), one per bar in a group. |
| `values` | list of numbers | `[]` | Bar values, one row per group: the first row (one value per label) is group 0, the next group 1, and so on. [$a, $b] gives this layout from the columns a and b. |
| `group_size` | number | `0.67` | Width of a group (all its bars together), in plot units. (0 to 16) |
| `shift` | number | `0` | Offset of every group along the axis, in plot units. |
| `horizontal` | true/false | `false` | When true, the bars are horizontal and the groups go up the Y axis. |
| `stacked` | true/false | `false` | When true, a group's bars are stacked into one bar instead of side by side. |
| `y_axis` | name | `Y1` | Y axis the series is drawn against: Y1 (default, left) or Y2, the plot's second Y axis on the right, for a quantity in other units. Y2's range, scale and ticks are the Plot's y2_ fields. (one of: Y1, Y2) |

## PlotBars

Draws vertical bars at each xs[i] with height ys[i].  If xs is empty, bars are placed at integer positions 0, 1, 2, ...  Must be a child of a Plot element.

Goes in: Plot.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Series name shown in the plot legend. |
| `color` | colour | `""` | Series colour as "#RRGGBB" or "#RRGGBBAA", used for its lines, fills and markers. Empty (default) takes the next colour of the plot's colormap. |
| `fill_color` | colour | `""` | Colour of filled areas (bar faces, shaded regions, surfaces) as "#RRGGBB" or "#RRGGBBAA". Empty (default) uses color. |
| `line_weight` | number | `1` | Line thickness in pixels (lines, bar edges, marker edges). (0 to 32) |
| `fill_alpha` | number | `-1` | Opacity of fills from 0 (transparent) to 1 (opaque). -1 (default) keeps the series type's own default. (-1 to 1) |
| `marker` | name | `Default` | Point marker: None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus or Asterisk. Default keeps the series type's own choice (markers on a scatter, none on a line). (one of: Default, None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus, Asterisk) |
| `marker_size` | number | `0` | Marker radius in pixels. 0 (default) keeps the automatic size. (0 to 64) |
| `xs` | list of numbers | `[]` | Array of bar X positions.  If empty, bars are placed at 0, 1, 2, ... using the ys values only. |
| `ys` | list of numbers | `[]` | Array of bar heights (or lengths for horizontal bars). |
| `bar_size` | number | `0.67` | Width (or height for horizontal) of each bar in plot units. (0 to 16) |
| `y_axis` | name | `Y1` | Y axis the series is drawn against: Y1 (default, left) or Y2, the plot's second Y axis on the right, for a quantity in other units. Y2's range, scale and ticks are the Plot's y2_ fields. (one of: Y1, Y2) |

## PlotBarsH

Draws horizontal bars at each ys[i] with length xs[i].  Must be a child of a Plot element.

Goes in: Plot.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Series name shown in the plot legend. |
| `color` | colour | `""` | Series colour as "#RRGGBB" or "#RRGGBBAA", used for its lines, fills and markers. Empty (default) takes the next colour of the plot's colormap. |
| `fill_color` | colour | `""` | Colour of filled areas (bar faces, shaded regions, surfaces) as "#RRGGBB" or "#RRGGBBAA". Empty (default) uses color. |
| `line_weight` | number | `1` | Line thickness in pixels (lines, bar edges, marker edges). (0 to 32) |
| `fill_alpha` | number | `-1` | Opacity of fills from 0 (transparent) to 1 (opaque). -1 (default) keeps the series type's own default. (-1 to 1) |
| `marker` | name | `Default` | Point marker: None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus or Asterisk. Default keeps the series type's own choice (markers on a scatter, none on a line). (one of: Default, None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus, Asterisk) |
| `marker_size` | number | `0` | Marker radius in pixels. 0 (default) keeps the automatic size. (0 to 64) |
| `xs` | list of numbers | `[]` | Array of bar X positions.  If empty, bars are placed at 0, 1, 2, ... using the ys values only. |
| `ys` | list of numbers | `[]` | Array of bar heights (or lengths for horizontal bars). |
| `bar_size` | number | `0.67` | Width (or height for horizontal) of each bar in plot units. (0 to 16) |
| `y_axis` | name | `Y1` | Y axis the series is drawn against: Y1 (default, left) or Y2, the plot's second Y axis on the right, for a quantity in other units. Y2's range, scale and ticks are the Plot's y2_ fields. (one of: Y1, Y2) |

## PlotDigital

Draws a digital signal trace: xs = timestamps, ys = 0 or 1. As in ImPlot, a trace is drawn at the bottom of the plot, 4 pixels tall per unit of ys, independent of the y axis (logic-analyser style).  Must be a child of a Plot element.

Goes in: Plot.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Series name shown in the plot legend. |
| `color` | colour | `""` | Series colour as "#RRGGBB" or "#RRGGBBAA", used for its lines, fills and markers. Empty (default) takes the next colour of the plot's colormap. |
| `fill_color` | colour | `""` | Colour of filled areas (bar faces, shaded regions, surfaces) as "#RRGGBB" or "#RRGGBBAA". Empty (default) uses color. |
| `line_weight` | number | `1` | Line thickness in pixels (lines, bar edges, marker edges). (0 to 32) |
| `fill_alpha` | number | `-1` | Opacity of fills from 0 (transparent) to 1 (opaque). -1 (default) keeps the series type's own default. (-1 to 1) |
| `marker` | name | `Default` | Point marker: None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus or Asterisk. Default keeps the series type's own choice (markers on a scatter, none on a line). (one of: Default, None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus, Asterisk) |
| `marker_size` | number | `0` | Marker radius in pixels. 0 (default) keeps the automatic size. (0 to 64) |
| `xs` | list of numbers | `[]` | Array of X coordinates, one per data point. If empty, 0, 1, 2, ... are used. |
| `ys` | list of numbers | `[]` | Array of Y coordinates, one per data point. |
| `y_axis` | name | `Y1` | Y axis the series is drawn against: Y1 (default, left) or Y2, the plot's second Y axis on the right, for a quantity in other units. Y2's range, scale and ticks are the Plot's y2_ fields. (one of: Y1, Y2) |

## PlotErrorBars

Draws error bars at each (xs[i], ys[i]): a line spanning the error, with caps. Without a color it takes the colour of the series drawn just before it, so write the PlotLine, PlotScatter or PlotBars of the same data first.  Must be a child of a Plot element.

Goes in: Plot.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Series name shown in the plot legend. |
| `color` | colour | `""` | Series colour as "#RRGGBB" or "#RRGGBBAA", used for its lines, fills and markers. Empty (default) takes the next colour of the plot's colormap. |
| `fill_color` | colour | `""` | Colour of filled areas (bar faces, shaded regions, surfaces) as "#RRGGBB" or "#RRGGBBAA". Empty (default) uses color. |
| `line_weight` | number | `1` | Line thickness in pixels (lines, bar edges, marker edges). (0 to 32) |
| `fill_alpha` | number | `-1` | Opacity of fills from 0 (transparent) to 1 (opaque). -1 (default) keeps the series type's own default. (-1 to 1) |
| `marker` | name | `Default` | Point marker: None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus or Asterisk. Default keeps the series type's own choice (markers on a scatter, none on a line). (one of: Default, None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus, Asterisk) |
| `marker_size` | number | `0` | Marker radius in pixels. 0 (default) keeps the automatic size. (0 to 64) |
| `xs` | list of numbers | `[]` | X coordinates, one per point. If empty, 0, 1, 2, ... are used. |
| `ys` | list of numbers | `[]` | Y coordinates the bars are centred on, one per point. |
| `err` | list of numbers | `[]` | Symmetric error of each point: its bar spans ys - err to ys + err (xs when horizontal). Leave empty and give neg and pos for asymmetric errors. |
| `neg` | list of numbers | `[]` | Error below each point (left when horizontal); asymmetric errors, with pos. |
| `pos` | list of numbers | `[]` | Error above each point (right when horizontal); asymmetric errors, with neg. |
| `horizontal` | true/false | `false` | When true, the bars are horizontal: the errors apply to xs. |
| `cap_size` | number | `4` | Width of the cap at each end of a bar, in pixels. (0 to 64) |
| `y_axis` | name | `Y1` | Y axis the series is drawn against: Y1 (default, left) or Y2, the plot's second Y axis on the right, for a quantity in other units. Y2's range, scale and ticks are the Plot's y2_ fields. (one of: Y1, Y2) |

## PlotHeatmap

Draws a 2-D heatmap from a flat row-major values array, inside a Plot whose axes match its bounds.  Must be a child of a Plot element.

Goes in: Plot.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Series name shown in the plot legend. |
| `color` | colour | `""` | Series colour as "#RRGGBB" or "#RRGGBBAA", used for its lines, fills and markers. Empty (default) takes the next colour of the plot's colormap. |
| `fill_color` | colour | `""` | Colour of filled areas (bar faces, shaded regions, surfaces) as "#RRGGBB" or "#RRGGBBAA". Empty (default) uses color. |
| `line_weight` | number | `1` | Line thickness in pixels (lines, bar edges, marker edges). (0 to 32) |
| `fill_alpha` | number | `-1` | Opacity of fills from 0 (transparent) to 1 (opaque). -1 (default) keeps the series type's own default. (-1 to 1) |
| `marker` | name | `Default` | Point marker: None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus or Asterisk. Default keeps the series type's own choice (markers on a scatter, none on a line). (one of: Default, None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus, Asterisk) |
| `marker_size` | number | `0` | Marker radius in pixels. 0 (default) keeps the automatic size. (0 to 64) |
| `values` | list of numbers | `[]` | Flattened row-major data array of size rows x cols. |
| `rows` | integer | `1` | Number of rows in the 2-D grid. (1 to 10000) |
| `cols` | integer | `1` | Number of columns in the 2-D grid. (1 to 10000) |
| `scale_min` | number | `0` | Value mapped to the low end of the colormap. |
| `scale_max` | number | `1` | Value mapped to the high end of the colormap. |
| `format` | text | `"%.1f"` | printf format for per-cell value labels; empty string disables cell labels. |
| `x_min` | number | `0` | X coordinate of the left edge of the heatmap bounds. |
| `x_max` | number | `1` | X coordinate of the right edge of the heatmap bounds. |
| `y_min` | number | `0` | Y coordinate of the bottom edge of the heatmap bounds. |
| `y_max` | number | `1` | Y coordinate of the top edge of the heatmap bounds. |
| `y_axis` | name | `Y1` | Y axis the series is drawn against: Y1 (default, left) or Y2, the plot's second Y axis on the right, for a quantity in other units. Y2's range, scale and ticks are the Plot's y2_ fields. (one of: Y1, Y2) |

## PlotHistogram

Draws a 1-D frequency histogram of the values array.  Must be a child of a Plot element.

Goes in: Plot.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Series name shown in the plot legend. |
| `color` | colour | `""` | Series colour as "#RRGGBB" or "#RRGGBBAA", used for its lines, fills and markers. Empty (default) takes the next colour of the plot's colormap. |
| `fill_color` | colour | `""` | Colour of filled areas (bar faces, shaded regions, surfaces) as "#RRGGBB" or "#RRGGBBAA". Empty (default) uses color. |
| `line_weight` | number | `1` | Line thickness in pixels (lines, bar edges, marker edges). (0 to 32) |
| `fill_alpha` | number | `-1` | Opacity of fills from 0 (transparent) to 1 (opaque). -1 (default) keeps the series type's own default. (-1 to 1) |
| `marker` | name | `Default` | Point marker: None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus or Asterisk. Default keeps the series type's own choice (markers on a scatter, none on a line). (one of: Default, None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus, Asterisk) |
| `marker_size` | number | `0` | Marker radius in pixels. 0 (default) keeps the automatic size. (0 to 64) |
| `values` | list of numbers | `[]` | Array of sample values to bin into the histogram. |
| `bins` | integer | `-1` | Number of bins.  -1 = square root, -2 = Sturges, -3 = Rice, -4 = Scott (ImPlotBin), >0 = explicit count. (-4 to 1e+05) |
| `cumulative` | true/false | `false` | When true, renders a cumulative distribution instead. |
| `density` | true/false | `false` | When true, normalizes bar heights to form a probability density. |
| `range_min` | number | `0` | Minimum sample value to include.  When range_min == range_max (default 0/0), ImPlot auto-ranges. |
| `range_max` | number | `0` | Maximum sample value to include.  When range_min == range_max (default 0/0), ImPlot auto-ranges. |
| `y_axis` | name | `Y1` | Y axis the series is drawn against: Y1 (default, left) or Y2, the plot's second Y axis on the right, for a quantity in other units. Y2's range, scale and ticks are the Plot's y2_ fields. (one of: Y1, Y2) |

## PlotHistogram2D

Draws a 2-D frequency heatmap of the (xs, ys) sample pairs.  Must be a child of a Plot element.

Goes in: Plot.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Series name shown in the plot legend. |
| `color` | colour | `""` | Series colour as "#RRGGBB" or "#RRGGBBAA", used for its lines, fills and markers. Empty (default) takes the next colour of the plot's colormap. |
| `fill_color` | colour | `""` | Colour of filled areas (bar faces, shaded regions, surfaces) as "#RRGGBB" or "#RRGGBBAA". Empty (default) uses color. |
| `line_weight` | number | `1` | Line thickness in pixels (lines, bar edges, marker edges). (0 to 32) |
| `fill_alpha` | number | `-1` | Opacity of fills from 0 (transparent) to 1 (opaque). -1 (default) keeps the series type's own default. (-1 to 1) |
| `marker` | name | `Default` | Point marker: None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus or Asterisk. Default keeps the series type's own choice (markers on a scatter, none on a line). (one of: Default, None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus, Asterisk) |
| `marker_size` | number | `0` | Marker radius in pixels. 0 (default) keeps the automatic size. (0 to 64) |
| `xs` | list of numbers | `[]` | Array of sample X coordinates. |
| `ys` | list of numbers | `[]` | Array of sample Y coordinates (must match xs in length). |
| `x_bins` | integer | `-1` | Bin count for X.  -1 = square root, -2 = Sturges, -3 = Rice, -4 = Scott (ImPlotBin), >0 = explicit count. (-4 to 1e+05) |
| `y_bins` | integer | `-1` | Bin count for Y.  -1 = square root, -2 = Sturges, -3 = Rice, -4 = Scott (ImPlotBin), >0 = explicit count. (-4 to 1e+05) |
| `y_axis` | name | `Y1` | Y axis the series is drawn against: Y1 (default, left) or Y2, the plot's second Y axis on the right, for a quantity in other units. Y2's range, scale and ticks are the Plot's y2_ fields. (one of: Y1, Y2) |

## PlotInfLines

Draws infinite reference lines across the plot.  Must be a child of a Plot element.

Goes in: Plot.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Series name shown in the plot legend. |
| `color` | colour | `""` | Series colour as "#RRGGBB" or "#RRGGBBAA", used for its lines, fills and markers. Empty (default) takes the next colour of the plot's colormap. |
| `fill_color` | colour | `""` | Colour of filled areas (bar faces, shaded regions, surfaces) as "#RRGGBB" or "#RRGGBBAA". Empty (default) uses color. |
| `line_weight` | number | `1` | Line thickness in pixels (lines, bar edges, marker edges). (0 to 32) |
| `fill_alpha` | number | `-1` | Opacity of fills from 0 (transparent) to 1 (opaque). -1 (default) keeps the series type's own default. (-1 to 1) |
| `marker` | name | `Default` | Point marker: None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus or Asterisk. Default keeps the series type's own choice (markers on a scatter, none on a line). (one of: Default, None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus, Asterisk) |
| `marker_size` | number | `0` | Marker radius in pixels. 0 (default) keeps the automatic size. (0 to 64) |
| `values` | list of numbers | `[]` | Array of axis positions at which to draw lines.  When horizontal is false these are X positions (vertical lines); when horizontal is true these are Y positions (horizontal lines). |
| `horizontal` | true/false | `false` | When false, draws vertical lines at each X value.  When true, draws horizontal lines at each Y value. |
| `y_axis` | name | `Y1` | Y axis the series is drawn against: Y1 (default, left) or Y2, the plot's second Y axis on the right, for a quantity in other units. Y2's range, scale and ticks are the Plot's y2_ fields. (one of: Y1, Y2) |

## PlotLine

Draws a connected line through the (xs, ys) data points.  Must be a child of a Plot element.

Goes in: Plot.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Series name shown in the plot legend. |
| `color` | colour | `""` | Series colour as "#RRGGBB" or "#RRGGBBAA", used for its lines, fills and markers. Empty (default) takes the next colour of the plot's colormap. |
| `fill_color` | colour | `""` | Colour of filled areas (bar faces, shaded regions, surfaces) as "#RRGGBB" or "#RRGGBBAA". Empty (default) uses color. |
| `line_weight` | number | `1` | Line thickness in pixels (lines, bar edges, marker edges). (0 to 32) |
| `fill_alpha` | number | `-1` | Opacity of fills from 0 (transparent) to 1 (opaque). -1 (default) keeps the series type's own default. (-1 to 1) |
| `marker` | name | `Default` | Point marker: None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus or Asterisk. Default keeps the series type's own choice (markers on a scatter, none on a line). (one of: Default, None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus, Asterisk) |
| `marker_size` | number | `0` | Marker radius in pixels. 0 (default) keeps the automatic size. (0 to 64) |
| `xs` | list of numbers | `[]` | Array of X coordinates, one per data point. If empty, 0, 1, 2, ... are used. |
| `ys` | list of numbers | `[]` | Array of Y coordinates, one per data point. |
| `y_axis` | name | `Y1` | Y axis the series is drawn against: Y1 (default, left) or Y2, the plot's second Y axis on the right, for a quantity in other units. Y2's range, scale and ticks are the Plot's y2_ fields. (one of: Y1, Y2) |

## PlotPieChart

Draws a pie chart.  Place inside a Plot with x_flags and y_flags NoDecorations, and axis limits [0,1] x [0,1] for correct scaling.  Must be a child of a Plot element.

Goes in: Plot.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Series name shown in the plot legend. |
| `color` | colour | `""` | Series colour as "#RRGGBB" or "#RRGGBBAA", used for its lines, fills and markers. Empty (default) takes the next colour of the plot's colormap. |
| `fill_color` | colour | `""` | Colour of filled areas (bar faces, shaded regions, surfaces) as "#RRGGBB" or "#RRGGBBAA". Empty (default) uses color. |
| `line_weight` | number | `1` | Line thickness in pixels (lines, bar edges, marker edges). (0 to 32) |
| `fill_alpha` | number | `-1` | Opacity of fills from 0 (transparent) to 1 (opaque). -1 (default) keeps the series type's own default. (-1 to 1) |
| `marker` | name | `Default` | Point marker: None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus or Asterisk. Default keeps the series type's own choice (markers on a scatter, none on a line). (one of: Default, None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus, Asterisk) |
| `marker_size` | number | `0` | Marker radius in pixels. 0 (default) keeps the automatic size. (0 to 64) |
| `labels` | text | `""` | Newline-separated list of slice labels, one per entry in the values array. |
| `values` | list of numbers | `[]` | Array of slice magnitudes.  When normalize is false, values are used as-is; when true, they are normalized to sum to 1. |
| `x` | number | `0.5` | X coordinate of the pie center in plot (data) units [0, 1]. (0 to 1) |
| `y` | number | `0.5` | Y coordinate of the pie center in plot (data) units [0, 1]. (0 to 1) |
| `radius` | number | `0.4` | Pie radius in plot (data) units. (0 to 1) |
| `normalize` | true/false | `false` | When true, slices are proportional to their values relative to the total sum. |
| `label_fmt` | text | `"%.1f%%"` | printf format for the per-slice value label. |
| `angle0` | number | `90` | Angle in degrees at which the first slice begins (0 = 3 o'clock, 90 = 12 o'clock). (0 to 360) |
| `y_axis` | name | `Y1` | Y axis the series is drawn against: Y1 (default, left) or Y2, the plot's second Y axis on the right, for a quantity in other units. Y2's range, scale and ticks are the Plot's y2_ fields. (one of: Y1, Y2) |

## PlotScatter

Draws a marker at each (xs[i], ys[i]) coordinate without connecting lines.  Must be a child of a Plot element.

Goes in: Plot.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Series name shown in the plot legend. |
| `color` | colour | `""` | Series colour as "#RRGGBB" or "#RRGGBBAA", used for its lines, fills and markers. Empty (default) takes the next colour of the plot's colormap. |
| `fill_color` | colour | `""` | Colour of filled areas (bar faces, shaded regions, surfaces) as "#RRGGBB" or "#RRGGBBAA". Empty (default) uses color. |
| `line_weight` | number | `1` | Line thickness in pixels (lines, bar edges, marker edges). (0 to 32) |
| `fill_alpha` | number | `-1` | Opacity of fills from 0 (transparent) to 1 (opaque). -1 (default) keeps the series type's own default. (-1 to 1) |
| `marker` | name | `Default` | Point marker: None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus or Asterisk. Default keeps the series type's own choice (markers on a scatter, none on a line). (one of: Default, None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus, Asterisk) |
| `marker_size` | number | `0` | Marker radius in pixels. 0 (default) keeps the automatic size. (0 to 64) |
| `xs` | list of numbers | `[]` | Array of X coordinates, one per data point. If empty, 0, 1, 2, ... are used. |
| `ys` | list of numbers | `[]` | Array of Y coordinates, one per data point. |
| `y_axis` | name | `Y1` | Y axis the series is drawn against: Y1 (default, left) or Y2, the plot's second Y axis on the right, for a quantity in other units. Y2's range, scale and ticks are the Plot's y2_ fields. (one of: Y1, Y2) |

## PlotShaded

Draws a shaded region between two curves, or between ys and a reference.  Must be a child of a Plot element.

Goes in: Plot.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Series name shown in the plot legend. |
| `color` | colour | `""` | Series colour as "#RRGGBB" or "#RRGGBBAA", used for its lines, fills and markers. Empty (default) takes the next colour of the plot's colormap. |
| `fill_color` | colour | `""` | Colour of filled areas (bar faces, shaded regions, surfaces) as "#RRGGBB" or "#RRGGBBAA". Empty (default) uses color. |
| `line_weight` | number | `1` | Line thickness in pixels (lines, bar edges, marker edges). (0 to 32) |
| `fill_alpha` | number | `-1` | Opacity of fills from 0 (transparent) to 1 (opaque). -1 (default) keeps the series type's own default. (-1 to 1) |
| `marker` | name | `Default` | Point marker: None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus or Asterisk. Default keeps the series type's own choice (markers on a scatter, none on a line). (one of: Default, None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus, Asterisk) |
| `marker_size` | number | `0` | Marker radius in pixels. 0 (default) keeps the automatic size. (0 to 64) |
| `xs` | list of numbers | `[]` | Array of X coordinates, one per data point. If empty, 0, 1, 2, ... are used. |
| `ys` | list of numbers | `[]` | Array of Y coordinates, one per data point. |
| `ys2` | list of numbers | `[]` | Second Y array for a band between ys and ys2.  Leave empty to shade between ys and ref. |
| `ref` | number | `0` | Horizontal reference line used when ys2 is empty. |
| `y_axis` | name | `Y1` | Y axis the series is drawn against: Y1 (default, left) or Y2, the plot's second Y axis on the right, for a quantity in other units. Y2's range, scale and ticks are the Plot's y2_ fields. (one of: Y1, Y2) |

## PlotStairs

Draws a staircase (step-interpolated) line through the data.  Must be a child of a Plot element.

Goes in: Plot.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Series name shown in the plot legend. |
| `color` | colour | `""` | Series colour as "#RRGGBB" or "#RRGGBBAA", used for its lines, fills and markers. Empty (default) takes the next colour of the plot's colormap. |
| `fill_color` | colour | `""` | Colour of filled areas (bar faces, shaded regions, surfaces) as "#RRGGBB" or "#RRGGBBAA". Empty (default) uses color. |
| `line_weight` | number | `1` | Line thickness in pixels (lines, bar edges, marker edges). (0 to 32) |
| `fill_alpha` | number | `-1` | Opacity of fills from 0 (transparent) to 1 (opaque). -1 (default) keeps the series type's own default. (-1 to 1) |
| `marker` | name | `Default` | Point marker: None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus or Asterisk. Default keeps the series type's own choice (markers on a scatter, none on a line). (one of: Default, None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus, Asterisk) |
| `marker_size` | number | `0` | Marker radius in pixels. 0 (default) keeps the automatic size. (0 to 64) |
| `xs` | list of numbers | `[]` | Array of X coordinates, one per data point. If empty, 0, 1, 2, ... are used. |
| `ys` | list of numbers | `[]` | Array of Y coordinates, one per data point. |
| `y_axis` | name | `Y1` | Y axis the series is drawn against: Y1 (default, left) or Y2, the plot's second Y axis on the right, for a quantity in other units. Y2's range, scale and ticks are the Plot's y2_ fields. (one of: Y1, Y2) |

## PlotStems

Draws vertical stems from the reference level to each (xs[i], ys[i]).  Must be a child of a Plot element.

Goes in: Plot.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Series name shown in the plot legend. |
| `color` | colour | `""` | Series colour as "#RRGGBB" or "#RRGGBBAA", used for its lines, fills and markers. Empty (default) takes the next colour of the plot's colormap. |
| `fill_color` | colour | `""` | Colour of filled areas (bar faces, shaded regions, surfaces) as "#RRGGBB" or "#RRGGBBAA". Empty (default) uses color. |
| `line_weight` | number | `1` | Line thickness in pixels (lines, bar edges, marker edges). (0 to 32) |
| `fill_alpha` | number | `-1` | Opacity of fills from 0 (transparent) to 1 (opaque). -1 (default) keeps the series type's own default. (-1 to 1) |
| `marker` | name | `Default` | Point marker: None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus or Asterisk. Default keeps the series type's own choice (markers on a scatter, none on a line). (one of: Default, None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus, Asterisk) |
| `marker_size` | number | `0` | Marker radius in pixels. 0 (default) keeps the automatic size. (0 to 64) |
| `xs` | list of numbers | `[]` | Array of X coordinates, one per data point. If empty, 0, 1, 2, ... are used. |
| `ys` | list of numbers | `[]` | Array of Y coordinates, one per data point. |
| `ref` | number | `0` | Y coordinate of the baseline from which stems grow. |
| `y_axis` | name | `Y1` | Y axis the series is drawn against: Y1 (default, left) or Y2, the plot's second Y axis on the right, for a quantity in other units. Y2's range, scale and ticks are the Plot's y2_ fields. (one of: Y1, Y2) |

## PlotTag

Draws a tag on an axis: a coloured box at a value, among the tick labels. Pair it with PlotInfLines to label a target or a threshold. Without a color it takes the colour of the series drawn just before it (no background if none).  Must be a child of a Plot element.

Goes in: Plot.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Series name shown in the plot legend. |
| `color` | colour | `""` | Series colour as "#RRGGBB" or "#RRGGBBAA", used for its lines, fills and markers. Empty (default) takes the next colour of the plot's colormap. |
| `fill_color` | colour | `""` | Colour of filled areas (bar faces, shaded regions, surfaces) as "#RRGGBB" or "#RRGGBBAA". Empty (default) uses color. |
| `line_weight` | number | `1` | Line thickness in pixels (lines, bar edges, marker edges). (0 to 32) |
| `fill_alpha` | number | `-1` | Opacity of fills from 0 (transparent) to 1 (opaque). -1 (default) keeps the series type's own default. (-1 to 1) |
| `marker` | name | `Default` | Point marker: None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus or Asterisk. Default keeps the series type's own choice (markers on a scatter, none on a line). (one of: Default, None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus, Asterisk) |
| `marker_size` | number | `0` | Marker radius in pixels. 0 (default) keeps the automatic size. (0 to 64) |
| `value` | number | `0` | Position of the tag on its axis, in data units. |
| `axis` | name | `Y` | Axis the tag sits on: Y (default; the left axis, or the right one with y_axis: Y2) or X. (one of: X, Y) |
| `text` | text | `""` | Text in the tag. Empty (default) shows the value, formatted like the tick labels. |
| `y_axis` | name | `Y1` | Y axis the series is drawn against: Y1 (default, left) or Y2, the plot's second Y axis on the right, for a quantity in other units. Y2's range, scale and ticks are the Plot's y2_ fields. (one of: Y1, Y2) |

## PlotText

Draws a text string at a plot coordinate.  Must be a child of a Plot element.

Goes in: Plot.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Series name shown in the plot legend. |
| `color` | colour | `""` | Series colour as "#RRGGBB" or "#RRGGBBAA", used for its lines, fills and markers. Empty (default) takes the next colour of the plot's colormap. |
| `fill_color` | colour | `""` | Colour of filled areas (bar faces, shaded regions, surfaces) as "#RRGGBB" or "#RRGGBBAA". Empty (default) uses color. |
| `line_weight` | number | `1` | Line thickness in pixels (lines, bar edges, marker edges). (0 to 32) |
| `fill_alpha` | number | `-1` | Opacity of fills from 0 (transparent) to 1 (opaque). -1 (default) keeps the series type's own default. (-1 to 1) |
| `marker` | name | `Default` | Point marker: None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus or Asterisk. Default keeps the series type's own choice (markers on a scatter, none on a line). (one of: Default, None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus, Asterisk) |
| `marker_size` | number | `0` | Marker radius in pixels. 0 (default) keeps the automatic size. (0 to 64) |
| `text` | text | `""` | The string to display at the (x, y) plot coordinate. |
| `x` | number | `0` | X coordinate in plot (data) space. |
| `y` | number | `0` | Y coordinate in plot (data) space. |
| `offset_x` | number | `0` | Horizontal pixel offset applied after coordinate projection. |
| `offset_y` | number | `0` | Vertical pixel offset applied after coordinate projection. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |
| `y_axis` | name | `Y1` | Y axis the series is drawn against: Y1 (default, left) or Y2, the plot's second Y axis on the right, for a quantity in other units. Y2's range, scale and ticks are the Plot's y2_ fields. (one of: Y1, Y2) |
