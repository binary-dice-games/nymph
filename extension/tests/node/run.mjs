// MIT License © 2026 Binary Dice Games
// spec/05_browser_extension plan steps 4-6: the extension's pure modules
// in Node (sniffer, cache, sizing, pool, URL routing, settings).
//
//   node run.mjs <nymph source dir>

import { readFile, readdir } from 'node:fs/promises';
import { join } from 'node:path';
import { pathToFileURL } from 'node:url';

const [sourceDir] = process.argv.slice(2);
if (!sourceDir) {
  console.error('usage: node run.mjs <nymph source dir>');
  process.exit(2);
}
const src = (path) => import(pathToFileURL(join(sourceDir, 'extension', 'src', path)).href);
const { PngSniffer, sniffStream, isNymphPng, MAX_HEADER_BYTES } = await src('content/sniff.js');
const { LruCache } = await src('content/cache.js');
const { hostStyle } = await src('content/sizing.js');
const { pickToPark, distanceToViewport } = await src('content/pool.js');
const { routeFor, fromBase64 } = await src('content/fetcher.js');
const { isOn, runnableOrigin } = await src('shared/settings.js');

// ── Test runner ──────────────────────────────────────────────────────────────

const tests = [];
const test = (name, fn) => tests.push({ name, fn });
const assert = (cond, message) => {
  if (!cond) {
    throw new Error(message);
  }
};
const assertEq = (actual, expected, what) =>
  assert(
    JSON.stringify(actual) === JSON.stringify(expected),
    `${what}: expected ${JSON.stringify(expected)}, got ${JSON.stringify(actual)}`,
  );

// ── PNG helpers ──────────────────────────────────────────────────────────────

/** The chunks of a PNG: {type, start, end} (end past the CRC). */
function chunks(bytes) {
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const out = [];
  for (let at = 8; at + 8 <= bytes.length; ) {
    const length = view.getUint32(at);
    const type = String.fromCharCode(...bytes.subarray(at + 4, at + 8));
    out.push({ type, start: at, end: at + 12 + length });
    at += 12 + length;
  }
  return out;
}

/** The PNG without its iTXt chunks: what a re-encoding service serves. */
function withoutText(bytes) {
  const keep = chunks(bytes).filter((c) => c.type !== 'iTXt');
  const parts = [bytes.subarray(0, 8), ...keep.map((c) => bytes.subarray(c.start, c.end))];
  return Buffer.concat(parts);
}

/** The PNG with its iTXt chunks moved after the image data. */
function textAfterIdat(bytes) {
  const all = chunks(bytes);
  const text = all.filter((c) => c.type === 'iTXt');
  const rest = all.filter((c) => c.type !== 'iTXt');
  const iend = rest.pop();
  const parts = [bytes.subarray(0, 8), ...rest.map((c) => bytes.subarray(c.start, c.end))];
  parts.push(...text.map((c) => bytes.subarray(c.start, c.end)), bytes.subarray(iend.start, iend.end));
  return Buffer.concat(parts);
}

/** A stream of `bytes` in pieces of `size`, counting the bytes pulled. */
function streamOf(bytes, size = 4096) {
  const counter = { pulled: 0, cancelled: false };
  let at = 0;
  const stream = new ReadableStream({
    pull(controller) {
      if (at >= bytes.length) {
        controller.close();
        return;
      }
      const piece = bytes.subarray(at, at + size);
      at += piece.length;
      counter.pulled += piece.length;
      controller.enqueue(new Uint8Array(piece));
    },
    cancel() {
      counter.cancelled = true;
    },
  }, { highWaterMark: 0 });
  return { stream, counter };
}

const exampleDir = join(sourceDir, 'examples');
const examplePngs = (await readdir(exampleDir)).filter((f) => f.endsWith('.png')).sort();
const read = async (name) => new Uint8Array(await readFile(join(exampleDir, name)));

// ── Sniffer ──────────────────────────────────────────────────────────────────

test('every example PNG is a nymph PNG and comes back whole', async () => {
  assert(examplePngs.length > 5, 'examples found');
  for (const name of examplePngs) {
    const bytes = await read(name);
    const { stream } = streamOf(bytes, 1000);
    const result = await sniffStream(stream);
    assert(result.nymph, `${name} is nymph`);
    assertEq(result.bytes.length, bytes.length, `${name} length`);
    assert(Buffer.compare(Buffer.from(result.bytes), Buffer.from(bytes)) === 0, `${name} bytes`);
    assert(isNymphPng(bytes), `${name} in memory`);
  }
});

test('a PNG without the chunk is not nymph, and its image data is never read', async () => {
  const plain = withoutText(await read('dashboard.png'));
  const firstIdat = chunks(plain).find((c) => c.type === 'IDAT').start;
  const { stream, counter } = streamOf(plain, 64);
  const result = await sniffStream(stream);
  assert(!result.nymph, 'not nymph');
  assertEq(result.reason, 'no nymph chunk before IDAT', 'reason');
  assert(counter.pulled <= firstIdat + 64, `read ${counter.pulled} bytes, IDAT at ${firstIdat}`);
  assert(counter.cancelled, 'stream cancelled');
});

test('a nymph chunk after the image data is not looked for', async () => {
  const moved = textAfterIdat(await read('line.png'));
  assert(!isNymphPng(moved), 'not nymph');
});

test('non-PNG bytes are rejected at once', async () => {
  const { stream, counter } = streamOf(new TextEncoder().encode('<html>not an image</html>'.repeat(100)), 16);
  const result = await sniffStream(stream);
  assert(!result.nymph, 'not nymph');
  assertEq(result.reason, 'not a PNG', 'reason');
  assertEq(counter.pulled, 16, 'one piece read');
});

test('another iTXt keyword is not nymph', async () => {
  const bytes = Buffer.from(await read('line.png'));
  const text = chunks(bytes).find((c) => c.type === 'iTXt');
  bytes[text.start + 8] = 'N'.charCodeAt(0); // "Nymph"
  assert(!isNymphPng(bytes), 'not nymph');
});

test('truncated and malformed streams are not nymph', async () => {
  const bytes = await read('line.png');
  assert(!(await sniffStream(streamOf(bytes.subarray(0, 20)).stream)).nymph, 'cut in IHDR');
  const huge = Buffer.from(bytes);
  huge.writeUInt32BE(0xffffffff, 8); // IHDR length
  assert(!isNymphPng(huge), 'bad length');
  const sniffer = new PngSniffer();
  assertEq(sniffer.push(bytes.subarray(0, 10)), 'more', 'undecided');
  assertEq(sniffer.end(), 'no', 'ended');
});

test('no IDAT within the header limit is not nymph', async () => {
  const sniffer = new PngSniffer({ maxHeaderBytes: 40 });
  const plain = withoutText(await read('line.png'));
  // IHDR ends at 33; the next chunk header is past 40 only if it is long.
  const padded = Buffer.concat([plain.subarray(0, 33), Buffer.from([0, 0, 0, 100, 0x74, 0x45, 0x58, 0x74]), Buffer.alloc(112)]);
  assertEq(sniffer.push(padded), 'no', 'verdict');
  assertEq(MAX_HEADER_BYTES, 1024 * 1024, 'default limit');
});

test('a nymph PNG above the size cap is refused', async () => {
  const bytes = await read('line.png');
  const result = await sniffStream(streamOf(bytes).stream, { maxBytes: 1000 });
  assert(!result.nymph, 'refused');
  assertEq(result.reason, 'too large', 'reason');
});

// ── Cache ────────────────────────────────────────────────────────────────────

test('the cache drops the least recently used entry', () => {
  const cache = new LruCache(2);
  cache.set('a', 1);
  cache.set('b', 2);
  cache.get('a');
  cache.set('c', 3);
  assertEq(cache.get('b'), undefined, 'b dropped');
  assertEq(cache.get('a'), 1, 'a kept');
  assertEq(cache.get('c'), 3, 'c kept');
  assertEq(cache.size, 2, 'size');
});

// ── Sizing ───────────────────────────────────────────────────────────────────

const computed = (more = {}) => ({ display: 'inline', maxWidth: '100%', maxHeight: 'none', verticalAlign: 'baseline', ...more });
const image = (more = {}) => ({
  attrWidth: null, attrHeight: null, inlineWidth: '', inlineHeight: '', naturalWidth: 800, naturalHeight: 600,
  computed: computed(), ...more,
});

test('an image without width rules gets its natural width and aspect ratio', () => {
  const s = hostStyle(image());
  assertEq([s.display, s.width, s.height, s.aspectRatio, s.maxWidth], ['inline-block', '800px', 'auto', '800 / 600', '100%'], 'style');
});

test('width and height attributes and inline styles are kept', () => {
  assertEq(hostStyle(image({ attrWidth: '300' })).width, '300px', 'attribute in px');
  assertEq(hostStyle(image({ attrWidth: '50%' })).width, '50%', 'attribute in %');
  assertEq(hostStyle(image({ attrWidth: 'oops' })).width, '800px', 'bad attribute ignored');
  assertEq(hostStyle(image({ inlineWidth: '20em', attrWidth: '300' })).width, '20em', 'inline style first');
  const tall = hostStyle(image({ attrHeight: '150' }));
  assertEq([tall.width, tall.height], ['auto', '150px'], 'height only');
  const both = hostStyle(image({ attrWidth: '300', attrHeight: '100' }));
  assertEq([both.width, both.height], ['300px', '100px'], 'both');
});

test('block images stay blocks and spacing is copied', () => {
  const s = hostStyle(image({ computed: computed({ display: 'block', marginTop: '8px', cssFloat: 'right' }) }));
  assertEq([s.display, s.marginTop, s.cssFloat], ['block', '8px', 'right'], 'style');
});

// ── Pool ─────────────────────────────────────────────────────────────────────

test('the farthest viewer outside the margin and not hovered is parked', () => {
  const entries = [
    { key: 'near', near: true, hovered: false, distance: 0 },
    { key: 'far', near: false, hovered: false, distance: 5000 },
    { key: 'farther-hovered', near: false, hovered: true, distance: 9000 },
    { key: 'mid', near: false, hovered: false, distance: 2000 },
  ];
  assertEq(pickToPark(entries), 'far', 'parked');
  assertEq(pickToPark(entries.filter((e) => e.key.startsWith('near'))), null, 'none parkable');
  assertEq(distanceToViewport({ top: -500, bottom: -100 }, 800), 100, 'above');
  assertEq(distanceToViewport({ top: 1000, bottom: 1300 }, 800), 200, 'below');
  assertEq(distanceToViewport({ top: 100, bottom: 300 }, 800), 0, 'inside');
});

// ── Fetch routes and settings ────────────────────────────────────────────────

test('URLs are read here, by the worker or not at all', () => {
  const page = 'https://github.com';
  assertEq(routeFor('https://github.com/o/r/raw/main/a.png', page), 'here', 'same origin');
  assertEq(routeFor('https://camo.githubusercontent.com/abc', page), 'worker', 'cross origin');
  assertEq(routeFor('data:image/png;base64,iVBOR', page), 'here', 'data');
  assertEq(routeFor('blob:https://github.com/1234', page), 'here', 'blob');
  assertEq(routeFor('chrome://favicon/x', page), 'never', 'browser URL');
  assertEq(routeFor('not a url', page), 'never', 'garbage');
  assertEq(Array.from(fromBase64(Buffer.from([1, 2, 255]).toString('base64'))), [1, 2, 255], 'base64');
});

test('sites are on unless turned off, and browser pages are never runnable', () => {
  assert(isOn({ off: [] }, 'https://github.com'), 'on by default');
  assert(!isOn({ off: ['https://github.com'] }, 'https://github.com'), 'off');
  assertEq(runnableOrigin('https://docs.google.com/document/d/1'), 'https://docs.google.com', 'https');
  assertEq(runnableOrigin('file:///home/me/report.html'), 'file://', 'file');
  assertEq(runnableOrigin('chrome://extensions'), null, 'chrome');
  assertEq(runnableOrigin('https://chromewebstore.google.com/detail/x'), null, 'store');
  assertEq(runnableOrigin(undefined), null, 'no URL');
});

// ── Run ──────────────────────────────────────────────────────────────────────

let failed = 0;
for (const { name, fn } of tests) {
  try {
    await fn();
    console.log(`ok   ${name}`);
  } catch (e) {
    failed++;
    console.log(`FAIL ${name}\n     ${e.stack ?? e}`);
  }
}
console.log(`${tests.length - failed} of ${tests.length} passed`);
process.exit(failed ? 1 : 0);
