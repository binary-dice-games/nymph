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

// ── Step 4: the Agents panel and its activity ────────────────────────────────

const dockVisible = (page) => page.evaluate(() => !document.getElementById('dock').hidden);
const entries = (page) =>
  page.evaluate(() => [...document.querySelectorAll('#dock .activity-list .entry')].map((e) => ({
    who: e.querySelector('.who')?.textContent,
    label: e.querySelector('.label')?.textContent,
    undo: !!e.querySelector('button[aria-label^="Undo"]'),
    state: e.querySelector('.state')?.textContent ?? '',
  })));
const undoFirst = (page) => page.click('#dock .activity-list .entry:first-child button[aria-label^="Undo"]');

test('without connectors in config.json there is no Agents button', async () => {
  const context = await browser.newContext({ serviceWorkers: 'block' });
  const page = await context.newPage();
  page.errors = [];
  await page.route('**/config.json', (r) => r.fulfill({ contentType: 'application/json', body: '{"agents": {}}' }));
  await page.goto(`${site.base}/?example=line`);
  await page.waitForFunction(() => document.documentElement.dataset.ready === 'true');
  assert(await page.isHidden('#btn-agents'), 'no Agents button');
  await context.close();
});

test('the panel opens by itself on an agent\'s first change, and lists it', async () => {
  const page = await openApp();
  await openExample(page, 'line');
  assert(await page.isVisible('#btn-agents'), 'the Agents button');
  assert(!(await dockVisible(page)), 'closed at first');
  await agent(page, 'document_state'); // a read: not listed, does not open it
  assert(!(await dockVisible(page)), 'still closed after a read');
  const src = (await source(page)).replace('title:', 'title: Changed #');
  const r = await agent(page, 'document_set_source', { source: src.replace(/title: Changed #.*\n/, 'title: Changed\n') });
  assert(!r.isError, text(r));
  await page.waitForFunction(() => !document.getElementById('dock').hidden);
  const list = await entries(page);
  eq(list.length, 1, 'one entry');
  eq(list[0].who, 'test-agent', 'who');
  assert(list[0].undo, 'with Undo');
  await done(page);
});

test('Undo in View mode restores the chart from before the change', async () => {
  const page = await openApp();
  await openExample(page, 'line');
  const before = await source(page);
  await agent(page, 'document_set_source', { source: before.replace(/title: .*/, 'title: By the agent') });
  await page.waitForFunction(() => !document.getElementById('dock').hidden);
  await undoFirst(page);
  await page.waitForFunction(() => document.querySelector('#dock .entry.undone'));
  assert(!(await source(page)).includes('By the agent'), 'restored');
  eq((await entries(page))[0].state, 'undone', 'marked undone');
  await done(page);
});

test('a group is one entry, undone in Edit mode by the editor\'s undo', async () => {
  const page = await openApp();
  await openExample(page, 'line', 'edit');
  const before = await source(page);
  await agent(page, 'document_begin_change', { label: 'Retitle the chart' });
  await agent(page, 'editor_set_fields', { path: 'root', fields: { title: 'One' } });
  await agent(page, 'editor_set_fields', { path: 'root', fields: { x_label: 'When' } });
  await agent(page, 'document_end_change');
  await page.waitForFunction(() => !document.getElementById('dock').hidden);
  const list = await entries(page);
  eq(list.length, 1, 'one entry for the group');
  eq(list[0].label, 'Retitle the chart', 'its label');
  await page.click('#dock .entry:first-child summary');
  eq(await page.locator('#dock .entry:first-child .tools li').count(), 2, 'two steps');
  const version = await page.evaluate(() => window.nymphApp.app.editor.version());
  await undoFirst(page);
  await page.waitForFunction(() => document.querySelector('#dock .entry.undone'));
  eq(await source(page), before, 'both undone');
  eq(await page.evaluate(() => window.nymphApp.app.editor.state().redo), 'Retitle the chart', 'by the editor\'s own undo');
  assert((await page.evaluate(() => window.nymphApp.app.editor.version())) > version, 'a new version');
  await done(page);
});

test('an entry changed since asks before restoring', async () => {
  const page = await openApp();
  await openExample(page, 'line');
  const before = await source(page);
  await agent(page, 'document_set_source', { source: before.replace(/title: .*/, 'title: By the agent') });
  await page.waitForFunction(() => !document.getElementById('dock').hidden);
  // The user changes it after the agent.
  await page.evaluate(() => window.nymphApp.commands.run('document_set_source', { source: window.nymphApp.app.doc.source.replace('By the agent', 'By the user') }));
  await undoFirst(page);
  await page.waitForSelector('dialog.dlg');
  assert((await page.textContent('dialog.dlg')).includes('Later changes are lost'), 'asks');
  await page.click('dialog.dlg button:not(.danger)');
  assert((await source(page)).includes('By the user'), 'Cancel keeps the user\'s change');
  await undoFirst(page);
  await page.click('dialog.dlg button.danger');
  await page.waitForFunction(() => !window.nymphApp.app.doc.source.includes('By the user'));
  await done(page);
});

test('Stop ends the agent\'s change and refuses its next write', async () => {
  const page = await openApp();
  await openExample(page, 'line', 'edit');
  await agent(page, 'document_begin_change', { label: 'Long change' });
  await agent(page, 'editor_set_fields', { path: 'root', fields: { title: 'One' } });
  await page.waitForFunction(() => !document.getElementById('dock').hidden);
  await page.waitForSelector('#dock .agents-status button');
  assert((await page.textContent('#dock .agents-status')).includes('test-agent is changing the chart'), 'status');
  await page.click('#dock .agents-status button');
  const r = await agent(page, 'editor_set_fields', { path: 'root', fields: { title: 'Two' } });
  assert(r.isError && text(r).includes('stopped'), text(r));
  assert(!(await agent(page, 'document_begin_change', { label: 'Asked first' })).isError, 'a new change after asking');
  await done(page);
});

test('an allowed external action is listed; an agent\'s label is shown as text', async () => {
  const page = await openApp();
  await openExample(page, 'line');
  await page.evaluate(() => (window.__x = 0));
  await agent(page, 'document_begin_change', { label: '<img src=x onerror="window.__x=1">' });
  await agent(page, 'document_set_source', { source: (await source(page)).replace(/title: .*/, 'title: X') });
  await agent(page, 'document_end_change');
  const download = agent(page, 'app_download', { kind: 'yaml' });
  await page.waitForSelector('dialog.dlg');
  await page.click('dialog.dlg button.primary');
  await download;
  const list = await entries(page);
  eq(list.length, 2, 'the change and the download');
  eq(list[0].state, 'allowed', 'the download');
  eq(list[1].label, '<img src=x onerror="window.__x=1">', 'label as text');
  eq(await page.locator('#dock img').count(), 0, 'no image element');
  eq(await page.evaluate(() => window.__x), 0, 'nothing ran');
  await done(page);
});

test('Ctrl+Shift+A toggles the panel; no request leaves the site', async () => {
  const page = await openApp();
  await openExample(page, 'bars');
  await page.click('#stage');
  await page.keyboard.press('Control+Shift+A');
  assert(await dockVisible(page), 'opened');
  assert(await page.isVisible('#agents-connect'), 'Connect an agent');
  await page.keyboard.press('Control+Shift+A');
  assert(!(await dockVisible(page)), 'closed');
  const outside = page.requests.filter((u) => !u.startsWith(site.base) && !u.startsWith('data:') && !u.startsWith('blob:'));
  eq(outside, [], 'requests outside the site');
  await done(page);
});

// ── Step 5: WebMCP ───────────────────────────────────────────────────────────

/** A stand-in for the browser's navigator.modelContext: records registrations. */
const stubWebMcp = () => {
  window.__tools = [];
  Object.defineProperty(navigator, 'modelContext', {
    value: {
      registerTool(tool) {
        window.__tools.push(tool);
        return { unregister: () => (window.__tools = window.__tools.filter((t) => t !== tool)) };
      },
    },
  });
};
const webmcpCall = (page, name, args = {}) => page.evaluate(([n, a]) => window.__tools.find((t) => t.name === n).execute(a), [name, args]);

test('WebMCP: the tools are registered while a chart is open', async () => {
  const page = await openApp({ init: stubWebMcp });
  eq(await page.evaluate(() => window.__tools.length), 0, 'none on the start screen');
  await openExample(page, 'line');
  const [n, expected] = await page.evaluate(() => [window.__tools.length, window.nymphApp.commands.agentTools().length]);
  eq(n, expected, 'every agent tool');
  await page.evaluate(() => window.nymphApp.commands.run('app_close'));
  await page.waitForFunction(() => !window.nymphApp.app.doc);
  eq(await page.evaluate(() => window.__tools.length), 0, 'removed with the chart');
  await done(page);
});

test('WebMCP: a call changes the chart as the browser\'s assistant', async () => {
  const page = await openApp({ init: stubWebMcp });
  await openExample(page, 'line');
  const src = await source(page);
  const r = await webmcpCall(page, 'document_set_source', { source: src.replace(/title: .*/, 'title: From WebMCP') });
  assert(!r.isError, text(r));
  assert((await source(page)).includes('From WebMCP'), 'changed');
  await page.waitForFunction(() => !document.getElementById('dock').hidden);
  eq((await entries(page))[0].who, 'the browser\'s assistant', 'attributed');
  assert((await page.textContent('#dock .agents-list')).includes('WebMCP'), 'listed as connected');
  // An external action still asks.
  const download = webmcpCall(page, 'app_download', { kind: 'yaml' });
  await page.waitForSelector('dialog.dlg');
  await page.click('dialog.dlg button:not(.primary)');
  assert((await download).isError, 'declined');
  await done(page);
});

test('WebMCP: the switch turns it off; without the API nothing happens', async () => {
  const page = await openApp({ init: stubWebMcp });
  await openExample(page, 'line');
  await page.click('#btn-agents');
  await page.uncheck('#agents-webmcp');
  eq(await page.evaluate(() => window.__tools.length), 0, 'unregistered');
  await page.check('#agents-webmcp');
  assert((await page.evaluate(() => window.__tools.length)) > 0, 'registered again');
  const plain = await openApp();
  await openExample(plain, 'line');
  eq(await plain.evaluate(() => window.nymphApp.app.webmcp.available), false, 'no WebMCP');
  eq(await plain.locator('#agents-webmcp').count(), 0, 'no switch');
  await done(page);
  await done(plain);
});

test('on a phone the panel is a bottom sheet, without Connect or editor tools', async () => {
  const page = await openApp({ mobile: true, init: stubWebMcp });
  await openExample(page, 'line');
  assert(await page.isVisible('#btn-agents'), 'Agents, for WebMCP');
  await page.click('#btn-agents');
  eq(await page.locator('#agents-connect').count(), 0, 'no Connect');
  eq(await page.evaluate(() => getComputedStyle(document.getElementById('dock')).position), 'fixed', 'a sheet');
  const names = await page.evaluate(() => window.__tools.map((t) => t.name));
  assert(!names.some((n) => n.startsWith('editor_')), 'no editor tools');
  await done(page);
});

// ── Step 8: the page side of the nymph mcp bridge ────────────────────────────

/** A `nymph mcp` process driven as an MCP client would. */
async function startMcp(origins) {
  const proc = spawn(nymphExe, ['mcp', '--rasterizer', 'software'], {
    env: { ...process.env, NYMPH_WEB_APP_ORIGINS: origins, NYMPH_EDITORS_DIR: join(webDir, `agents-test-editors-${process.pid}`) },
    stdio: ['pipe', 'pipe', 'inherit'],
  });
  let buffer = '';
  const waiting = new Map();
  const notes = [];
  proc.stdout.on('data', (d) => {
    buffer += d;
    let nl;
    while ((nl = buffer.indexOf('\n')) >= 0) {
      const m = JSON.parse(buffer.slice(0, nl));
      buffer = buffer.slice(nl + 1);
      if (m.id !== undefined && waiting.has(m.id)) {
        waiting.get(m.id)(m);
        waiting.delete(m.id);
      } else if (m.method) {
        notes.push(m.method);
      }
    }
  });
  let id = 0;
  const rpc = (method, params = {}) =>
    new Promise((resolve) => {
      const n = ++id;
      waiting.set(n, resolve);
      proc.stdin.write(`${JSON.stringify({ jsonrpc: '2.0', id: n, method, params })}\n`);
    });
  await rpc('initialize', { protocolVersion: '2025-11-25', capabilities: {}, clientInfo: { name: 'Test Client', version: '1' } });
  proc.stdin.write(`${JSON.stringify({ jsonrpc: '2.0', method: 'notifications/initialized' })}\n`);
  const tool = async (name, args = {}) => (await rpc('tools/call', { name, arguments: args })).result;
  const listed = async () => (await rpc('tools/list')).result.tools.map((t) => t.name);
  const stop = () => proc.kill();
  return { tool, listed, notes, stop };
}

/** Waits until `nymph mcp` lists the page's tools (the page answered tools/list). */
async function untilPaired(mcp) {
  for (let i = 0; i < 200; i++) {
    if ((await mcp.listed()).includes('document_state')) {
      return;
    }
    await new Promise((r) => setTimeout(r, 50));
  }
  throw new Error('the page never paired');
}

const haveNymph = nymphExe && existsSync(nymphExe);
const bridgeTest = (name, fn) =>
  test(name, async () => {
    if (!haveNymph) {
      console.log(`     (skipped: no native nymph given)`);
      return;
    }
    await fn();
  });

bridgeTest('bridge: an agent pairs by link, edits the chart, and the tab reconnects after a reload', async () => {
  const mcp = await startMcp('http://127.0.0.1:*');
  try {
    const pair = (await mcp.tool('web_app_pair')).structuredContent;
    const page = await openApp({ path: `/?example=line#pair=${pair.code}` });
    await shown(page);
    await untilPaired(mcp);
    assert(mcp.notes.includes('notifications/tools/list_changed'), 'the client was told the list changed');
    assert(!(await page.evaluate(() => location.hash)).includes('pair'), 'the code left the address');
    eq(text(await mcp.tool('app_set_mode', { mode: 'edit' })).includes('edit'), true, 'switched to Edit');
    await shown(page);
    const r = await mcp.tool('editor_set_fields', { path: 'root', fields: { title: 'From the agent' } });
    assert(!r.isError, JSON.stringify(r));
    assert((await source(page)).includes('From the agent'), 'the chart changed');
    await page.waitForFunction(() => !document.getElementById('dock').hidden);
    eq((await entries(page))[0].who, 'test-client', 'attributed to the client');
    assert((await page.textContent('#dock .agents-list')).includes('test-client'), 'connected');
    await undoFirst(page);
    await page.waitForFunction(() => !window.nymphApp.app.editor.source().includes('From the agent'));
    // A reload of the tab reconnects with its resume token.
    await page.reload();
    await page.waitForFunction(() => document.documentElement.dataset.ready === 'true');
    await shown(page);
    await page.waitForFunction(() => window.nymphApp.app.bridge.links.size === 1, null, { timeout: 10000 });
    const state = await mcp.tool('document_state');
    assert(!state.isError, JSON.stringify(state));
    await done(page);
  } finally {
    mcp.stop();
  }
});

bridgeTest('bridge: a code typed in the Connect dialog pairs; a wrong one says so', async () => {
  const mcp = await startMcp('http://127.0.0.1:*');
  try {
    const pair = (await mcp.tool('web_app_pair')).structuredContent;
    const page = await openApp({ path: '/?example=bars' });
    await shown(page);
    await page.click('#btn-agents');
    await page.click('#agents-connect');
    await page.fill('#pair-code', `${pair.port}-AAAA-AAAA`);
    await page.click('dialog.dlg button.primary');
    await page.waitForSelector('.toast');
    assert((await page.textContent('#toasts')).includes('not valid'), await page.textContent('#toasts'));
    await page.click('#agents-connect');
    await page.fill('#pair-code', pair.code.toLowerCase());
    await page.click('dialog.dlg button.primary');
    await untilPaired(mcp);
    const r = await mcp.tool('document_state');
    assert(!r.isError && text(r).includes('bars'), text(r));
    await done(page);
  } finally {
    mcp.stop();
  }
});

bridgeTest('bridge: a page from an origin nymph mcp does not accept is refused', async () => {
  const mcp = await startMcp('https://binary-dice-games.github.io');
  try {
    const pair = (await mcp.tool('web_app_pair')).structuredContent;
    const page = await openApp({ path: '/?example=bars' });
    await shown(page);
    const error = await page.evaluate((c) => window.nymphApp.app.bridge.pair(c).then(() => '', (e) => e.message), pair.code);
    assert(error.includes('Could not reach') || error.includes('closed'), error);
    assert(!(await mcp.listed()).includes('document_state'), 'not paired');
    await done(page);
  } finally {
    mcp.stop();
  }
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
