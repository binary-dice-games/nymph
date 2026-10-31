// MIT License © 2026 Binary Dice Games
// spec/15_web_app plan steps 7 to 9: the app's pure functions in Node: URL
// rewrites, the view key ignored by "modified", share links' encoding, and
// the import wizard's CSV parsing and chart writing (app/import.js).
//
//   node app.mjs <build>/web <nymph source dir>

import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { pathToFileURL } from 'node:url';

const [webDir, sourceDir] = process.argv.slice(2);
const appDir = join(sourceDir, 'web', 'app', 'app');
const { rawUrl, sniff, isPng } = await import(pathToFileURL(join(appDir, 'open.js')).href);
const { withoutView, viewBlock, withView, Doc, baseName } = await import(pathToFileURL(join(appDir, 'document.js')).href);
const { encodeSource, decodeSource } = await import(pathToFileURL(join(appDir, 'share.js')).href);
const imp = await import(pathToFileURL(join(appDir, 'import.js')).href);
const { chartIcon } = await import(pathToFileURL(join(appDir, 'icons.js')).href);
void webDir;

const tests = [];
const test = (name, fn) => tests.push({ name, fn });
const eq = (a, b, what) => {
  if (a !== b) {
    throw new Error(`${what}: expected ${JSON.stringify(b)}, got ${JSON.stringify(a)}`);
  }
};

test('GitHub file pages and gists open their raw files', () => {
  eq(rawUrl('https://github.com/o/r/blob/main/charts/a.yaml'), 'https://raw.githubusercontent.com/o/r/main/charts/a.yaml', 'blob');
  eq(rawUrl('https://github.com/o/r/raw/v1/a.png'), 'https://raw.githubusercontent.com/o/r/v1/a.png', 'raw');
  eq(rawUrl('https://gist.github.com/someone/0123abcd'), 'https://gist.githubusercontent.com/someone/0123abcd/raw', 'gist');
  eq(rawUrl(' https://example.com/x.yaml '), 'https://example.com/x.yaml', 'other');
  let threw = false;
  try {
    rawUrl('file:///etc/passwd');
  } catch {
    threw = true;
  }
  eq(threw, true, 'file: refused');
});

test('the view key does not count as a change', () => {
  const a = 'root: {type: Plot}\ndata: |\n  x\n  1\n';
  const b = `${a}view:\n  root: {x: [0, 1]}\n`;
  eq(withoutView(b), withoutView(a), 'same without the view');
  const doc = new Doc({ name: 'a.yaml', origin: { kind: 'local' }, source: a });
  doc.source = b;
  eq(doc.modified, false, 'zoom alone is not modified');
  doc.source = b.replace('Plot', 'Plot3D');
  eq(doc.modified, true, 'an edit is');
  eq(doc.fileName('png'), 'a.png', 'png name');
  eq(baseName('C:\\x\\y.yaml'), 'y.yaml', 'base name');
});

test('a guest\'s own view key replaces the owner\'s (spec/18_collaboration)', () => {
  const owner = 'root: {type: Plot}\nview:\n  root: {x: [0, 1]}\n\ndata: |\n  x\n  1\n';
  eq(viewBlock(owner), 'view:\n  root: {x: [0, 1]}\n', 'the view block');
  eq(viewBlock('root: {type: Plot}\n'), '', 'no view');
  const mine = 'view:\n  root: {x: [5, 6]}\n';
  const shown = withView(owner, mine);
  eq(viewBlock(shown), mine, 'the guest\'s view');
  eq(withoutView(shown), withoutView(owner), 'the owner\'s chart otherwise');
  eq(withView(owner, ''), withoutView(owner), 'no view of its own');
});

test('sniffing: PNG, UTF-8 text, and the rest refused', () => {
  eq(sniff(new Uint8Array([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0])).png, true, 'png');
  eq(sniff(new TextEncoder().encode('\ufeffroot: x')).text, 'root: x', 'bom dropped');
  let threw = false;
  try {
    sniff(new Uint8Array([0xff, 0xfe, 0x00, 0xd8]));
  } catch {
    threw = true;
  }
  eq(threw, true, 'binary refused');
  eq(isPng(new Uint8Array([1, 2])), false, 'short');
});

test('share links round-trip the source exactly', async () => {
  const source = 'root:\n  type: Label\n  text: "Grüße, 日本 — ok"\n'.repeat(20);
  const y = await encodeSource(source);
  eq(/^[A-Za-z0-9_-]+$/.test(y), true, 'base64url');
  eq(await decodeSource(y), source, 'same text');
});

// ── Import (app/import.js) ───────────────────────────────────────────────────

test('CSV: quotes, delimiters, unique names and number formats', () => {
  const t = imp.parseCsv('\ufeffQuarter,"Revenue, USD",Revenue,\nQ1,"$1,200",3,x\n\nQ2,"1,500.5",4,y\n');
  eq(t.header.join('|'), 'Quarter|Revenue, USD|Revenue|column 4', 'header');
  eq(t.rows.length, 2, 'blank line skipped');
  const info = imp.analyzeTable(t);
  eq(info.columns[1].numeric, true, 'currency and thousands are numbers');
  eq(info.columns[1].numbers[1], 1500.5, 'value');
  eq(info.columns[0].numeric, false, 'text column');
  const eu = imp.analyzeTable(imp.parseCsv('name;share\na;1,5\nb;2,25\n'));
  eq(eu.delimiter, ';', 'semicolon');
  eq(eu.columns[1].numbers[1], 2.25, 'decimal comma');
  eq(imp.detectDelimiter('a\tb\n1\t2'), '\t', 'tab');
  let threw = false;
  try {
    imp.parseCsv('only,a,header\n');
  } catch {
    threw = true;
  }
  eq(threw, true, 'no data rows');
});

test('CSV: suggested kind and default mapping', () => {
  const info = imp.analyzeTable(imp.parseCsv('team,open,closed\nWeb,1,2\nAPI,3,4\n'));
  eq(imp.suggestKind(info), 'bars', 'categories: bars');
  const m = imp.defaultMapping('bars', info);
  eq(m.x, 0, 'categories from the text column');
  eq(m.series.join(','), '1,2', 'every number column');
  eq(imp.defaultMapping('pie', info).series.length, 1, 'a pie takes one');
  const xy = imp.analyzeTable(imp.parseCsv('t,v\n1,2\n2,3\n'));
  eq(imp.suggestKind(xy), 'line', 'numbers only: line');
  eq(imp.defaultMapping('line', xy).x, 0, 'first number column is X');
  const mixed = imp.analyzeTable(imp.parseCsv('q,revenue,margin\nQ1,120000,20\nQ2,130000,25\n'));
  eq(imp.defaultMapping('line', mixed).series.join(','), '1', 'a far smaller column is left out');
});

test('CSV: every chart kind writes a report-style source', () => {
  const info = imp.analyzeTable(imp.parseCsv('Quarter,Revenue,"unit cost",a:b\nQ1,121,9,1\nQ2,138,8,2\n'));
  for (const k of imp.CHART_KINDS) {
    const look = { ...imp.defaultLook('sales.csv'), subtitle: 'In 2025: kUSD', cards: true, theme: 'ocean', header: 'box' };
    const src = imp.csvChartSource(info, k.id, { x: 0, series: k.series === 'one' ? [1] : [1, 2, 3] }, look, ['#112233']);
    eq(/^image: \{width: 960, height: 600, theme: ocean/m.test(src), true, `${k.id}: image`);
    eq(src.includes('card: true'), true, `${k.id}: text box`);
    eq(src.includes('font: PT_Serif-Web-Bold.ttf'), true, `${k.id}: heading font`);
    eq(src.includes('{type: Separator}'), true, `${k.id}: rule`);
    eq(src.includes('text: "In 2025: kUSD"'), true, `${k.id}: quoted subtitle`);
    eq(/^data: \|\n {2}Quarter, Revenue, unit cost, a:b/m.test(src), true, `${k.id}: data`);
  }
  const bars = imp.csvChartSource(info, 'bars', { x: 0, series: [1, 2, 3] }, imp.defaultLook('x.csv'));
  eq(bars.includes('values: [$Revenue, "$unit cost", $4]'), true, 'column references');
  const pie = imp.csvChartSource(info, 'pie', { x: 0, series: [1] }, imp.defaultLook('x.csv'));
  eq(pie.includes('Revenue share'), true, 'pie shares as a column');
  eq(pie.includes('46.72'), true, 'share value');
  let threw = false;
  try {
    imp.csvChartSource(info, 'line', { x: 0, series: [] }, imp.defaultLook('x.csv'));
  } catch {
    threw = true;
  }
  eq(threw, true, 'no series');
});

test('YAML texts and column references are quoted where needed', () => {
  eq(imp.yamlText('Sales'), 'Sales', 'plain');
  eq(imp.yamlText('a, b'), '"a, b"', 'comma');
  eq(imp.yamlText('yes'), '"yes"', 'reserved');
  eq(imp.yamlText('$5 each'), '"$$5 each"', 'leading $ doubled');
  eq(imp.columnRef({ name: 'x_1', index: 0 }), '$x_1', 'simple');
  eq(imp.columnRef({ name: 'unit price', index: 1 }), '"$unit price"', 'spaces');
  eq(imp.columnRef({ name: '2024', index: 4 }), '$5', 'digits by index');
  eq(imp.niceCeil(171), 200, 'nice');
  eq(imp.niceCeil(0.9), 1, 'nice small');
});

test('mermaid: detection, and the diagram framed with a heading', () => {
  eq(imp.isMermaidFile('a.mmd', ''), true, 'by name');
  eq(imp.isMermaidFile('a.txt', '%% c\nflowchart LR\n A-->B'), true, 'by keyword');
  eq(imp.isMermaidFile('a.csv', 'a,b\n1,2'), false, 'csv');
  const conv = 'nymph: 1\ndescription: A flow.\nimage: {width: 700, height: 300}\nsource: Converted.\nroot:\n  type: Flowchart\n  title: Orders\n  children:\n    - {type: FlowNode, id: A, text: A}\n';
  eq(imp.mermaidTitle(conv), 'Orders', 'title');
  const out = imp.mermaidChartSource(conv, { ...imp.defaultLook('orders.mmd', 'mermaid'), title: 'Orders', theme: 'paper' });
  eq(out.includes('    - type: Flowchart\n      height: -1\n      children:\n        - {type: FlowNode'), true, 'root as a stretching child');
  eq(out.includes('title: Orders\n'), false, 'the heading replaces its title');
  eq(/^image: \{width: 720, height: \d+, theme: paper/m.test(out), true, 'image');
  eq(Number(/height: (\d+)/.exec(out)[1]) > 300, true, 'taller for the heading');
  eq(out.includes('source: Converted.'), true, 'other keys kept');
  const flow = imp.mermaidChartSource('root: {type: Label, text: x}\n', { ...imp.defaultLook('x.mmd', 'mermaid'), size: 'wide' });
  eq(flow.includes('    - {type: Label, text: x}'), true, 'flow-style root');
  eq(flow.includes('width: 960, height: 600'), true, 'chosen size');
});

// ── Chart pictograms (app/icons.js) ─────────────────────────────────────────

test('every template and import chart kind has a pictogram', () => {
  const templates = JSON.parse(readFileSync(join(sourceDir, 'web', 'app', 'templates', 'index.json'), 'utf8'));
  for (const t of templates) {
    eq(chartIcon(t.icon).startsWith('<svg'), true, `template ${t.name}`);
  }
  for (const k of ['line', 'area', 'bars', 'stacked', 'hbars', 'scatter', 'pie', 'histogram']) {
    eq(chartIcon(k).startsWith('<svg'), true, `kind ${k}`);
  }
  eq(chartIcon('nope'), '', 'unknown type');
});

let failed = 0;
for (const { name, fn } of tests) {
  try {
    await fn();
    console.log(`ok   ${name}`);
  } catch (e) {
    failed++;
    console.log(`FAIL ${name}: ${e.message}`);
  }
}
console.log(`${tests.length - failed}/${tests.length} passed`);
process.exit(failed ? 1 : 0);
