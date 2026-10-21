# MindMap (nymph element reference)

Part of the [nymph element reference](../elements.md), which has the
conventions these tables follow.

## MindMap

A mind map: a root idea in the middle with branches to both sides, each branch in its own colour, laid out automatically. Children: one root MindNode (branches nested inside it), MindLink.

Goes in: root, Child, CollapsingHeader, Disabled, Group, HorizontalLayout, Indent, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, VerticalLayout, Window. Children: MindNode, MindLink.

| Field | Type | Default | Description |
|---|---|---|---|
| `title` | text | `""` | Title drawn above the diagram, at the full text size; empty for none. |
| `width` | number | `0` | Width in pixels. 0 (default) is the natural width in a layout; as root it fills the image. In a HorizontalLayout a negative value is a stretch weight. (-1000 to 8192) |
| `height` | number | `0` | Height in pixels. 0 (default) is the natural height in a layout; as root it fills the image. In a VerticalLayout a negative value is a stretch weight. (-1000 to 8192) |
| `edge_style` | name | `Curve` | Shape of the connectors: Elbow (right angles), Curve (smooth) or Straight. (one of: Elbow, Curve, Straight) |
| `node_spacing` | number | `0` | Pixels between sibling subtrees; 0 uses the default (20). (0 to 500) |
| `rank_spacing` | number | `0` | Pixels between levels; 0 uses the default (40). (0 to 500) |
| `min_zoom` | number | `0.5` | Smallest scale the diagram may be drawn at to fit its space. Needing less is an error that gives the size the diagram needs. (0.05 to 1) |
| `max_zoom` | number | `1` | Largest scale: 1 (default) never enlarges a small diagram; 2 lets it grow to fill a large space. (0.1 to 8) |
| `line_color` | colour | `""` | Default colour of edges and node outlines; empty for the theme's. |
| `fill_color` | colour | `""` | Default fill of nodes; empty for the theme's. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## MindNode

A node of a mind map. Its children, written inside it, are its branches; `color` sets the branch's colour (inherited down it).

Goes in: MindMap, MindNode. Children: MindNode.

| Field | Type | Default | Description |
|---|---|---|---|
| `id` | text | `""` | Optional id; needed to be named by a MindLink. |
| `text` | text | `""` | The label; empty shows the id. |
| `side` | name | `Auto` | Children of the root only: Left or Right pins the branch to that side; Auto (default) balances the two sides. (one of: Auto, Left, Right) |
| `collapsed` | true/false | `false` | The children are not drawn; a +N badge counts what is hidden. |
| `color` | colour | `""` | Outline colour; empty for the diagram's. |
| `fill_color` | colour | `""` | Fill colour; empty for the diagram's. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## MindLink

A dashed curve between two nodes of a mind map (both need an id); it does not move anything.

Goes in: MindMap.

| Field | Type | Default | Description |
|---|---|---|---|
| `from` | text | `""` | Id of the node the link leaves. |
| `to` | text | `""` | Id of the node it reaches. |
| `label` | text | `""` | Text on the link. |
| `color` | colour | `""` | Line colour; empty for the theme's. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |
