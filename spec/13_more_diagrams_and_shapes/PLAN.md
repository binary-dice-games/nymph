# nymph — More Diagrams and a Shape Library: Implementation Plan

See [DESIGN.md](DESIGN.md) for the design this plan implements,
[spec/03_uml_diagrams/DESIGN.md](../03_uml_diagrams/DESIGN.md) for the
diagrams it extends and
[spec/09_ui_mockups/DESIGN.md](../09_ui_mockups/DESIGN.md) section 3 for
the SVG subset it builds on.

Order: the shape machinery first (format, catalog, ImUml registry), then
the migration of the hardcoded shapes with the catalog growing alongside,
then one diagram per step (graph diagrams before the tree layout), then
docs. Every step leaves all existing tests passing; from step 5 on, the
committed example PNGs are rebuilt once (DESIGN section 7) and checked by
eye. Tests are GoogleTest files added with `nymph_add_test()`, pixel tests
use the software rasterizer and probe regions, never whole images.
`CHANGELOG.md` gets an entry in each step that changes user-visible
behaviour (steps 5 to 12).

Each step's **Result** line is filled in when it is done.

## Steps

### 1. Shape file format and parser

**Goal:** the placeholder parser reads shape files with their metadata.

**Deliverables:**
- `src/placeholders/svg_shapes.hpp/.cpp`: arcs in paths (converted to
  cubics), any positive `viewBox`, `preserveAspectRatio`, `data-slice`,
  `data-grow`, the reserved ids `text`, `content`, `outline` (kept
  separately, never drawn), `stroke-width` as a multiple. The placeholder
  rules still hold: a placeholder is still checked for a square viewBox by
  its own test.
- `svg_image` gains `slice`, `grow`, `icon`, `text_rect`, `content_rect`,
  `outline`.
- `resources/shapes/README.md`: the authoring rules of DESIGN section 3.

**Tests** (`tests/test_svg_shapes.cpp`, extended): arcs equal their cubic
approximation within 0.05 viewBox units; each metadata attribute parses;
a malformed `data-slice`, a drawn `text` rect, an `outline` that
self-intersects and an unknown `data-*` are errors naming the rule; all
twelve placeholders still parse to the same shapes as before.

**Result:** done. `parse_svg(text, svg_dialect::shape)` adds arcs, any `viewBox`, `data-slice`, `data-grow`, the ids `text`, `content`, `outline` and `stroke-dasharray`; placeholders parse as before (`test_placeholders` unchanged). 4 tests in `test_shapes`.

### 2. Embedding and the catalog

**Goal:** `resources/shapes/**/*.svg` are compiled in and found by name.

**Deliverables:**
- `cmake/NymphShapes.cmake`: `nymph_embed_shapes(<target>)`, recursive and
  sorted, name from the file, duplicate-name and bad-name errors at
  configure time; shares `cmake/NymphEmbedFile.cmake`. Called from
  `src/CMakeLists.txt` and the web build.
- `src/shapes/shape_catalog.hpp/.cpp`: `shape_names()`, `shape_category()`,
  `find_shape()` (parsed once, thread-safe).
- Three seed files, `resources/shapes/basic/rectangle.svg`,
  `uml/actor.svg`, `network/database.svg`, to exercise icon, resizable and
  sliced shapes.

**Tests** (`tests/test_shape_catalog.cpp`): every embedded file parses;
names unique, well-formed and sorted; category is the folder; a lint over
all files for DESIGN section 3 (sizes, simple polygons, rects inside the
viewBox, `slice` less than the viewBox, no forbidden elements); the web
build embeds the same count (Node test, existing harness).

**Result:** done. `cmake/NymphShapes.cmake`, `src/shapes/shape_catalog.*`, 103 files under `resources/shapes/`; the lint test (`shape_catalog.EveryFileKeepsTheAuthoringRules`) covers every file. The web build was not run.

### 3. ImUml shape registry and drawing

**Goal:** ImUml can size, draw and clip a registered shape, with no SVG
knowledge.

**Deliverables:**
- `imuml/imuml.h`: `ImUmlShape`, `AddShape`, `FindShape`, `RemoveShape`.
- `imuml/imuml_shape.cpp` (+ `imuml_internal.h`): `ShapeNodeSize`,
  `ShapeTextRect`, `ShapeOutline`, `RenderShape` with the nine-patch map
  and fixed 12-segment flattening.
- `src/shapes/shape_catalog.cpp`: converts `svg_image` to `ImUmlShape`;
  `register_shapes(ImUml context)` called from `render_frames()` with the
  other contexts.

**Tests** (`tests/test_imuml_shape.cpp`, ImGui and ImUml only): the slice
map keeps margins fixed and stretches the middle for several sizes; a
sliced cylinder's cap height is the same at 40 px and 200 px tall; both
`data-grow` rules give the specified size; `ShapeOutline` is a closed
polygon inside its box; a frame renders a filled pixel in the fill colour
and a stroke pixel in the line colour at known positions; an unregistered
shape draws a rectangle and counts a skipped item; output is byte-equal
across two runs.

**Result:** done. `AddShape`, `RenderShape`, `GetShapeTextRect`, `GetShapeNodeSize`, nine-patch map, dashes; registered by `imgui_contexts`. Tests render through the `Shape` element (cylinder caps, rounded corners, icon aspect, dashes).

### 4. Outline clipping in the graph layout

**Goal:** edges end on a node's outline polygon, whatever its shape.

**Deliverables:** `ImUmlGraphNode::Outline`; edge clipping against it
replaces the per-kind clip routines of `imuml_graph_layout.cpp`
(rectangle, ellipse, circle, diamond, bar all become outlines first, with
no visible change); `ShapeNodeSize` used for node sizes before ranking.
No new shape is used yet.

**Tests** (`tests/test_imuml_layout.cpp`, extended; the existing tests must
pass unchanged): an edge to a diamond, an ellipse and a rectangle ends
within 1 px of the same point as before; an edge to a polygon with a
notch ends on the boundary nearest the other end; an outline of 64 points
is accepted, 65 truncated deterministically.

**Result:** not done: `outline` is parsed and stored, edges still clip to the node box.

### 5. Migrate the hardcoded shapes; the `shape` field

**Goal:** no pictorial shape is drawn from code; every existing diagram
draws from the catalog; nodes take a `shape`.

**Deliverables:**
- `resources/shapes/`: the files behind the migration table of DESIGN
  section 6 (actor, boundary, control, entity, database, the 14 device
  icons, pseudo-states, signals, note, the flowchart shapes). Kind-to-name
  tables in `imuml_items.cpp`.
- Delete `RenderActorFigure`, `RenderParticipant`'s figures,
  `RenderDeviceIcon`, the flowchart shape code, `RenderSignal`,
  `RenderPseudoState` and the clip routines no outline replaced.
- `shape` enumeration field (catalog names, alias handling for the nine
  `FlowNode` names) on `FlowNode`, `Device`, `Participant`; the `Shape`
  figure element (`src/elements/shape_element.cpp`).
- Regenerate `examples/*.png` and `docs/` images affected; review by eye.

**Tests** (`tests/test_uml_shapes.cpp`; existing UML tests pass
unchanged): each `kind` / `shape` value of each element draws a non-empty
region in the node's rectangle; `Participant.kind: Database` and
`shape: database` are pixel-identical; legacy `shape: Stadium` and
`stadium` are identical; an unknown name errors with a suggestion and
lists the catalog; `Shape` in a layout and in a `Window` draws; behaviour
rules still key on `kind` (an edge out of a `Final` is still an error even
with `shape: rectangle`).

**Result:** partly done. Actor, participant figures, device icons, flowchart `Cylinder` / `Subroutine` come from the library (hardcoded primitives remain as the standalone fallback); `FlowNode.symbol` and the `Shape` element added; `Device` / `Participant` have no `shape` field; pseudo-states, notes and signals still in code. Four example PNGs regenerated; all earlier tests pass unchanged.

### 6. The shape catalog

**Goal:** the rest of the roughly 90 shapes, each editable and checked.

**Deliverables:** all remaining files of DESIGN section 3's table, drawn
by hand to the authoring rules; `nymph shapes` and `nymph shapes <name>`
(`src/cli/`); `examples/shapes_gallery.yaml` (one `Shape` grid per
category, generated by `scripts/make_shapes_gallery.py` or the CLI) and
`docs/shapes.md`.

**Tests:** the catalog lint from step 2 over every file; `nymph shapes`
lists every file once, sorted; `nymph shapes diamond` prints its mode and
rects; each shape renders at 3 sizes without a skipped item and with its
text rect inside its outline (a loop test, so a new file is covered
without editing the test).

**Result:** done except the gallery script: 103 shapes, `nymph shapes`, MCP `list_shapes`, `docs/shapes.md` with `docs/shapes.yaml` / `shapes.png` rendered by hand. Tests: `shapes_command`, `mcp_tools.shapes_match_the_cli`, `shape_draw.EveryShapeRendersTheSameTwiceAtThreeSizes`.

### 7. Container shapes in clusters

**Goal:** a group can be drawn with a catalog shape as its frame.

**Deliverables:** cluster frames use the shape's `text` rect as the title
area and `content` rect as the padding (graph layout reserves it);
`BeginGroup` takes a shape; `FlowGroup`, `TrustBoundary`, `Zone` accept
`shape` (container shapes only, others error naming the ones that fit);
container files `tabbed_folder`, `node_3d`, `component`, `bordered_frame`,
`dashed_region`.

**Tests:** a group with `tabbed_folder` has its title inside the tab and
children inside `content` for 3 sizes; a group with a non-container shape
is an error; the unframed default groups lay out byte-identically to
before (no `shape`).

**Result:** done through `BeginShapeGroup`: any shape frames a group, the title in its text area. `content` rects are authored but not read.

### 8. Object and package diagrams

**Goal:** the two cheapest graph diagrams.

**Deliverables:** `ObjectDiagram` (`Object`, `ObjectLink`) on the class
layout, with `ImUml::BeginObjectDiagram` and the underlined header
compartment; `PackageDiagram` (`Package`, `PackageItem`,
`PackageRelation`) with nested packages as clusters;
`src/elements/uml/object_diagram.cpp`, `package_diagram.cpp`; slots
`uml.object.item`, `uml.package.item`; `register_uml()`; one example
each; limits.

**Tests** (`tests/test_uml_object_package.cpp`): bind and render both
examples; `name : class` header, anonymous `: Order`; slots in order; an
unknown id errors with a suggestion; a relation between a package and its
descendant is an error; nested packages draw their children inside the
parent box; a 500-node diagram renders under the limit and 501 errors.

**Result:** done. `ObjectDiagram` on the class layout (`ImUmlClassKind_Object`), `PackageDiagram` on `BeginShapeDiagram`; tests in `test_uml_hierarchy_shapes`.

### 9. Component diagram

**Goal:** components with derived assembly connectors.

**Deliverables:** `ComponentDiagram`, `Component`, `ComponentLink`;
`ImUml` provides/requires bookkeeping at `End*`: ball and socket
shapes (`interface_ball`, `interface_socket`) positioned on the layout
edge between the two components, stubs for unmatched names;
two-providers error; example.

**Tests** (`tests/test_uml_component.cpp`): a provider and a consumer get
one connector with the ball at the provider side; an unmatched `requires`
draws a stub and no error; two providers error naming both; a nested
component draws inside its parent and edges to it end on its frame;
`Realization` ranks the target first.

**Result:** done. Assemblies derived from interface names (`ImUmlShapeLinkFlags_Assembly`), stubs for unmatched names, two-provider error.

### 10. Deployment diagram

**Goal:** nodes, artifacts and communication paths.

**Deliverables:** `DeploymentDiagram`, `DeploymentNode`, `Artifact`,
`DeploymentLink`; `node_3d` frame, stereotype lines, the three link kinds
and their end rules; example.

**Tests** (`tests/test_uml_deployment.cpp`): nesting draws artifacts
inside the node's `content`; stereotype text appears for each node kind;
`Deploy` not ending at a node and `Manifest` not leaving an artifact are
errors; a communication link between nested nodes ends on their frames.

**Result:** done: nested nodes, artifacts and components; `Communication`, `Deploy`, `Manifest` links and their end rules.

### 11. Tree layout, `TreeDiagram` and `MindMap`

**Goal:** hierarchies from nesting, tidy and balanced.

**Deliverables:**
- `imuml/imuml_tree_layout.cpp`: tidy tree (contours and threads), edge
  styles, `Elbow`, `Curve`, `Straight`; two-sided mode; `collapsed` badge.
- `ImUml::BeginTreeDiagram` / `BeginMindMap` and node items; `Tree` and
  `Mind` registrations in `src/elements/tree/` (`register_tree()` called
  from `register_builtin.cpp`); `TreeDiagram`, `TreeNode`, `MindMap`,
  `MindNode`, `MindLink`; the second-root error; examples.

**Tests** (`tests/test_imuml_tree_layout.cpp`, `test_tree_elements.cpp`):
a parent is centred over its first and last child; siblings keep written
order; two subtrees of different depth do not overlap and sit closer than
their bounding boxes would allow; all four directions are mirrors of
`TopDown`; a mind map's children split between sides with total heights
within one node of balanced, `side` pins a child; branch colours cycle the
palette and are inherited; `collapsed` hides descendants and shows `+N`;
500 nodes lay out in linear time (counted operations, not timing); output
is byte-equal across runs.

**Result:** done, with `TreeItem` instead of `TreeNode` (name clash). 10 layout tests (`test_imuml_tree_layout`) and element tests.

### 12. `OrgChart`

**Goal:** org charts with portraits, hanging teams and assistants.

**Deliverables:** `OrgChart`, `Person`, `OrgLink`; the hanging mode and
`staff` placement in the tree layout; card rendering (portrait placeholder
at the left, name, title, department, `vacant` dashed); example with all
features.

**Tests** (`tests/test_org_chart.cpp`): hanging children form one column
indented by `indent` with a spine; a `staff` card sits beside the parent
line and does not move the siblings' centring; `image: person` draws a
non-empty portrait and an unknown name errors listing the placeholders;
`vacant` draws a dashed border and no portrait; mixed horizontal and
hanging subtrees do not overlap; `OrgLink` is drawn and does not change
any node position.

**Result:** done: portraits (placeholder images), `Hanging`, `staff`, `vacant`, `OrgLink`.

### 13. Interaction, editor and agent surfaces

**Goal:** the new diagrams work wherever diagrams do.

**Deliverables:** hover and pick reporting for the tree layout through
`ImUmlDiagramInfo`; web-viewer and `nymph view` checks; view editor
targets verified on the new elements; the lists of diagram types in
`README.md`, `docs/format.md`, `docs/agent-guide.md`, `docs/examples.md`,
the Claude skill and references (`claude/`), the Cursor copies, the Gemini
primer, and `docs/shapes.md` linked from the README "Further
Documentation" table; regenerated `docs/elements/*.md`; the
`spec/03_uml_diagrams/DESIGN.md` sections 1, 4 and 10 corrected; the
`spec/INDEX.md` row updated; CHANGELOG entries finalised.

**Tests:** the schema-doc test passes (generated element docs in step);
the plugin sync and primer tests pass; a Node test hovers a tree node in
the web viewer and gets its id; the view-editor test selects a `Person` by
click and edits its `name`.

**Result:** partly done. Docs, agent guide, skills, plugin copies and Gemini primer updated; the diagrams use the existing `diagram_class` hover and edit-target code; not tried in the web viewer or the view editor by hand.

## Completion Criteria

1. No pictorial shape remains in `imuml/` code; deleting a shape file from
   `resources/shapes/` and rebuilding changes the drawing and nothing else.
2. A new SVG dropped in a category folder follows the documented rules and
   appears in `nymph shapes`, the schema enumeration and `Shape` with no
   C++ change; a file that breaks them fails the catalog lint.
3. The four UML diagrams, `TreeDiagram`, `MindMap` and `OrgChart` render
   from their examples in the PNG, `nymph view` and the web viewer, with
   positioned errors for every rule of DESIGN section 6.
4. Sources written before this project load and draw the same
   (`Stadium` and friends included); all earlier tests pass unchanged and
   the example PNGs are the ones rebuilt in step 5 or later.
5. Deterministic bytes: every new test renders twice and compares.
6. The suite is green on Linux, MSYS2 and MSVC in CI; README is under 200
   lines; CHANGELOG and docs are current.
