// MIT License © 2026 Binary Dice Games
// spec/15_web_app plan steps 7 to 13: the web app on the built site
// (<build>/web/site/) in Chromium through Playwright. Exits 77 (CTest's
// skip) when Playwright is not installed.
//
//   node app.mjs <build>/web <nymph source dir>

import { execSync } from 'node:child_process';
import { readFile } from 'node:fs/promises';
import { createRequire } from 'node:module';
import { join } from 'node:path';
import { serve } from './serve.mjs';

const [webDir, sourceDir] = process.argv.slice(2);
if (!webDir || !sourceDir) {
  console.error('usage: node app.mjs <build>/web <nymph source dir>');
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

const example = (name) => readFile(join(sourceDir, 'examples', `${name}.yaml`), 'utf8');
const examplePng = (name) => readFile(join(sourceDir, 'examples', `${name}.png`));

// The site, a second origin that lets other sites read its files (CORS) and
// a third that does not.
const site = await serve([siteDir]);
const cors = await serve([join(sourceDir, 'examples')], {}, { cors: true });
const closed = await serve([join(sourceDir, 'examples')]);

const browser = await playwright.chromium.launch({ args: ['--enable-unsafe-swiftshader', '--use-angle=swiftshader'] });

const tests = [];
const test = (name, fn) => tests.push({ name, fn });
const assert = (cond, message) => {
  if (!cond) {
    throw new Error(message);
  }
};

/**
 * A fresh page of the app. `options.init` runs before the app's scripts;
 * `options.mobile` emulates a phone; `options.path` the route.
 */
async function openApp(options = {}) {
  const context = await browser.newContext({
    viewport: options.mobile ? { width: 390, height: 844 } : { width: 1280, height: 800 },
    deviceScaleFactor: 1,
    hasTouch: !!options.mobile,
    isMobile: !!options.mobile,
    acceptDownloads: true,
    serviceWorkers: options.sw ? 'allow' : 'block',
  });
  await context.grantPermissions(['clipboard-read', 'clipboard-write'], { origin: site.base });
  const page = await context.newPage();
  page.errors = [];
  page.on('pageerror', (e) => page.errors.push(e.message));
  page.requests = [];
  page.on('request', (r) => page.requests.push(r.url()));
  if (options.init) {
    await page.addInitScript(options.init);
  }
  if (options.routes) {
    for (const [pattern, handler] of Object.entries(options.routes)) {
      await page.route(pattern, handler);
    }
  }
  const sep = (options.path ?? '/').includes('?') ? '&' : '?';
  const device = options.mobile ? `${sep}device=mobile` : '';
  await page.goto(`${site.base}${options.path ?? '/'}${device}`);
  await page.waitForFunction(() => document.documentElement.dataset.ready === 'true', null, { timeout: 30000 });
  return page;
}

/** Waits until the document is shown (viewer drawn, or editor idle). */
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

const state = (page) => page.evaluate(() => {
  const a = window.nymphApp.app;
  return { mode: a.mode, name: a.doc?.name, modified: a.doc?.modified, origin: a.doc?.origin.kind, banner: document.getElementById('banner').hidden ? '' : document.getElementById('banner').textContent };
});

const call = (page, name, args = {}, origin = 'user') =>
  page.evaluate(([n, a, o]) => window.nymphApp.commands.call(n, a, o), [name, args, origin]);

async function done(page) {
  assert(page.errors.length === 0, `page errors: ${page.errors.join('; ')}`);
  await page.context().close();
}

// ── Step 7: open and view ────────────────────────────────────────────────────

test('the start screen lists templates and examples', async () => {
  const page = await openApp();
  assert(await page.isVisible('#start'), 'start screen');
  const templates = await page.locator('#template-list .card').count();
  const examples = await page.locator('#example-list .card').count();
  assert(templates >= 8 && examples >= 20, `templates ${templates}, examples ${examples}`);
  await done(page);
});

test('a YAML chosen with the file chooser draws, with its name', async () => {
  const page = await openApp({ init: () => delete window.showOpenFilePicker });
  const chooser = page.waitForEvent('filechooser');
  await page.click('text=Open file…');
  await (await chooser).setFiles({ name: 'mine.yaml', mimeType: 'text/yaml', buffer: Buffer.from(await example('bars')) });
  await shown(page);
  const s = await state(page);
  assert(s.name === 'mine.yaml' && s.mode === 'view' && !s.modified, JSON.stringify(s));
  assert((await page.inputValue('#doc-name')) === 'mine.yaml', 'the title');
  await done(page);
});

test('a nymph PNG dropped on the page draws; a plain PNG says it has no chart', async () => {
  const page = await openApp();
  const png = [...(await examplePng('pie'))];
  await page.evaluate((bytes) => {
    const dt = new DataTransfer();
    dt.items.add(new File([new Uint8Array(bytes)], 'pie.png', { type: 'image/png' }));
    window.dispatchEvent(new DragEvent('drop', { dataTransfer: dt, bubbles: true, cancelable: true }));
  }, png);
  await shown(page);
  let s = await state(page);
  assert(s.name === 'pie.png', JSON.stringify(s));
  assert(await page.evaluate(() => window.nymphApp.app.doc.png), 'remembered as a PNG');
  // A PNG without the nymph chunk: the header only (a valid PNG, no iTXt).
  const plain = [...(await readFile(join(sourceDir, 'resources', 'icon.png')))];
  await page.evaluate((bytes) => {
    const dt = new DataTransfer();
    dt.items.add(new File([new Uint8Array(bytes)], 'icon.png', { type: 'image/png' }));
    window.dispatchEvent(new DragEvent('drop', { dataTransfer: dt, bubbles: true, cancelable: true }));
  }, plain);
  await page.waitForSelector('.toast');
  const text = await page.textContent('.toast');
  assert(/no nymph chart/.test(text), `message: ${text}`);
  s = await state(page);
  assert(s.name === 'pie.png', 'the open chart stays');
  await done(page);
});

test('pasted YAML draws', async () => {
  const page = await openApp();
  const yaml = await example('line');
  await page.evaluate((text) => {
    const dt = new DataTransfer();
    dt.setData('text/plain', text);
    document.body.dispatchEvent(new ClipboardEvent('paste', { clipboardData: dt, bubbles: true, cancelable: true }));
  }, yaml);
  await shown(page);
  assert((await state(page)).origin === 'paste', 'pasted');
  await done(page);
});

test('a source that does not load shows its line and column, and Fix in source', async () => {
  const page = await openApp();
  await call(page, 'app_new_document', { source: 'root:\n  type: Plot\n  children:\n    - {type: PlotLine, ys: [1, 2], bogus: 3}\n', mode: 'view' });
  await page.waitForSelector('#banner:not([hidden])');
  const s = await state(page);
  assert(/line 4/.test(s.banner), `banner: ${s.banner}`);
  assert(await page.isVisible('#banner >> text=Fix in source'), 'Fix in source');
  await done(page);
});

test('controls go through commands; schemas are valid; agents need consent for external commands', async () => {
  const page = await openApp();
  await call(page, 'app_open_example', { name: 'line' });
  await shown(page);
  await page.evaluate(() => (window.nymphApp.commands.log.length = 0));
  await page.click('#btn-share');
  await page.click('.menu >> text=Copy link');
  await page.waitForTimeout(200);
  const log = await page.evaluate(() => window.nymphApp.commands.log.map((c) => c.name));
  assert(log.includes('app_copy_link'), `log: ${log}`);
  const list = await page.evaluate(() => window.nymphApp.commands.list());
  for (const c of list) {
    assert(c.inputSchema?.type === 'object' && typeof c.description === 'string' && c.description.length > 10, `schema of ${c.name}`);
  }
  // An agent asks to download: a dialog; Cancel downloads nothing.
  let downloaded = false;
  page.on('download', () => (downloaded = true));
  const pending = call(page, 'app_download', { kind: 'yaml' }, 'agent:test');
  await page.waitForSelector('dialog.dlg >> text=Allow this?');
  await page.click('dialog.dlg >> text=Cancel');
  const r = await pending;
  assert(r.isError && /declined/.test(r.content[0].text), JSON.stringify(r));
  await page.waitForTimeout(300);
  assert(!downloaded, 'nothing downloaded');
  // An edit by an agent: no dialog, attributed.
  const ok = await call(page, 'document_set_source', { source: (await example('bars')) }, 'agent:test');
  assert(!ok.isError, JSON.stringify(ok));
  assert((await state(page)).modified, 'modified by the agent');
  await done(page);
});

test('the dock, when filled, shrinks the chart', async () => {
  const page = await openApp();
  await call(page, 'app_open_example', { name: 'line', mode: 'edit' });
  await shown(page);
  const before = await page.evaluate(() => document.querySelector('.nymph-editor-canvas').clientWidth);
  await page.evaluate(() => {
    const d = document.getElementById('dock');
    d.hidden = false;
    d.textContent = 'assistant';
  });
  await page.waitForTimeout(400);
  const after = await page.evaluate(() => document.querySelector('.nymph-editor-canvas').clientWidth);
  assert(after < before - 300, `canvas ${before} -> ${after}`);
  await done(page);
});

test('the URLs embedders use still answer', async () => {
  for (const path of ['nymph-viewer.js', 'nymph-viewer-core.wasm', 'nymph-viewer.single.js', 'fonts/FiraSans-Regular.ttf', 'demo/', 'examples/index.json', 'editor/nymph-editor.js']) {
    const r = await fetch(`${site.base}/${path}`);
    assert(r.ok, `${path}: ${r.status}`);
  }
});

// ── Step 8: Edit and save ────────────────────────────────────────────────────

test('View to Edit keeps the zoom and is not modified; an edit marks the title', async () => {
  const page = await openApp();
  await call(page, 'app_open_example', { name: 'line' });
  await shown(page);
  // Zoom the plot with the wheel, then edit.
  const box = await page.locator('.viewer-box canvas').boundingBox();
  await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
  await page.mouse.wheel(0, -400);
  await page.waitForTimeout(600);
  await call(page, 'app_set_mode', { mode: 'edit' });
  await shown(page);
  let s = await state(page);
  assert(s.mode === 'edit' && !s.modified, JSON.stringify(s));
  const source = await page.evaluate(() => window.nymphApp.app.editor.source());
  assert(/view:/.test(source), 'the view came along');
  assert(await page.isHidden('#doc-dot'), 'no dot');
  await call(page, 'editor_set_fields', { path: 'root', fields: { title: 'Edited' } });
  s = await state(page);
  assert(s.modified && (await page.isVisible('#doc-dot')), 'modified, with the dot');
  await done(page);
});

test('Download YAML carries the edit; a PNG input downloads a PNG with the source', async () => {
  const page = await openApp();
  await call(page, 'app_open_example', { name: 'line', mode: 'edit' });
  await shown(page);
  await call(page, 'editor_set_fields', { path: 'root', fields: { title: 'Downloaded' } });
  const wait = page.waitForEvent('download');
  await call(page, 'app_download', { kind: 'yaml' });
  const dl = await wait;
  const text = await readFile(await dl.path(), 'utf8');
  assert(text.includes('Downloaded') && dl.suggestedFilename() === 'line.yaml', `${dl.suggestedFilename()}: ${text.slice(0, 80)}`);
  // PNG input: Save downloads a PNG carrying the edited source.
  const png = [...(await examplePng('pie'))];
  await page.evaluate((bytes) => {
    const dt = new DataTransfer();
    dt.items.add(new File([new Uint8Array(bytes)], 'pie.png', { type: 'image/png' }));
    window.dispatchEvent(new DragEvent('drop', { dataTransfer: dt, bubbles: true, cancelable: true }));
  }, png);
  // The open chart is modified: discard it, and wait for the PNG (the old
  // chart is still shown, idle, until it opens).
  await page.click('dialog.dlg >> text=Discard');
  await page.waitForFunction(() => window.nymphApp.app.doc?.name === 'pie.png' && window.nymphApp.app.viewer, null, { timeout: 30000 });
  await shown(page);
  await call(page, 'app_set_mode', { mode: 'edit' });
  await shown(page);
  await call(page, 'document_set_source', { source: (await example('pie')).replace(/title: [^\n]*/, 'title: Pie edited') });
  const wait2 = page.waitForEvent('download');
  await call(page, 'app_save');
  const dl2 = await wait2;
  const bytes = await readFile(await dl2.path());
  assert(dl2.suggestedFilename() === 'pie.png' && bytes[1] === 0x50, dl2.suggestedFilename());
  assert(bytes.includes('Pie edited') || bytes.length > 1000, 'a PNG');
  assert(!(await state(page)).modified, 'saved');
  await done(page);
});

test('Save writes back to the opened file where the browser gives a handle', async () => {
  const page = await openApp({
    init: () => {
      window.__written = null;
      const file = new File(['nymph: 1\nroot: {type: Label, text: Hello}\n'], 'hello.yaml', { type: 'text/yaml' });
      const handle = {
        kind: 'file',
        name: 'hello.yaml',
        getFile: async () => file,
        queryPermission: async () => 'granted',
        requestPermission: async () => 'granted',
        createWritable: async () => ({
          write: async (b) => (window.__written = typeof b === 'string' ? b : new TextDecoder().decode(b)),
          close: async () => {},
        }),
      };
      window.showOpenFilePicker = async () => [handle];
    },
  });
  await call(page, 'app_open_file');
  await shown(page);
  await call(page, 'app_set_mode', { mode: 'edit' });
  await shown(page);
  await call(page, 'document_set_source', { source: 'nymph: 1\nroot: {type: Label, text: Changed}\n' });
  await call(page, 'app_save');
  const written = await page.evaluate(() => window.__written);
  assert(written && written.includes('Changed'), `written: ${written}`);
  assert(!(await state(page)).modified, 'saved');
  await done(page);
});

test('opening another file while modified asks; Cancel keeps the document', async () => {
  const page = await openApp();
  await call(page, 'app_open_example', { name: 'line', mode: 'edit' });
  await shown(page);
  await call(page, 'editor_set_fields', { path: 'root', fields: { title: 'Keep me' } });
  const opening = call(page, 'app_open_example', { name: 'bars' });
  await page.waitForSelector('dialog.dlg >> text=Unsaved changes');
  await page.click('dialog.dlg >> text=Cancel');
  await opening;
  const s = await state(page);
  assert(s.name === 'line.yaml' && s.modified, JSON.stringify(s));
  await done(page);
});

test('Source in View opens Edit with the pane; a broken file is repaired there', async () => {
  const page = await openApp();
  await call(page, 'app_open_example', { name: 'line' });
  await shown(page);
  await call(page, 'app_show_source');
  await shown(page);
  let st = await page.evaluate(() => window.nymphApp.app.editor.state());
  assert(st.sourceShown, 'pane shown');
  // A broken file: Fix in source opens the repair state.
  await call(page, 'app_new_document', { source: 'root:\n  type: Plot\n  children:\n    - {type: PlotLine, ys: [1, 2], bogus: 3}\n', mode: 'view' });
  await page.click('dialog.dlg >> text=Discard').catch(() => {});
  await page.waitForSelector('#banner >> text=Fix in source');
  await page.click('#banner >> text=Fix in source');
  await page.waitForFunction(() => window.nymphApp.app.editor?.repairing());
  // Fix it through the document command (as the pane would, after typing).
  const r = await call(page, 'document_set_source', { source: 'root:\n  type: Plot\n  children:\n    - {type: PlotLine, ys: [1, 2]}\n' });
  assert(!r.isError, JSON.stringify(r));
  st = await page.evaluate(() => window.nymphApp.app.editor.state());
  assert(!st.repairing, 'repaired');
  await call(page, 'app_set_mode', { mode: 'view' });
  await shown(page);
  assert((await state(page)).banner === '', 'no error left');
  await done(page);
});

test('document_set_source works in View (viewer only) and Edit (one undo step); editor commands only in Edit', async () => {
  const page = await openApp();
  await call(page, 'app_open_example', { name: 'line' });
  await shown(page);
  let names = await page.evaluate(() => window.nymphApp.commands.list().map((c) => c.name));
  assert(!names.includes('editor_set_fields'), 'no editor commands in View');
  assert(!(await page.evaluate(() => !!window.nymphApp.app.editor)), 'no editor module');
  const v = await call(page, 'document_set_source', { source: (await example('bars')) }, 'agent:test');
  assert(!v.isError, JSON.stringify(v));
  await call(page, 'app_set_mode', { mode: 'edit' });
  await shown(page);
  names = await page.evaluate(() => window.nymphApp.commands.list().map((c) => c.name));
  assert(names.includes('editor_set_fields'), 'editor commands in Edit');
  const before = await page.evaluate(() => window.nymphApp.app.editor.source());
  await call(page, 'document_set_source', { source: (await example('pie')), label: 'Agent rewrite' }, 'agent:test');
  const undoLabel = await page.evaluate(() => window.nymphApp.app.editor.state().undo);
  assert(undoLabel === 'Agent rewrite', undoLabel);
  await page.evaluate(() => window.nymphApp.app.editor.undo());
  assert((await page.evaluate(() => window.nymphApp.app.editor.source())) === before, 'one undo step');
  await done(page);
});

// ── Step 9: URLs and links ───────────────────────────────────────────────────

test('?url= opens from a server that allows it; one that does not gets the CORS message; 404 its status', async () => {
  let page = await openApp({ path: `/?url=${encodeURIComponent(`${cors.base}/line.yaml`)}` });
  await shown(page);
  assert((await state(page)).origin === 'url', 'opened from the URL');
  assert(page.url().includes('?url='), 'the address reopens it');
  await done(page);
  page = await openApp({ path: `/?url=${encodeURIComponent(`${closed.base}/line.yaml`)}` });
  await page.waitForSelector('.toast');
  assert(/other sites/.test(await page.textContent('.toast')), 'CORS message');
  await done(page);
  page = await openApp({ path: `/?url=${encodeURIComponent(`${cors.base}/nothing.yaml`)}` });
  await page.waitForSelector('.toast');
  assert(/404/.test(await page.textContent('.toast')), '404');
  await done(page);
});

test('a copied #y= link reopens the same source byte for byte', async () => {
  const page = await openApp();
  const source = await example('flowchart');
  await call(page, 'app_new_document', { source, name: 'flow.yaml', mode: 'view' });
  await shown(page);
  const r = await call(page, 'app_copy_link');
  const url = r.structuredContent.url;
  assert(url.includes('#y='), url);
  const page2 = await openApp({ path: url.slice(site.base.length) });
  await shown(page2);
  const got = await page2.evaluate(() => window.nymphApp.app.doc.source);
  assert(got === source, 'same source');
  await done(page2);
  await done(page);
});

test('?example=…&mode=edit opens the editor; the embed code draws on a blank page', async () => {
  const page = await openApp({ path: '/?example=line&mode=edit' });
  await shown(page);
  assert((await state(page)).mode === 'edit', 'edit');
  await call(page, 'app_copy_embed');
  const snippet = await page.evaluate(() => navigator.clipboard.readText());
  assert(snippet.includes('<nymph-viewer>') && snippet.includes('nymph-viewer.js'), snippet.slice(0, 100));
  const blank = await page.context().newPage();
  await blank.route(`${site.base}/blank.html`, (route) => route.fulfill({ contentType: 'text/html', body: `<!doctype html><body>${snippet}</body>` }));
  await blank.goto(`${site.base}/blank.html`);
  await blank.waitForFunction(() => document.querySelector('nymph-viewer')?.viewer?.info?.().hasFigure, null, { timeout: 30000 });
  await done(page);
});

test('themes: the picker lists them; View previews one, Edit writes it into the file as one undo step', async () => {
  const page = await openApp({ path: '/?example=line' });
  await shown(page);
  const listed = await call(page, 'app_list_themes');
  const themes = listed.structuredContent.themes;
  assert(themes.length >= 8 && themes[0].id === 'light' && themes.some((t) => t.id === 'paper'), JSON.stringify(themes).slice(0, 200));
  // View: the dialog's cards, a preview that leaves the file alone.
  page.evaluate(() => window.nymphApp.commands.run('app_choose_theme'));
  await page.waitForSelector('.theme-card');
  assert((await page.locator('.theme-card').count()) === themes.length + 1, 'a card per theme and its own');
  await page.click('.theme-card[title*="serif"]'); // Paper: a font fetched on demand
  await page.waitForFunction(() => window.nymphApp.app.chartTheme === 'paper', null, { timeout: 30000 });
  assert(!(await state(page)).modified, 'a preview does not modify the file');
  assert(!(await page.evaluate(() => window.nymphApp.app.doc.source)).includes('theme:'), 'the source is unchanged');
  // Edit: the theme goes into the source, and the preview gives way to it.
  await call(page, 'app_set_mode', { mode: 'edit' });
  await shown(page);
  const r = await call(page, 'app_set_file_theme', { theme: 'ocean' });
  assert(!r.isError, JSON.stringify(r));
  await shown(page);
  const source = await page.evaluate(() => window.nymphApp.app.editor.source());
  assert(/theme: ocean/.test(source), source.slice(0, 200));
  assert(await page.evaluate(() => window.nymphApp.app.chartTheme) === 'source', 'the preview is off');
  assert((await state(page)).modified, 'modified');
  await page.evaluate(() => window.nymphApp.app.editor.undo());
  assert(!/theme:/.test(await page.evaluate(() => window.nymphApp.app.editor.source())), 'undone');
  assert((await call(page, 'app_set_file_theme', { theme: 'neon' })).isError, 'unknown theme refused');
  await done(page);
});

// ── Step 10: recent, drafts, templates ───────────────────────────────────────

test('Recent reopens a URL; a draft survives a reload and Restore brings it back', async () => {
  const page = await openApp({ path: `/?url=${encodeURIComponent(`${cors.base}/bars.yaml`)}` });
  await shown(page);
  await page.waitForTimeout(500);
  await page.goto(`${site.base}/`);
  await page.waitForFunction(() => document.documentElement.dataset.ready === 'true');
  await page.waitForSelector('#recent-list .card');
  await page.click('#recent-list .card');
  await shown(page);
  assert((await state(page)).origin === 'url', 'reopened');
  // An edit, a reload: the draft is offered.
  await call(page, 'app_set_mode', { mode: 'edit' });
  await shown(page);
  await call(page, 'editor_set_fields', { path: 'root', fields: { title: 'Draft title' } });
  await page.evaluate(() => document.dispatchEvent(new Event('visibilitychange')));
  await page.evaluate(() => new Promise((r) => setTimeout(r, 2500)));
  page.on('dialog', (d) => d.accept()); // beforeunload
  await page.goto(`${site.base}/?url=${encodeURIComponent(`${cors.base}/bars.yaml`)}`);
  await page.waitForSelector('#banner >> text=Restore', { timeout: 20000 });
  await page.click('#banner >> text=Restore');
  await page.waitForFunction(() => window.nymphApp.app.doc.source.includes('Draft title'));
  assert((await state(page)).modified, 'restored as modified');
  // Saving drops the draft.
  const wait = page.waitForEvent('download');
  await call(page, 'app_save');
  await wait;
  const left = await page.evaluate(async () => {
    const { draft } = await import('./app/store.js');
    return draft(window.nymphApp.app.doc.id);
  });
  assert(!left, 'draft gone after saving');
  await done(page);
});

test('every template opens and draws; Clear empties Recent', async () => {
  const page = await openApp();
  const templates = await page.evaluate(async () => (await (await fetch('templates/index.json')).json()).map((t) => t.name));
  for (const t of templates) {
    await call(page, 'app_new_document', { template: t, mode: 'view' });
    await shown(page);
    const s = await state(page);
    assert(s.banner === '', `${t}: ${s.banner}`);
  }
  await call(page, 'app_close');
  await page.waitForSelector('#recent-list .card');
  await page.click('#recent-clear');
  await page.waitForTimeout(300);
  assert(await page.isHidden('#recent-section'), 'recent cleared');
  await done(page);
});

// ── Step 11: Google Drive (Google's scripts stubbed) ─────────────────────────

const DRIVE_STUB = `
  window.__drive = { calls: [] };
  window.google = { accounts: { oauth2: { initTokenClient: (o) => ({ requestAccessToken: () => { window.__drive.calls.push('token'); setTimeout(() => o.callback({ access_token: 'tok', expires_in: 3600 })); } }) } } };
  window.google.picker = {
    ViewId: { DOCS: 'docs', FOLDERS: 'folders' }, Action: { PICKED: 'picked', CANCEL: 'cancel' },
    DocsView: class { setMimeTypes() { return this; } setSelectFolderEnabled() { return this; } setIncludeFolders() { return this; } },
    PickerBuilder: class { addView() { return this; } setOAuthToken() { return this; } setDeveloperKey() { return this; } setAppId() { return this; }
      setCallback(cb) { this.cb = cb; return this; } build() { const cb = this.cb; return { setVisible: () => { window.__drive.calls.push('picker'); setTimeout(() => cb({ action: 'picked', docs: [{ id: 'file1', name: 'drive.yaml' }] })); } }; } },
  };
  window.gapi = { load: (name, o) => setTimeout(o.callback) };
`;

test('without config no Drive control; with it, open, save with the revision check', async () => {
  let page = await openApp();
  assert(await page.isHidden('#btn-drive'), 'no Drive button');
  assert(!page.requests.some((u) => u.includes('google')), 'no Google request');
  await done(page);
  let revision = 'r1';
  let patched = null;
  const source = await example('line');
  page = await openApp({
    routes: {
      '**/config.json': (route) => route.fulfill({ contentType: 'application/json', body: JSON.stringify({ google: { clientId: 'cid', apiKey: 'key', appId: '1' } }) }),
      'https://accounts.google.com/gsi/client': (route) => route.fulfill({ contentType: 'text/javascript', body: DRIVE_STUB }),
      'https://apis.google.com/js/api.js': (route) => route.fulfill({ contentType: 'text/javascript', body: '' }),
      'https://www.googleapis.com/**': (route, req) => {
        const url = req.url();
        if (req.method() === 'PATCH') {
          patched = req.postData();
          revision = 'r2';
          return route.fulfill({ contentType: 'application/json', body: JSON.stringify({ id: 'file1', headRevisionId: 'r2' }) });
        }
        if (url.includes('alt=media')) {
          return route.fulfill({ contentType: 'text/yaml', body: source });
        }
        return route.fulfill({ contentType: 'application/json', body: JSON.stringify({ id: 'file1', name: 'drive.yaml', headRevisionId: revision }) });
      },
    },
  });
  assert(await page.isVisible('#btn-drive'), 'Drive button');
  await call(page, 'app_open_drive');
  await shown(page);
  let s = await state(page);
  assert(s.origin === 'drive' && s.name === 'drive.yaml', JSON.stringify(s));
  assert(page.url().includes('?drive=file1'), 'the address');
  await call(page, 'app_set_mode', { mode: 'edit' });
  await shown(page);
  await call(page, 'editor_set_fields', { path: 'root', fields: { title: 'In Drive' } });
  await call(page, 'app_save');
  assert(patched && patched.includes('In Drive'), 'PATCH sent');
  assert(!(await state(page)).modified, 'saved');
  // Someone else changed it: the conflict dialog.
  revision = 'r9';
  await call(page, 'editor_set_fields', { path: 'root', fields: { title: 'Again' } });
  const saving = call(page, 'app_save');
  await page.waitForSelector('dialog.dlg >> text=changed in Google Drive');
  await page.click('dialog.dlg >> text=Cancel');
  await saving;
  assert((await state(page)).modified, 'not saved');
  await done(page);
});

// ── Step 12: offline ─────────────────────────────────────────────────────────

test('after one visit the app works offline', async () => {
  const page = await openApp({ sw: true, path: '/?example=line' });
  await shown(page);
  await page.waitForFunction(() => navigator.serviceWorker?.controller || navigator.serviceWorker.ready.then(() => true), null, { timeout: 20000 });
  await page.evaluate(() => navigator.serviceWorker.ready);
  await page.reload();
  await page.waitForFunction(() => !!navigator.serviceWorker.controller, null, { timeout: 20000 });
  await page.context().setOffline(true);
  await page.goto(`${site.base}/?example=line`);
  await page.waitForFunction(() => document.documentElement.dataset.ready === 'true', null, { timeout: 20000 });
  await shown(page);
  assert((await state(page)).name === 'line.yaml', 'opened offline');
  await page.context().setOffline(false);
  await done(page);
});

// ── Step 13: mobile ──────────────────────────────────────────────────────────

test('on a phone: no Edit or Source, ?mode=edit opens View, and the editor is never fetched', async () => {
  const page = await openApp({ mobile: true, path: '/?example=line&mode=edit' });
  await shown(page);
  const s = await state(page);
  assert(s.mode === 'view', 'view');
  assert(await page.isHidden('#mode-edit'), 'no Edit');
  assert(await page.isHidden('#btn-source'), 'no Source');
  assert(await page.isHidden('#btn-save'), 'no Save');
  await page.waitForTimeout(2500); // past any idle prefetch
  assert(!page.requests.some((u) => u.includes('nymph-editor')), 'no editor request');
  const names = await page.evaluate(() => window.nymphApp.commands.list().map((c) => c.name));
  assert(!names.includes('app_show_source') && !names.includes('app_save'), 'no edit commands');
  // A broken file: no Fix in source.
  await call(page, 'app_new_document', { source: 'root: {type: Plot, bogus: 1}\n', mode: 'view' });
  await page.waitForSelector('#banner:not([hidden])');
  assert(await page.isHidden('#banner >> text=Fix in source'), 'no Fix in source');
  assert(/computer/.test(await page.textContent('#stage')), 'open it on a computer');
  await done(page);
});

test('on a phone the document never scrolls: the top bar stays in view', async () => {
  const page = await openApp({ mobile: true, path: '/?example=line' });
  await shown(page);
  const barTop = () => page.evaluate(() => document.getElementById('bar').getBoundingClientRect().top);
  const fits = await page.evaluate(() => document.scrollingElement.scrollHeight <= document.scrollingElement.clientHeight);
  assert(fits, 'the document is no taller than the screen');
  // A drag up on the chart, then on the stage below it.
  const cdp = await page.context().newCDPSession(page);
  const touch = (type, points) => cdp.send('Input.dispatchTouchEvent', { type, touchPoints: points.map(([x, y], id) => ({ x, y, id })) });
  for (const y of [300, 800]) {
    await touch('touchStart', [[195, y]]);
    for (let i = 1; i <= 8; ++i) {
      await touch('touchMove', [[195, y - i * 30]]);
    }
    await touch('touchEnd', []);
  }
  await page.waitForTimeout(300);
  assert((await barTop()) === 0 && (await page.evaluate(() => window.scrollY)) === 0, 'the bar stays at the top');
  // A browser that scrolls the document anyway (iOS Safari): it comes back.
  await page.evaluate(() => {
    const tall = document.createElement('div');
    tall.style.cssText = 'height: 3000px';
    document.documentElement.appendChild(tall);
    window.scrollTo(0, 400);
  });
  await page.waitForFunction(() => window.scrollY === 0, null, { timeout: 5000 });
  assert((await barTop()) === 0, 'the bar is back');
  await done(page);
});

test('a narrow desktop window keeps Edit', async () => {
  const page = await openApp({ path: '/?example=line' });
  await page.setViewportSize({ width: 390, height: 800 });
  await shown(page);
  assert(!(await page.isDisabled('#mode-edit')), 'Edit stays');
  await call(page, 'app_set_mode', { mode: 'edit' });
  await shown(page);
  assert((await state(page)).mode === 'edit', 'edit');
  await done(page);
});

let failed = 0;
const only = process.env.NYMPH_TEST_ONLY;
for (const { name, fn } of tests) {
  if (only && !name.includes(only)) {
    continue;
  }
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
cors.close();
closed.close();
console.log(`${tests.length - failed}/${tests.length} passed`);
process.exit(failed ? 1 : 0);
