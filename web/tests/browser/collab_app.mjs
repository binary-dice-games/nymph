// MIT License © 2026 Binary Dice Games
// spec/18_collaboration plan steps 7 and 8: live collaboration in the web
// app on the built site (<build>/web/site/), an owner and guests in
// separate browser contexts, through the local test tracker (config.json
// is replaced: no public tracker or STUN server is reached). Exits 77
// (CTest's skip) when Playwright is not installed.
//
//   node collab_app.mjs <build>/web <nymph source dir>

import { execSync } from 'node:child_process';
import { readFile } from 'node:fs/promises';
import { createRequire } from 'node:module';
import { join } from 'node:path';
import { serve } from './serve.mjs';
import { startTracker } from './tracker.mjs';

const [webDir, sourceDir] = process.argv.slice(2);
if (!webDir || !sourceDir) {
  console.error('usage: node collab_app.mjs <build>/web <nymph source dir>');
  process.exit(2);
}
const siteDir = join(webDir, 'site');

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

const tracker = await startTracker();
const config = JSON.parse(await readFile(join(siteDir, 'config.json'), 'utf8'));
config.collab = { trackers: [tracker.url], iceServers: [], torrent: { announceMs: { host: 1000, guest: 700 }, gatherMs: 1500 } };
const site = await serve([siteDir], {
  '/config.json': (req, res) => res.writeHead(200, { 'content-type': 'application/json' }).end(JSON.stringify(config)),
});
const browser = await playwright.chromium.launch({
  args: ['--enable-unsafe-swiftshader', '--use-angle=swiftshader', '--disable-features=WebRtcHideLocalIpsWithMdns'],
});

const tests = [];
const test = (name, fn) => tests.push({ name, fn });
const assert = (cond, message) => {
  if (!cond) {
    throw new Error(message);
  }
};

/** A page of the app in its own context (another computer), named `name`. */
async function openApp(name, path = '/', options = {}) {
  const context = await browser.newContext({ viewport: { width: 1280, height: 800 }, deviceScaleFactor: 1, acceptDownloads: true, serviceWorkers: 'block' });
  await context.grantPermissions(['clipboard-read', 'clipboard-write'], { origin: site.base });
  const page = await context.newPage();
  page.errors = [];
  page.on('pageerror', (e) => page.errors.push(e.message));
  await page.addInitScript(([n, agreed]) => {
    localStorage.setItem('nymph.collab.me', JSON.stringify({ name: n, color: '#3e63dd' }));
    if (agreed) {
      localStorage.setItem('nymph.collab.agreed', '1');
    }
  }, [name, options.agreed !== false]);
  await page.goto(`${site.base}${path}`);
  await page.waitForFunction(() => document.documentElement.dataset.ready === 'true', null, { timeout: 30000 });
  return page;
}

async function shown(page) {
  await page.waitForFunction(
    () => {
      const a = window.nymphApp?.app;
      if (!a?.doc) {
        return false;
      }
      if (a.mode === 'edit') {
        return a.editor && a.editor.m._nymph_editor_wants_frame() === 0;
      }
      return a.viewer && !a.viewer.wantsFrame?.();
    },
    null,
    { timeout: 30000 },
  );
}

const run = (page, name, args = {}) => page.evaluate(([n, a]) => window.nymphApp.commands.run(n, a), [name, args]);
const guestSource = (page) => page.evaluate(() => window.nymphApp.app.doc?.origin.kind === 'collab' && window.nymphApp.app.doc.source);

/** An owner sharing an example, and a guest that joined through the invite link. */
async function live(example = 'bars') {
  const owner = await openApp('Ana', `/?example=${example}`);
  await shown(owner);
  await run(owner, 'collab_start');
  const link = await owner.evaluate(() => window.nymphApp.app.collab.inviteLink());
  assert(/#collab=[A-Za-z0-9_-]{22}$/.test(link), `invite link ${link}`);
  const guest = await openApp('Ben', link.slice(site.base.length));
  await guest.waitForFunction(() => window.nymphApp.app.doc?.origin.kind === 'collab', null, { timeout: 40000 });
  await shown(guest);
  await guest.waitForFunction(() => !!window.nymphApp.app.collab.wb, null, { timeout: 10000 });
  return { owner, guest };
}

async function done(...pages) {
  for (const p of pages) {
    assert(p.errors.length === 0, `page errors: ${p.errors.join('; ')}`);
    await p.context().close();
  }
}

test('figureBox(): the viewer\'s canvas, the editor\'s figure under its tab bar, with figure events', async () => {
  const page = await openApp('Solo', '/?example=bars');
  await shown(page);
  const v = await page.evaluate(async () => {
    const viewer = window.nymphApp.app.viewer;
    const box = viewer.figureBox();
    const c = viewer.canvas.getBoundingClientRect();
    const o = viewer.container.getBoundingClientRect();
    const events = [];
    viewer.on('figure', (b) => events.push(b));
    const yaml = viewer.source().replace(/^image:.*$/m, '').replace(/^/, 'image: {width: 300, height: 200}\n');
    await viewer.setSource(yaml);
    await viewer.rendered();
    return { box, canvas: { left: c.left - o.left, top: c.top - o.top, width: c.width }, events };
  });
  assert(Math.abs(v.box.left - v.canvas.left) < 1 && Math.abs(v.box.top - v.canvas.top) < 1 && Math.abs(v.box.width - v.canvas.width) < 1, `viewer box ${JSON.stringify(v)}`);
  assert(v.box.scale > 0, 'scale');
  assert(v.events.length >= 1 && Math.abs(v.events.at(-1).width / v.events.at(-1).scale - 300) < 1, `figure event ${JSON.stringify(v.events)}`);
  await run(page, 'app_set_mode', { mode: 'edit' });
  await shown(page);
  const e = await page.evaluate(() => {
    const editor = window.nymphApp.app.editor;
    return { box: editor.figureBox(), tab: editor.state().tabHeight };
  });
  assert(e.box && Math.abs(e.box.top - e.tab) < 1 && e.box.left === 0 && e.box.width > 0, `editor box ${JSON.stringify(e)}`);
  await done(page);
});

test('the overlay sits on the figure in View and in Edit', async () => {
  const page = await openApp('Ana', '/?example=bars');
  await shown(page);
  await run(page, 'collab_start');
  const where = () =>
    page.evaluate(() => {
      const c = window.nymphApp.app.collab;
      const m = c.adapter.transform();
      const target = c.h.module();
      const box = target.figureBox();
      const t = target.container.getBoundingClientRect();
      const o = c.wb.element.getBoundingClientRect();
      return { e: m.e, f: m.f, want: { e: t.left - o.left + box.left, f: t.top - o.top + box.top }, connected: c.wb.element.isConnected };
    });
  const view = await where();
  assert(view.connected && Math.abs(view.e - view.want.e) < 1 && Math.abs(view.f - view.want.f) < 1, `view ${JSON.stringify(view)}`);
  await run(page, 'app_set_mode', { mode: 'edit' });
  await shown(page);
  const edit = await where();
  assert(edit.connected && Math.abs(edit.e - edit.want.e) < 1 && Math.abs(edit.f - edit.want.f) < 1 && edit.f > 0, `edit ${JSON.stringify(edit)}`);
  await run(page, 'collab_stop');
  await done(page);
});

test('the start dialog says what is shared, once per browser; a TURN entry is used', async () => {
  const page = await openApp('Ana', '/?example=bars', { agreed: false });
  await shown(page);
  const started = run(page, 'collab_start');
  await page.waitForSelector('dialog >> text=Your browsers connect directly');
  await page.click('dialog button:text-is("Share")');
  await started;
  assert(await page.evaluate(() => window.nymphApp.app.collab.active), 'started');
  await run(page, 'collab_stop');
  await run(page, 'collab_start');
  assert((await page.locator('dialog').count()) === 0, 'not asked again');
  await page.evaluate(() => localStorage.setItem('nymph.collab.turn', JSON.stringify({ urls: 'turns:turn.example.com:443', username: 'u', credential: 'p' })));
  const ice = await page.evaluate(() => window.nymphApp.app.collab.iceServers());
  assert(ice.some((s) => s.urls === 'turns:turn.example.com:443' && s.username === 'u' && s.credential === 'p'), `ice ${JSON.stringify(ice)}`);
  await run(page, 'collab_stop');
  await done(page);
});

test('nothing connects to a tracker before Collaborate', async () => {
  const before = tracker.connections();
  const page = await openApp('Solo', '/?example=bars');
  await shown(page);
  await new Promise((r) => setTimeout(r, 1500));
  assert(tracker.connections() === before, 'a tracker was contacted');
  assert(!(await page.isHidden('#btn-collab')), 'the Collaborate button is shown');
  await done(page);
});

test('a guest sees the owner\'s chart, and every change of it, and cannot edit it', async () => {
  const { owner, guest } = await live();
  const ownerYaml = await owner.evaluate(() => window.nymphApp.app.viewer.toYaml());
  assert((await guestSource(guest)) === ownerYaml, 'the same source');
  // The token left the guest's address bar.
  assert(!(await guest.evaluate(() => location.href)).includes('collab='), 'token in the address');
  // A change by the owner (an agent's rewrite) reaches the guest.
  const changed = ownerYaml.replace(/title: ([^\n]*)/, 'title: Changed live');
  assert(changed !== ownerYaml, 'the example has a title');
  await run(owner, 'document_set_source', { source: changed });
  await guest.waitForFunction(() => window.nymphApp.app.doc.source.includes('Changed live'), null, { timeout: 10000 });
  // The guest views only.
  const refused = await guest.evaluate(() => window.nymphApp.commands.run('app_set_mode', { mode: 'edit' }).then(() => 'edited', (e) => e.message));
  assert(/owner/.test(refused), `edit refused: ${refused}`);
  assert(await guest.isHidden('#btn-save'), 'no Save for a guest');
  const forged = await guest.evaluate(() => window.nymphApp.commands.run('document_set_source', { source: 'root: {type: Label, text: x}' }).then(() => 'set', (e) => e.message));
  assert(/owner/.test(forged), `set source refused: ${forged}`);
  // A guest's doc message never reaches the owner.
  await guest.evaluate(() => window.nymphApp.app.collab.session.channel('doc').send({ t: 'doc', v: 99, source: 'root: {type: Label, text: forged}' }));
  await new Promise((r) => setTimeout(r, 500));
  assert(!(await owner.evaluate(() => window.nymphApp.app.doc.source)).includes('forged'), 'owner unchanged');
  // The owner in Edit mode: an edit there reaches the guest too.
  await run(owner, 'app_set_mode', { mode: 'edit' });
  await shown(owner);
  const edited = changed.replace('Changed live', 'Edited in the editor');
  await run(owner, 'document_set_source', { source: edited });
  await guest.waitForFunction(() => window.nymphApp.app.doc.source.includes('Edited in the editor'), null, { timeout: 10000 });
  // State, as an agent sees it.
  const st = await owner.evaluate(() => window.nymphApp.commands.run('collab_state'));
  assert(st.role === 'owner' && st.people.length === 2, `state ${JSON.stringify(st)}`);
  await done(owner, guest);
});

test('Follow the owner shows the owner\'s view; the guest\'s own pan stops it', async () => {
  const { owner, guest } = await live('line');
  // The owner zooms (wheel over the chart): the guest follows.
  const box = await owner.locator('#stage canvas').boundingBox();
  await owner.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
  await owner.mouse.wheel(0, -400);
  await owner.waitForFunction(() => /\nview:/.test(window.nymphApp.app.viewer.toYaml()), null, { timeout: 10000 });
  const ownerView = await owner.evaluate(() => window.nymphApp.app.viewer.toYaml().split('\nview:')[1]);
  await guest.waitForFunction((v) => (window.nymphApp.app.viewer.toYaml().split('\nview:')[1] ?? '') === v, ownerView, { timeout: 10000 });
  // The guest pans by hand: no longer following.
  const gbox = await guest.locator('#stage canvas').boundingBox();
  await guest.mouse.move(gbox.x + gbox.width / 2, gbox.y + gbox.height / 2);
  await guest.mouse.down();
  await guest.mouse.move(gbox.x + gbox.width / 2 + 80, gbox.y + gbox.height / 2 + 10, { steps: 5 });
  await guest.mouse.up();
  assert((await guest.evaluate(() => window.nymphApp.app.collab.following)) === false, 'following stopped');
  await guest.waitForFunction(() => !window.nymphApp.app.viewer.wantsFrame(), null, { timeout: 10000 });
  const guestView = await guest.evaluate(() => window.nymphApp.app.viewer.toYaml().split('\nview:')[1]);
  // The owner changes the chart: the guest gets it, and keeps its own view.
  const yaml = await owner.evaluate(() => window.nymphApp.app.viewer.toYaml());
  await run(owner, 'document_set_source', { source: yaml.replace(/title: ([^\n]*)/, 'title: Still mine') });
  await guest.waitForFunction(() => window.nymphApp.app.doc.source.includes('Still mine'), null, { timeout: 10000 });
  await guest.waitForFunction(() => !window.nymphApp.app.viewer.wantsFrame(), null, { timeout: 10000 });
  const after = await guest.evaluate(() => window.nymphApp.app.viewer.toYaml().split('\nview:')[1]);
  assert(after === guestView, 'the guest\'s view stayed');
  await done(owner, guest);
});

test('a source that does not load keeps the guest\'s last chart', async () => {
  const { owner, guest } = await live();
  const before = await guestSource(guest);
  await owner.evaluate(() => window.nymphApp.app.collab.session.channel('doc').send({ t: 'doc', v: 1000, source: 'root:\n  type: NoSuchElement\n' }));
  await guest.waitForFunction(() => /error; showing the last version/.test(document.getElementById('banner').textContent), null, { timeout: 10000 });
  assert((await guestSource(guest)) === before, 'last good chart kept');
  await done(owner, guest);
});

test('drawings reach everyone; the PNG with drawings shows them and still opens as the chart', async () => {
  const { owner, guest } = await live();
  await owner.evaluate(() => window.nymphApp.app.collab.wb.setTool('pen'));
  const box = await owner.locator('#stage canvas').boundingBox();
  const x = box.x + box.width * 0.3;
  const y = box.y + box.height * 0.4;
  await owner.mouse.move(x, y);
  await owner.mouse.down();
  for (let i = 1; i <= 15; ++i) {
    await owner.mouse.move(x + i * 8, y + i * 3);
  }
  await owner.mouse.up();
  await guest.waitForFunction(() => window.nymphApp.app.collab.wb.items().length === 1, null, { timeout: 10000 });
  // The guest's download: pixels under the stroke differ from the plain PNG.
  const download = guest.waitForEvent('download');
  await run(guest, 'collab_download_png');
  const file = await download;
  assert(/-with-drawings\.png$/.test(file.suggestedFilename()), `file name ${file.suggestedFilename()}`);
  const bytes = await readFile(await file.path());
  const check = await guest.evaluate(async (data) => {
    const drawn = new Uint8Array(data);
    const plain = window.nymphApp.app.viewer.toPng();
    const pixels = async (png) => {
      const b = await createImageBitmap(new Blob([png], { type: 'image/png' }));
      const c = new OffscreenCanvas(b.width, b.height);
      const ctx = c.getContext('2d');
      ctx.drawImage(b, 0, 0);
      return { w: b.width, h: b.height, d: ctx.getImageData(0, 0, b.width, b.height).data };
    };
    const [p, q] = [await pixels(plain), await pixels(drawn)];
    let diff = 0;
    for (let i = 0; i < p.d.length; i += 4) {
      if (Math.abs(p.d[i] - q.d[i]) + Math.abs(p.d[i + 1] - q.d[i + 1]) + Math.abs(p.d[i + 2] - q.d[i + 2]) > 60) {
        diff++;
      }
    }
    const { pngSource } = await import('./app/open.js');
    const source = await pngSource(drawn);
    return { same: p.w === q.w && p.h === q.h, diff, source };
  }, [...bytes]);
  assert(check.same, 'same size as the chart PNG');
  assert(check.diff > 50, `${check.diff} pixels changed`);
  assert(check.source && check.source === (await guestSource(guest)), 'reopens as the chart, without the drawings');
  await done(owner, guest);
});

test('Only I can draw: guests\' whiteboards lock and a forged operation is dropped', async () => {
  const { owner, guest } = await live();
  await owner.evaluate(() => window.nymphApp.app.collab.setDrawingLocked(true));
  await guest.waitForFunction(() => window.nymphApp.app.collab.wb.readOnly === true, null, { timeout: 10000 });
  await guest.evaluate(() => {
    const wb = window.nymphApp.app.collab.wb;
    const op = wb.model.add({ kind: 'sticker', x: 10, y: 10, size: 30, name: 'star' });
    window.nymphApp.app.collab.session.channel('wb').send({ t: 'op', op });
  });
  await new Promise((r) => setTimeout(r, 800));
  assert((await owner.evaluate(() => window.nymphApp.app.collab.wb.items().length)) === 0, 'forged op dropped');
  await done(owner, guest);
});

test('the owner removes a guest; a new link keeps the people already in', async () => {
  const { owner, guest } = await live();
  const second = await openApp('Cleo', (await owner.evaluate(() => window.nymphApp.app.collab.inviteLink())).slice(site.base.length));
  await second.waitForFunction(() => window.nymphApp.app.doc?.origin.kind === 'collab', null, { timeout: 40000 });
  const oldLink = await owner.evaluate(() => window.nymphApp.app.collab.inviteLink());
  await owner.evaluate(() => window.nymphApp.app.collab.session.newInvite());
  const newLink = await owner.evaluate(() => window.nymphApp.app.collab.inviteLink());
  assert(newLink !== oldLink, 'a new link');
  const id = await guest.evaluate(() => window.nymphApp.app.collab.session.me.id);
  await owner.evaluate((g) => window.nymphApp.app.collab.session.remove(g), id);
  await guest.waitForFunction(() => /removed you/.test(document.getElementById('banner').textContent), null, { timeout: 10000 });
  await second.waitForFunction(() => window.nymphApp.app.collab.session.state === 'open');
  const people = await owner.evaluate(() => window.nymphApp.app.collab.session.peers().length);
  assert(people === 2, `${people} people left`);
  await done(owner, guest, second);
});

test('closing the owner\'s chart asks, then ends the session for the guests', async () => {
  const { owner, guest } = await live();
  owner.evaluate(() => window.nymphApp.commands.run('app_close'));
  await owner.click('dialog button:text-is("End the session")');
  await guest.waitForFunction(() => /ended the session/.test(document.getElementById('banner').textContent), null, { timeout: 10000 });
  assert((await guest.evaluate(() => window.nymphApp.app.collab.wb.readOnly)) === true, 'whiteboard read-only');
  assert((await guestSource(guest)) !== false, 'the last chart stays');
  await owner.waitForFunction(() => !window.nymphApp.app.doc);
  await done(owner, guest);
});

test('an owner\'s reload resumes the room and the guest reconnects, with the drawings', async () => {
  const { owner, guest } = await live();
  await owner.evaluate(() => {
    const wb = window.nymphApp.app.collab.wb;
    wb.local(() => wb.model.add({ kind: 'text', x: 20, y: 20, size: 18, text: 'kept', color: '#1c2024' }));
  });
  await guest.waitForFunction(() => window.nymphApp.app.collab.wb.items().length === 1, null, { timeout: 10000 });
  await new Promise((r) => setTimeout(r, 500)); // the resume record is written
  await owner.reload();
  await owner.waitForFunction(() => document.documentElement.dataset.ready === 'true');
  await shown(owner);
  await owner.waitForFunction(() => window.nymphApp.app.collab.session?.state === 'open', null, { timeout: 10000 });
  await guest.waitForFunction(() => window.nymphApp.app.collab.session.state === 'open' && window.nymphApp.app.collab.session.peers().length === 2, null, { timeout: 40000 });
  const ownerItems = await owner.evaluate(() => window.nymphApp.app.collab.wb.items().map((i) => i.text).join());
  assert(ownerItems === 'kept', `owner's drawings after reload: ${ownerItems}`);
  await done(owner, guest);
});

test('in a session the owner\'s editor has the guests\' layout: drawings land on the same spot', async () => {
  const { owner, guest } = await live('flowchart');
  await run(owner, 'app_set_mode', { mode: 'edit' });
  await shown(owner);
  const size = (page) => page.evaluate(() => {
    const b = window.nymphApp.app.collab.h.module().figureBox();
    return `${Math.round(b.width / b.scale)}x${Math.round(b.height / b.scale)}`;
  });
  const [o, g] = [await size(owner), await size(guest)];
  assert(o === g, `owner ${o}, guest ${g}`);
  // Drawn from the top of the figure box, where clicks and the overlay
  // expect it: the band at the top holds the chart's title, not background.
  await owner.waitForTimeout(500);
  const fb = await owner.evaluate(() => {
    const e = window.nymphApp.app.editor;
    const b = e.figureBox();
    const r = e.container.getBoundingClientRect();
    return { x: r.left + b.left, y: r.top + b.top, w: b.width, h: b.height, area: r.bottom - (r.top + b.top) };
  });
  assert(fb.area > fb.h + 20, `the area is taller than the chart (${fb.area} > ${fb.h})`);
  const shot = await owner.screenshot({ clip: { x: fb.x, y: fb.y, width: fb.w, height: 40 } });
  const dark = await owner.evaluate(async (bytes) => {
    const b = await createImageBitmap(new Blob([new Uint8Array(bytes)], { type: 'image/png' }));
    const c = new OffscreenCanvas(b.width, b.height);
    const ctx = c.getContext('2d');
    ctx.drawImage(b, 0, 0);
    const d = ctx.getImageData(0, 0, b.width, b.height).data;
    let n = 0;
    for (let i = 0; i < d.length; i += 4) {
      n += d[i] + d[i + 1] + d[i + 2] < 250 ? 1 : 0;
    }
    return n;
  }, [...shot]);
  assert(dark > 30, `${dark} dark pixels at the top of the figure`);
  // Leaving the session gives the editor its whole area again.
  await run(owner, 'collab_stop');
  await owner.waitForFunction((was) => {
    const b = window.nymphApp.app.editor.figureBox();
    return `${Math.round(b.width / b.scale)}x${Math.round(b.height / b.scale)}` !== was;
  }, o, { timeout: 10000 });
  await done(owner, guest);
});

test('typing a note in View mode: "e" and "?" stay in the note', async () => {
  const { owner, guest } = await live();
  await guest.evaluate(() => window.nymphApp.app.collab.wb.setTool('text'));
  const box = await guest.locator('#stage canvas').boundingBox();
  await guest.mouse.click(box.x + 60, box.y + 60);
  await guest.keyboard.type('see me? yes');
  await guest.keyboard.press('Enter');
  await owner.waitForFunction(() => window.nymphApp.app.collab.wb.items().some((i) => i.text === 'see me? yes'), null, { timeout: 10000 });
  assert((await guest.evaluate(() => window.nymphApp.app.mode)) === 'view', 'still in View');
  assert((await guest.locator('dialog').count()) === 0, 'no shortcuts dialog');
  await done(owner, guest);
});

test('agents see collab_state only; the panel shows people and paths', async () => {
  const { owner, guest } = await live();
  const tools = await owner.evaluate(() => window.nymphApp.commands.agentTools().map((t) => t.name).filter((n) => n.startsWith('collab_')));
  assert(tools.join() === 'collab_state', `agent tools ${tools}`);
  await owner.waitForFunction(() => document.getElementById('collab-dock').textContent.includes('Ben'), null, { timeout: 10000 });
  await owner.waitForFunction(() => /direct/.test(document.getElementById('collab-dock').textContent), null, { timeout: 15000 });
  assert(/Live · 2/.test(await owner.textContent('#btn-collab')), 'the bar shows the session');
  await done(owner, guest);
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
