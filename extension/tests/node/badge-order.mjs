// MIT License © 2026 Binary Dice Games
// Loads the service worker with a stand-in `chrome` namespace and replays
// a toggle that races a chart count: the first badge update stalls before
// writing while the count arrives. Prints the tab's final badge text.
// Run by run.mjs in its own process (api.js must load with the stand-in).
//
//   node badge-order.mjs <nymph source dir>

import { join } from 'node:path';
import { pathToFileURL } from 'node:url';

const [sourceDir] = process.argv.slice(2);
const listeners = () => {
  const fns = [];
  return { addListener: (fn) => fns.push(fn), fire: (...args) => fns.map((fn) => fn(...args)) };
};
const tab = { id: 1, url: 'http://a.test/page' };
const session = {};
const badge = {};
let stall = null; // the first setIcon waits for this
const stalled = new Promise((resolve) => (stall = resolve));
let icons = 0;
globalThis.chrome = {
  runtime: { id: 'nymph', onInstalled: listeners(), onMessage: listeners(), getURL: (p) => p },
  storage: {
    session: {
      get: async (key) => (key in session ? { [key]: session[key] } : {}),
      set: async (items) => void Object.assign(session, items),
      remove: async (key) => void delete session[key],
    },
    sync: { get: async () => ({ off: [] }), set: async () => {} },
    onChanged: listeners(),
  },
  tabs: { query: async () => [tab], onUpdated: listeners(), onRemoved: listeners(), sendMessage: async () => {} },
  action: {
    enable: async () => {},
    disable: async () => {},
    setIcon: async () => {
      if (icons++ === 0) {
        await stalled;
      }
    },
    setBadgeText: async ({ tabId, text }) => void (badge[tabId] = text),
    setBadgeBackgroundColor: async () => {},
    setTitle: async () => {},
    onClicked: listeners(),
  },
  permissions: { contains: async () => true },
};

await import(pathToFileURL(join(sourceDir, 'extension', 'src', 'background', 'service-worker.js')).href);
const tick = () => new Promise((resolve) => setTimeout(resolve, 20));
// The site was turned on: an update reads count 0, then stalls.
chrome.storage.onChanged.fire({ off: { newValue: [] } }, 'sync');
await tick();
// The page's charts come back and report 3.
chrome.runtime.onMessage.fire({ type: 'nymph-count', count: 3 }, { id: 'nymph', tab }, () => {});
await tick();
stall();
await tick();
console.log(JSON.stringify(badge[tab.id] ?? null));
