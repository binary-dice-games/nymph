// MIT License © 2026 Binary Dice Games
// spec/15_web_app plan steps 7 to 9: the app's pure functions in Node: URL
// rewrites, the view key ignored by "modified", share links' encoding.
//
//   node app.mjs <build>/web <nymph source dir>

import { join } from 'node:path';
import { pathToFileURL } from 'node:url';

const [webDir, sourceDir] = process.argv.slice(2);
const appDir = join(sourceDir, 'web', 'app', 'app');
const { rawUrl, sniff, isPng } = await import(pathToFileURL(join(appDir, 'open.js')).href);
const { withoutView, viewBlock, withView, Doc, baseName } = await import(pathToFileURL(join(appDir, 'document.js')).href);
const { encodeSource, decodeSource } = await import(pathToFileURL(join(appDir, 'share.js')).href);
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
