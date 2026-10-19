// MIT License © 2026 Binary Dice Games
// spec/04_web_viewer plan step 9: the WebAssembly module driven from Node
// with the software rasterizer.
//
//   node run.mjs <build>/web <nymph source dir>
//
// No dependencies: a tiny test runner and PNG decoder are below.

import { readFile, readdir } from 'node:fs/promises';
import { join } from 'node:path';
import { pathToFileURL } from 'node:url';
import { gzipSync, inflateSync } from 'node:zlib';

const [webDir, sourceDir] = process.argv.slice(2);
if (!webDir || !sourceDir) {
  console.error('usage: node run.mjs <build>/web <nymph source dir>');
  process.exit(2);
}

// ── Test runner ──────────────────────────────────────────────────────────────

const tests = [];
const test = (name, fn) => tests.push({ name, fn });
const assert = (cond, message) => {
  if (!cond) {
    throw new Error(message);
  }
};
const assertEq = (actual, expected, what) =>
  assert(actual === expected, `${what}: expected ${JSON.stringify(expected)}, got ${JSON.stringify(actual)}`);

// ── PNG decoding (8-bit RGB / RGBA, not interlaced: what nymph writes) ───────

function decodePng(bytes) {
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  let width = 0, height = 0, channels = 0;
  const idat = [];
  for (let at = 8; at < bytes.length; ) {
    const length = view.getUint32(at);
    const type = String.fromCharCode(...bytes.subarray(at + 4, at + 8));
    const data = bytes.subarray(at + 8, at + 8 + length);
    if (type === 'IHDR') {
      width = view.getUint32(at + 8);
      height = view.getUint32(at + 12);
      assert(data[8] === 8 && data[12] === 0, 'only 8-bit, non-interlaced PNGs');
      channels = { 2: 3, 6: 4 }[data[9]];
      assert(channels, `colour type ${data[9]} not supported`);
    } else if (type === 'IDAT') {
      idat.push(data);
    }
    at += 12 + length;
  }
  const raw = inflateSync(Buffer.concat(idat));
  const stride = width * channels;
  const out = new Uint8Array(width * height * 4);
  let prev = new Uint8Array(stride);
  for (let y = 0; y < height; ++y) {
    const filter = raw[y * (stride + 1)];
    const line = raw.subarray(y * (stride + 1) + 1, (y + 1) * (stride + 1));
    const cur = new Uint8Array(stride);
    for (let i = 0; i < stride; ++i) {
      const a = i >= channels ? cur[i - channels] : 0;
      const b = prev[i];
      const c = i >= channels ? prev[i - channels] : 0;
      let v = line[i];
      if (filter === 1) v += a;
      else if (filter === 2) v += b;
      else if (filter === 3) v += (a + b) >> 1;
      else if (filter === 4) {
        const p = a + b - c, pa = Math.abs(p - a), pb = Math.abs(p - b), pc = Math.abs(p - c);
        v += pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
      }
      cur[i] = v & 255;
    }
    for (let x = 0; x < width; ++x) {
      for (let k = 0; k < 4; ++k) {
        out[(y * width + x) * 4 + k] = k < channels ? cur[x * channels + k] : 255;
      }
    }
    prev = cur;
  }
  return { width, height, rgba: out };
}

/** The GL DESIGN tolerance (spec/02_opengl_rasterizer/DESIGN.md section 6). */
function compareImages(a, b) {
  assertEq(a.width, b.width, 'width');
  assertEq(a.height, b.height, 'height');
  let within = 0, worst = 0, sum = 0;
  const pixels = a.width * a.height;
  for (let i = 0; i < pixels; ++i) {
    let diff = 0;
    for (let k = 0; k < 4; ++k) {
      const d = Math.abs(a.rgba[i * 4 + k] - b.rgba[i * 4 + k]);
      diff = Math.max(diff, d);
      sum += d;
    }
    worst = Math.max(worst, diff);
    within += diff <= 2 ? 1 : 0;
  }
  const share = within / pixels, mean = sum / (pixels * 4);
  assert(share >= 0.995 && worst <= 64 && mean < 0.5, `images differ: ${(share * 100).toFixed(3)}% within 2, worst ${worst}, mean ${mean.toFixed(4)}`);
  return { share, worst, mean };
}

// ── The module ───────────────────────────────────────────────────────────────

const fontRequests = new Map(); // name -> count
const corruptFonts = new Set();
const { createViewer, NymphError } = await import(pathToFileURL(join(webDir, 'nymph-viewer.js')).href);

async function viewer(source, options = {}) {
  return createViewer(null, {
    source,
    fetchFont: async (name) => {
      fontRequests.set(name, (fontRequests.get(name) ?? 0) + 1);
      const bytes = new Uint8Array(await readFile(join(webDir, 'fonts', name)));
      if (corruptFonts.has(name)) {
        bytes[bytes.length >> 1] ^= 1;
      }
      return bytes;
    },
    ...options,
  });
}

/** Runs frames until the viewer settles; the last one drawn. */
function settle(v, clock = { t: 0 }) {
  let last = null;
  for (let i = 0; i < 20 && v.wantsFrame(); ++i) {
    last = v.frameSoftware(clock.t) ?? last;
    clock.t += 1 / 60;
  }
  return last;
}

const examples = (await readdir(join(sourceDir, 'examples'))).filter((f) => f.endsWith('.yaml')).sort();

// ── Tests ────────────────────────────────────────────────────────────────────

// First: the module loads its options once, so the corrupt-font check must
// see the font before any other test loads it.
test('a font file that does not match the catalog is rejected', async () => {
  const source = 'image: {width: 200, height: 60}\nroot: {type: Label, text: Hi, font: PT_Serif-Web-Italic.ttf}\n';
  corruptFonts.add('PT_Serif-Web-Italic.ttf');
  const v = await viewer('root: {type: Label, text: start}\n');
  let error = null;
  await v.setSource(source).catch((e) => (error = e));
  assert(error instanceof NymphError, 'expected a NymphError');
  assertEq(error.kind, 'fonts', 'kind');
  assert(/SHA-256/.test(error.message), error.message);
  assert(v.source().includes('start'), 'the previous chart stays');
  corruptFonts.clear();
  await v.setSource(source); // the failed load is retried
  assert(settle(v) !== null, 'drawn');
  v.destroy();
});

test('every example renders like its committed PNG', async () => {
  for (const name of examples) {
    const source = await readFile(join(sourceDir, 'examples', name), 'utf8');
    const v = await viewer(source);
    const png = decodePng(new Uint8Array(await readFile(join(sourceDir, 'examples', name.replace('.yaml', '.png')))));
    const scale = /scale:\s*(\d)/.exec(source.split('\nroot:')[0])?.[1] ?? 1;
    v.resize(png.width / scale, png.height / scale, Number(scale));
    const frame = settle(v);
    assert(frame !== null, `${name}: no frame`);
    try {
      compareImages(frame, png);
    } catch (e) {
      throw new Error(`${name}: ${e.message}`);
    }
    v.destroy();
  }
});

test('each font is fetched once, however many viewers use it', async () => {
  for (const [name, count] of fontRequests) {
    assert(count === 1 || (name === 'PT_Serif-Web-Italic.ttf' && count === 2), `${name} fetched ${count} times`);
  }
  assert(fontRequests.has('FiraSans-Regular.ttf'), 'the fonts example fetched its fonts');
  assert(!fontRequests.has('PT_Serif-Web-Regular.ttf'), 'an unused font is not fetched');
});

test('a bad source rejects with the library error and its position', async () => {
  const v = await viewer('root: {type: Label, text: ok}\n');
  let error = null;
  await v.setSource('root: {type: Plot, children: [{type: PlotLyne}]}\n').catch((e) => (error = e));
  assert(error instanceof NymphError, 'expected a NymphError');
  assertEq(error.kind, 'source', 'kind');
  assertEq(error.line, 1, 'line');
  assertEq(error.column, 38, 'column');
  assert(error.message.startsWith("unknown element type 'PlotLyne' (did you mean 'PlotLine'?)"), error.message);
  assertEq(v.source(), 'root: {type: Label, text: ok}\n', 'source kept');
  v.destroy();
});

test('hovering a point reports it', async () => {
  const v = await viewer(
    'image: {width: 400, height: 300}\nroot:\n  type: Plot\n  x_min: 0\n  x_max: 4\n  y_min: 0\n  y_max: 10\n' +
      '  children:\n    - {type: PlotLine, label: temp, xs: [0, 1, 2, 3, 4], ys: [1, 4, 7, 2, 5]}\n',
  );
  v.resize(400, 300, 1);
  const clock = { t: 0 };
  settle(v, clock);
  const p = v.state().views[0];
  const x = p.x + ((2 - p.xMin) / (p.xMax - p.xMin)) * p.width;
  const y = p.y + ((p.yMax - 7) / (p.yMax - p.yMin)) * p.height;
  v.pointer('move', { x, y });
  settle(v, clock);
  const hovered = v.state().hovered;
  assert(hovered !== null, 'something hovered');
  assertEq(hovered.label, 'temp', 'label');
  assertEq(hovered.index, 2, 'index');
  assertEq(hovered.values.y, '7', 'y');
  v.pointer('down', { x, y });
  settle(v, clock);
  v.pointer('up', { x, y });
  settle(v, clock);
  assertEq(v.state().click?.index, 2, 'click');
  assertEq(v.state().click, null, 'the click is taken once');
  v.destroy();
});

test('setData, setPng and toPng', async () => {
  const v = await viewer('root: {type: Plot, children: [{type: PlotLine, label: a, ys: $v}]}\ndata: {v: [1, 2]}\n');
  await v.setData({ v: [3, 4, 5] });
  assert(v.source().includes('{"v":[3,4,5]}'), v.source());
  const png = v.toPng();
  assertEq(Buffer.from(png.subarray(1, 4)).toString(), 'PNG', 'signature');
  assert(Buffer.from(png).includes(v.source()), 'the PNG carries the source on screen');
  assert((await v.setPng(png)) === true, 'a nymph PNG');
  const plain = new Uint8Array(await readFile(join(sourceDir, 'resources', 'Nymph.png')));
  assert((await v.setPng(plain)) === false, 'a PNG without a nymph source');
  assertEq(v.state().hasFigure, false, 'no figure');
  v.destroy();
});

test('updates apply in call order', async () => {
  const v = await viewer('root: {type: Label, text: one}\n');
  const order = [];
  const a = v.setSource('image: {width: 200, height: 60}\nroot: {type: Label, text: two, font: FiraMono-Bold.ttf}\n').then(() => order.push('a'));
  const b = v.setSource('root: {type: Label, text: three}\n').then(() => order.push('b'));
  await Promise.all([a, b]);
  assertEq(order.join(), 'a,b', 'order');
  assert(v.source().includes('three'), 'the last update wins');
  v.destroy();
});

test('the module is small', async () => {
  const wasm = await readFile(join(webDir, 'nymph-viewer-core.wasm'));
  const size = gzipSync(wasm, { level: 9 }).length;
  assert(size <= 1.5 * 1024 * 1024, `${size} bytes gzip`);
  console.log(`    nymph-viewer-core.wasm: ${wasm.length} bytes, ${size} gzip`);
});

test('the single-file build draws the same pixels as the two-file one', async () => {
  const single = await import(pathToFileURL(join(webDir, 'nymph-viewer.single.js')).href);
  const source = await readFile(join(sourceDir, 'examples', 'line.yaml'), 'utf8');
  const a = await single.createViewer(null, { source });
  const b = await viewer(source);
  a.resize(800, 420, 1);
  b.resize(800, 420, 1);
  const fa = settle(a), fb = settle(b);
  assert(fa && fb, 'both drawn');
  assert(Buffer.from(fa.rgba).equals(Buffer.from(fb.rgba)), 'same pixels');
  a.destroy();
  b.destroy();
});

test('toYaml saves the view the user left, and reopening it shows the same', async () => {
  const source =
    'image: {width: 400, height: 300}\nroot:\n  type: Plot\n  x_min: 0\n  x_max: 10\n  y_min: 0\n  y_max: 10\n' +
    '  children:\n    - {type: PlotLine, label: a, xs: [0, 10], ys: [0, 10]}\n';
  const v = await viewer(source);
  v.resize(400, 300, 1);
  const clock = { t: 0 };
  settle(v, clock);
  assertEq(v.toYaml(), source, 'nothing changed');
  const p = v.state().views[0];
  const at = (dx) => ({ x: p.x + p.width / 2 + dx, y: p.y + p.height / 2 });
  v.pointer('move', at(0));
  settle(v, clock);
  v.pointer('down', at(0));
  settle(v, clock);
  for (let k = 1; k <= 4; ++k) {
    v.pointer('move', at(-15 * k));
    settle(v, clock);
  }
  v.pointer('up', at(-60));
  settle(v, clock);
  const moved = v.state().views[0];
  const yaml = v.toYaml();
  assert(/\nview:\n  root: \{x: \[/.test(yaml), yaml);
  const again = await viewer(yaml);
  again.resize(400, 300, 1);
  settle(again);
  const reopened = again.state().views[0];
  assert(Math.abs(reopened.xMin - moved.xMin) < 1e-4 && Math.abs(reopened.xMax - moved.xMax) < 1e-4, `${reopened.xMin} vs ${moved.xMin}`);
  // The PNG shows the view on screen and carries the YAML.
  assert(Buffer.from(v.toPng()).includes('view:'), 'the PNG carries the view');
  v.destroy();
  again.destroy();
});

test('user-interface widgets take the pointer and the keyboard', async () => {
  // spec/09_ui_mockups PLAN step 12.
  const source =
    'image: {width: 400, height: 200, padding: 0}\nroot:\n  type: Screen\n  children:\n' +
    '    - type: Window\n      title: Form\n      x: 20\n      y: 20\n      width: 300\n      height: 150\n' +
    '      children:\n        - {type: InputText, label: Name, value: Sun, width: 200}\n';
  const v = await viewer(source);
  v.resize(400, 200, 1);
  const clock = { t: 0 };
  settle(v, clock);
  const field = { x: 100, y: 20 + 22 + 8 + 9 }; // under the title bar, in the field
  v.pointer('move', field);
  settle(v, clock);
  assertEq(v.state().hovered?.type, 'InputText', 'the field is under the pointer');
  assertEq(v.state().wantsKeyboard, false, 'no field has the keyboard yet');
  const revision = v.state().uiRevision;
  v.pointer('down', field);
  settle(v, clock);
  v.pointer('up', field);
  settle(v, clock);
  assertEq(v.state().wantsKeyboard, true, 'the clicked field has the keyboard');
  v.key('a', true, 1); // Ctrl+A
  settle(v, clock);
  v.key('a', false, 0);
  v.text('Moon');
  settle(v, clock);
  v.pointer('move', { x: field.x + 1, y: field.y });
  settle(v, clock);
  assertEq(v.state().hovered?.values.value, 'Moon', 'typed text');
  assert(v.state().uiRevision !== revision, 'the UI revision changes');
  assert(v.toYaml().includes('root/children/0/children/0: {value: "Moon"}'), v.toYaml());
  v.key('Escape', true, 0);
  settle(v, clock);
  v.key('Escape', false, 0);
  settle(v, clock);
  assertEq(v.state().wantsKeyboard, false, 'Escape leaves the field');
  v.destroy();
});

// ── Run ──────────────────────────────────────────────────────────────────────

let failed = 0;
for (const t of tests) {
  try {
    await t.fn();
    console.log(`ok   ${t.name}`);
  } catch (e) {
    ++failed;
    console.log(`FAIL ${t.name}\n     ${e.stack ?? e}`);
  }
}
console.log(`${tests.length - failed} of ${tests.length} passed`);
process.exit(failed === 0 ? 0 : 1);
