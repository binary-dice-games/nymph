# nymph — Mermaid Import

nymph reads [mermaid](https://mermaid.js.org) diagrams wherever it reads a
chart, converts them to nymph YAML and draws that. It never writes mermaid:
outputs stay PNG or YAML, and a PNG drawn from mermaid carries the
converted YAML.

The idea: an AI agent knows mermaid well, so it drafts a diagram in
mermaid, revises it until the user is happy, then converts it. From then on
the chart is an ordinary nymph source, refined with nymph's own fields
(colours, fonts, shapes, layout hints, sizes) or in `nymph view --edit`.

## Workflow

| Step | Command line | MCP (`nymph mcp`) |
|---|---|---|
| Draw the mermaid | `nymph diagram.mmd` (writes `diagram.png`) | `render_chart` with the mermaid as `source`, or `input_path` |
| Check it | `nymph check diagram.mmd` | `check_chart` |
| Convert it | `nymph convert diagram.mmd -o diagram.yaml` (or `nymph diagram.mmd -o diagram.yaml`) | `convert_chart`, with `output_path` ending in `.yaml` |
| Edit it in the window | `nymph view diagram.mmd --edit`; Save writes `diagram.yaml` next to it | `editor_set_source` takes mermaid too |
| Show it in a web page | `<nymph-viewer src="diagram.mmd">` or `viewer.setSource(mermaid)` | |

Mermaid is recognised by its content, not its file name: the first line
that is not blank, a `%%` comment, a `%%{init}%%` directive or `---` front
matter starts with a diagram keyword (`flowchart`, `sequenceDiagram`, ...).
A text that is only a ` ```mermaid ` fenced block is read too. `.mmd` is
the usual extension.

**Errors** point at the mermaid: `diagram.mmd:4:10: unknown mermaid shape
'blob' (did you mean 'bolt'?)`. A problem nymph finds in the converted
source (a cycle in a Sankey diagram, a diagram over nymph's limits) names
the mermaid line it came from and ends with `(nymph YAML line N)`.

**Warnings** name what nymph dropped and do not stop the conversion:
`nymph: diagram.mmd:7:3: warning: click is ignored (a nymph figure has no
links or callbacks)`. They go to standard error on the command line and
into the result text over MCP.

The converted source declares every node explicitly, keeps the mermaid ids,
writes `image` at the size the diagram needs, and records `source:
Converted from a mermaid <kind>.`; the same mermaid always gives the same
YAML.

## What each kind becomes

| mermaid | nymph |
|---|---|
| `flowchart`, `graph` | `Flowchart` |
| `sequenceDiagram` | `SequenceDiagram` |
| `classDiagram` | `ClassDiagram` |
| `stateDiagram`, `stateDiagram-v2` | `StateDiagram` |
| `erDiagram` | `ERDiagram` |
| `gantt` | `GanttChart` |
| `pie` | `Plot` with a `PlotPieChart` |
| `mindmap` | `MindMap` |
| `sankey-beta` | `SankeyDiagram` |
| `xychart-beta` | `Plot` with `PlotBars` / `PlotBarsH` / `PlotLine` |

Other kinds (`journey`, `timeline`, `gitGraph`, `C4Context`, `block-beta`,
`architecture-beta`, `quadrantChart`, ...) are an error that lists these.

Everywhere: `title` (front matter or a `title` line) becomes the diagram's
`title`; `accTitle` / `accDescr` the source's `description`; `direction` /
`TB TD BT LR RL` the diagram's `direction`. Styling (`classDef`, `class`,
`:::name`, `style`, `linkStyle`) maps the CSS `fill`, `stroke` and `color`
to `fill_color`, `color` and `text_color`; other properties are warnings.
`%%{init}%%` themes and front matter `config` are warnings.

### Flowchart

| mermaid | nymph |
|---|---|
| `A`, `A[t]` | `FlowNode` (`Box`); a node named only in a link is declared where it is first named |
| `A(t)`, `A([t])`, `A[[t]]`, `A[(t)]`, `A((t))`, `A{t}`, `A{{t}}`, `A[/t/]` | `shape` `Rounded`, `Stadium`, `Subroutine`, `Cylinder`, `Circle`, `Diamond`, `Hexagon`, `Parallelogram` |
| `A[\t\]`, `A(((t)))`, `A>t]`, `A[/t\]`, `A[\t/]` | `Parallelogram`, `Circle`, `symbol: banner`, `symbol: trapezoid` |
| `A@{ shape: name, label: "t" }` | the shape below |
| `-->`, `---`, `-.->`, `-.-`, `==>`, `===`, `<-->`, `--o`, `--x` | `FlowLink` `line` (`Solid`, `Dotted`, `Thick`) and `arrow` (`End`, `None`, `Both`; circle and cross ends draw an arrow) |
| `-->\|t\|`, `-- t -->`, `-. t .->`, `== t ==>` | `label` |
| longer links (`---->`) | `min_length` |
| `A & B --> C --> D` | one link per pair |
| `subgraph id [title] ... end` | `FlowGroup` holding its nodes (a node belongs to the innermost subgraph that names it); a link may end at a group |
| `~~~`, `click`, `direction` in a subgraph, link ids and animation | warnings |

`@{ shape: ... }` names (each alias works too): `rect`, `rounded`,
`stadium`, `subproc`, `cyl`, `circle`, `dbl-circ`, `sm-circ`, `fr-circ`,
`f-circ`, `diam`, `hex`, `lean-r`, `lean-l`, `h-cyl`, `lin-cyl` and `text`
map to the shapes above; `trap-b` / `trap-t` → `trapezoid`, `doc`,
`lin-doc`, `tag-doc` → `document`, `docs` → `multi_document`, `delay`,
`manual-input` → `manual_input`, `cloud`, `notch-rect` → `card`, `tri` →
`triangle`, `extract`, `flip-tri` → `merge`, `fork` → `fork_bar`,
`hourglass` → `collate`, `cross-circ` → `summing_junction`, `brace` /
`brace-r` / `braces` → `brace_left`, `win-pane` → `internal_storage`,
`lin-rect`, `div-rect`, `tag-rect` → `process`, `curv-trap` → `display`,
`bow-rect` → `stored_data`, `odd`, `flag` → `banner`, `notch-pent` →
`pentagon` ([shapes.md](shapes.md) shows them); `bolt` is a box, with a
warning.

### Sequence diagram

| mermaid | nymph |
|---|---|
| `participant A as Name`, `actor A`, `participant A@{"type": "database"}` | `Participant` (`kind` `Actor`, `Boundary`, `Control`, `Entity`, `Database`); every participant is declared first, in order of appearance |
| `->>`, `->` / `-->>`, `-->` / `-)`, `--)` / `-x`, `--x` | `Message` `kind` `Sync` / `Reply` / `Async` / `Sync` or `Reply` |
| `+` / `-` after the arrow, `activate` / `deactivate` | `activate` / `deactivate` on the message |
| `create participant`, `destroy` | `created: true` and a `Create` message; a `Destroy` message |
| `Note left of / right of / over A,B` | `Note` |
| `loop`, `alt` / `else`, `opt`, `par` / `and`, `critical` / `option`, `break`, `rect` | `Fragment` (`Else` between operands; `rect` is `Group` with its colour) |
| `autonumber` | `autonumber: true` |
| `box`, `link`, autonumber's start and step | warnings |

### Class diagram

| mermaid | nymph |
|---|---|
| `class A`, `class A["Label"]`, `class A~T~`, `class A { ... }`, `A : member` | `Class` with `name`, `generic`, and members: with `(` a method, else an attribute (`List~int~` is written `List<int>`) |
| `<<interface>>`, `<<abstract>>`, `<<enumeration>>` | `kind` `Interface`, `Abstract`, `Enum` |
| `<\|--`, `<\|..`, `*--`, `o--`, `-->`, `..>`, `--`, `..` (either direction) | `ClassRelation` `Inheritance`, `Realization`, `Composition`, `Aggregation`, `DirectedAssociation`, `Dependency`, `Association`, `Dependency` |
| `A "1" --> "*" B : label` | `from_label`, `to_label`, `label` |
| `note for A "t"`, `note "t"` | `Note` |
| `namespace`, lollipop `()--`, other stereotypes, `click` | warnings |

### State diagram

| mermaid | nymph |
|---|---|
| `s1`, `state "Desc" as s1`, `s1 : Desc` | `State` with `name` |
| `[*]` | an `Initial` (as a source) or `Final` (as a target) state per composite, with ids such as `start`, `end`, `Moving_start` |
| `s1 --> s2 : label` | `Transition` |
| `state C { ... }` | a composite `State` (it takes states named in it, even if named above it) |
| `<<choice>>`, `<<fork>>`, `<<join>>` | `kind` `Choice`, `Fork`, `Join` |
| `note left of / right of s` | `Note` |
| `--` (concurrent regions) | an error: split them into composites |

### ER diagram

| mermaid | nymph |
|---|---|
| `CUSTOMER`, `p[Person]`, `"NAME WITH SPACES"` | `Entity` |
| `CUSTOMER { type name PK, FK "comment" }` | `attributes`, written as in mermaid |
| `A \|\|--o{ B : label`, or `A one or more to zero or many B : label` | `Relationship` with `from_cardinality` / `to_cardinality` (`ExactlyOne`, `ZeroOrOne`, `ZeroOrMore`, `OneOrMore`) and `label` |
| `..` (or `optionally to`) | `identifying: false` |

### Gantt chart

nymph resolves mermaid's schedule itself: every task gets its `start` (or
the `after` that mermaid wrote) and its `end` or `duration` in days.

| mermaid | nymph |
|---|---|
| `section S` | `GanttSection` |
| `Name : [tags,] [id,] [start,] end-or-duration` | `GanttTask`; a task without a start starts when the previous one ends |
| `after a b`, `until x` | `after: [a, b]`; `end` at x's start |
| `done`, `active`, `crit`, `milestone` | `status` `Done`, `Active`, `Critical`; `milestone: true` |
| `3d`, `1w`, `12h`, `30m` | `duration` in days |
| `dateFormat` (`YYYY`, `YY`, `MM`, `M`, `DD`, `D`, `HH`, `H`, `mm`, `m`, `ss`, `s`, `X`, `x`) | dates written as `YYYY-MM-DD` or `YYYY-MM-DD HH:MM` |
| `excludes`, `axisFormat`, `tickInterval`, `todayMarker`, `vert`, `click` | warnings (durations count every day; no `today` line is drawn) |

### Pie, Sankey and XY charts

| mermaid | nymph |
|---|---|
| `pie title t`, `"label" : value` | a `Plot` with a `PlotPieChart`; the slices are a `data` table (`showData` is a warning) |
| `sankey-beta` rows `source,target,value` | `SankeyDiagram` over a `data` table |
| `xychart-beta [horizontal]`, `x-axis [a, b]` or `x-axis "t" lo --> hi`, `y-axis "t" lo --> hi`, `bar [...]`, `line [...]` | a `Plot` with category ticks, `PlotBars` (`PlotBarsH` when horizontal; several bar series side by side) and `PlotLine` over a `data` table |

### Mind map

Indentation gives the tree; every node form (`id[t]`, `id(t)`, `id((t))`,
`id))t((`, `id)t(`, `id{{t}}`, plain text) becomes a `MindNode` with its
text. Icons (`::icon()`) and classes (`:::`) are warnings.
