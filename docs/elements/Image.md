# Image (nymph element reference)

Part of the [nymph element reference](../elements.md), which has the
conventions these tables follow.

## Image

A built-in placeholder image (a portrait silhouette, a landscape, a logo, ...) to show where an image goes. Also a layout element.

Goes in: root, Child, CollapsingHeader, Disabled, Group, HorizontalLayout, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, VerticalLayout, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `image` | name | `cross` | Which built-in placeholder image to show (`nymph schema Image` lists them; docs/examples.md shows them). (one of: building, chart, cross, document, icon, landscape, logo, map, people, person, product, video) |
| `width` | number | `0` | Width in pixels; in a window a negative value reaches that many pixels before the right edge (-1 fills); in a layout the usual size hint. (-1000 to 8192) |
| `height` | number | `0` | Height in pixels; in a window a negative value fills the height left; in a layout the usual size hint. (-1000 to 8192) |
| `fit` | name | `contain` | contain keeps the aspect ratio and shows the whole image; cover fills the box and clips; stretch fills it in both directions. (one of: contain, cover, stretch) |
| `tint` | colour | `""` | Colour of the image's shapes; its half tones use it at half alpha. Empty: the theme's disabled text colour. |
| `background` | colour | `""` | Colour of the box behind the image and of its cut-outs; "#00000000" is transparent. Empty: the theme's frame colour. |
| `rounding` | number | `0` | Corner radius of the box; half its size gives a circle. (0 to 4096) |
| `border` | true/false | `false` | Outlines the box in the theme's border colour. |
| `label` | text | `""` | A caption over the bottom of the image, for intent: "Hero photo, 1600 x 600". |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |
