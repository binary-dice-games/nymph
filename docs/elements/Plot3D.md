# Plot3D (nymph element reference)

Part of the [nymph element reference](../elements.md), which has the
conventions these tables follow.

## Plot3D

A 3D plot (ImPlot3D). Children are 3D series: Plot3DLine, Plot3DSurface, ...

Goes in: root, Child, CollapsingHeader, Disabled, Group, HorizontalLayout, Indent, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, VerticalLayout, Window. Children: Plot3DLine, Plot3DMesh, Plot3DQuad, Plot3DScatter, Plot3DSurface, Plot3DText, Plot3DTriangle.

| Field | Type | Default | Description |
|---|---|---|---|
| `title` | text | `"##plot3d"` | Plot title shown at the top; prefix ## to hide the title bar. |
| `x_label` | text | `""` | X axis label; empty string = no label. |
| `y_label` | text | `""` | Y axis label; empty string = no label. |
| `z_label` | text | `""` | Z axis label; empty string = no label. |
| `width` | number | `-1` | Plot width in pixels; -1 (default) fills the available width. In a HorizontalLayout a negative value is a stretch weight (-1 and -3 split the space left 1:3) and 0 the natural width. (-1000 to 8192) |
| `height` | number | `400` | Plot height in pixels. A Plot3D used as root with no height fills the image. In a VerticalLayout a negative value is a stretch weight (-1 and -3 split the space left 1:3) and 0 the natural height (400). (-1000 to 8192) |
| `flags` | flags | `None` | ImPlot3DFlags bitmask (combine names with '\|'; see implot3d.h). (combine with '\|': None, NoTitle, NoLegend, NoMouseText, NoClip, NoMenus, Equal, NoRotate, NoPan, NoZoom, NoInputs, CanvasOnly) |
| `x_flags` | flags | `None` | ImPlot3DAxisFlags for the X axis (combine names with '\|'). (combine with '\|': None, NoLabel, NoGridLines, NoTickMarks, NoTickLabels, LockMin, LockMax, AutoFit, Invert, PanStretch, Lock, NoDecorations) |
| `y_flags` | flags | `None` | ImPlot3DAxisFlags for the Y axis (combine names with '\|'). (combine with '\|': None, NoLabel, NoGridLines, NoTickMarks, NoTickLabels, LockMin, LockMax, AutoFit, Invert, PanStretch, Lock, NoDecorations) |
| `z_flags` | flags | `None` | ImPlot3DAxisFlags for the Z axis (combine names with '\|'). (combine with '\|': None, NoLabel, NoGridLines, NoTickMarks, NoTickLabels, LockMin, LockMax, AutoFit, Invert, PanStretch, Lock, NoDecorations) |
| `colormap` | name | `Default` | Colormap for this plot: the colours series take in turn when they set none, and the scale of heatmaps and surfaces. Deep (the usual default), Dark, Pastel, Paired, Viridis, Plasma, Hot, Cool, Pink, Jet, Twilight, RdBu, BrBG, PiYG, Spectral or Greys. Default leaves the current colormap. (one of: Default, Deep, Dark, Pastel, Paired, Viridis, Plasma, Hot, Cool, Pink, Jet, Twilight, RdBu, BrBG, PiYG, Spectral, Greys) |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## Plot3DLine

Draws a connected 3-D line through (xs[i], ys[i], zs[i]) data points. Must be a child of a Plot3D element.

Goes in: Plot3D.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Series name shown in the plot legend. |
| `color` | colour | `""` | Series colour as "#RRGGBB" or "#RRGGBBAA", used for its lines, fills and markers. Empty (default) takes the next colour of the plot's colormap. |
| `fill_color` | colour | `""` | Colour of filled areas (bar faces, shaded regions, surfaces) as "#RRGGBB" or "#RRGGBBAA". Empty (default) uses color. |
| `line_weight` | number | `1` | Line thickness in pixels (lines, bar edges, marker edges). (0 to 32) |
| `fill_alpha` | number | `-1` | Opacity of fills from 0 (transparent) to 1 (opaque). -1 (default) keeps the series type's own default. (-1 to 1) |
| `marker` | name | `Default` | Point marker: None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus or Asterisk. Default keeps the series type's own choice (markers on a scatter, none on a line). (one of: Default, None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus, Asterisk) |
| `marker_size` | number | `0` | Marker radius in pixels. 0 (default) keeps the automatic size. (0 to 64) |
| `xs` | list of numbers | `[]` | Array of X coordinates, one per data point. |
| `ys` | list of numbers | `[]` | Array of Y coordinates, one per data point. |
| `zs` | list of numbers | `[]` | Array of Z coordinates, one per data point. |

## Plot3DMesh

Draws an indexed 3-D triangle mesh. xs/ys/zs are vertex arrays; indices is a flat list of vertex indices (groups of 3 per triangle). Must be a child of a Plot3D element.

Goes in: Plot3D.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Series name shown in the plot legend. |
| `color` | colour | `""` | Series colour as "#RRGGBB" or "#RRGGBBAA", used for its lines, fills and markers. Empty (default) takes the next colour of the plot's colormap. |
| `fill_color` | colour | `""` | Colour of filled areas (bar faces, shaded regions, surfaces) as "#RRGGBB" or "#RRGGBBAA". Empty (default) uses color. |
| `line_weight` | number | `1` | Line thickness in pixels (lines, bar edges, marker edges). (0 to 32) |
| `fill_alpha` | number | `-1` | Opacity of fills from 0 (transparent) to 1 (opaque). -1 (default) keeps the series type's own default. (-1 to 1) |
| `marker` | name | `Default` | Point marker: None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus or Asterisk. Default keeps the series type's own choice (markers on a scatter, none on a line). (one of: Default, None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus, Asterisk) |
| `marker_size` | number | `0` | Marker radius in pixels. 0 (default) keeps the automatic size. (0 to 64) |
| `xs` | list of numbers | `[]` | X coordinate of each mesh vertex. |
| `ys` | list of numbers | `[]` | Y coordinate of each mesh vertex. |
| `zs` | list of numbers | `[]` | Z coordinate of each mesh vertex. |
| `indices` | list of integers | `[]` | Flat list of vertex indices forming triangles; length must be a multiple of 3. |

## Plot3DQuad

Draws filled 3-D quads. xs/ys/zs are vertex arrays whose length must be a multiple of 4 (every 4 consecutive vertices form one quad). Must be a child of a Plot3D element.

Goes in: Plot3D.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Series name shown in the plot legend. |
| `color` | colour | `""` | Series colour as "#RRGGBB" or "#RRGGBBAA", used for its lines, fills and markers. Empty (default) takes the next colour of the plot's colormap. |
| `fill_color` | colour | `""` | Colour of filled areas (bar faces, shaded regions, surfaces) as "#RRGGBB" or "#RRGGBBAA". Empty (default) uses color. |
| `line_weight` | number | `1` | Line thickness in pixels (lines, bar edges, marker edges). (0 to 32) |
| `fill_alpha` | number | `-1` | Opacity of fills from 0 (transparent) to 1 (opaque). -1 (default) keeps the series type's own default. (-1 to 1) |
| `marker` | name | `Default` | Point marker: None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus or Asterisk. Default keeps the series type's own choice (markers on a scatter, none on a line). (one of: Default, None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus, Asterisk) |
| `marker_size` | number | `0` | Marker radius in pixels. 0 (default) keeps the automatic size. (0 to 64) |
| `xs` | list of numbers | `[]` | Vertex X coordinates; length must be a multiple of 4. |
| `ys` | list of numbers | `[]` | Vertex Y coordinates; length must be a multiple of 4. |
| `zs` | list of numbers | `[]` | Vertex Z coordinates; length must be a multiple of 4. |

## Plot3DScatter

Draws a marker at each (xs[i], ys[i], zs[i]) without connecting lines. Must be a child of a Plot3D element.

Goes in: Plot3D.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Series name shown in the plot legend. |
| `color` | colour | `""` | Series colour as "#RRGGBB" or "#RRGGBBAA", used for its lines, fills and markers. Empty (default) takes the next colour of the plot's colormap. |
| `fill_color` | colour | `""` | Colour of filled areas (bar faces, shaded regions, surfaces) as "#RRGGBB" or "#RRGGBBAA". Empty (default) uses color. |
| `line_weight` | number | `1` | Line thickness in pixels (lines, bar edges, marker edges). (0 to 32) |
| `fill_alpha` | number | `-1` | Opacity of fills from 0 (transparent) to 1 (opaque). -1 (default) keeps the series type's own default. (-1 to 1) |
| `marker` | name | `Default` | Point marker: None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus or Asterisk. Default keeps the series type's own choice (markers on a scatter, none on a line). (one of: Default, None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus, Asterisk) |
| `marker_size` | number | `0` | Marker radius in pixels. 0 (default) keeps the automatic size. (0 to 64) |
| `xs` | list of numbers | `[]` | Array of X coordinates, one per data point. |
| `ys` | list of numbers | `[]` | Array of Y coordinates, one per data point. |
| `zs` | list of numbers | `[]` | Array of Z coordinates, one per data point. |

## Plot3DSurface

Draws a colored 3-D parametric surface on an x_count x y_count grid. xs, ys, zs are flattened row-major arrays. Must be a child of a Plot3D element.

Goes in: Plot3D.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Series name shown in the plot legend. |
| `color` | colour | `""` | Series colour as "#RRGGBB" or "#RRGGBBAA", used for its lines, fills and markers. Empty (default) takes the next colour of the plot's colormap. |
| `fill_color` | colour | `""` | Colour of filled areas (bar faces, shaded regions, surfaces) as "#RRGGBB" or "#RRGGBBAA". Empty (default) uses color. |
| `line_weight` | number | `1` | Line thickness in pixels (lines, bar edges, marker edges). (0 to 32) |
| `fill_alpha` | number | `-1` | Opacity of fills from 0 (transparent) to 1 (opaque). -1 (default) keeps the series type's own default. (-1 to 1) |
| `marker` | name | `Default` | Point marker: None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus or Asterisk. Default keeps the series type's own choice (markers on a scatter, none on a line). (one of: Default, None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus, Asterisk) |
| `marker_size` | number | `0` | Marker radius in pixels. 0 (default) keeps the automatic size. (0 to 64) |
| `xs` | list of numbers | `[]` | Flattened x_count x y_count array of X coordinates for each grid point. |
| `ys` | list of numbers | `[]` | Flattened x_count x y_count array of Y coordinates for each grid point. |
| `zs` | list of numbers | `[]` | Flattened x_count x y_count array of Z (height) values. |
| `x_count` | integer | `2` | Number of grid points along the X (first) dimension. (2 to 4096) |
| `y_count` | integer | `2` | Number of grid points along the Y (second) dimension. (2 to 4096) |
| `scale_min` | number | `0` | Minimum value used to map Z height to colormap. Both scale_min and scale_max equal to 0 triggers auto-range. |
| `scale_max` | number | `0` | Maximum value used to map Z height to colormap. Both scale_min and scale_max equal to 0 triggers auto-range. |

## Plot3DText

Renders a text string at a given 3-D plot coordinate. Must be a child of a Plot3D element.

Goes in: Plot3D.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Series name shown in the plot legend. |
| `color` | colour | `""` | Series colour as "#RRGGBB" or "#RRGGBBAA", used for its lines, fills and markers. Empty (default) takes the next colour of the plot's colormap. |
| `fill_color` | colour | `""` | Colour of filled areas (bar faces, shaded regions, surfaces) as "#RRGGBB" or "#RRGGBBAA". Empty (default) uses color. |
| `line_weight` | number | `1` | Line thickness in pixels (lines, bar edges, marker edges). (0 to 32) |
| `fill_alpha` | number | `-1` | Opacity of fills from 0 (transparent) to 1 (opaque). -1 (default) keeps the series type's own default. (-1 to 1) |
| `marker` | name | `Default` | Point marker: None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus or Asterisk. Default keeps the series type's own choice (markers on a scatter, none on a line). (one of: Default, None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus, Asterisk) |
| `marker_size` | number | `0` | Marker radius in pixels. 0 (default) keeps the automatic size. (0 to 64) |
| `text` | text | `""` | The string to display at the given 3-D position. |
| `x` | number | `0` | Plot X coordinate of the text anchor. |
| `y` | number | `0` | Plot Y coordinate of the text anchor. |
| `z` | number | `0` | Plot Z coordinate of the text anchor. |
| `angle` | number | `0` | Rotation angle of the text in degrees. (-360 to 360) |
| `offset_x` | number | `0` | Pixel offset applied to the text after projection (horizontal). |
| `offset_y` | number | `0` | Pixel offset applied to the text after projection (vertical). |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## Plot3DTriangle

Draws filled 3-D triangles. xs/ys/zs are vertex arrays whose length must be a multiple of 3 (every 3 consecutive vertices form one triangle). Must be a child of a Plot3D element.

Goes in: Plot3D.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Series name shown in the plot legend. |
| `color` | colour | `""` | Series colour as "#RRGGBB" or "#RRGGBBAA", used for its lines, fills and markers. Empty (default) takes the next colour of the plot's colormap. |
| `fill_color` | colour | `""` | Colour of filled areas (bar faces, shaded regions, surfaces) as "#RRGGBB" or "#RRGGBBAA". Empty (default) uses color. |
| `line_weight` | number | `1` | Line thickness in pixels (lines, bar edges, marker edges). (0 to 32) |
| `fill_alpha` | number | `-1` | Opacity of fills from 0 (transparent) to 1 (opaque). -1 (default) keeps the series type's own default. (-1 to 1) |
| `marker` | name | `Default` | Point marker: None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus or Asterisk. Default keeps the series type's own choice (markers on a scatter, none on a line). (one of: Default, None, Auto, Circle, Square, Diamond, Up, Down, Left, Right, Cross, Plus, Asterisk) |
| `marker_size` | number | `0` | Marker radius in pixels. 0 (default) keeps the automatic size. (0 to 64) |
| `xs` | list of numbers | `[]` | Vertex X coordinates; length must be a multiple of 3. |
| `ys` | list of numbers | `[]` | Vertex Y coordinates; length must be a multiple of 3. |
| `zs` | list of numbers | `[]` | Vertex Z coordinates; length must be a multiple of 3. |
