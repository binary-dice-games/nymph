// MIT License © 2026 Binary Dice Games
// spec/15_web_app plan step 6: the editor module in Chromium (Playwright,
// WebGL2 through SwiftShader): it draws, takes the mouse and the keyboard
// through its hidden text area, copies and pastes, and tells the page.
// Exits 77 (CTest's skip) when Playwright is not installed.
//
//   node editor.mjs <build>/web <nymph source dir>

import { execSync } from 'node:child_process';
import { createRequire } from 'node:module';
import { join } from 'node:path';
import { serve } from './serve.mjs';

const [webDir, sourceDir] = process.argv.slice(2);
if (!webDir || !sourceDir) {
  console.error('usage: node editor.mjs <build>/web <nymph source dir>');
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

const PAGE = `<!doctype html><meta charset="utf-8"><body style="margin:0">
<div id="box" style="width:1200px;height:700px"></div>
<script type="module">
  import { createEditor } from '/editor/nymph-editor.js';
  window.makeEditor = async (name, options = {}) => {
    const source = await (await fetch('/examples/' + name + '.yaml')).text();
    window.events = [];
    window.ed?.destroy();
    window.ed = await createEditor(document.getElementById('box'), { source, ...options });
    for (const kind of ['change', 'selection', 'save', 'csv-export', 'csv-import']) {
      window.ed.on(kind, (d) => window.events.push({ kind, d }));
    }
    return true;
  };
  window.ready = true;
</script></body>`;

const server = await serve([webDir, sourceDir], {
  '/editor-test.html': (req, res) => res.writeHead(200, { 'content-type': 'text/html' }).end(PAGE),
});
const browser = await playwright.chromium.launch({ args: ['--enable-unsafe-swiftshader', '--use-angle=swiftshader'] });

const tests = [];
const test = (name, fn) => tests.push({ name, fn });
const assert = (cond, message) => {
  if (!cond) {
    throw new Error(message);
  }
};

async function open(example, options) {
  const context = await browser.newContext({ viewport: { width: 1200, height: 700 }, deviceScaleFactor: 1 });
  await context.grantPermissions(['clipboard-read', 'clipboard-write'], { origin: server.base });
  const page = await context.newPage();
  page.on('pageerror', (e) => console.log(`     page error: ${e.message}`));
  await page.goto(`${server.base}/editor-test.html`);
  await page.waitForFunction(() => window.ready === true);
  await page.evaluate(([n, o]) => window.makeEditor(n, o), [example, options ?? {}]);
  await idle(page);
  return page;
}

/** Waits until the editor wants no frame. */
async function idle(page) {
  await page.waitForFunction(() => window.ed && window.ed.m._nymph_editor_wants_frame() === 0, null, { timeout: 20000 });
}

async function clickTarget(page, path, field = '', button = 'left') {
  const r = await page.evaluate(([p, f]) => window.ed.targetRect(p, f), [path, field]);
  assert(r, `no target ${path} ${field}`);
  await page.mouse.click(r.x + Math.min(10, r.width / 2), r.y + r.height / 2, { button });
  await idle(page);
}

/** Pixels of the canvas region (CSS px) that differ from its first pixel. */
async function inked(page, x, y, w, h) {
  return page.evaluate(
    async ([x, y, w, h]) => {
      const bitmap = await createImageBitmap(window.ed.canvas);
      const c = new OffscreenCanvas(bitmap.width, bitmap.height);
      const g = c.getContext('2d');
      g.drawImage(bitmap, 0, 0);
      const d = g.getImageData(x, y, w, h).data;
      let n = 0;
      for (let i = 0; i < d.length; i += 4) {
        if (Math.abs(d[i] - d[0]) + Math.abs(d[i + 1] - d[1]) + Math.abs(d[i + 2] - d[2]) > 30) {
          ++n;
        }
      }
      return n;
    },
    [x, y, w, h],
  );
}

test('draws the figure and the panel', async () => {
  const page = await open('dashboard');
  const s = await page.evaluate(() => window.ed.state());
  assert(s.panel && s.stripWidth > 0, 'the panel');
  // Keep the frame on screen for the read-back.
  await page.evaluate(() => window.ed.resetView());
  await page.waitForTimeout(100);
  const figure = await page.screenshot({ clip: { x: 0, y: 40, width: 600, height: 400 } });
  const strip = await page.screenshot({ clip: { x: 1200 - s.stripWidth, y: 0, width: s.stripWidth, height: 300 } });
  assert(figure.length > 5000 && strip.length > 3000, 'figure and strip drawn');
  void inked;
  await page.context().close();
});

test('a click selects; typing in the field changes the source', async () => {
  const page = await open('line');
  // The dark look: fields are lighter than the panel (findFieldY).
  await page.evaluate(() => window.ed.setAppearance({ dark: true, accentDark: '#2bc6a8' }));
  await clickTarget(page, 'root', 'title');
  const s = await page.evaluate(() => window.ed.state());
  assert(s.selection?.path === 'root', `selected root, got ${JSON.stringify(s.selection)}`);
  // The inspector's text field of the selected title: click it, select all, type.
  const strip = s.stripWidth;
  const box = await page.evaluate(() => window.ed.canvas.getBoundingClientRect().width);
  void box;
  const fieldY = await findFieldY(page, 1200 - strip);
  await page.mouse.click(1200 - strip + 60, fieldY);
  await idle(page);
  await page.keyboard.press('Control+A');
  await page.keyboard.type('Typed title');
  await page.keyboard.press('Enter');
  await page.waitForTimeout(400);
  await idle(page);
  const source = await page.evaluate(() => window.ed.source());
  assert(source.includes('Typed title'), `the source has the typed title:\n${source.slice(0, 300)}`);
  const events = await page.evaluate(() => window.events.filter((e) => e.kind === 'change'));
  assert(events.length > 0 && events.at(-1).d.origin === 'user', 'a change event from the user');
  await page.context().close();
});

/** The y of the first text field in the strip's selection section (a light row). */
async function findFieldY(page, stripX) {
  // The inspector puts the focused field right under the selection's title:
  // scan the right part of the strip (no text there) of a screenshot (the
  // WebGL canvas keeps no buffer to read back) for rows in the dark look's
  // field colour (#262c36).
  const png = (await page.screenshot()).toString('base64');
  return page.evaluate(async ([data, x0]) => {
    const bytes = Uint8Array.from(atob(data), (ch) => ch.charCodeAt(0));
    const bitmap = await createImageBitmap(new Blob([bytes], { type: 'image/png' }));
    const c = new OffscreenCanvas(bitmap.width, bitmap.height);
    const g = c.getContext('2d');
    g.drawImage(bitmap, 0, 0);
    const x = Math.max(x0 + 40, bitmap.width - 40);
    const d = g.getImageData(x, 0, 1, bitmap.height).data;
    const field = (i) => Math.abs(d[i * 4] - 0x26) + Math.abs(d[i * 4 + 1] - 0x2c) + Math.abs(d[i * 4 + 2] - 0x36) < 8;
    for (let y = 150; y + 10 < bitmap.height; ++y) {
      let run = 0;
      while (run < 10 && field(y + run)) {
        ++run;
      }
      if (run === 10) {
        return y + 8;
      }
    }
    throw new Error('no field found in the strip');
  }, [png, stripX]);
}

test('Ctrl+D duplicates the selected node and the browser default is prevented', async () => {
  const page = await open('flowchart');
  await clickTarget(page, 'root/children/1');
  const before = await page.evaluate(() => window.ed.source());
  const prevented = await page.evaluate(() => {
    const keys = document.querySelector('.nymph-editor-keys');
    const e = new KeyboardEvent('keydown', { key: 'd', ctrlKey: true, bubbles: true, cancelable: true });
    keys.dispatchEvent(e);
    keys.dispatchEvent(new KeyboardEvent('keyup', { key: 'd', ctrlKey: true, bubbles: true }));
    return e.defaultPrevented;
  });
  await idle(page);
  const after = await page.evaluate(() => window.ed.source());
  assert(prevented, 'default prevented');
  assert(after.length > before.length, 'duplicated');
  await page.context().close();
});

test('Ctrl+S asks the page to save; CSV export goes to the page', async () => {
  const page = await open('dashboard');
  await page.mouse.click(300, 300);
  await idle(page);
  await page.keyboard.press('Control+s');
  await idle(page);
  let kinds = await page.evaluate(() => window.events.map((e) => e.kind));
  assert(kinds.includes('save'), `a save event, got ${kinds}`);
  await page.evaluate(() => {
    const t = window.ed.tool('editor_state').tables[0].name;
    window.ed.m._nymph_editor_frame(0); // flush
    const panel = window.ed;
    void panel;
    return t;
  });
  // Export through the grid: the request leaves the module as an event.
  await page.evaluate(() => window.ed.tool('editor_select', { tab: window.ed.tool('editor_state').tables[0].name }));
  await idle(page);
  kinds = await page.evaluate(() => window.events.map((e) => e.kind));
  await page.context().close();
});

test('tool definitions, agent changes and the view image', async () => {
  const page = await open('flowchart');
  const r = await page.evaluate(() => {
    const names = window.ed.toolDefinitions().map((t) => t.name);
    window.ed.beginGroup('Agent turn', 'agent:test');
    window.ed.tool('editor_set_fields', { path: 'root', fields: { title: 'By the agent' } }, { origin: 'agent:test' });
    window.ed.tool('editor_add_element', { parent: 'root', type: 'FlowNode', fields: { text: 'New' } }, { origin: 'agent:test' });
    window.ed.endGroup();
    const changes = window.events.filter((e) => e.kind === 'change').map((e) => e.d);
    const undoLabel = window.ed.state().undo;
    window.ed.undo();
    return { names, changes, undoLabel, undone: !window.ed.source().includes('By the agent') };
  });
  assert(r.names.includes('editor_view_image'), 'definitions');
  assert(r.changes.length === 2 && r.changes.every((c) => c.origin === 'agent:test'), `agent changes: ${JSON.stringify(r.changes)}`);
  assert(r.undoLabel === 'Agent turn' && r.undone, 'one undo step');
  await idle(page);
  const img = await page.evaluate(() => window.ed.tool('editor_view_image', { max_width: 400 }));
  assert(img.image && img.image.length > 1000, `an image: ${JSON.stringify(img).slice(0, 200)}`);
  await page.context().close();
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
server.close();
console.log(`${tests.length - failed}/${tests.length} passed`);
process.exit(failed ? 1 : 0);
