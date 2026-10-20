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
  // Written at once, so "did the script run on this page?" can be answered:
  // document.documentElement.dataset.nymphLoader
  document.documentElement.dataset.nymphLoader = 'running';
  const ext = globalThis.browser ?? globalThis.chrome; // as shared/api.js (a classic script cannot import)
  const origin = location.protocol === 'file:' ? 'file://' : location.origin;
  let main = null;
  let loading = null;
  let chat = null;
  let loadingChat = null;

  async function apply() {
    let off = [];
    let theme = 'source';
    let chatSites = ['https://gemini.google.com'];
    let chatTheme = 'auto';
    try {
      const stored = await ext.storage.sync.get(['off', 'theme', 'chatSites', 'chatTheme']);
      off = Array.isArray(stored.off) ? stored.off : [];
      theme = stored.theme === 'auto' ? 'auto' : 'source';
      chatSites = Array.isArray(stored.chatSites) ? stored.chatSites : chatSites;
      chatTheme = stored.chatTheme === 'source' ? 'source' : 'auto';
    } catch (e) {
      // the extension was reloaded or removed: this script is orphaned
      document.documentElement.dataset.nymphStatus = `settings unreadable: ${e?.message ?? e}`;
      return;
    }
    const on = !off.includes(origin);
    const status = { images: on ? 'starting' : 'site off', chat: 'not listed' };
    // The two modes are independent: one failing to load must not stop the other.
    try {
      if (on && !main) {
        loading ??= import(ext.runtime.getURL('src/content/main.js'));
        main = await loading;
      }
      if (main) {
        if (on) {
          main.start({ theme });
          status.images = 'on';
        } else {
          main.stop();
        }
      }
    } catch (e) {
      status.images = `error: ${e?.message ?? e}`;
    }
    // Chat mode (spec/10_gemini_extension): only on the sites the user listed.
    const chatOn = on && chatSites.includes(origin);
    status.chat = !on ? 'site off' : chatOn ? 'starting' : 'not listed';
    try {
      if (chatOn && !chat) {
        loadingChat ??= import(ext.runtime.getURL('src/content/chat.js'));
        chat = await loadingChat;
      }
      if (chat) {
        if (chatOn) {
          chat.start({ theme: chatTheme, origin });
          status.chat = 'on';
        } else {
          chat.stop();
        }
      }
    } catch (e) {
      status.chat = `error: ${e?.message ?? e}`;
    }
    // Readable from the page console: document.documentElement.dataset.nymphStatus
    document.documentElement.dataset.nymphStatus = `images ${status.images}; chat ${status.chat}`;
  }

  ext.storage.onChanged.addListener((changes, area) => {
    if (area === 'sync' && (changes.off || changes.theme || changes.chatSites || changes.chatTheme)) {
      apply();
    }
  });
  apply();
})();
