# Label (nymph element reference)

Part of the [nymph element reference](../elements.md), which has the
conventions these tables follow.

## Label

A read-only text element.

Goes in: root, Child, CollapsingHeader, Disabled, Group, HorizontalLayout, Indent, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, VerticalLayout, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `text` | text | `""` | Displayed text content. |
| `text_color` | colour | `""` | Optional "#RRGGBBAA"/"#RRGGBB" text color; empty uses the theme's. |
| `text_color_light` | colour | `""` | Text color used instead of text_color when image.theme is light. Ignored when empty. |
| `text_color_dark` | colour | `""` | Text color used instead of text_color when image.theme is dark or classic. Ignored when empty. |
| `wrap` | true/false | `false` | When true, wraps text to the available width instead of running past it. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
