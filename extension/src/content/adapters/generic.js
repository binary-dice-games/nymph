// MIT License © 2026 Binary Dice Games
/**
 * @file generic.js
 * @brief The default chat adapter: markdown renderers that put a code block in
 * `<pre><code class="language-x">` (spec/10_gemini_extension/DESIGN.md
 * section 4.3). Site adapters (gemini.js) build on its helpers.
 */

/**
 * The language label of a code element from its `language-x` class.
 * @param {Element|null} el
 * @return {string} the label, or '' when none
 */
export function languageOf(el) {
  for (const name of el?.classList ?? []) {
    const match = /^(?:language|lang)-(.+)$/.exec(name);
    if (match) {
      return match[1].toLowerCase();
    }
  }
  return '';
}

/**
 * Whether the page looks dark: the body's background luminance.
 * @return {boolean}
 */
export function pageIsDark() {
  const m = /rgba?\((\d+),\s*(\d+),\s*(\d+)(?:,\s*([\d.]+))?/.exec(getComputedStyle(document.body).backgroundColor);
  if (m && (m[4] === undefined || Number(m[4]) > 0.5)) {
    return 0.2126 * m[1] + 0.7152 * m[2] + 0.0722 * m[3] < 128;
  }
  return matchMedia('(prefers-color-scheme: dark)').matches;
}

/**
 * The first visible text box of a page: a `textarea` or a `contenteditable`
 * element that is not part of the extension.
 * @return {HTMLElement|null}
 */
export function firstComposer() {
  for (const box of document.querySelectorAll('textarea, [contenteditable="true"], [contenteditable=""]')) {
    const r = box.getBoundingClientRect();
    if (r.width > 0 && r.height > 0 && !box.closest('nymph-ext-chart')) {
      return box;
    }
  }
  return null;
}

/**
 * Types text at the end of a text box the way the user would, so the page's
 * editor sees an edit (and enables its send button). Never submits. A synthetic
 * `beforeinput` event does not reach Gemini's editor; `execCommand` does
 * (spec/10_gemini_extension/PLAN.md step 1).
 * @param {HTMLElement|null} box a `textarea` or a `contenteditable` element
 * @param {string} text what to add, after a blank line when the box has text
 * @return {boolean} whether the browser took the text (false: use the clipboard)
 */
export function insertIntoComposer(box, text) {
  if (!box) {
    return false;
  }
  box.focus();
  const plain = box instanceof HTMLTextAreaElement || box instanceof HTMLInputElement;
  if (plain) {
    box.setSelectionRange(box.value.length, box.value.length);
  } else {
    const range = document.createRange();
    range.selectNodeContents(box);
    range.collapse(false);
    const selection = getSelection();
    selection.removeAllRanges();
    selection.addRange(range);
  }
  const filled = (plain ? box.value : box.textContent ?? '').trim() !== '';
  try {
    return document.execCommand('insertText', false, (filled ? '\n\n' : '') + text);
  } catch {
    return false;
  }
}

/** The adapter for any page with `<pre><code>` blocks. */
export const generic = {
  name: 'generic',

  /**
   * @param {ParentNode} root
   * @return {Array<{container: HTMLElement, body: HTMLElement, text: string, label: string}>}
   */
  codeBlocks(root) {
    const blocks = [];
    for (const pre of root.querySelectorAll('pre')) {
      const code = pre.querySelector('code');
      blocks.push({
        container: pre,
        body: pre,
        text: (code ?? pre).textContent ?? '',
        label: languageOf(code) || languageOf(pre),
      });
    }
    return blocks;
  },

  /** @return {HTMLElement|null} the prompt box */
  composer: firstComposer,

  /** @return {boolean} whether the page is still writing an answer (never known here) */
  isStreaming() {
    return false;
  },

  /** @return {'light'|'dark'} */
  theme() {
    return pageIsDark() ? 'dark' : 'light';
  },
};
