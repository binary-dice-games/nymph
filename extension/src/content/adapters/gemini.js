// MIT License © 2026 Binary Dice Games
/**
 * @file gemini.js
 * @brief The adapter for gemini.google.com (spec/10_gemini_extension/DESIGN.md
 * section 4.3). Written against the page as seen on 2026-10-19 (PLAN step 1):
 * a response holds `<code-block>` elements in the light DOM, each with a header
 * (the language label, a copy button) and, in
 * `.formatted-code-block-internal-container`, a `<pre>`. Class names beyond
 * `code-block` and `pre` are Angular's and may change; every lookup falls back.
 */

import { firstComposer, pageIsDark } from './generic.js';

/** The language label from the block's header: its first line, when it is one word. */
function labelOf(el) {
  const first = (el.innerText ?? '').split('\n')[0].trim();
  return /^[A-Za-z][\w+#.-]{0,19}$/.test(first) ? first.toLowerCase() : '';
}

export const gemini = {
  name: 'gemini',

  /**
   * @param {ParentNode} root
   * @return {Array<{container: HTMLElement, body: HTMLElement, text: string, label: string}>}
   */
  codeBlocks(root) {
    const blocks = [];
    for (const el of root.querySelectorAll('code-block')) {
      const pre = el.querySelector('pre');
      if (!pre) {
        continue;
      }
      const body = el.querySelector('.formatted-code-block-internal-container') ?? pre;
      blocks.push({ container: el, body, text: pre.textContent ?? '', label: labelOf(el) });
    }
    return blocks;
  },

  /** @return {HTMLElement|null} the prompt box: Gemini's Quill editor */
  composer() {
    return document.querySelector('rich-textarea .ql-editor') ?? firstComposer();
  },

  /**
   * Whether an answer is being written: the page shows a stop button while it
   * is (unverified, PLAN step 1; the settle delay covers a wrong guess).
   * @return {boolean}
   */
  isStreaming() {
    return document.querySelector('button[aria-label*="Stop" i]') !== null;
  },

  /** @return {'light'|'dark'} */
  theme() {
    return pageIsDark() ? 'dark' : 'light';
  },
};
