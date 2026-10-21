# Window (nymph element reference)

Part of the [nymph element reference](../elements.md), which has the
conventions these tables follow.

Other elements it takes have their own pages: [ActivityDiagram](ActivityDiagram.md), [ClassDiagram](ClassDiagram.md), [ColorScale](ColorScale.md), [ComponentDiagram](ComponentDiagram.md), [DataFlowDiagram](DataFlowDiagram.md), [DeploymentDiagram](DeploymentDiagram.md), [ERDiagram](ERDiagram.md), [Flowchart](Flowchart.md), [GanttChart](GanttChart.md), [HorizontalLayout](HorizontalLayout.md), [Label](Label.md), [MindMap](MindMap.md), [NetworkDiagram](NetworkDiagram.md), [ObjectDiagram](ObjectDiagram.md), [OrgChart](OrgChart.md), [PackageDiagram](PackageDiagram.md), [Plot](Plot.md), [Plot3D](Plot3D.md), [SankeyDiagram](SankeyDiagram.md), [Screen](Screen.md), [Separator](Separator.md), [SeparatorText](SeparatorText.md), [SequenceDiagram](SequenceDiagram.md), [Shape](Shape.md), [Spring](Spring.md), [StateDiagram](StateDiagram.md), [TreeDiagram](TreeDiagram.md), [UseCaseDiagram](UseCaseDiagram.md), [VerticalLayout](VerticalLayout.md), [Image](Image.md), [Window](Window.md).

## Window

An ImGui window: title bar, decorations and widgets. On a Screen it is placed at x, y; as root or in a layout it fills its rectangle.

Goes in: root, Child, CollapsingHeader, Disabled, Group, HorizontalLayout, Indent, Popup, PopupModal, Screen, TabItem, TableRow, Tooltip, TreeNode, VerticalLayout, Window. Children: MenuBar, ArrowButton, Bullet, BulletText, Button, Checkbox, Child, CollapsingHeader, ColorButton, ColorEdit3, ColorEdit4, ColorPicker3, ColorPicker4, Combo, Disabled, DragFloat, DragFloat2, DragFloat3, DragFloat4, DragFloatRange2, DragInt, DragInt2, DragInt3, DragInt4, DragIntRange2, Dummy, Group, Image, ImageButton, Indent, InputDouble, InputFloat, InputFloat2, InputFloat3, InputFloat4, InputInt, InputInt2, InputInt3, InputInt4, InputText, InputTextMultiline, InputTextWithHint, LabelText, ListBox, NewLine, ProgressBar, RadioButton, SameLine, Selectable, Separator, SeparatorText, SliderAngle, SliderFloat, SliderFloat2, SliderFloat3, SliderFloat4, SliderInt, SliderInt2, SliderInt3, SliderInt4, SmallButton, Spacing, TabBar, Table, Text, TextColored, TextDisabled, TextLink, TextWrapped, Tooltip, TreeNode, VSliderFloat, VSliderInt, ActivityDiagram, ClassDiagram, ColorScale, ComponentDiagram, DataFlowDiagram, DeploymentDiagram, ERDiagram, Flowchart, GanttChart, HorizontalLayout, Label, MindMap, NetworkDiagram, ObjectDiagram, OrgChart, PackageDiagram, Plot, Plot3D, SankeyDiagram, Screen, SequenceDiagram, Shape, Spring, StateDiagram, TreeDiagram, UseCaseDiagram, VerticalLayout, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `title` | text | `""` | Text of the title bar. ImGui's "##" hides the rest from the title; window titles need not be unique. |
| `x` | number | `0` | Left edge relative to the Screen, pixels. Windows that write neither x nor y are cascaded from (16, 16). Not allowed when the Window is placed by a layout (as root or in a layout). (0 to 8192) |
| `y` | number | `0` | Top edge relative to the Screen, pixels (see x). (0 to 8192) |
| `width` | number | `0` | Width: positive is pixels, 0 fits the content (ImGui's auto-fit), negative reaches the Screen's right edge. As a layout element: the usual size hint. (-1000 to 8192) |
| `height` | number | `0` | Height: positive is pixels, 0 fits the content, negative reaches the Screen's bottom edge. As a layout element: the usual size hint. (-1000 to 8192) |
| `collapsed` | true/false | `false` | Shows only the title bar. |
| `closable` | true/false | `false` | Shows a close button; in a live view it hides the window. |
| `flags` | flags | `None` | ImGuiWindowFlags without the prefix, like NoResize\|NoCollapse. A MenuBar child sets MenuBar. (combine with '\|': None, NoTitleBar, NoResize, NoMove, NoScrollbar, NoScrollWithMouse, NoCollapse, AlwaysAutoResize, NoBackground, NoMouseInputs, MenuBar, HorizontalScrollbar, NoFocusOnAppearing, NoBringToFrontOnFocus, AlwaysVerticalScrollbar, AlwaysHorizontalScrollbar, NoNavInputs, NoNavFocus, UnsavedDocument, NoNav, NoDecoration, NoInputs) |
| `scroll` | list of numbers | `[]` | Initial scroll position [x, y] in pixels; empty for none. |
| `id` | text | `""` | A name WindowTransitions refer to (from, to); unique on its Screen. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## Menu

ImGui::BeginMenu: a menu in a MenuBar or in another Menu; its children are its items.

Goes in: MainMenuBar, Menu, MenuBar, Popup, PopupModal. Children: Menu, MenuItem, ArrowButton, Bullet, BulletText, Button, Checkbox, Child, CollapsingHeader, ColorButton, ColorEdit3, ColorEdit4, ColorPicker3, ColorPicker4, Combo, Disabled, DragFloat, DragFloat2, DragFloat3, DragFloat4, DragFloatRange2, DragInt, DragInt2, DragInt3, DragInt4, DragIntRange2, Dummy, Group, Image, ImageButton, Indent, InputDouble, InputFloat, InputFloat2, InputFloat3, InputFloat4, InputInt, InputInt2, InputInt3, InputInt4, InputText, InputTextMultiline, InputTextWithHint, LabelText, ListBox, NewLine, ProgressBar, RadioButton, SameLine, Selectable, Separator, SeparatorText, SliderAngle, SliderFloat, SliderFloat2, SliderFloat3, SliderFloat4, SliderInt, SliderInt2, SliderInt3, SliderInt4, SmallButton, Spacing, TabBar, Table, Text, TextColored, TextDisabled, TextLink, TextWrapped, Tooltip, TreeNode, VSliderFloat, VSliderInt.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text of the menu; "##" hides the rest. |
| `open` | true/false | `false` | Shows the menu open, with its items (one open Menu per level). |
| `enabled` | true/false | `true` | Whether the menu can be opened. |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## MenuItem

ImGui::MenuItem: an item of a Menu; a click reports the item and closes the menu.

Goes in: MainMenuBar, Menu, MenuBar, Popup, PopupModal.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text of the item; "##" hides the rest. |
| `shortcut` | text | `""` | Shortcut text shown on the right ("Ctrl+S"); it does nothing. |
| `checked` | true/false | `false` | Shows a check mark; when written, a click in a live view toggles it. |
| `enabled` | true/false | `true` | Whether the item can be chosen. |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## MenuBar

ImGui::BeginMenuBar: the menu bar of a Window or Child, holding Menus.

Goes in: Child, Window. Children: Menu, MenuItem, ArrowButton, Bullet, BulletText, Button, Checkbox, Child, CollapsingHeader, ColorButton, ColorEdit3, ColorEdit4, ColorPicker3, ColorPicker4, Combo, Disabled, DragFloat, DragFloat2, DragFloat3, DragFloat4, DragFloatRange2, DragInt, DragInt2, DragInt3, DragInt4, DragIntRange2, Dummy, Group, Image, ImageButton, Indent, InputDouble, InputFloat, InputFloat2, InputFloat3, InputFloat4, InputInt, InputInt2, InputInt3, InputInt4, InputText, InputTextMultiline, InputTextWithHint, LabelText, ListBox, NewLine, ProgressBar, RadioButton, SameLine, Selectable, Separator, SeparatorText, SliderAngle, SliderFloat, SliderFloat2, SliderFloat3, SliderFloat4, SliderInt, SliderInt2, SliderInt3, SliderInt4, SmallButton, Spacing, TabBar, Table, Text, TextColored, TextDisabled, TextLink, TextWrapped, Tooltip, TreeNode, VSliderFloat, VSliderInt.

No fields.

## TabItem

ImGui::BeginTabItem: one tab of a TabBar; its children show while it is selected.

Goes in: TabBar. Children: ArrowButton, Bullet, BulletText, Button, Checkbox, Child, CollapsingHeader, ColorButton, ColorEdit3, ColorEdit4, ColorPicker3, ColorPicker4, Combo, Disabled, DragFloat, DragFloat2, DragFloat3, DragFloat4, DragFloatRange2, DragInt, DragInt2, DragInt3, DragInt4, DragIntRange2, Dummy, Group, Image, ImageButton, Indent, InputDouble, InputFloat, InputFloat2, InputFloat3, InputFloat4, InputInt, InputInt2, InputInt3, InputInt4, InputText, InputTextMultiline, InputTextWithHint, LabelText, ListBox, NewLine, ProgressBar, RadioButton, SameLine, Selectable, Separator, SeparatorText, SliderAngle, SliderFloat, SliderFloat2, SliderFloat3, SliderFloat4, SliderInt, SliderInt2, SliderInt3, SliderInt4, SmallButton, Spacing, TabBar, Table, Text, TextColored, TextDisabled, TextLink, TextWrapped, Tooltip, TreeNode, VSliderFloat, VSliderInt, ActivityDiagram, ClassDiagram, ColorScale, ComponentDiagram, DataFlowDiagram, DeploymentDiagram, ERDiagram, Flowchart, GanttChart, HorizontalLayout, Label, MindMap, NetworkDiagram, ObjectDiagram, OrgChart, PackageDiagram, Plot, Plot3D, SankeyDiagram, Screen, SequenceDiagram, Shape, Spring, StateDiagram, TreeDiagram, UseCaseDiagram, VerticalLayout, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text of the tab; "##" hides the rest. |
| `selected` | true/false | `false` | Shows this tab; without one selected, the first tab is shown. |
| `closable` | true/false | `false` | Shows a close button on the tab; in a live view it removes the tab. |
| `flags` | flags | `None` | ImGuiTabItemFlags without the prefix. (combine with '\|': None, UnsavedDocument, NoCloseWithMiddleMouseButton, NoPushId, NoTooltip, NoReorder, Leading, Trailing, NoAssumedClosure) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## TabItemButton

ImGui::TabItemButton: a tab that acts as a button (a "+" to add a tab).

Goes in: TabBar.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text of the button tab. |
| `flags` | flags | `None` | ImGuiTabItemFlags without the prefix, like Trailing. (combine with '\|': None, UnsavedDocument, NoCloseWithMiddleMouseButton, NoPushId, NoTooltip, NoReorder, Leading, Trailing, NoAssumedClosure) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## TableColumn

One column of a Table (ImGui::TableSetupColumn): its header, width and text cells.

Goes in: Table.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Header of the column; a table shows a header row when any column has one. |
| `width` | number | `0` | Width in pixels (WidthFixed) or a stretch weight (WidthStretch); 0 is the table's sizing. (0 to 4096) |
| `flags` | flags | `None` | ImGuiTableColumnFlags without the prefix. (combine with '\|': None, Disabled, DefaultHide, DefaultSort, WidthStretch, WidthFixed, NoResize, NoReorder, NoHide, NoClip, NoSort, NoSortAscending, NoSortDescending, NoHeaderLabel, NoHeaderWidth, PreferSortAscending, PreferSortDescending, IndentEnable, IndentDisable, AngledHeader) |
| `cells` | list of texts | `[]` | The column's text, one cell per row, or a $column whose cells are the rows. |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## TableRow

One row of a Table: each child is one cell, left to right (ImGui::TableNextRow, TableNextColumn).

Goes in: Table. Children: ArrowButton, Bullet, BulletText, Button, Checkbox, Child, CollapsingHeader, ColorButton, ColorEdit3, ColorEdit4, ColorPicker3, ColorPicker4, Combo, Disabled, DragFloat, DragFloat2, DragFloat3, DragFloat4, DragFloatRange2, DragInt, DragInt2, DragInt3, DragInt4, DragIntRange2, Dummy, Group, Image, ImageButton, Indent, InputDouble, InputFloat, InputFloat2, InputFloat3, InputFloat4, InputInt, InputInt2, InputInt3, InputInt4, InputText, InputTextMultiline, InputTextWithHint, LabelText, ListBox, NewLine, ProgressBar, RadioButton, SameLine, Selectable, Separator, SeparatorText, SliderAngle, SliderFloat, SliderFloat2, SliderFloat3, SliderFloat4, SliderInt, SliderInt2, SliderInt3, SliderInt4, SmallButton, Spacing, TabBar, Table, Text, TextColored, TextDisabled, TextLink, TextWrapped, Tooltip, TreeNode, VSliderFloat, VSliderInt, ActivityDiagram, ClassDiagram, ColorScale, ComponentDiagram, DataFlowDiagram, DeploymentDiagram, ERDiagram, Flowchart, GanttChart, HorizontalLayout, Label, MindMap, NetworkDiagram, ObjectDiagram, OrgChart, PackageDiagram, Plot, Plot3D, SankeyDiagram, Screen, SequenceDiagram, Shape, Spring, StateDiagram, TreeDiagram, UseCaseDiagram, VerticalLayout, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `height` | number | `0` | Smallest height of the row in pixels; 0 fits its cells. (0 to 4096) |
| `header` | true/false | `false` | Draws the row as a header row (ImGuiTableRowFlags_Headers). |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## ArrowButton

ImGui::ArrowButton: a square button with an arrow.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | ID of the button (not shown). |
| `direction` | name | `Right` | Where the arrow points. (one of: Left, Right, Up, Down) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## Bullet

ImGui::Bullet: a bullet point; the next widget follows on the same line.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## BulletText

ImGui::BulletText: text after a bullet.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `text` | text | `""` | The text, shown as written (never a printf format). |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## Button

ImGui::Button: a push button.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text shown on the widget; "##" hides the rest (ImGui labels). |
| `size` | list of numbers | `[]` | Size [width, height] in pixels (ImGui's size argument): 0 on an axis fits the label, a negative value stops that far before the right or bottom edge (-1 reaches it); empty fits both. |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## Checkbox

ImGui::Checkbox: a box that can be ticked.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text shown on the widget; "##" hides the rest (ImGui labels). |
| `value` | true/false | `false` | Whether the box is ticked. |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## Child

ImGui::BeginChild: a scrolling region inside a window.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window. Children: MenuBar, ArrowButton, Bullet, BulletText, Button, Checkbox, Child, CollapsingHeader, ColorButton, ColorEdit3, ColorEdit4, ColorPicker3, ColorPicker4, Combo, Disabled, DragFloat, DragFloat2, DragFloat3, DragFloat4, DragFloatRange2, DragInt, DragInt2, DragInt3, DragInt4, DragIntRange2, Dummy, Group, Image, ImageButton, Indent, InputDouble, InputFloat, InputFloat2, InputFloat3, InputFloat4, InputInt, InputInt2, InputInt3, InputInt4, InputText, InputTextMultiline, InputTextWithHint, LabelText, ListBox, NewLine, ProgressBar, RadioButton, SameLine, Selectable, Separator, SeparatorText, SliderAngle, SliderFloat, SliderFloat2, SliderFloat3, SliderFloat4, SliderInt, SliderInt2, SliderInt3, SliderInt4, SmallButton, Spacing, TabBar, Table, Text, TextColored, TextDisabled, TextLink, TextWrapped, Tooltip, TreeNode, VSliderFloat, VSliderInt, ActivityDiagram, ClassDiagram, ColorScale, ComponentDiagram, DataFlowDiagram, DeploymentDiagram, ERDiagram, Flowchart, GanttChart, HorizontalLayout, Label, MindMap, NetworkDiagram, ObjectDiagram, OrgChart, PackageDiagram, Plot, Plot3D, SankeyDiagram, Screen, SequenceDiagram, Shape, Spring, StateDiagram, TreeDiagram, UseCaseDiagram, VerticalLayout, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | ID of the region (not shown). |
| `size` | list of numbers | `[]` | Size [width, height]: positive is pixels, 0 fills the space left (or fits the content with an AutoResize flag), negative stops that far before the edge. Empty is [0, 0]. |
| `child_flags` | flags | `None` | ImGuiChildFlags without the prefix, like Borders. (combine with '\|': None, Borders, AlwaysUseWindowPadding, ResizeX, ResizeY, AutoResizeX, AutoResizeY, AlwaysAutoResize, FrameStyle, NavFlattened) |
| `flags` | flags | `None` | ImGuiWindowFlags without the prefix. (combine with '\|': None, NoTitleBar, NoResize, NoMove, NoScrollbar, NoScrollWithMouse, NoCollapse, AlwaysAutoResize, NoBackground, NoMouseInputs, MenuBar, HorizontalScrollbar, NoFocusOnAppearing, NoBringToFrontOnFocus, AlwaysVerticalScrollbar, AlwaysHorizontalScrollbar, NoNavInputs, NoNavFocus, UnsavedDocument, NoNav, NoDecoration, NoInputs) |
| `scroll` | list of numbers | `[]` | Initial scroll position [x, y] in pixels; empty for none. |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## CollapsingHeader

ImGui::CollapsingHeader: a full-width header that shows or hides its children.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window. Children: ArrowButton, Bullet, BulletText, Button, Checkbox, Child, CollapsingHeader, ColorButton, ColorEdit3, ColorEdit4, ColorPicker3, ColorPicker4, Combo, Disabled, DragFloat, DragFloat2, DragFloat3, DragFloat4, DragFloatRange2, DragInt, DragInt2, DragInt3, DragInt4, DragIntRange2, Dummy, Group, Image, ImageButton, Indent, InputDouble, InputFloat, InputFloat2, InputFloat3, InputFloat4, InputInt, InputInt2, InputInt3, InputInt4, InputText, InputTextMultiline, InputTextWithHint, LabelText, ListBox, NewLine, ProgressBar, RadioButton, SameLine, Selectable, Separator, SeparatorText, SliderAngle, SliderFloat, SliderFloat2, SliderFloat3, SliderFloat4, SliderInt, SliderInt2, SliderInt3, SliderInt4, SmallButton, Spacing, TabBar, Table, Text, TextColored, TextDisabled, TextLink, TextWrapped, Tooltip, TreeNode, VSliderFloat, VSliderInt, ActivityDiagram, ClassDiagram, ColorScale, ComponentDiagram, DataFlowDiagram, DeploymentDiagram, ERDiagram, Flowchart, GanttChart, HorizontalLayout, Label, MindMap, NetworkDiagram, ObjectDiagram, OrgChart, PackageDiagram, Plot, Plot3D, SankeyDiagram, Screen, SequenceDiagram, Shape, Spring, StateDiagram, TreeDiagram, UseCaseDiagram, VerticalLayout, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text of the header; "##" hides the rest (ImGui labels). |
| `open` | true/false | `false` | Whether it is expanded, showing its children. |
| `flags` | flags | `None` | ImGuiTreeNodeFlags without the prefix, like Leaf\|Bullet; DefaultOpen opens it when open is not written. (combine with '\|': None, Selected, Framed, DefaultOpen, AllowOverlap, NoTreePushOnOpen, NoAutoOpenOnLog, OpenOnDoubleClick, OpenOnArrow, Leaf, Bullet, FramePadding, SpanAvailWidth, SpanFullWidth, SpanLabelWidth, SpanAllColumns, LabelSpanAllColumns, DrawLinesNone, DrawLinesFull, DrawLinesToNodes, CollapsingHeader) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## ColorButton

ImGui::ColorButton: a square of colour.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | ID of the button, shown in its tooltip (ImGui's desc_id). |
| `value` | colour | `""` | The colour, "#RRGGBB" or "#RRGGBBAA"; empty is white. |
| `flags` | flags | `None` | ImGuiColorEditFlags without the prefix. (combine with '\|': None, NoAlpha, NoPicker, NoOptions, NoSmallPreview, NoInputs, NoTooltip, NoLabel, NoSidePreview, NoDragDrop, NoBorder, AlphaOpaque, AlphaNoBg, AlphaPreviewHalf, AlphaBar, HDR, DisplayRGB, DisplayHSV, DisplayHex, Uint8, Float, PickerHueBar, PickerHueWheel, InputRGB, InputHSV) |
| `size` | list of numbers | `[]` | Size [width, height] in pixels; empty is a square of the frame height. |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## ColorEdit3

ImGui colour editor: numeric fields and a colour square that opens a picker.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text shown beside the widget; "##" hides the rest (ImGui labels). |
| `value` | colour | `""` | The colour, "#RRGGBB" or "#RRGGBBAA"; empty is white. |
| `flags` | flags | `None` | ImGuiColorEditFlags without the prefix. (combine with '\|': None, NoAlpha, NoPicker, NoOptions, NoSmallPreview, NoInputs, NoTooltip, NoLabel, NoSidePreview, NoDragDrop, NoBorder, AlphaOpaque, AlphaNoBg, AlphaPreviewHalf, AlphaBar, HDR, DisplayRGB, DisplayHSV, DisplayHex, Uint8, Float, PickerHueBar, PickerHueWheel, InputRGB, InputHSV) |
| `width` | number | `0` | Item width in pixels (ImGui's SetNextItemWidth): 0 is ImGui's default, a positive value fixed pixels, a negative value stops that many pixels before the container's right edge (-1 reaches it). (-4096 to 4096) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## ColorEdit4

ImGui colour editor: numeric fields and a colour square that opens a picker.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text shown beside the widget; "##" hides the rest (ImGui labels). |
| `value` | colour | `""` | The colour, "#RRGGBB" or "#RRGGBBAA"; empty is white. |
| `flags` | flags | `None` | ImGuiColorEditFlags without the prefix. (combine with '\|': None, NoAlpha, NoPicker, NoOptions, NoSmallPreview, NoInputs, NoTooltip, NoLabel, NoSidePreview, NoDragDrop, NoBorder, AlphaOpaque, AlphaNoBg, AlphaPreviewHalf, AlphaBar, HDR, DisplayRGB, DisplayHSV, DisplayHex, Uint8, Float, PickerHueBar, PickerHueWheel, InputRGB, InputHSV) |
| `width` | number | `0` | Item width in pixels (ImGui's SetNextItemWidth): 0 is ImGui's default, a positive value fixed pixels, a negative value stops that many pixels before the container's right edge (-1 reaches it). (-4096 to 4096) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## ColorPicker3

ImGui colour picker: a saturation / value square and a hue bar.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text shown beside the widget; "##" hides the rest (ImGui labels). |
| `value` | colour | `""` | The colour, "#RRGGBB" or "#RRGGBBAA"; empty is white. |
| `flags` | flags | `None` | ImGuiColorEditFlags without the prefix. (combine with '\|': None, NoAlpha, NoPicker, NoOptions, NoSmallPreview, NoInputs, NoTooltip, NoLabel, NoSidePreview, NoDragDrop, NoBorder, AlphaOpaque, AlphaNoBg, AlphaPreviewHalf, AlphaBar, HDR, DisplayRGB, DisplayHSV, DisplayHex, Uint8, Float, PickerHueBar, PickerHueWheel, InputRGB, InputHSV) |
| `width` | number | `0` | Item width in pixels (ImGui's SetNextItemWidth): 0 is ImGui's default, a positive value fixed pixels, a negative value stops that many pixels before the container's right edge (-1 reaches it). (-4096 to 4096) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## ColorPicker4

ImGui colour picker: a saturation / value square and a hue bar.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text shown beside the widget; "##" hides the rest (ImGui labels). |
| `value` | colour | `""` | The colour, "#RRGGBB" or "#RRGGBBAA"; empty is white. |
| `flags` | flags | `None` | ImGuiColorEditFlags without the prefix. (combine with '\|': None, NoAlpha, NoPicker, NoOptions, NoSmallPreview, NoInputs, NoTooltip, NoLabel, NoSidePreview, NoDragDrop, NoBorder, AlphaOpaque, AlphaNoBg, AlphaPreviewHalf, AlphaBar, HDR, DisplayRGB, DisplayHSV, DisplayHex, Uint8, Float, PickerHueBar, PickerHueWheel, InputRGB, InputHSV) |
| `width` | number | `0` | Item width in pixels (ImGui's SetNextItemWidth): 0 is ImGui's default, a positive value fixed pixels, a negative value stops that many pixels before the container's right edge (-1 reaches it). (-4096 to 4096) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## Combo

ImGui::Combo: a box showing the selected item that opens a list of the others.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text shown on the widget; "##" hides the rest (ImGui labels). |
| `items` | list of texts | `[]` | The items, or a $column whose cells are the items. |
| `selected` | integer | `0` | Index of the selected item, from 0; -1 for none. (-1 to 1e+06) |
| `height_in_items` | integer | `-1` | Rows of the open list; -1 is ImGui's default. (-1 to 1000) |
| `width` | number | `0` | Item width in pixels (ImGui's SetNextItemWidth): 0 is ImGui's default, a positive value fixed pixels, a negative value stops that many pixels before the container's right edge (-1 reaches it). (-4096 to 4096) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## Disabled

ImGui::BeginDisabled: its children are greyed out and take no input.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window. Children: ArrowButton, Bullet, BulletText, Button, Checkbox, Child, CollapsingHeader, ColorButton, ColorEdit3, ColorEdit4, ColorPicker3, ColorPicker4, Combo, Disabled, DragFloat, DragFloat2, DragFloat3, DragFloat4, DragFloatRange2, DragInt, DragInt2, DragInt3, DragInt4, DragIntRange2, Dummy, Group, Image, ImageButton, Indent, InputDouble, InputFloat, InputFloat2, InputFloat3, InputFloat4, InputInt, InputInt2, InputInt3, InputInt4, InputText, InputTextMultiline, InputTextWithHint, LabelText, ListBox, NewLine, ProgressBar, RadioButton, SameLine, Selectable, Separator, SeparatorText, SliderAngle, SliderFloat, SliderFloat2, SliderFloat3, SliderFloat4, SliderInt, SliderInt2, SliderInt3, SliderInt4, SmallButton, Spacing, TabBar, Table, Text, TextColored, TextDisabled, TextLink, TextWrapped, Tooltip, TreeNode, VSliderFloat, VSliderInt, ActivityDiagram, ClassDiagram, ColorScale, ComponentDiagram, DataFlowDiagram, DeploymentDiagram, ERDiagram, Flowchart, GanttChart, HorizontalLayout, Label, MindMap, NetworkDiagram, ObjectDiagram, OrgChart, PackageDiagram, Plot, Plot3D, SankeyDiagram, Screen, SequenceDiagram, Shape, Spring, StateDiagram, TreeDiagram, UseCaseDiagram, VerticalLayout, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## DragFloat

ImGui a drag box for one number.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text shown beside the widget; "##" hides the rest (ImGui labels). |
| `value` | number | `0` | The value. (-1e+30 to 1e+30) |
| `min` | number | `0` | Smallest value; min = max = 0 is unbounded. (-1e+30 to 1e+30) |
| `max` | number | `0` | Largest value. (-1e+30 to 1e+30) |
| `speed` | number | `1` | Value change per pixel of mouse movement. (0 to 1e+06) |
| `format` | text | `"%.3f"` | printf format of the shown value: one %f, %e or %g (like "%.1f ms"). |
| `flags` | flags | `None` | ImGuiSliderFlags without the prefix, like AlwaysClamp. (combine with '\|': None, Logarithmic, NoRoundToFormat, NoInput, WrapAround, ClampOnInput, ClampZeroRange, NoSpeedTweaks, AlwaysClamp) |
| `width` | number | `0` | Item width in pixels (ImGui's SetNextItemWidth): 0 is ImGui's default, a positive value fixed pixels, a negative value stops that many pixels before the container's right edge (-1 reaches it). (-4096 to 4096) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## DragFloat2

ImGui a drag box for 2 numbers.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text shown beside the widget; "##" hides the rest (ImGui labels). |
| `value` | list of numbers | `[]` | The 2 values. |
| `min` | number | `0` | Smallest value; min = max = 0 is unbounded. (-1e+30 to 1e+30) |
| `max` | number | `0` | Largest value. (-1e+30 to 1e+30) |
| `speed` | number | `1` | Value change per pixel of mouse movement. (0 to 1e+06) |
| `format` | text | `"%.3f"` | printf format of the shown value: one %f, %e or %g (like "%.1f ms"). |
| `flags` | flags | `None` | ImGuiSliderFlags without the prefix, like AlwaysClamp. (combine with '\|': None, Logarithmic, NoRoundToFormat, NoInput, WrapAround, ClampOnInput, ClampZeroRange, NoSpeedTweaks, AlwaysClamp) |
| `width` | number | `0` | Item width in pixels (ImGui's SetNextItemWidth): 0 is ImGui's default, a positive value fixed pixels, a negative value stops that many pixels before the container's right edge (-1 reaches it). (-4096 to 4096) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## DragFloat3

ImGui a drag box for 3 numbers.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text shown beside the widget; "##" hides the rest (ImGui labels). |
| `value` | list of numbers | `[]` | The 3 values. |
| `min` | number | `0` | Smallest value; min = max = 0 is unbounded. (-1e+30 to 1e+30) |
| `max` | number | `0` | Largest value. (-1e+30 to 1e+30) |
| `speed` | number | `1` | Value change per pixel of mouse movement. (0 to 1e+06) |
| `format` | text | `"%.3f"` | printf format of the shown value: one %f, %e or %g (like "%.1f ms"). |
| `flags` | flags | `None` | ImGuiSliderFlags without the prefix, like AlwaysClamp. (combine with '\|': None, Logarithmic, NoRoundToFormat, NoInput, WrapAround, ClampOnInput, ClampZeroRange, NoSpeedTweaks, AlwaysClamp) |
| `width` | number | `0` | Item width in pixels (ImGui's SetNextItemWidth): 0 is ImGui's default, a positive value fixed pixels, a negative value stops that many pixels before the container's right edge (-1 reaches it). (-4096 to 4096) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## DragFloat4

ImGui a drag box for 4 numbers.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text shown beside the widget; "##" hides the rest (ImGui labels). |
| `value` | list of numbers | `[]` | The 4 values. |
| `min` | number | `0` | Smallest value; min = max = 0 is unbounded. (-1e+30 to 1e+30) |
| `max` | number | `0` | Largest value. (-1e+30 to 1e+30) |
| `speed` | number | `1` | Value change per pixel of mouse movement. (0 to 1e+06) |
| `format` | text | `"%.3f"` | printf format of the shown value: one %f, %e or %g (like "%.1f ms"). |
| `flags` | flags | `None` | ImGuiSliderFlags without the prefix, like AlwaysClamp. (combine with '\|': None, Logarithmic, NoRoundToFormat, NoInput, WrapAround, ClampOnInput, ClampZeroRange, NoSpeedTweaks, AlwaysClamp) |
| `width` | number | `0` | Item width in pixels (ImGui's SetNextItemWidth): 0 is ImGui's default, a positive value fixed pixels, a negative value stops that many pixels before the container's right edge (-1 reaches it). (-4096 to 4096) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## DragFloatRange2

ImGui::DragFloatRange2: two drag boxes for a range of numbers.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text shown beside the widget; "##" hides the rest (ImGui labels). |
| `value` | list of numbers | `[]` | The range [low, high]. |
| `speed` | number | `1` | Value change per pixel of mouse movement. (0 to 1e+06) |
| `min` | number | `0` | Smallest value; min = max = 0 is unbounded. (-1e+09 to 1e+09) |
| `max` | number | `0` | Largest value. (-1e+09 to 1e+09) |
| `format` | text | `"%.3f"` | printf format of the low value. |
| `format_max` | text | `""` | printf format of the high value; empty is format. |
| `flags` | flags | `None` | ImGuiSliderFlags without the prefix. (combine with '\|': None, Logarithmic, NoRoundToFormat, NoInput, WrapAround, ClampOnInput, ClampZeroRange, NoSpeedTweaks, AlwaysClamp) |
| `width` | number | `0` | Item width in pixels (ImGui's SetNextItemWidth): 0 is ImGui's default, a positive value fixed pixels, a negative value stops that many pixels before the container's right edge (-1 reaches it). (-4096 to 4096) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## DragInt

ImGui a drag box for one integer.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text shown beside the widget; "##" hides the rest (ImGui labels). |
| `value` | integer | `0` | The value. (-1e+09 to 1e+09) |
| `min` | integer | `0` | Smallest value; min = max = 0 is unbounded. (-1e+09 to 1e+09) |
| `max` | integer | `0` | Largest value. (-1e+09 to 1e+09) |
| `speed` | number | `1` | Value change per pixel of mouse movement. (0 to 1e+06) |
| `format` | text | `"%d"` | printf format of the shown value: one %d (like "%d px"). |
| `flags` | flags | `None` | ImGuiSliderFlags without the prefix, like AlwaysClamp. (combine with '\|': None, Logarithmic, NoRoundToFormat, NoInput, WrapAround, ClampOnInput, ClampZeroRange, NoSpeedTweaks, AlwaysClamp) |
| `width` | number | `0` | Item width in pixels (ImGui's SetNextItemWidth): 0 is ImGui's default, a positive value fixed pixels, a negative value stops that many pixels before the container's right edge (-1 reaches it). (-4096 to 4096) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## DragInt2

ImGui a drag box for 2 integers.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text shown beside the widget; "##" hides the rest (ImGui labels). |
| `value` | list of integers | `[]` | The 2 values. |
| `min` | integer | `0` | Smallest value; min = max = 0 is unbounded. (-1e+09 to 1e+09) |
| `max` | integer | `0` | Largest value. (-1e+09 to 1e+09) |
| `speed` | number | `1` | Value change per pixel of mouse movement. (0 to 1e+06) |
| `format` | text | `"%d"` | printf format of the shown value: one %d (like "%d px"). |
| `flags` | flags | `None` | ImGuiSliderFlags without the prefix, like AlwaysClamp. (combine with '\|': None, Logarithmic, NoRoundToFormat, NoInput, WrapAround, ClampOnInput, ClampZeroRange, NoSpeedTweaks, AlwaysClamp) |
| `width` | number | `0` | Item width in pixels (ImGui's SetNextItemWidth): 0 is ImGui's default, a positive value fixed pixels, a negative value stops that many pixels before the container's right edge (-1 reaches it). (-4096 to 4096) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## DragInt3

ImGui a drag box for 3 integers.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text shown beside the widget; "##" hides the rest (ImGui labels). |
| `value` | list of integers | `[]` | The 3 values. |
| `min` | integer | `0` | Smallest value; min = max = 0 is unbounded. (-1e+09 to 1e+09) |
| `max` | integer | `0` | Largest value. (-1e+09 to 1e+09) |
| `speed` | number | `1` | Value change per pixel of mouse movement. (0 to 1e+06) |
| `format` | text | `"%d"` | printf format of the shown value: one %d (like "%d px"). |
| `flags` | flags | `None` | ImGuiSliderFlags without the prefix, like AlwaysClamp. (combine with '\|': None, Logarithmic, NoRoundToFormat, NoInput, WrapAround, ClampOnInput, ClampZeroRange, NoSpeedTweaks, AlwaysClamp) |
| `width` | number | `0` | Item width in pixels (ImGui's SetNextItemWidth): 0 is ImGui's default, a positive value fixed pixels, a negative value stops that many pixels before the container's right edge (-1 reaches it). (-4096 to 4096) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## DragInt4

ImGui a drag box for 4 integers.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text shown beside the widget; "##" hides the rest (ImGui labels). |
| `value` | list of integers | `[]` | The 4 values. |
| `min` | integer | `0` | Smallest value; min = max = 0 is unbounded. (-1e+09 to 1e+09) |
| `max` | integer | `0` | Largest value. (-1e+09 to 1e+09) |
| `speed` | number | `1` | Value change per pixel of mouse movement. (0 to 1e+06) |
| `format` | text | `"%d"` | printf format of the shown value: one %d (like "%d px"). |
| `flags` | flags | `None` | ImGuiSliderFlags without the prefix, like AlwaysClamp. (combine with '\|': None, Logarithmic, NoRoundToFormat, NoInput, WrapAround, ClampOnInput, ClampZeroRange, NoSpeedTweaks, AlwaysClamp) |
| `width` | number | `0` | Item width in pixels (ImGui's SetNextItemWidth): 0 is ImGui's default, a positive value fixed pixels, a negative value stops that many pixels before the container's right edge (-1 reaches it). (-4096 to 4096) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## DragIntRange2

ImGui::DragIntRange2: two drag boxes for a range of integers.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text shown beside the widget; "##" hides the rest (ImGui labels). |
| `value` | list of integers | `[]` | The range [low, high]. |
| `speed` | number | `1` | Value change per pixel of mouse movement. (0 to 1e+06) |
| `min` | number | `0` | Smallest value; min = max = 0 is unbounded. (-1e+09 to 1e+09) |
| `max` | number | `0` | Largest value. (-1e+09 to 1e+09) |
| `format` | text | `"%d"` | printf format of the low value. |
| `format_max` | text | `""` | printf format of the high value; empty is format. |
| `flags` | flags | `None` | ImGuiSliderFlags without the prefix. (combine with '\|': None, Logarithmic, NoRoundToFormat, NoInput, WrapAround, ClampOnInput, ClampZeroRange, NoSpeedTweaks, AlwaysClamp) |
| `width` | number | `0` | Item width in pixels (ImGui's SetNextItemWidth): 0 is ImGui's default, a positive value fixed pixels, a negative value stops that many pixels before the container's right edge (-1 reaches it). (-4096 to 4096) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## Dummy

ImGui::Dummy: empty space of a given size.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `size` | list of numbers | `[]` | Size [width, height] of the empty space, pixels. |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## Group

ImGui::BeginGroup: its children act as one item, so a SameLine after it puts the next widget beside the whole group.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window. Children: ArrowButton, Bullet, BulletText, Button, Checkbox, Child, CollapsingHeader, ColorButton, ColorEdit3, ColorEdit4, ColorPicker3, ColorPicker4, Combo, Disabled, DragFloat, DragFloat2, DragFloat3, DragFloat4, DragFloatRange2, DragInt, DragInt2, DragInt3, DragInt4, DragIntRange2, Dummy, Group, Image, ImageButton, Indent, InputDouble, InputFloat, InputFloat2, InputFloat3, InputFloat4, InputInt, InputInt2, InputInt3, InputInt4, InputText, InputTextMultiline, InputTextWithHint, LabelText, ListBox, NewLine, ProgressBar, RadioButton, SameLine, Selectable, Separator, SeparatorText, SliderAngle, SliderFloat, SliderFloat2, SliderFloat3, SliderFloat4, SliderInt, SliderInt2, SliderInt3, SliderInt4, SmallButton, Spacing, TabBar, Table, Text, TextColored, TextDisabled, TextLink, TextWrapped, Tooltip, TreeNode, VSliderFloat, VSliderInt, ActivityDiagram, ClassDiagram, ColorScale, ComponentDiagram, DataFlowDiagram, DeploymentDiagram, ERDiagram, Flowchart, GanttChart, HorizontalLayout, Label, MindMap, NetworkDiagram, ObjectDiagram, OrgChart, PackageDiagram, Plot, Plot3D, SankeyDiagram, Screen, SequenceDiagram, Shape, Spring, StateDiagram, TreeDiagram, UseCaseDiagram, VerticalLayout, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## ImageButton

ImGui::ImageButton with a built-in placeholder image: a button showing the image inside its frame.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | ID of the button (not shown); "##" as in ImGui labels. |
| `image` | name | `cross` | Which built-in placeholder image to show (`nymph schema Image` lists them; docs/examples.md shows them). (one of: building, chart, cross, document, icon, landscape, logo, map, people, person, product, video) |
| `width` | number | `64` | Width in pixels; in a window a negative value reaches that many pixels before the right edge (-1 fills); in a layout the usual size hint. (-1000 to 8192) |
| `height` | number | `64` | Height in pixels; in a window a negative value fills the height left; in a layout the usual size hint. (-1000 to 8192) |
| `fit` | name | `contain` | contain keeps the aspect ratio and shows the whole image; cover fills the box and clips; stretch fills it in both directions. (one of: contain, cover, stretch) |
| `tint` | colour | `""` | Colour of the image's shapes; its half tones use it at half alpha. Empty: the theme's disabled text colour. |
| `background` | colour | `""` | Colour of the box behind the image and of its cut-outs; "#00000000" is transparent. Empty: the theme's frame colour. |
| `rounding` | number | `0` | Corner radius of the box; half its size gives a circle. (0 to 4096) |
| `border` | true/false | `false` | Outlines the box in the theme's border colour. |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## Indent

ImGui::Indent: its children start further right.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window. Children: ArrowButton, Bullet, BulletText, Button, Checkbox, Child, CollapsingHeader, ColorButton, ColorEdit3, ColorEdit4, ColorPicker3, ColorPicker4, Combo, Disabled, DragFloat, DragFloat2, DragFloat3, DragFloat4, DragFloatRange2, DragInt, DragInt2, DragInt3, DragInt4, DragIntRange2, Dummy, Group, Image, ImageButton, Indent, InputDouble, InputFloat, InputFloat2, InputFloat3, InputFloat4, InputInt, InputInt2, InputInt3, InputInt4, InputText, InputTextMultiline, InputTextWithHint, LabelText, ListBox, NewLine, ProgressBar, RadioButton, SameLine, Selectable, Separator, SeparatorText, SliderAngle, SliderFloat, SliderFloat2, SliderFloat3, SliderFloat4, SliderInt, SliderInt2, SliderInt3, SliderInt4, SmallButton, Spacing, TabBar, Table, Text, TextColored, TextDisabled, TextLink, TextWrapped, Tooltip, TreeNode, VSliderFloat, VSliderInt, ActivityDiagram, ClassDiagram, ColorScale, ComponentDiagram, DataFlowDiagram, DeploymentDiagram, ERDiagram, Flowchart, GanttChart, HorizontalLayout, Label, MindMap, NetworkDiagram, ObjectDiagram, OrgChart, PackageDiagram, Plot, Plot3D, SankeyDiagram, Screen, SequenceDiagram, Shape, Spring, StateDiagram, TreeDiagram, UseCaseDiagram, VerticalLayout, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `width` | number | `0` | Indent in pixels; 0 is the style's IndentSpacing. (0 to 1024) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## InputDouble

ImGui an input box for one number.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text shown beside the widget; "##" hides the rest (ImGui labels). |
| `value` | number | `0` | The value. (-1e+30 to 1e+30) |
| `step` | number | `0` | Step of the - and + buttons; 0 hides them. (0 to 1e+09) |
| `step_fast` | number | `0` | Step with Ctrl held; 0 for none. (0 to 1e+09) |
| `format` | text | `"%.6f"` | printf format of the shown value: one %f, %e or %g (like "%.1f ms"). |
| `flags` | flags | `None` | ImGuiInputTextFlags without the prefix. (combine with '\|': None, CharsDecimal, CharsHexadecimal, CharsScientific, CharsUppercase, CharsNoBlank, AllowTabInput, EnterReturnsTrue, EscapeClearsAll, CtrlEnterForNewLine, ReadOnly, Password, AlwaysOverwrite, AutoSelectAll, ParseEmptyRefVal, DisplayEmptyRefVal, NoHorizontalScroll, NoUndoRedo) |
| `width` | number | `0` | Item width in pixels (ImGui's SetNextItemWidth): 0 is ImGui's default, a positive value fixed pixels, a negative value stops that many pixels before the container's right edge (-1 reaches it). (-4096 to 4096) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## InputFloat

ImGui an input box for one number.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text shown beside the widget; "##" hides the rest (ImGui labels). |
| `value` | number | `0` | The value. (-1e+30 to 1e+30) |
| `step` | number | `0` | Step of the - and + buttons; 0 hides them. (0 to 1e+09) |
| `step_fast` | number | `0` | Step with Ctrl held; 0 for none. (0 to 1e+09) |
| `format` | text | `"%.3f"` | printf format of the shown value: one %f, %e or %g (like "%.1f ms"). |
| `flags` | flags | `None` | ImGuiInputTextFlags without the prefix. (combine with '\|': None, CharsDecimal, CharsHexadecimal, CharsScientific, CharsUppercase, CharsNoBlank, AllowTabInput, EnterReturnsTrue, EscapeClearsAll, CtrlEnterForNewLine, ReadOnly, Password, AlwaysOverwrite, AutoSelectAll, ParseEmptyRefVal, DisplayEmptyRefVal, NoHorizontalScroll, NoUndoRedo) |
| `width` | number | `0` | Item width in pixels (ImGui's SetNextItemWidth): 0 is ImGui's default, a positive value fixed pixels, a negative value stops that many pixels before the container's right edge (-1 reaches it). (-4096 to 4096) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## InputFloat2

ImGui an input box for 2 numbers.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text shown beside the widget; "##" hides the rest (ImGui labels). |
| `value` | list of numbers | `[]` | The 2 values. |
| `format` | text | `"%.3f"` | printf format of the shown value: one %f, %e or %g (like "%.1f ms"). |
| `flags` | flags | `None` | ImGuiInputTextFlags without the prefix. (combine with '\|': None, CharsDecimal, CharsHexadecimal, CharsScientific, CharsUppercase, CharsNoBlank, AllowTabInput, EnterReturnsTrue, EscapeClearsAll, CtrlEnterForNewLine, ReadOnly, Password, AlwaysOverwrite, AutoSelectAll, ParseEmptyRefVal, DisplayEmptyRefVal, NoHorizontalScroll, NoUndoRedo) |
| `width` | number | `0` | Item width in pixels (ImGui's SetNextItemWidth): 0 is ImGui's default, a positive value fixed pixels, a negative value stops that many pixels before the container's right edge (-1 reaches it). (-4096 to 4096) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## InputFloat3

ImGui an input box for 3 numbers.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text shown beside the widget; "##" hides the rest (ImGui labels). |
| `value` | list of numbers | `[]` | The 3 values. |
| `format` | text | `"%.3f"` | printf format of the shown value: one %f, %e or %g (like "%.1f ms"). |
| `flags` | flags | `None` | ImGuiInputTextFlags without the prefix. (combine with '\|': None, CharsDecimal, CharsHexadecimal, CharsScientific, CharsUppercase, CharsNoBlank, AllowTabInput, EnterReturnsTrue, EscapeClearsAll, CtrlEnterForNewLine, ReadOnly, Password, AlwaysOverwrite, AutoSelectAll, ParseEmptyRefVal, DisplayEmptyRefVal, NoHorizontalScroll, NoUndoRedo) |
| `width` | number | `0` | Item width in pixels (ImGui's SetNextItemWidth): 0 is ImGui's default, a positive value fixed pixels, a negative value stops that many pixels before the container's right edge (-1 reaches it). (-4096 to 4096) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## InputFloat4

ImGui an input box for 4 numbers.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text shown beside the widget; "##" hides the rest (ImGui labels). |
| `value` | list of numbers | `[]` | The 4 values. |
| `format` | text | `"%.3f"` | printf format of the shown value: one %f, %e or %g (like "%.1f ms"). |
| `flags` | flags | `None` | ImGuiInputTextFlags without the prefix. (combine with '\|': None, CharsDecimal, CharsHexadecimal, CharsScientific, CharsUppercase, CharsNoBlank, AllowTabInput, EnterReturnsTrue, EscapeClearsAll, CtrlEnterForNewLine, ReadOnly, Password, AlwaysOverwrite, AutoSelectAll, ParseEmptyRefVal, DisplayEmptyRefVal, NoHorizontalScroll, NoUndoRedo) |
| `width` | number | `0` | Item width in pixels (ImGui's SetNextItemWidth): 0 is ImGui's default, a positive value fixed pixels, a negative value stops that many pixels before the container's right edge (-1 reaches it). (-4096 to 4096) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## InputInt

ImGui an input box for one integer.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text shown beside the widget; "##" hides the rest (ImGui labels). |
| `value` | integer | `0` | The value. (-1e+09 to 1e+09) |
| `step` | number | `1` | Step of the - and + buttons; 0 hides them. (0 to 1e+09) |
| `step_fast` | number | `100` | Step with Ctrl held; 0 for none. (0 to 1e+09) |
| `flags` | flags | `None` | ImGuiInputTextFlags without the prefix. (combine with '\|': None, CharsDecimal, CharsHexadecimal, CharsScientific, CharsUppercase, CharsNoBlank, AllowTabInput, EnterReturnsTrue, EscapeClearsAll, CtrlEnterForNewLine, ReadOnly, Password, AlwaysOverwrite, AutoSelectAll, ParseEmptyRefVal, DisplayEmptyRefVal, NoHorizontalScroll, NoUndoRedo) |
| `width` | number | `0` | Item width in pixels (ImGui's SetNextItemWidth): 0 is ImGui's default, a positive value fixed pixels, a negative value stops that many pixels before the container's right edge (-1 reaches it). (-4096 to 4096) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## InputInt2

ImGui an input box for 2 integers.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text shown beside the widget; "##" hides the rest (ImGui labels). |
| `value` | list of integers | `[]` | The 2 values. |
| `flags` | flags | `None` | ImGuiInputTextFlags without the prefix. (combine with '\|': None, CharsDecimal, CharsHexadecimal, CharsScientific, CharsUppercase, CharsNoBlank, AllowTabInput, EnterReturnsTrue, EscapeClearsAll, CtrlEnterForNewLine, ReadOnly, Password, AlwaysOverwrite, AutoSelectAll, ParseEmptyRefVal, DisplayEmptyRefVal, NoHorizontalScroll, NoUndoRedo) |
| `width` | number | `0` | Item width in pixels (ImGui's SetNextItemWidth): 0 is ImGui's default, a positive value fixed pixels, a negative value stops that many pixels before the container's right edge (-1 reaches it). (-4096 to 4096) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## InputInt3

ImGui an input box for 3 integers.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text shown beside the widget; "##" hides the rest (ImGui labels). |
| `value` | list of integers | `[]` | The 3 values. |
| `flags` | flags | `None` | ImGuiInputTextFlags without the prefix. (combine with '\|': None, CharsDecimal, CharsHexadecimal, CharsScientific, CharsUppercase, CharsNoBlank, AllowTabInput, EnterReturnsTrue, EscapeClearsAll, CtrlEnterForNewLine, ReadOnly, Password, AlwaysOverwrite, AutoSelectAll, ParseEmptyRefVal, DisplayEmptyRefVal, NoHorizontalScroll, NoUndoRedo) |
| `width` | number | `0` | Item width in pixels (ImGui's SetNextItemWidth): 0 is ImGui's default, a positive value fixed pixels, a negative value stops that many pixels before the container's right edge (-1 reaches it). (-4096 to 4096) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## InputInt4

ImGui an input box for 4 integers.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text shown beside the widget; "##" hides the rest (ImGui labels). |
| `value` | list of integers | `[]` | The 4 values. |
| `flags` | flags | `None` | ImGuiInputTextFlags without the prefix. (combine with '\|': None, CharsDecimal, CharsHexadecimal, CharsScientific, CharsUppercase, CharsNoBlank, AllowTabInput, EnterReturnsTrue, EscapeClearsAll, CtrlEnterForNewLine, ReadOnly, Password, AlwaysOverwrite, AutoSelectAll, ParseEmptyRefVal, DisplayEmptyRefVal, NoHorizontalScroll, NoUndoRedo) |
| `width` | number | `0` | Item width in pixels (ImGui's SetNextItemWidth): 0 is ImGui's default, a positive value fixed pixels, a negative value stops that many pixels before the container's right edge (-1 reaches it). (-4096 to 4096) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## InputText

ImGui::InputText: a one-line text box.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text shown beside the widget; "##" hides the rest (ImGui labels). |
| `value` | text | `""` | The text in the box. |
| `flags` | flags | `None` | ImGuiInputTextFlags without the prefix. (combine with '\|': None, CharsDecimal, CharsHexadecimal, CharsScientific, CharsUppercase, CharsNoBlank, AllowTabInput, EnterReturnsTrue, EscapeClearsAll, CtrlEnterForNewLine, ReadOnly, Password, AlwaysOverwrite, AutoSelectAll, ParseEmptyRefVal, DisplayEmptyRefVal, NoHorizontalScroll, NoUndoRedo) |
| `width` | number | `0` | Item width in pixels (ImGui's SetNextItemWidth): 0 is ImGui's default, a positive value fixed pixels, a negative value stops that many pixels before the container's right edge (-1 reaches it). (-4096 to 4096) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## InputTextMultiline

ImGui::InputTextMultiline: a text box of several lines.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text shown beside the widget; "##" hides the rest (ImGui labels). |
| `value` | text | `""` | The text in the box. |
| `size` | list of numbers | `[]` | Size [width, height] in pixels; 0 on an axis is ImGui's default. |
| `flags` | flags | `None` | ImGuiInputTextFlags without the prefix. (combine with '\|': None, CharsDecimal, CharsHexadecimal, CharsScientific, CharsUppercase, CharsNoBlank, AllowTabInput, EnterReturnsTrue, EscapeClearsAll, CtrlEnterForNewLine, ReadOnly, Password, AlwaysOverwrite, AutoSelectAll, ParseEmptyRefVal, DisplayEmptyRefVal, NoHorizontalScroll, NoUndoRedo) |
| `width` | number | `0` | Item width in pixels (ImGui's SetNextItemWidth): 0 is ImGui's default, a positive value fixed pixels, a negative value stops that many pixels before the container's right edge (-1 reaches it). (-4096 to 4096) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## InputTextWithHint

ImGui::InputTextWithHint: a one-line text box showing a hint while empty.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text shown beside the widget; "##" hides the rest (ImGui labels). |
| `value` | text | `""` | The text in the box. |
| `hint` | text | `""` | Grey text shown while the box is empty. |
| `flags` | flags | `None` | ImGuiInputTextFlags without the prefix. (combine with '\|': None, CharsDecimal, CharsHexadecimal, CharsScientific, CharsUppercase, CharsNoBlank, AllowTabInput, EnterReturnsTrue, EscapeClearsAll, CtrlEnterForNewLine, ReadOnly, Password, AlwaysOverwrite, AutoSelectAll, ParseEmptyRefVal, DisplayEmptyRefVal, NoHorizontalScroll, NoUndoRedo) |
| `width` | number | `0` | Item width in pixels (ImGui's SetNextItemWidth): 0 is ImGui's default, a positive value fixed pixels, a negative value stops that many pixels before the container's right edge (-1 reaches it). (-4096 to 4096) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## LabelText

ImGui::LabelText: a read-only value with a label, aligned like other widgets.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text on the right, like a widget's label. |
| `text` | text | `""` | The value shown on the left. |
| `width` | number | `0` | Item width in pixels (ImGui's SetNextItemWidth): 0 is ImGui's default, a positive value fixed pixels, a negative value stops that many pixels before the container's right edge (-1 reaches it). (-4096 to 4096) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## ListBox

ImGui::ListBox: a framed list of items, one selected.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text shown on the widget; "##" hides the rest (ImGui labels). |
| `items` | list of texts | `[]` | The items, or a $column whose cells are the items. |
| `selected` | integer | `-1` | Index of the selected item, from 0; -1 for none. (-1 to 1e+06) |
| `height_in_items` | integer | `-1` | Rows shown before scrolling; -1 is ImGui's default. (-1 to 1000) |
| `width` | number | `0` | Item width in pixels (ImGui's SetNextItemWidth): 0 is ImGui's default, a positive value fixed pixels, a negative value stops that many pixels before the container's right edge (-1 reaches it). (-4096 to 4096) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## NewLine

ImGui::NewLine: ends a line of SameLine widgets, or adds an empty line.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## ProgressBar

ImGui::ProgressBar: a bar filled to a fraction.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `fraction` | number | `0` | How far along, 0 to 1. (0 to 1) |
| `size` | list of numbers | `[]` | Size [width, height] in pixels; a negative width fills the row less that many pixels, empty fills the row at the frame height. |
| `overlay` | text | `""` | Text on the bar; empty shows the percentage. |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## RadioButton

ImGui::RadioButton: one choice of several. In a live view a click chooses it and clears the other RadioButtons of the same parent.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text shown on the widget; "##" hides the rest (ImGui labels). |
| `active` | true/false | `false` | Whether this button is the chosen one. |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## SameLine

ImGui::SameLine: the next widget goes on the same line as the previous one.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `offset` | number | `0` | Position from the left of the window; 0 follows the previous widget. (0 to 8192) |
| `spacing` | number | `-1` | Space after the previous widget; -1 is the style's. (-1 to 1024) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## Selectable

ImGui::Selectable: a line of text that highlights when selected.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text shown on the widget; "##" hides the rest (ImGui labels). |
| `selected` | true/false | `false` | Whether it is highlighted as selected. |
| `flags` | flags | `None` | ImGuiSelectableFlags without the prefix. (combine with '\|': None, NoAutoClosePopups, SpanAllColumns, AllowDoubleClick, Disabled, AllowOverlap, Highlight) |
| `size` | list of numbers | `[]` | Size [width, height] in pixels; 0 on an axis fills the row or fits the text. |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## SliderAngle

ImGui::SliderAngle: a slider for an angle, shown in degrees.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text shown beside the widget; "##" hides the rest (ImGui labels). |
| `value` | number | `0` | The angle in degrees. (-1e+06 to 1e+06) |
| `min` | number | `-360` | Smallest angle, degrees. (-1e+06 to 1e+06) |
| `max` | number | `360` | Largest angle, degrees. (-1e+06 to 1e+06) |
| `format` | text | `"%.0f deg"` | printf format of the shown degrees. |
| `flags` | flags | `None` | ImGuiSliderFlags without the prefix. (combine with '\|': None, Logarithmic, NoRoundToFormat, NoInput, WrapAround, ClampOnInput, ClampZeroRange, NoSpeedTweaks, AlwaysClamp) |
| `width` | number | `0` | Item width in pixels (ImGui's SetNextItemWidth): 0 is ImGui's default, a positive value fixed pixels, a negative value stops that many pixels before the container's right edge (-1 reaches it). (-4096 to 4096) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## SliderFloat

ImGui a slider for one number.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text shown beside the widget; "##" hides the rest (ImGui labels). |
| `value` | number | `0` | The value. (-1e+30 to 1e+30) |
| `min` | number | `0` | Smallest value of the slider. (-1e+30 to 1e+30) |
| `max` | number | `1` | Largest value. (-1e+30 to 1e+30) |
| `format` | text | `"%.3f"` | printf format of the shown value: one %f, %e or %g (like "%.1f ms"). |
| `flags` | flags | `None` | ImGuiSliderFlags without the prefix, like AlwaysClamp. (combine with '\|': None, Logarithmic, NoRoundToFormat, NoInput, WrapAround, ClampOnInput, ClampZeroRange, NoSpeedTweaks, AlwaysClamp) |
| `width` | number | `0` | Item width in pixels (ImGui's SetNextItemWidth): 0 is ImGui's default, a positive value fixed pixels, a negative value stops that many pixels before the container's right edge (-1 reaches it). (-4096 to 4096) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## SliderFloat2

ImGui a slider for 2 numbers.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text shown beside the widget; "##" hides the rest (ImGui labels). |
| `value` | list of numbers | `[]` | The 2 values. |
| `min` | number | `0` | Smallest value of the slider. (-1e+30 to 1e+30) |
| `max` | number | `1` | Largest value. (-1e+30 to 1e+30) |
| `format` | text | `"%.3f"` | printf format of the shown value: one %f, %e or %g (like "%.1f ms"). |
| `flags` | flags | `None` | ImGuiSliderFlags without the prefix, like AlwaysClamp. (combine with '\|': None, Logarithmic, NoRoundToFormat, NoInput, WrapAround, ClampOnInput, ClampZeroRange, NoSpeedTweaks, AlwaysClamp) |
| `width` | number | `0` | Item width in pixels (ImGui's SetNextItemWidth): 0 is ImGui's default, a positive value fixed pixels, a negative value stops that many pixels before the container's right edge (-1 reaches it). (-4096 to 4096) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## SliderFloat3

ImGui a slider for 3 numbers.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text shown beside the widget; "##" hides the rest (ImGui labels). |
| `value` | list of numbers | `[]` | The 3 values. |
| `min` | number | `0` | Smallest value of the slider. (-1e+30 to 1e+30) |
| `max` | number | `1` | Largest value. (-1e+30 to 1e+30) |
| `format` | text | `"%.3f"` | printf format of the shown value: one %f, %e or %g (like "%.1f ms"). |
| `flags` | flags | `None` | ImGuiSliderFlags without the prefix, like AlwaysClamp. (combine with '\|': None, Logarithmic, NoRoundToFormat, NoInput, WrapAround, ClampOnInput, ClampZeroRange, NoSpeedTweaks, AlwaysClamp) |
| `width` | number | `0` | Item width in pixels (ImGui's SetNextItemWidth): 0 is ImGui's default, a positive value fixed pixels, a negative value stops that many pixels before the container's right edge (-1 reaches it). (-4096 to 4096) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## SliderFloat4

ImGui a slider for 4 numbers.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text shown beside the widget; "##" hides the rest (ImGui labels). |
| `value` | list of numbers | `[]` | The 4 values. |
| `min` | number | `0` | Smallest value of the slider. (-1e+30 to 1e+30) |
| `max` | number | `1` | Largest value. (-1e+30 to 1e+30) |
| `format` | text | `"%.3f"` | printf format of the shown value: one %f, %e or %g (like "%.1f ms"). |
| `flags` | flags | `None` | ImGuiSliderFlags without the prefix, like AlwaysClamp. (combine with '\|': None, Logarithmic, NoRoundToFormat, NoInput, WrapAround, ClampOnInput, ClampZeroRange, NoSpeedTweaks, AlwaysClamp) |
| `width` | number | `0` | Item width in pixels (ImGui's SetNextItemWidth): 0 is ImGui's default, a positive value fixed pixels, a negative value stops that many pixels before the container's right edge (-1 reaches it). (-4096 to 4096) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## SliderInt

ImGui a slider for one integer.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text shown beside the widget; "##" hides the rest (ImGui labels). |
| `value` | integer | `0` | The value. (-1e+09 to 1e+09) |
| `min` | integer | `0` | Smallest value of the slider. (-1e+09 to 1e+09) |
| `max` | integer | `100` | Largest value. (-1e+09 to 1e+09) |
| `format` | text | `"%d"` | printf format of the shown value: one %d (like "%d px"). |
| `flags` | flags | `None` | ImGuiSliderFlags without the prefix, like AlwaysClamp. (combine with '\|': None, Logarithmic, NoRoundToFormat, NoInput, WrapAround, ClampOnInput, ClampZeroRange, NoSpeedTweaks, AlwaysClamp) |
| `width` | number | `0` | Item width in pixels (ImGui's SetNextItemWidth): 0 is ImGui's default, a positive value fixed pixels, a negative value stops that many pixels before the container's right edge (-1 reaches it). (-4096 to 4096) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## SliderInt2

ImGui a slider for 2 integers.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text shown beside the widget; "##" hides the rest (ImGui labels). |
| `value` | list of integers | `[]` | The 2 values. |
| `min` | integer | `0` | Smallest value of the slider. (-1e+09 to 1e+09) |
| `max` | integer | `100` | Largest value. (-1e+09 to 1e+09) |
| `format` | text | `"%d"` | printf format of the shown value: one %d (like "%d px"). |
| `flags` | flags | `None` | ImGuiSliderFlags without the prefix, like AlwaysClamp. (combine with '\|': None, Logarithmic, NoRoundToFormat, NoInput, WrapAround, ClampOnInput, ClampZeroRange, NoSpeedTweaks, AlwaysClamp) |
| `width` | number | `0` | Item width in pixels (ImGui's SetNextItemWidth): 0 is ImGui's default, a positive value fixed pixels, a negative value stops that many pixels before the container's right edge (-1 reaches it). (-4096 to 4096) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## SliderInt3

ImGui a slider for 3 integers.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text shown beside the widget; "##" hides the rest (ImGui labels). |
| `value` | list of integers | `[]` | The 3 values. |
| `min` | integer | `0` | Smallest value of the slider. (-1e+09 to 1e+09) |
| `max` | integer | `100` | Largest value. (-1e+09 to 1e+09) |
| `format` | text | `"%d"` | printf format of the shown value: one %d (like "%d px"). |
| `flags` | flags | `None` | ImGuiSliderFlags without the prefix, like AlwaysClamp. (combine with '\|': None, Logarithmic, NoRoundToFormat, NoInput, WrapAround, ClampOnInput, ClampZeroRange, NoSpeedTweaks, AlwaysClamp) |
| `width` | number | `0` | Item width in pixels (ImGui's SetNextItemWidth): 0 is ImGui's default, a positive value fixed pixels, a negative value stops that many pixels before the container's right edge (-1 reaches it). (-4096 to 4096) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## SliderInt4

ImGui a slider for 4 integers.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text shown beside the widget; "##" hides the rest (ImGui labels). |
| `value` | list of integers | `[]` | The 4 values. |
| `min` | integer | `0` | Smallest value of the slider. (-1e+09 to 1e+09) |
| `max` | integer | `100` | Largest value. (-1e+09 to 1e+09) |
| `format` | text | `"%d"` | printf format of the shown value: one %d (like "%d px"). |
| `flags` | flags | `None` | ImGuiSliderFlags without the prefix, like AlwaysClamp. (combine with '\|': None, Logarithmic, NoRoundToFormat, NoInput, WrapAround, ClampOnInput, ClampZeroRange, NoSpeedTweaks, AlwaysClamp) |
| `width` | number | `0` | Item width in pixels (ImGui's SetNextItemWidth): 0 is ImGui's default, a positive value fixed pixels, a negative value stops that many pixels before the container's right edge (-1 reaches it). (-4096 to 4096) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## SmallButton

ImGui::SmallButton: a button without vertical padding.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text shown on the widget; "##" hides the rest (ImGui labels). |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## Spacing

ImGui::Spacing: a little vertical space.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## TabBar

ImGui::BeginTabBar: a row of tabs (TabItem children); the selected tab shows its children.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window. Children: TabItem, TabItemButton.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | ID of the tab bar (not shown). |
| `flags` | flags | `None` | ImGuiTabBarFlags without the prefix, like Reorderable. (combine with '\|': None, Reorderable, AutoSelectNewTabs, TabListPopupButton, NoCloseWithMiddleMouseButton, NoTabListScrollingButtons, NoTooltip, DrawSelectedOverline, FittingPolicyMixed, FittingPolicyShrink, FittingPolicyScroll) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## Table

ImGui::BeginTable: TableColumn children define the columns (and their text cells, often $columns); TableRow children add rows of widgets after them.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window. Children: TableColumn, TableRow.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | ID of the table (not shown). |
| `flags` | flags | `None` | ImGuiTableFlags without the prefix, like Borders\|RowBg. (combine with '\|': None, Resizable, Reorderable, Hideable, Sortable, ContextMenuInBody, RowBg, BordersInnerH, BordersOuterH, BordersInnerV, BordersOuterV, NoBordersInBody, NoBordersInBodyUntilResize, SizingFixedFit, SizingFixedSame, SizingStretchProp, SizingStretchSame, NoHostExtendX, NoHostExtendY, NoKeepColumnsVisible, PreciseWidths, NoClip, PadOuterX, NoPadOuterX, NoPadInnerX, ScrollX, ScrollY, SortMulti, SortTristate, HighlightHoveredColumn, BordersH, BordersV, BordersInner, BordersOuter, Borders) |
| `freeze` | list of integers | `[]` | Columns and rows [columns, rows] that stay in view while scrolling; empty for none. |
| `width` | number | `0` | Outer width in pixels; 0 fills the row. (0 to 8192) |
| `height` | number | `0` | Outer height in pixels; 0 fits the rows (with ScrollY: fills the space left). (0 to 8192) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## Text

ImGui::Text: a line of text.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `text` | text | `""` | The text, shown as written (never a printf format). |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## TextColored

ImGui::TextColored: text in a colour.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `text` | text | `""` | The text, shown as written (never a printf format). |
| `color` | colour | `""` | Colour of the text; empty is the theme's. |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## TextDisabled

ImGui::TextDisabled: greyed-out text.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `text` | text | `""` | The text, shown as written (never a printf format). |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## TextLink

ImGui::TextLink: underlined text that can be clicked; it reports a click and never opens a URL.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | The link text. |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## TextWrapped

ImGui::TextWrapped: text wrapped at the window's edge.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `text` | text | `""` | The text, shown as written (never a printf format). |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## Tooltip

ImGui::BeginItemTooltip: a tooltip window for the widget just before it, holding any widgets.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window. Children: ArrowButton, Bullet, BulletText, Button, Checkbox, Child, CollapsingHeader, ColorButton, ColorEdit3, ColorEdit4, ColorPicker3, ColorPicker4, Combo, Disabled, DragFloat, DragFloat2, DragFloat3, DragFloat4, DragFloatRange2, DragInt, DragInt2, DragInt3, DragInt4, DragIntRange2, Dummy, Group, Image, ImageButton, Indent, InputDouble, InputFloat, InputFloat2, InputFloat3, InputFloat4, InputInt, InputInt2, InputInt3, InputInt4, InputText, InputTextMultiline, InputTextWithHint, LabelText, ListBox, NewLine, ProgressBar, RadioButton, SameLine, Selectable, Separator, SeparatorText, SliderAngle, SliderFloat, SliderFloat2, SliderFloat3, SliderFloat4, SliderInt, SliderInt2, SliderInt3, SliderInt4, SmallButton, Spacing, TabBar, Table, Text, TextColored, TextDisabled, TextLink, TextWrapped, Tooltip, TreeNode, VSliderFloat, VSliderInt, ActivityDiagram, ClassDiagram, ColorScale, ComponentDiagram, DataFlowDiagram, DeploymentDiagram, ERDiagram, Flowchart, GanttChart, HorizontalLayout, Label, MindMap, NetworkDiagram, ObjectDiagram, OrgChart, PackageDiagram, Plot, Plot3D, SankeyDiagram, Screen, SequenceDiagram, Shape, Spring, StateDiagram, TreeDiagram, UseCaseDiagram, VerticalLayout, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `open` | true/false | `false` | Shows the tooltip (in the PNG too); else it shows while the widget before it is hovered. |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## TreeNode

ImGui::TreeNode: an expandable node of a tree; its children are indented under it.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window. Children: ArrowButton, Bullet, BulletText, Button, Checkbox, Child, CollapsingHeader, ColorButton, ColorEdit3, ColorEdit4, ColorPicker3, ColorPicker4, Combo, Disabled, DragFloat, DragFloat2, DragFloat3, DragFloat4, DragFloatRange2, DragInt, DragInt2, DragInt3, DragInt4, DragIntRange2, Dummy, Group, Image, ImageButton, Indent, InputDouble, InputFloat, InputFloat2, InputFloat3, InputFloat4, InputInt, InputInt2, InputInt3, InputInt4, InputText, InputTextMultiline, InputTextWithHint, LabelText, ListBox, NewLine, ProgressBar, RadioButton, SameLine, Selectable, Separator, SeparatorText, SliderAngle, SliderFloat, SliderFloat2, SliderFloat3, SliderFloat4, SliderInt, SliderInt2, SliderInt3, SliderInt4, SmallButton, Spacing, TabBar, Table, Text, TextColored, TextDisabled, TextLink, TextWrapped, Tooltip, TreeNode, VSliderFloat, VSliderInt, ActivityDiagram, ClassDiagram, ColorScale, ComponentDiagram, DataFlowDiagram, DeploymentDiagram, ERDiagram, Flowchart, GanttChart, HorizontalLayout, Label, MindMap, NetworkDiagram, ObjectDiagram, OrgChart, PackageDiagram, Plot, Plot3D, SankeyDiagram, Screen, SequenceDiagram, Shape, Spring, StateDiagram, TreeDiagram, UseCaseDiagram, VerticalLayout, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text of the header; "##" hides the rest (ImGui labels). |
| `open` | true/false | `false` | Whether it is expanded, showing its children. |
| `flags` | flags | `None` | ImGuiTreeNodeFlags without the prefix, like Leaf\|Bullet; DefaultOpen opens it when open is not written. (combine with '\|': None, Selected, Framed, DefaultOpen, AllowOverlap, NoTreePushOnOpen, NoAutoOpenOnLog, OpenOnDoubleClick, OpenOnArrow, Leaf, Bullet, FramePadding, SpanAvailWidth, SpanFullWidth, SpanLabelWidth, SpanAllColumns, LabelSpanAllColumns, DrawLinesNone, DrawLinesFull, DrawLinesToNodes, CollapsingHeader) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## VSliderFloat

ImGui::VSliderFloat: a vertical slider for a number.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text shown beside the widget; "##" hides the rest (ImGui labels). |
| `size` | list of numbers | `[]` | Size [width, height] in pixels; empty is [18, 160]. |
| `value` | number | `0` | The value. (-1e+30 to 1e+30) |
| `min` | number | `0` | Smallest value. (-1e+30 to 1e+30) |
| `max` | number | `1` | Largest value. (-1e+30 to 1e+30) |
| `format` | text | `"%.3f"` | printf format of the shown value. |
| `flags` | flags | `None` | ImGuiSliderFlags without the prefix. (combine with '\|': None, Logarithmic, NoRoundToFormat, NoInput, WrapAround, ClampOnInput, ClampZeroRange, NoSpeedTweaks, AlwaysClamp) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |

## VSliderInt

ImGui::VSliderInt: a vertical slider for an integer.

Goes in: Child, CollapsingHeader, Disabled, Group, Indent, MainMenuBar, Menu, MenuBar, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `label` | text | `""` | Text shown beside the widget; "##" hides the rest (ImGui labels). |
| `size` | list of numbers | `[]` | Size [width, height] in pixels; empty is [18, 160]. |
| `value` | integer | `0` | The value. (-1e+09 to 1e+09) |
| `min` | integer | `0` | Smallest value. (-1e+09 to 1e+09) |
| `max` | integer | `100` | Largest value. (-1e+09 to 1e+09) |
| `format` | text | `"%d"` | printf format of the shown value. |
| `flags` | flags | `None` | ImGuiSliderFlags without the prefix. (combine with '\|': None, Logarithmic, NoRoundToFormat, NoInput, WrapAround, ClampOnInput, ClampZeroRange, NoSpeedTweaks, AlwaysClamp) |
| `disabled` | true/false | `false` | Draws the widget greyed out and ignores input (BeginDisabled). |
| `tooltip` | text | `""` | Text shown while the pointer is over the widget in a live view (SetItemTooltip); never in a PNG. |
| `opens` | text | `""` | The id of a Popup or PopupModal on the same Screen that a click on this widget opens in a live view. |
| `font` | text | `""` | Font of this element's text and of everything inside it: an embedded font file name (default, FiraMono-Bold.ttf, FiraMono-Regular.ttf, FiraSans-Bold.ttf, FiraSans-Italic.ttf, FiraSans-Regular.ttf, PT_Serif-Web-Bold.ttf, PT_Serif-Web-Italic.ttf, PT_Serif-Web-Regular.ttf). Empty (default) inherits the enclosing element's font, or image.font. |
| `font_size` | number | `0` | Text size in pixels for this element and everything inside it. 0 (default) inherits the enclosing element's size, or image.font_size. (0 to 96) |
| `text_color` | colour | `""` | Text colour ("#RRGGBB" or "#RRGGBBAA") for this element and everything inside it: titles, axis and tick labels, legends and labels. Empty (default) inherits, or uses the theme's. |
