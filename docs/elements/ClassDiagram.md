# ClassDiagram (nymph element reference)

Part of the [nymph element reference](../elements.md), which has the
conventions these tables follow.

## ClassDiagram

A UML class diagram, laid out automatically (Graphviz dot's layered layout). Children: Class, ClassRelation, Note.

Goes in: root, Child, CollapsingHeader, Disabled, Group, HorizontalLayout, Indent, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, VerticalLayout, Window. Children: Class, ClassRelation, Note.

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

## Class

A class box with name, attribute and method compartments.

Goes in: ClassDiagram.

| Field | Type | Default | Description |
|---|---|---|---|
| `id` | text | `""` | Unique id that relations and notes name. |
| `name` | text | `""` | Name shown; empty shows the id. |
| `kind` | name | `Class` | Abstract, Interface and Enum show their stereotype; an abstract name is italic. (one of: Class, Abstract, Interface, Enum) |
| `stereotype` | text | `""` | Stereotype shown as «text» above the name; replaces the kind's. |
| `generic` | text | `""` | Type parameters (mermaid's Animal~T~), shown on the box's corner. |
| `attributes` | list of texts | `[]` | Attribute lines, drawn as written ("+name: string"). A trailing $ underlines (static), a trailing * italicises (abstract). |
| `methods` | list of texts | `[]` | Method lines, drawn as written ("+speak() string"); $ and * as above. |
| `rank` | text | `""` | Nodes with the same rank text are placed in one rank (Graphviz dot's rank=same); empty for none. |
| `color` | colour | `""` | Outline colour; empty for the diagram's. |
| `fill_color` | colour | `""` | Fill colour; empty for the diagram's. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## ClassRelation

A relation between two classes.

Goes in: ClassDiagram.

| Field | Type | Default | Description |
|---|---|---|---|
| `from` | text | `""` | Id of the class the relation starts at (the child, the whole, the user). |
| `to` | text | `""` | Id of the class it points to (the parent, the part, the used). |
| `kind` | name | `Association` | Inheritance and Realization draw a hollow triangle at `to` and rank `to` above; Aggregation / Composition a hollow / filled diamond at `from`; DirectedAssociation and Dependency (dashed) an open arrow at `to`. (one of: Association, DirectedAssociation, Inheritance, Realization, Dependency, Aggregation, Composition) |
| `label` | text | `""` | Text drawn beside the line. |
| `from_label` | text | `""` | Text near the `from` end (a multiplicity such as "1"). |
| `to_label` | text | `""` | Text near the `to` end (a multiplicity such as "*"). |
| `weight` | number | `1` | How strongly the layout keeps this edge short and straight (Graphviz dot's weight). (0 to 100) |
| `min_length` | integer | `1` | Least number of ranks between the two ends (dot's minlen); 0 allows one rank. (0 to 10) |
| `constraint` | true/false | `true` | false: the edge is drawn but does not influence the ranks (dot's constraint). |
| `color` | colour | `""` | Line colour; empty for the diagram's. |
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
