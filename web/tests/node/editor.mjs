// MIT License © 2026 Binary Dice Games
// spec/15_web_app plan step 6: the editor module (nymph-editor-core.wasm)
// driven from Node, headless (frames run, nothing is drawn).
//
//   node editor.mjs <build>/web <nymph source dir>

import { readFile } from 'node:fs/promises';
import { join } from 'node:path';
import { pathToFileURL } from 'node:url';

const [webDir, sourceDir] = process.argv.slice(2);
if (!webDir || !sourceDir) {
  console.error('usage: node editor.mjs <build>/web <nymph source dir>');
  process.exit(2);
}

const tests = [];
const test = (name, fn) => tests.push({ name, fn });
const assert = (cond, message) => {
  if (!cond) {
    throw new Error(message);
  }
};
const assertEq = (actual, expected, what) =>
  assert(actual === expected, `${what}: expected ${JSON.stringify(expected)}, got ${JSON.stringify(actual)}`);

const { createEditor, NymphError } = await import(pathToFileURL(join(webDir, 'editor', 'nymph-editor.js')).href);
const { loadNymph } = await import(pathToFileURL(join(webDir, 'nymph-viewer.js')).href);
const fetchFont = async (name) => new Uint8Array(await readFile(join(webDir, 'fonts', name)));
const example = (name) => readFile(join(sourceDir, 'examples', `${name}.yaml`), 'utf8');

const editor = await createEditor(null, { headless: true, fetchFont, width: 1100, height: 700 });

test('opens an example, unmodified, with the panel', async () => {
  await editor.open(await example('dashboard'));
  editor.frame(4);
  const s = editor.state();
  assertEq(s.modified, false, 'modified');
  assertEq(s.panel, true, 'panel');
  assert(s.stripWidth > 0, 'a strip');
});

test('a tool sets a field, which marks the document modified and can be undone', async () => {
  await editor.open(await example('line'));
  const before = editor.source();
  const changes = [];
  const off = editor.on('change', (c) => changes.push(c));
  const r = editor.tool('editor_set_fields', { path: 'root', fields: { title: 'Changed by a test' } }, { origin: 'agent:test' });
  assert(typeof r.version === 'number', 'a version');
  assert(editor.source().includes('Changed by a test'), 'source has the title');
  assertEq(editor.modified(), true, 'modified');
  assertEq(changes.length, 1, 'one change event');
  assertEq(changes[0].origin, 'agent:test', 'origin');
  assertEq(changes[0].label, 'Editor set fields', 'label');
  editor.undo();
  assertEq(editor.source(), before, 'undone');
  assertEq(editor.modified(), false, 'not modified');
  off();
});

test('a group of tool calls is one undo step', async () => {
  await editor.open(await example('flowchart'));
  const before = editor.source();
  editor.beginGroup('Agent turn', 'agent:test');
  editor.tool('editor_add_element', { parent: 'root', type: 'FlowNode', fields: { text: 'Billing' } }, { origin: 'agent:test' });
  editor.tool('editor_set_fields', { path: 'root', fields: { title: 'Grouped' } }, { origin: 'agent:test' });
  editor.endGroup();
  assertEq(editor.state().undo, 'Agent turn', 'undo label');
  editor.undo();
  assertEq(editor.source(), before, 'one undo restores both');
});

test('setSource asChange is one undo step; without it, a new document', async () => {
  await editor.open(await example('line'));
  const before = editor.source();
  const changed = before.replace(/title: [^\n]*/, 'title: From the text');
  await editor.setSource(changed, { asChange: true });
  assertEq(editor.source(), changed, 'changed');
  assertEq(editor.modified(), true, 'modified');
  editor.undo();
  assertEq(editor.source(), before, 'undone');
  await editor.setSource(changed);
  assertEq(editor.modified(), false, 'a new document is not modified');
  assertEq(editor.state().undo, '', 'no history');
});

test('toPng carries toYaml', async () => {
  await editor.open(await example('bars'));
  const png = editor.toPng();
  assert(png.length > 100 && png[1] === 0x50, 'a PNG');
  const core = await loadNymph({ fetchFont });
  const text = new TextDecoder().decode(png);
  assert(text.includes('PlotBars') || text.includes('nymph'), 'the PNG carries the source');
  void core;
});

test('a source that does not bind rejects with its position and keeps the document', async () => {
  await editor.open(await example('line'));
  const before = editor.source();
  let error = null;
  try {
    await editor.open('root:\n  type: Plot\n  children:\n    - {type: PlotLine, ys: [1, 2], bogus: 3}\n');
  } catch (e) {
    error = e;
  }
  assert(error instanceof NymphError, 'a NymphError');
  assert(error.line === 4, `line 4, got ${error.line}`);
  assertEq(editor.source(), before, 'previous source kept');
});

test('a refused tool change is a NymphError and changes nothing', async () => {
  await editor.open(await example('line'));
  const before = editor.source();
  let error = null;
  try {
    editor.tool('editor_set_fields', { path: 'root', fields: { title: 'x' } , expected_version: 12345 });
  } catch (e) {
    error = e;
  }
  assert(error instanceof NymphError, 'a NymphError');
  assertEq(editor.source(), before, 'unchanged');
});

test('tool definitions are the editor tools', () => {
  const names = editor.toolDefinitions().map((t) => t.name);
  for (const n of ['editor_state', 'editor_set_fields', 'editor_add_element', 'editor_undo']) {
    assert(names.includes(n), `has ${n}`);
  }
});

test('CSV export and import go through the page', async () => {
  await editor.open(await example('dashboard'));
  const state = editor.tool('editor_state');
  assert(state.tables && state.tables.length > 0, 'the example has tables');
  const table = state.tables[0].name;
  const csv = editor.tool('editor_table', { table }).csv;
  assert(typeof csv === 'string' && csv.includes('\n'), 'the table as CSV');
  const lines = csv.trim().split('\n');
  const extra = lines[lines.length - 1];
  editor.importCsv(table, csv.trim() + '\n' + extra + '\n');
  assertEq(editor.tool('editor_table', { table }).csv.trim().split('\n').length, lines.length + 1, 'a row more');
  editor.undo();
  let error = null;
  try {
    editor.importCsv(table, 'nothing,like\n1,2\n');
  } catch (e) {
    error = e;
  }
  assert(error instanceof NymphError, 'a CSV that breaks the chart is refused');
});

test('a broken source is refused and the document stays', async () => {
  const before = editor.source();
  const broken = 'root:\n  type: Plot\n  children:\n    - {type: PlotLine, ys: [1, 2], bogus: 3}\n';
  let error = null;
  try {
    await editor.open(broken);
  } catch (e) {
    error = e;
  }
  assert(error instanceof NymphError, 'a source that does not load is refused');
  assertEq(editor.source(), before, 'the document is unchanged');
});

let failed = 0;
for (const { name, fn } of tests) {
  try {
    await fn();
    console.log(`ok   ${name}`);
  } catch (e) {
    failed++;
    console.log(`FAIL ${name}: ${e.stack ?? e.message}`);
  }
}
editor.destroy();
console.log(`${tests.length - failed}/${tests.length} passed`);
process.exit(failed ? 1 : 0);
