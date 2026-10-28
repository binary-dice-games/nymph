// MIT License © 2026 Binary Dice Games
// spec/18_collaboration plan steps 4 and 5: collab-session over real WebRTC
// between two browser contexts (host candidates only, through the local
// test tracker or the manual codes), and collab-whiteboard over it, in
// Chromium through Playwright. Exits 77 (CTest's skip) when Playwright is
// not installed. Nothing here reaches a public tracker or STUN server.
//
//   node collab.mjs <build>/web <nymph source dir>

import { execSync } from 'node:child_process';
import { createRequire } from 'node:module';
import { join } from 'node:path';
import { serve } from './serve.mjs';
import { startTracker } from './tracker.mjs';

const [webDir, sourceDir] = process.argv.slice(2);
if (!webDir || !sourceDir) {
  console.error('usage: node collab.mjs <build>/web <nymph source dir>');
  process.exit(2);
}

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

const PAGE = `<!doctype html><meta charset="utf-8"><title>collab test</title>
<style>body{margin:0} #box{position:relative;width:600px;height:400px;background:#eee}</style>
<div id="box"><button id="under" style="position:absolute;left:300px;top:200px;width:60px;height:30px" onclick="window.clicked=(window.clicked||0)+1">under</button></div>`;
const site = await serve([join(sourceDir, 'web', 'collab')], {
  '/t.html': (req, res) => res.writeHead(200, { 'content-type': 'text/html; charset=utf-8' }).end(PAGE),
});
const tracker = await startTracker();
// Host candidates as IP addresses, not mDNS names, so two contexts on one
// machine reach each other with no STUN server.
const browser = await playwright.chromium.launch({ args: ['--disable-features=WebRtcHideLocalIpsWithMdns'] });

const tests = [];
const test = (name, fn) => tests.push({ name, fn });
const assert = (cond, message) => {
  if (!cond) {
    throw new Error(message);
  }
};

/** A page in a fresh context (another "computer"), the packages imported. */
async function peer(context) {
  const ctx = context ?? (await browser.newContext());
  const page = await ctx.newPage();
  page.on('pageerror', (e) => console.log(`  page error: ${e.message}`));
  await page.goto(`${site.base}/t.html`);
  await page.evaluate(async () => {
    window.cs = await import('/collab-session.js');
    window.wbm = await import('/collab-whiteboard.js');
  });
  return { page, ctx };
}

/** Starts a session in `page` as window.s; returns its token. */
async function start(page, options) {
  return page.evaluate(async (o) => {
    window.got = [];
    window.errors = [];
    window.s = await window.cs.createSession(o);
    window.s.channel('chat').on('message', (m, from) => window.got.push({ m, from }));
    window.s.on('error', (e) => window.errors.push(e.reason));
    return window.s.token;
  }, options);
}

const torrent = (extra = {}) => [{ kind: 'torrent', urls: [tracker.url], announceMs: { host: 1000, guest: 700 }, gatherMs: 1500, ...extra }];
const waitOpen = (page) => page.waitForFunction(() => window.s.state === 'open' && window.s.peers().length >= 2, null, { timeout: 30000 });

test('a guest joins through the tracker; messages flow both ways on both channels', async () => {
  const a = await peer();
  const b = await peer();
  const token = await start(a.page, { role: 'host', me: { name: 'Host', color: '#e5484d' }, rendezvous: torrent() });
  await start(b.page, { role: 'guest', token, me: { name: 'Guest', color: '#3e63dd' }, rendezvous: torrent() });
  await waitOpen(b.page);
  await a.page.waitForFunction(() => window.s.peers().length === 2);
  await a.page.evaluate(() => window.s.channel('chat').send({ text: 'from host' }));
  await b.page.evaluate(() => window.s.channel('chat').send({ text: 'from guest' }));
  await b.page.evaluate(() => window.s.channel('chat').send({ text: 'fast' }, { fast: true }));
  await a.page.waitForFunction(() => window.got.length === 2);
  await b.page.waitForFunction(() => window.got.length === 1);
  const got = await a.page.evaluate(() => window.got.map((g) => g.m.text).sort().join());
  assert(got === 'fast,from guest', `host got ${got}`);
  // The path is known: direct, on one machine.
  await a.page.waitForFunction(() => window.s.peers().some((p) => p.role === 'guest' && p.path === 'direct'), null, { timeout: 15000 });
  // The tracker never saw a readable SDP.
  const leaked = tracker.log.some((m) => JSON.stringify(m).includes('v=0'));
  assert(!leaked, 'a readable SDP went through the tracker');
  // Stopping the tracker does not drop the link.
  await a.page.evaluate(() => window.s.rendezvous.forEach((r) => r.stop()));
  await b.page.evaluate(() => window.s.channel('chat').send({ text: 'still here' }));
  await a.page.waitForFunction(() => window.got.some((g) => g.m.text === 'still here'));
  await a.page.evaluate(() => window.s.close());
  await b.page.waitForFunction(() => window.s.state === 'ended');
  await a.ctx.close();
  await b.ctx.close();
});

test('a guest that comes before the host connects once the host is there', async () => {
  const a = await peer();
  const b = await peer();
  const token = await a.page.evaluate(() => window.cs.newToken());
  await start(b.page, { role: 'guest', token, me: { name: 'Early' }, rendezvous: torrent() });
  await new Promise((r) => setTimeout(r, 1500));
  await start(a.page, { role: 'host', token, me: { name: 'Host' }, rendezvous: torrent() });
  await waitOpen(b.page);
  await a.page.evaluate(() => window.s.close());
  await a.ctx.close();
  await b.ctx.close();
});

test('the manual codes connect with no tracker', async () => {
  const a = await peer();
  const b = await peer();
  const token = await start(a.page, { role: 'host', me: { name: 'Host' }, rendezvous: [{ kind: 'manual', gatherMs: 1500 }] });
  await start(b.page, { role: 'guest', token, me: { name: 'Guest' }, rendezvous: [{ kind: 'manual', gatherMs: 1500 }] });
  const offer = await b.page.evaluate(() => window.s.manual.offerCode());
  assert(/^[A-Za-z0-9_-]+$/.test(offer) && offer.length < 4000, `offer code ${offer.length} characters`);
  const answer = await a.page.evaluate((c) => window.s.manual.acceptOffer(c), offer);
  await b.page.evaluate((c) => window.s.manual.acceptAnswer(c), answer);
  await waitOpen(b.page);
  // A code of another room is refused with a message.
  const c = await peer();
  await start(c.page, { role: 'host', me: { name: 'Other' }, rendezvous: [{ kind: 'manual' }] });
  const refused = await c.page.evaluate((code) => window.s.manual.acceptOffer(code).then(() => '', (e) => e.reason), offer);
  assert(refused === 'bad-code', `another room's code: ${refused}`);
  await a.page.evaluate(() => window.s.close());
  for (const p of [a, b, c]) {
    await p.ctx.close();
  }
});

test('no tracker reachable: the guest is told so', async () => {
  const b = await peer();
  await start(b.page, { role: 'guest', token: await b.page.evaluate(() => window.cs.newToken()), me: { name: 'Lost' }, rendezvous: [{ kind: 'torrent', urls: ['ws://127.0.0.1:9'] }], connectTimeoutMs: 11000 });
  await b.page.waitForFunction(() => window.errors.includes('no-tracker'), null, { timeout: 20000 });
  await b.ctx.close();
});

// ── Whiteboard ────────────────────────────────────────────────────────────────

/** A whiteboard over #box in `page`, at `scale` content units to CSS px. */
async function board(page, scale = 1) {
  await page.evaluate((k) => {
    const box = document.getElementById('box');
    window.adapter = {
      element: box,
      scale: k,
      transform: () => ({ a: window.adapter.scale, b: 0, c: 0, d: window.adapter.scale, e: 0, f: 0 }),
      listeners: new Set(),
      onChange(cb) {
        this.listeners.add(cb);
        return () => this.listeners.delete(cb);
      },
    };
    window.wb = window.wbm.createWhiteboard(box, { session: window.s, adapter: window.adapter });
  }, scale);
}

/** Two tabs of one browser over a BroadcastChannel, each with a whiteboard. */
async function pair(scaleB = 1) {
  const ctx = await browser.newContext();
  const a = await peer(ctx);
  const b = await peer(ctx);
  const token = await start(a.page, { role: 'host', me: { name: 'Ana', color: '#e5484d' }, rendezvous: [{ kind: 'broadcast' }] });
  await start(b.page, { role: 'guest', token, me: { name: 'Ben', color: '#3e63dd' }, rendezvous: [{ kind: 'broadcast' }] });
  await waitOpen(b.page);
  await board(a.page);
  await board(b.page, scaleB);
  return { ctx, a: a.page, b: b.page, token };
}

const items = (page) => page.evaluate(() => window.wb.items());

test('a stroke appears at the same content position for a peer at another scale', async () => {
  const { ctx, a, b } = await pair(0.5);
  await a.evaluate(() => window.wb.setTool('pen'));
  await a.mouse.move(100, 100);
  await a.mouse.down();
  for (let i = 1; i <= 20; ++i) {
    await a.mouse.move(100 + i * 5, 100 + i * 2);
  }
  // The peer sees the ink while it is drawn.
  await b.waitForFunction(() => document.querySelector('collab-whiteboard').shadowRoot.querySelector('.live path')?.getAttribute('d')?.length > 20, null, { timeout: 5000 });
  await a.mouse.up();
  await b.waitForFunction(() => window.wb.items().length === 1);
  const [ia, ib] = [(await items(a))[0], (await items(b))[0]];
  assert(ia.points[0] === 100 && ib.points[0] === 100, `content position ${ia.points[0]} / ${ib.points[0]}`);
  // On screen, the peer at half scale draws it at half the CSS position.
  const left = await b.evaluate(() => document.querySelector('collab-whiteboard').shadowRoot.querySelector('.items path:not(.hit)').getBoundingClientRect().left);
  assert(Math.abs(left - 50) < 4, `drawn at ${left}`);
  // The peer's cursor is shown with its name.
  await a.mouse.move(200, 150);
  await b.waitForFunction(() => [...document.querySelector('collab-whiteboard').shadowRoot.querySelectorAll('.cursor span')].some((s) => s.textContent === 'Ana'));
  await ctx.close();
});

test('text from a peer is shown as text; unknown stickers draw a placeholder', async () => {
  const { ctx, a, b } = await pair();
  await a.evaluate(() => {
    const m = window.wb.model;
    window.s.channel('wb').send({ t: 'op', op: m.add({ kind: 'text', x: 20, y: 20, size: 18, text: '<b>bold</b><img src=x onerror=alert(1)>', color: '#1c2024' }) });
    window.s.channel('wb').send({ t: 'op', op: m.add({ kind: 'sticker', x: 60, y: 60, size: 30, name: 'not-a-sticker' }) });
  });
  await b.waitForFunction(() => window.wb.items().length === 2);
  const r = await b.evaluate(() => {
    const root = document.querySelector('collab-whiteboard').shadowRoot;
    return {
      text: root.querySelector('.text-item text').textContent,
      elements: root.querySelectorAll('.text-item b, .text-item img').length,
      placeholder: !!root.querySelector('#sticker-not-a-sticker'),
    };
  });
  assert(r.text.includes('<b>bold</b>') && r.elements === 0, `text shown as ${JSON.stringify(r)}`);
  assert(r.placeholder, 'placeholder symbol');
  await ctx.close();
});

test('pointer mode lets a click through; text, eraser and undo propagate', async () => {
  const { ctx, a, b } = await pair();
  await a.mouse.click(320, 210);
  assert((await a.evaluate(() => window.clicked)) === 1, 'click reached the page in pointer mode');
  await a.evaluate(() => window.wb.setTool('pen'));
  await a.mouse.click(320, 210);
  assert((await a.evaluate(() => window.clicked)) === 1, 'no click through while drawing');
  // A text note, typed.
  await a.evaluate(() => window.wb.setTool('text'));
  await a.mouse.click(40, 300);
  await a.keyboard.type('Check this');
  await a.keyboard.press('Enter');
  await b.waitForFunction(() => window.wb.items().some((i) => i.text === 'Check this'));
  // Eraser on the dot drawn above.
  await a.evaluate(() => window.wb.setTool('eraser'));
  const before = (await items(b)).length;
  await a.mouse.move(320, 210);
  await a.mouse.down();
  await a.mouse.move(321, 211);
  await a.mouse.up();
  await b.waitForFunction((n) => window.wb.items().length === n - 1, before);
  // Undo brings it back for everyone.
  await a.evaluate(() => window.wb.setTool('pen'));
  await a.keyboard.press('Control+z');
  await b.waitForFunction((n) => window.wb.items().length === n, before);
  // Esc goes back to pointer mode.
  await a.keyboard.press('Escape');
  assert((await a.evaluate(() => window.wb.tool)) === 'pointer', 'Esc');
  await ctx.close();
});

test('a late joiner gets the items; a read-only whiteboard draws nothing', async () => {
  const { ctx, a, b, token } = await pair();
  await a.evaluate(() => window.wb.setTool('sticker'));
  await a.mouse.click(150, 150);
  await b.waitForFunction(() => window.wb.items().length === 1);
  const c = await peer(ctx);
  await start(c.page, { role: 'guest', token, me: { name: 'Cleo' }, rendezvous: [{ kind: 'broadcast' }] });
  await waitOpen(c.page);
  await board(c.page);
  await c.page.waitForFunction(() => window.wb.items().length === 1);
  await c.page.evaluate(() => (window.wb.readOnly = true));
  await c.page.evaluate(() => window.wb.setTool('pen'));
  assert((await c.page.evaluate(() => window.wb.tool)) === 'pointer', 'read-only stays in pointer mode');
  await c.page.mouse.click(200, 200);
  assert((await items(c.page)).length === 1, 'nothing drawn');
  // toSvg: a standalone SVG of the items.
  const svg = await a.evaluate(() => window.wb.toSvg({ x: 0, y: 0, width: 600, height: 400 }));
  assert(svg.startsWith('<svg') && svg.includes('viewBox="0 0 600 400"') && svg.includes('sticker-check'), 'toSvg');
  const cb = await a.evaluate(() => window.wb.contentBounds());
  assert(cb && cb.width > 0 && cb.height > 0, 'contentBounds covers the drawings');
  await ctx.close();
});

test('the demo page works served alone, with no nymph file', async () => {
  const ctx = await browser.newContext();
  const open = async () => {
    const page = await ctx.newPage();
    page.on('dialog', (d) => d.accept(d.type() === 'prompt' ? 'Demo' : undefined));
    page.on('pageerror', (e) => console.log(`  page error: ${e.message}`));
    return page;
  };
  const a = await open();
  await a.goto(`${site.base}/demo/?local`);
  await a.click('#start');
  await a.waitForFunction(() => document.getElementById('link').value.includes('#collab='));
  const link = await a.inputValue('#link');
  const b = await open();
  await b.goto(link.replace('/demo/?local', '/demo/?local'));
  await b.waitForFunction(() => document.getElementById('state').textContent === 'open', null, { timeout: 10000 });
  assert(await b.evaluate(() => !!document.querySelector('collab-whiteboard')), 'a whiteboard over the picture');
  await a.waitForFunction(() => document.getElementById('people').textContent.split(',').length === 2);
  // Only the two packages and the page were loaded: nothing of nymph.
  const loaded = await b.evaluate(() => performance.getEntriesByType('resource').map((r) => new URL(r.name).pathname));
  assert(loaded.every((p) => /^\/(collab-session|collab-whiteboard)\.js$/.test(p)), `loaded ${loaded.join(', ')}`);
  await ctx.close();
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
await browser.close();
site.close();
tracker.close();
console.log(`${tests.length - failed}/${tests.length} passed`);
process.exit(failed ? 1 : 0);
