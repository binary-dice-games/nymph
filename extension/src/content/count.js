// MIT License © 2026 Binary Dice Games
/**
 * @file count.js
 * @brief The number of charts on the page for the toolbar badge, summed over
 * the image mode (main.js) and the chat mode (chat.js), which both report.
 */

const counts = { images: 0, chat: 0 };
let last = -1;

/**
 * Reports one mode's chart count; the worker gets the total when it changes.
 * @param {'images'|'chat'} kind
 * @param {number} count charts live or parked in that mode
 */
export function setCount(kind, count) {
  counts[kind] = count;
  const total = counts.images + counts.chat;
  if (total === last) {
    return;
  }
  last = total;
  try {
    chrome.runtime.sendMessage({ type: 'nymph-count', count: total }).catch(() => {});
  } catch {
    // the extension was reloaded: this script is orphaned
  }
}
