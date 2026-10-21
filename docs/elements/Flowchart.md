# Flowchart (nymph element reference)

Part of the [nymph element reference](../elements.md), which has the
conventions these tables follow.

## Flowchart

A flowchart or any node-and-link diagram (process, decision tree, dependency or architecture graph, mermaid's flowchart), laid out automatically (Graphviz dot's layered layout). Children: FlowNode, FlowGroup, FlowLink, Note.

Goes in: root, Child, CollapsingHeader, Disabled, Group, HorizontalLayout, Indent, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, VerticalLayout, Window. Children: FlowGroup, FlowLink, FlowNode, Note.

| Field | Type | Default | Description |
|---|---|---|---|
| `title` | text | `""` | Title drawn above the diagram, at the full text size; empty for none. |
| `width` | number | `0` | Width in pixels. 0 (default) is the natural width in a layout; as root it fills the image. In a HorizontalLayout a negative value is a stretch weight. (-1000 to 8192) |
| `height` | number | `0` | Height in pixels. 0 (default) is the natural height in a layout; as root it fills the image. In a VerticalLayout a negative value is a stretch weight. (-1000 to 8192) |
| `direction` | name | `TopDown` | Direction of the ranks (Graphviz dot's rankdir; mermaid's TB, BT, LR, RL). (one of: TopDown, BottomUp, LeftRight, RightLeft) |
| `edge_style` | name | `Spline` | Shape of the edges: smooth curves (Spline) or straight segments (Polyline). (one of: Spline, Polyline) |
| `node_spacing` | number | `0` | Pixels between nodes in one rank (dot's nodesep); 0 uses the default (30). (0 to 500) |
| `rank_spacing` | number | `0` | Pixels between ranks (dot's ranksep); 0 uses the default (46). (0 to 500) |
| `min_zoom` | number | `0.5` | Smallest scale the diagram may be drawn at to fit its space. Needing less is an error that gives the size the diagram needs. (0.05 to 1) |
| `max_zoom` | number | `1` | Largest scale: 1 (default) never enlarges a small diagram; 2 lets it grow to fill a large space. (0.1 to 8) |
| `line_color` | colour | `""` | Default colour of edges and node outlines; empty for the theme's. |
| `fill_color` | colour | `""` | Default fill of nodes; empty for the theme's. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## FlowGroup

A group of nodes in a titled box (mermaid's subgraph), such as a service, a team or a phase. Children: FlowNode, FlowGroup, FlowLink, Note; links inside may also join nodes outside.

Goes in: FlowGroup, Flowchart. Children: FlowGroup, FlowLink, FlowNode, Note.

| Field | Type | Default | Description |
|---|---|---|---|
| `id` | text | `""` | Unique id; links may name a group to end at its box. |
| `title` | text | `""` | Title at the top of the box; empty shows the id. |
| `color` | colour | `""` | Outline colour; empty for the diagram's. |
| `fill_color` | colour | `""` | Fill colour; empty for the diagram's. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## FlowLink

A link between two nodes or groups of a flowchart.

Goes in: FlowGroup, Flowchart.

| Field | Type | Default | Description |
|---|---|---|---|
| `from` | text | `""` | Id of the node or group the link leaves. |
| `to` | text | `""` | Id of the node or group it reaches. |
| `label` | text | `""` | Text in the middle of the link, such as a decision's answer. |
| `line` | name | `Solid` | Solid (default), Dotted (dashed: optional or asynchronous) or Thick (the main path). (one of: Solid, Dotted, Thick) |
| `arrow` | name | `End` | Arrowheads: End (at to, default), None (a plain line) or Both. (one of: End, None, Both) |
| `weight` | number | `1` | How strongly the layout keeps this edge short and straight (Graphviz dot's weight). (0 to 100) |
| `min_length` | integer | `1` | Least number of ranks between the two ends (dot's minlen); 0 allows one rank. (0 to 10) |
| `constraint` | true/false | `true` | false: the edge is drawn but does not influence the ranks (dot's constraint). |
| `color` | colour | `""` | Line colour; empty for the diagram's. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## FlowNode

A node of a flowchart.

Goes in: FlowGroup, Flowchart.

| Field | Type | Default | Description |
|---|---|---|---|
| `id` | text | `""` | Unique id that links and notes name. |
| `text` | text | `""` | Text in the node (a line break starts a new line); empty shows the id. |
| `shape` | name | `Box` | Box (a step, default), Rounded, Stadium (start / end), Diamond (decision), Circle, Parallelogram (input / output), Hexagon (preparation), Cylinder (database) or Subroutine (a predefined process). (one of: Box, Rounded, Stadium, Diamond, Circle, Parallelogram, Hexagon, Cylinder, Subroutine) |
| `symbol` | name | `none` | none (default: the `shape` above is used) or any shape of the library (`nymph shapes` lists them, docs/shapes.md shows them); it replaces `shape` and sizes the node around the text. (one of: none, access_point, actor, arrow_bent, arrow_double, arrow_down, arrow_left, arrow_right, arrow_u_turn, arrow_up, artifact, banner, bordered_frame, boundary, brace_left, bracket_left, bracket_right, callout_box, card, chevron, circle, cloud, collate, component, connector, container, control, cross, cube, cylinder, dashed_region, data, database, decision, deep_history, delay, diamond, display, document, ellipse, entity, extract, final, firewall, flow_final, fork_bar, function, heart, hexagon, history, initial, interface_ball, interface_socket, internal_storage, internet, laptop, lifeline_x, load_balancer, manual_input, manual_operation, merge, mobile, multi_document, node_3d, note, object_box, octagon, off_page_connector, or, package, parallelogram, pentagon, pentagon_arrow, plus, predefined_process, preparation, process, queue, rectangle, ribbon, right_triangle, rounded_rectangle, router, server, signal_receive, signal_send, sort, speech_bubble, square, stadium, star, sticky_note, storage, stored_data, summing_junction, swimlane, switch, tabbed_folder, terminator, thought_bubble, trapezoid, triangle, user, workstation) |
| `rank` | text | `""` | Nodes with the same rank text are placed in one rank (Graphviz dot's rank=same); empty for none. |
| `color` | colour | `""` | Outline colour; empty for the diagram's. |
| `fill_color` | colour | `""` | Fill colour; empty for the diagram's. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## Note

A note. In a graph diagram it sits beside the nodes it is attached to; in a sequence diagram it takes a row, over participants or beside one.

Goes in: ActivityDiagram, ClassDiagram, Component, ComponentDiagram, DataFlowDiagram, DeploymentDiagram, DeploymentNode, ERDiagram, FlowGroup, Flowchart, Fragment, NetworkDiagram, ObjectDiagram, Package, PackageDiagram, SequenceDiagram, State, StateDiagram, TrustBoundary, UseCaseDiagram, Zone.

| Field | Type | Default | Description |
|---|---|---|---|
| `text` | text | `""` | The note's text; \n starts a new line. |
| `attach` | list of texts | `[]` | Ids the note is attached to. Graph diagrams: drawn next to the first, joined to each by a dashed line; none is a free note. Sequence diagrams: one participant, with side. |
| `over` | list of texts | `[]` | Sequence diagrams only: the note spans these one or two participants' lifelines. |
| `side` | name | `Right` | Sequence diagrams only, with attach: which side of the lifeline. (one of: Left, Right) |
| `color` | colour | `""` | Outline colour; empty for the diagram's. |
| `fill_color` | colour | `""` | Fill colour; empty for the diagram's. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |
