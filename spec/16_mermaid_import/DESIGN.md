# nymph — Mermaid Import: Design

**Status: designed and planned; nothing built** (see [PLAN.md](PLAN.md)).

## 1. Purpose / Scope

AI agents are trained on a great deal of [mermaid](https://mermaid.js.org)
and write it fluently; nymph's YAML they learn from the prompt (the skill
text, `chart_schema`, `example_chart`). This project lets an agent start a
diagram in mermaid and finish it in nymph:

1. The agent writes the first version as mermaid text.
2. nymph reads it wherever it reads a chart today (the command line,
   `nymph mcp`, `nymph view`, the web viewer), converts it to nymph YAML
   and draws that.
3. The agent revises the mermaid until the user is happy.
4. The user (or the agent) **converts** it: the nymph YAML is saved, and
   from then on the chart is an ordinary nymph source, adjusted with
   nymph's own fields (colours, fonts, layout hints, sizes, the editor).

**Inputs** become PNG, nymph YAML or mermaid. **Outputs** stay PNG or
nymph YAML: nymph never writes mermaid, and a PNG always carries YAML, never
the mermaid it came from.

In scope:

- A mermaid reader and converter in the `nymph` library (`src/mermaid/`),
  used by the native executable and the WebAssembly module alike.
- The diagram kinds nymph already draws, mapped as closely as nymph allows
  (section 4): `flowchart` / `graph`, `sequenceDiagram`, `classDiagram`,
  `stateDiagram(-v2)`, `erDiagram`, `gantt`, `pie`, `mindmap`,
  `sankey-beta` and `xychart-beta`.
- Mermaid input on every entry point: `nymph <in>`, `render`, `check`, a
  new `convert` command, `nymph view` (and saving from its editor),
  `nymph mcp` (`render_chart`, `check_chart`, a new `convert_chart` tool,
  `editor_set_source`), the WebAssembly module and its JavaScript API
  (`setSource`, `<nymph-viewer src="x.mmd">`), and through `render_chart`
  the Claude App.
- Agent documentation: when to start in mermaid, how to convert, what
  carries over (`docs/mermaid.md`, the skill, `docs/agent-guide.md`).

Not in scope:

- Writing mermaid (nymph YAML or PNG to mermaid).
- Mermaid kinds nymph has no element for: `journey`, `timeline`,
  `quadrantChart`, `gitGraph`, `C4*`, `requirementDiagram`, `block-beta`,
  `architecture-beta`, `packet-beta`, `kanban`, `radar-beta`,
  `treemap-beta`, `zenuml` (section 10 says where each could go). They are
  an error that lists the kinds nymph reads.
- Mermaid's theming (`%%{init}%%` themes and `themeVariables`), CSS,
  `click` callbacks and links, icons, and animations: nymph's look is set
  by nymph's fields after conversion.
- Mermaid in the Chrome extension: GitHub already draws mermaid blocks,
  and the extension replaces nymph PNGs only. The Gemini chat mode
  ([10_gemini_extension](../10_gemini_extension/DESIGN.md)) may pick it up
  later (section 10).

Files owned or changed: new `src/mermaid/` (reader, one converter per
diagram kind, YAML writer), `src/nymph.hpp` (public API), `src/cli/`
(`cli.cpp`: `convert`, the `-o .yaml` rule, warnings; `cli_support`: one
`load_source()` for every front end), `src/mcp/tools.cpp` and
`editor_tools.cpp`, `src/window/view_command.cpp` and `viewer_window.cpp`
(mermaid input, saving), `web/nymph_web.cpp`, `web/js/nymph-viewer.js`,
`claude/skill/`, `docs/` (new `mermaid.md`; `cli.md`, `viewer.md`,
`claude.md`, `agent-guide.md`), `examples/mermaid/`, `tests/`.

## 2. Design Goals

1. **The YAML stays the only model.** Mermaid is converted once, at the
   boundary where a front end reads its input. Everything after that
   (binding, rendering, the PNG's text chunk, the editor's patches, the
   `view` key, `extract`) sees nymph YAML and does not know mermaid
   existed.
2. **Mermaid as agents write it.** Mermaid's own rules hold on the way in:
   nodes are declared by being named, `[*]` is a start or an end by where
   it stands, a gantt task starts after the previous one. The YAML that
   comes out declares everything explicitly, so nymph's rule "nothing is
   created by being named" (03 DESIGN, section 7) still holds for the
   source that is kept.
3. **Errors point at the mermaid.** Every problem is reported at a line
   and column of the text the agent wrote, including problems only nymph's
   own checks find (a cycle in a sankey, a transition into an initial
   state, a diagram too large for its image).
4. **Nothing is lost silently.** What nymph cannot draw is either an
   error (it changes the structure: concurrent states, an unsupported
   kind) or a warning naming the line (it changes only the look or the
   interaction: `click`, icons, `box` in a sequence). A front end shows
   the warnings; the conversion still succeeds.
5. **Readable, stable output.** The YAML looks like the examples a person
   would write (one element per line, flow mappings, ids kept as written,
   only non-default fields), and the same mermaid gives the same bytes on
   every platform and build. It is what the user adjusts afterwards and
   what the agent diffs.
6. **One converter for native and web.** Plain C++ in the `nymph` library,
   no I/O, no new dependency, no JavaScript: the WebAssembly module gets
   it for free and draws exactly what the native tool draws.
7. **Safe on untrusted text.** The web viewer converts text from any page:
   linear time, bounded nesting, the source size limit, no recursion that
   the input controls beyond the nesting limit.

## 3. Key Abstractions

```
             text (file, stdin, MCP argument, setSource, editor_set_source)
                │
         load_source()  ── is_mermaid()? ──no──► YAML as is ─────────────────┐
                │ yes                                                        │
         import_mermaid()                                                    │
           ├─ reader:    lines, %% comments, front matter, %%{init}%%,      │
           │             ```mermaid fence, diagram header → kind            │
           ├─ converter: one per kind → diagram model (nodes, edges, ...)   │
           │             implicit declarations made explicit, positions kept │
           ├─ writer:    nymph YAML text + line map (YAML line → mermaid pos)│
           ├─ sizing:    image width / height from the diagram's natural size│
           └─ check:     check_source(yaml); errors remapped to the mermaid  │
                │                                                            │
           mermaid_import {source, kind, warnings}                           │
                │                                                            ▼
                └──────────────► nymph YAML ──► render / check / view / editor / PNG
```

- **`is_mermaid(text)`**: true when the first line that is not blank, a
  `%%` comment, an `%%{...}%%` directive or inside a leading `---` front
  matter block starts with a mermaid diagram keyword (`flowchart`,
  `graph`, `sequenceDiagram`, ... and the unsupported ones too, so they
  get a clear error rather than a YAML parse error), or the whole text is
  one fenced block opened with ` ```mermaid `. A nymph source can never
  match: its first significant line is a `key:` mapping entry, and a lone
  `flowchart TD` is not a valid nymph document. The detection is by
  content, not by file extension, so standard input, the MCP `source`
  argument and `setSource()` work the same as files; `.mmd` / `.mermaid`
  are only the conventional names.
- **Reader** (`src/mermaid/reader.*`): splits the text into statements
  with their line and column (mermaid's `;` separators included), strips
  comments, reads the front matter's `title` (and `config` only to warn
  that it is ignored), and `accTitle` / `accDescr` lines. It knows the
  shared lexical rules: quoted labels (`"..."`), markdown strings
  (`` "`**bold**`" ``: the markers are dropped, the text kept), `<br>` as a
  line break, `#quot;` / `#35;` entity codes.
- **Converters** (`src/mermaid/<kind>.cpp`): one per diagram kind, each a
  small hand-written recursive-descent parser over the reader's statements
  that fills a diagram model: plain structs of nodes, edges, groups and
  notes holding nymph's field values and the mermaid position each came
  from. A converter applies mermaid's semantics (section 4) and records
  warnings. It never builds YAML text itself.
- **YAML writer** (`src/mermaid/yaml_writer.*`): turns a model into the
  source text, in the style of `examples/`: `nymph: 1`, `description`,
  `image`, `source`, `root` with block-style containers and one flow
  mapping per leaf element, `data` as CSV for pie, sankey and xychart.
  Scalars go through `encode_text()` (`src/edit/field_value.hpp`, already
  in the `nymph` library). It keeps a **line map**: for each YAML line,
  the mermaid line and column of the statement that produced it.
- **Sizing**: the converter binds the YAML once and asks the layout engine
  for the root's natural size (as a diagram inside a layout would take
  it), then writes `image: {width, height}` with that size plus padding,
  clamped to 320 to 4096 pixels (wider for a `LeftRight` flowchart, as it
  happens). A big mermaid diagram so renders at its natural scale instead
  of failing the `min_zoom` floor of a default 800 x 500 image, and the
  size is written down for the user to change.
- **`mermaid_import`**: the result: the nymph YAML, the mermaid kind
  (`"flowchart"`, ...), and the warnings, each with its mermaid line and
  column. Before returning, `import_mermaid()` runs `check_source()` on the
  YAML; a `nymph::error` from it is re-thrown at the mermaid position the
  line map gives for the error's YAML line, its message ending with
  `(nymph YAML line L)`. So a successful import is always a valid source,
  and no caller has to map positions.
- **`load_source()`** (`src/cli/cli_support.*`, replacing `source_of()`):
  bytes in, nymph YAML out for every front end: a PNG gives the source it
  carries, mermaid is imported, anything else is returned as is. It
  returns the warnings and the kind alongside, for the front end to show.
  The WebAssembly module, which does not link `nymph_cli`, calls
  `is_mermaid()` / `import_mermaid()` directly (section 5.5).

## 4. Mapping

Section by kind. "Warning" means the statement is dropped (or changed)
and a warning names its line; "error" fails the import at that line.
Approximations listed here as such are silent: they keep the meaning and
change only the drawing. `direction` / `TB TD BT LR RL` map to the
diagram's `direction` everywhere a nymph diagram has one. `title` (front
matter or the kind's own `title` statement) maps to the diagram's `title`;
`accTitle` and `accDescr` to the source's `description` (otherwise
`description` says "<Kind> converted from mermaid." with the title when
there is one). `source` says `Converted from a mermaid <kind>.` Styling
(`classDef`, `class`, `:::name`, `style`, `linkStyle`) maps the CSS
properties `fill`, `stroke` and `color` to `fill_color`, `color` /
`line_color` and `text_color` where the element has them; other properties
(`stroke-width`, `stroke-dasharray`, fonts) are warnings.

### 4.1 `flowchart` / `graph` → `Flowchart`

| mermaid | nymph |
|---|---|
| `A`, `A[t]` | `FlowNode {id: A, text: t}` (`Box`); a node first named in an edge is declared there, in order of first mention |
| `A(t)`, `A([t])`, `A[[t]]`, `A[(t)]`, `A((t))`, `A{t}`, `A{{t}}`, `A[/t/]` | `Rounded`, `Stadium`, `Subroutine`, `Cylinder`, `Circle`, `Diamond`, `Hexagon`, `Parallelogram` |
| `A[\t\]`, `A(((t)))`, `A>t]`, `A[/t\]`, `A[\t/]` | approximations: `Parallelogram`; `Circle`; `symbol: banner`; `symbol: trapezoid` (both) |
| `A@{shape: name, label: "t"}` (mermaid 11) | the `shape` above with the same meaning, else the nearest library `symbol` (`doc` → `document`, `docs` → `multi_document`, `delay`, `cloud`, `manual-input`, `database`, ...: the full table lives in `src/mermaid/flowchart.cpp` and `docs/mermaid.md`); an unknown name is an error with a suggestion |
| `-->`, `---`, `-.->`, `-.-`, `==>`, `===`, `<-->` | `FlowLink` `line` `Solid` / `Dotted` / `Thick`, `arrow` `End` / `None` / `Both` |
| `--o`, `--x` (circle, cross ends) | approximation: `arrow: End` |
| `~~~` (invisible link) | warning; dropped |
| `-->\|t\|`, `-- t -->`, `-. t .->`, `== t ==>` | `label: t` |
| extra dashes / dots / equals (`---->`) | `min_length` (mermaid's rank length) |
| `A --> B --> C`, `A & B --> C & D` | one `FlowLink` per pair, in mermaid's order |
| `subgraph id [title] ... end`, `subgraph title ... end` | `FlowGroup {id, title}` with its nodes as children; a node belongs to the innermost subgraph that names it, as in mermaid; links to a subgraph id end at the group |
| `direction` inside a subgraph | warning (one direction per diagram in nymph) |
| `click`, `e1@-->` link ids, `animate` | warning; dropped (the link itself is kept) |

Links are written after all nodes, at the diagram's top level, so a link
may join nodes of different groups. A `Note` has no mermaid flowchart
equivalent and is never produced.

### 4.2 `sequenceDiagram` → `SequenceDiagram`

| mermaid | nymph |
|---|---|
| `participant A`, `participant A as Name`, `actor A` | `Participant {id, name, kind: Participant / Actor}`; participants first named in a message are declared before the first message, in order of mention |
| `participant A@{"type": "database"}` (boundary, control, entity, database) | `kind` of the same name; `collections`, `queue`: approximation, `Participant` |
| `->>`, `-->>`, `-)`, `--)`, `->`, `-->` | `Message kind` `Sync`, `Reply`, `Async`, `Async` (dashed: approximation), `Sync`, `Reply` (no-head lines: approximation) |
| `-x`, `--x`, `<<->>` | approximations: `Sync` / `Reply`, `Sync` |
| `+` / `-` on the target | `activate` / `deactivate` |
| `activate A` / `deactivate A` | `activate: true` on the message just before that reached A / `deactivate: true` on the latest message from A; no such message is an error ("write it as ->>+") |
| `create participant A` + the next message to A | `Participant {created: true}` and that message `kind: Create` |
| `destroy A` + the next message to A | that message `kind: Destroy` |
| `Note left of / right of A: t`, `Note over A,B: t` | `Note {attach: [A], side}`, `Note {over: [A, B]}` |
| `loop`, `alt`/`else`, `opt`, `par`/`and`, `critical`/`option`, `break` | `Fragment kind` `Loop`, `Alt` + `Else`, `Opt`, `Par` + `Else`, `Critical` + `Else`, `Break` |
| `rect rgb(...)` | `Fragment kind: Group` with `fill_color` |
| `autonumber` | `autonumber: true` (a start or step value is a warning) |
| `box Name ... end` | warning; the participants are kept, the box dropped |
| `link`, `links`, `properties`, `details` | warning; dropped |

### 4.3 `classDiagram` → `ClassDiagram`

| mermaid | nymph |
|---|---|
| `class A`, `class A["Label"]`, `class A~T~` | `Class {id, name, generic}`; classes named only in relations are declared in order of mention |
| `class A { ... }`, `A : member` | members with `(` go to `methods`, others to `attributes`, written as is (mermaid's `$` / `*` suffixes are nymph's already) |
| `<<interface>>`, `<<abstract>>`, `<<enumeration>>` / `<<enum>>` (in the block or `<<x>> A`) | `kind` `Interface`, `Abstract`, `Enum`; another stereotype is a warning |
| `A <\|-- B`, `A <\|.. B` | `ClassRelation {from: B, to: A, kind: Inheritance / Realization}` (nymph's `from` is the child) |
| `A *-- B`, `A o-- B` | `{from: A, to: B, kind: Composition / Aggregation}` (the diamond at `from`) |
| `A --> B`, `A ..> B`, `A -- B`, `A .. B` | `DirectedAssociation`, `Dependency`, `Association`, `Dependency` (approximation: dashed with a head) |
| reversed forms (`B --\|> A`, `B --* A`, `A <-- B`, ...) | the same relation, ends swapped as needed |
| two-way heads (`<\|--\|>`, `*--*`) | approximation: the `to` end's kind only |
| `A "1" --> "*" B : label` | `from_label`, `to_label`, `label` |
| `note for A "t"`, `note "t"` | `Note {attach: [A]}`, a free `Note` |
| `namespace N { ... }` | warning; the classes are kept, the grouping dropped |
| lollipop `()--`, `click`, `link`, `callback`, `cssClass` | warning; dropped (`cssClass` with a `classDef`: styling, above) |

### 4.4 `stateDiagram` / `stateDiagram-v2` → `StateDiagram`

| mermaid | nymph |
|---|---|
| `s1`, `state "Desc" as s1`, `s1 : Desc` | `State {id: s1, name}`; several `s1 : d` lines join with line breaks; states named only in transitions are declared in order of mention |
| `[*] --> s1`, `s1 --> [*]` | `State {kind: Initial}` / `{kind: Final}`, one of each per scope (ids `start`, `end`, or `<composite>_start` / `<composite>_end`), declared first / last in that scope |
| `s1 --> s2 : label` | `Transition {from, to, label}` |
| `state C { ... }` | composite `State` with the inner states and transitions as children |
| `state c <<choice>>`, `<<fork>>`, `<<join>>` | `kind` `Choice`, `Fork`, `Join` |
| `--` (concurrent regions) | error: nymph has no concurrent regions; split into separate composites |
| `note left of / right of s: t`, `note ... end note` | `Note {attach: [s]}` (side is approximated by the layout) |

### 4.5 `erDiagram` → `ERDiagram`

| mermaid | nymph |
|---|---|
| `CUSTOMER`, `p[Person]` | `Entity {id, name}`; entities named only in relationships are declared in order of mention |
| `CUSTOMER { type name PK, FK "comment" }` | `attributes: ["type name PK, FK \"comment\""]` (nymph takes mermaid's attribute form already) |
| `A \|\|--o{ B : label` | `Relationship {from: A, to: B, from_cardinality: ExactlyOne, to_cardinality: ZeroOrMore, label}` |
| `\|o` / `o\|`, `\|\|`, `}o` / `o{`, `}\|` / `\|{`, and the word forms (`zero or one`, `only one`, `one or more`, `many(0)`, ...) | `ZeroOrOne`, `ExactlyOne`, `ZeroOrMore`, `OneOrMore` |
| `--` / `..` | `identifying: true` / `false` |

### 4.6 `gantt` → `GanttChart`

The converter resolves mermaid's scheduling itself, so every task gets an
explicit `start` and `end` (or `duration`), and `after` is written only
where the mermaid wrote `after` (nymph draws an arrow for each).

| mermaid | nymph |
|---|---|
| `section S` | `GanttSection {name: S}` with the tasks that follow |
| `Name : [tags,] [id,] [start,] end-or-duration` | `GanttTask {id, name, start or after, end or duration}`; ids are generated (`t1`, ...) only where an `after` needs one |
| no start | starts when the previous task ends: written as its computed `start` |
| `after a b` | `after: [a, b]` |
| `done`, `active`, `crit` (several: `crit` wins, then `active`) | `status` `Done`, `Active`, `Critical` |
| `milestone` | `milestone: true` |
| durations `3d`, `1w`, `12h`, `30m` | `duration` in days (fractions for hours and minutes) |
| `dateFormat` | dates are read with it (tokens `YYYY`, `YY`, `MM`, `DD`, `HH`, `mm`, `X`; other tokens are an error) and written ISO |
| `until x` | `end` set to x's computed start |
| `excludes`, `weekday`, `axisFormat`, `tickInterval`, `todayMarker`, `vert`, `click` | warning; dropped (durations count every day) |

No `today` is written: mermaid's marker is the day the chart is drawn,
which would make the output depend on the clock (goal 5).

### 4.7 `pie`, `sankey-beta`, `xychart-beta` → plots

| mermaid | nymph |
|---|---|
| `pie [showData] [title t]`, `"label" : value` | `Plot` with the pie example's settings (`examples/pie.yaml`) and one `PlotPieChart {labels: $label, values: $value}`; the slices become a `data` table `label, value`; `showData` is a warning |
| `sankey-beta` CSV rows `source,target,value` | `SankeyDiagram {from: $source, to: $target, values: $value}` with the rows as the `data` table (CSV quoting kept) |
| `xychart-beta [horizontal]`, `title`, `x-axis [a, b, ...]` / `x-axis "label" lo --> hi`, `y-axis "label" [lo --> hi]`, `bar [...]`, `line [...]` | `Plot {title, x_label, y_label, x_ticks + x_tick_labels for categories, y_min / y_max}`, one `PlotBars` (`PlotBarsH` when horizontal) or `PlotLine` per series over a `data` table, labelled `bar1`, `line1`, ... |

### 4.8 `mindmap` → `MindMap`

Indentation gives the tree; the first node is the root `MindNode`.
`id[text]`, `id(text)`, `id((text))`, `id))text((`, `id)text(`,
`id{{text}}` and plain text all become `MindNode {text}` (approximation:
nymph's mind map has one node look); an `id` is written only when it is
not the text. `::icon(...)` and `:::class` are warnings.

## 5. Front Ends

### 5.1 Command line

| Command | With a mermaid input |
|---|---|
| `nymph diagram.mmd` | writes `diagram.png`, carrying the converted YAML |
| `nymph diagram.mmd -o diagram.yaml` | writes the converted YAML: on the implicit command, an `-o` ending in `.yaml` / `.yml` converts instead of rendering |
| `nymph render diagram.mmd [-o x.png]`, `render - -o -` | renders, as for YAML |
| `nymph check diagram.mmd` | converts and validates; silent when valid |
| `nymph convert <in> [-o <out.yaml>]` | **new**: the nymph YAML of any input (mermaid converted, a PNG's source, YAML checked and passed through) to standard output or `-o`; `-o` must end in `.yaml` / `.yml` |
| `nymph view diagram.mmd [--edit]` | below (5.3) |

Warnings go to standard error, one line each, and do not change the exit
code: `nymph: diagram.mmd:7:3: warning: click is ignored (nymph has no
links in a figure)`. Errors read as today, at the mermaid position. `-o`
never writes mermaid; `render -o x.mmd` writes a PNG named `x.mmd`, as it
writes a PNG under any other name today.

### 5.2 `nymph mcp`

- `render_chart` and `check_chart` take mermaid in `source` or in the file
  named by `input_path`, detected by content. `render_chart`'s
  `structuredContent.source` is the converted YAML (so the Claude App, which
  draws that field, shows a mermaid chart unchanged), and its text adds one
  line: `Converted from mermaid (flowchart); convert_chart gives the nymph
  YAML.` plus the warnings.
- **New `convert_chart`**: `source` or `input_path` (mermaid, YAML or a nymph
  PNG), optional `output_path` (must end in `.yaml` / `.yml`, goal 5 of
  06: the server writes only charts and their sources). Returns the nymph
  YAML as text and the warnings; read-only unless `output_path` is given.
  It is the step an agent takes when the user is satisfied.
- `editor_set_source` (14) accepts mermaid: the open editor shows the
  converted chart as one undoable change, so an agent can iterate on
  mermaid while the user watches `nymph view --edit`.
- Tool descriptions and the skill say when mermaid is the better start (a
  diagram whose structure is still changing) and when to write YAML
  directly (plots, layouts, user interfaces, anything already converted).

### 5.3 `nymph view`

`nymph view diagram.mmd` converts before the window opens (errors and
warnings on standard error, as `check`). The window shows the converted
chart. Saving never writes mermaid:

- `Y` writes `diagram.view.yaml` and `S` `diagram.view.png`, as for any
  input.
- The editor's save writes `diagram.yaml` next to the input (asking before
  overwriting an existing file, through the editor's save dialog), and
  from then on the window's input is `diagram.yaml`: the title, later
  saves and the unsaved-changes check follow it. The `.mmd` file is never
  touched.

### 5.4 Library API

`is_mermaid()` and `import_mermaid()` (section 7) are public, so a program
linking `nymph::nymph` can read mermaid too. `render_png()` and
`check_source()` keep taking nymph YAML only: the PNG they write embeds
their input verbatim, and embedding mermaid would break goal 1.

### 5.5 WebAssembly and JavaScript

- `nymph_viewer_set_source` converts when `is_mermaid()` says so; the
  viewer then holds the YAML, so `viewer.source()`, `toYaml()`, `toPng()`
  and the right-click menu's "Save YAML" / "Save PNG" all give nymph
  output. A conversion error is the usual `NymphError` with the mermaid
  line and column.
- New export `nymph_import_mermaid(text, size)` returns JSON
  `{source, kind, warnings: [{line, column, message}]}`, wrapped as
  `importMermaid(text)` on the loaded module (for tools that only want to
  convert).
- `NymphViewer.info()` gains `importedFrom: "mermaid"` (absent otherwise),
  and the `nymph-load` event carries it with the warnings.
- `<nymph-viewer src="diagram.mmd">` and mermaid as the element's text
  content work with no new attribute (detection by content).
- The Claude App needs no change: it draws `render_chart`'s YAML.

## 6. Data Flow

**Agent iterating in the chat:** agent writes mermaid → `render_chart
{source: <mermaid>}` → `load_source()` imports → renderer → PNG with the
YAML and a text line naming the kind and warnings; the App shows the live
chart from `structuredContent.source`. The agent edits the mermaid and
renders again. When the user is satisfied: `convert_chart {source,
output_path: chart.yaml}` → the YAML; later edits are nymph edits.

**User with the window:** `nymph view diagram.mmd --edit` → converted YAML
in the editor; the agent sends new mermaid through `editor_set_source`, or
the user edits fields directly (which already makes it a nymph chart);
save → `diagram.yaml`.

**Error:** mermaid line 12 names an unknown `@{shape: blob}` → the
flowchart converter throws at 12:9 with a suggestion. A sankey cycle →
`check_source()` fails at YAML line 4 (`from:`) → the line map gives
mermaid line 2 (the first CSV row) → reported at `diagram.mmd:2:1: the
flows form a cycle ... (nymph YAML line 4)`.

## 7. Public API Contract

```cpp
namespace bdg::nymph {

/// One thing a mermaid import dropped or changed, at its mermaid position.
struct import_warning {
  int line = 0;   ///< 1-based
  int column = 0; ///< 1-based, in characters
  std::string message;
};

/// A mermaid diagram converted to a nymph source.
struct mermaid_import {
  std::string source;                  ///< nymph YAML; check_source() accepts it
  std::string kind;                    ///< mermaid's keyword: "flowchart", "sequenceDiagram", ...
  std::vector<import_warning> warnings; ///< in mermaid line order
};

/// True when `text` is a mermaid diagram (section 3); never true for a
/// nymph source. Cheap: reads the first significant line.
bool is_mermaid(std::string_view text) noexcept;

/// Converts a mermaid diagram to an equivalent nymph source.
/// @throws nymph::error positioned in `text` for a syntax error, an
///         unsupported diagram kind or construct, or a problem nymph's
///         checks find in the result (the message ends with the YAML line).
mermaid_import import_mermaid(std::string_view text);

} // namespace bdg::nymph
```

Command line, MCP and JavaScript additions as in section 5. Output is
deterministic: the same text and nymph build give the same `source` bytes.

## 8. Design Decisions

- **Convert at the boundary; never keep mermaid.** The alternative, a
  second source format the PNG could carry, would give every later stage
  (the editor's text patches, `extract`, the `view` key, the schema-driven
  inspector, the Claude App) two formats to handle and would let a PNG
  carry text nymph's YAML tools cannot edit. The user's rule "outputs are
  PNG or YAML" follows directly.
- **A hand-written C++ reader, not mermaid.js.** The native tool has no
  JavaScript engine, and in the browser mermaid's parsers are tied to its
  DOM renderer and are megabytes for a feature that needs a fraction.
  Mermaid's grammars (jison) are not a stable interface either. A small
  recursive-descent reader per kind, tested against mermaid's documented
  examples, covers what agents write and runs identically everywhere.
- **Lenient in, explicit out.** Mermaid's implicit declarations are its
  semantics, and an agent writing mermaid expects them; rejecting them
  would defeat the purpose. The converter makes them explicit, so the YAML
  that is kept obeys nymph's strict rules and a later typo in it is still
  an error (03's decision stands for nymph sources).
- **Detection by content, not by extension.** Standard input, MCP
  `source`, `setSource()` and `editor_set_source` have no file name. The
  first significant line decides, and the two formats cannot be confused.
  A fenced ` ```mermaid ` block is accepted because agents often paste one.
- **Warnings for look, errors for structure.** A dropped `click` or icon
  leaves a correct diagram, so failing would only cost a round trip;
  a dropped concurrent region or an unknown kind would draw something
  else than what was meant, so it fails.
- **The import validates its own output.** Without it, an error found by
  binding would point into a YAML text the agent never saw. Running
  `check_source()` once inside `import_mermaid()` and remapping through the
  line map keeps goal 3 in one place; the cost is one extra bind and
  layout, small next to a render.
- **The image fits the diagram.** Mermaid has no image size; nymph's
  default 800 x 500 with `min_zoom` 0.5 would reject many converted
  diagrams. Measuring the natural size and writing it into `image` makes
  conversion succeed and leaves an explicit number to adjust.
- **Gantt scheduling resolved in the converter.** Writing mermaid's
  implicit "after the previous task" as nymph's `after` would add
  dependency arrows mermaid does not draw; computing explicit dates keeps
  the picture and the meaning.
- **`convert` as its own command and tool.** It is the step the workflow
  names, an agent looks for it by name, and it also works on YAML (a
  check) and PNGs (an extract), so one call gives "the nymph source of
  this" whatever the input.

## 9. Constraints and Invariants

- A successful `import_mermaid()` returns a source `check_source()`
  accepts, with `nymph: 1`, `description`, `image` and `source` keys.
- No PNG written by nymph carries mermaid; no file nymph writes is mermaid.
- Output is a pure function of the input text and the build: no clock, no
  locale, no hashing order. Ids are kept as written (quoted when YAML needs
  it); generated ids (`start`, `t1`, ...) never collide with written ones
  (a numeric suffix is added).
- Limits (`src/core/limits.hpp`): the input obeys the source size limit;
  subgraphs, composite states and fragments nest at most 8 deep
  (`kMaxUmlNesting`), mind map levels at most 32 (`kMaxTreeDepth`), both
  checked before recursing; diagram limits (500 nodes, 2000 edges, 500
  gantt tasks, ...) are nymph's own, reported at the mermaid line of the
  first element over the limit.
- The reader is linear in the input; positions count characters (UTF-8),
  as nymph's errors do.
- Plain C++20 with no OS calls and no new dependency; builds on Linux,
  macOS, MSVC, MSYS2 and Emscripten.

## 10. Integration Boundaries and Extension Points

- Depends on: the element schema and field values of
  [01_core_features](../01_core_features/DESIGN.md) and
  [03_uml_diagrams](../03_uml_diagrams/DESIGN.md) /
  [13_more_diagrams_and_shapes](../13_more_diagrams_and_shapes/DESIGN.md)
  (which fields exist; the shape library for `@{shape}`), `check_source()`
  and the layout engine's measure for sizing, `encode_text()`.
- Changes the front ends of [04_web_viewer](../04_web_viewer/DESIGN.md),
  [06_claude_integration](../06_claude_integration/DESIGN.md) (tools, skill)
  and [14_editor_workspace](../14_editor_workspace/DESIGN.md)
  (`editor_set_source`, saving), without changing their contracts for YAML.
- **A new mermaid kind** is one converter file and a row in section 4:
  `architecture-beta` → `NetworkDiagram` / `DeploymentDiagram`;
  `C4Context` / `C4Container` → `ComponentDiagram`; `timeline` and
  `journey` → a `GanttChart` or a layout of labels; `block-beta` → a
  `Flowchart` with ranks; `requirementDiagram` → `ClassDiagram` with
  stereotypes.
- **Concurrent state regions**, should 03 add them (its section 10), turn
  that error into a mapping.
- **Mermaid in chat pages**: the Gemini chat mode (10) could detect
  ` ```mermaid ` blocks too and show them as nymph charts with the same
  `importMermaid()`.
