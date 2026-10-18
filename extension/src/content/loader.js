// MIT License © 2026 Binary Dice Games
/**
 * @file loader.js
 * @brief The content script declared in the manifest, on every page
 * (spec/05_browser_extension/DESIGN.md section 4). A classic script, as
 * Manifest V3 requires: it reads whether this site is on and, when it is,
 * imports the module that does the work. It follows the toolbar button
 * live through the stored settings.
 */
(() => {
  const origin = location.protocol === 'file:' ? 'file://' : location.origin;
  let main = null;
  let loading = null;

  async function apply() {
    let off = [];
    let theme = 'source';
    try {
      const stored = await chrome.storage.sync.get(['off', 'theme']);
      off = Array.isArray(stored.off) ? stored.off : [];
      theme = stored.theme === 'auto' ? 'auto' : 'source';
    } catch {
      return; // the extension was reloaded or removed: this script is orphaned
    }
    const on = !off.includes(origin);
    if (on && !main) {
      loading ??= import(chrome.runtime.getURL('src/content/main.js'));
      main = await loading;
    }
    if (main) {
      if (on) {
        main.start({ theme });
      } else {
        main.stop();
      }
    }
  }

  chrome.storage.onChanged.addListener((changes, area) => {
    if (area === 'sync' && (changes.off || changes.theme)) {
      apply();
    }
  });
  apply();
})();
