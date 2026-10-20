# NetworkDiagram (nymph element reference)

Part of the [nymph element reference](../elements.md), which has the
conventions these tables follow.

## NetworkDiagram

A network diagram: devices drawn as icons, joined by links, grouped in zones, laid out automatically. Children: Device, Link, Zone, Note.

Goes in: root, Child, CollapsingHeader, Disabled, Group, HorizontalLayout, Indent, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, VerticalLayout, Window. Children: Device, Link, Zone, Note.

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

## Device

A network device drawn as an icon: server, database, storage, router, switch, firewall, load balancer, access point, cloud, internet, workstation, laptop, mobile or user.

Goes in: NetworkDiagram, Zone.

| Field | Type | Default | Description |
|---|---|---|---|
| `id` | text | `""` | Unique id that links and notes name. |
| `name` | text | `""` | Name shown under the icon; empty shows the id. |
| `kind` | name | `Server` | The icon. (one of: Server, Database, Storage, Router, Switch, Firewall, LoadBalancer, AccessPoint, Cloud, Internet, Workstation, Laptop, Mobile, User) |
| `address` | text | `""` | Shown under the name, dimmer: an IP address, a host name, a CIDR. |
| `rank` | text | `""` | Nodes with the same rank text are placed in one rank (Graphviz dot's rank=same); empty for none. |
| `color` | colour | `""` | Outline colour; empty for the diagram's. |
| `fill_color` | colour | `""` | Fill colour; empty for the diagram's. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## Link

A connection between two devices.

Goes in: NetworkDiagram, Zone.

| Field | Type | Default | Description |
|---|---|---|---|
| `from` | text | `""` | Id of one device. |
| `to` | text | `""` | Id of the other device. |
| `kind` | name | `Wired` | Wired: solid line. Wireless: short dashes. Vpn: long dashes, a thicker line. (one of: Wired, Wireless, Vpn) |
| `label` | text | `""` | Text beside the line ("1 Gbit/s", "VLAN 20"). |
| `from_label` | text | `""` | Text near the `from` end (a port, an interface: "eth0"). |
| `to_label` | text | `""` | Text near the `to` end. |
| `directed` | true/false | `false` | true: an arrowhead at the `to` end (traffic in one direction). |
| `weight` | number | `1` | How strongly the layout keeps this edge short and straight (Graphviz dot's weight). (0 to 100) |
| `min_length` | integer | `1` | Least number of ranks between the two ends (dot's minlen); 0 allows one rank. (0 to 10) |
| `constraint` | true/false | `true` | false: the edge is drawn but does not influence the ranks (dot's constraint). |
| `color` | colour | `""` | Line colour; empty for the diagram's. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## Zone

A dashed box around devices: a subnet, a VLAN, a site, a cloud region; zones nest.

Goes in: NetworkDiagram, Zone. Children: Device, Link, Zone, Note.

| Field | Type | Default | Description |
|---|---|---|---|
| `id` | text | `""` | Unique id (zones cannot be the end of a link). |
| `name` | text | `""` | Label in the box's corner ("DMZ 10.0.1.0/24"); empty shows the id. |
| `color` | colour | `""` | Outline colour; empty for the diagram's. |
| `fill_color` | colour | `""` | Fill colour; empty for the diagram's. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## Note

A note. In a graph diagram it sits beside the nodes it is attached to; in a sequence diagram it takes a row, over participants or beside one.

Goes in: ActivityDiagram, ClassDiagram, DataFlowDiagram, ERDiagram, FlowGroup, Flowchart, Fragment, NetworkDiagram, SequenceDiagram, State, StateDiagram, TrustBoundary, UseCaseDiagram, Zone.

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
