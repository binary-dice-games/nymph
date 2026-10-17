# nymph — UML Diagrams: Implementation Plan

See [DESIGN.md](DESIGN.md) for the design this plan implements, and
[spec/01_core_features/DESIGN.md](../01_core_features/DESIGN.md) section 4
(element schema, layout engine, `draw_context`, `render_frames()`) for the
code it plugs into.

Steps are ordered so the library's foundations come first (schema type,
ImUml context, shapes, the layout engine, each testable alone), then one
diagram kind per step, simplest graph diagram first, then the sequence
diagram, then composite states, then documentation. Every step leaves all
existing tests passing unmodified and the committed example PNGs
byte-identical. Every test is a GoogleTest file in `tests/`, added with
`nymph_add_test()`; ImUml's own tests use only ImGui and ImUml (plus
nymph's `render_frames()` where pixels are checked).

Pixel tests use the software rasterizer and probe a few pixels or count
pixels of a colour in a region; they never compare whole images against
committed PNGs, except the examples.

## Steps

### 1. `text_array` field type

**Goal:** element fields can hold a list of strings, including from a
`$column`.

**Deliverables:**
- `src/schema/value.hpp`: `std::vector<std::string>` alternative.
- `src/schema/field_desc.hpp`: `field_type::text_array`,
  `texts_field()` builder.
- `src/schema/convert.cpp`: a sequence of scalars, each converted with
  the `text` rules; a non-sequence or a nested item is an error at it.
- `src/document/bind.cpp`: `$col` into `text_array` gives the cells.
- `src/cli/schema_doc.cpp`: type name `text list`, default printed as a
  YAML flow list.

**Tests** (`tests/test_schema.cpp`, `tests/test_bind.cpp`):
- `["a", b, 2025, "$$x"]` gives `{"a", "b", "2025", "$x"}`; `[]` is empty.
- A scalar, a mapping or a nested list is an error positioned at it,
  naming `Type.field`.
- `$col` gives the column's cells in row order; `[$a, $b]` is an error.
- `nymph schema --markdown` prints a test class's `text_array` field.

### 2. ImUml library skeleton and context

**Goal:** an `imuml` CMake target with a context, a style that follows the
ImGui theme, and empty diagram scopes that reserve space and fit.

**Deliverables:**
- `imuml/CMakeLists.txt`, `imuml/imuml.h`, `imuml/imuml_internal.h`,
  `imuml/imuml.cpp`: `CreateContext`, `DestroyContext`,
  `Get/SetCurrentContext`, `ImUmlStyle`, `ImUmlCol_`, `IMUML_AUTO_COL`
  resolution, `StyleColors*`, `Push/PopStyleColor/Var`,
  `SetNextItemColors`, diagram pool, `Begin/EndClassDiagram` (others
  follow in their steps) with size reservation, zoom computation,
  `MeasureOnly`, title, `GetLastDiagramInfo()`.
- Root `CMakeLists.txt` / `cmake/NymphDependencies.cmake`: build `imuml`
  against the ImGui target; `nymph` links it.
- `src/render/figure_renderer.cpp`: create, theme and restore the ImUml
  context with the ImPlot ones.
- `CLAUDE.md`: a line saying `imuml/` follows ImGui naming.

**Tests** (`tests/test_imuml_context.cpp`):
- Create / destroy / set current; `render_frames()` restores the
  previously current ImUml context, also when the draw function throws.
- Auto colours resolve from the ImGui style: `Text` equals
  `ImGuiCol_Text` under light and dark.
- An empty diagram of size `(-1, 200)` reserves the available width and
  200 px; `MeasureOnly` reserves nothing and reports the title's size.
- The zoom is `min(available / natural)`, capped at `MaxZoom`.

### 3. Rendering primitives

**Goal:** every shape, line style and arrowhead of DESIGN section 4 draws
correctly at any zoom.

**Deliverables:**
- `imuml/imuml_render.cpp`, declarations in `imuml_internal.h`: the
  `Render*` functions, dashed paths by arc length, explicit segment
  counts, text at `font_size * zoom`.

**Tests** (`tests/test_imuml_render.cpp`, through `render_frames()`):
- Each primitive draws inside its rectangle and nowhere else.
- Hollow markers (triangle, diamond) leave their centre at the background
  colour; filled ones do not.
- A dashed horizontal line has alternating runs of `DashLength` and
  `GapLength` (± 1 px); a solid one has none.
- Drawing at zoom 0.5 covers half the extent of zoom 1 (± 1 px).
- Rendering the same scene twice gives the same bytes.

### 4. Graph layout engine

**Goal:** the layered layout of DESIGN section 4 as a pure function.

**Deliverables:**
- `imuml/imuml_graph_layout.cpp`, `ImUml::LayoutGraph(const
  ImUmlGraphInput&, ImUmlGraphOutput*)` in `imuml_internal.h`: components,
  cycle reversal, ranking with constraints, dummies, ordering, coordinates,
  edge routing and clipping, splines, directions, rounding.

**Tests** (`tests/test_imuml_graph_layout.cpp`, no ImGui context):
- No two node rectangles overlap (plus spacing), for a set of fixed
  graphs: chain, tree, diamond, K(3,3), a cycle, a self-loop, parallel
  edges, two components.
- Every non-reversed edge goes from a lower rank to a higher one; a cycle
  is laid out with exactly one reversed edge.
- A graph drawable without crossings (a tree, a ladder) is laid out with
  0 crossings.
- Rank constraints hold (pinned first and last ranks).
- Edge ends lie on their node's shape boundary (± 1 px) for rectangle,
  ellipse, diamond and circle.
- `LeftRight` equals `TopDown` transposed.
- The same input twice gives identical output; coordinates are whole
  numbers.

### 5. Class diagram

**Goal:** `ClassDiagram`, `Class`, `ClassRelation` and graph `Note` render
from YAML.

**Deliverables:**
- ImUml: `BeginClass`, `ClassAttribute`, `ClassMethod`, `EndClass`,
  `Relation`, `Note`; `EndClassDiagram()` lays out, caches and draws;
  member suffixes `$` / `*`; `ItalicFont`.
- `src/elements/uml/uml.hpp`, `uml_common.hpp/.cpp` (shared fields, id
  table, `Note`, fit check in `arrange()`, `diagram_scope`, italic sibling
  lookup), `class_diagram.cpp`; `register_uml()` called from
  `register_builtin.cpp`; limits in `src/core/limits.hpp`.
- `examples/uml_class.yaml` and its PNG; `docs/elements.md` regenerated.

**Tests** (`tests/test_uml_class.cpp`):
- Schema is pinned (names, slots, fields, defaults), as for plot2d.
- The DESIGN.md example renders; PNG round trip; same bytes twice.
- Errors, each with position: duplicate id; unknown `from` with a
  suggestion; a `Class` outside a `ClassDiagram` says where it goes; too
  many nodes.
- `Inheritance` places `to` above `from` (rectangles from the last layout).
- A diagram too big for its rectangle at `min_zoom` fails `check_source()`
  with the needed and available sizes; with a lower `min_zoom` it renders.
- A diagram with 0 hints in a `VerticalLayout` gets its natural height.
- `fill_color` on a class changes pixels inside its box only.

### 6. State diagram (flat)

**Goal:** `StateDiagram`, `State` (no children yet), `Transition` render.

**Deliverables:** ImUml `Begin/EndStateDiagram`, `BeginState`,
`StateActivity`, `EndState`, `Transition`; pseudo-state shapes wired to
the graph layout; `src/elements/uml/state_diagram.cpp`;
`examples/uml_state.yaml`.

**Tests** (`tests/test_uml_state.cpp`):
- Example renders, round trip, determinism.
- Transition into `Initial` / out of `Final` is an error at `to` /
  `from`; children on a pseudo-state are rejected (until step 11, any
  child state is rejected with "composite states are not supported yet").
- An `Initial` state is drawn as a filled circle (centre pixel is the
  line colour), `Final` as a bullseye.

### 7. Activity diagram

**Goal:** `ActivityDiagram`, `Action`, `ActivityControl`, `Flow` render.

**Deliverables:** ImUml `Begin/EndActivityDiagram`, `Action`,
`ControlNode`, `Flow`; `src/elements/uml/activity_diagram.cpp`;
`examples/uml_activity.yaml`.

**Tests** (`tests/test_uml_activity.cpp`):
- Example renders, round trip, determinism.
- Flow rules of DESIGN section 3 are errors with positions.
- `Fork` / `Join` bars are perpendicular to the flow in `TopDown` and
  `LeftRight`; guards are drawn (text pixels near the edge midpoint).

### 8. Use case diagram

**Goal:** `UseCaseDiagram`, `Actor`, `UseCase`, `UseCaseRelation` render,
with the system boundary.

**Deliverables:** ImUml `Begin/EndUseCaseDiagram`,
`SetupSystemBoundary`, `Actor`, `UseCase`, `UseCaseLink`; actor rank
constraints; boundary around the use cases;
`src/elements/uml/usecase_diagram.cpp`; `examples/uml_usecase.yaml`.

**Tests** (`tests/test_uml_usecase.cpp`):
- Example renders, round trip, determinism.
- Left actors are left of the boundary, right actors right of it; every
  use case is inside it; with an empty `system` no boundary is drawn.
- Relation kind rules are errors with positions.

### 9. Sequence diagram

**Goal:** `SequenceDiagram`, `Participant`, `Message` and sequence notes
render, with activations, autonumber, create and destroy.

**Deliverables:**
- `imuml/imuml_sequence_layout.cpp` (`LayoutSequence()`), ImUml
  `Begin/EndSequenceDiagram`, `SetupSequence`, `Participant`, `Message`,
  `NoteOver`, `NoteSide`.
- `src/elements/uml/sequence_diagram.cpp` (without `Fragment` / `Else`);
  `examples/uml_sequence.yaml` (fragments added in step 10).

**Tests** (`tests/test_imuml_sequence_layout.cpp`,
`tests/test_uml_sequence.cpp`):
- Layout: columns in declaration order; each gap fits the widest label
  over it; rows strictly increase in submission order; nested
  activations are offset; a created participant's box is at its row; a
  destroyed lifeline ends at its row.
- Element: example renders, round trip, determinism; `deactivate` with no
  activation, messages after destroy, a misplaced `Create`, a participant
  declared after a message, `over` on a graph note are errors with
  positions.

### 10. Sequence fragments

**Goal:** `Fragment` and `Else` render, nested.

**Deliverables:** ImUml `BeginFragment`, `FragmentElse`, `EndFragment`
and their rows; `Fragment` / `Else` elements; the sequence example gains
an `Alt` and a `Loop`.

**Tests** (in the step 9 files):
- A fragment's rectangle contains every message inside it; a nested one
  lies inside its parent with the nesting margin; `over` overrides the
  span.
- `Else` outside `Alt` / `Par` / `Critical`, or first or last, is an
  error.

### 11. Composite states

**Goal:** a `State` with children is drawn as a composite state.

**Deliverables:** nested graph layout (inside-out), border-crossing
transitions, nesting limit; `State` accepts `uml.state.item`; the state
example gains a composite state.

**Tests** (in `tests/test_uml_state.cpp`, `test_imuml_graph_layout.cpp`):
- Child states lie inside the composite's rectangle below its header.
- A transition from an outer state to an inner one ends on the inner
  state's border.
- Ids are unique across nesting levels; nesting deeper than the limit is
  an error.

### 12. Documentation and release

**Goal:** agents can find, write and fix UML diagrams from the docs alone.

**Deliverables:**
- `docs/format.md`: a UML diagrams section (ids, fit, errors);
  `docs/agent-guide.md`: one recipe per diagram; `docs/examples.md`: the
  five examples; `docs/elements.md` regenerated.
- `README.md`: Core Concepts row for diagrams, the overview sentence.
- `CHANGELOG.md`: `### Added` entries.
- This plan's step results, DESIGN.md status, `spec/INDEX.md` status.

**Tests:**
- `test_docs` (schema doc in step) passes; every example renders to its
  committed PNG byte for byte with the software rasterizer.

## Completion Criteria

- [ ] All tests pass under `ctest -j` three times in a row.
- [ ] The five diagram examples render, round-trip, and are byte-identical
      across two runs with `--rasterizer software`.
- [ ] Every error rule of DESIGN.md section 3 has a test with its position.
- [ ] `imuml/` builds with only ImGui (no nymph include) and has its own
      tests.
- [ ] Existing example PNGs are unchanged.
- [ ] Builds on Linux (GCC); MSYS2 and MSVC recorded as verified or not.
- [ ] README, `docs/`, CHANGELOG, DESIGN.md and `spec/INDEX.md` match the
      code.
