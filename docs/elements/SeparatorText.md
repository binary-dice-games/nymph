# SeparatorText (nymph element reference)

Part of the [nymph element reference](../elements.md), which has the
conventions these tables follow.

## SeparatorText

A horizontal rule with a text label embedded in the line.

Goes in: root, Child, CollapsingHeader, Disabled, Group, HorizontalLayout, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, VerticalLayout, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text shown inline with the separator line. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |
