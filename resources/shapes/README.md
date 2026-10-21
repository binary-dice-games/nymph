# Shape library

The shapes `Shape`, `FlowNode.symbol` and the `shape` field of diagram nodes
name (`nymph shapes` lists them, `docs/shapes.md` shows them), compiled into
nymph by `cmake/NymphShapes.cmake`. A shape is one SVG file,
`resources/shapes/<category>/<name>.svg`; the name is the file name (lower
case letters, digits and `_`, unique across all categories) and the folder is
only a grouping. Design: `spec/13_more_diagrams_and_shapes/DESIGN.md`
section 3.

They are drawn by nymph with ImDrawList, not by an SVG renderer, so each file
keeps to the subset of the placeholder images (`resources/placeholders/`):
one `<svg>` holding self-closing `path`, `rect`, `circle`, `ellipse`,
`polygon` and `line` elements; no groups, transforms, gradients, text, images
or XML comments. With these additions:

| Feature | Rule |
|---|---|
| `viewBox` | Any size. One unit is one pixel at zoom 1: the shape's nominal size. |
| Colours | `fill` / `stroke` are `#000000` (the node's line colour), `#808080` (the line colour at half alpha), `#FFFFFF` (the node's fill colour) or `none`. |
| `stroke-width` | A multiple of the diagram's line weight (default 1). |
| `stroke-dasharray` | `dash gap` in viewBox units. |
| Paths | Arcs (`A`) are allowed; they become cubic curves. |
| `preserveAspectRatio="none"` | On `<svg>`: a resizable shape that fills any size. Without it: an icon, drawn at its aspect ratio with the label below. |
| `data-slice="l t r b"` | Resizable shapes: margins, in viewBox units, that keep their size when the shape is resized (corner radii, cylinder caps, a tab). Everything between them stretches. The margins must cover the whole curved part (a cap's lower edge too). |
| `data-grow` | `margins` (default): the node is the label plus the viewBox minus the text rect. `proportional`: the node is the label scaled by viewBox / text rect (diamond, ellipse). |
| `<rect id="text" fill="none" stroke="none" .../>` | Where the label goes (never drawn). Without it: the whole shape. |
| `<rect id="content" fill="none" stroke="none" .../>` | Containers: where child nodes go. |
| `id="outline"` | One closed shape, drawn or not, used for clipping edges (up to 64 points after flattening). |

Every filled path must be a simple polygon (no self-intersections: draw a
bow tie as two triangles). `tests/test_shapes.cpp` lints every file, so a file
that breaks the rules fails the build's tests.

To add one, drop a file that follows these rules in a category folder and
rebuild; nothing else changes. Draw it for nymph (MIT) or add a licence note.
