// MIT License © 2026 Binary Dice Games
/**
 * @file main.js The nymph web app (spec/15_web_app/DESIGN.md): the start
 * screen, the document screen with its View and Edit modes, opening,
 * saving, sharing, recent files and drafts, and the command registry every
 * control goes through.
 */

import { NymphError, createViewer } from '../nymph-viewer.js';
import { Activity } from './agents/activity.js';
import { Bridge } from './agents/bridge.js';
import { AgentsPanel } from './agents/panel.js';
import { WebMcp } from './agents/webmcp.js';
import { Collab } from './collab/collab.js';
import { Commands } from './commands.js';
import { device, editAvailability } from './device.js';
import { Doc, baseName, withoutView } from './document.js';
import { icons } from './icons.js';
import { OpenError, fetchUrl, pickFile, pngSource, readFile, sniff, writeHandle } from './open.js';
import { DriveProvider } from './providers/drive.js';
import { appBase, decodeSource, embedCode, linkFor } from './share.js';
import * as store from './store.js';
import { ask, banner, busy, closeMenu, dialog, dialogButtons, download, el, showMenu, toast } from './ui.js';

const $ = (id) => document.getElementById(id);

/** The app's state: one document at a time. */
const app = {
  doc: null,
  mode: 'view', // 'view' | 'edit'
  viewer: null,
  editor: null,
  editorModule: null,
  config: {},
  version: { version: '', commit: 'dev' },
  drive: null,
  chartTheme: 'source',
  draftTimer: 0,
  opening: 0, // a token: the latest open wins
};

/** The registry every control calls (section 3.11). */
const commands = new Commands(() => ({ hasDocument: !!app.doc, mode: app.mode }), { editing: device.directEditing });
window.nymphApp = { app, commands }; // for tests and the agent connectors

// ── Start ─────────────────────────────────────────────────────────────────────

/**
 * Scrolls the document back to the top whenever the browser moves it (iOS
 * Safari does, around the on-screen keyboard), so the top bar stays in
 * view. The app's panes scroll; the document never should.
 */
function keepDocumentInPlace() {
  const restore = () => {
    if (window.scrollX === 0 && window.scrollY === 0) {
      return;
    }
    // Not while typing: the browser keeps the focused field above the keyboard.
    if (document.activeElement?.matches?.('input, textarea:not(.nymph-editor-keys), [contenteditable]')) {
      return;
    }
    window.scrollTo(0, 0);
  };
  window.addEventListener('scroll', restore, { passive: true });
  document.addEventListener('focusout', () => setTimeout(restore, 100));
}

async function init() {
  applyAppTheme(load('theme') ?? 'auto');
  app.chartTheme = load('chartTheme') ?? 'source';
  [app.version, app.config] = await Promise.all([fetchJson('version.json', app.version), fetchJson('config.json', {})]);
  app.drive = new DriveProvider(app.config.google ?? {});
  if (device.directEditing) {
    // Listed to agents before the editor module loads (spec/17_agent_connectors).
    commands.editorTools = await fetchJson('editor-tools.json', []);
  }
  defineCommands();
  // What the registry needs for agents' change groups and activity
  // (spec/17_agent_connectors/DESIGN.md sections 4.4 and 5).
  commands.hooks.beginGroup = (label, origin) => app.editor?.beginGroup(label, origin);
  commands.hooks.endGroup = () => app.editor?.endGroup();
  commands.hooks.snapshot = () =>
    app.doc ? { doc: app.doc.id, source: app.editor ? app.editor.source() : app.doc.source, version: app.editor?.version() ?? null, mode: app.mode } : null;
  setUpAgents();
  setUpCollab();
  wireControls();
  wireDropAndPaste();
  wireKeys();
  keepDocumentInPlace();
  renderStart();
  registerServiceWorker();
  if (device.launchQueue) {
    window.launchQueue.setConsumer(async (params) => {
      const handle = params.files?.[0];
      if (handle) {
        const file = await handle.getFile();
        await openBytes(await readFile(file), file.name, { kind: 'local', handle });
      }
    });
  }
  await route();
  document.documentElement.dataset.ready = 'true';
}

/**
 * The agents the user runs (spec/17_agent_connectors): the Agents panel and
 * its activity, WebMCP, and the `nymph mcp` bridge.
 */
function setUpAgents() {
  const agents = app.config.agents ?? {};
  const offers = { bridge: !!agents.bridge && device.agents.bridge, webmcp: !!agents.webmcp && device.agents.webmcp };
  app.activity = new Activity(commands, {
    snapshot: () => commands.hooks.snapshot(),
    restore: (source) => commands.run('document_set_source', { source, label: 'Undo an agent\'s change' }),
    undoEditor: () => app.editor?.undo(),
    confirm: async (message) =>
      (await ask('Undo', message, [
        { label: 'Cancel', value: false },
        { label: 'Restore', value: true, kind: 'danger', focus: true },
      ])) === true,
  });
  app.webmcp = new WebMcp(commands, {
    onCall: () => app.agents.connected('webmcp', { name: 'The browser\'s assistant', route: 'through WebMCP' }),
  });
  app.bridge = new Bridge(commands, {
    documentName: () => app.doc?.name ?? 'untitled',
    version: app.version.version || 'dev',
    instructions:
      'The nymph web app: the chart the user has open in a browser tab. Read app_read_guide with name web-app first; call document_state before changing anything.',
    connected: (id, info) => app.agents.connected(id, info),
    disconnected: (id) => app.agents.disconnected(id),
  });
  app.agents = new AgentsPanel({
    dock: $('dock'),
    button: $('btn-agents'),
    commands,
    activity: app.activity,
    offers,
    webmcpEnabled: () => app.webmcp.enabled,
    setWebmcp: (on) => app.webmcp.setEnabled(on, !!app.doc),
    pair: (code) => app.bridge.pair(code),
  });
  if (offers.bridge) {
    app.bridge.resume();
  }
}

/**
 * Live collaboration (spec/18_collaboration): the owner shares the open
 * chart, guests see it live, everyone draws on a whiteboard over it.
 */
function setUpCollab() {
  app.collab = new Collab({
    app,
    commands,
    config: app.config.collab ?? {},
    base: appBase,
    stage: $('stage'),
    dock: $('collab-dock'),
    button: $('btn-collab'),
    act,
    ui: { toast, banner, ask, dialog, dialogButtons, el, download, busy },
    module: () => app.editor ?? app.viewer,
    currentYaml: () => (app.editor ? app.editor.toYaml() : app.viewer ? app.viewer.toYaml() : app.doc.source),
    ownYaml: () => app.viewer?.toYaml() ?? null,
    openGuest: (source, name) => openSource(source, name, { kind: 'collab' }, { mode: 'view', collab: true }),
    applyGuest: async (source) => {
      if (!app.viewer) {
        await showViewer(source);
      } else {
        await app.viewer.setSource(source);
      }
      app.doc.source = source;
      app.doc.savedSource = source;
      updateTitle();
    },
    updateTitle: () => updateTitle(),
    // The owner's editor shows the chart's own layout while sharing
    // (spec/18_collaboration/DESIGN.md section 6.4).
    sessionChanged: () => app.editor?.setFit(app.collab.role === 'host' && app.collab.active ? 'natural' : 'fill'),
  });
}

async function fetchJson(path, fallback) {
  try {
    const r = await fetch(path, { cache: 'no-cache' });
    return r.ok ? await r.json() : fallback;
  } catch {
    return fallback;
  }
}

function load(key) {
  try {
    return localStorage.getItem(`nymph.${key}`);
  } catch {
    return null;
  }
}

function keep(key, value) {
  try {
    localStorage.setItem(`nymph.${key}`, value);
  } catch {
    // no storage: the setting lasts for the page
  }
}

/** The module URLs with the build's commit, so a deploy is picked up at once. */
function wasmUrl(path) {
  return new URL(`${path}?v=${encodeURIComponent(app.version.commit ?? 'dev')}`, appBase()).href;
}

// ── Routes (section 3.6) ──────────────────────────────────────────────────────

async function route() {
  const params = new URLSearchParams(location.search);
  const hash = new URLSearchParams(location.hash.slice(1));
  const mode = params.get('mode') === 'edit' ? 'edit' : 'view';
  try {
    if (hash.get('y')) {
      const source = await decodeSource(hash.get('y'));
      await openSource(source, params.get('name') ?? 'shared.yaml', { kind: 'link' }, { mode, keepUrl: true });
    } else if (params.get('url')) {
      await commands.run('app_open_url', { url: params.get('url'), mode });
    } else if (params.get('example')) {
      await commands.run('app_open_example', { name: params.get('example'), mode });
    } else if (params.get('template')) {
      await commands.run('app_new_document', { template: params.get('template'), mode });
    } else if (params.get('drive')) {
      await openDrive(params.get('drive'), mode);
    }
  } catch (e) {
    showStart();
    toast(e.message ?? String(e), 6000);
  }
  const invite = hash.get('collab') ?? (!app.doc && !location.search ? app.collab.rejoinToken() : null);
  if (invite) {
    // An invite link (spec/18_collaboration): its token leaves the address
    // bar at once; the tab keeps it to rejoin on reload.
    hash.delete('collab');
    history.replaceState(null, '', `${location.pathname}${location.search}${hash.toString() ? `#${hash}` : ''}`);
    commands.run('collab_join', { token: invite }).catch((e) => toast(e.message ?? String(e), 8000));
  }
  if (hash.get('pair')) {
    // A pairing link from an agent (spec/17_agent_connectors section 7.2):
    // used once, so it leaves the address before anything else.
    const code = hash.get('pair');
    hash.delete('pair');
    history.replaceState(null, '', `${location.pathname}${location.search}${hash.toString() ? `#${hash}` : ''}`);
    if (app.config.agents?.bridge && device.agents.bridge) {
      try {
        await app.bridge.pair(code);
        toast('Connected: the agent can now work on the open chart.');
      } catch (e) {
        toast(e.message ?? String(e), 8000);
      }
    }
  }
}

/** The address that reopens the open document (or the app's base). */
function updateAddress() {
  const doc = app.doc;
  const base = appBase();
  let url = base;
  if (doc?.origin.kind === 'url') {
    url = `${base}?url=${encodeURIComponent(doc.origin.url)}`;
  } else if (doc?.origin.kind === 'example') {
    url = `${base}?example=${encodeURIComponent(doc.origin.example)}`;
  } else if (doc?.origin.kind === 'drive') {
    url = `${base}?drive=${encodeURIComponent(doc.origin.fileId)}`;
  } else if (doc?.origin.kind === 'link' && doc.origin.keepUrl) {
    return;
  }
  // A live chart's address is the app's: the invite token never stays in it.
  const params = new URLSearchParams(location.search);
  if (params.get('device')) {
    url += `${url.includes('?') ? '&' : '?'}device=${params.get('device')}`;
  }
  history.replaceState(null, '', url);
}

// ── Opening ───────────────────────────────────────────────────────────────────

/** Asks to save or discard unsaved changes, and to end a live session; false to stay. */
async function confirmLeave({ collab = false } = {}) {
  if (!collab && app.doc && !(await app.collab.stop({ ask: true }))) {
    return false;
  }
  if (!app.doc?.modified || app.doc.origin.kind === 'collab') {
    return true;
  }
  const choice = await ask('Unsaved changes', `${app.doc.name} has changes that are not saved.`, [
    { label: 'Cancel', value: 'cancel' },
    { label: 'Discard', value: 'discard', kind: 'danger' },
    { label: 'Save', value: 'save', kind: 'primary', focus: true },
  ]);
  if (choice === 'save') {
    try {
      await save();
    } catch {
      return false;
    }
    return !app.doc?.modified;
  }
  if (choice === 'discard') {
    await store.dropDraft(app.doc.id);
    return true;
  }
  return false;
}

/** Opens bytes from anywhere: a YAML text or a nymph PNG. */
async function openBytes(bytes, name, origin, opts = {}) {
  const kind = sniff(bytes);
  if (kind.png) {
    const source = await pngSource(bytes);
    if (source === null) {
      throw new OpenError(
        'This image has no nymph chart in it (it may have been re-encoded by the site it came from). Ask for the YAML, or the original PNG.',
        'png',
      );
    }
    return openSource(source, name, origin, { ...opts, png: true });
  }
  return openSource(kind.text, name, origin, opts);
}

/** Opens a source as the document. */
async function openSource(source, name, origin, opts = {}) {
  if (!(await confirmLeave({ collab: !!opts.collab }))) {
    return false;
  }
  const token = ++app.opening;
  const doc = new Doc({ name, origin, source, png: !!opts.png, id: opts.id });
  if (opts.keepUrl) {
    origin.keepUrl = true;
  }
  await closeDocument();
  if (token !== app.opening) {
    return false;
  }
  app.doc = doc;
  showDocument();
  // Unsaved changes from an earlier visit (a closed tab, a crash).
  const draft = origin.kind === 'collab' ? null : await store.draft(doc.id);
  if (draft && draft.source !== doc.source && draft.base === doc.savedSource && device.directEditing) {
    banner({
      kind: 'info',
      text: `Unsaved changes from ${new Date(draft.savedAt).toLocaleString()} were kept in this browser.`,
      actions: [
        {
          label: 'Restore',
          primary: true,
          run: async () => {
            banner(null);
            doc.source = draft.source;
            await showMode(app.mode, { force: true });
            updateTitle();
          },
        },
        { label: 'Discard', run: () => (store.dropDraft(doc.id), banner(null)) },
      ],
    });
  }
  const wanted = opts.mode === 'edit' && editAvailability().ok ? 'edit' : 'view';
  await showMode(wanted, { force: true });
  updateAddress();
  if (origin.kind !== 'collab') {
    remember(doc);
    app.collab.documentOpened(doc); // resumes this tab's live session after a reload
  }
  return true;
}

/** Records the document in Recent, with a thumbnail when the viewer drew it. */
async function remember(doc) {
  let thumb;
  try {
    if (app.viewer && app.doc === doc) {
      await app.viewer.rendered();
      thumb = await thumbnail(app.viewer.toPng());
    }
  } catch {
    // no thumbnail
  }
  await store.remember(doc, thumb);
}

async function thumbnail(png) {
  const bitmap = await createImageBitmap(new Blob([png], { type: 'image/png' }));
  const w = 320;
  const h = Math.max(1, Math.round((bitmap.height / bitmap.width) * w));
  const c = document.createElement('canvas');
  c.width = w;
  c.height = h;
  c.getContext('2d').drawImage(bitmap, 0, 0, w, h);
  return c.toDataURL('image/png');
}

async function openUrl(url, mode) {
  const got = await busy(fetchUrl(url));
  return openBytes(got.bytes, baseName(got.name), { kind: 'url', url: got.url }, { mode });
}

async function openDrive(fileId, mode) {
  if (!app.drive.available()) {
    throw new OpenError('Google Drive is not set up on this site');
  }
  const got = await busy(app.drive.openById(fileId));
  return openBytes(got.bytes, got.name, got.origin, { mode });
}

async function closeDocument() {
  commands.documentClosed();
  app.collab?.documentClosed();
  destroyEditor();
  app.viewer?.destroy();
  app.viewer = null;
  $('stage').replaceChildren();
  banner(null);
}

// ── The document screen ───────────────────────────────────────────────────────

function showStart() {
  app.doc = null;
  app.mode = 'view';
  $('doc').hidden = true;
  $('start').hidden = false;
  $('doc-title').hidden = true;
  $('doc-actions').hidden = true;
  document.title = 'nymph — view and edit charts';
  app.webmcp?.update(false);
  app.activity?.clear();
  renderStart();
}

function showDocument() {
  $('start').hidden = true;
  $('doc').hidden = false;
  $('doc-title').hidden = false;
  $('doc-actions').hidden = false;
  const edit = editAvailability();
  // A live chart is the owner's: a guest views it (spec/18_collaboration).
  const guest = app.doc?.origin.kind === 'collab';
  $('mode-edit').parentElement.hidden = !device.directEditing || guest;
  $('btn-source').hidden = !device.directEditing || guest;
  $('mode-edit').disabled = !edit.ok;
  $('mode-edit').title = edit.ok ? 'Edit the chart (E). Undo history is not kept when you go back to View.' : edit.why;
  // The mobile page saves nothing (nothing is ever modified there): its
  // downloads are in the Share menu.
  $('btn-save').parentElement.hidden = !device.directEditing || guest;
  app.activity?.clear();
  app.agents?.documentChanged();
  app.webmcp?.update(true);
  app.bridge?.focus(); // the chart's name, for `editor: "web:<name>"`
  updateTitle();
}

function updateTitle() {
  const doc = app.doc;
  if (!doc) {
    return;
  }
  const input = $('doc-name');
  if (document.activeElement !== input) {
    input.value = doc.name;
  }
  $('doc-dot').hidden = !doc.modified;
  $('doc-origin').innerHTML = icons[{ local: 'file', url: 'link', drive: 'drive', example: 'example', template: 'template', paste: 'paste', link: 'share', collab: 'live' }[doc.origin.kind] ?? 'file'];
  $('doc-origin').title = originText(doc);
  document.title = `${doc.modified ? '• ' : ''}${doc.name} — nymph`;
  $('mode-view').setAttribute('aria-pressed', String(app.mode === 'view'));
  $('mode-edit').setAttribute('aria-pressed', String(app.mode === 'edit'));
  $('btn-source').setAttribute('aria-pressed', String(app.mode === 'edit' && !!app.editor?.state().sourceShown));
  $('btn-save').title = doc.savesInPlace ? 'Save (Ctrl+S)' : 'Save: download the file (Ctrl+S)';
}

function originText(doc) {
  const o = doc.origin;
  return {
    local: o.handle ? 'A file on this computer (saved in place)' : 'A file on this computer (saved as a download)',
    url: `From ${o.url}`,
    drive: 'In Google Drive',
    example: 'An example',
    template: 'A new chart from a template',
    paste: 'Pasted',
    link: 'From a shared link',
    new: 'A new chart',
    collab: 'A live chart: it changes as its owner edits it',
  }[o.kind];
}

/** Shows the document in `mode`, switching modules as needed (section 4). */
async function showMode(mode, { force = false, sourceLine } = {}) {
  const doc = app.doc;
  if (!doc) {
    return;
  }
  if (mode === 'edit' && !editAvailability().ok) {
    toast(editAvailability().why);
    mode = 'view';
  }
  if (mode === 'edit' && doc.origin.kind === 'collab') {
    toast('This live chart is its owner\'s: it changes as they edit it.');
    mode = 'view';
  }
  if (!force && mode === app.mode && (mode === 'view' ? app.viewer : app.editor)) {
    return;
  }
  commands.endChange(); // an agent's change group does not span the switch
  const stage = $('stage');
  if (mode === 'view') {
    const source = app.editor ? app.editor.toYaml() : doc.source;
    destroyEditor();
    app.mode = 'view';
    doc.source = source;
    stage.className = 'stage viewing';
    await showViewer(source);
  } else {
    const source = app.viewer?.toYaml?.() ?? doc.source;
    await showEditor(source, { sourceLine });
  }
  app.collab.stageChanged();
  app.collab.documentChanged();
  updateTitle();
}

async function showViewer(source) {
  const stage = $('stage');
  app.viewer?.destroy();
  app.viewer = null;
  stage.replaceChildren();
  const box = el('div', { class: 'viewer-box' });
  stage.append(box);
  let viewer;
  try {
    viewer = await busy(
      createViewer(box, {
        fit: 'source',
        theme: app.chartTheme,
        fileName: app.doc.name.replace(/\.(ya?ml|png)$/i, ''),
        wasmUrl: wasmUrl('nymph-viewer-core.wasm'),
      }),
    );
  } catch (e) {
    stage.replaceChildren(el('div', { class: 'message' }, `The viewer could not start: ${e.message}`));
    return;
  }
  app.viewer = viewer;
  try {
    await viewer.setSource(source);
    if ($('banner').classList.contains('error')) {
      banner(null);
    }
  } catch (e) {
    showSourceError(e);
  }
  viewer.on('view', () => {
    if (app.doc && app.mode === 'view') {
      app.doc.source = viewer.toYaml();
      updateTitle();
      app.collab.documentChanged();
    }
  });
  app.collab.stageChanged();
}

/** The banner of a source that does not load. */
function showSourceError(e) {
  const where = e.line ? `line ${e.line}, column ${e.column}` : '';
  const canEdit = editAvailability().ok;
  $('stage').replaceChildren(
    el('div', { class: 'message' }, el('p', {}, 'This chart does not load.'), el('p', {}, canEdit ? 'Fix it in the source.' : 'Open it on a computer to fix it.')),
  );
  banner({
    kind: 'error',
    where,
    text: e.message,
    dismiss: false,
    actions: canEdit ? [{ label: 'Fix in source', primary: true, run: () => showMode('edit', { force: true, sourceLine: e.line || 1 }) }] : [],
  });
}

async function loadEditorModule() {
  if (!app.editorModule) {
    app.editorModule = import('../editor/nymph-editor.js');
    app.editorModule.catch(() => (app.editorModule = null));
  }
  return app.editorModule;
}

async function showEditor(source, { sourceLine } = {}) {
  const stage = $('stage');
  const { createEditor } = await busy(loadEditorModule());
  const box = el('div', { class: 'editor-box' });
  let editor;
  try {
    editor = await busy(
      createEditor(box, {
        source,
        repair: true,
        fileName: app.doc.name,
        theme: app.chartTheme,
        appearance: pageAppearance(),
        // A live session: the chart's own layout, as the guests see it.
        fit: app.collab?.role === 'host' && app.collab.active ? 'natural' : 'fill',
        wasmUrl: wasmUrl('editor/nymph-editor-core.wasm'),
        fontBaseUrl: new URL('fonts/', appBase()).href,
      }),
    );
  } catch (e) {
    toast(`The editor could not start: ${e.message}`, 6000);
    return;
  }
  // The viewer goes once the editor is ready.
  app.viewer?.destroy();
  app.viewer = null;
  stage.replaceChildren(box);
  stage.className = 'stage editing';
  // createEditor measured a detached box: give it the stage's size now.
  editor.resize();
  app.editor = editor;
  app.mode = 'edit';
  const doc = app.doc;
  doc.source = editor.source();
  if (editor.repairing()) {
    banner({ kind: 'error', text: 'This chart does not load. Fix it in the source pane; the editor opens once it does.', dismiss: false });
  } else if ($('banner').classList.contains('error')) {
    banner(null);
  }
  if (sourceLine || editor.repairing()) {
    editor.showSource(true, sourceLine ?? 0);
  }
  editor.on('change', () => {
    doc.source = editor.source();
    if (!editor.repairing() && $('banner').classList.contains('error')) {
      banner(null);
    }
    updateTitle();
    scheduleDraft();
    app.collab.documentChanged();
  });
  editor.on('save', () => commands.run('app_save').catch((e) => toast(e.message)));
  editor.on('csv-export', (d) => download(d.csv, d.fileName, 'text/csv'));
  editor.on('csv-import', (d) => importCsv(d.table));
  editor.on('error', (e) => toast(e.message ?? String(e)));
  for (const def of editor.toolDefinitions()) {
    commands.define({
      name: def.name,
      title: def.title,
      description: def.description,
      inputSchema: def.inputSchema,
      scope: 'editor',
      effect: def.annotations?.readOnlyHint ? 'read' : 'edit',
      run: (args, { origin }) => editor.tool(def.name, args, { origin }),
    });
  }
  editor.focus();
}

function destroyEditor() {
  if (app.editor) {
    commands.remove((name) => name.startsWith('editor_'));
    app.editor.destroy();
    app.editor = null;
  }
}

function importCsv(table) {
  const input = $('csv-input');
  input.onchange = async () => {
    const file = input.files?.[0];
    input.value = '';
    if (file && app.editor) {
      try {
        app.editor.importCsv(table, await file.text());
      } catch (e) {
        toast(e.message);
      }
    }
  };
  input.click();
}

function scheduleDraft() {
  clearTimeout(app.draftTimer);
  app.draftTimer = setTimeout(writeDraft, 2000);
}

function writeDraft() {
  const doc = app.doc;
  if (doc?.modified) {
    store.saveDraft(doc);
  } else if (doc) {
    store.dropDraft(doc.id);
  }
}

// ── Saving (section 3.3) ──────────────────────────────────────────────────────

/** The bytes a save writes: the source with the view, as YAML or PNG. */
function bytesOf(kind) {
  const yaml = app.editor ? app.editor.toYaml() : app.viewer ? app.viewer.toYaml() : app.doc.source;
  if (kind === 'png') {
    return { bytes: app.editor ? app.editor.toPng() : app.viewer.toPng(), mime: 'image/png', yaml };
  }
  return { bytes: yaml, mime: 'application/yaml', yaml };
}

function savedAs(yaml) {
  const doc = app.doc;
  doc.source = yaml;
  doc.savedSource = yaml;
  app.editor?.markSaved();
  store.dropDraft(doc.id);
  updateTitle();
  remember(doc);
}

/** Save: back to where it came from where possible, else a download. */
async function save() {
  const doc = app.doc;
  if (app.editor?.repairing()) {
    // The text as it is (it does not load yet), as YAML.
    const text = app.editor.paneText() || doc.source;
    if (doc.origin.kind === 'local' && doc.origin.handle && !doc.png) {
      await writeHandle(doc.origin.handle, text);
    } else {
      download(text, doc.fileName('yaml'), 'application/yaml');
    }
    toast('Saved the text as it is: it does not load yet');
    return;
  }
  const kind = doc.png ? 'png' : 'yaml';
  const { bytes, mime, yaml } = bytesOf(kind);
  if (doc.origin.kind === 'local' && doc.origin.handle) {
    await writeHandle(doc.origin.handle, bytes);
    savedAs(yaml);
    toast(`Saved ${doc.name}`);
  } else if (doc.origin.kind === 'drive') {
    try {
      await busy(app.drive.save(doc, typeof bytes === 'string' ? new TextEncoder().encode(bytes) : bytes, mime));
    } catch (e) {
      if (e.kind !== 'conflict') {
        throw e;
      }
      const choice = await ask('The file changed in Google Drive', 'Someone changed it since you opened it.', [
        { label: 'Cancel', value: 'cancel' },
        { label: 'Save as a copy', value: 'copy' },
        { label: 'Overwrite', value: 'overwrite', kind: 'danger' },
      ]);
      if (choice === 'overwrite') {
        await busy(app.drive.save(doc, typeof bytes === 'string' ? new TextEncoder().encode(bytes) : bytes, mime, { overwrite: true }));
      } else if (choice === 'copy') {
        return saveToDrive();
      } else {
        return;
      }
    }
    savedAs(yaml);
    toast(`Saved ${doc.name} to Google Drive`);
  } else {
    download(bytes, doc.fileName(kind), mime);
    savedAs(yaml);
    toast(`Downloaded ${doc.fileName(kind)}`);
  }
}

async function saveToDrive() {
  const doc = app.doc;
  const kind = doc.png ? 'png' : 'yaml';
  const { bytes, mime, yaml } = bytesOf(kind);
  const origin = await busy(app.drive.saveAs(doc, typeof bytes === 'string' ? new TextEncoder().encode(bytes) : bytes, doc.fileName(kind), mime));
  if (origin) {
    doc.origin = { kind: 'drive', fileId: origin.fileId, revision: origin.revision };
    doc.name = origin.name ?? doc.name;
    savedAs(yaml);
    updateAddress();
    toast(`Saved ${doc.name} to Google Drive`);
  }
}

/** Save as…: a new file on this computer (File System Access), or a download. */
async function saveAs() {
  const doc = app.doc;
  const kind = doc.png ? 'png' : 'yaml';
  if (typeof window.showSaveFilePicker !== 'function') {
    return commands.run('app_download', { kind });
  }
  let handle;
  try {
    handle = await window.showSaveFilePicker({
      suggestedName: doc.fileName(kind),
      types: [
        kind === 'png' ? { description: 'PNG with its nymph source', accept: { 'image/png': ['.png'] } } : { description: 'nymph YAML', accept: { 'application/yaml': ['.yaml', '.yml'] } },
      ],
    });
  } catch (e) {
    if (e.name === 'AbortError') {
      return;
    }
    throw e;
  }
  const { bytes, yaml } = bytesOf(kind);
  await writeHandle(handle, bytes);
  doc.origin = { kind: 'local', handle };
  doc.name = handle.name;
  savedAs(yaml);
  updateAddress();
  toast(`Saved ${doc.name}`);
}

// ── Commands (section 3.11) ───────────────────────────────────────────────────

const str = (description) => ({ type: 'string', description });
const schema = (properties = {}, required = []) => ({ type: 'object', properties, required, additionalProperties: false });

function defineCommands() {
  const editOk = () => editAvailability().ok;

  // ── Document: the source text, with either module ──
  commands.define({
    name: 'document_state',
    title: 'Document state',
    description: 'The open chart: name, where it came from, mode (view or edit), whether it has unsaved changes, and the error it shows if it does not load.',
    scope: 'document',
    run: () => ({
      name: app.doc.name,
      origin: app.doc.origin.kind,
      mode: app.mode,
      modified: app.doc.modified,
      png: app.doc.png,
      repairing: !!app.editor?.repairing(),
      version: app.editor?.version() ?? null,
      error: $('banner').classList.contains('error') ? $('banner').textContent : '',
    }),
  });
  commands.define({
    name: 'document_source',
    title: 'Document source',
    description: 'The whole YAML source of the open chart.',
    scope: 'document',
    run: () => ({ source: app.editor ? app.editor.source() : app.doc.source }),
  });
  commands.define({
    name: 'document_set_source',
    title: 'Set document source',
    description: 'Replaces the whole YAML source of the open chart, as one undo step in Edit mode. Refused, changing nothing, when it does not load (the loader\'s message says why).',
    scope: 'document',
    effect: 'edit',
    inputSchema: schema({ source: str('The new YAML source.'), label: str('What the change does, for the undo history.') }, ['source']),
    run: async ({ source, label }, { origin }) => {
      if (app.doc.origin.kind === 'collab') {
        throw new Error('this live chart is its owner\'s: it changes only when they edit it');
      }
      if (app.editor) {
        await app.editor.setSource(source, { asChange: true, label: label ?? 'Edit source', origin });
        app.doc.source = app.editor.source();
      } else {
        await app.viewer.setSource(source);
        banner(null);
        app.doc.source = source;
      }
      updateTitle();
      scheduleDraft();
      app.collab.documentChanged();
      return { ok: true };
    },
  });
  commands.define({
    name: 'document_check',
    title: 'Check a source',
    description: 'Checks a YAML source without showing it: ok, or the loader\'s message with line and column.',
    scope: 'app',
    inputSchema: schema({ source: str('A nymph YAML source.') }, ['source']),
    run: async ({ source }) => {
      const probe = await createViewer(null, { wasmUrl: wasmUrl('nymph-viewer-core.wasm') });
      try {
        await probe.setSource(source);
        return { ok: true };
      } catch (e) {
        return { ok: false, line: e.line ?? 0, column: e.column ?? 0, message: e.message };
      } finally {
        probe.destroy();
      }
    },
  });
  commands.define({
    name: 'document_image',
    title: 'Document image',
    description: 'A PNG of the chart as it is on screen (zoom and pan included).',
    scope: 'document',
    run: async () => {
      if (app.editor) {
        const r = app.editor.tool('editor_view_image', { max_width: 1200 });
        return { content: [{ type: 'image', data: r.image, mimeType: 'image/png' }] };
      }
      const png = app.viewer.toPng();
      let s = '';
      for (let i = 0; i < png.length; i += 0x8000) {
        s += String.fromCharCode(...png.subarray(i, i + 0x8000));
      }
      return { content: [{ type: 'image', data: btoa(s), mimeType: 'image/png' }] };
    },
  });
  commands.define({
    name: 'document_select',
    title: 'Select an element',
    description: 'Selects an element (by its path, e.g. root/children/1) in the editor and outlines it.',
    scope: 'editor',
    inputSchema: schema({ path: str('Element path.'), field: str('A field of it.') }, ['path']),
    run: (args) => app.editor.tool('editor_select', args),
  });

  // ── Agents (spec/17_agent_connectors/DESIGN.md section 4) ──
  commands.define({
    name: 'document_begin_change',
    title: 'Begin a change',
    description:
      'Starts a change made of several tool calls, so the user can undo it as one step: call it before the first, and document_end_change after the last.',
    scope: 'document',
    inputSchema: schema({ label: str('What the change does, as the user will see it in the undo history, e.g. "Add the billing service".') }, ['label']),
    run: ({ label }, { origin }) => {
      if (origin === 'user') {
        throw new Error('change groups are for agents');
      }
      return commands.beginChange(origin, label);
    },
  });
  commands.define({
    name: 'document_end_change',
    title: 'End a change',
    description: 'Ends the change document_begin_change started.',
    scope: 'document',
    run: (args, { origin }) => ({ ended: commands.endChange(origin) }),
  });
  commands.define({
    name: 'app_read_guide',
    title: 'Read a guide',
    description:
      'Reads nymph\'s documentation for agents: web-app (how to work in this app: read it first), agent-guide (the format, with examples), format (the source format in full), elements (every element type), or elements/<Diagram> (one family, e.g. elements/Flowchart).',
    inputSchema: schema({ name: str('web-app, agent-guide, format, elements, or elements/<Diagram>.') }, ['name']),
    run: async ({ name }) => {
      if (!/^(web-app|agent-guide|format|elements|elements\/[A-Za-z0-9]+)$/.test(name)) {
        throw new Error('name must be web-app, agent-guide, format, elements, or elements/<Diagram>');
      }
      const r = await fetch(`agent/${name}.md`);
      if (!r.ok) {
        throw new Error(`there is no guide '${name}'`);
      }
      return r.text();
    },
  });
  commands.define({
    name: 'app_example_source',
    title: 'Example source',
    description: 'The YAML of one of nymph\'s examples (examples/index.json lists them), without opening it: a starting point to adapt.',
    inputSchema: schema({ name: str('Example name, e.g. line or flowchart.') }, ['name']),
    run: async ({ name }) => {
      if (!/^[A-Za-z0-9_-]+$/.test(name)) {
        throw new Error('an example name is letters, digits, - and _');
      }
      const r = await fetch(`examples/${name}.yaml`);
      if (!r.ok) {
        throw new Error(`there is no example '${name}'`);
      }
      return { name, source: await r.text() };
    },
  });

  // ── App ──
  commands.define({
    name: 'app_list_templates',
    title: 'List templates',
    description: 'The templates a new chart can start from: name, title, family.',
    run: () => fetchJson('templates/index.json', []).then((list) => ({ templates: list })),
  });
  commands.define({
    name: 'app_new_document',
    title: 'New chart',
    description: 'Opens a new chart from a YAML source or a template name (app_list_templates).',
    effect: 'edit',
    inputSchema: schema({ source: str('A YAML source.'), template: str('A template name.'), name: str('The file name.'), mode: { type: 'string', enum: ['view', 'edit'] } }),
    run: async ({ source, template, name, mode }) => {
      if (template) {
        const r = await fetch(`templates/${encodeURIComponent(template)}.yaml`);
        if (!r.ok) {
          throw new Error(`there is no template '${template}'`);
        }
        source = await r.text();
      }
      if (typeof source !== 'string') {
        throw new Error('give a source or a template');
      }
      await openSource(source, name ?? `${template ?? 'chart'}.yaml`, { kind: template ? 'template' : 'new' }, { mode: mode ?? (device.directEditing ? 'edit' : 'view') });
      return { ok: true };
    },
  });
  commands.define({
    name: 'app_open_url',
    title: 'Open a URL',
    description: 'Opens a chart (YAML or nymph PNG) from a URL. GitHub file pages and gists open their raw file.',
    effect: 'external',
    inputSchema: schema({ url: str('An http(s) URL.'), mode: { type: 'string', enum: ['view', 'edit'] } }, ['url']),
    run: async ({ url, mode }) => ((await openUrl(url, mode)) ? { ok: true } : { ok: false }),
  });
  commands.define({
    name: 'app_open_example',
    title: 'Open an example',
    description: 'Opens one of nymph\'s examples by name (examples/index.json).',
    inputSchema: schema({ name: str('Example name, e.g. line.'), mode: { type: 'string', enum: ['view', 'edit'] } }, ['name']),
    run: async ({ name, mode }) => {
      const r = await fetch(`examples/${encodeURIComponent(name)}.yaml`);
      if (!r.ok) {
        throw new Error(`there is no example '${name}'`);
      }
      await openSource(await r.text(), `${name}.yaml`, { kind: 'example', example: name }, { mode });
      return { ok: true };
    },
  });
  commands.define({
    name: 'app_open_file',
    agent: false, // opens a picker or a dialog for the user
    title: 'Open a file',
    description: 'Asks the user for a file on this computer.',
    effect: 'external',
    run: async () => {
      const picked = await pickFile($('file-input'));
      if (picked) {
        await openBytes(await readFile(picked.file), picked.file.name, { kind: 'local', handle: picked.handle });
      }
    },
  });
  commands.define({
    name: 'app_open_url_dialog',
    agent: false, // opens a picker or a dialog for the user
    title: 'Open from URL',
    description: 'Asks the user for a URL to open.',
    effect: 'external',
    run: async () => {
      const url = await dialog('Open from URL', (body, close) => {
        const input = el('input', { type: 'url', placeholder: 'https://… a .yaml or a nymph .png', autofocus: true, 'aria-label': 'URL' });
        const form = el('form', {}, input, dialogButtons(close, [{ label: 'Cancel', value: undefined }, { label: 'Open', value: 'submit', kind: 'primary' }]));
        form.onsubmit = (e) => (e.preventDefault(), close(input.value));
        form.querySelector('.primary').type = 'submit';
        form.querySelector('.primary').onclick = null;
        body.append(el('p', {}, 'A file on GitHub, a gist, or any site that lets other sites read it.'), form);
      });
      if (url) {
        await commands.run('app_open_url', { url });
      }
    },
  });
  commands.define({
    name: 'app_open_drive',
    agent: false, // opens a picker or a dialog for the user
    title: 'Open from Google Drive',
    description: 'Opens a chart the user picks in Google Drive.',
    effect: 'external',
    available: () => app.drive?.available(),
    run: async () => {
      const got = await app.drive.open();
      if (got) {
        await openBytes(got.bytes, got.name, got.origin);
      }
    },
  });
  commands.define({
    name: 'app_new_dialog',
    agent: false, // opens a picker or a dialog for the user
    title: 'New chart…',
    description: 'Shows the templates to start a new chart from.',
    run: async () => {
      const list = await fetchJson('templates/index.json', []);
      const name = await dialog('New chart', (body, close) => {
        body.append(
          el('p', {}, 'Start from a template:'),
          el(
            'ul',
            { class: 'cards small' },
            list.map((t) => el('li', {}, el('button', { type: 'button', class: 'card', onclick: () => close(t.name) }, el('div', { class: 'label' }, el('strong', {}, t.title), el('span', {}, t.family))))),
          ),
          dialogButtons(close, [{ label: 'Cancel', value: undefined }]),
        );
      });
      if (name) {
        await commands.run('app_new_document', { template: name });
      }
    },
  });
  commands.define({
    name: 'app_set_mode',
    title: 'View or edit',
    description: 'Switches between viewing the chart and editing it.',
    scope: 'document',
    inputSchema: schema({ mode: { type: 'string', enum: ['view', 'edit'] } }, ['mode']),
    available: () => true,
    run: async ({ mode }) => {
      if (mode === 'edit' && !editOk()) {
        throw new Error(editAvailability().why);
      }
      if (mode === 'edit' && app.doc.origin.kind === 'collab') {
        throw new Error('this live chart is its owner\'s: it changes only when they edit it');
      }
      await showMode(mode);
      return { mode: app.mode };
    },
  });
  commands.define({
    name: 'app_show_source',
    title: 'Source',
    description: 'Shows or hides the YAML source beside the chart (switching to Edit).',
    scope: 'document',
    available: editOk,
    inputSchema: schema({ show: { type: 'boolean' }, line: { type: 'integer' } }),
    run: async ({ show, line }) => {
      if (app.mode !== 'edit') {
        await showMode('edit', { sourceLine: line ?? 1 });
        return { shown: true };
      }
      const on = show ?? !app.editor.state().sourceShown;
      app.editor.showSource(on, line ?? 0);
      updateTitle();
      return { shown: on };
    },
  });
  commands.define({
    name: 'app_save',
    title: 'Save',
    description: 'Saves the chart back to where it came from (the file, Google Drive), or downloads it.',
    scope: 'document',
    effect: 'external',
    available: () => device.directEditing,
    run: () => save(),
  });
  commands.define({
    name: 'app_save_as',
    agent: false, // opens a picker or a dialog for the user
    title: 'Save as',
    description: 'Saves the chart as a new file on this computer.',
    scope: 'document',
    effect: 'external',
    available: () => device.directEditing,
    run: () => saveAs(),
  });
  commands.define({
    name: 'app_save_drive',
    title: 'Save to Google Drive',
    description: 'Saves the chart as a new file in Google Drive.',
    scope: 'document',
    effect: 'external',
    available: () => device.directEditing && app.drive?.available(),
    run: () => saveToDrive(),
  });
  commands.define({
    name: 'app_download',
    title: 'Download',
    description: 'Downloads the chart as YAML, or as a PNG that carries its YAML.',
    scope: 'document',
    effect: 'external',
    inputSchema: schema({ kind: { type: 'string', enum: ['yaml', 'png'] } }, ['kind']),
    run: ({ kind }) => {
      const b = bytesOf(kind);
      download(b.bytes, app.doc.fileName(kind), b.mime);
      return { file: app.doc.fileName(kind) };
    },
  });
  commands.define({
    name: 'app_copy_link',
    title: 'Copy link',
    description: 'Copies a link that reopens the chart.',
    scope: 'document',
    effect: 'external',
    run: async () => {
      const link = await linkFor(app.doc, bytesOf('yaml').yaml, app.mode);
      if (link.long && !(await ask('A long link', 'This chart is large: some chat apps cut links this long. Copy it anyway, or share the file instead.', [
        { label: 'Cancel', value: false },
        { label: 'Download the file', value: 'file' },
        { label: 'Copy anyway', value: true, kind: 'primary', focus: true },
      ]).then((v) => (v === 'file' ? (commands.run('app_download', { kind: 'yaml' }), false) : v)))) {
        return { copied: false };
      }
      await navigator.clipboard.writeText(link.url);
      toast(link.kind === 'drive' ? 'Link copied: it works for people the file is shared with' : 'Link copied');
      return { copied: true, url: link.url };
    },
  });
  commands.define({
    name: 'app_copy_embed',
    title: 'Copy embed code',
    description: 'Copies HTML that shows the chart live in any web page.',
    scope: 'document',
    effect: 'external',
    run: async () => {
      await navigator.clipboard.writeText(embedCode(withoutView(bytesOf('yaml').yaml)));
      toast('Embed code copied');
      return { copied: true };
    },
  });
  commands.define({
    name: 'app_reset_view',
    title: 'Reset view',
    description: 'Puts zoom and pan back as the source describes them.',
    scope: 'document',
    run: () => (app.editor ?? app.viewer)?.resetView(),
  });
  commands.define({
    name: 'app_set_theme',
    title: 'App theme',
    description: 'The app\'s own colours: auto (as the system), light or dark.',
    inputSchema: schema({ theme: { type: 'string', enum: ['auto', 'light', 'dark'] } }, ['theme']),
    run: ({ theme }) => {
      applyAppTheme(theme);
      keep('theme', theme);
      return { theme };
    },
  });
  commands.define({
    name: 'app_set_chart_theme',
    title: 'Chart theme',
    description:
      'Shows the chart in another chart theme (an id from app_list_themes) without changing the file, or in its own (source).',
    inputSchema: schema({ theme: { type: 'string', description: "'source', or a theme id (light, dark, ocean, ...)" } }, ['theme']),
    run: async ({ theme }) => {
      await (app.editor ?? app.viewer)?.setTheme(theme === 'source' ? '' : theme);
      app.chartTheme = theme;
      keep('chartTheme', theme);
      return { theme };
    },
  });
  commands.define({
    name: 'app_list_themes',
    title: 'Chart themes',
    description: 'The chart themes: id, name, description, base colours and swatch colours.',
    scope: 'document',
    run: () => ({ themes: chartThemes() }),
  });
  commands.define({
    name: 'app_set_file_theme',
    title: 'Set the chart theme',
    description:
      "Writes the chart's theme into the file (image.theme), as one undo step; Edit mode only. 'light' is the default.",
    scope: 'editor',
    effect: 'edit',
    inputSchema: schema({ theme: { type: 'string', description: 'A theme id from app_list_themes.' } }, ['theme']),
    run: async ({ theme }, { origin }) => {
      if (!app.editor) {
        throw new Error('open the chart in the editor to change its theme');
      }
      if (app.chartTheme !== 'source') {
        // Show the file's theme, not a preview over it.
        await app.editor.setTheme('');
        app.chartTheme = 'source';
        keep('chartTheme', 'source');
      }
      return app.editor.tool('editor_set_fields', { path: 'image', fields: { theme: theme === 'light' ? null : theme } }, { origin });
    },
  });
  commands.define({
    name: 'app_choose_theme',
    agent: false, // opens a picker or a dialog for the user
    title: 'Theme…',
    description: 'Opens the theme picker: in Edit mode it sets the file\'s theme, in View mode it previews one.',
    scope: 'document',
    run: () => chooseTheme(),
  });
  commands.define({
    name: 'app_close',
    title: 'Close',
    description: 'Closes the chart and goes back to the start page (asking about unsaved changes).',
    scope: 'document',
    run: async () => {
      if (await confirmLeave()) {
        await closeDocument();
        showStart();
        updateAddress();
      }
    },
  });
  commands.define({ name: 'app_about', agent: false, title: 'About', description: 'Version and licences.', run: () => about() });
  commands.define({ name: 'app_shortcuts', agent: false, title: 'Keyboard shortcuts', description: 'The keyboard shortcuts.', run: () => shortcuts() });
}

/** The chart themes the loaded module knows, [] before one is loaded. */
function chartThemes() {
  try {
    return (app.editor ?? app.viewer)?.themes() ?? [];
  } catch {
    return [];
  }
}

/** The theme of the open file: its image.theme, "light" when it has none. */
function fileTheme() {
  const m = /^image:\s*(?:\{[^}\n]*\btheme:\s*([\w-]+)|\n(?:[ \t]+.*\n)*?[ \t]+theme:\s*([\w-]+))/m.exec(app.doc?.source ?? '');
  return m ? (m[1] ?? m[2]) : 'light';
}

/**
 * The theme picker (like a slide editor's): a card per chart theme with
 * its colours. Edit mode writes the choice into the file; View mode
 * previews it.
 */
async function chooseTheme() {
  const themes = chartThemes();
  const editing = app.mode === 'edit' && app.editor;
  const current = editing || app.chartTheme === 'source' ? fileTheme() : app.chartTheme;
  const choice = await dialog('Theme', (body, close) => {
    body.append(
      el(
        'p',
        { class: 'muted' },
        editing
          ? 'Colours, fonts and series palette for everything the chart does not set itself. Saved in the file (image.theme); Ctrl+Z undoes it.'
          : 'A preview: the file keeps its own theme. Open the chart in Edit to change it.',
      ),
    );
    const grid = el('div', { class: 'theme-grid', role: 'listbox', 'aria-label': 'Chart themes' });
    if (!editing) {
      grid.append(
        el(
          'button',
          { type: 'button', class: `theme-card own${app.chartTheme === 'source' ? ' on' : ''}`, onclick: () => close('source') },
          el('span', { class: 'swatch own' }, 'Aa'),
          el('span', { class: 'theme-name' }, 'Its own theme'),
          el('span', { class: 'theme-desc' }, 'As the file sets it.'),
        ),
      );
    }
    for (const t of themes) {
      const swatch = el(
        'span',
        { class: 'swatch', style: `background:${t.background};color:${t.text_color}` },
        el('span', { class: 'swatch-title' }, 'Aa'),
        el('span', { class: 'swatch-dots' }, t.palette.slice(0, 5).map((c) => el('i', { style: `background:${c}` }))),
      );
      grid.append(
        el(
          'button',
          {
            type: 'button',
            role: 'option',
            'aria-selected': String(t.id === current),
            class: `theme-card${t.id === current ? ' on' : ''}`,
            title: t.description,
            onclick: () => close(t.id),
          },
          swatch,
          el('span', { class: 'theme-name' }, t.name),
          el('span', { class: 'theme-desc' }, t.description),
        ),
      );
    }
    body.append(grid, dialogButtons(close, [{ label: 'Close', value: undefined }]));
  });
  if (choice === undefined) {
    return;
  }
  if (editing) {
    await commands.run('app_set_file_theme', { theme: choice === 'source' ? 'light' : choice });
  } else {
    await commands.run('app_set_chart_theme', { theme: choice });
  }
}

function applyAppTheme(theme) {
  if (theme === 'auto') {
    delete document.documentElement.dataset.theme;
  } else {
    document.documentElement.dataset.theme = theme;
  }
  app.editor?.setAppearance(pageAppearance());
}

/** The page's look for the editor panel: light or dark as shown, and its accent. */
function pageAppearance() {
  const chosen = document.documentElement.dataset.theme;
  const dark = chosen ? chosen === 'dark' : matchMedia('(prefers-color-scheme: dark)').matches;
  const accent = getComputedStyle(document.documentElement).getPropertyValue('--accent').trim();
  return { dark, accent, accentDark: accent };
}

// "auto" follows the system as it changes.
matchMedia('(prefers-color-scheme: dark)').addEventListener?.('change', () => app.editor?.setAppearance(pageAppearance()));

// ── Controls ──────────────────────────────────────────────────────────────────

/** Runs a command for a control, showing a failure as a toast. */
function act(name, args) {
  commands.run(name, args).catch((e) => toast(e.message ?? String(e), 6000));
}

function wireControls() {
  document.querySelectorAll('[data-command]').forEach((b) => b.addEventListener('click', () => act(b.dataset.command)));
  $('home').addEventListener('click', () => (app.doc ? act('app_close') : null));
  $('btn-drive').hidden = !app.drive.available();
  $('mode-view').addEventListener('click', () => act('app_set_mode', { mode: 'view' }));
  $('mode-edit').addEventListener('click', () => act('app_set_mode', { mode: 'edit' }));
  $('btn-source').addEventListener('click', () => act('app_show_source'));
  $('btn-save').addEventListener('click', () => act('app_save'));
  $('btn-more').innerHTML = icons.more;
  $('btn-save-menu').innerHTML = icons.caret;
  $('btn-share').prepend(el('span', { html: icons.share, style: 'display:inline-flex' }));
  $('btn-save-menu').addEventListener('click', (e) => {
    const doc = app.doc;
    const edit = device.directEditing;
    showMenu(e.currentTarget, [
      edit && { label: doc.savesInPlace ? 'Save' : 'Save (download)', icon: 'save', note: 'Ctrl+S', run: () => act('app_save') },
      edit && typeof window.showSaveFilePicker === 'function' && { label: 'Save as…', icon: 'file', run: () => act('app_save_as') },
      edit && app.drive.available() && { label: 'Save to Google Drive…', icon: 'drive', run: () => act('app_save_drive') },
      edit && '-',
      { label: 'Download YAML', icon: 'download', run: () => act('app_download', { kind: 'yaml' }) },
      { label: 'Download PNG', icon: 'image', note: 'carries the YAML', run: () => act('app_download', { kind: 'png' }) },
      app.collab.wb && { label: 'Download PNG with drawings', icon: 'image', run: () => act('collab_download_png') },
    ].filter(Boolean));
  });
  $('btn-share').addEventListener('click', (e) =>
    showMenu(e.currentTarget, [
      { label: 'Copy link', icon: 'link', run: () => act('app_copy_link') },
      { label: 'Copy embed code', icon: 'code', run: () => act('app_copy_embed') },
      app.collab.wb ? null : { label: 'Share live…', icon: 'live', hidden: $('btn-collab').hidden || app.doc?.origin.kind === 'collab', run: () => act('collab_start') },
      '-',
      { label: 'Download PNG', icon: 'image', run: () => act('app_download', { kind: 'png' }) },
      app.collab.wb && { label: 'Download PNG with drawings', icon: 'image', run: () => act('collab_download_png') },
      { label: 'Download YAML', icon: 'download', run: () => act('app_download', { kind: 'yaml' }) },
    ]),
  );
  $('btn-more').addEventListener('click', (e) => {
    const theme = load('theme') ?? 'auto';
    const mark = (on) => (on ? '✓' : '');
    // Under 720 px the bar keeps View | Edit and Save; the rest is here.
    const narrow = matchMedia('(max-width: 720px)').matches;
    showMenu(e.currentTarget, [
      ...(app.doc && narrow
        ? [
            device.directEditing && { label: 'Source', icon: 'code', note: 'Ctrl+U', run: () => act('app_show_source') },
            { label: 'Copy link', icon: 'link', run: () => act('app_copy_link') },
            { label: 'Copy embed code', icon: 'code', run: () => act('app_copy_embed') },
            { label: 'Download PNG', icon: 'image', run: () => act('app_download', { kind: 'png' }) },
            { label: 'Download YAML', icon: 'download', run: () => act('app_download', { kind: 'yaml' }) },
            '-',
          ]
        : []),
      ...(app.doc
        ? [
            { label: 'Reset view', icon: 'reset', run: () => act('app_reset_view') },
            { label: 'Theme…', icon: 'theme', run: () => act('app_choose_theme') },
            '-',
          ]
        : []),
      ...['auto', 'light', 'dark'].map((t) => ({ label: `App: ${t === 'auto' ? 'as the system' : t}`, icon: 'theme', note: mark(theme === t), run: () => act('app_set_theme', { theme: t }) })),
      '-',
      !device.mobile && { label: 'Keyboard shortcuts', icon: 'keyboard', note: '?', run: () => act('app_shortcuts') },
      { label: 'About nymph', icon: 'info', run: () => act('app_about') },
      app.doc && { label: 'Close chart', icon: 'close', run: () => act('app_close') },
    ].filter(Boolean));
  });
  $('doc-name').addEventListener('change', (e) => {
    if (app.doc) {
      app.doc.name = baseName(e.target.value, app.doc.name);
      updateTitle();
    }
  });
  $('doc-name').addEventListener('keydown', (e) => {
    if (e.key === 'Enter' || e.key === 'Escape') {
      e.target.blur();
    }
  });
  $('recent-clear').addEventListener('click', async () => {
    await store.clearAll();
    renderStart();
  });
  window.addEventListener('beforeunload', (e) => {
    if (app.doc?.modified) {
      writeDraft();
      e.preventDefault();
    }
  });
  document.addEventListener('visibilitychange', () => {
    if (document.visibilityState === 'hidden') {
      writeDraft();
    }
  });
}

function wireDropAndPaste() {
  const overlay = $('drop-overlay');
  let depth = 0;
  window.addEventListener('dragenter', (e) => {
    if ([...(e.dataTransfer?.types ?? [])].includes('Files')) {
      depth++;
      overlay.hidden = false;
    }
  });
  window.addEventListener('dragleave', () => {
    depth = Math.max(0, depth - 1);
    overlay.hidden = depth > 0 ? overlay.hidden : true;
  });
  window.addEventListener('dragover', (e) => e.preventDefault());
  window.addEventListener('drop', async (e) => {
    e.preventDefault();
    depth = 0;
    overlay.hidden = true;
    const item = [...(e.dataTransfer?.items ?? [])].find((i) => i.kind === 'file');
    const handle = item?.getAsFileSystemHandle ? await item.getAsFileSystemHandle().catch(() => null) : null;
    const file = e.dataTransfer?.files?.[0];
    if (file) {
      openBytes(await readFile(file), file.name, { kind: 'local', handle: handle?.kind === 'file' ? handle : null }).catch((err) => toast(err.message, 6000));
    }
  });
  document.addEventListener('paste', async (e) => {
    const target = e.composedPath?.()[0] ?? e.target; // inside a shadow root too
    if (target.closest?.('input, textarea, .nymph-editor-keys') || app.mode === 'edit') {
      return;
    }
    const file = [...(e.clipboardData?.files ?? [])][0];
    const text = e.clipboardData?.getData('text');
    try {
      if (file) {
        e.preventDefault();
        await openBytes(await readFile(file), file.name || 'pasted.png', { kind: 'paste' });
      } else if (text && /\broot\s*:/.test(text)) {
        e.preventDefault();
        await openSource(text, 'pasted.yaml', { kind: 'paste' });
      }
    } catch (err) {
      toast(err.message, 6000);
    }
  });
}

function wireKeys() {
  document.addEventListener(
    'keydown',
    (e) => {
      const ctrl = e.ctrlKey || e.metaKey;
      // The real target, inside a shadow root too (the whiteboard's note box).
      const target = e.composedPath?.()[0] ?? e.target;
      const typing = target.closest?.('input, textarea:not(.nymph-editor-keys), [contenteditable]');
      if (ctrl && e.shiftKey && (e.key === 'a' || e.key === 'A') && app.doc && !$('btn-agents').hidden) {
        e.preventDefault();
        e.stopPropagation();
        app.agents.toggle();
      } else if (ctrl && e.shiftKey && (e.key === 'l' || e.key === 'L') && app.doc && !$('btn-collab').hidden) {
        e.preventDefault();
        e.stopPropagation();
        act(app.collab.session ? 'collab_panel' : 'collab_start');
      } else if (ctrl && (e.key === 'o' || e.key === 'O')) {
        e.preventDefault();
        e.stopPropagation();
        act('app_open_file');
      } else if (ctrl && (e.key === 's' || e.key === 'S') && app.doc && app.mode === 'view') {
        e.preventDefault();
        act('app_save');
      } else if (ctrl && (e.key === 'u' || e.key === 'U') && app.doc && app.mode === 'view' && device.directEditing) {
        e.preventDefault();
        act('app_show_source');
      } else if (!typing && !ctrl && !e.altKey && app.mode === 'view') {
        if (e.key === 'e' && app.doc && editAvailability().ok) {
          act('app_set_mode', { mode: 'edit' });
        } else if (e.key === '?') {
          act('app_shortcuts');
        }
      }
      if (e.key === 'Escape') {
        closeMenu();
      }
    },
    true,
  );
}

// ── The start screen ──────────────────────────────────────────────────────────

async function renderStart() {
  $('btn-new').hidden = false;
  renderTemplates();
  renderExamples();
  const list = await store.recent();
  $('recent-section').hidden = list.length === 0;
  $('recent-list').replaceChildren(
    ...list.map((r) => {
      const reopen = reopenable(r);
      return el(
        'li',
        {},
        el(
          'button',
          { type: 'button', class: 'card', disabled: !reopen.ok, title: reopen.ok ? `Open ${r.name}` : reopen.why, onclick: () => openRecent(r) },
          el('div', { class: 'thumb' }, r.thumb ? el('img', { src: r.thumb, alt: '' }) : el('span', { html: icons.file })),
          el('div', { class: 'label' }, el('strong', {}, r.name), el('span', {}, `${recentWhere(r)} · ${new Date(r.openedAt).toLocaleDateString()}`)),
        ),
      );
    }),
  );
}

function recentWhere(r) {
  return { local: 'This computer', url: new URL(r.url ?? 'https://x').hostname, drive: 'Google Drive', example: 'Example', template: 'Template', paste: 'Pasted', link: 'Shared link', new: 'New' }[r.kind] ?? r.kind;
}

function reopenable(r) {
  if (r.handle || r.source || r.url || r.example) {
    return { ok: true };
  }
  if (r.fileId) {
    return app.drive.available() ? { ok: true } : { ok: false, why: 'Google Drive is not set up on this site' };
  }
  return { ok: false, why: 'This file cannot be opened again from here: open it again from your computer.' };
}

async function openRecent(r) {
  try {
    if (r.handle) {
      if ((await r.handle.queryPermission?.({ mode: 'read' })) !== 'granted') {
        await r.handle.requestPermission?.({ mode: 'read' });
      }
      const file = await r.handle.getFile();
      await openBytes(await readFile(file), file.name, { kind: 'local', handle: r.handle });
    } else if (r.url) {
      await openUrl(r.url);
    } else if (r.example) {
      await commands.run('app_open_example', { name: r.example });
    } else if (r.fileId) {
      await openDrive(r.fileId);
    } else if (r.source) {
      await openSource(r.source, r.name, { kind: r.kind }, { id: r.id, png: r.png });
    }
  } catch (e) {
    toast(e.message ?? String(e), 6000);
  }
}

let templatesShown = false;

async function renderTemplates() {
  if (templatesShown) {
    return;
  }
  templatesShown = true;
  const list = await fetchJson('templates/index.json', []);
  $('templates-section').hidden = list.length === 0;
  $('template-list').replaceChildren(
    ...list.map((t) =>
      el(
        'li',
        {},
        el(
          'button',
          { type: 'button', class: 'card', onclick: () => act('app_new_document', { template: t.name }) },
          el('div', { class: 'thumb' }, el('img', { src: `templates/${t.name}.png`, alt: '', loading: 'lazy', onerror: (e) => e.target.remove() })),
          el('div', { class: 'label' }, el('strong', {}, t.title), el('span', {}, t.family)),
        ),
      ),
    ),
  );
}

let examplesShown = false;

async function renderExamples() {
  if (examplesShown) {
    return;
  }
  examplesShown = true;
  const list = await fetchJson('examples/index.json', []);
  $('examples-section').hidden = list.length === 0;
  $('example-list').replaceChildren(
    ...list.map((name) =>
      el(
        'li',
        {},
        el(
          'button',
          { type: 'button', class: 'card', onclick: () => act('app_open_example', { name }) },
          el('div', { class: 'thumb' }, el('img', { src: `examples/${name}.png`, alt: '', loading: 'lazy' })),
          el('div', { class: 'label' }, el('strong', {}, name.replace(/_/g, ' '))),
        ),
      ),
    ),
  );
}

// ── About, shortcuts ──────────────────────────────────────────────────────────

function about() {
  return dialog('About nymph', (body, close) => {
    body.append(
      el('p', {}, 'nymph turns a YAML description of a chart into a picture: plots, UML diagrams, flowcharts, Gantt and Sankey charts, user-interface mockups. This page views and edits them in your browser; your files stay here.'),
      el('p', {}, `Version ${app.version.version || 'development'} (${(app.version.commit ?? 'dev').slice(0, 10)}).`),
      el('p', {}, 'MIT License © 2026 Binary Dice Games. Fonts under the SIL Open Font License. Built with Dear ImGui, ImPlot, ImPlot3D, ImGuiColorTextEdit (MIT) and libyaml.'),
      el('p', {}, el('a', { href: 'https://github.com/binary-dice-games/nymph', target: '_blank', rel: 'noopener' }, 'Source code and documentation')),
      dialogButtons(close, [{ label: 'Close', value: true, kind: 'primary', focus: true }]),
    );
  });
}

function shortcuts() {
  const rows = [
    ['Ctrl+O', 'Open a file'],
    ['Ctrl+S', 'Save'],
    ['E', 'Edit (in View)'],
    ['Ctrl+U', 'Show or hide the YAML source'],
    ['Ctrl+Z / Ctrl+Y', 'Undo / redo (in Edit)'],
    ['Delete, Ctrl+D', 'Delete / duplicate the selected element'],
    ['Alt+↑ / Alt+↓', 'Move the selected element'],
    ['Ctrl+F / Ctrl+H', 'Find / replace in the source'],
    ['Ctrl+Shift+A', 'Show or hide the agents working on the chart'],
    ['Ctrl+Shift+L', 'Share the chart live, or show the people in the live session'],
    ['Esc (drawing)', 'Back to the pointer, to use the chart under the drawings'],
    ['Drag, wheel, double-click', 'Pan, zoom, fit the chart'],
    ['?', 'This list'],
  ];
  return dialog('Keyboard shortcuts', (body, close) => {
    body.append(
      el('table', {}, rows.map(([k, v]) => el('tr', {}, el('td', {}, el('kbd', {}, k)), el('td', {}, v)))),
      dialogButtons(close, [{ label: 'Close', value: true, kind: 'primary', focus: true }]),
    );
  });
}

// ── Offline (section 3.9) ─────────────────────────────────────────────────────

function registerServiceWorker() {
  if (!('serviceWorker' in navigator) || new URLSearchParams(location.search).has('nosw')) {
    return;
  }
  // The mobile page never fetches the editor: its worker does not precache it.
  navigator.serviceWorker
    .register(`sw.js?editor=${device.directEditing ? 1 : 0}`)
    .then((reg) => {
      const offer = (worker) => {
        if (!navigator.serviceWorker.controller) {
          return; // the first install: nothing to update
        }
        toastUpdate(worker);
      };
      if (reg.waiting) {
        offer(reg.waiting);
      }
      reg.addEventListener('updatefound', () => {
        const worker = reg.installing;
        worker?.addEventListener('statechange', () => {
          if (worker.state === 'installed') {
            offer(worker);
          }
        });
      });
    })
    .catch(() => {});
}

function toastUpdate(worker) {
  const b = document.getElementById('update-bar') ?? el('div', { id: 'update-bar', class: 'banner info', style: 'position:fixed;bottom:16px;right:16px;border-radius:10px;border:1px solid var(--border);z-index:80;box-shadow:var(--shadow)' });
  b.replaceChildren(
    el('div', { class: 'text' }, 'A new version of nymph is ready.'),
    el('button', {
      type: 'button',
      class: 'btn primary',
      onclick: async () => {
        if (app.doc?.modified) {
          writeDraft();
        }
        worker.postMessage('skipWaiting');
        navigator.serviceWorker.addEventListener('controllerchange', () => location.reload(), { once: true });
      },
    }, 'Reload'),
    el('button', { type: 'button', class: 'btn ghost', onclick: () => b.remove() }, 'Later'),
  );
  document.body.append(b);
}

// ── Prefetch of the editor (section 4) ────────────────────────────────────────

function prefetchEditor() {
  if (!editAvailability().ok) {
    return; // never on mobile
  }
  const idle = window.requestIdleCallback ?? ((cb) => setTimeout(cb, 1500));
  idle(() => {
    for (const href of ['editor/nymph-editor.js', 'editor/nymph-editor-core.js', wasmUrl('editor/nymph-editor-core.wasm')]) {
      document.head.append(el('link', { rel: 'prefetch', href }));
    }
  });
}

init()
  .then(() => {
    if (app.doc) {
      prefetchEditor();
    } else {
      // Once a document is shown.
      const once = commands.onCall(() => {
        if (app.doc) {
          once();
          prefetchEditor();
        }
      });
    }
  })
  .catch((e) => {
    console.error(e);
    toast(`nymph could not start: ${e.message}`, 10000);
  });

export { app, commands, NymphError };
