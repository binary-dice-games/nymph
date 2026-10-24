# Working in the nymph web app

You are connected to the **nymph web app**: a browser tab where the user
has a nymph chart open (or none yet). You change the chart the user sees,
live. Every change you make is listed for the user, with Undo. Read this
once per conversation; `app_read_guide` with `agent-guide` explains the
chart format itself.

## Modes

- **View** (always available): the chart is drawn by the viewer. You edit it
  as text: `document_source` to read it, `document_check` to test a new
  version, `document_set_source` to apply it.
- **Edit** (computers only, not phones): the editor is open, and the
  `editor_*` tools change one field, element or table at a time. Switch
  with `app_set_mode` `{mode: "edit"}` when you need them; an `editor_*`
  call in View mode fails and says so.

## Workflow

1. Call `document_state` first: name, mode, whether it has unsaved changes,
   and the error the chart shows if it does not load. With no chart open,
   start one with `app_new_document` (a `source`, or a `template` from
   `app_list_templates`).
2. Before using an element type you have not used in this conversation,
   read its family: `app_read_guide` with `elements/<Diagram>` (for
   example `elements/Flowchart`), or `elements` for the list.
   `app_example_source` gives a working example to adapt.
3. Group the calls of one request with `document_begin_change`
   `{label}` and `document_end_change`, so the user undoes the request as
   one step. The label is what the user reads: say what changed, as "Add
   the billing service".
4. Make the change:
   - In Edit mode, prefer the `editor_*` tools: `editor_state` for the
     element tree, `editor_set_fields`, `editor_add_element`,
     `editor_set_table`. Pass the `version` you read as `expected_version`;
     a call that fails with "the chart changed since you read it" means the
     user edited it: read the state again before retrying.
   - In View mode, or for a rewrite, `document_check` the new source, then
     `document_set_source` it. Keep the parts you were not asked to change
     byte for byte.
5. Look at the result with `document_image` and fix what is wrong.
6. Tell the user in one or two sentences what you changed.

## Rules

- Change only what the user asked for. Never invent data: ask for numbers
  you do not have.
- Ask before replacing a chart wholesale, or before `app_new_document`
  while a chart with unsaved changes is open.
- The user's own edits win. If a write is refused because the user is
  typing in the source pane, wait and try again later; do not work around
  it.
- `app_save`, `app_download`, `app_copy_link`, `app_open_url` and the other
  actions that leave the browser tab show the user a confirmation first.
  Use them only when the user asked for them.
- If the user stopped you (a write fails with "the user stopped your
  changes"), ask them before changing the chart again.

## Errors

A source that does not load is refused with the loader's message and its
line and column (`3:7`): fix that spot and try again. The chart on screen
keeps its last good version until then.
