// MIT License © 2026 Binary Dice Games
// spec/05_browser_extension plan steps 5-8: the extension in Chromium,
// through Playwright, on local fixture pages (WebGL2 through SwiftShader
// when there is no GPU).
//
//   node run.mjs <build>/extension/nymph <nymph source dir> [test name filter]
//
// Pages are served on two origins (127.0.0.1 and localhost, one port
// from the OS): the second is "another site" for toggling and for
// cross-origin images. Pages with ?csp=github get a GitHub-like Content
// Security Policy, which blocks WebAssembly in content scripts, so the
// extension runs its viewers in frames there.
//
// Exits 77 (CTest's skip) when Playwright is not installed.

import { execSync } from 'node:child_process';
import { mkdtemp, readFile, readdir, rm } from 'node:fs/promises';
import { createServer } from 'node:http';
import { createRequire } from 'node:module';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { crc32, inflateSync } from 'node:zlib';

const [extDir, sourceDir, filter] = process.argv.slice(2);
if (!extDir || !sourceDir) {
  console.error('usage: node run.mjs <build>/extension/nymph <nymph source dir> [filter]');
  process.exit(2);
}

/** Playwright from the project, or the global install. */
async function loadPlaywright() {
  try {
    return await import('playwright');
  } catch {
    try {
      const root = execSync('npm root -g', { encoding: 'utf8' }).trim();
      return createRequire(join(root, 'noop.js'))('playwright');
    } catch {
      return null;
    }
  }
}

const playwright = await loadPlaywright();
if (!playwright) {
  console.log('Playwright is not installed: skipped');
  process.exit(77);
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
  assert(
    JSON.stringify(actual) === JSON.stringify(expected),
    `${what}: expected ${JSON.stringify(expected)}, got ${JSON.stringify(actual)}`,
  );

// ── PNG bytes ────────────────────────────────────────────────────────────────

const exampleDir = join(sourceDir, 'examples');
const examples = new Map();
for (const f of (await readdir(exampleDir)).filter((f) => f.endsWith('.png'))) {
  examples.set(f, await readFile(join(exampleDir, f)));
}

function chunks(bytes) {
  const out = [];
  for (let at = 8; at + 8 <= bytes.length; ) {
    const length = bytes.readUInt32BE(at);
    out.push({ type: bytes.toString('latin1', at + 4, at + 8), start: at, end: at + 12 + length });
    at += 12 + length;
  }
  return out;
}

function chunk(type, data) {
  const head = Buffer.alloc(8);
  head.writeUInt32BE(data.length, 0);
  head.write(type, 4, 'latin1');
  const crc = Buffer.alloc(4);
  crc.writeUInt32BE(crc32(Buffer.concat([Buffer.from(type, 'latin1'), data])) >>> 0, 0);
  return Buffer.concat([head, data, crc]);
}

/** The PNG with its iTXt chunks replaced by `text` (null: removed). */
function withText(bytes, text) {
  const parts = [bytes.subarray(0, 8)];
  for (const c of chunks(bytes)) {
    if (c.type === 'iTXt') {
      continue;
    }
    parts.push(bytes.subarray(c.start, c.end));
    if (c.type === 'IHDR' && text !== null) {
      parts.push(chunk('iTXt', Buffer.concat([Buffer.from('nymph\0\0\0\0\0', 'latin1'), Buffer.from(text)])));
    }
  }
  return Buffer.concat(parts);
}

/** The PNG with one byte of its nymph chunk flipped (a bad CRC). */
function corrupt(bytes) {
  const copy = Buffer.from(bytes);
  const text = chunks(copy).find((c) => c.type === 'iTXt');
  copy[text.start + 20] ^= 0xff;
  return copy;
}

function decodePng(bytes) {
  let width = 0, height = 0, channels = 0;
  const idat = [];
  for (const c of chunks(bytes)) {
    const data = bytes.subarray(c.start + 8, c.end - 4);
    if (c.type === 'IHDR') {
      width = data.readUInt32BE(0);
      height = data.readUInt32BE(4);
      channels = { 2: 3, 6: 4 }[data[9]];
      assert(data[8] === 8 && data[12] === 0 && channels, 'only 8-bit RGB(A), non-interlaced PNGs');
    } else if (c.type === 'IDAT') {
      idat.push(data);
    }
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

/**
 * Two screenshots look alike: the browser tolerance of
 * spec/04_web_viewer/PLAN.md step 10 (at least 99.5 % of pixels within 16,
 * at most 0.5 % beyond 64, mean below 0.5), on the colour channels.
 */
function looksAlike(a, b, what) {
  assertEq([a.width, a.height], [b.width, b.height], `${what} size`);
  let within = 0, beyond = 0, sum = 0;
  const pixels = a.width * a.height;
  for (let i = 0; i < pixels; i++) {
    let d = 0;
    for (let k = 0; k < 3; k++) {
      d = Math.max(d, Math.abs(a.rgba[i * 4 + k] - b.rgba[i * 4 + k]));
    }
    within += d <= 16 ? 1 : 0;
    beyond += d > 64 ? 1 : 0;
    sum += d;
  }
  const stats = `within ${(within / pixels * 100).toFixed(2)} %, beyond ${(beyond / pixels * 100).toFixed(2)} %, mean ${(sum / pixels).toFixed(3)}`;
  assert(within / pixels >= 0.995 && beyond / pixels <= 0.005 && sum / pixels < 0.5, `${what}: ${stats}`);
}

/** Mean luminance of a decoded image. */
function luminance(img) {
  let sum = 0;
  for (let i = 0; i < img.width * img.height; i++) {
    sum += 0.2126 * img.rgba[i * 4] + 0.7152 * img.rgba[i * 4 + 1] + 0.0722 * img.rgba[i * 4 + 2];
  }
  return sum / (img.width * img.height);
}

// ── Fixture pages ────────────────────────────────────────────────────────────

const GITHUB_CSP =
  "default-src 'none'; base-uri 'self'; connect-src 'self'; frame-src 'self'; " +
  "img-src 'self' data: blob: http://localhost:*; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'";

const page = (body, script = '') => `<!doctype html><html><head><meta charset="utf-8">
<style>body { margin: 8px; font: 14px sans-serif; } .narrow { width: 400px; } .md img { max-width: 100%; }</style>
</head><body>${body}<script>${script}</script></body></html>`;

// Sources for the chat-mode fixtures (spec/10_gemini_extension).
const lineYaml = await readFile(join(exampleDir, 'line.yaml'), 'utf8');

const FIXTURES = {
  // A chat answer: a nymph block, plain YAML and Python in <pre><code>.
  chat: () => page(`<div id="msgs"></div>`, `
    const yaml = ${JSON.stringify(lineYaml)};
    const pre = (id, lang, text) => '<pre id="' + id + '"><code class="language-' + lang + '">' + text + '</code></pre>';
    window.render = () => {
      msgs.innerHTML = '<p>Here you go</p>' + pre('chart', 'nymph', '') + pre('compose', 'yaml', '') + pre('py', 'python', 'print(1)');
      chart.firstChild.textContent = yaml;
      compose.firstChild.textContent = 'services:\\n  web:\\n    image: nginx\\n';
    };
    render();
    window.revise = () => { chart.firstChild.textContent = yaml.replace('Load test', 'Revised title'); };`),
  // A chat with a prompt box that already holds text, and a send button.
  chatbox: () => page(`<div id="msgs"></div><div id="box" contenteditable="true" style="border: 1px solid #888; min-height: 40px">first</div>
    <button id="send" type="button">Send</button>`, `window.sent = 0; send.onclick = () => window.sent++;`),
  // Blocks that start like a nymph source but the viewer rejects, and one it does not claim.
  chatbad: () => page(`<div id="msgs"></div>`, `
    const block = (id, label, text) => '<pre id="' + id + '"><code' + (label ? ' class="language-' + label + '"' : '') + '>' + text + '</code></pre>';
    msgs.innerHTML =
      block('unknown', 'nymph', 'root:\\n  type: NoSuchElement\\n') +
      block('broken', 'nymph', 'nymph: 1\\nroot: [broken: ') +
      block('months', '', 'nymph: 1\\nimage: {width: 400, height: 300}\\nroot:\\n  type: Plot\\n  children:\\n    - {type: PlotBars, xs: $month, ys: $revenue}\\ndata: |\\n  month, revenue\\n  Jan, 35\\n  Feb, 42\\n') +
      block('other', 'yaml', 'description: not claimed\\nroot:\\n  type: NoSuchElement\\n');`),
  // An invalid nymph block, with a prompt box and a send button.
  chatfix: () => page(`<div id="msgs"></div><div id="box" contenteditable="true" style="border: 1px solid #888; min-height: 40px"></div>
    <button id="send" type="button">Send</button>`, `
    window.sent = 0; send.onclick = () => window.sent++;
    msgs.innerHTML = '<pre id="bad"><code class="language-nymph">nymph: 1\\nroot:\\n  type: NoSuchElement\\n</code></pre>';`),
  // An answer written token by token, a line every 40 ms.
  chatstream: () => page(`<div id="msgs"><pre id="chart"><code class="language-nymph" id="c"></code></pre></div>`, `
    const lines = ${JSON.stringify(lineYaml)}.split('\\n');
    let i = 0;
    window.finished = false;
    const timer = setInterval(() => {
      c.textContent += lines[i++] + '\\n';
      if (i >= lines.length) { clearInterval(timer); window.finished = true; }
    }, 40);`),
  // Nymph and plain PNGs as markdown renders them, in links.
  markdown: (q) => page(`<div class="md">
    <p>Intro</p>
    <p><a id="link" href="/img/line.png"><img id="line" src="/img/line.png" alt="Line chart"></a></p>
    <p><img id="plain" src="/plain/bars.png" alt="plain"></p>
    <p>Text <img id="small" src="/img/bars.png" width="300" alt="bars"> after</p>
    <div class="narrow"><img id="scaled" src="/img/line.png?scaled" alt="scaled"></div>
    <p><img id="tiny" src="/img/line.png?tiny" width="16" height="16"></p>
  </div>`),
  // srcset, picture, lazy loading, a 2x image (shown at its natural size).
  responsive: () => page(`
    <img id="srcset" srcset="/img/line.png?a 1x, /img/line.png?b 2x" alt="srcset">
    <picture><source srcset="/img/bars.png?pic" media="(min-width: 1px)"><img id="picture" src="/plain/bars.png" alt="picture"></picture>
    <div style="height: 2000px"></div>
    <img id="lazy" loading="lazy" src="/img/heatmap.png?lazy" alt="lazy">`),
  // A feed that appends a chart whenever the bottom comes into view.
  feed: () => page(`<div id="feed"></div><div id="more" style="height: 10px"></div>`, `
    let n = 0;
    window.feedOpen = true;
    const add = () => { if (!window.feedOpen) return; for (let i = 0; i < 3; i++) { const p = document.createElement('p');
      p.innerHTML = '<img class="item" src="/img/line.png?feed' + (n++) + '">'; feed.appendChild(p); } };
    add();
    new IntersectionObserver((r) => r[0].isIntersecting && add()).observe(more);`),
  // A framework that re-renders: replaces image nodes and resets attributes.
  framework: () => page(`<div id="app"></div>`, `
    let renders = 0;
    const render = () => { app.innerHTML = '<p>render ' + (renders++) + '</p><img id="chart" src="/img/line.png">'; };
    render();
    window.rerender = render;
    window.reattr = () => document.getElementById('chart').removeAttribute('data-nymph-ext');`),
  // Images whose source changes, and that are removed.
  changes: () => page(`<img id="a" src="/img/line.png"> <img id="b" src="/img/bars.png">`),
  // Many charts, to fill the pool.
  many: () => page(Array.from({ length: 48 }, (_, i) =>
    `<p style="margin: 0 0 400px">${i}<br><img class="m" src="/img/line.png?m${i}" width="400"></p>`).join('')),
  // Broken nymph PNGs.
  broken: () => page(`<img id="corrupt" src="/bad/corrupt.png"> <img id="notyaml" src="/bad/notyaml.png">
    <img id="good" src="/img/line.png">`),
  // Only plain images.
  plain: () => page(`<img src="/plain/line.png"> <img src="/plain/bars.png">`),
  // Images centred in fixed-size panes, as image viewers and previews do.
  centred: () => page(['flex', 'grid'].map((kind) => `
    <div class="pane ${kind}"><img id="${kind}" src="/img/line.png?${kind}" alt="${kind}"></div>`).join('') +
    // claude.ai's file preview: size attributes a stylesheet overrides.
    `<div class="pane flex"><img id="override" class="auto" width="0" height="0" src="/img/line.png?override"></div>`, '')
    .replace('</style>', `.pane { position: relative; width: 900px; height: 700px; margin-bottom: 8px; }
      .pane img { max-width: 100%; max-height: 100%; }
      .flex { display: flex; align-items: center; justify-content: center; }
      .grid { display: grid; place-items: center; }
      .pane img.auto { width: auto; height: auto; object-fit: contain; }</style>`),
  // An image from the other origin, which sends no CORS headers.
  cross: (q, port) => page(`<img id="cross" src="http://localhost:${port}/img/line.png?cross">`),
};

/** Every request the server saw: {url, host, dest, cookie}. */
const requests = [];

const server = createServer(async (req, res) => {
  const url = new URL(req.url, `http://${req.headers.host}`);
  const [, kind, name] = url.pathname.split('/');
  requests.push({ path: url.pathname + url.search, host: req.headers.host, dest: req.headers['sec-fetch-dest'], cookie: req.headers.cookie });
  const png = (bytes) => {
    res.writeHead(200, { 'content-type': 'image/png', 'cache-control': 'max-age=3600' });
    res.end(bytes);
  };
  if (kind === 'img' && examples.has(name)) {
    return png(examples.get(name));
  }
  if (kind === 'plain' && examples.has(name)) {
    return png(withText(examples.get(name), null));
  }
  if (kind === 'bad' && name === 'corrupt.png') {
    return png(corrupt(examples.get('line.png')));
  }
  if (kind === 'bad' && name === 'notyaml.png') {
    return png(withText(examples.get('line.png'), 'root: [this is: not, a nymph source'));
  }
  if (kind === 'page' && FIXTURES[name]) {
    const headers = { 'content-type': 'text/html; charset=utf-8' };
    if (url.searchParams.get('csp') === 'github') {
      headers['content-security-policy'] = GITHUB_CSP;
    }
    res.writeHead(200, headers);
    return res.end(FIXTURES[name](url.searchParams, server.address().port));
  }
  res.writeHead(404);
  res.end();
});
await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
const port = server.address().port;
// localhost must reach the same server.
const other = createServer((req, res) => server.emit('request', req, res));
await new Promise((resolve) => other.listen(port, 'localhost', resolve).on('error', resolve));
const SITE = `http://127.0.0.1:${port}`;
const OTHER = `http://localhost:${port}`;

// ── Browser with the extension ───────────────────────────────────────────────

const profile = await mkdtemp(join(tmpdir(), 'nymph-ext-'));
const context = await playwright.chromium.launchPersistentContext(profile, {
  channel: 'chromium',
  headless: true,
  viewport: { width: 1000, height: 800 },
  deviceScaleFactor: 1,
  args: [
    `--disable-extensions-except=${extDir}`,
    `--load-extension=${extDir}`,
    '--enable-unsafe-swiftshader',
    '--use-angle=swiftshader',
  ],
});
const worker = context.serviceWorkers()[0] ?? (await context.waitForEvent('serviceworker'));
// The welcome tab opened on install.
await new Promise((resolve) => setTimeout(resolve, 500));
for (const p of context.pages()) {
  if (p.url().startsWith('chrome-extension://')) {
    await p.close();
  }
}

/** Sets the extension's settings as the options page would. */
const setSettings = (values) => worker.evaluate((v) => chrome.storage.sync.set(v), values);

/** Clicks the toolbar button for the tab showing `url`. @return whether the site is now on */
const clickButton = (url) =>
  worker.evaluate(async (u) => {
    const [tab] = (await chrome.tabs.query({})).filter((t) => t.url === u);
    return globalThis.nymphToggleTab(tab);
  }, url);

/** The button's state for the tab showing `url` (null: a tab whose URL the extension cannot see). */
const buttonState = (url) =>
  worker.evaluate(async (u) => {
    const [tab] = (await chrome.tabs.query({})).filter((t) => (u === null ? !t.url : t.url === u));
    return {
      badge: await chrome.action.getBadgeText({ tabId: tab.id }),
      title: await chrome.action.getTitle({ tabId: tab.id }),
      enabled: await chrome.action.isEnabled(tab.id),
    };
  }, url);

/** Polls `fn` until it returns a truthy value. */
async function waitFor(fn, what, timeout = 20000) {
  const end = Date.now() + timeout;
  for (;;) {
    const value = await fn();
    if (value) {
      return value;
    }
    if (Date.now() > end) {
      throw new Error(`timed out waiting for ${what}`);
    }
    await new Promise((resolve) => setTimeout(resolve, 100));
  }
}

/** The page's charts: images hidden by the extension, with their hosts' boxes. */
const charts = (p) =>
  p.evaluate(() =>
    [...document.querySelectorAll('img[data-nymph-ext="hidden"]')].map((img) => {
      const host = img.nextElementSibling;
      const r = host?.localName === 'nymph-ext-chart' ? host.getBoundingClientRect() : null;
      const box = r && [r.x + scrollX, r.y + scrollY, r.width, r.height].map(Math.round);
      return { id: img.id || img.className, box, live: !host.classList.contains('nymph-ext-starting') };
    }).filter((c) => c.live),
  );

const hostCount = (p) => p.evaluate(() => document.querySelectorAll('nymph-ext-chart').length);

/** Boxes of the page's images by id, in page coordinates (with the extension off). */
const imageBoxes = (p) =>
  p.evaluate(() =>
    Object.fromEntries([...document.images].filter((i) => i.id).map((i) => {
      const r = i.getBoundingClientRect();
      return [i.id, [r.x + scrollX, r.y + scrollY, r.width, r.height].map(Math.round)];
    })),
  );

/**
 * Scrolls through the page, so every chart is on screen once: in frame
 * mode Chrome runs a frame only while it is on screen.
 */
async function visitAll(p) {
  const height = await p.evaluate(() => document.documentElement.scrollHeight);
  for (let y = 0; y < height; y += 300) {
    await p.evaluate((top) => scrollTo(0, top), y);
    await new Promise((resolve) => setTimeout(resolve, 150));
  }
  await p.evaluate(() => scrollTo(0, 0));
}

async function openPage(name, query = '', origin = SITE) {
  const p = await context.newPage();
  await p.goto(`${origin}/page/${name}${query}`);
  return p;
}

/** Screenshot of a box in page coordinates, scrolled into view first, decoded. */
async function shot(p, box) {
  await p.evaluate((y) => scrollTo(0, Math.max(0, y - 20)), box[1]);
  await new Promise((resolve) => setTimeout(resolve, 200));
  const scroll = await p.evaluate(() => scrollY);
  const clip = { x: box[0], y: box[1] - scroll, width: box[2], height: box[3] };
  return decodePng(await p.screenshot({ clip }));
}

/** Viewport coordinates of a page box's centre (scrolled into view). */
async function centreOf(p, box) {
  await p.evaluate((y) => scrollTo(0, Math.max(0, y - 20)), box[1]);
  const scroll = await p.evaluate(() => scrollY);
  return [box[0] + box[2] / 2, box[1] - scroll + box[3] / 2];
}

/** All images loaded and decoded. */
const imagesLoaded = (p) => p.evaluate(() => Promise.all([...document.images].map((i) => i.decode().catch(() => {}))));

// ── Tests ────────────────────────────────────────────────────────────────────

for (const mode of ['inline', 'frame']) {
  const csp = mode === 'frame' ? '?csp=github' : '';

  test(`${mode}: only nymph PNGs become charts, in the image's box, looking like it`, async () => {
    await setSettings({ off: [SITE] });
    const p = await openPage('markdown', csp);
    await imagesLoaded(p);
    const before = await imageBoxes(p);
    const pictures = { line: await shot(p, before.line), small: await shot(p, before.small) };
    await setSettings({ off: [] }); // the button, without a reload
    await waitFor(async () => (await visitAll(p), (await charts(p)).length === 3), 'three charts');
    const after = Object.fromEntries((await charts(p)).map((c) => [c.id, c.box]));
    assertEq(Object.keys(after).sort(), ['line', 'scaled', 'small'], 'replaced');
    for (const id of Object.keys(after)) {
      assertEq(after[id], before[id], `${id} box`);
    }
    looksAlike(await shot(p, after.line), pictures.line, 'natural size');
    // The plain PNG and the icon-sized image stay images.
    assert(await p.evaluate(() => !document.getElementById('plain').hasAttribute('data-nymph-ext')), 'plain kept');
    await p.close();
  });

  test(`${mode}: using a chart inside a link does not follow it`, async () => {
    const p = await openPage('markdown', csp);
    await waitFor(async () => (await charts(p)).some((c) => c.id === 'line'), 'chart');
    const [{ box }] = (await charts(p)).filter((c) => c.id === 'line');
    const [cx, cy] = await centreOf(p, box);
    await p.mouse.move(cx, cy);
    await p.mouse.down();
    await p.mouse.move(cx - 60, cy, { steps: 5 });
    await p.mouse.up();
    await p.mouse.click(cx, cy);
    await new Promise((resolve) => setTimeout(resolve, 500));
    assert(p.url().includes('/page/markdown'), `still on the page: ${p.url()}`);
    await p.close();
  });

  test(`${mode}: srcset, picture and lazy images are replaced`, async () => {
    const p = await openPage('responsive', csp);
    await waitFor(async () => (await charts(p)).length === 2, 'srcset and picture');
    await p.evaluate(() => document.getElementById('lazy').scrollIntoView());
    await waitFor(async () => (await charts(p)).length === 3, 'lazy image');
    await p.close();
  });

  test(`${mode}: charts follow a feed that loads more as it scrolls`, async () => {
    const p = await openPage('feed', csp);
    await waitFor(async () => (await charts(p)).length >= 1, 'first item');
    for (let i = 0; i < 3; i++) {
      await p.evaluate(() => scrollTo(0, document.body.scrollHeight));
      await new Promise((resolve) => setTimeout(resolve, 300));
    }
    await p.evaluate(() => (window.feedOpen = false));
    const items = await p.evaluate(() => document.querySelectorAll('img.item').length);
    assert(items >= 9, `feed grew to ${items}`);
    await p.evaluate(() => [...document.querySelectorAll('img.item')].pop().scrollIntoView());
    await waitFor(async () => {
      const last = await p.evaluate(() => [...document.querySelectorAll('img.item')].pop().getAttribute('data-nymph-ext'));
      const hosts = await p.evaluate(() => document.querySelectorAll('nymph-ext-chart:not(.nymph-ext-starting)').length);
      return last === 'hidden' && hosts >= 2;
    }, 'the newest item replaced', 20000);
    await p.close();
  });
}

test('a chart centred in a pane stays where its image was', async () => {
  await setSettings({ off: [SITE] });
  const p = await openPage('centred');
  await imagesLoaded(p);
  const before = await imageBoxes(p);
  await setSettings({ off: [] });
  await waitFor(async () => (await visitAll(p), (await charts(p)).length === 3), 'three charts');
  const after = Object.fromEntries((await charts(p)).map((c) => [c.id, c.box]));
  for (const id of Object.keys(before)) {
    assertEq(after[id], before[id], `${id} box`);
  }
  await p.close();
});

test('a chart reflows with the window as its image did', async () => {
  await setSettings({ off: [SITE] });
  const p = await openPage('markdown');
  await p.setViewportSize({ width: 600, height: 800 });
  await imagesLoaded(p);
  const narrow = await imageBoxes(p);
  await p.setViewportSize({ width: 1000, height: 800 });
  await setSettings({ off: [] });
  await waitFor(async () => (await charts(p)).some((c) => c.id === 'line'), 'chart');
  await p.setViewportSize({ width: 600, height: 800 });
  await new Promise((resolve) => setTimeout(resolve, 300));
  const line = (await charts(p)).find((c) => c.id === 'line');
  assertEq(line.box, narrow.line, 'box at 600 px');
  await p.close();
});

test('the options page turns a site back on', async () => {
  await setSettings({ off: [SITE] });
  const p = await context.newPage();
  await p.goto(`chrome-extension://${new URL(worker.url()).host}/src/options/options.html`);
  await p.getByRole('button', { name: 'Turn on' }).click();
  await waitFor(() => worker.evaluate(async () => (await chrome.storage.sync.get('off')).off.length === 0), 'site on');
  await p.close();
});

test('a re-rendering framework keeps one chart per image', async () => {
  const p = await openPage('framework');
  await waitFor(async () => (await charts(p)).length === 1, 'chart');
  for (let i = 0; i < 5; i++) {
    await p.evaluate(() => window.rerender());
    await new Promise((resolve) => setTimeout(resolve, 100));
  }
  await waitFor(async () => (await charts(p)).length === 1 && (await hostCount(p)) === 1, 'one chart after re-renders');
  // The page strips the extension's attribute: the image is hidden again.
  await p.evaluate(() => window.reattr());
  await waitFor(() => p.evaluate(() => document.getElementById('chart').getAttribute('data-nymph-ext') === 'hidden'), 'hidden again');
  assertEq(await hostCount(p), 1, 'hosts');
  await p.close();
});

test('an image whose source changes or that is removed takes its chart along', async () => {
  const p = await openPage('changes');
  await waitFor(async () => (await charts(p)).length === 2, 'two charts');
  await p.evaluate(() => (document.getElementById('a').src = '/plain/line.png'));
  await waitFor(async () => (await charts(p)).length === 1 && (await hostCount(p)) === 1, 'a back to an image');
  await p.evaluate(() => (document.getElementById('a').src = '/img/heatmap.png'));
  await waitFor(async () => (await charts(p)).length === 2, 'a is a new chart');
  await p.evaluate(() => document.getElementById('b').remove());
  await waitFor(async () => (await hostCount(p)) === 1, "b's chart removed");
  await p.close();
});

for (const [mode, csp, cap] of [['inline', '', 32], ['frame', '?csp=github', 12]]) {
test(`${mode}: at most ${cap} viewers live, and a parked chart comes back with its zoom`, async () => {
  const p = await openPage('many', csp);
  await waitFor(async () => (await charts(p)).length >= 1, 'first chart');
  const first = (await charts(p))[0].box;
  // Zoom the first chart.
  const [fx, fy] = await centreOf(p, first);
  await p.mouse.move(fx, fy);
  for (let i = 0; i < 3; i++) {
    await p.mouse.wheel(0, -120);
    await new Promise((resolve) => setTimeout(resolve, 100));
  }
  await new Promise((resolve) => setTimeout(resolve, 500));
  await p.mouse.move(5, 5);
  await new Promise((resolve) => setTimeout(resolve, 300));
  const zoomed = await shot(p, first);
  let most = 0;
  const height = await p.evaluate(() => document.body.scrollHeight);
  for (let y = 0; y < height; y += 600) {
    await p.evaluate((top) => scrollTo(0, top), y);
    await new Promise((resolve) => setTimeout(resolve, 250));
    most = Math.max(most, await hostCount(p));
  }
  await new Promise((resolve) => setTimeout(resolve, 1000));
  most = Math.max(most, await hostCount(p));
  assert(most <= cap, `${most} viewers live`);
  assert(most >= cap - 4, `the pool was used: ${most}`);
  const parked = await p.evaluate(() => !document.querySelector('img.m').hasAttribute('data-nymph-ext'));
  assert(parked, 'the first chart was parked');
  await p.evaluate(() => scrollTo(0, 0));
  await waitFor(() => p.evaluate(() => document.querySelector('img.m').getAttribute('data-nymph-ext') === 'hidden'), 'revived');
  await new Promise((resolve) => setTimeout(resolve, 300));
  looksAlike(await shot(p, first), zoomed, 'zoom kept');
  await p.close();
});
}

test('corrupt and non-nymph sources stay images', async () => {
  const p = await openPage('broken');
  await waitFor(async () => (await charts(p)).length === 1, 'the good chart');
  await new Promise((resolve) => setTimeout(resolve, 1000));
  assertEq((await charts(p)).map((c) => c.id), ['good'], 'replaced');
  assertEq(await hostCount(p), 1, 'no leftover hosts');
  await p.close();
});

test('a cross-origin image without CORS is read without cookies', async () => {
  await context.addCookies([{ name: 'secret', value: 'x', domain: 'localhost', path: '/' }]);
  const p = await openPage('cross');
  await waitFor(async () => (await charts(p)).length === 1, 'chart');
  const reads = requests.filter((r) => r.path.includes('?cross') && r.dest === 'empty');
  assert(reads.length >= 1, 'read by the extension');
  assert(reads.every((r) => !r.cookie), `no cookie: ${JSON.stringify(reads)}`);
  await p.close();
});

test('a page without nymph PNGs loads no WebAssembly', async () => {
  const p = await openPage('plain');
  await imagesLoaded(p);
  await new Promise((resolve) => setTimeout(resolve, 1500));
  // The module would add its shared WebGL canvas and the viewer style; the
  // content script's import of the viewer is visible as a resource entry.
  const loaded = await p.evaluate(() => performance.getEntriesByType('resource').some((e) => e.name.includes('nymph-viewer')));
  assert(!loaded, 'viewer not loaded');
  assertEq(await hostCount(p), 0, 'no hosts');
  await p.close();
});

test('the toolbar button turns a site off and on, in every tab, and is remembered', async () => {
  await setSettings({ off: [] });
  const a = await openPage('markdown');
  const b = await openPage('changes');
  const c = await openPage('changes', '', OTHER);
  await waitFor(async () => (await charts(a)).length === 3 && (await charts(b)).length === 2 && (await charts(c)).length === 2, 'charts');
  const urlA = a.url();
  await waitFor(async () => (await buttonState(urlA)).badge === '3', 'badge 3');
  assert((await buttonState(urlA)).title.includes('on for 127.0.0.1'), 'title on');

  assertEq(await clickButton(urlA), false, 'now off');
  await waitFor(async () => (await hostCount(a)) === 0 && (await hostCount(b)) === 0, 'charts removed in both tabs');
  assert(await a.evaluate(() => !document.querySelector('[data-nymph-ext]') && !document.getElementById('nymph-ext-style')), 'page restored');
  assertEq((await charts(c)).length, 2, 'the other site stays on');
  await waitFor(async () => (await buttonState(urlA)).badge === '', 'badge cleared');
  assert((await buttonState(urlA)).title.includes('off for 127.0.0.1'), 'title off');

  await a.reload();
  await imagesLoaded(a);
  await new Promise((resolve) => setTimeout(resolve, 1000));
  assertEq(await hostCount(a), 0, 'still off after a reload');

  assertEq(await clickButton(urlA), true, 'now on');
  await waitFor(async () => (await charts(a)).length === 3 && (await charts(b)).length === 2, 'charts back');
  await waitFor(async () => (await buttonState(urlA)).badge === '3', 'badge back');
  for (const p of [a, b, c]) {
    await p.close();
  }
});

test('the button is disabled where the extension cannot run', async () => {
  const p = await context.newPage();
  await p.goto('about:blank');
  await waitFor(async () => (await buttonState(null)).enabled === false, 'disabled');
  await p.close();
});

test('the badge switches a chart back to its image, and opens it', async () => {
  const p = await openPage('markdown');
  await waitFor(async () => (await charts(p)).some((c) => c.id === 'line'), 'chart');
  const box = (await charts(p)).find((c) => c.id === 'line').box;
  await centreOf(p, box);
  box[1] -= await p.evaluate(() => scrollY);
  await p.mouse.move(box[0] + 50, box[1] + 50);
  // Badge: two 52 px buttons, 4 px apart, 6 px from the top-right corner.
  const y = box[1] + 6 + 8;
  await p.mouse.click(box[0] + box[2] - 6 - 52 - 4 - 26, y);
  const label = () => p.evaluate(() => document.querySelector('#line + nymph-ext-chart').shadowRoot.querySelector('[data-act="toggle"]').textContent);
  await waitFor(async () => (await label()) === 'Chart', 'showing the image');
  assert(await p.evaluate(() => document.querySelector('#line + nymph-ext-chart').classList.contains('nymph-ext-showing-image')), 'image shown');
  await p.mouse.click(box[0] + box[2] - 6 - 52 - 4 - 26, y);
  await waitFor(async () => (await label()) === 'Image', 'showing the chart');
  const popup = context.waitForEvent('page');
  await p.mouse.click(box[0] + box[2] - 6 - 26, y);
  const opened = await popup;
  assert(opened.url().endsWith('/img/line.png'), `opened ${opened.url()}`);
  await opened.close();
  await p.close();
});

test('theme "auto" follows the browser colour scheme', async () => {
  const p = await openPage('changes');
  await p.emulateMedia({ colorScheme: 'dark' });
  await waitFor(async () => (await charts(p)).length === 2, 'charts');
  await new Promise((resolve) => setTimeout(resolve, 300));
  const box = (await charts(p))[0].box;
  const light = luminance(await shot(p, box));
  await setSettings({ theme: 'auto' });
  await waitFor(async () => (await charts(p)).length === 2, 'charts again');
  await new Promise((resolve) => setTimeout(resolve, 800));
  const dark = luminance(await shot(p, (await charts(p))[0].box));
  assert(dark < light - 40, `darker: ${light.toFixed(1)} -> ${dark.toFixed(1)}`);
  await setSettings({ theme: 'source' });
  await p.close();
});

// ── Chat mode (spec/10_gemini_extension plan steps 3 and 4) ─────────────────

/** Charts of chat mode that are drawn (not starting). */
const chatLive = (p) => p.evaluate(() => document.querySelectorAll('nymph-ext-chart:not(.nymph-ext-starting)').length);

/** Clicks a badge button of the first chat chart (the badge shows on hover only). */
const badge = (p, act) =>
  p.evaluate((a) => document.querySelector('nymph-ext-chart').shadowRoot.querySelector(`[data-act="${a}"]`).click(), act);

for (const mode of ['inline', 'frame']) {
  const csp = mode === 'frame' ? '?csp=github' : '';

  test(`${mode}: chat mode draws a finished nymph block and leaves other code`, async () => {
    await setSettings({ chatSites: [SITE] });
    const p = await openPage('chat', csp);
    await waitFor(async () => (await chatLive(p)) === 1, 'one chart');
    const state = await p.evaluate(() => {
      const host = document.querySelector('nymph-ext-chart');
      return {
        hosts: document.querySelectorAll('nymph-ext-chart').length,
        next: host.nextElementSibling?.id,
        hidden: [...document.querySelectorAll('[data-nymph-ext="source"]')].map((e) => e.id),
        hostWidth: host.getBoundingClientRect().width,
        hostHeight: host.getBoundingClientRect().height,
        column: document.getElementById('msgs').getBoundingClientRect().width,
      };
    });
    assertEq(state.hosts, 1, 'one host');
    assertEq(state.next, 'chart', 'the host stands before the block');
    assertEq(state.hidden, ['chart'], 'only the nymph block is collapsed');
    assert(state.hostWidth > 100 && state.hostWidth <= state.column + 1, `width ${state.hostWidth} in column ${state.column}`);
    assert(Math.abs(state.hostHeight / state.hostWidth - 420 / 800) < 0.02, `shape ${state.hostWidth} x ${state.hostHeight}`);
    await p.close();
  });

  test(`${mode}: the badge switches a chat chart to its code and back, and copies its YAML`, async () => {
    await setSettings({ chatSites: [SITE] });
    await context.grantPermissions(['clipboard-read', 'clipboard-write'], { origin: SITE });
    const p = await openPage('chat', csp);
    await waitFor(async () => (await chatLive(p)) === 1, 'one chart');
    const code = () => p.evaluate(() => ({
      attr: document.getElementById('chart').getAttribute('data-nymph-ext'),
      display: getComputedStyle(document.getElementById('chart')).display,
      label: document.querySelector('nymph-ext-chart').shadowRoot.querySelector('[data-act="toggle"]').textContent,
    }));
    const hostHeight = () => p.evaluate(() => Math.round(document.querySelector('nymph-ext-chart').getBoundingClientRect().height));
    const chartHeight = await hostHeight();
    await badge(p, 'toggle');
    assertEq(await code(), { attr: 'source-shown', display: 'block', label: 'Chart' }, 'code shown');
    assert((await hostHeight()) < 60, `no empty gap above the code (host ${await hostHeight()} px)`);
    await badge(p, 'toggle');
    assertEq(await code(), { attr: 'source', display: 'none', label: 'Code' }, 'chart shown');
    assertEq(await hostHeight(), chartHeight, 'the chart has its height back');
    await p.bringToFront();
    await badge(p, 'copy');
    const copied = await waitFor(async () => {
      const text = await p.evaluate(() => navigator.clipboard.readText().catch(() => ''));
      return text.includes('root:') ? text : '';
    }, 'the YAML on the clipboard');
    assert(copied.includes('Load test'), "the chart's own YAML");
    await p.close();
  });

  test(`${mode}: a block that starts like nymph is replaced, with the error when it is invalid`, async () => {
    await setSettings({ chatSites: [SITE] });
    const p = await openPage('chatbad', csp);
    await waitFor(() => p.evaluate(() => document.querySelectorAll('nymph-ext-chart.nymph-ext-error').length === 3), 'three error panels', 15000);
    const state = await p.evaluate(() => {
      const panel = (id) => {
        const host = document.getElementById(id).previousElementSibling;
        return host?.localName === 'nymph-ext-chart'
          ? { detail: host.shadowRoot.querySelector('.detail').textContent, shown: getComputedStyle(host.shadowRoot.querySelector('.error')).display }
          : null;
      };
      return {
        unknown: panel('unknown'), broken: panel('broken'), months: panel('months'),
        otherHost: document.getElementById('other').previousElementSibling?.localName === 'nymph-ext-chart',
        otherDisplay: getComputedStyle(document.getElementById('other')).display,
        collapsed: [...document.querySelectorAll('[data-nymph-ext="source"]')].map((e) => e.id),
      };
    });
    for (const id of ['unknown', 'broken', 'months']) {
      assertEq(state[id]?.shown, 'block', `${id}: the error is shown`);
      assert(state[id].detail.length > 10, `${id}: has a message (${JSON.stringify(state[id].detail)})`);
    }
    assert(state.months.detail.includes("'Jan' is not a number"), `the detail says what is wrong: ${state.months.detail}`);
    assert(/\d/.test(state.months.detail), `the detail says where: ${state.months.detail}`);
    assertEq(state.otherHost, false, 'a block that does not start like nymph gets no host');
    assertEq(state.otherDisplay, 'block', 'and stays visible');
    assertEq(state.collapsed.sort(), ['broken', 'months', 'unknown'], 'the claimed blocks are collapsed');
    // The code is one click away, and the error comes back.
    await p.evaluate(() => document.getElementById('months').previousElementSibling.shadowRoot.querySelector('[data-act="toggle"]').click());
    assertEq(await p.evaluate(() => getComputedStyle(document.getElementById('months')).display), 'block', 'code shown');
    assertEq(await p.evaluate(() => getComputedStyle(document.getElementById('months').previousElementSibling.shadowRoot.querySelector('.error')).display), 'none', 'error hidden meanwhile');
    await p.close();
  });

  test(`${mode}: the error's button puts the fix request in the prompt box and does not send it`, async () => {
    await setSettings({ chatSites: [SITE] });
    const p = await openPage('chatfix', csp);
    await waitFor(() => p.evaluate(() => document.querySelectorAll('nymph-ext-chart.nymph-ext-error').length === 1), 'the error panel', 15000);
    await p.evaluate(() => document.querySelector('nymph-ext-chart').shadowRoot.querySelector('[data-act="fix"]').click());
    await waitFor(() => p.evaluate(() => document.getElementById('box').innerText.includes('failed to draw')), 'the request in the box');
    const box = await p.evaluate(() => ({ text: document.getElementById('box').innerText, sent: window.sent }));
    assert(box.text.includes('Fix the YAML and send the whole chart again.'), 'asks for the whole chart');
    assert(box.text.length > 80, 'with the error in it');
    assertEq(box.sent, 0, 'nothing was sent');
    await p.close();
  });

  test(`${mode}: a streaming answer is drawn only once it is finished`, async () => {
    await setSettings({ chatSites: [SITE] });
    const p = await openPage('chatstream', csp);
    let samples = 0;
    while (!(await p.evaluate(() => window.finished))) {
      assertEq(await hostCount(p), 0, 'no chart while streaming');
      samples++;
      await new Promise((resolve) => setTimeout(resolve, 100));
    }
    assert(samples >= 3, `streaming was observed (${samples} samples)`);
    await waitFor(async () => (await chatLive(p)) === 1, 'the chart after the last token', 8000);
    await p.close();
  });
}

test('a revised block updates its chart in place', async () => {
  await setSettings({ chatSites: [SITE] });
  const p = await openPage('chat');
  await waitFor(async () => (await chatLive(p)) === 1, 'one chart');
  await new Promise((resolve) => setTimeout(resolve, 300));
  await p.evaluate(() => document.querySelector('nymph-ext-chart').setAttribute('data-test', 'same'));
  const box = await p.evaluate(() => {
    const r = document.querySelector('nymph-ext-chart').getBoundingClientRect();
    return [r.x + scrollX, r.y + scrollY, r.width, r.height].map(Math.round);
  });
  const before = await shot(p, box);
  await p.evaluate(() => window.revise());
  await new Promise((resolve) => setTimeout(resolve, 1500)); // settle, then redraw
  const after = await shot(p, box);
  assertEq(await p.evaluate(() => document.querySelector('nymph-ext-chart')?.getAttribute('data-test')), 'same', 'same host');
  assertEq(await hostCount(p), 1, 'still one chart');
  let differing = 0;
  for (let i = 0; i < before.rgba.length; i += 4) {
    differing += before.rgba[i] !== after.rgba[i] ? 1 : 0;
  }
  assert(differing > 50, `the title changed on screen (${differing} pixels differ)`);
  await p.close();
});

test('a message the page re-renders gets its chart again, without a leftover', async () => {
  await setSettings({ chatSites: [SITE] });
  const p = await openPage('chat');
  await waitFor(async () => (await chatLive(p)) === 1, 'one chart');
  await p.evaluate(() => window.render());
  await waitFor(async () => (await chatLive(p)) === 1 && (await hostCount(p)) === 1, 'one chart again');
  assertEq(await p.evaluate(() => document.querySelector('nymph-ext-chart').nextElementSibling.id), 'chart', 'before the new block');
  await p.close();
});

test('chat mode runs only on the listed sites, and turns off with the site', async () => {
  await setSettings({ chatSites: [OTHER] }); // not this site
  const p = await openPage('chat');
  await new Promise((resolve) => setTimeout(resolve, 2500));
  assertEq(await hostCount(p), 0, 'unlisted site untouched');
  assertEq(await p.evaluate(() => document.querySelectorAll('[data-nymph-ext]').length), 0, 'nothing collapsed');
  await setSettings({ chatSites: [SITE] }); // listed, without a reload
  await waitFor(async () => (await chatLive(p)) === 1, 'the chart appears');
  await setSettings({ off: [SITE] }); // the toolbar button
  await waitFor(async () => (await hostCount(p)) === 0, 'the chart goes');
  assertEq(await p.evaluate(() => getComputedStyle(document.getElementById('chart')).display), 'block', 'code shown again');
  assertEq(await p.evaluate(() => document.querySelectorAll('[data-nymph-ext], #nymph-ext-style').length), 0, 'no marks left');
  await p.close();
});

// ── Teaching a chat nymph (spec/10_gemini_extension plan step 8) ────────────

/** What the toolbar menu does for the tab showing `url`. */
const teach = (url) =>
  worker.evaluate(async (u) => {
    const [tab] = (await chrome.tabs.query({})).filter((t) => t.url === u);
    return globalThis.nymphTeachTab(tab);
  }, url);

const PRIMER_START = 'You can draw charts and diagrams with nymph.';

test('the toolbar menu puts the instructions in the prompt box and never sends them', async () => {
  await setSettings({ chatSites: [SITE] });
  const p = await openPage('chatbox');
  await waitFor(() => p.evaluate(() => (document.documentElement.dataset.nymphStatus ?? '').includes('chat on')), 'chat mode on');
  assertEq(await teach(p.url()), 'inserted', 'result');
  const box = await p.evaluate(() => ({ text: document.getElementById('box').innerText, sent: window.sent }));
  assert(box.text.startsWith('first'), 'what was in the box stays');
  assert(box.text.includes(PRIMER_START), 'the instructions are in the box');
  assert(box.text.includes('nymph: 1'), 'with its examples');
  assertEq(box.sent, 0, 'nothing was sent');
  assert(await p.evaluate(() => !!document.querySelector('nymph-ext-toast')), 'the page says what happened');
  await p.close();
});

test('with no prompt box the instructions go to the clipboard', async () => {
  await setSettings({ chatSites: [SITE] });
  await context.grantPermissions(['clipboard-read', 'clipboard-write'], { origin: SITE });
  const p = await openPage('chat');
  await waitFor(() => p.evaluate(() => (document.documentElement.dataset.nymphStatus ?? '').includes('chat on')), 'chat mode on');
  await p.bringToFront();
  const result = await teach(p.url());
  assert(result === 'copied' || result === 'failed', `result ${result}`);
  if (result === 'copied') {
    assert((await p.evaluate(() => navigator.clipboard.readText())).startsWith(PRIMER_START), 'the instructions are on the clipboard');
  }
  await p.close();
});

test('a site that is not listed has no instructions to give', async () => {
  await setSettings({ chatSites: [OTHER] });
  const p = await openPage('chatbox');
  await new Promise((resolve) => setTimeout(resolve, 800));
  assertEq(await teach(p.url()), 'unavailable', 'result');
  assertEq(await p.evaluate(() => document.getElementById('box').innerText), 'first', 'the box is untouched');
  await p.close();
});

test('the options page lists chat sites, adds and removes them, and shows the instructions', async () => {
  await setSettings({ chatSites: ['https://gemini.google.com'], chatTheme: 'auto' });
  const p = await context.newPage();
  await p.goto(`chrome-extension://${new URL(worker.url()).host}/src/options/options.html`);
  await p.getByText('https://gemini.google.com').first().waitFor();
  await p.fill('#chat-site', 'chat.example.com');
  await p.getByRole('button', { name: 'Add site' }).click();
  await waitFor(() => worker.evaluate(async () => (await chrome.storage.sync.get('chatSites')).chatSites.includes('https://chat.example.com')), 'site added');
  await p.waitForFunction(() => document.getElementById('chat-site').value === ''); // the page has finished with the add
  await p.fill('#chat-site', 'not a site at all');
  await p.getByRole('button', { name: 'Add site' }).click();
  const noteText = await p.textContent('#chat-site-note'); assert(noteText.includes('not a web address'), 'a bad address is refused: note is ' + JSON.stringify(noteText) + ', field ' + JSON.stringify(await p.inputValue('#chat-site')) + ', stored ' + JSON.stringify(await worker.evaluate(async () => (await chrome.storage.sync.get('chatSites')).chatSites)));
  await p.getByRole('listitem').filter({ hasText: 'https://chat.example.com' }).getByRole('button', { name: 'Remove' }).click();
  await waitFor(() => worker.evaluate(async () => (await chrome.storage.sync.get('chatSites')).chatSites.length === 1), 'site removed');
  await p.check('input[name="chatTheme"][value="source"]');
  await waitFor(() => worker.evaluate(async () => (await chrome.storage.sync.get('chatTheme')).chatTheme === 'source'), 'theme saved');
  await waitFor(() => p.evaluate(() => document.getElementById('primer').textContent.startsWith('You can draw')), 'the instructions are shown');
  await p.close();
});

// ── Run ──────────────────────────────────────────────────────────────────────

let failed = 0, ran = 0;
for (const { name, fn } of tests) {
  if (filter && !name.includes(filter)) {
    continue;
  }
  ran++;
  await setSettings({ off: [], theme: 'source', chatSites: ['https://gemini.google.com'], chatTheme: 'auto' });
  const start = Date.now();
  try {
    await fn();
    console.log(`ok   ${name} (${Date.now() - start} ms)`);
  } catch (e) {
    failed++;
    console.log(`FAIL ${name}\n     ${e.stack ?? e}`);
    for (const p of context.pages()) {
      await p.close().catch(() => {});
    }
  }
}
console.log(`${ran - failed} of ${ran} passed`);
await context.close();
server.close();
other.close();
await rm(profile, { recursive: true, force: true });
process.exit(failed ? 1 : 0);
