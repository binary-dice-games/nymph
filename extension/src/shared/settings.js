// MIT License © 2026 Binary Dice Games
/**
 * @file settings.js
 * @brief The extension's settings in `chrome.storage.sync`
 * (spec/05_browser_extension/DESIGN.md section 5.2).
 */

/** Defaults of every setting. */
export const DEFAULTS = Object.freeze({
  /** Origins the user turned off with the toolbar button. */
  off: [],
  /** The charts' theme: the source's (as the PNG) or the page's colour scheme. */
  theme: 'source',
});

/**
 * Reads every setting, defaults filled in.
 * @return {Promise<{off: string[], theme: 'source'|'auto'}>}
 */
export async function readSettings() {
  const stored = await chrome.storage.sync.get(Object.keys(DEFAULTS));
  return { ...DEFAULTS, ...stored };
}

/**
 * Whether the extension runs on a site.
 * @param {{off: string[]}} settings
 * @param {string} origin e.g. "https://github.com"
 * @return {boolean}
 */
export function isOn(settings, origin) {
  return !settings.off.includes(origin);
}

/**
 * Turns a site on or off.
 * @param {string} origin
 * @param {boolean} on
 * @return {Promise<void>}
 */
export async function setSite(origin, on) {
  const { off } = await readSettings();
  const next = off.filter((o) => o !== origin);
  if (!on) {
    next.push(origin);
  }
  await chrome.storage.sync.set({ off: next });
}

/**
 * Whether the extension can run on a URL at all (not on browser pages,
 * the Web Store or the PDF viewer).
 * @param {string|undefined} url
 * @return {string|null} the URL's origin when it can, else null
 */
export function runnableOrigin(url) {
  if (!url) {
    return null;
  }
  let u;
  try {
    u = new URL(url);
  } catch {
    return null;
  }
  if (u.protocol !== 'http:' && u.protocol !== 'https:' && u.protocol !== 'file:') {
    return null;
  }
  if (u.hostname === 'chromewebstore.google.com' || (u.hostname === 'chrome.google.com' && u.pathname.startsWith('/webstore'))) {
    return null;
  }
  return u.protocol === 'file:' ? 'file://' : u.origin;
}
