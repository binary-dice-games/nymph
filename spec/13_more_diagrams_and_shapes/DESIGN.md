# nymph — More Diagrams and a Shape Library: Design

**Status: implemented (plan steps 1 to 3, 5 to 13, with the differences of section 11); verified on Windows (MSVC, Debug); Linux, macOS and MSYS2 not yet.** [PLAN.md](PLAN.md) is the
ordered implementation plan. This document describes only what this
project adds or changes. The diagrams it extends are
[spec/03_uml_diagrams/DESIGN.md](../03_uml_diagrams/DESIGN.md) (*UML
DESIGN* below); the SVG subset and the embedding of editable vector files
are [spec/09_ui_mockups/DESIGN.md](../09_ui_mockups/DESIGN.md) section 3,
"Placeholder images" (*mockup DESIGN*); the element schema and layout are
[spec/01_core_features/DESIGN.md](../01_core_features/DESIGN.md) (*core
DESIGN*).

## 1. Purpose / Scope

Three pieces of work that depend on each other:

1. **A shape library.** Every shape ImUml draws from code today (the use
   case actor, the sequence participant kinds, the state and activity
   pseudo-nodes, the flowchart shapes, the network device icons, the note)
   becomes an SVG file in `resources/shapes/`, embedded in the binary like
   the placeholder images and editable with any SVG editor. The library
   grows to about 90 general shapes (basic geometry, flowchart, arrows,
   callouts, UML, network, containers), the set a user finds in Lucidchart
   or Excalidraw, and any graph or tree node can use any of them.
2. **Four more UML diagrams**: component, deployment, package, object.
3. **Three hierarchy diagrams** on a new tree layout: generic trees, mind
   maps, org charts.

Not in this project: timing, communication, composite structure and
interaction overview diagrams; ports on component borders; activity
swimlanes (UML DESIGN section 10); user-supplied shape files or inline
shapes in the source (section 10); free-form canvases with hand-placed
shapes (positions stay automatic, UML DESIGN goal 2); a hand-drawn
("rough") rendering style; arrowheads and cardinality markers as SVG (they
orient to an edge, so they stay code); mermaid text syntax.

Owns or changes:

| Path | Change |
|---|---|
| `resources/shapes/` | New: the shape files, one folder per category (`basic/`, `flowchart/`, `arrows/`, `callouts/`, `uml/`, `network/`, `containers/`), and a README with the authoring rules. |
| `cmake/NymphShapes.cmake` | New: embeds `resources/shapes/**/*.svg` (a recursive `nymph_embed_placeholders`). |
| `src/placeholders/svg_shapes.*` | The parser gains what shapes need (section 3); placeholders keep working unchanged. |
| `src/shapes/` | New: the catalog (names, categories, parsing into `ImUmlShape`, registration). |
| `imuml/` | `ImUmlShape` and its registry, shape drawing and sizing, outline clipping in the graph layout, the five new diagram kinds, the tree layout (`imuml_tree_layout.cpp`). The hardcoded shape code in `imuml_render.cpp` and `imuml_items.cpp` is removed. |
| `src/elements/uml/` | New: `component_diagram.cpp`, `deployment_diagram.cpp`, `package_diagram.cpp`, `object_diagram.cpp`; `shape` field on existing nodes. |
| `src/elements/tree/` | New: `tree_diagram.cpp`, `mind_map.cpp`, `org_chart.cpp`, `register_tree()`. |
| `src/elements/shape_element.cpp` | New: the `Shape` figure element. |
| `src/cli/` | `nymph shapes`. |
| `tests/`, `examples/`, `docs/` | Tests, one example per new diagram, `docs/shapes.md` (gallery), element and agent docs. |

## 2. Design Goals

1. **Shapes are data.** A shape is an SVG file a person can open, look at
   and change, then rebuild; adding one needs no C++. No shape geometry
   stays in ImUml code except arrowheads and markers.
2. **One mechanism for every node.** A shape names its text area, its
   outline and how it resizes inside the file, so the same code sizes,
   draws and clips any shape, whichever diagram holds it.
3. **Deterministic**, as before (UML DESIGN goal 3): same source, same
   build, same bytes. Shapes are flattened with a fixed segment count, not
   ImGui's adaptive tessellation. The migration changes the pixels of the
   committed example PNGs once (section 7); after that they are stable.
4. **No coordinates in the source.** New diagrams lay themselves out
   (UML DESIGN goal 2). Hierarchies are written by nesting, never by
   position.
5. **Strict, positioned errors** for ids, kinds and shape names, with
   suggestions (UML DESIGN goal 4).
6. **Existing sources keep working.** Every element and field written
   today still loads and draws the same shape (modulo goal 3's one-time
   change); renamed values stay as aliases (section 7).

## 3. The Shape Format

A shape is one SVG file in `resources/shapes/<category>/<name>.svg`. The
name is the file name without the extension (`^[a-z][a-z0-9_]*$`, unique
across all categories; the folder is only a grouping for the docs). The
file follows the subset of the placeholder images (mockup DESIGN section 3:
`path`, `rect`, `circle`, `ellipse`, `polygon`, `line`; no groups,
transforms, gradients, text, images or comments), with these changes and
additions, all parsed by the same `parse_svg`:

| Feature | Rule |
|---|---|
| `viewBox` | Any positive size (shapes are not square). One viewBox unit is one logical pixel at zoom 1; it is the shape's nominal size. |
| `preserveAspectRatio="none"` | Absent: an **icon**, drawn at its aspect ratio (actor, devices, dots). Present: a **resizable** shape (boxes, diamonds, arrows) that fills any node size. |
| `data-slice="l t r b"` | Resizable only. Margins in viewBox units that keep their size when the shape is resized, as a nine-patch: points in the left margin do not move, points in the middle stretch, points in the right margin move with the right edge. Corner radii, cylinder caps and folded corners stay round. Default `0 0 0 0` (plain stretch). |
| `id="text"` | One `rect` that is never drawn (it must have `fill="none"` and `stroke="none"`). The area the label goes in. Absent on a resizable shape: the whole viewBox; absent on an icon: the label is drawn below the icon. |
| `data-grow` | How a node grows around its label. `margins` (default): node size = label size + the viewBox minus the text rect, at least the nominal size (boxes, notes). `proportional`: node size = label size scaled by viewBox size / text rect size (diamond, ellipse, hexagon, whose text rect is inscribed in the outline). |
| `id="outline"` | One closed shape, drawn or not, that is the border for edge clipping and hit testing. Absent: the viewBox rectangle (the bounding box for icons). At most 64 points after flattening. |
| `id="content"` | Container shapes (package, 3D node, frame): the rect that holds child nodes. The `text` rect is then the title area. A shape with a `content` rect can be a cluster frame (section 5). |
| Colours | As placeholders, in the diagram's terms: `#000000` is the line colour (`color`), `#FFFFFF` the node fill (`fill_color`), `#808080` the line colour at half alpha; `none`. |
| `stroke-width` | A multiple of the diagram's line weight (default 1; 2 is heavy). Not a viewBox length, so lines stay the same weight in every shape and at every size. |
| `A` / `a` in paths | Now accepted and converted to cubic curves by the parser (Inkscape and Lucid exports use arcs). |

Resizing is a piecewise-linear map of x and y applied to every point and
control point after parsing (the slice margins keep slope 1, the middle
has the stretch). It is exact for lines and cubics, so a sliced outline
needs no re-flattening logic of its own.

Lint (a test over every file, like the placeholder rules): parses; names
unique and well-formed; `text` and `content` inside the viewBox and not
drawn; `outline` a simple closed polygon; every filled path simple;
`data-slice` margins sum to less than the viewBox; at most 200 segments
and 16 KB per file; icons have no `data-slice`.

### The catalog

About 90 shapes at the first release, drawn for nymph (MIT; Lucidchart and
Excalidraw are the reference for which shapes to include, no asset of
theirs is copied):

| Category | Shapes |
|---|---|
| `basic` | `rectangle`, `rounded_rectangle`, `stadium`, `ellipse`, `circle`, `square`, `diamond`, `triangle`, `right_triangle`, `trapezoid`, `parallelogram`, `pentagon`, `hexagon`, `octagon`, `star`, `plus`, `cross`, `heart`, `cloud`, `cylinder`, `cube`, `frame`, `line_bracket` (left, right) |
| `flowchart` | `process`, `predefined_process`, `decision`, `terminator`, `data`, `document`, `multi_document`, `manual_input`, `manual_operation`, `preparation`, `display`, `delay`, `stored_data`, `internal_storage`, `database`, `off_page_connector`, `connector`, `merge`, `extract`, `summing_junction`, `or`, `sort`, `collate`, `card` |
| `arrows` | `arrow_right`, `arrow_left`, `arrow_up`, `arrow_down`, `arrow_double`, `arrow_bent`, `arrow_u_turn`, `chevron`, `pentagon_arrow` |
| `callouts` | `note`, `sticky_note`, `speech_bubble`, `thought_bubble`, `callout_box`, `banner`, `ribbon` |
| `uml` | `actor`, `boundary`, `control`, `entity`, `component`, `package`, `node_3d`, `artifact`, `interface_ball`, `interface_socket`, `initial`, `final`, `flow_final`, `history`, `deep_history`, `fork_bar`, `signal_send`, `signal_receive`, `lifeline_x`, `object_box` |
| `network` | `server`, `router`, `switch`, `firewall`, `load_balancer`, `access_point`, `internet`, `storage`, `workstation`, `laptop`, `mobile`, `user`, `queue`, `container`, `function` |
| `containers` | `tabbed_folder`, `swimlane`, `bordered_frame`, `dashed_region` |

Shapes the UML DESIGN draws by name keep their names (`Participant.kind:
Database` → `database`, `Device.kind: Router` → `router`); section 6 has
the mapping.

## 4. Key Abstractions

```
 resources/shapes/**/*.svg
        │ cmake/NymphShapes.cmake (embed)
        ▼
 src/shapes/shape_catalog ── parse_svg ──► ImUmlShape ──► ImUml::AddShape()   (once per
        │  names, categories                                                    context)
        ▼
 element `shape: name` ──► ImUml::SetNextNodeShape(name) ──► node item ──► layout (size, clip)
                                                                  └──► End*: RenderShape()
```

### `ImUmlShape` and the registry (`imuml/`)

Plain data, in ImUml's style (ImVector, no STL): name, viewBox size, icon
or resizable, slice margins, text rect, content rect, grow mode, outline
polygon, and the drawable paths with their colour role, stroke multiple
and flattened points kept as cubic segments. `ImUml::AddShape(const
ImUmlShape&)` copies it into the current context; `FindShape(name)`;
`RemoveShape`. ImUml has **no SVG parser and no files**: it knows geometry
only (Goal 1 of the UML DESIGN: usable by any ImGui program, which can
register its own shapes). A node whose shape is not registered is drawn as
a plain rectangle and counted in `GetLastDiagramInfo().SkippedItems`;
nymph's checks make this unreachable.

Functions (`imuml_internal.h`, with `imuml_shape.cpp`):
`ShapeNodeSize(shape, label_size)` (the grow rule), `ShapeTextRect(shape,
size)`, `ShapeOutline(shape, size)` (the resized outline polygon, used by
the layout), `RenderShape(dl, shape, min, max, zoom, line, fill, weight)`.
Curves are flattened with a fixed 12 segments per cubic. `ImUmlStyle` is
unchanged; the node's `color` / `fill_color` and the diagram's line weight
colour every shape.

### Node shapes in the graph layout

`ImUmlGraphNode` gains an `Outline` (polygon relative to the node centre,
at the node's final size). The layout clips an edge to the polygon (ray
against polygon edges) in place of the per-kind clipping of UML DESIGN
section 4, step 7; its `Kind` clip routines (rectangle, ellipse, circle,
diamond, bar) are removed once every kind has an outline. Size is decided
before ranking by `ShapeNodeSize`, so a diamond still widens to hold its
text. Cluster frames with a `content` rect (section 5) reserve the padding
the shape states instead of the fixed `NodePadding`.

### Tree layout (`imuml/imuml_tree_layout.cpp`)

A pure function, separate from the graph layout because a hierarchy is a
rooted tree whose children stay in the written order and whose parent is
centred over them, which dot's layout does not guarantee. Input: nodes
with size, parent, order, `side` and `hanging` flags; options (mode,
direction, spacings, edge style). Output: node rectangles, edge paths,
total size. No ImGui context. Three modes share the core:

1. **Tidy tree** for `TreeDiagram` and `OrgChart`: the algorithm of
   Buchheim, Jünger and Leipert, "Improving Walker's Algorithm to Run in
   Linear Time" (2002), implemented from the paper, with node sizes along
   both axes (separation uses half widths), siblings in written order, the
   parent centred over its first and last child, and subtrees packed with
   the contour threads so a small subtree sits close to its neighbour.
   Levels are separated by `rank_spacing` plus the tallest node of the
   level (the same rule as the graph layout).
2. **Hanging** (org chart): a node marked `hanging` places its children in a
   single column under it, indented by `indent`, joined by a vertical spine
   with a short elbow to each child; a hanging subtree's width is its
   widest card. Mixed with tidy children of other parents. This is the
   layout org charts use for teams with many leaves.
3. **Two-sided** (mind map): the root is centred; its children are split in
   two groups, kept in order, that balance the groups' summed subtree
   heights (a child with `side: Left | Right` is pinned), the right group
   top to bottom, the left group top to bottom as written; each group is a
   tidy tree growing away from the root. Branches are curves from the
   parent's edge to the child's.

Edge styles: `Elbow` (orthogonal: down to a bus, across, down), `Curve`
(a cubic from edge to edge), `Straight`. All coordinates are whole logical
pixels; all ties follow written order. `staff` nodes (org chart assistants)
sit beside the parent on the parent's rank band with a short side
connector and do not take part in centring.

### New diagram elements

| Diagram | Element | Children | Layout |
|---|---|---|---|
| Component | `ComponentDiagram` | `Component` (nested components make it a cluster), `ComponentLink`, `Note` | graph |
| Deployment | `DeploymentDiagram` | `DeploymentNode` (nested), `Artifact`, `Component`, `DeploymentLink`, `Note` | graph |
| Package | `PackageDiagram` | `Package` (nested), `PackageItem`, `PackageRelation`, `Note` | graph |
| Object | `ObjectDiagram` | `Object`, `ObjectLink`, `Note` | graph |
| Tree | `TreeDiagram` | one `TreeNode`, nesting through its `children`; `Note` | tree |
| Mind map | `MindMap` | one `MindNode`, nesting; `MindLink`, `Note` | tree, two-sided |
| Org chart | `OrgChart` | one `Person`, nesting; `OrgLink`, `Note` | tree, hanging |

## 5. Source Format

All the shared fields, ids, references and the `Note` of UML DESIGN
section 3 apply: `title`, `width`, `height`, `min_zoom`, `max_zoom`,
`line_color`, `fill_color`, the text style, and for the graph diagrams
`direction`, `edge_style`, `node_spacing`, `rank_spacing` and the layout
hints.

### The `shape` field

An enumeration over the catalog names (so a wrong name gets the list and
a suggestion, as `image` does for placeholders), on these nodes:
`FlowNode` (extended; section 7), `Device` and `Participant` (an override
of what `kind` selects), `Component`, `Artifact`, `DeploymentNode`,
`Package`, `PackageItem`, `Object`, `TreeNode`, `MindNode`, `Person`, and
the `Shape` element. Each element has its own default. A resizable shape
takes the node's text as its label; an icon is drawn at its nominal size
(scaled by the diagram's zoom) with the label below, or inside its `text`
rect when it has one. Containers (groups with children) need a shape with a
`content` rect; any other shape on a container is an error naming the
shapes that fit.

### Component diagram

```yaml
root:
  type: ComponentDiagram
  children:
    - {type: Component, id: web, name: Web app, requires: [IOrders, IAuth]}
    - type: Component
      id: shop
      name: Shop
      children:
        - {type: Component, id: orders, name: Orders, provides: [IOrders], requires: [IPay]}
        - {type: Component, id: billing, name: Billing, provides: [IPay]}
    - {type: Component, id: auth, name: Auth service, provides: [IAuth]}
    - {type: ComponentLink, from: orders, to: billing, kind: Dependency, label: events}
```

- `Component`: `id`, `name`, `stereotype` (default `«component»`; empty
  hides it), `provides`, `requires` (`text_array` of interface names),
  `shape` (default `component`), `rank`. A component with children is a
  cluster drawn as a titled frame (the same code path as a composite
  state).
- **Assemblies are derived.** For each interface a component requires, if
  exactly one component provides it, ImUml draws the ball-and-socket
  connector between them (a ball at the provider, a socket at the
  consumer, the interface name beside it). An interface nobody provides is
  drawn as a socket stub with its name (an external dependency). A
  provided interface nobody requires is a ball stub. Two providers of one
  name are an error ("interface 'IPay' is provided by 'billing' and
  'legacy_billing'"), resolved with a `ComponentLink`.
- `ComponentLink`: `from`, `to`, `kind` (`Dependency` dashed open arrow,
  `Association` solid line, `Delegation` solid open arrow, `Realization`
  dashed hollow triangle), `label`, the layout hints. The layout ranks
  `from` first, except `Realization`, which ranks `to` first, as in a
  class diagram.

### Deployment diagram

```yaml
root:
  type: DeploymentDiagram
  children:
    - type: DeploymentNode
      id: srv
      name: App server
      kind: Device
      children:
        - type: DeploymentNode
          id: jvm
          name: JVM 21
          kind: ExecutionEnvironment
          children:
            - {type: Artifact, id: war, name: shop.war}
            - {type: Component, id: shop, name: Shop}
    - {type: DeploymentNode, id: db, name: Database host, kind: Device}
    - {type: DeploymentLink, from: srv, to: db, label: "JDBC / TLS"}
```

- `DeploymentNode`: `id`, `name`, `kind` (`Node`, `Device`,
  `ExecutionEnvironment`; shown as `«device»`, `«execution environment»`),
  `shape` (default `node_3d`), `rank`. Children (nodes, artifacts,
  components, notes) are deployed on it: it is a cluster drawn with its
  shape as the frame (title in the `text` rect, children in the `content`
  rect).
- `Artifact`: `id`, `name`, `shape` (default `artifact`); the stereotype
  `«artifact»` is drawn above the name.
- `DeploymentLink`: `from`, `to`, `kind` (`Communication` solid, the label
  is the protocol; `Deploy` dashed open arrow `«deploy»`, artifact or
  component to node, for a deployment drawn as a link instead of by
  nesting; `Manifest` dashed `«manifest»`, artifact to component), `label`,
  `from_label`, `to_label`, the layout hints. Communication goes between
  nodes, `Deploy` ends at a node, `Manifest` from an artifact: errors
  otherwise.

### Package diagram

```yaml
root:
  type: PackageDiagram
  children:
    - type: Package
      id: ui
      name: ui
      children:
        - {type: PackageItem, id: view, name: OrderView}
    - {type: Package, id: domain, name: domain}
    - {type: PackageRelation, from: ui, to: domain, kind: Import}
```

- `Package`: `id`, `name`, `shape` (default `package`, the tabbed folder),
  `rank`; children are packages, items and notes. A package without
  children is a plain folder shape sized to its name.
- `PackageItem`: `id`, `name`, `kind` (`Class`, `Interface`, `Enum`,
  `Component`, `Other`; a stereotype line for all but `Class`), `shape`.
- `PackageRelation`: `from`, `to` (packages or items; a relation between a
  package and something inside it is an error, as for composite states),
  `kind` (`Dependency` dashed open arrow, `Import` dashed, `«import»`,
  `Access` dashed, `«access»`, `Merge` dashed, `«merge»`), `label`, the
  layout hints.

### Object diagram

```yaml
root:
  type: ObjectDiagram
  children:
    - {type: Object, id: o1, name: order1, class: Order, slots: ["total = 42.50", "state = PAID"]}
    - {type: Object, id: c1, class: Customer, slots: ["email = ana@example.com"]}
    - {type: ObjectLink, from: o1, to: c1, label: placedBy, kind: DirectedAssociation}
```

- `Object`: `id`, `name` (instance name; empty draws an anonymous
  `: Order`), `class` (text), `slots` (`text_array`, one `attribute =
  value` per line, drawn as written), `shape` (default `object_box`).
  The header is `name : class`, underlined; the box is the class box of
  UML DESIGN with a header compartment and a slot compartment.
- `ObjectLink`: `from`, `to`, `kind` (`Association`, `DirectedAssociation`,
  `Aggregation`, `Composition`, `Dependency`, as `ClassRelation`), `label`,
  `from_label`, `to_label`, the layout hints.

### Tree diagram

```yaml
root:
  type: TreeDiagram
  direction: LeftRight
  edge_style: Elbow
  children:
    - type: TreeNode
      text: src
      children:
        - {type: TreeNode, text: core, shape: folder}
        - type: TreeNode
          text: elements
          children: [{type: TreeNode, text: uml}, {type: TreeNode, text: ui}]
```

- `TreeDiagram`: `direction` (`TopDown` default, `LeftRight`; `BottomUp`
  and `RightLeft` mirror), `edge_style` (`Elbow` default, `Curve`,
  `Straight`), `node_spacing`, `rank_spacing`. Exactly one `TreeNode` as a
  child (the root); more than one is an error pointing at the second.
- `TreeNode`: `id` (optional; unique when given), `text` (`\n` breaks
  lines), `shape` (default `rounded_rectangle`), colours, `collapsed`
  (bool: the children are not drawn; a `+N` badge shows how many nodes
  are hidden), `children`.

### Mind map

```yaml
root:
  type: MindMap
  children:
    - type: MindNode
      text: Launch plan
      children:
        - type: MindNode
          text: Marketing
          children: [{type: MindNode, text: Blog}, {type: MindNode, text: Video}]
        - {type: MindNode, id: eng, text: Engineering, side: Left}
        - {type: MindNode, id: qa, text: QA}
    - {type: MindLink, from: qa, to: eng, label: blocks}
```

- `MindMap`: `branch_style` (`Curve` default, `Elbow`, `Straight`),
  `palette` (colours cycled across the root's children; empty: the theme's
  series colours), `node_spacing`, `rank_spacing`, one root `MindNode`.
- `MindNode`: `id` (optional), `text`, `shape` (default: the root
  `rounded_rectangle`; other nodes `none`, text over its branch line, as
  XMind), `side` (`Left`, `Right`, root children only; empty: balanced),
  `color` (branch colour; inherited down the branch from the palette),
  `collapsed`, `children`. A branch is drawn in its colour with a line
  weight that falls with depth.
- `MindLink`: `from`, `to` (ids), `label`; a dashed curve between two
  nodes that does not influence the layout.

### Org chart

```yaml
root:
  type: OrgChart
  children:
    - type: Person
      id: ceo
      name: Ana Ruiz
      title: CEO
      image: person
      children:
        - {type: Person, id: asst, name: Leo Park, title: Executive assistant, staff: true}
        - type: Person
          id: eng
          name: Mia Chen
          title: VP Engineering
          children_layout: Hanging
          children:
            - {type: Person, name: Sam Ortiz, title: Backend lead}
            - {type: Person, name: Kai Novak, title: Frontend lead}
        - {type: Person, id: cfo, name: Tom Reyes, title: CFO, vacant: true}
    - {type: OrgLink, from: cfo, to: eng, label: "dotted line"}
```

- `OrgChart`: as `TreeDiagram` (`direction` default `TopDown`, `edge_style`
  default `Elbow`), one root `Person`.
- `Person`: `id` (optional), `name`, `title`, `department` (a small third
  line), `image` (a placeholder image name of mockup DESIGN section 3,
  shown as a portrait at the card's left; empty: no portrait), `shape`
  (default `rounded_rectangle`), colours, `children_layout` (`Horizontal`
  default, `Hanging`), `staff` (bool: an assistant card beside the parent
  line), `vacant` (bool: dashed card, no portrait), `collapsed`, `children`.
- `OrgLink`: `from`, `to`, `label`; a dotted reporting line that does not
  influence the layout.

### `Shape` figure element

`Shape` draws one catalog shape in a layout cell or a mockup window, as
`Image` draws a placeholder: `shape`, `text`, `width`, `height` (layout
hints; an icon keeps its aspect ratio), `color`, `fill_color`, the text
style. Fills both the `figure` and `ui.widget` slots. For legends,
annotations, and dashboards that mix a few shapes with charts.

## 6. Public Contract

### Migration of the hardcoded shapes

| Code today | Becomes |
|---|---|
| `RenderActorFigure` (use case `Actor`, sequence `Actor`) | `actor` |
| `RenderParticipant` kinds `Boundary`, `Control`, `Entity`, `Database` | `boundary`, `control`, `entity`, `database`; `Participant` is `rectangle` |
| `RenderPseudoState` and activity controls | `initial`, `final`, `flow_final`, `decision`, `fork_bar`, `history`, `deep_history` |
| `RenderSignal` | `signal_send`, `signal_receive` |
| `RenderDeviceIcon`, 14 kinds | `server`, `database`, `storage`, `router`, `switch`, `firewall`, `load_balancer`, `access_point`, `cloud`, `internet`, `workstation`, `laptop`, `mobile`, `user` |
| `RenderNote` | `note` |
| `RenderUseCase` | `ellipse` |
| `FlowNode` shapes `Box`, `Rounded`, `Stadium`, `Diamond`, `Circle`, `Parallelogram`, `Hexagon`, `Cylinder`, `Subroutine` | `rectangle`, `rounded_rectangle`, `stadium`, `diamond`, `circle`, `parallelogram`, `hexagon`, `cylinder`, `predefined_process` |
| `RenderFragment`, `RenderClassBox`, ER tables, `RenderStateBox`, `RenderEdgeLabel`, `RenderArrowHead`, `RenderCardinality`, `RenderPath` | stay in code: they are structure (compartments, aligned columns, edge ends), not shapes |

Behaviour that depends on a shape's identity (a state transition may not
leave `final`; the layout puts actors in the first rank) keys on the
element's `kind`, never on the shape file, so editing or replacing a file
cannot break a rule.

### Schema

- New elements and fields as in section 5; `nymph schema <type>` prints
  them, `docs/elements.md` and the per-element docs are regenerated.
- `FlowNode.shape` accepts every catalog name and, as aliases, the nine
  CamelCase names above (listed under *Deprecated* in the CHANGELOG, shown
  by `nymph schema` only as aliases).
- `nymph shapes` prints the catalog, one `category/name` per line;
  `nymph shapes <name>` prints the shape's size, mode (icon or resizable)
  and rects. `docs/shapes.md` is the gallery, one image per category,
  generated by a script from the catalog.
- ImUml API: `AddShape`, `FindShape`, `RemoveShape`, `SetNextNodeShape`,
  the new `Begin*Diagram` pairs and item functions named after the
  elements (`BeginComponent`, `ComponentProvides`, `ComponentLink`,
  `BeginDeploymentNode`, `Artifact`, `Package`, `PackageItem`, `Object`,
  `ObjectSlot`, `TreeNode`, `MindNode`, `Person`, ...), following the
  conventions of UML DESIGN section 4.

### Errors

Added to the id and kind rules of UML DESIGN section 3: an unknown shape
name; a shape without a `content` rect on a container; two providers of
one interface; a second root in a tree; a relation between a package or
node and its own descendant; `Deploy` not ending at a node; `Manifest` not
leaving an artifact; `collapsed` on a leaf (allowed, ignored). Fit and
limits as before: at most 500 nodes per diagram, tree depth at most 32.

## 7. Design Decisions

- **SVG files, not code, not an icon font.** Code is what exists and is
  why changing an actor means editing C++; an icon font cannot carry fills,
  cut-outs or per-shape text rects. SVG is what shape libraries are made
  in, any editor opens it, and the parser exists (mockup DESIGN section 3).
- **Metadata as `id`s and `data-` attributes in the file.** An editor keeps
  them, a person sees them in the XML editor, and the file stays a valid
  SVG that browsers and Inkscape display (the `text` rect is invisible
  there). Rejected: a sidecar `.json` per shape (two files to keep in
  step, the usual way shapes rot).
- **Nine-patch slicing, not plain stretch.** A stretched cylinder gets a
  tall flat cap; a stretched rounded rectangle gets elliptical corners.
  Lucidchart avoids it with per-shape handles; slice margins are the same
  idea with four numbers, and the map is linear, so deterministic and
  cheap. Rejected: SVG transforms and groups (a general renderer to write
  and test), and procedural shapes with parameters (code again).
- **Two grow rules.** A note or a box wants fixed margins around any label;
  a diamond wants its margins to scale with the label. One rule fails
  one of them, so the file says which (`data-grow`).
- **ImUml gets geometry, nymph gets files.** The parser, the embedding and
  the catalog names are nymph's (`src/shapes/`); ImUml sees `ImUmlShape`
  values it was given. This keeps ImUml free of file formats and nymph
  includes (UML DESIGN section 8) and lets another ImGui program register
  its own shapes. Cost: a program that uses ImUml without nymph has no
  shapes until it registers some, and gets plain boxes. Rejected: a second
  SVG parser inside ImUml (duplicate code), moving the parser into ImUml
  (placeholders would depend on a UML library).
- **Outline polygons for clipping** replace five hand-written clip routines
  and give every new shape correct edge ends. Cost: a polygon of up to 64
  points approximates curves (error under 1 px at the sizes drawn).
- **The migration regenerates the example PNGs once.** The old shapes were
  drawn with ImGui's primitives (`AddCircle`, adaptive tessellation); the
  catalog draws fixed-segment paths. Pixels move by antialiasing, not
  design; the pixel tests probe regions and must pass unchanged, the
  committed examples are rebuilt and compared by eye in the same commit.
- **Legacy `FlowNode.shape` names are aliases, not an error.** Agents and
  docs written before this project use `Stadium`; breaking them is not
  worth a naming tidy-up. They are deprecated, not removed.
- **A separate tree layout, not dot.** dot ranks and orders a graph; a
  hierarchy must keep its written sibling order, centre parents over their
  children and, for a mind map, grow two ways from the root. A tidy-tree
  algorithm does that in linear time with a layout that reads as a tree.
  Rejected: reusing dot with `rank=same` tricks (siblings reorder; no
  hanging layout; no two-sided growth).
- **Hierarchy by nesting, ids optional.** The tree is the YAML's own
  nesting (no `parent:` fields to mistype, no cycles to check). An id is
  needed only to refer to a node from a `MindLink` or `OrgLink`, so a
  large tree is not full of unused ids. Rejected: a flat list with `parent`
  ids (every tree error becomes a reference error, and the order of
  siblings is implicit).
- **Mind map, tree and org chart are three elements on one engine.** Their
  nodes carry different data (a mind node is text, a person has a name, a
  title and a portrait) and their defaults differ (curved colour branches
  against orthogonal cards); one element with a `style` switch would make
  every field apply to only one of three modes.
- **Assembly connectors are derived from interface names.** An agent
  writes `provides: [IPay]` on one component and `requires: [IPay]` on
  another and cannot get the wiring wrong or forget a link. Ambiguity is an
  error, with `ComponentLink` as the escape. Rejected: explicit
  provided/required interface nodes with separate assembly links (three
  elements to say one thing), and component ports (out of scope).
- **Nesting is deployment** in the deployment diagram: an artifact written
  inside a node is deployed on it, as the UML picture shows, and `Deploy`
  links are for a deployment shown across the diagram. No second way to say
  the same thing inside one box.
- **`Shape` as an element** lets a user put the library to work without a
  diagram, in a dashboard or mockup, at the cost of one small element
  class that mostly reuses `RenderShape`.

## 8. Constraints and Invariants

- A shape file is data: nothing in it can reach the filesystem, the
  network, or a format string (no `<text>`, `<image>`, `<use>`, `href`).
  The lint test enforces it; the parser rejects anything outside the subset
  naming the rule.
- Shape drawing and layout are pure functions of the shape, the node size
  and the style; no iteration over pointer-keyed containers; whole-pixel
  coordinates, as the graph layout. The tree layout follows written
  order everywhere; ties never depend on hashing.
- The catalog is parsed once per process, thread-safe, and registered into
  each ImUml context when it is created (and in the web build, where the
  files are compiled into the module like fonts and placeholders).
- Names are unique across categories, checked at configure time
  (`nymph_embed_shapes` fails with both paths) and by the lint test.
- Binary size: the catalog is about 90 files of at most a few hundred bytes
  to 2 KB each; the web module embeds them all.
- Platform: no new OS dependency. CMake globbing is recursive and sorted.
  Generated sources are per file, like placeholders, so MSVC string-length
  limits are not hit.
- The tree layout bounds: at most 500 nodes, depth 32, linear work.

## 9. Integration Boundaries

- Builds on the graph layout, clusters, notes, fit rules and the ImUml
  context of [UML DESIGN](../03_uml_diagrams/DESIGN.md) and replaces its
  shape code; updates its sections 1, 4 and 10 where they would mislead
  (the diagram table, the rendering primitives, the extension points).
- Reuses the SVG parser and embedding of
  [mockup DESIGN](../09_ui_mockups/DESIGN.md) section 3, and the
  `Image` placeholder names for org chart portraits.
- Interaction ([spec/04_web_viewer](../04_web_viewer/DESIGN.md)): graph
  diagrams keep their hover and picks; the tree layout reports node
  rectangles through the same `ImUmlDiagramInfo` hover fields, so picks,
  pan and zoom work for trees; the view editor
  ([spec/08_view_editor](../08_view_editor/DESIGN.md)) targets the new
  elements through the generic element list with no change.
- Agent surfaces that list diagram types (`README.md`, `docs/format.md`,
  `docs/agent-guide.md`, the Claude and Cursor skills and references, the
  Gemini primer) gain the new diagrams and `nymph shapes`; the plugin sync
  and primer generators already copy from one source.
- Nothing else depends on this project.

## 10. Extension Points

- **Inline shapes in the source** (a top-level `shapes:` key holding SVG
  text), so a chart carries its custom shapes with it as it carries its
  data; needs a size limit and the lint rules at load.
- **User shape directories** (`NYMPH_SHAPES`), for local, uncommitted
  libraries on the CLI only.
- **Ports** on shapes (named connection points in the file, `id="port-n"`)
  and component ports on top of them.
- **Swimlanes**, **timing**, **communication** diagrams
  (UML DESIGN section 10).
- **Arrowheads as SVG markers** with an origin and a direction.
- **A rough (hand-drawn) style** that jitters the flattened outline with a
  seeded generator.
- **Edge-to-shape routing** with ports for orthogonal edges.

## 11. As built: differences from sections 1 to 10

Read these first; where they disagree with the sections above, they win.

- **Element names.** A tree node is `TreeItem`, not `TreeNode`: `TreeNode` is the ImGui widget of user-interface mockups. A mind map's node is `MindNode`, an org chart's `Person`, as designed.
- **MCP tool.** `list_shapes` (optional `name`) is a fifth tool next to `list_fonts`, answering the same text as `nymph shapes [<name>]`. The Claude and Cursor skills, `docs/claude.md` and the MCP bundle manifest list it.
- **Shapes.** 103 files. `database` exists once (category `flowchart`) and serves flowchart nodes, sequence participants and network devices alike; the cylinder's `data-slice` is `0 24 0 12` (the top cap's lower edge is part of the cap, so its margin is 24, not 12 as an early draft of section 3 implied). `stroke-dasharray` is a supported attribute (the `dashed_region` frame needs it).
- **ImUml shape API.** `ImUmlShapeDesc` carries flattened polylines (the 12-segment flattening happens in `src/shapes/`), so ImUml has no curve code. Placeholder images are registered too, as `image:<name>`, for org chart portraits. Diagram kinds: `BeginShapeDiagram` serves the component, deployment and package diagrams (`ShapeNode`, `BeginShapeGroup`, `ShapeLink`); the object diagram is a class diagram whose boxes have `ImUmlClassKind_Object`; trees use `BeginTreeDiagram` with `SetupTree(mode)` and `BeginTreeNode(ImUmlTreeNodeDesc)`.
- **Where shapes are used.** `Shape` is a `figure` element only (not in mockup windows). `FlowNode` takes any shape in a new `symbol` field and keeps `shape` for the nine mermaid names; `Device` and `Participant` keep selecting their icon by `kind` and gained no `shape` field. Component, deployment, package and tree-family nodes take `shape`.
- **Migration (section 6).** Actor (use case and sequence), the sequence participants `Boundary`, `Control`, `Entity` and `Database`, the 14 network device icons and the flowchart `Cylinder` and `Subroutine` are drawn from the library. The primitives they replaced (`RenderActorFigure`, `RenderDeviceIcon`, `RenderCylinder`) stay as the **fallback** for an ImUml used without a registered library, so ImUml alone still draws something; nymph always registers the library. Parametric outlines stay in code: rounded and stadium boxes, diamond, circle and ellipse, and the flowchart `Parallelogram` and `Hexagon` (their slant depends on the node's height). The pseudo-states of state and activity diagrams, the signals and the note also still use their code, because their size follows a measured text; the library has `initial`, `final`, `signal_send` and the like for `Shape` and `symbol`.
- **No outline clipping.** Edges still end on the node's box, ellipse, circle or diamond as before; `outline` is parsed and stored but the graph layout does not use it yet (plan step 4 is open). Nodes with shapes use the box.
- **Group frames.** A group (`Component`, `DeploymentNode`, `Package`, a `Flowchart` shape group) is a cluster drawn with its shape: the title sits in the shape's text area, the header band is the shape's top margin plus the title, and `content` is not read (the cluster padding applies). Any shape can frame a group.
- **Tree layout.** The contour packing of Buchheim, Juenger and Leipert is done with explicit contours per node (arrays by depth) instead of threads: simpler, quadratic in the worst case, linear in practice. The mind map's palette is fixed (eight colours, a branch `color` overrides); `MindMap` has no `palette` field and its `edge_style` plays the part of `branch_style`. Collapsed nodes are skipped by the element, which passes the number of hidden descendants for the `+N` badge.
- **Limits.** Trees may be 32 levels deep (`kMaxTreeDepth`); the node and edge limits of UML DESIGN section 8 apply.
- **Not done.** Outline clipping, `shape` on `Device` and `Participant`, `Shape` in windows, the `MindMap.palette` field, migrating pseudo-states, notes and signals, a script that generates `docs/shapes.md` (it is rendered from `docs/shapes.yaml` by hand), and runs of the web build and of the Linux, macOS and MSYS2 builds.
