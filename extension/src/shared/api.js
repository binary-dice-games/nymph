// MIT License © 2026 Binary Dice Games
/**
 * @file api.js
 * @brief The one place that chooses the WebExtension namespace
 * (spec/11_cross_browser_extension/DESIGN.md section 4). Chrome, Edge,
 * Firefox and Safari all expose promise-returning `chrome.*` in Manifest V3;
 * Firefox and Safari also expose `browser.*`, which is preferred where it exists.
 */

/** The browser's extension API. */
export const ext = globalThis.browser ?? globalThis.chrome;

/**
 * Whether the extension may run on every site. Chrome grants this on
 * install; Firefox and Safari let the user withhold it.
 * @param {object} [api] the namespace (tests pass a fake)
 * @return {Promise<boolean>} true too when the browser has no permissions API
 */
export async function hasHostAccess(api = ext) {
  if (!api?.permissions?.contains) {
    return true;
  }
  try {
    return Boolean(await api.permissions.contains({ origins: ['<all_urls>'] }));
  } catch {
    return true;
  }
}

/**
 * Whether the extension can ask the user for site access.
 * @param {object} [api] the namespace
 * @return {boolean}
 */
export function canRequestAccess(api = ext) {
  return typeof api?.permissions?.request === 'function';
}
