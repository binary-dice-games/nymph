# Shape (nymph element reference)

Part of the [nymph element reference](../elements.md), which has the
conventions these tables follow.

## Shape

One shape of the built-in library (boxes, flowchart symbols, arrows, callouts, UML and network icons) with an optional label. Use it for legends, annotations and dashboards; diagram nodes take the same names in their `shape` field.

Goes in: root, Child, CollapsingHeader, Disabled, Group, HorizontalLayout, Indent, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, VerticalLayout, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `shape` | name | `rectangle` | Which shape of the library to draw (`nymph shapes` lists them, docs/shapes.md shows them). (one of: access_point, actor, arrow_bent, arrow_double, arrow_down, arrow_left, arrow_right, arrow_u_turn, arrow_up, artifact, banner, bordered_frame, boundary, brace_left, bracket_left, bracket_right, callout_box, card, chevron, circle, cloud, collate, component, connector, container, control, cross, cube, cylinder, dashed_region, data, database, decision, deep_history, delay, diamond, display, document, ellipse, entity, extract, final, firewall, flow_final, fork_bar, function, heart, hexagon, history, initial, interface_ball, interface_socket, internal_storage, internet, laptop, lifeline_x, load_balancer, manual_input, manual_operation, merge, mobile, multi_document, node_3d, note, object_box, octagon, off_page_connector, or, package, parallelogram, pentagon, pentagon_arrow, plus, predefined_process, preparation, process, queue, rectangle, ribbon, right_triangle, rounded_rectangle, router, server, signal_receive, signal_send, sort, speech_bubble, square, stadium, star, sticky_note, storage, stored_data, summing_junction, swimlane, switch, tabbed_folder, terminator, thought_bubble, trapezoid, triangle, user, workstation) |
| `text` | text | `""` | The label: inside the shape, or below an icon-style shape. `\n` breaks lines. |
| `width` | number | `0` | Width hint in pixels: 0 (default) is the natural width, a positive value fixed, a negative value a stretch weight, as for any layout child. (-1000 to 8192) |
| `height` | number | `0` | Height hint in pixels, as for width. An icon-style shape keeps its aspect ratio inside the box. (-1000 to 8192) |
| `color` | colour | `""` | Line colour as "#RRGGBB[AA]"; empty is the theme's text colour. |
| `fill_color` | colour | `""` | Fill colour as "#RRGGBB[AA]"; empty is the theme's frame colour. |
| `line_weight` | number | `1.5` | Line width in pixels. (0.25 to 16) |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |
