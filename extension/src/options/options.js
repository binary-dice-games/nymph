// MIT License © 2026 Binary Dice Games
/**
 * @file options.js
 * @brief The options page: the theme and the sites turned off
 * (spec/05_browser_extension/DESIGN.md section 3.8).
 */

import { readSettings, setSite } from '../shared/settings.js';

const list = document.getElementById('off');
const none = document.getElementById('none');

async function render() {
  const settings = await readSettings();
  for (const input of document.querySelectorAll('input[name="theme"]')) {
    input.checked = input.value === settings.theme;
  }
  list.replaceChildren();
  for (const origin of settings.off) {
    const li = document.createElement('li');
    li.textContent = origin === 'file://' ? 'Local files' : origin;
    const button = document.createElement('button');
    button.type = 'button';
    button.textContent = 'Turn on';
    button.addEventListener('click', () => setSite(origin, true));
    li.appendChild(button);
    list.appendChild(li);
  }
  none.hidden = settings.off.length > 0;
}

for (const input of document.querySelectorAll('input[name="theme"]')) {
  input.addEventListener('change', () => chrome.storage.sync.set({ theme: input.value }));
}
chrome.storage.onChanged.addListener((_, area) => area === 'sync' && render());
document.getElementById('welcome').hidden = new URLSearchParams(location.search).get('welcome') !== '1';
render();
