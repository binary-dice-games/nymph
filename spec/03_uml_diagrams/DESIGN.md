# nymph — UML Diagrams: Design

**Status: implemented (plan steps 1-13); verified on Linux (GCC); MSYS2 and MSVC not yet.** [PLAN.md](PLAN.md)
is the ordered implementation plan. This document describes only what
this project adds or changes; the rest of the architecture is
[spec/01_core_features/DESIGN.md](../01_core_features/DESIGN.md)
(referred to below as *core DESIGN*).

## 1. Purpose / Scope

nymph draws plots: `Plot` and `Plot3D` are elements whose children are
series, drawn by ImPlot and ImPlot3D inside the figure's ImGui frame.
This project adds UML diagrams the same way: a diagram is an element
whose children are the diagram's nodes and edges, described in the YAML
source and drawn inside the same frame. Two references:

- **Mermaid** for what an agent can say (the diagram kinds, their
  vocabulary, the look of the shapes).
- **Graphviz** (`dot`) for how a text description becomes a drawing: its
  layered layout algorithm, the attributes that steer it without
  coordinates (`rankdir`, `nodesep`, `ranksep`, `weight`, `minlen`,
  `constraint`, `rank=same`), its clusters and its edge routing.

The syntax is nymph's YAML, so
a diagram can sit in a layout next to plots and labels, travels inside the
PNG, and gets the same positioned, strict errors.

There is no ImGui library for UML diagrams, so this project writes one
first: **ImUml** (`imuml/`), an ImGui extension in the style of ImPlot.
It knows nothing about nymph, YAML or the schema; the nymph elements are a
thin layer that turns bound nodes into ImUml calls, as `src/elements/plot2d.cpp`
does for ImPlot.

Diagrams in this project, the most used UML kinds (mermaid has each):

| Diagram | Element | Children |
|---|---|---|
| Class | `ClassDiagram` | `Class`, `ClassRelation`, `Note` |
| Sequence | `SequenceDiagram` | `Participant`, `Message`, `Fragment` (with `Else`), `Note` |
| State | `StateDiagram` | `State` (composite when it has children), `Transition`, `Note` |
| Activity | `ActivityDiagram` | `Action`, `ActivityControl`, `Flow`, `Note` |
| Use case | `UseCaseDiagram` | `Actor`, `UseCase`, `UseCaseRelation`, `Note` |
| Entity-relationship (database structure; not UML, added later) | `ERDiagram` | `Entity`, `Relationship`, `Note` |
| Data flow (not UML, added later) | `DataFlowDiagram` | `ExternalEntity`, `Process`, `DataStore`, `DataFlow`, `TrustBoundary`, `Note` |
| Network (not UML, added later) | `NetworkDiagram` | `Device`, `Link`, `Zone`, `Note` |
| Flowchart (added later; not UML) | `Flowchart` | `FlowNode`, `FlowGroup`, `FlowLink`, `Note` |

Not in this project (section 10 lists where they would go): component,
deployment, object, package, timing and communication diagrams; activity
swimlanes; concurrent regions in states; mermaid's text syntax as an input
format; interaction (hover, drag, zoom) in ImUml; hand-placed node
positions.

Owns or changes:

| Path | Change |
|---|---|
| `imuml/` | New: the ImUml library (`imuml.h`, `imuml_internal.h`, `imuml.cpp`, `imuml_items.cpp`, `imuml_render.cpp`, `imuml_graph_layout.cpp`, `imuml_sequence_layout.cpp`, `CMakeLists.txt`). |
| `src/elements/uml/` | New: the diagram elements and `register_uml()`. |
| `src/elements/register_builtin.cpp` | Calls `register_uml()`. |
| `src/schema/` | New field type `text_array` (section 3). |
| `src/document/bind.cpp` | `$column` into a `text_array` field. |
| `src/cli/schema_doc.cpp` | Prints `text_array` fields. |
| `src/render/figure_renderer.cpp` | Creates and restores an ImUml context next to ImPlot's; applies the theme to ImUml's style. |
| `src/core/limits.hpp` | Diagram size limits (section 8). |
| `tests/`, `examples/`, `docs/` | Tests, one example per diagram, element and agent docs. |

## 2. Design Goals

1. **An ImGui library first.** ImUml is usable by any ImGui program, with
   ImGui's conventions (a context, a style, `Begin*`/`End*` scopes, items
   submitted every frame, `ImDrawList` drawing). nymph is its first user,
   not its only possible one.
2. **Automatic layout.** As in mermaid, the source lists nodes and edges;
   positions are computed. An agent never writes coordinates.
3. **Deterministic.** Same source and same build give the same PNG bytes
   (core DESIGN goal 3): the layout has no randomness, no hash-order
   iteration and a fixed number of iterations, and its result is rounded
   to whole pixels.
4. **Strict, like the rest of nymph.** Every id a field refers to must be
   declared; a misplaced child, a reference to an unknown id, or an edge
   UML forbids (into an initial state) is an error positioned at the
   field, with a suggestion. ImUml itself never fails: it skips what it
   cannot draw, since nymph has already checked.
5. **Fits or fails.** A diagram is scaled down to fit its rectangle, but
   not below `min_zoom`; beyond that it is a layout error with the numbers,
   never a diagram too small to read (core DESIGN section 7, "Overflow is
   an error").
6. **Theme and text style inherited.** Diagrams follow `image.theme` and
   the `font` / `font_size` / `text_color` inheritance exactly as plots do.

## 3. Source Format

A diagram is a `figure` element: it can be `root` or a layout child, takes
`width` / `height` hints (section 6) and the text style fields
(`with_text_style()`). Children are listed in `children` as for plots;
their order is meaningful only in a sequence diagram (time) and as the
tie-breaker of the graph layout.

### Ids and references

Every node has an `id` (non-empty text, unique within the diagram,
including nested states). `name` is what is shown; it defaults to the id.
Fields that refer to nodes (`from`, `to`, `attach`, `over`) hold ids. An
unknown id is an error with a suggestion ("unknown id 'Aminal' (did you
mean 'Animal'?)"). Unlike mermaid, nothing is declared implicitly by being
referenced: a typo would otherwise draw a new box the agent cannot see.

### New field type: `text_array`

Member lists, activity lists and id lists are lists of strings:
`attributes: ["+name: string", "+age: int"]`. `field_type::text_array`
holds `std::vector<std::string>`; every item is text as written (the
`text` rules of core DESIGN section 4, `$$` included). `$col` into a
`text_array` gives the column's cells, so members can come from data.
An empty list is allowed.

### Shared diagram fields

| Field | Type | Default | Meaning |
|---|---|---|---|
| `title` | text | `""` | Drawn above the diagram, like a plot title. |
| `width`, `height` | number | `0` | Layout hints (core DESIGN section 3, "Layout"). |
| `direction` | enum | per diagram | `TopDown`, `BottomUp`, `LeftRight`, `RightLeft` (graph diagrams only; dot's `rankdir`, mermaid's TB, BT, LR, RL). |
| `edge_style` | enum | `Spline` | `Spline` or `Polyline` (graph diagrams only; dot's `splines`). |
| `node_spacing`, `rank_spacing` | number | style | Pixels between nodes in a rank, and between ranks (dot's `nodesep`, `ranksep`). |
| `min_zoom`, `max_zoom` | number | `0.5`, `1` | Allowed scale when fitting the rectangle (section 6). |
| `line_color`, `fill_color` | colour | theme | Default edge and node colours of the whole diagram. |
| `font`, `font_size`, `text_color` | | inherit | Text style. |

Every node and edge also takes `color` (border or line) and `fill_color`
(nodes), empty for the diagram's default.

Layout hints, after dot's attributes, so an agent can steer the layout
without coordinates (graph diagrams only):

| Field | On | Default | dot | Meaning |
|---|---|---|---|---|
| `weight` | edges | `1` | `weight` | How strongly the edge is kept short and straight (integer 0–100). |
| `min_length` | edges | `1` | `minlen` | Least number of ranks between the two ends (0 puts them in one rank). |
| `constraint` | edges | `true` | `constraint` | `false`: the edge is drawn but does not influence ranks. |
| `rank` | nodes | `""` | `rank=same` | Nodes with the same non-empty `rank` text are placed in one rank. |

### Class diagram

```yaml
root:
  type: ClassDiagram
  title: Animals
  children:
    - type: Class
      id: Animal
      kind: Abstract
      attributes: ["+name: string", "+age: int"]
      methods: ["+makeSound()*", "+isMammal() bool"]
    - {type: Class, id: Duck, attributes: ["+beakColor: string"], methods: ["+swim()"]}
    - {type: Class, id: Pond}
    - {type: ClassRelation, from: Duck, to: Animal, kind: Inheritance}
    - {type: ClassRelation, from: Pond, to: Duck, kind: Aggregation, from_label: "1", to_label: "*", label: has}
    - {type: Note, text: "Ducks can fly", attach: [Duck]}
```

`Class`: `id`, `name`, `kind` (`Class`, `Abstract`, `Interface`, `Enum`;
the last three show `«abstract»`, `«interface»`, `«enumeration»` and an
abstract name is italic), `stereotype` (free text, overrides the kind's),
`generic` (type parameters such as `T`, mermaid's `Animal~T~`: drawn in a
dashed box on the top-right corner), `attributes`, `methods` (`text_array`). Members are drawn
as written, with mermaid's two suffixes: a trailing `$` is static
(underlined) and a trailing `*` abstract (italic); the suffix is not
drawn. Compartments with no members are drawn empty, except that a class
with neither list is a single name box (mermaid's look).

`ClassRelation`: `from`, `to`, `kind`, `label`, `from_label`, `to_label`
(multiplicities, drawn next to each end). Kinds, and which way the layout
ranks the two ends:

| Kind | Line | Marker | Ranked first |
|---|---|---|---|
| `Association` | solid | none | `from` |
| `DirectedAssociation` | solid | open arrow at `to` | `from` |
| `Inheritance` | solid | hollow triangle at `to` | `to` (parents above) |
| `Realization` | dashed | hollow triangle at `to` | `to` |
| `Dependency` | dashed | open arrow at `to` | `from` |
| `Aggregation` | solid | hollow diamond at `from` | `from` (the whole) |
| `Composition` | solid | filled diamond at `from` | `from` |

### Sequence diagram

```yaml
root:
  type: SequenceDiagram
  autonumber: true
  children:
    - {type: Participant, id: user, name: User, kind: Actor}
    - {type: Participant, id: web, name: Web app}
    - {type: Participant, id: db, name: Database, kind: Database}
    - {type: Message, from: user, to: web, text: "POST /login", activate: true}
    - type: Fragment
      kind: Alt
      label: valid password
      children:
        - {type: Message, from: web, to: db, text: load user}
        - {type: Message, from: db, to: web, text: row, kind: Reply}
        - {type: Else, label: otherwise}
        - {type: Message, from: web, to: user, text: "401", kind: Reply}
    - {type: Message, from: web, to: user, text: "200 OK", kind: Reply, deactivate: true}
    - {type: Note, text: session cookie set, over: [user, web]}
```

Children are in time order, top to bottom. `SequenceDiagram` fields:
`autonumber` (bool), `mirror_participants` (bool, default true: the
participant boxes are repeated at the bottom, as mermaid does), plus the
shared fields except `direction`, `edge_style` and `rank_spacing`.

- `Participant`: `id`, `name`, `kind` (`Participant`, `Actor`,
  `Boundary`, `Control`, `Entity`, `Database`), `created` (bool: the box
  appears at the first message to it, which must have `kind: Create`).
  Columns are in declaration order; participants may be declared only
  before the first message.
- `Message`: `from`, `to` (may be equal: a self message), `text`, `kind`
  (`Sync` solid with a filled arrow, `Async` solid with an open arrow,
  `Reply` dashed with an open arrow, `Create`, `Destroy`: the target's
  lifeline ends in a cross at this row), `activate` (bool: starts an
  activation on `to`), `deactivate` (bool: ends one on `from`), mermaid's
  `+` / `-`.
- `Fragment`: `kind` (`Loop`, `Alt`, `Opt`, `Par`, `Critical`, `Break`,
  `Group`; `Group` is mermaid's `rect`, a filled box with no tab), `label`,
  `over` (ids of the leftmost and rightmost participant it covers; empty:
  those its messages touch). Accepts sequence items, nested fragments
  included.
- `Else`: `label`. Separates the operands of an `Alt`, `Par` or
  `Critical` fragment; anywhere else, first or last in its fragment, it is
  an error.
- `Note`: `text`, and `over` (one or two ids) or `attach` (one id) with
  `side: Left | Right` (default `Right`).

Errors beyond references: `deactivate` on a participant with no active
activation, a message from or to a destroyed participant, a `Create`
message to a participant without `created: true` or not as its first
message, an `Else` misplaced.

### State diagram

```yaml
root:
  type: StateDiagram
  direction: LeftRight
  children:
    - {type: State, id: start, kind: Initial}
    - {type: State, id: idle, name: Idle}
    - {type: State, id: running, name: Running, activities: ["entry / start timer", "exit / stop timer"]}
    - {type: State, id: done, kind: Final}
    - {type: Transition, from: start, to: idle}
    - {type: Transition, from: idle, to: running, label: "start [ready] / log"}
    - {type: Transition, from: running, to: idle, label: pause}
    - {type: Transition, from: running, to: done}
```

- `State`: `id`, `name`, `kind` (`Simple`, `Initial`, `Final`, `Choice`,
  `Fork`, `Join`, `History`, `DeepHistory`), `activities` (`text_array`,
  drawn in a second compartment). A `Simple` state with child states,
  transitions and notes is a composite state: its children are laid out
  as a diagram of their own inside it.
- `Transition`: `from`, `to`, `label` (mermaid's `event [guard] / action`,
  as text).

Errors: a transition into an `Initial` or out of a `Final` state;
children or activities on a pseudo-state; an `Initial` state with more
than one outgoing transition; a transition between a composite state and
a state inside it.

### Activity diagram

```yaml
root:
  type: ActivityDiagram
  children:
    - {type: ActivityControl, id: start, kind: Initial}
    - {type: Action, id: receive, text: Receive order}
    - {type: ActivityControl, id: stock, kind: Decision}
    - {type: Action, id: ship, text: Ship}
    - {type: Action, id: cancel, text: Cancel order}
    - {type: ActivityControl, id: merge, kind: Merge}
    - {type: ActivityControl, id: end, kind: Final}
    - {type: Flow, from: start, to: receive}
    - {type: Flow, from: receive, to: stock}
    - {type: Flow, from: stock, to: ship, guard: in stock}
    - {type: Flow, from: stock, to: cancel, guard: else}
    - {type: Flow, from: ship, to: merge}
    - {type: Flow, from: cancel, to: merge}
    - {type: Flow, from: merge, to: end}
```

- `Action`: `id`, `text`, `kind` (`Action` rounded box, `Send` and
  `Receive` signal shapes).
- `ActivityControl`: `id`, `kind` (`Initial`, `Final`, `FlowFinal`,
  `Decision`, `Merge`, `Fork`, `Join`), `label` (drawn next to it).
- `Flow`: `from`, `to`, `guard` (drawn as `[guard]`).

Errors: a flow into `Initial`, out of `Final` / `FlowFinal`.

### Use case diagram

```yaml
root:
  type: UseCaseDiagram
  system: Online shop
  children:
    - {type: Actor, id: customer, name: Customer}
    - {type: Actor, id: bank, name: Bank, side: Right}
    - {type: UseCase, id: buy, name: Buy item}
    - {type: UseCase, id: pay, name: Pay}
    - {type: UseCaseRelation, from: customer, to: buy}
    - {type: UseCaseRelation, from: buy, to: pay, kind: Include}
    - {type: UseCaseRelation, from: pay, to: bank}
```

- `UseCaseDiagram`: `system` (text: the boundary's title; empty draws no
  boundary); `direction` defaults to `LeftRight`.
- `Actor`: `id`, `name`, `side` (`Left`, `Right`): actors are placed in
  the first or last rank, outside the boundary.
- `UseCase`: `id`, `name`.
- `UseCaseRelation`: `from`, `to`, `kind` (`Association` solid line,
  `Include` and `Extend` dashed open arrows labelled `«include»` /
  `«extend»`, `Generalization` hollow triangle at `to`).

Errors: `Include` / `Extend` not between use cases, `Association` not
between an actor and a use case, `Generalization` between different kinds.

### ER diagram

Database structure, after mermaid's `erDiagram`, in crow's foot notation:

```yaml
root:
  type: ERDiagram
  children:
    - {type: Entity, id: customer, name: CUSTOMER, attributes: ["int id PK", "string email UK"]}
    - {type: Entity, id: order, name: ORDER, attributes: ["int id PK", "int customer_id FK \"buyer\""]}
    - {type: Relationship, from: customer, to: order, from_cardinality: ExactlyOne, to_cardinality: ZeroOrMore, label: places}
```

- `Entity`: `id`, `name` (the table name), `attributes` (`text_array`),
  `rank`. Each attribute is mermaid's `type name`, then optional keys
  `PK`, `FK`, `UK` (separated by commas or spaces) and a double-quoted
  comment; anything else after the name is an error at the field, naming
  the item. Drawn as a table: a header over one row per attribute, in
  aligned columns type | name | keys | comment (an empty column takes no
  space).
- `Relationship`: `from`, `to` (entities), `from_cardinality`,
  `to_cardinality` (`ExactlyOne` ||, `ZeroOrOne` o|, `ZeroOrMore` o{,
  `OneOrMore` |{; defaults `ExactlyOne` and `ZeroOrMore`, a one-to-many),
  `label`, `identifying` (default true: solid; false: dashed). Each
  cardinality is drawn at its own end, the "many" crow's foot spreading at
  the entity and the minimum (circle or bar) behind it. The layout ranks
  `from` first.

### Data flow diagram

Yourdon / DeMarco notation, with threat-modelling trust boundaries:

```yaml
root:
  type: DataFlowDiagram
  children:
    - {type: ExternalEntity, id: user, name: User}
    - type: TrustBoundary
      id: app
      name: App
      children:
        - {type: Process, id: login, name: Log in, number: "1"}
        - {type: DataStore, id: users, name: Users, number: D1}
    - {type: DataFlow, from: user, to: login, label: credentials}
    - {type: DataFlow, from: login, to: users, label: lookup, bidirectional: true}
```

- `ExternalEntity` (a box with a heavy border), `Process` (a circle, its
  `number` above a rule over the name) and `DataStore` (a box open on the
  right, its `number` in a left cell): `id`, `name`, `number` (not on
  entities), `rank`, colours.
- `DataFlow`: `from`, `to`, `label`, `bidirectional` (an arrowhead at both
  ends), the edge layout fields. One end must be a `Process` (entities and
  stores only exchange data through a process); neither may be a
  `TrustBoundary`.
- `TrustBoundary`: `id`, `name`, colours, children (nodes, nested
  boundaries, notes). A dashed box titled at its top left, laid out as a
  cluster (section 4). The diagram defaults to `LeftRight`.

### Network diagram

```yaml
root:
  type: NetworkDiagram
  children:
    - {type: Device, id: fw, name: Firewall, kind: Firewall, address: 203.0.113.1}
    - type: Zone
      id: lan
      name: LAN
      children:
        - {type: Device, id: sw, name: Switch, kind: Switch}
    - {type: Link, from: fw, to: sw, from_label: eth1}
```

- `Device`: `id`, `name`, `kind` (`Server`, `Database`, `Storage`,
  `Router`, `Switch`, `Firewall`, `LoadBalancer`, `AccessPoint`, `Cloud`,
  `Internet`, `Workstation`, `Laptop`, `Mobile`, `User`), `address`,
  `rank`, colours. Drawn as a line icon over the name, the address below
  in the lifeline colour; icons are drawn from primitives, so no image
  assets ship.
- `Link`: `from`, `to` (devices, not zones), `kind` (`Wired` solid,
  `Wireless` dashed, `Vpn` long heavy dashes), `label`, `from_label` /
  `to_label` (ports, at each end), `directed` (an arrow at `to`), the edge
  layout fields. Links are undirected by default; the layout still ranks
  `from` first.
- `Zone`: as `TrustBoundary`, for subnets, VLANs, sites.

### Flowchart

Added after the UML diagrams, for what agents draw most that is not UML:
processes, decision trees, dependency and architecture graphs (mermaid's
`flowchart`). Same layout, fields and `Note` as the graph diagrams.

```yaml
root:
  type: Flowchart
  children:
    - {type: FlowNode, id: start, text: Push, shape: Stadium}
    - {type: FlowNode, id: ok, text: Tests pass?, shape: Diamond}
    - type: FlowGroup
      id: prod
      title: Production
      children:
        - {type: FlowNode, id: deploy, text: Deploy}
    - {type: FlowLink, from: start, to: ok}
    - {type: FlowLink, from: ok, to: prod, label: "yes", line: Thick}
```

- `FlowNode`: `id`, `text` (default the id; `\n` breaks lines), `shape`
  (`Box`, `Rounded`, `Stadium`, `Diamond`, `Circle`, `Parallelogram`,
  `Hexagon`, `Cylinder`, `Subroutine`: mermaid's common shapes), `rank`.
  A diamond is twice as wide as tall around its text; a circle's
  diameter is its text box's diagonal.
- `FlowGroup`: `id`, `title`; children are nodes, groups, links and notes.
  A group with children is a cluster, exactly as a composite state (the
  same code path: `Parent` and `OpenStates`); an empty one is a titled
  box. Its inside is drawn halfway between the background and the node
  fill, so its nodes stand out.
- `FlowLink`: `from`, `to` (nodes or groups: a link to a group ends at
  its box, as a transition to a composite state), `label`, `line`
  (`Solid`, `Dotted`, `Thick`: twice the line weight), `arrow` (`End`,
  `None`, `Both`), and the edge layout fields. mermaid's `-->`, `---`,
  `-.->`, `==>`, `<-->`.

Errors: a link between a group and a node inside it (as between a
composite state and its states). Links may cross group borders freely.

`FlowGroup` is not the `BeginGroup()` of data flow and network diagrams:
those are dashed outlines that edges never end at, while a flowchart
group is a filled, titled box that links may name, as mermaid's
subgraphs.

### `Note`

One element class for every diagram (slot `uml.note`): `text`, `attach`
(ids; a graph note is drawn near the first and joined to each by a dashed
line; none: a free note, laid out as a node), `over` and `side` (sequence
diagrams only; an error elsewhere).

## 4. Key Abstractions

```
 nymph side                                      ImUml side (imuml/)
 ─────────────────────────────────────────────   ──────────────────────────────────────
 bind_source() ── check(): ids, kinds, rules
       │
 figure ──► diagram element::arrange()  ───────► BeginClassDiagram(MeasureOnly) ... End
       │      (fit check, needs fonts)               └ items recorded → graph layout → size
       └──► diagram element::draw()     ───────► BeginClassDiagram(id, rect)
              dc.draw_children()        ───────►   Class / ClassRelation / Note items
                                        ───────► EndClassDiagram(): layout, fit, draw on
                                                   the window's ImDrawList
```

### ImUml (`imuml/`)

An ImGui extension written like ImPlot: C++ with ImGui's types and
containers (`ImVector`, `ImGuiStorage`), no STL, no exceptions,
`ImUml::` functions and `ImUml*` types, `CamelCase` names. That is the
naming convention of this subsystem (CLAUDE.md: follow each subsystem's
naming); nymph's own code keeps `snake_case`. It depends only on ImGui.

**Context and style.** `ImUmlContext` holds the style, the diagram pool
keyed by `ImGuiID`, the diagram being submitted and scratch buffers.
`CreateContext()`, `DestroyContext()`, `GetCurrentContext()`,
`SetCurrentContext()`, as ImPlot. `ImUmlStyle` has variables
(`LineWeight`, `NodePadding`, `NodeRounding`, `NodeSpacing`,
`RankSpacing`, `ArrowSize`, `DashLength`, `GapLength`,
`ParticipantSpacing`, `MessageSpacing`, `ActivationWidth`, `NoteFold`,
`MaxZoom`) and colours `ImUmlCol_` (`Line`, `NodeFill`,
`NodeBorder`, `Text`, `NoteFill`, `NoteBorder`, `FragmentBorder`,
`FragmentTab`, `Activation`, `Lifeline`, `EdgeLabelBg`, `Boundary`,
`Title`, `Bg`). As in ImPlot, a colour may be `IMUML_AUTO_COL`, resolved
from the current ImGui style at `Begin*` (text from `ImGuiCol_Text`,
fills and borders blended from `ImGuiCol_WindowBg`, `ImGuiCol_ButtonActive`
and the text colour), so diagrams follow the ImGui theme with no work.
`StyleColorsLight/Dark/Classic()` compute the same values from ImGui's
own themes; `StyleColorsAuto()` restores automatic colours.
`PushStyleColor/Var`, `PopStyleColor/Var`, `GetColorU32`. Next-item
setters, as ImGui's `SetNextItem*`: `SetNextItemColors(line, fill)`,
`SetNextNodeRank(group)` (dot's rank=same) and
`SetNextEdgeLayout(weight, min_length, constraint)`. The minimum zoom is
not ImUml's business: a library draws what fits; nymph checks `min_zoom`
itself (section 6).

**Diagram scopes.** One pair per diagram kind:
`BeginClassDiagram(title_id, size, flags)` / `EndClassDiagram()`, and the
same for `Sequence`, `State`, `Activity`, `UseCase`. `Begin*` returns
false when the diagram is clipped (then items and `End*` are skipped, as
ImGui's convention), reserves `size` at the cursor (`-1` fills the
available width or height, `0` the natural size), and starts recording.
`Setup*` calls after `Begin*` and before the first item configure it, as
ImPlot's `SetupAxes`: `SetupLayout(direction, edge_style)`,
`SetupSystemBoundary(label)`, `SetupSequence(flags)`.

**Items** are recorded, not drawn, because positions are known only once
every node and edge has been submitted. The context copies their text
into a per-diagram buffer and remembers the current font, font size and
text colour with each item, so ImGui's push stacks apply per item:

| Diagram | Items |
|---|---|
| Class | `BeginClass(id, name, kind, stereotype)`, `ClassAttribute(text)`, `ClassMethod(text)`, `EndClass()`; `Relation(from, to, kind, label, from_label, to_label)` |
| Sequence | `Participant(id, name, kind, flags)`, `Message(from, to, text, kind, flags)`, `BeginFragment(kind, label, over_from, over_to)`, `FragmentElse(label)`, `EndFragment()`, `NoteOver(text, a, b)`, `NoteSide(text, id, side)` |
| State | `BeginState(id, name, kind)`, `StateActivity(text)`, `EndState()` (states and transitions between them are nested children), `Transition(from, to, label)` |
| Activity | `Action(id, text, kind)`, `ControlNode(id, kind, label)`, `Flow(from, to, guard)` |
| Use case | `Actor(id, name, side)`, `UseCase(id, name)`, `UseCaseLink(from, to, kind)` |
| ER (`Begin/EndERDiagram`) | `BeginEntity(id, name)`, `EntityAttribute(type, name, keys, comment)`, `EndEntity()`; `Relationship(from, to, from_cardinality, to_cardinality, label, flags)` |
| Data flow (`Begin/EndDataFlowDiagram`) | `ExternalEntity(id, name)`, `Process(id, name, number)`, `DataStore(id, name, number)`, `DataFlow(from, to, label, flags)`, `BeginGroup(id, label)` / `EndGroup()` |
| Network (`Begin/EndNetworkDiagram`) | `Device(id, name, kind, address)`, `Link(from, to, kind, label, from_label, to_label, flags)`, `BeginGroup(id, label)` / `EndGroup()` |
| Flowchart | `FlowNode(id, text, shape)`, `BeginFlowGroup(id, title)` / `EndFlowGroup()`, `FlowLink(from, to, label, flags)` |
| Any graph | `Note(text, attach_ids, count)` |

Ids are strings hashed with `ImHashStr` into `ImGuiID`s within the
diagram's ID scope. An edge whose end was never submitted is skipped and
counted (`GetLastDiagramInfo().SkippedItems`); nymph's checks make this
unreachable.

**`End*`** measures every node at zoom 1 (text with
`ImFont::CalcTextSizeA`), runs the layout engine of its kind, computes
the zoom that fits the reserved size (section 6), and draws on
`ImGui::GetWindowDrawList()` at that zoom, text at `font_size * zoom`.
With `ImUmlDiagramFlags_MeasureOnly` it stops after the layout: nothing
is drawn, no space is reserved, and `GetLastDiagramInfo()` gives the
natural size. The layout is cached in the diagram's pool entry under a
hash of the recorded items, so an unchanged diagram is laid out once
across frames (nymph runs three frames).

**Rendering primitives** (`imuml_internal.h`, like ImGui's `RenderFrame`
and `RenderArrow`): `RenderClassBox` (name, stereotype, compartments),
`RenderNote` (folded corner), `RenderActor` (stick figure),
`RenderUseCase` (ellipse with text), `RenderStateBox`,
`RenderPseudoState` (initial dot, final bullseye, flow-final cross,
choice / decision diamond, fork / join bar, history `H` / `H*`),
`RenderSignal` (send / receive), `RenderParticipant` (box, actor,
boundary, control, entity, database cylinder), `RenderPath` (polyline or
spline, solid or dashed by arc length), `RenderArrowHead` (open, filled,
hollow triangle, hollow / filled diamond, destroy cross), `RenderEdgeLabel`
(text on a background box), `RenderFragment` (box, pentagon tab,
operand separators), `RenderBoundary`, `RenderCardinality` (crow's foot
ends), `RenderDeviceIcon` (one line icon per `ImUmlDeviceKind`). Each takes a rectangle or points,
a zoom and colours; curves and circles use an explicit segment count, so
the geometry does not depend on ImGui's adaptive tessellation.

### Graph layout (`imuml_graph_layout.cpp`)

The class, state, activity and use case diagrams share one layered
layout: the algorithm of Graphviz's `dot`, as published in Gansner,
Koutsofios, North and Vo, "A Technique for Drawing Directed Graphs"
(IEEE TSE, 1993), implemented from the paper (Graphviz's source, under
the Eclipse Public License, is read for behaviour, never copied; section
7). mermaid's dagre is a port of the same paper. The layout is a pure
function: nodes with sizes, edges with label sizes, weights, minimum
lengths and constraint flags, rank groups and a direction in; node
centres, edge paths and label positions out. No ImGui context.

1. **Components**: connected components in order of their first node,
   each laid out alone, then packed into rows (dot with `pack`) aiming at
   a 1.6:1 block in the output, so unconnected nodes do not make one long
   row.
2. **Acyclic**: a depth-first search in submission order reverses back
   edges for ranking; they are drawn with their real direction. Edges with
   `constraint: false` and self-loops are left out of ranking.
3. **Rank** (dot phase 1): minimise the sum of `weight * length` over
   edges subject to `length >= min_length`, solved with **network
   simplex**: a feasible tight spanning tree, then pivots on the tree edge
   with a negative cut value, chosen by a cyclic search with dot's search
   limit, until none is negative or the iteration cap is reached; ranks
   normalised to start at 0 and balanced as in the paper. Nodes in one
   `rank` group are merged into one ranking node first; use case actors
   are then moved to a rank before (or after) every other node; the
   per-kind "ranked first" end of section 3 decides each edge's
   direction.
4. **Virtual nodes**: an edge spanning several ranks becomes a chain of
   virtual nodes, one per rank crossed; an edge label becomes a virtual
   node of the label's size in the middle rank (dot's label handling), so
   ranks are spaced to fit labels.
5. **Order** (dot phase 2, `mincross`): initial order by depth-first
   search from the nodes in rank order; then 24 iterations, alternating down
   and up, each ordering a rank by the **weighted median** of its
   neighbours' positions (nodes with no neighbours keep their place),
   followed by **transpose**: swapping adjacent nodes while that lowers
   the crossings. Ties are broken by current position, and the order with
   the fewest crossings is kept. A node joined only by same-rank edges (a
   note) is then moved next to the node it is attached to.
6. **Position** (dot phase 3): x coordinates by network simplex on dot's
   auxiliary graph: each edge becomes a new node with two edges whose
   weight is the edge's `weight` times dot's factor (1 between real nodes,
   2 with one virtual end, 8 between two virtual nodes, so long edges
   straighten), and neighbours in a rank are separated by half their
   widths plus `node_spacing`. The same solver as step 3. Then dot's final
   balancing: the solver leaves a node whose cost is flat between its
   neighbours (a parent over two children) at one end, so each node moves
   to the middle of its neighbours when that costs no more. y
   coordinates: ranks stacked with `rank_spacing` plus the tallest node of
   each rank (half `rank_spacing` when labels doubled the ranks).
7. **Edges** (dot phase 4, simplified): a path through the virtual node
   centres, ends clipped to the node's shape (rectangle, ellipse, circle,
   diamond, bar). A label's room is reserved to the right of its virtual
   node, so the edge runs straight through the node's centre and the label
   sits beside it, as dot draws edge labels. `Polyline` keeps the straight
   segments; `Spline` turns the path into cubic Béziers whose cross
   coordinate is a monotone cubic of the rank coordinate (Fritsch-Carlson,
   as d3's monotone curves): the tangent runs along the rank axis where
   the path turns and is the harmonic mean of the neighbouring slopes
   elsewhere. The curve never swings past its points sideways, straight
   runs stay straight and a two-point edge is a straight line. Parallel
   edges between the same two nodes are fanned apart; self-loops are a
   loop on the node's side away from the flow.
8. **Direction**: computed top-down; `BottomUp`, `LeftRight`, `RightLeft`
   by swapping node width and height before and transforming after (dot's
   `rankdir`).
9. **Pixels**: every coordinate is rounded to a whole logical pixel.

**Composite states and groups** (`TrustBoundary`, `Zone`, recorded by
`BeginGroup`) are dot's clusters (`ImUmlGraphInput::Clusters`,
`ImUmlGraphNode::Cluster`), laid out with the whole graph, not on their
own, so every edge is ranked, ordered and routed knowing every box:

1. **Extended graph.** Each cluster gets a top and a bottom node. Hidden
   edges (weight 1, drawn never) keep the top node above and the bottom
   node below every member and nested cluster, so a cluster spans
   consecutive ranks and stays compact. The top node's rank holds the
   header band (the bottom node's for `BottomUp`; for `LeftRight` /
   `RightLeft` the band is margin on the rank-start side). An edge to a
   composite ends at the box side facing its other end, chosen after
   ranking.
2. **Membership.** A node belongs to its innermost cluster; an edge's
   virtual nodes belong to the innermost cluster holding both ends, so an
   edge leaving a box has its route points outside it.
3. **Borders.** Every rank a cluster spans gets a left and a right border
   node, joined rank to rank by weightless segments: the ordering then
   counts an edge passing over a box as crossings.
4. **Ordering** sorts each rank hierarchically (dot's recursive mincross,
   simplified): nodes and sibling boxes by median, a box by its members'
   mean, then each box's contents; a box's borders stay first and last.
   Transpose swaps only within one cluster, or moves a node across a whole
   neighbouring box. A final pair of sweeps (down, then up) moves each
   edge's run of virtual nodes to one side of a box, which single moves
   cannot do because each only shifts where the edge would cross; the
   order with the fewest crossings is kept. Last, every rank is re-sorted
   by one key per box (its borders' mean position over all its ranks), so
   sibling boxes keep the same left-to-right order on every rank.
5. **Position.** Borders are separated from contents by the cluster's
   padding, aligned rank to rank by heavy auxiliary edges (weight 1000,
   so a box is a rectangle), and the header rank is at least as wide as
   the header. A node or box beside a cluster on one rank is also kept
   clear of that cluster's borders on every other rank it spans (hard
   constraints), so sibling boxes never overlap.
6. **Box and routes.** A box spans its borders across and its contents
   plus padding and header along the ranks. Edge ends at a box are
   clipped to it; a segment still crossing a box neither end belongs to
   (between two ranks) bends around the box's nearest corner, or two.

The use case system boundary is drawn around the use cases' bounding box,
not as a cluster.

**Notes** with `attach` are placed as nodes in the rank of their first
attached node, next to it (a dashed connector, no ranking edge); a note
attached to a node inside a composite state is laid out inside that
composite. Free notes are nodes of their own component.

### Sequence layout (`imuml_sequence_layout.cpp`)

A pure function, separate from the graph layout because a sequence
diagram is a grid, not a graph:

1. **Columns**: participants in order, each as wide as its box. The gap
   between two adjacent columns starts at `ParticipantSpacing` and grows
   to fit the widest message label between them; a label spanning several
   columns grows the gaps it spans evenly, processed from the shortest
   span up. Notes beside a participant widen its outer gap.
2. **Rows**: one per message (self messages two), note, fragment header,
   `Else` and fragment footer, in order, each as tall as its text plus
   `MessageSpacing`.
3. **Activations**: a stack per participant; nested activations are
   offset sideways by half `ActivationWidth`; a message starts or ends at
   the outer edge of the participant's bars on its side (the first bar it
   meets).
4. **Fragments**: rows from header to footer; horizontally the covered
   participants plus a margin per nesting level.
5. **Lifelines** run from the header box (or the creating row) to the
   footer box (or the destroying row).

### nymph elements (`src/elements/uml/`)

| File | Contents |
|---|---|
| `uml_common.hpp/.cpp` | The shared diagram fields, `Note`, the id table and its checks (unique, known, suggestion), the fit check, enum name tables shared across diagrams, and `diagram_scope`: begins and ends an ImUml diagram at a rectangle, in measure or draw mode. |
| `class_diagram.cpp` | `ClassDiagram`, `Class`, `ClassRelation`. |
| `sequence_diagram.cpp` | `SequenceDiagram`, `Participant`, `Message`, `Fragment`, `Else`. |
| `state_diagram.cpp` | `StateDiagram`, `State`, `Transition`. |
| `activity_diagram.cpp` | `ActivityDiagram`, `Action`, `ActivityControl`, `Flow`. |
| `usecase_diagram.cpp` | `UseCaseDiagram`, `Actor`, `UseCase`, `UseCaseRelation`. |
| `er_diagram.cpp` | `ERDiagram`, `Entity` (attribute syntax check), `Relationship`. |
| `dataflow_diagram.cpp` | `DataFlowDiagram`, `ExternalEntity`, `Process`, `DataStore`, `DataFlow`, `TrustBoundary` (the process-end rule). |
| `network_diagram.cpp` | `NetworkDiagram`, `Device`, `Link`, `Zone`. |
| `flowchart.cpp` | `Flowchart`, `FlowNode`, `FlowGroup`, `FlowLink`. |
| `uml.hpp` | `register_uml(registry&)` and the slot names. |

Slots: each diagram has slot `figure` and accepts its item slot and
`uml.note`: `uml.class.item`, `uml.sequence.item`, `uml.state.item`,
`uml.activity.item`, `uml.usecase.item`, `uml.er.item`,
`uml.dataflow.item`, `uml.network.item`, `uml.flow.item`; the groups
(`TrustBoundary`, `Zone`, `FlowGroup`) accept their diagram's too.
`Fragment` accepts
`uml.sequence.item` and `uml.note`; `State` accepts `uml.state.item` and
`uml.note`. Everything else accepts nothing.

Each child class's `draw()` issues its ImUml item call. The diagram's
`draw()` opens the ImUml scope at its rectangle, calls
`dc.draw_children()` and closes it, like `Plot`. The diagram's `check()`
walks its subtree once and runs every reference and kind rule of section
3, because a child's `check()` cannot see its siblings.

## 5. Data Flow

1. `bind_source()` binds the diagram subtree like any other; the diagram's
   `check()` validates ids, references and UML rules (section 3). Errors
   stop here, positioned at the field.
2. Layout pass, inside the first frame: a diagram with a 0 hint is
   measured with `diagram_scope` in measure mode (items submitted, ImUml
   lays out, returns its natural size plus the title). In `arrange()`, the
   diagram compares its natural size with its rectangle (section 6); a
   misfit is an error, found by `nymph check` too.
3. Draw pass: `draw()` submits the same items in draw mode; `End*` lays
   out (from the cache after the first frame), fits and draws.
4. The draw lists go to the rasterizer unchanged; nothing in the
   rasterizers or the PNG code changes.

## 6. Public Contract

### Sizing and fit

- Natural size: the laid-out diagram at zoom 1 plus its title. A diagram
  with 0 hints in a layout takes its natural size; as `root` with no
  `width` / `height` it fills the content rectangle (as a root `Plot`).
- Drawing: zoom = min(available / natural) on both axes, capped at
  `max_zoom` (default 1: a small diagram is not blown up; set 2 to fill a
  large image), and centred in the rectangle. The title is drawn at full
  size.
- If the zoom needed is below `min_zoom`, `arrange()` fails: "ClassDiagram
  needs 1240 x 860 px (at min_zoom 0.5: 620 x 430), has 560 x 400",
  positioned at the element, so the agent enlarges the image, splits the
  diagram or lowers `min_zoom`.

### ImUml API (`imuml/imuml.h`)

The functions of section 4 plus `GetLastDiagramInfo()` (natural size,
its content and title parts, size reserved, zoom used, node and edge
counts, skipped items). Misuse (an item outside
a scope, mismatched `Begin`/`End`) is an `IM_ASSERT`, as in ImGui;
anything a source could cause is drawn as well as possible, never
asserted. `imuml_internal.h` exposes the layout functions and the last
layout's node rectangles and edge points, for tests and tools.

### nymph

No new library function or command. `nymph schema` and
`docs/elements.md` list the new elements; `nymph schema ClassDiagram`
prints one.

## 7. Design Decisions

- **A library of our own, ImGui-style, not a port of mermaid.** mermaid
  is JavaScript drawing SVG with dagre; nothing of it runs inside an ImGui
  frame. Writing ImUml as an ImGui extension keeps diagrams on the same
  path as plots (one frame, one draw list, both rasterizers, the theme
  and font stacks) and leaves a library that wish or any ImGui program can
  use. Rejected: rendering through Graphviz or mermaid-cli (external
  programs or large dependencies, not deterministic across versions, Goal
  6 of core DESIGN), and drawing straight from the nymph elements (the
  drawing would not be reusable and would mix schema and geometry).
- **ImUml in `imuml/`, not `extern/`.** It is nymph's code, developed and
  tested here; `extern/` holds unmodified third-party submodules that
  CLAUDE.md says not to edit. A top-level folder with its own CMake
  target keeps it free of nymph includes and easy to split out later.
- **Deferred items, drawn at `End*`.** ImPlot draws each item at once
  because its axes are known after `BeginPlot`; a diagram's positions are
  not known until every node and edge is in. ImGui's tables solve the same
  problem the same way (sizes resolved at `EndTable`). Rejected: a
  retained graph object built by the caller (not ImGui's style, and the
  caller would own lifetimes ImGui users do not expect).
- **Measure-only mode instead of a separate measuring API.** nymph's
  layout must know the natural size before drawing; submitting the same
  items with a flag reuses one code path, so measuring and drawing cannot
  disagree.
- **dot's algorithm, implemented from its paper.** Graphviz's `dot` is
  the reference for laying out a graph from text: its results are what
  people expect a generated diagram to look like, and its attributes
  (`weight`, `minlen`, `constraint`, `rank=same`) give an agent levers
  that need no coordinates. Network simplex gives compact ranks (short
  edges, few long ones) and, on the auxiliary graph, straight long edges
  and balanced parents; one solver serves both phases. Median plus
  transpose is dot's crossing reduction. Rejected: longest-path ranking
  with barycentre placement (the first draft: wider, more uneven layouts);
  Brandes-Köpf placement (a second algorithm to write, and dot's results
  are the reference); dot's box-constrained spline router (much of dot's
  code; corner detours cover the cases clusters leave).
- **Clusters in the whole layout, not composites laid out inside-out.**
  The first version laid out each composite's insides alone and treated
  the composite as one big node outside: an edge from an inner state to
  an outer one was seen by neither layout, so it was drawn straight across
  other states and edges. Laying out one graph with cluster constraints
  (dot's approach) lets ranking, ordering and routing see the whole edge,
  so it goes around the box. Cost: the hierarchical ordering, border
  nodes, and the side sweeps.
- **Monotone splines, not rank-axis tangents everywhere.** The first
  version gave every path point a tangent along the rank axis (the shape
  of dot's splines near virtual nodes); with no box router to go with it,
  every diagonal segment became an S and long edges wiggled. Catmull-Rom
  tangents fixed most of that but overshoot where a straight run meets a
  turn. A monotone cubic of the rank coordinate bends only where the path
  does and never overshoots, at the cost of a slightly sharper turn at
  the bend. Edge weights were not the cause: dot's 1 / 2 / 8 factors
  already keep long edges straight.
- **Graphviz as a reference, not a dependency.** Linking `libgvc` /
  `cgraph` would bring a large C library with plugins and its own
  configuration, under the Eclipse Public License (weak copyleft) into an
  MIT project that vendors only small libraries (core DESIGN goal 6), and
  its layout could change between Graphviz versions. Writing the four
  phases ourselves keeps them deterministic and in ImUml's style.
  Graphviz code is never copied into nymph; the paper is the source.
- **Fit by zoom with a floor.** mermaid scales its SVG to the page;
  doing the same keeps most diagrams usable at any image size, and the
  floor keeps text readable (Goal 5). Text is drawn at the zoomed size,
  not scaled after rasterizing, so it stays sharp (ImGui 1.92 bakes glyphs
  at any size).
- **Per-diagram element names, one `Note`.** `ClassRelation`,
  `Transition`, `Flow`, `UseCaseRelation` and `Message` each validate
  their own kinds and get their own schema text, which agents read; a
  single generic edge type would accept kinds that mean nothing in a given
  diagram. `Note` is the same everywhere, so it is one class.
- **Explicit declarations, no implicit nodes.** mermaid creates a node
  when an edge names it; with an agent that cannot see the image, a typo
  must be an error (core DESIGN goal 5).
- **`text_array` rather than newline-joined text.** A YAML list is how an
  agent naturally writes members, each item gets its own error position,
  and `$col` maps a column onto it. A `|` block in a text field would work
  but mixes "one member" and "one line of a wrapped member". Cost: one
  more `value` alternative.
- **Member markup is mermaid's suffixes only.** Visibility characters are
  drawn as written; parsing `+`, `-`, `#`, `~` into icons would add
  nothing an agent cannot write. Italic needs an italic font: ImUml
  takes `ImUmlStyle::ItalicFont`; nymph sets it to the current font's
  `-Italic` sibling when one is embedded (`FiraSans-Regular.ttf` →
  `FiraSans-Italic.ttf`), else abstract text is drawn upright with an
  `{abstract}` suffix.

## 8. Constraints and Invariants

- ImUml includes only `imgui.h` / `imgui_internal.h` and its own headers;
  nothing in it names nymph. nymph's elements include only `imuml.h`.
- Layout is a pure function of the recorded items, the fonts and the
  style: no global state, no time, no randomness, no iteration over
  pointer-keyed or hashed containers in an order that reaches the output.
  Coordinates are rounded to whole logical pixels. As with ImPlot, bytes
  are guaranteed per build, not across compilers (float layout).
- The ImUml context is created and destroyed by `render_frames()` with
  the ImPlot contexts, and the previously current one is restored, so
  rendering stays callable inside another ImGui program.
- Limits (in `src/core/limits.hpp`, checked in `check()`): at most 500
  nodes, 2000 edges and 2000 sequence rows per diagram, and composite
  states nested at most 8 deep. The layout's cost is bounded by these, the
  fixed mincross iterations and a network simplex iteration cap (dot's
  `nslimit`); a capped solve is still a valid, deterministic layout.
- Network simplex is deterministic: the initial tree, the entering and
  leaving edge searches and every tie follow submission order.
- No source text reaches printf: ImUml draws text with `AddText`, never
  through a format string.
- Platform: plain C++ on ImGui types; no OS calls. Built and tested on
  Linux, MSYS2 and MSVC like the rest (CLAUDE.md, MSVC template
  strictness).

## 9. Integration Boundaries

- Depends on core DESIGN: the element schema and slots (section 4), the
  layout engine's `measure()` / `arrange()` hooks, `draw_context`,
  `text_style_scope`, `render_frames()`; and on ImGui 1.92's
  `ImDrawList` and dynamic font sizes.
- Rasterizers ([spec/02_opengl_rasterizer](../02_opengl_rasterizer/DESIGN.md)):
  unchanged; diagrams are ordinary draw data.
- `register_builtin_elements()` gains `register_uml()`; the schema doc
  test keeps `docs/elements.md` in step.
- Nothing else depends on this project.

## 10. Extension Points

- **More diagrams** (component, deployment, package, object): new graph
  diagram kinds reuse the graph layout and add a `Begin*` pair, items and
  shapes; nymph adds one file in `src/elements/uml/`.
- **Activity swimlanes**: a `lane` field on activity nodes; the ordering
  step sorts each rank by lane first and the coordinate step confines
  each lane to a band.
- **Orthogonal edges** (`edge_style: Orthogonal`, dot's `splines=ortho`),
  and dot's box-constrained spline router for fewer edge-node overlaps.
- **Concurrent state regions**: composite states with several child
  layouts stacked and separated by dashed lines.
- **Interaction** in ImUml for live ImGui programs (hover highlight,
  dragging nodes, pan and zoom), behind flags off by default.
- **A mermaid importer** (`nymph import diagram.mmd`) that writes the
  equivalent YAML, if agents ask for it.
