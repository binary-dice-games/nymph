# Screen (nymph element reference)

Part of the [nymph element reference](../elements.md), which has the
conventions these tables follow.

Other elements it takes have their own pages: [ActivityDiagram](ActivityDiagram.md), [ClassDiagram](ClassDiagram.md), [ColorScale](ColorScale.md), [DataFlowDiagram](DataFlowDiagram.md), [ERDiagram](ERDiagram.md), [Flowchart](Flowchart.md), [GanttChart](GanttChart.md), [HorizontalLayout](HorizontalLayout.md), [Label](Label.md), [NetworkDiagram](NetworkDiagram.md), [Plot](Plot.md), [Plot3D](Plot3D.md), [SankeyDiagram](SankeyDiagram.md), [Screen](Screen.md), [Separator](Separator.md), [SeparatorText](SeparatorText.md), [SequenceDiagram](SequenceDiagram.md), [Spring](Spring.md), [StateDiagram](StateDiagram.md), [UseCaseDiagram](UseCaseDiagram.md), [VerticalLayout](VerticalLayout.md), [Image](Image.md), [Window](Window.md).

## Screen

An area where ImGui windows live, each at its own position, with their popups, menu bar and the transitions between them; usually the root.

Goes in: root, Child, CollapsingHeader, Disabled, Group, HorizontalLayout, Indent, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, VerticalLayout, Window. Children: MainMenuBar, Popup, PopupModal, Window, WindowTransition.

| Field | Type | Default | Description |
|---|---|---|---|
| `width` | number | `0` | Width hint: 0 is the natural width (the windows' extent), a positive value fixed pixels, a negative value a stretch weight in a HorizontalLayout. As root it fills the image. (-1000 to 8192) |
| `height` | number | `0` | Height hint: 0 is the natural height, a positive value fixed pixels, a negative value a stretch weight in a VerticalLayout. As root it fills the image. (-1000 to 8192) |
| `background` | colour | `""` | Colour behind the windows; empty is the theme's window background shaded by 10 % (darker in light themes, lighter in dark ones), so windows stand out. |
| `window_rounding` | number | `0` | Corner radius of windows, pixels. (0 to 32) |
| `window_padding` | number | `8` | Space inside windows around their content, pixels. (0 to 64) |
| `window_border` | true/false | `true` | Draws a border around windows. |
| `frame_rounding` | number | `0` | Corner radius of buttons, inputs and other framed widgets. (0 to 32) |
| `frame_padding` | list of numbers | `[]` | Padding inside framed widgets [x, y]; empty is ImGui's [4, 3]. |
| `frame_border` | true/false | `false` | Draws a border around framed widgets. |
| `item_spacing` | list of numbers | `[]` | Space between widgets [x, y]; empty is ImGui's [8, 4]. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## WindowTransition

A labelled arrow from one Window to another on the same Screen: how one interface leads to the next.

Goes in: Screen.

| Field | Type | Default | Description |
|---|---|---|---|
| `from` | text | `""` | The id of the Window the transition starts from. |
| `to` | text | `""` | The id of the Window it leads to (the arrowhead); may equal from (a loop). |
| `label` | text | `""` | The event that triggers it, shown on the arrow: "ok", "cancel", "finished loading". |
| `color` | colour | `""` | Colour of the arrow; empty is the theme's text colour at 60 % opacity. |
| `thickness` | number | `1.5` | Line width in pixels. (1 to 8) |
| `dashed` | true/false | `false` | Draws the line dashed (an optional or error path, say). |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## MainMenuBar

ImGui::BeginMainMenuBar: a menu bar across the top of the Screen (leave room for it above the windows).

Goes in: Screen. Children: Menu, MenuItem, ArrowButton, Bullet, BulletText, Button, Checkbox, Child, CollapsingHeader, ColorButton, ColorEdit3, ColorEdit4, ColorPicker3, ColorPicker4, Combo, Disabled, DragFloat, DragFloat2, DragFloat3, DragFloat4, DragFloatRange2, DragInt, DragInt2, DragInt3, DragInt4, DragIntRange2, Dummy, Group, Image, ImageButton, Indent, InputDouble, InputFloat, InputFloat2, InputFloat3, InputFloat4, InputInt, InputInt2, InputInt3, InputInt4, InputText, InputTextMultiline, InputTextWithHint, LabelText, ListBox, NewLine, ProgressBar, RadioButton, SameLine, Selectable, Separator, SeparatorText, SliderAngle, SliderFloat, SliderFloat2, SliderFloat3, SliderFloat4, SliderInt, SliderInt2, SliderInt3, SliderInt4, SmallButton, Spacing, TabBar, Table, Text, TextColored, TextDisabled, TextLink, TextWrapped, Tooltip, TreeNode, VSliderFloat, VSliderInt.

No fields.

## Popup

ImGui::BeginPopup: a small window shown when open is true or a widget that opens it is clicked; a click outside closes it in a live view.

Goes in: Screen. Children: ArrowButton, Bullet, BulletText, Button, Checkbox, Child, CollapsingHeader, ColorButton, ColorEdit3, ColorEdit4, ColorPicker3, ColorPicker4, Combo, Disabled, DragFloat, DragFloat2, DragFloat3, DragFloat4, DragFloatRange2, DragInt, DragInt2, DragInt3, DragInt4, DragIntRange2, Dummy, Group, Image, ImageButton, Indent, InputDouble, InputFloat, InputFloat2, InputFloat3, InputFloat4, InputInt, InputInt2, InputInt3, InputInt4, InputText, InputTextMultiline, InputTextWithHint, LabelText, ListBox, NewLine, ProgressBar, RadioButton, SameLine, Selectable, Separator, SeparatorText, SliderAngle, SliderFloat, SliderFloat2, SliderFloat3, SliderFloat4, SliderInt, SliderInt2, SliderInt3, SliderInt4, SmallButton, Spacing, TabBar, Table, Text, TextColored, TextDisabled, TextLink, TextWrapped, Tooltip, TreeNode, VSliderFloat, VSliderInt, Menu, MenuItem, ActivityDiagram, ClassDiagram, ColorScale, DataFlowDiagram, ERDiagram, Flowchart, GanttChart, HorizontalLayout, Label, NetworkDiagram, Plot, Plot3D, SankeyDiagram, Screen, SequenceDiagram, Spring, StateDiagram, UseCaseDiagram, VerticalLayout, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `id` | text | `""` | The name widgets refer to in their opens field; unique on its Screen. |
| `open` | true/false | `false` | Shows the popup (in the PNG too). |
| `x` | number | `0` | Left edge relative to the Screen when written; else under the widget that opens it, else (modals) centred. (0 to 8192) |
| `y` | number | `0` | Top edge relative to the Screen (see x). (0 to 8192) |
| `flags` | flags | `None` | ImGuiWindowFlags without the prefix. (combine with '\|': None, NoTitleBar, NoResize, NoMove, NoScrollbar, NoScrollWithMouse, NoCollapse, AlwaysAutoResize, NoBackground, NoMouseInputs, MenuBar, HorizontalScrollbar, NoFocusOnAppearing, NoBringToFrontOnFocus, AlwaysVerticalScrollbar, AlwaysHorizontalScrollbar, NoNavInputs, NoNavFocus, UnsavedDocument, NoNav, NoDecoration, NoInputs) |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## PopupModal

ImGui::BeginPopupModal: a dialog over the Screen, which dims the rest; shown when open is true or a widget that opens it is clicked.

Goes in: Screen. Children: ArrowButton, Bullet, BulletText, Button, Checkbox, Child, CollapsingHeader, ColorButton, ColorEdit3, ColorEdit4, ColorPicker3, ColorPicker4, Combo, Disabled, DragFloat, DragFloat2, DragFloat3, DragFloat4, DragFloatRange2, DragInt, DragInt2, DragInt3, DragInt4, DragIntRange2, Dummy, Group, Image, ImageButton, Indent, InputDouble, InputFloat, InputFloat2, InputFloat3, InputFloat4, InputInt, InputInt2, InputInt3, InputInt4, InputText, InputTextMultiline, InputTextWithHint, LabelText, ListBox, NewLine, ProgressBar, RadioButton, SameLine, Selectable, Separator, SeparatorText, SliderAngle, SliderFloat, SliderFloat2, SliderFloat3, SliderFloat4, SliderInt, SliderInt2, SliderInt3, SliderInt4, SmallButton, Spacing, TabBar, Table, Text, TextColored, TextDisabled, TextLink, TextWrapped, Tooltip, TreeNode, VSliderFloat, VSliderInt, Menu, MenuItem, ActivityDiagram, ClassDiagram, ColorScale, DataFlowDiagram, ERDiagram, Flowchart, GanttChart, HorizontalLayout, Label, NetworkDiagram, Plot, Plot3D, SankeyDiagram, Screen, SequenceDiagram, Spring, StateDiagram, UseCaseDiagram, VerticalLayout, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `id` | text | `""` | The name widgets refer to in their opens field; unique on its Screen. |
| `open` | true/false | `false` | Shows the popup (in the PNG too). |
| `x` | number | `0` | Left edge relative to the Screen when written; else under the widget that opens it, else (modals) centred. (0 to 8192) |
| `y` | number | `0` | Top edge relative to the Screen (see x). (0 to 8192) |
| `flags` | flags | `None` | ImGuiWindowFlags without the prefix. (combine with '\|': None, NoTitleBar, NoResize, NoMove, NoScrollbar, NoScrollWithMouse, NoCollapse, AlwaysAutoResize, NoBackground, NoMouseInputs, MenuBar, HorizontalScrollbar, NoFocusOnAppearing, NoBringToFrontOnFocus, AlwaysVerticalScrollbar, AlwaysHorizontalScrollbar, NoNavInputs, NoNavFocus, UnsavedDocument, NoNav, NoDecoration, NoInputs) |
| `title` | text | `""` | Text of the dialog's title bar. |
| `closable` | true/false | `false` | Shows a close button in the title bar. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |
