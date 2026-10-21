# TreeDiagram (nymph element reference)

Part of the [nymph element reference](../elements.md), which has the
conventions these tables follow.

## TreeDiagram

A tree (a hierarchy, a file layout, a breakdown): one root TreeItem whose children are nested inside it, laid out automatically with each parent centred over its children. Child: TreeItem.

Goes in: root, Child, CollapsingHeader, Disabled, Group, HorizontalLayout, Indent, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, VerticalLayout, Window. Children: TreeItem.

| Field | Type | Default | Description |
|---|---|---|---|
| `title` | text | `""` | Title drawn above the diagram, at the full text size; empty for none. |
| `width` | number | `0` | Width in pixels. 0 (default) is the natural width in a layout; as root it fills the image. In a HorizontalLayout a negative value is a stretch weight. (-1000 to 8192) |
| `height` | number | `0` | Height in pixels. 0 (default) is the natural height in a layout; as root it fills the image. In a VerticalLayout a negative value is a stretch weight. (-1000 to 8192) |
| `direction` | name | `TopDown` | Direction the levels grow: TopDown (default), BottomUp, LeftRight or RightLeft. (one of: TopDown, BottomUp, LeftRight, RightLeft) |
| `edge_style` | name | `Elbow` | Shape of the connectors: Elbow (right angles), Curve (smooth) or Straight. (one of: Elbow, Curve, Straight) |
| `node_spacing` | number | `0` | Pixels between sibling subtrees; 0 uses the default (20). (0 to 500) |
| `rank_spacing` | number | `0` | Pixels between levels; 0 uses the default (40). (0 to 500) |
| `min_zoom` | number | `0.5` | Smallest scale the diagram may be drawn at to fit its space. Needing less is an error that gives the size the diagram needs. (0.05 to 1) |
| `max_zoom` | number | `1` | Largest scale: 1 (default) never enlarges a small diagram; 2 lets it grow to fill a large space. (0.1 to 8) |
| `line_color` | colour | `""` | Default colour of edges and node outlines; empty for the theme's. |
| `fill_color` | colour | `""` | Default fill of nodes; empty for the theme's. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## TreeItem

A node of a tree. Its children, written inside it, are the nodes below it.

Goes in: TreeDiagram, TreeItem. Children: TreeItem.

| Field | Type | Default | Description |
|---|---|---|---|
| `id` | text | `""` | Optional id; unique when given. |
| `text` | text | `""` | The label (a line break starts a new line); empty shows the id. |
| `shape` | name | `rounded_rectangle` | The node's drawing. Any shape of the library (`nymph shapes` lists them, docs/shapes.md shows them); default rounded_rectangle. (one of: access_point, actor, arrow_bent, arrow_double, arrow_down, arrow_left, arrow_right, arrow_u_turn, arrow_up, artifact, banner, bordered_frame, boundary, brace_left, bracket_left, bracket_right, callout_box, card, chevron, circle, cloud, collate, component, connector, container, control, cross, cube, cylinder, dashed_region, data, database, decision, deep_history, delay, diamond, display, document, ellipse, entity, extract, final, firewall, flow_final, fork_bar, function, heart, hexagon, history, initial, interface_ball, interface_socket, internal_storage, internet, laptop, lifeline_x, load_balancer, manual_input, manual_operation, merge, mobile, multi_document, node_3d, note, object_box, octagon, off_page_connector, or, package, parallelogram, pentagon, pentagon_arrow, plus, predefined_process, preparation, process, queue, rectangle, ribbon, right_triangle, rounded_rectangle, router, server, signal_receive, signal_send, sort, speech_bubble, square, stadium, star, sticky_note, storage, stored_data, summing_junction, swimlane, switch, tabbed_folder, terminator, thought_bubble, trapezoid, triangle, user, workstation) |
| `collapsed` | true/false | `false` | The children are not drawn; a +N badge counts what is hidden. |
| `color` | colour | `""` | Outline colour; empty for the diagram's. |
| `fill_color` | colour | `""` | Fill colour; empty for the diagram's. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |
