// MIT License © 2026 Binary Dice Games
// The host bridge of the nymph MCP App (spec/06_claude_integration/DESIGN.md
// section 4.2): MCP Apps messages over postMessage (JSON-RPC 2.0), and the
// web viewer showing the chart of render_chart's result. Inlined after
// nymph-viewer.single.js, in the same module, so createViewer is in scope.

const PROTOCOL_VERSION = '2026-01-26';

// ── JSON-RPC to the host ──────────────────────────────────────────────────────

const pending = new Map();
const handlers = new Map();
let nextId = 1;

function post(message) {
  window.parent.postMessage({ jsonrpc: '2.0', ...message }, '*');
}

/** A request to the host; resolves with its result, rejects with its error. */
function request(method, params) {
  const id = nextId++;
  post({ id, method, params });
  return new Promise((resolve, reject) => pending.set(id, { resolve, reject }));
}

function notify(method, params) {
  post({ method, params });
}

window.addEventListener('message', (event) => {
  if (event.source !== window.parent) {
    return;
  }
  const m = event.data;
  if (!m || m.jsonrpc !== '2.0') {
    return;
  }
  if (m.method === undefined) {
    const p = pending.get(m.id);
    if (p) {
      pending.delete(m.id);
      m.error ? p.reject(new Error(m.error.message ?? 'host error')) : p.resolve(m.result);
    }
    return;
  }
  const handler = handlers.get(m.method);
  let result = {};
  try {
    result = handler ? handler(m.params ?? {}) ?? {} : {};
  } catch (e) {
    if (m.id !== undefined) {
      post({ id: m.id, error: { code: -32603, message: String(e?.message ?? e) } });
    }
    return;
  }
  if (m.id !== undefined) {
    // Requests from the host (ui/resource-teardown, ...) need an answer.
    post({ id: m.id, result });
  }
});

// ── The page ──────────────────────────────────────────────────────────────────

const chartBox = document.getElementById('chart');
const message = document.getElementById('message');
const fullscreenButton = document.getElementById('fullscreen');

/** What tests (and a curious user's console) can look at. */
const app = { viewer: null, host: null, displayMode: 'inline', error: null };
window.nymphApp = app;

/** The viewer theme for the host's: the chat's light or dark, else the source's. */
function themeOf(context) {
  return context?.theme === 'dark' || context?.theme === 'light' ? context.theme : 'source';
}

/** Tells the host how tall the page is, so it can size the frame. */
let lastHeight = 0;
function reportSize() {
  const height = Math.ceil(document.documentElement.scrollHeight);
  if (height !== lastHeight) {
    lastHeight = height;
    notify('ui/notifications/size-changed', { width: Math.ceil(document.documentElement.scrollWidth), height });
  }
}
new ResizeObserver(reportSize).observe(document.body);

function base64ToBytes(text) {
  const binary = atob(text);
  const bytes = new Uint8Array(binary.length);
  for (let i = 0; i < binary.length; ++i) {
    bytes[i] = binary.charCodeAt(i);
  }
  return bytes;
}

/** Fonts come from the server's app-only tool: the page has no network. */
async function fetchFont(name) {
  const result = await request('tools/call', { name: 'viewer_font', arguments: { name } });
  if (result?.isError || typeof result?.structuredContent?.base64 !== 'string') {
    throw new Error(result?.content?.[0]?.text ?? `no font '${name}'`);
  }
  return base64ToBytes(result.structuredContent.base64);
}

/** The PNG of the result, for when the viewer cannot show the chart. */
function showFallback(result, why) {
  app.error = why;
  message.textContent = why;
  const image = (result?.content ?? []).find((c) => c.type === 'image' && c.mimeType === 'image/png');
  if (image) {
    const img = document.createElement('img');
    img.id = 'fallback';
    img.alt = 'nymph chart';
    img.src = `data:image/png;base64,${image.data}`;
    chartBox.replaceChildren(img);
  }
}

async function showResult(result) {
  const source = result?.structuredContent?.source;
  if (result?.isError || typeof source !== 'string') {
    // The host shows the error itself; nothing to draw.
    message.textContent = result?.isError ? '' : 'No chart in this result.';
    return;
  }
  try {
    if (app.viewer) {
      await app.viewer.setSource(source);
    } else {
      app.viewer = await createViewer(chartBox, { source, fit: 'width', theme: themeOf(app.host?.hostContext), fetchFont });
    }
    await app.viewer.rendered();
    message.textContent = '';
    app.error = null;
  } catch (e) {
    showFallback(result, `The live chart could not be shown (${e?.message ?? e}); this is the rendered image.`);
  }
  reportSize();
}

handlers.set('ui/notifications/tool-result', (params) => {
  showResult(params);
});

handlers.set('ui/notifications/host-context-changed', (params) => {
  app.host = { ...app.host, hostContext: { ...app.host?.hostContext, ...params } };
  if (params.theme && app.viewer) {
    app.viewer.setTheme(themeOf(params) === 'source' ? '' : themeOf(params));
  }
  if (params.displayMode) {
    setDisplayMode(params.displayMode);
  }
});

handlers.set('ui/resource-teardown', () => {
  app.viewer?.destroy();
  app.viewer = null;
  return {};
});

function setDisplayMode(mode) {
  app.displayMode = mode;
  document.body.classList.toggle('fullscreen', mode === 'fullscreen');
  fullscreenButton.textContent = mode === 'fullscreen' ? 'Exit full screen' : 'Full screen';
}

fullscreenButton.addEventListener('click', async () => {
  const wanted = app.displayMode === 'fullscreen' ? 'inline' : 'fullscreen';
  try {
    const result = await request('ui/request-display-mode', { mode: wanted });
    setDisplayMode(result?.mode ?? app.displayMode);
  } catch {
    // The host refused; stay as we are.
  }
});

// ── Start ─────────────────────────────────────────────────────────────────────

(async () => {
  try {
    app.host = await request('ui/initialize', {
      protocolVersion: PROTOCOL_VERSION,
      appInfo: { name: 'nymph', version: '1' },
      appCapabilities: { availableDisplayModes: ['inline', 'fullscreen'] },
    });
  } catch (e) {
    app.host = { error: String(e?.message ?? e) };
  }
  const modes = app.host?.hostContext?.availableDisplayModes ?? [];
  fullscreenButton.hidden = !modes.includes('fullscreen');
  setDisplayMode(app.host?.hostContext?.displayMode ?? 'inline');
  notify('ui/notifications/initialized', {});
  reportSize();
})();
