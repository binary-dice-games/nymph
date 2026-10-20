# ColorScale (nymph element reference)

Part of the [nymph element reference](../elements.md), which has the
conventions these tables follow.

## ColorScale

A colour bar: the colormap from scale_min to scale_max with ticks, to read the values of a heatmap or surface. Put it beside the plot in a HorizontalLayout and give it the plot's colormap.

Goes in: root, Child, CollapsingHeader, Disabled, Group, HorizontalLayout, Indent, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, VerticalLayout, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Label along the scale (rotated). Empty (default) for none. |
| `scale_min` | number | `0` | Value at the low end of the colormap; match the heatmap's. |
| `scale_max` | number | `1` | Value at the high end of the colormap; match the heatmap's. |
| `colormap` | name | `Default` | Colormap for this plot: the colours series take in turn when they set none, and the scale of heatmaps and surfaces. Deep (the usual default), Dark, Pastel, Paired, Viridis, Plasma, Hot, Cool, Pink, Jet, Twilight, RdBu, BrBG, PiYG, Spectral or Greys. Default leaves the current colormap. (one of: Default, Deep, Dark, Pastel, Paired, Viridis, Plasma, Hot, Cool, Pink, Jet, Twilight, RdBu, BrBG, PiYG, Spectral, Greys) |
| `format` | text | `"%g"` | printf format of the tick labels. |
| `flags` | flags | `None` | NoLabel hides the label, Opposite puts the ticks on the left, Invert puts the low end at the top. (combine with '\|': None, NoLabel, Opposite, Invert) |
| `width` | number | `0` | Width in pixels; 0 (default) is the natural width, enough for the tick labels. (-1000 to 8192) |
| `height` | number | `300` | Height in pixels; the default 300 matches a Plot's, so the two line up side by side (give both the same height). A ColorScale used as root with no height fills the image. (-1000 to 8192) |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |
