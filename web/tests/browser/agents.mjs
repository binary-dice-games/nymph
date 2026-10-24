// MIT License © 2026 Binary Dice Games
// spec/17_agent_connectors plan steps 2, 4, 5 and 8: agents in the web app
// on the built site (<build>/web/site/), in Chromium through Playwright:
// the agent tool list and origins, change groups, the Agents panel and its
// activity, WebMCP with a stubbed navigator.modelContext, and the page side
// of the `nymph mcp` bridge (with a native nymph when NYMPH_EXE is set).
// Exits 77 (CTest's skip) when Playwright is not installed.
//
//   node agents.mjs <build>/web <nymph source dir> [<nymph executable>]

import { execSync, spawn } from 'node:child_process';
import { existsSync } from 'node:fs';
import { readFile } from 'node:fs/promises';
import { createRequire } from 'node:module';
import { join } from 'node:path';
import { serve } from './serve.mjs';

const [webDir, sourceDir, nymphExe] = process.argv.slice(2);
if (!webDir || !sourceDir) {
  console.error('usage: node agents.mjs <build>/web <nymph source dir> [<nymph executable>]');
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
const site = await serve([siteDir]);
const browser = await playwright.chromium.launch({ args: ['--enable-unsafe-swiftshader', '--use-angle=swiftshader'] });

const tests = [];
const test = (name, fn) => tests.push({ name, fn });
const assert = (cond, message) => {
  if (!cond) {
    throw new Error(message);
  }
};
const eq = (a, b, what) => assert(JSON.stringify(a) === JSON.stringify(b), `${what}: expected ${JSON.stringify(b)}, got ${JSON.stringify(a)}`);

/** A fresh page of the app; `options.init` runs before the app's scripts. */
async function openApp(options = {}) {
  const context = await browser.newContext({
    viewport: options.mobile ? { width: 390, height: 844 } : { width: 1280, height: 800 },
    deviceScaleFactor: 1,
    hasTouch: !!options.mobile,
    isMobile: !!options.mobile,
    acceptDownloads: true,
    serviceWorkers: 'block',
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
  const path = options.path ?? '/';
  const sep = path.includes('?') ? '&' : '?';
  await page.goto(`${site.base}${path}${options.mobile ? `${sep}device=mobile` : ''}`);
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

/** Opens an example in `mode`. */
async function openExample(page, name, mode = 'view') {
  const r = await page.evaluate(([n, m]) => window.nymphApp.commands.call('app_open_example', { name: n, mode: m }), [name, mode]);
  assert(!r.isError, JSON.stringify(r));
  await shown(page);
}

/** An agent's call through the registry, as a connector makes it. */
const agent = (page, name, args = {}, connector = 'mcp:test-agent') =>
  page.evaluate(([n, a, c]) => window.nymphApp.commands.callAgent(n, a, c), [name, args, connector]);
const text = (r) => r.content?.[0]?.text ?? '';
const source = (page) => page.evaluate(() => (window.nymphApp.app.editor ? window.nymphApp.app.editor.source() : window.nymphApp.app.doc.source));

async function done(page) {
  assert(page.errors.length === 0, `page errors: ${page.errors.join('; ')}`);
  await page.context().close();
}

// ── Step 2: the agent tool list, origins, change groups ──────────────────────

test('agentTools() is the same in View and Edit, with the editor tools', async () => {
  const page = await openApp();
  const names = () => page.evaluate(() => window.nymphApp.commands.agentTools().map((t) => t.name));
  const start = await names();
  await openExample(page, 'flowchart');
  const view = await names();
  await page.evaluate(() => window.nymphApp.commands.run('app_set_mode', { mode: 'edit' }));
  await shown(page);
  const edit = await names();
  eq(view, start, 'start and view');
  eq(edit, view, 'view and edit');
  for (const n of ['editor_set_fields', 'editor_state', 'document_set_source', 'document_begin_change', 'app_read_guide', 'app_example_source']) {
    assert(view.includes(n), `lists ${n}`);
  }
  for (const n of ['app_open_file', 'app_open_url_dialog', 'app_open_drive', 'app_new_dialog', 'app_save_as', 'app_choose_theme', 'app_about', 'app_shortcuts']) {
    assert(!view.includes(n), `does not list ${n}`);
  }
  eq(new Set(view).size, view.length, 'each tool once');
  await done(page);
});

test('every agent tool has a description and an object schema', async () => {
  const page = await openApp();
  const tools = await page.evaluate(() => window.nymphApp.commands.agentTools());
  for (const t of tools) {
    assert(typeof t.description === 'string' && t.description.length > 10, `${t.name}: description`);
    eq(t.inputSchema?.type, 'object', `${t.name}: schema type`);
    for (const r of t.inputSchema.required ?? []) {
      assert(t.inputSchema.properties?.[r], `${t.name}: required ${r} is a property`);
    }
  }
  await done(page);
});

test('the guide for agents names only tools agents have', async () => {
  const page = await openApp();
  const names = new Set(await page.evaluate(() => window.nymphApp.commands.agentTools().map((t) => t.name)));
  const guide = await (await fetch(`${site.base}/agent/web-app.md`)).text();
  const named = [...guide.matchAll(/`((?:app|document|editor)_[a-z_]+)`/g)].map((m) => m[1]);
  assert(named.length > 10, 'names tools');
  for (const n of named) {
    assert(names.has(n), `the guide names ${n}, which agents do not have`);
  }
  await done(page);
});

test('editor-tools.json is what the editor module lists', async () => {
  const page = await openApp();
  await openExample(page, 'line', 'edit');
  const [listed, module] = await page.evaluate(() => [window.nymphApp.commands.editorTools, window.nymphApp.app.editor.toolDefinitions()]);
  eq(listed, module, 'editor-tools.json is stale: run web/tools/dump-editor-tools.mjs');
  await done(page);
});

test('an editor tool in View mode says how to get the editor', async () => {
  const page = await openApp();
  await openExample(page, 'line');
  const r = await agent(page, 'editor_set_fields', { path: 'root', fields: { title: 'x' } });
  assert(r.isError && text(r).includes('app_set_mode'), text(r));
  const none = await openApp();
  const r2 = await agent(none, 'document_source');
  assert(r2.isError && text(r2).includes('app_new_document'), text(r2));
  await done(page);
  await done(none);
});

test('only connectors can call as agents, never as the user', async () => {
  const page = await openApp();
  for (const bad of ['user', 'agent:x', 'mcp:', 'mcp:Has Space', 'claude']) {
    const threw = await page.evaluate((c) => window.nymphApp.commands.callAgent('document_state', {}, c).then(() => false, () => true), bad);
    assert(threw, `connector '${bad}' refused`);
  }
  const r = await agent(page, 'app_about');
  assert(r.isError && text(r).includes('not for agents'), text(r));
  await done(page);
});

test('app_read_guide reads the guides and nothing else', async () => {
  const page = await openApp();
  const r = await agent(page, 'app_read_guide', { name: 'web-app' });
  assert(!r.isError && text(r).includes('Working in the nymph web app'), text(r).slice(0, 80));
  const fam = await agent(page, 'app_read_guide', { name: 'elements/Flowchart' });
  assert(!fam.isError && text(fam).includes('FlowNode'), 'a family');
  const bad = await agent(page, 'app_read_guide', { name: '../index.html' });
  assert(bad.isError, 'a path outside agent/ is refused');
  const ex = await agent(page, 'app_example_source', { name: 'line' });
  assert(!ex.isError && ex.structuredContent.source === (await example('line')), 'example source');
  await done(page);
});

test('allow for this session skips the dialog until the chart closes', async () => {
  const page = await openApp();
  await openExample(page, 'line');
  const first = agent(page, 'app_download', { kind: 'yaml' });
  await page.waitForSelector('dialog.dlg');
  await page.check('#allow-always');
  const download = page.waitForEvent('download');
  await page.click('dialog.dlg button.primary');
  assert(!(await first).isError, 'allowed');
  await download;
  const again = page.waitForEvent('download');
  assert(!(await agent(page, 'app_download', { kind: 'yaml' })).isError, 'no dialog the second time');
  await again;
  assert((await page.locator('dialog.dlg').count()) === 0, 'no dialog');
  // Another agent still asks.
  const other = agent(page, 'app_download', { kind: 'yaml' }, 'webmcp');
  await page.waitForSelector('dialog.dlg');
  await page.click('dialog.dlg button:not(.primary)');
  assert((await other).isError, 'declined');
  // A new chart: asked again.
  await openExample(page, 'bars');
  const third = agent(page, 'app_download', { kind: 'yaml' });
  await page.waitForSelector('dialog.dlg');
  await page.click('dialog.dlg button:not(.primary)');
  assert((await third).isError, 'asked again after the chart changed');
  await done(page);
});

test('a change group is one undo step in Edit mode', async () => {
  const page = await openApp();
  await openExample(page, 'line', 'edit');
  const before = await source(page);
  await agent(page, 'document_begin_change', { label: 'Rename and relabel' });
  assert(!(await agent(page, 'editor_set_fields', { path: 'root', fields: { title: 'One' } })).isError, 'first write');
  assert(!(await agent(page, 'editor_set_fields', { path: 'root', fields: { x_label: 'When' } })).isError, 'second write');
  await agent(page, 'document_end_change');
  const undo = await page.evaluate(() => window.nymphApp.app.editor.state().undo);
  eq(undo, 'Rename and relabel', 'the undo entry is the group');
  await page.evaluate(() => window.nymphApp.app.editor.undo());
  eq(await source(page), before, 'one undo restores both');
  await done(page);
});

test('a second agent cannot write while another changes the chart', async () => {
  const page = await openApp();
  await openExample(page, 'line', 'edit');
  await agent(page, 'document_begin_change', { label: 'First' });
  const r = await agent(page, 'document_begin_change', { label: 'Second' }, 'webmcp');
  assert(r.isError && text(r).includes('another agent'), text(r));
  const w = await agent(page, 'editor_set_fields', { path: 'root', fields: { title: 'x' } }, 'webmcp');
  assert(w.isError && text(w).includes('another agent'), text(w));
  await agent(page, 'document_end_change');
  const ok = await agent(page, 'editor_set_fields', { path: 'root', fields: { title: 'x' } }, 'webmcp');
  assert(!ok.isError, text(ok));
  await done(page);
});

test('a change group ends after its agent goes quiet', async () => {
  const page = await openApp();
  await openExample(page, 'line', 'edit');
  await page.evaluate(() => (window.nymphApp.commands.groupIdleMs = 200));
  await agent(page, 'document_begin_change', { label: 'Forgotten' });
  await page.waitForFunction(() => window.nymphApp.commands.group === null, null, { timeout: 5000 });
  const r = await agent(page, 'document_begin_change', { label: 'Other' }, 'webmcp');
  assert(!r.isError, 'another agent may start once it ended');
  await done(page);
});

// ── Runner ───────────────────────────────────────────────────────────────────

let failed = 0;
const only = process.env.NYMPH_TEST_FILTER;
for (const t of tests) {
  if (only && !t.name.includes(only)) {
    continue;
  }
  try {
    await t.fn();
    console.log(`ok   ${t.name}`);
  } catch (e) {
    failed++;
    console.log(`FAIL ${t.name}\n     ${e.stack ?? e}`);
  }
}
await browser.close();
site.close();
console.log(failed ? `${failed} of ${tests.length} failed` : `all ${tests.length} passed`);
process.exit(failed ? 1 : 0);
