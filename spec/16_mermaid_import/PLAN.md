# nymph — Mermaid Import: Plan

Design: [DESIGN.md](DESIGN.md). Each step is a reviewable deliverable; the
next starts after its tests pass. Steps 1-6 give a working path end to end
for the first two kinds (pie, flowchart) on every front end; steps 7-12
add the other kinds to the same converter; step 13 finishes the
documentation. Tests go in `tests/` with `nymph_add_test(<name>
<sources>)`, deterministic, with temporary file names that include the
test name. Converter tests compare against **golden files**:
`tests/data/mermaid/<kind>/<case>.mmd` beside the exact expected
`<case>.yaml` (and `<case>.warnings` when there are any), so a reviewer
reads input and output side by side. A `CHANGELOG.md` entry is added with
each user-visible step (3, 4, 5, 6, and one per new kind).

## 1. Reader, writer and public API, with `pie`

**Goal:** `import_mermaid()` converts a `pie` chart to a valid nymph source
and reports errors and warnings at mermaid positions.

**Deliverables:** `src/mermaid/reader.hpp/.cpp` (statements with
positions, `%%` comments, `%%{init}%%` directives, front matter `title` /
`config`, ` ```mermaid ` fence, `accTitle` / `accDescr`, quoted and
markdown strings, `<br>`, entity codes, kind keyword);
`src/mermaid/yaml_writer.hpp/.cpp` (the model-to-YAML writer with line
map, `encode_text()` for scalars, `data` as CSV); `src/mermaid/pie.cpp`;
`src/mermaid/mermaid.cpp` (`is_mermaid()`, `import_mermaid()`: dispatch by
kind, the "unsupported kind" error listing the supported ones, the final
`check_source()` with remapped errors); the declarations of DESIGN.md
section 7 in `src/nymph.hpp`; sources listed in `src/CMakeLists.txt` in
the `nymph` library. Pie needs no sizing (a plot fills any image), so
sizing waits for step 2.

**Tests** (`test_mermaid_reader`, `test_mermaid_pie`):
`is_mermaid` true for every mermaid kind keyword, supported or not, (with front
matter, comments, a directive, a fence, leading blank lines, CRLF) and
false for every file in `examples/` and an empty text; statements keep
line and column across `;`, comments and multi-byte UTF-8; markdown
strings, `<br>` and entity codes decode. Pie golden files (title, values,
`showData` warning, accessibility description, front matter title); a
`gitGraph` is an error at 1:1 naming the supported kinds; a bad value is
an error at its column; a remap test: a converter stub whose YAML fails
`check_source()` reports the mermaid line and `(nymph YAML line N)`;
converting twice gives identical bytes.

## 2. `flowchart`

**Goal:** mermaid flowcharts (DESIGN.md section 4.1, styling included)
convert to `Flowchart` sources with an image sized to the diagram.

**Deliverables:** `src/mermaid/flowchart.cpp` (nodes and shapes, `@{}`
shapes with the shape-name table, links and their forms, chains and `&`,
subgraphs and node membership, styling statements to colour fields,
warnings); sizing in `mermaid.cpp` (bind, measure the root's natural size,
write `image`, clamped to 320 to 4096). Before writing it, confirm the
layout engine exposes a diagram's natural size outside a layout (as
`nymph view`'s `layout_width` / `layout_height` do); if it does not,
record the helper added here.

**Tests** (`test_mermaid_flowchart`): golden files for every shape (both
syntaxes), every link form with and without text, extra dashes to
`min_length`, chains with `&`, implicit nodes in order of mention, nested
subgraphs with a node named in two of them (innermost wins: compare with
mermaid's documented example), a link to a subgraph, `graph LR` / `TD` and
`flowchart RL`, `classDef` + `class` + `:::` + `style` + `linkStyle`
(colours mapped, `stroke-width` warned), `click`, `~~~` and subgraph
`direction` warnings; `@{shape: blob}` is an error with a suggestion;
subgraphs nested 9 deep are an error before recursion; a 600-node chain
reports the node limit at the 501st node's line; `examples/flowchart.yaml`
written as mermaid converts to a source that renders; the written `image`
fits (rendering the converted source never fails the `min_zoom` check)
for a 3-node and a 120-node chart.

## 3. Command line

**Goal:** the `nymph` command reads mermaid everywhere it reads YAML and
converts with `convert` or `-o x.yaml`.

**Deliverables:** `cli_support`: `load_source()` (PNG, mermaid, YAML)
returning the YAML, kind and warnings, replacing `source_of()` at its call
sites; `cli.cpp`: `convert` (stdout by default, `-o` must end in `.yaml` /
`.yml`), the implicit command's `-o *.yaml` rule, warnings on standard
error as `nymph: <file>:<line>:<col>: warning: <message>`, usage text;
`docs/cli.md` (commands table, warnings, a mermaid example).

**Tests** (`test_cli`, extended): `nymph x.mmd` writes `x.png` whose
extracted source equals `nymph convert x.mmd`; `-o x.yaml` writes that
YAML; stdin mermaid with `render - -o -`; `check` silent on valid mermaid
and positioned on invalid; warnings on stderr with exit 0; `convert` of a
YAML passes it through, of a PNG extracts, of a bad YAML fails positioned;
`convert -o x.png` and `convert -o x.mmd` are usage errors (exit 2);
nothing is written on failure.

## 4. MCP tools

**Goal:** agents render, check and convert mermaid through `nymph mcp`,
and send mermaid to the open editor.

**Deliverables:** `src/mcp/tools.cpp`: `load_chart()` through
`load_source()`; `render_chart`'s kind and warnings line, descriptions
mentioning mermaid; new `convert_chart` (`source` | `input_path`,
`output_path` `.yaml` / `.yml`); `src/mcp/editor_tools.cpp`:
`editor_set_source` converts mermaid (errors as tool errors at mermaid
positions); `docs/claude.md`. The agent text was written ahead of the
code (with the design): `claude/skill/nymph/SKILL.md` ("Diagrams: draft
in mermaid, finish in nymph"), `docs/agent-guide.md` ("Starting from
mermaid", two command rows), `cursor/rule/nymph.mdc.in`, and their plugin
copies. This step checks that text against what was built and corrects
it (tool names, arguments, warning wording). The skill and plugin sync
checks are updated if they pin tool lists.

**Tests** (`test_mcp_tools`, `test_editor_server` extended): `render_chart`
with mermaid `source` returns an image, `structuredContent.source` equal to
`convert_chart`'s YAML, and the kind line; a warning appears in the text;
an error is `isError` at the mermaid position; `convert_chart` writes only
`.yaml` / `.yml` (`.png` and `.mmd` refused) and returns YAML for YAML and
PNG inputs; `tools/list` includes `convert_chart` with annotations;
`editor_set_source` with mermaid changes the editor's source to the YAML
in one undo step. By hand: Claude Code renders a mermaid flowchart and the
App shows it live.

## 5. `nymph view`

**Goal:** `nymph view x.mmd [--edit]` shows the converted chart and saves
only YAML or PNG.

**Deliverables:** `view_command.cpp`: `prepare_view()` through
`load_source()`, warnings printed before the window opens;
`viewer_window.cpp`: a mermaid input saves to `<stem>.yaml` (asking before
overwriting an existing file through the save dialog), after which the
window's input, title and unsaved-changes check follow the YAML file;
`Y` / `S` unchanged (`.view.yaml`, `.view.png`); `docs/viewer.md`.

**Tests** (`test_view_command` or the nearest existing view test):
`prepare_view` on a `.mmd` gives the converted source and size; the save
path of a mermaid input is `<stem>.yaml` and of YAML / PNG inputs is
unchanged (a pure function, tested without a window); an existing
`<stem>.yaml` asks. By hand on Linux (Xvfb) and Windows: open, edit a
field, save, check that the `.mmd` is unchanged and the `.yaml` renders.

## 6. WebAssembly and JavaScript

**Goal:** the web viewer and `<nymph-viewer>` show mermaid, and the
module converts on its own.

**Deliverables:** `web/nymph_web.cpp`: `nymph_viewer_set_source` imports
mermaid; new export `nymph_import_mermaid` (JSON result); the exported
function list in `web/CMakeLists.txt`; `web/js/nymph-viewer.js`:
`importMermaid()` on the core, `info().importedFrom` and the warnings on
`nymph-load`, file names for saving from a `.mmd` `src`;
`docs/viewer.md` (API, a mermaid embedding example).

**Tests** (`web/tests/node`): `importMermaid` returns the same YAML as the
native converter for the step 1-2 golden files (byte compare, proving
native and web agree); `setSource(mermaid)` then `source()` and `toYaml()`
give YAML; an error has the mermaid position. (`web/tests/browser`):
`<nymph-viewer src="x.mmd">` draws and fires `nymph-load` with
`importedFrom`. The Claude App test page draws a `render_chart` result made
from mermaid.

## 7. `sequenceDiagram`

**Goal:** sequence diagrams convert per DESIGN.md section 4.2.

**Deliverables:** `src/mermaid/sequence.cpp`; dispatch entry; CHANGELOG.

**Tests** (`test_mermaid_sequence`): golden files for every arrow,
`+` / `-`, `activate` / `deactivate` statements (and the error without a
message to attach to), create / destroy, notes (left, right, over two),
every fragment with `else` / `and` / `option`, `rect`, `autonumber`, `box`
and `link` warnings, participant types; `examples/uml_sequence.yaml` as
mermaid converts and renders.

## 8. `classDiagram`

**Goal:** class diagrams convert per DESIGN.md section 4.3.

**Deliverables:** `src/mermaid/class.cpp`; dispatch; CHANGELOG.

**Tests** (`test_mermaid_class`): golden files for class blocks and
`A : member` lines (methods split by `(`), generics, labels, stereotypes,
every relation in both directions with the `from` / `to` that nymph
expects, cardinalities and labels, notes, `namespace` and lollipop
warnings; `examples/uml_class.yaml` as mermaid converts and renders.

## 9. `stateDiagram`

**Goal:** state diagrams convert per DESIGN.md section 4.4.

**Deliverables:** `src/mermaid/state.cpp`; dispatch; CHANGELOG.

**Tests** (`test_mermaid_state`): golden files for `[*]` at top level and
inside composites (scoped ids, no collision with a written `start`),
descriptions both ways and repeated, composites 3 deep, choice / fork /
join, notes; `--` is an error at its line; a transition into `[*]` as a
target written as a source (`[*]` both sides) behaves as mermaid;
`examples/uml_state.yaml` as mermaid converts and renders.

## 10. `erDiagram`

**Goal:** ER diagrams convert per DESIGN.md section 4.5.

**Deliverables:** `src/mermaid/er.cpp`; dispatch; CHANGELOG.

**Tests** (`test_mermaid_er`): golden files for every cardinality symbol
and word form on both ends, identifying and non-identifying, aliases,
attribute blocks with keys and comments, entities named only in
relationships; `examples/uml_er.yaml` as mermaid converts and renders.

## 11. `gantt`

**Goal:** gantt charts convert per DESIGN.md section 4.6, with mermaid's
schedule resolved to explicit dates.

**Deliverables:** `src/mermaid/gantt.cpp` (date formats, durations,
implicit starts, `after`, `until`, tags); dispatch; CHANGELOG.

**Tests** (`test_mermaid_gantt`): golden files for implicit sequencing
across sections, `after` with several ids, `until`, every duration unit,
`dateFormat DD-MM-YYYY` and `X`, status precedence, milestones, generated
ids only where needed; an unsupported date token is an error;
`excludes weekends` is a warning; no `today` key is ever written;
`examples/gantt.yaml` as mermaid converts and renders.

## 12. `mindmap`, `sankey-beta`, `xychart-beta`

**Goal:** the remaining kinds convert per DESIGN.md sections 4.7 and 4.8.

**Deliverables:** `src/mermaid/mindmap.cpp`, `sankey.cpp`, `xychart.cpp`;
dispatch; CHANGELOG.

**Tests** (`test_mermaid_charts`): mind map golden files for every node
form, irregular indentation (mermaid's rule: deeper than the parent),
icon / class warnings, 33 levels an error; sankey with quoted CSV cells
and a cycle reported at the mermaid row; xychart with categories and a
numeric x range, bars and lines, horizontal, axis ranges.

## 13. Documentation and examples

**Goal:** users and agents can find, use and trust mermaid import.

**Deliverables:** `docs/mermaid.md` (the workflow, every mapping table of
DESIGN.md section 4 as user documentation, the `@{shape}` table, what is
dropped); `examples/mermaid/` with one `.mmd` per supported kind and the
PNGs rendered from them (added to whatever regenerates or checks
`examples/`); `docs/examples.md` section; `README.md` (one sentence in the
introduction, the Quick Start, the Further Documentation row for
`docs/mermaid.md`); `docs/agent-guide.md` and the skill point to
`docs/mermaid.md`; MCP resource `nymph://docs/mermaid` if the agent guide
resource pattern makes it one line; `spec/INDEX.md` status;
`spec/03_uml_diagrams/DESIGN.md` section 10's importer bullet points here.

**Tests:** `test_docs` (or the docs check that exists) covers the new
page if it checks links; every `examples/mermaid/*.mmd` converts with no
warnings and renders (added to the examples test); the skill size check
still passes.

## Completion Criteria

- Every test above passes in CI on Linux, macOS, MSVC and MSYS2, and the
  web tests in Node and Chromium; existing tests still pass and every
  existing example renders to the same bytes (YAML input is untouched).
- For each supported kind, mermaid's own documentation examples (copied
  into `tests/data/mermaid/upstream/`, with their licence noted) convert
  without errors except where DESIGN.md section 4 says error, and render.
- The same mermaid gives the same YAML from the native tool, `nymph mcp`
  and the WebAssembly module.
- No command, tool or viewer writes mermaid; every written PNG carries
  YAML.
- By hand: Claude Code draws a mermaid flowchart through `render_chart`,
  revises it, and `convert_chart` writes a YAML that `nymph view --edit`
  opens and the user adjusts.
- `spec/INDEX.md`, `README.md`, `docs/cli.md`, `docs/viewer.md`,
  `docs/claude.md`, `docs/agent-guide.md`, `docs/mermaid.md`, the skill and
  `CHANGELOG.md` describe what was built.
