# SequenceDiagram (nymph element reference)

Part of the [nymph element reference](../elements.md), which has the
conventions these tables follow.

## SequenceDiagram

A UML sequence diagram: participants in columns, children in time order top to bottom. Children: Participant (first), Message, Fragment, Note.

Goes in: root, Child, CollapsingHeader, Disabled, Group, HorizontalLayout, Indent, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, VerticalLayout, Window. Children: Else, Fragment, Message, Participant, Note.

| Field | Type | Default | Description |
|---|---|---|---|
| `title` | text | `""` | Title drawn above the diagram, at the full text size; empty for none. |
| `width` | number | `0` | Width in pixels. 0 (default) is the natural width in a layout; as root it fills the image. In a HorizontalLayout a negative value is a stretch weight. (-1000 to 8192) |
| `height` | number | `0` | Height in pixels. 0 (default) is the natural height in a layout; as root it fills the image. In a VerticalLayout a negative value is a stretch weight. (-1000 to 8192) |
| `node_spacing` | number | `0` | Least pixels between participant boxes; 0 uses the default (40). Columns widen to fit message text. (0 to 500) |
| `autonumber` | true/false | `false` | Number the messages (1., 2., ...). |
| `mirror_participants` | true/false | `true` | Repeat the participant boxes at the bottom. |
| `min_zoom` | number | `0.5` | Smallest scale the diagram may be drawn at to fit its space. Needing less is an error that gives the size the diagram needs. (0.05 to 1) |
| `max_zoom` | number | `1` | Largest scale: 1 (default) never enlarges a small diagram; 2 lets it grow to fill a large space. (0.1 to 8) |
| `line_color` | colour | `""` | Default colour of edges and node outlines; empty for the theme's. |
| `fill_color` | colour | `""` | Default fill of nodes; empty for the theme's. |
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

## Else

Separates the operands of an Alt, Par or Critical fragment.

Goes in: Fragment, SequenceDiagram.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Condition of the next operand, shown as [label]. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## Fragment

A combined fragment around messages: loop, alt (with Else), opt, par, critical, break or group.

Goes in: Fragment, SequenceDiagram. Children: Else, Fragment, Message, Participant, Note.

| Field | Type | Default | Description |
|---|---|---|---|
| `kind` | name | `Loop` | The operator shown in the frame's tab (loop, alt, opt, par, critical, break); Group is a filled box with no tab (mermaid's rect). (one of: Loop, Alt, Opt, Par, Critical, Break, Group) |
| `label` | text | `""` | Condition or title, shown as [label] next to the tab. |
| `over` | list of texts | `[]` | Ids of the leftmost and rightmost participants the frame covers; empty covers those its messages touch. |
| `fill_color` | colour | `""` | Group only: the box's colour; empty for the theme's. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## Message

A message between two participants, one row of the diagram. Rows follow the order of the children.

Goes in: Fragment, SequenceDiagram.

| Field | Type | Default | Description |
|---|---|---|---|
| `from` | text | `""` | Id of the sending participant. |
| `to` | text | `""` | Id of the receiving participant; the same as from for a self message. |
| `text` | text | `""` | Text drawn above the arrow. |
| `kind` | name | `Sync` | Sync: solid line, filled arrow. Async: open arrow. Reply: dashed, open arrow. Create: dashed arrow to a participant with created: true. Destroy: the target's lifeline ends in a cross. (one of: Sync, Async, Reply, Create, Destroy) |
| `activate` | true/false | `false` | Start an activation bar on `to` (mermaid's +). |
| `deactivate` | true/false | `false` | End the latest activation bar of `from` (mermaid's -). |
| `color` | colour | `""` | Line colour; empty for the diagram's. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## Participant

A participant: a column with its lifeline. Declare every participant before the first message; columns follow declaration order.

Goes in: Fragment, SequenceDiagram.

| Field | Type | Default | Description |
|---|---|---|---|
| `id` | text | `""` | Unique id that messages and notes name. |
| `name` | text | `""` | Name shown in the box; empty shows the id. |
| `kind` | name | `Participant` | Participant is a box; Actor a stick figure; Boundary, Control, Entity the robustness icons; Database a cylinder. (one of: Participant, Actor, Boundary, Control, Entity, Database) |
| `created` | true/false | `false` | true: the participant appears at the Create message that makes it, not at the top. |
| `color` | colour | `""` | Outline colour; empty for the diagram's. |
| `fill_color` | colour | `""` | Fill colour; empty for the diagram's. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |
