# Spring (nymph element reference)

Part of the [nymph element reference](../elements.md), which has the
conventions these tables follow.

## Spring

Expandable, invisible space that shares leftover room with sibling stretch children.

Goes in: root, Child, CollapsingHeader, Disabled, Group, HorizontalLayout, Indent, Popup, PopupModal, TabItem, TableRow, Tooltip, TreeNode, VerticalLayout, Window.

| Field | Type | Default | Description |
|---|---|---|---|
| `weight` | number | `1` | Expandable, invisible space in a HorizontalLayout (width) or VerticalLayout (height): shares the space left over with the other stretch children, in proportion to this weight. Two Springs around an element centre it; one between two elements pushes them to opposite edges. 0 collapses it. (0 to 10000) |
