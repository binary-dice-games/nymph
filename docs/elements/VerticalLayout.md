# VerticalLayout (nymph element reference)

Part of the [nymph element reference](../elements.md), which has the
conventions these tables follow.

Other elements it takes have their own pages: [ActivityDiagram](ActivityDiagram.md), [ClassDiagram](ClassDiagram.md), [ColorScale](ColorScale.md), [ComponentDiagram](ComponentDiagram.md), [DataFlowDiagram](DataFlowDiagram.md), [DeploymentDiagram](DeploymentDiagram.md), [ERDiagram](ERDiagram.md), [Flowchart](Flowchart.md), [GanttChart](GanttChart.md), [HorizontalLayout](HorizontalLayout.md), [Label](Label.md), [MindMap](MindMap.md), [NetworkDiagram](NetworkDiagram.md), [ObjectDiagram](ObjectDiagram.md), [OrgChart](OrgChart.md), [PackageDiagram](PackageDiagram.md), [Plot](Plot.md), [Plot3D](Plot3D.md), [SankeyDiagram](SankeyDiagram.md), [Screen](Screen.md), [Separator](Separator.md), [SeparatorText](SeparatorText.md), [SequenceDiagram](SequenceDiagram.md), [Shape](Shape.md), [Spring](Spring.md), [StateDiagram](StateDiagram.md), [TreeDiagram](TreeDiagram.md), [UseCaseDiagram](UseCaseDiagram.md), [VerticalLayout](VerticalLayout.md), [Image](Image.md), [Window](Window.md).

## VerticalLayout

Stacks children top-to-bottom.

Goes in: root, Child, CollapsingHeader, Disabled, Group, HorizontalLayout, Indent, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, VerticalLayout, Window. Children: ActivityDiagram, ClassDiagram, ColorScale, ComponentDiagram, DataFlowDiagram, DeploymentDiagram, ERDiagram, Flowchart, GanttChart, HorizontalLayout, Image, Label, MindMap, NetworkDiagram, ObjectDiagram, OrgChart, PackageDiagram, Plot, Plot3D, SankeyDiagram, Screen, Separator, SeparatorText, SequenceDiagram, Shape, Spring, StateDiagram, TreeDiagram, UseCaseDiagram, VerticalLayout, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `spacing` | number | `0` | Space between child elements in pixels. (0 to 256) |
| `width` | number | `0` | Width hint: 0 (default) is the natural width, a positive value fixed pixels, a negative value a stretch weight inside a HorizontalLayout (it shares the space left, in proportion to its magnitude). Outside a HorizontalLayout anything but a positive value fills the width. (-1000 to 8192) |
| `height` | number | `0` | Height hint: 0 (default) is the natural height, a positive value fixed pixels, a negative value a stretch weight inside a VerticalLayout. Outside a VerticalLayout anything but a positive value fills the height. (-1000 to 8192) |
| `card` | true/false | `false` | Draws the children on a rounded, filled, bordered panel, inset by "padding". Colors follow the theme. |
| `padding` | number | `12` | Inner margin of a card, in pixels. Ignored unless "card" is true. (0 to 128) |
| `rounding` | number | `10` | Corner radius of a card, in pixels. Ignored unless "card" is true. (0 to 64) |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |
