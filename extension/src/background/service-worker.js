// MIT License © 2026 Binary Dice Games
/**
 * @file service-worker.js
 * @brief The extension's background: reads cross-origin images for the
 * content scripts, and owns the toolbar button
 * (spec/05_browser_extension/DESIGN.md sections 3.8 and 4).
 */

import { canRequestAccess, ext, hasHostAccess } from '../shared/api.js';
import { sniffStream } from '../content/sniff.js';
import { isOn, readSettings, runnableOrigin, setSite } from '../shared/settings.js';

// ── Cross-origin reads ───────────────────────────────────────────────────────

/** Bytes to base64 (messages are JSON). */
function toBase64(bytes) {
  let text = '';
  for (let i = 0; i < bytes.length; i += 0x8000) {
    text += String.fromCharCode(...bytes.subarray(i, i + 0x8000));
  }
  return btoa(text);
}

/**
 * Fetches an image without cookies and sniffs it.
 * @param {string} url an http(s) URL
 * @return {Promise<{nymph: boolean, base64?: string, reason?: string, error?: string}>}
 */
async function sniffUrl(url) {
  const u = new URL(url);
  if (u.protocol !== 'http:' && u.protocol !== 'https:') {
    return { error: 'unsupported URL' };
  }
  try {
    const response = await fetch(url, { cache: 'force-cache', credentials: 'omit' });
    if (!response.ok || !response.body) {
      return { nymph: false, reason: `HTTP ${response.status}` };
    }
    const result = await sniffStream(response.body);
    return result.nymph ? { nymph: true, base64: toBase64(result.bytes) } : { nymph: false, reason: result.reason };
  } catch (e) {
    return { error: String(e?.message ?? e) };
  }
}

// ── The toolbar button ───────────────────────────────────────────────────────

/** Charts replaced per tab, kept in session storage (the worker sleeps). */
async function chartCount(tabId) {
  const key = `count:${tabId}`;
  const stored = await ext.storage.session.get(key);
  return stored[key] ?? 0;
}

/**
 * Shows a tab's state on the button: icon, badge and title.
 * @param {number} tabId
 * @param {string|undefined} url the tab's URL
 */
async function showTab(tabId, url) {
  const origin = runnableOrigin(url);
  try {
    if (!origin) {
      await ext.action.disable(tabId);
      await ext.action.setIcon({ tabId, path: { 16: '/icons/off-16.png', 32: '/icons/off-32.png' } });
      await ext.action.setBadgeText({ tabId, text: '' });
      await ext.action.setTitle({ tabId, title: 'nymph cannot run on this page' });
      return;
    }
    await ext.action.enable(tabId);
    if (!(await hasHostAccess())) {
      // Firefox and Safari let the user withhold site access
      // (spec/11_cross_browser_extension/DESIGN.md section 3.2).
      await ext.action.setIcon({ tabId, path: { 16: '/icons/off-16.png', 32: '/icons/off-32.png' } });
      await ext.action.setBadgeText({ tabId, text: '' });
      await ext.action.setTitle({ tabId, title: 'nymph needs access to sites. Click to allow.' });
      return;
    }
    const settings = await readSettings();
    const on = isOn(settings, origin);
    const count = on ? await chartCount(tabId) : 0;
    const site = origin === 'file://' ? 'local files' : new URL(origin).host;
    const state = on ? 'on' : 'off';
    await ext.action.setIcon({ tabId, path: { 16: `/icons/${state}-16.png`, 32: `/icons/${state}-32.png` } });
    await ext.action.setBadgeText({ tabId, text: count > 0 ? String(count) : '' });
    await ext.action.setBadgeBackgroundColor({ tabId, color: '#3367d6' });
    const charts = count === 1 ? '1 chart' : `${count} charts`;
    await ext.action.setTitle({
      tabId,
      title: on ? `nymph: on for ${site}, ${charts}. Click to turn off.` : `nymph: off for ${site}. Click to turn on.`,
    });
  } catch {
    // The tab closed meanwhile.
  }
}

async function showAllTabs() {
  for (const tab of await ext.tabs.query({})) {
    await showTab(tab.id, tab.url);
  }
}

/**
 * The button's click: turns the tab's site off or on. Content scripts of
 * every tab of the site follow the stored setting.
 * @param {{id: number, url?: string}} tab
 * @return {Promise<boolean|null>} whether the site is now on (null: cannot run there)
 */
async function toggleTab(tab) {
  if (!(await hasHostAccess())) {
    // The click is the user gesture the request needs.
    if (canRequestAccess()) {
      await ext.permissions.request({ origins: ['<all_urls>'] });
    }
    await showAllTabs();
    return null;
  }
  const origin = runnableOrigin(tab.url);
  if (!origin) {
    return null;
  }
  const on = !isOn(await readSettings(), origin);
  await setSite(origin, on);
  await showAllTabs();
  return on;
}
// For the browser tests, which cannot click the toolbar.
globalThis.nymphToggleTab = toggleTab;

ext.action.onClicked.addListener((tab) => toggleTab(tab));
ext.permissions?.onAdded?.addListener(showAllTabs);
ext.permissions?.onRemoved?.addListener(showAllTabs);

// "Give this chat the nymph instructions", in the button's context menu
// (spec/10_gemini_extension/DESIGN.md section 4.5): the page's content script
// puts the text in the prompt box.
const TEACH_MENU = 'nymph-teach';

/**
 * @param {{id?: number}} tab
 * @return {Promise<string>} what the page did: inserted, copied, failed, or
 *   unavailable (chat mode is not running there)
 */
async function teachTab(tab) {
  if (!tab?.id) {
    return 'unavailable';
  }
  try {
    return (await ext.tabs.sendMessage(tab.id, { type: 'nymph-teach' })) ?? 'unavailable';
  } catch {
    return 'unavailable';
  }
}
// For the browser tests, which cannot use the menu.
globalThis.nymphTeachTab = teachTab;

ext.contextMenus?.onClicked.addListener((info, tab) => {
  if (info.menuItemId === TEACH_MENU) {
    teachTab(tab);
  }
});

ext.tabs.onUpdated.addListener((tabId, info, tab) => {
  if (info.status === 'loading' && info.url !== undefined) {
    ext.storage.session.remove(`count:${tabId}`);
  }
  if (info.status !== undefined || info.url !== undefined) {
    showTab(tabId, tab.url);
  }
});
ext.tabs.onRemoved.addListener((tabId) => ext.storage.session.remove(`count:${tabId}`));
ext.storage.onChanged.addListener((changes, area) => {
  if (area === 'sync' && changes.off) {
    showAllTabs();
  }
});

ext.runtime.onInstalled.addListener((details) => {
  showAllTabs();
  ext.contextMenus?.removeAll(() =>
    ext.contextMenus.create({ id: TEACH_MENU, title: 'Give this chat the nymph instructions', contexts: ['action'] }));
  if (details.reason === 'install') {
    ext.tabs.create({ url: ext.runtime.getURL('src/options/options.html?welcome=1') });
  }
});

// ── Messages from content scripts ────────────────────────────────────────────

ext.runtime.onMessage.addListener((message, sender, reply) => {
  if (sender.id !== ext.runtime.id || !sender.tab) {
    return false;
  }
  if (message?.type === 'nymph-sniff' && typeof message.url === 'string') {
    sniffUrl(message.url).then(reply);
    return true; // answers asynchronously
  }
  if (message?.type === 'nymph-count' && Number.isInteger(message.count)) {
    ext.storage.session.set({ [`count:${sender.tab.id}`]: message.count }).then(() => showTab(sender.tab.id, sender.tab.url));
    return false;
  }
  return false;
});
