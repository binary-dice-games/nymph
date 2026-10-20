// MIT License © 2026 Binary Dice Games
/**
 * @file options.js
 * @brief The options page: the theme, the sites turned off
 * (spec/05_browser_extension/DESIGN.md section 3.8), and chat mode: its sites,
 * its theme and the instructions that teach Gemini nymph
 * (spec/10_gemini_extension/DESIGN.md sections 4.5 and 5.2).
 */

import { canRequestAccess, ext, hasHostAccess } from '../shared/api.js';
import { readSettings, setSite } from '../shared/settings.js';

const list = document.getElementById('off');
const none = document.getElementById('none');
const chatList = document.getElementById('chat-sites');
const noChatSites = document.getElementById('no-chat-sites');

/** One row of a site list: the label and a button. */
function row(label, buttonText, onClick) {
  const li = document.createElement('li');
  li.textContent = label;
  const button = document.createElement('button');
  button.type = 'button';
  button.textContent = buttonText;
  button.addEventListener('click', onClick);
  li.appendChild(button);
  return li;
}

/**
 * The origin a typed address stands for.
 * @param {string} text
 * @return {string|null} e.g. "https://gemini.google.com", or null when it is not a web address
 */
export function originOf(text) {
  try {
    const url = new URL(/^[a-z]+:\/\//i.test(text.trim()) ? text.trim() : `https://${text.trim()}`);
    // Chrome's parser takes spaces in a host and percent-encodes them: refuse those.
    const host = /^[a-z0-9]([a-z0-9.-]*[a-z0-9])?$/i.test(url.hostname) || /^\[[0-9a-f:.]+\]$/i.test(url.hostname);
    return host && (url.protocol === 'https:' || url.protocol === 'http:') ? url.origin : null;
  } catch {
    return null;
  }
}

async function render() {
  const settings = await readSettings();
  for (const input of document.querySelectorAll('input[name="theme"]')) {
    input.checked = input.value === settings.theme;
  }
  for (const input of document.querySelectorAll('input[name="chatTheme"]')) {
    input.checked = input.value === settings.chatTheme;
  }
  list.replaceChildren(
    ...settings.off.map((origin) =>
      row(origin === 'file://' ? 'Local files' : origin, 'Turn on', () => setSite(origin, true))),
  );
  none.hidden = settings.off.length > 0;
  chatList.replaceChildren(
    ...settings.chatSites.map((origin) =>
      row(origin, 'Remove', () => ext.storage.sync.set({ chatSites: settings.chatSites.filter((o) => o !== origin) }))),
  );
  noChatSites.hidden = settings.chatSites.length > 0;
}

for (const input of document.querySelectorAll('input[name="theme"]')) {
  input.addEventListener('change', () => ext.storage.sync.set({ theme: input.value }));
}
for (const input of document.querySelectorAll('input[name="chatTheme"]')) {
  input.addEventListener('change', () => ext.storage.sync.set({ chatTheme: input.value }));
}

document.getElementById('add-chat-site').addEventListener('submit', async (e) => {
  e.preventDefault();
  const note = document.getElementById('chat-site-note');
  const field = document.getElementById('chat-site');
  const origin = originOf(field.value);
  if (!origin) {
    note.textContent = 'That is not a web address.';
    return;
  }
  const { chatSites } = await readSettings();
  if (!chatSites.includes(origin)) {
    await ext.storage.sync.set({ chatSites: [...chatSites, origin] });
  }
  note.textContent = '';
  field.value = '';
});

// The instructions for Gemini (generated from the template and the examples).
const primerBox = document.getElementById('primer');
let primerText = '';
fetch(ext.runtime.getURL('src/gemini/primer.txt'))
  .then((r) => r.text())
  .then((text) => {
    primerText = text.replace(/\r\n/g, '\n');
    primerBox.textContent = primerText;
  });
document.getElementById('copy-primer').addEventListener('click', async () => {
  const note = document.getElementById('copy-note');
  try {
    await navigator.clipboard.writeText(primerText);
    note.textContent = 'Copied.';
  } catch {
    note.textContent = 'Could not copy: open "Show the instructions" and copy them from there.';
  }
});

// Site access (Firefox and Safari let the user withhold it).
const accessBox = document.getElementById('access');
async function showAccess() {
  accessBox.hidden = await hasHostAccess();
  document.getElementById('allow-access').hidden = !canRequestAccess();
}
document.getElementById('allow-access').addEventListener('click', async () => {
  const note = document.getElementById('access-note');
  try {
    note.textContent = (await ext.permissions.request({ origins: ['<all_urls>'] })) ? '' : 'Access was not granted.';
  } catch (e) {
    note.textContent = `Could not ask: ${e?.message ?? e}`;
  }
  showAccess();
});
ext.permissions?.onAdded?.addListener(showAccess);
ext.permissions?.onRemoved?.addListener(showAccess);
showAccess();

// Wording that depends on what the browser offers (feature tests, never the browser's name).
if (typeof ext.runtime.getBrowserInfo === 'function') {
  document.getElementById('pin-hint').textContent =
    'Open the Extensions menu in the toolbar and choose "Pin to Toolbar" beside "nymph charts".';
}
if (!ext.contextMenus) {
  document.getElementById('menu-hint').hidden = true;
}

ext.storage.onChanged.addListener((_, area) => area === 'sync' && render());
document.getElementById('welcome').hidden = new URLSearchParams(location.search).get('welcome') !== '1';
render();
