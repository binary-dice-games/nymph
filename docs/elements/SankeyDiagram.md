# SankeyDiagram (nymph element reference)

Part of the [nymph element reference](../elements.md), which has the
conventions these tables follow.

## SankeyDiagram

A Sankey diagram: quantities flowing from node to node as bands whose width is proportional to the amount (energy, budgets, traffic, conversions). Nodes are placed in columns automatically. Flows come from the from / to / values lists or SankeyFlow children; SankeyNode sets a node's label and colour. Cycles are not allowed.

Goes in: root, Child, CollapsingHeader, Disabled, Group, HorizontalLayout, Indent, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, VerticalLayout, Window. Children: SankeyFlow, SankeyNode.

| Field | Type | Default | Description |
|---|---|---|---|
| `title` | text | `""` | Title above the diagram; empty for none. |
| `width` | number | `0` | Width in pixels; 0 (default) is 600 in a HorizontalLayout and fills elsewhere; a negative value is a stretch weight in a HorizontalLayout. (-1000 to 8192) |
| `height` | number | `0` | Height in pixels; 0 (default) is the natural height (at least 300, more for many nodes) in a VerticalLayout and fills elsewhere; a negative value is a stretch weight in a VerticalLayout. (-1000 to 8192) |
| `from` | list of texts | `[]` | Node each flow leaves, such as $source; with to and values. |
| `to` | list of texts | `[]` | Node each flow reaches, such as $target. |
| `values` | list of numbers | `[]` | Quantity of each flow, such as $amount; each above 0. |
| `colormap` | name | `Default` | Colormap for this plot: the colours series take in turn when they set none, and the scale of heatmaps and surfaces. Deep (the usual default), Dark, Pastel, Paired, Viridis, Plasma, Hot, Cool, Pink, Jet, Twilight, RdBu, BrBG, PiYG, Spectral or Greys. Default leaves the current colormap. (one of: Default, Deep, Dark, Pastel, Paired, Viridis, Plasma, Hot, Cool, Pink, Jet, Twilight, RdBu, BrBG, PiYG, Spectral, Greys) |
| `link_color` | name | `Source` | Colour of the bands: Source (the node they leave, default), Target (the node they reach) or Gray. (one of: Source, Target, Gray) |
| `node_width` | number | `16` | Width of the node bars in pixels. (2 to 200) |
| `node_spacing` | number | `12` | Least vertical gap between nodes of a column, in pixels. (0 to 200) |
| `show_values` | true/false | `true` | When true, each label ends with the node's total. |
| `value_format` | text | `"%g"` | printf format of the totals in the labels ("%.0f GWh"). |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## SankeyFlow

A flow of `value` from one node to another. The same flows can be given as the diagram's from / to / values lists.

Goes in: SankeyDiagram.

| Field | Type | Default | Description |
|---|---|---|---|
| `from` | text | `""` | Node the quantity leaves. |
| `to` | text | `""` | Node it reaches. |
| `value` | number | `0` | The quantity; the band's width is proportional to it. Above 0. |
| `color` | colour | `""` | Colour of the band; empty follows the diagram's link_color. |

## SankeyNode

Optional: sets the label or colour of a node that flows name. Nodes are listed in this order within their column.

Goes in: SankeyDiagram.

| Field | Type | Default | Description |
|---|---|---|---|
| `id` | text | `""` | The name flows use for the node. |
| `name` | text | `""` | Label shown; empty shows the id. |
| `color` | colour | `""` | Colour of the node (and of the bands leaving it); empty takes the colormap's. |
