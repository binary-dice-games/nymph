// MIT License © 2026 Binary Dice Games
/**
 * @file code-replacer.js
 * @brief Puts a viewer before a chat's code block that holds a nymph
 * source, and the block back (spec/10_gemini_extension/DESIGN.md sections
 * 3.2, 3.4 and 4.2).
 *
 * The host element goes right before the block's container, in a shadow
 * root as for images. The code stays in the page: only its body is hidden,
 * with an attribute and the page-level rule of replacer.js, so the chat's
 * own header, copy button and find-in-page keep working.
 *
 * A block that is plainly meant as a nymph source (`claimed`) is replaced
 * even when the viewer rejects it: the host then shows the viewer's error
 * (line, column and what is wrong) in place of the chart.
 */

import { sourceSize } from './source-detect.js';
import { ChartHost, HIDDEN_ATTR, HOST_TAG, SHADOW_CSS, canRunInline } from './replacer.js';

/** The attribute value on a body that is hidden, and on one the user opened. */
const HIDDEN = 'source';
const SHOWN = 'source-shown';

const CODE_CSS = `
:host { display: block; margin: 0 0 8px; }
.badge button { width: auto; padding: 4px 8px; }
:host(.nymph-ext-starting) .box { min-height: 0; }
.error { display: none; line-height: 1.45; padding: 10px 12px; border: 1px solid #c0392b; border-left-width: 4px;
  border-radius: 6px; background: rgba(192, 57, 43, .09); color: inherit; font: 13px/1.45 system-ui, sans-serif; }
:host(.nymph-ext-error) .error { display: block; }
:host(.nymph-ext-error) .box { display: none; }
.error .detail { margin: 6px 0 8px; white-space: pre-wrap; word-break: break-word; font: 12px/1.45 ui-monospace, Consolas, monospace; }
.error .fix { font: 600 12px/1 system-ui, sans-serif; padding: 6px 10px; border-radius: 4px; cursor: pointer; color: #fff;
  background: rgba(40, 60, 110, .9); border: 1px solid rgba(255, 255, 255, .35); }
.error .fix:hover, .error .fix:focus-visible { background: rgba(51, 103, 214, .95); outline: none; }
/* last, so it wins over the rules that show the box and the error */
:host(.nymph-ext-showing-code) .box, :host(.nymph-ext-showing-code) .error { display: none; }
`;

/**
 * The text of a viewer error for the user: its position first, as the
 * command line prints it.
 * @param {any} e an error from the viewer (a `NymphError` has `line` and `column`)
 * @return {string}
 */
export function describeError(e) {
  const message = String(e?.message ?? e);
  return e?.line && !/^\d+:\d+/.test(message) && !/^line \d+/i.test(message)
    ? `line ${e.line}, column ${e.column ?? 1}: ${message}`
    : message;
}

/**
 * @typedef {object} CodeBlock
 * @property {HTMLElement} container the element the host goes before
 * @property {HTMLElement} body the part hidden while the chart shows
 * @property {string} text the block's source
 * @property {string} label the block's language label ('' when none)
 */

/**
 * @typedef {object} MountOptions
 * @property {'source'|'auto'} theme
 * @property {boolean} [claimed] show the viewer's error in place of the chart when it rejects the source
 * @property {(message: string) => void} [onFix] called with the error text by the "Ask Gemini to fix" button
 */

/** A chart before one code block. */
export class CodeReplacement extends ChartHost {
  /** @param {CodeBlock} block */
  constructor(block) {
    super('nymph chart');
    this.block = block;
    /** Whether the user switched this chart to its code. */
    this.showingCode = false;
    /** The host's `aspect-ratio` while the chart shows, restored after the code. */
    this.shape = '';
    /** The viewer's error when the host shows one instead of a chart, else null. */
    this.error = null;
  }

  /**
   * Creates the viewer before the block and hides the code once drawn.
   * @param {MountOptions} options
   * @param {string} [source] the source to draw (default: the block's text);
   *   a parked chart passes its YAML with the user's view
   * @return {Promise<void>} rejects (the block untouched) when the viewer
   *   rejects the source or cannot start, unless the block is `claimed`: then
   *   the host shows the error and `this.error` holds its text
   */
  async mount(options, source = this.block.text) {
    const { container, body, text } = this.block;
    const host = document.createElement(HOST_TAG);
    host.className = 'nymph-ext-starting';
    const shadow = host.attachShadow({ mode: 'open' });
    shadow.innerHTML = `<style>${SHADOW_CSS}${CODE_CSS}</style><div class="box"></div>` +
      '<div class="error"><strong>nymph could not draw this chart</strong><div class="detail"></div>' +
      '<button type="button" class="fix" data-act="fix">Ask Gemini to fix</button></div>' +
      '<div class="badge"><button type="button" data-act="toggle">Code</button>' +
      '<button type="button" data-act="copy" title="Copy the chart\'s YAML">Copy</button></div>';
    if (!options.onFix) {
      shadow.querySelector('.fix').hidden = true;
    }
    const hint = sourceSize(text);
    // Until the chart is drawn the box has the shape the source asked for, so
    // a frame (which has no height of its own) has room to start in.
    Object.assign(host.style, {
      width: '100%',
      maxWidth: `${hint.width}px`,
      aspectRatio: `${hint.width} / ${hint.height}`,
    });
    this.host = host;
    container.before(host);
    const box = shadow.querySelector('.box');
    const settings = { theme: options.theme === 'auto' ? 'auto' : 'source', fileName: 'chart' };
    let failure = null;
    try {
      if (!container.isConnected) {
        throw new Error('the block left the page');
      }
      if (canRunInline()) {
        await this.startInline(box, { source }, settings);
      } else {
        await this.startFrame(box, { source }, settings);
      }
    } catch (e) {
      failure = e;
    }
    if (!host.isConnected || !container.isConnected) {
      this.unmount();
      throw new Error('the block left the page');
    }
    if (failure) {
      if (!options.claimed) {
        this.unmount();
        throw failure;
      }
      this.showError(shadow, describeError(failure));
    } else {
      if (this.layout?.width > 0 && this.layout?.height > 0) {
        host.style.maxWidth = `${this.layout.width}px`;
        host.style.aspectRatio = `${this.layout.width} / ${this.layout.height}`;
      }
      this.shape = host.style.aspectRatio;
    }
    body.setAttribute(HIDDEN_ATTR, HIDDEN);
    this.wire(shadow, options);
    host.classList.remove('nymph-ext-starting');
  }

  /** @private Replaces the chart by the error: frees the viewer, keeps the host. */
  showError(shadow, message) {
    this.error = message;
    try {
      this.viewer?.destroy();
    } catch {
      // already gone
    }
    this.viewer = null;
    this.port?.close();
    this.port = null;
    shadow.querySelector('.box').replaceChildren();
    shadow.querySelector('.detail').textContent = message;
    shadow.querySelector('[data-act="copy"]').textContent = 'Copy error';
    Object.assign(this.host.style, { aspectRatio: 'auto', maxWidth: '' });
    this.shape = '';
    this.host.classList.add('nymph-ext-error');
  }

  /** @private Badge buttons and the fix button. */
  wire(shadow, options) {
    const host = this.host;
    const body = this.block.body;
    const toggle = shadow.querySelector('[data-act="toggle"]');
    shadow.addEventListener('click', async (e) => {
      const act = e.target?.dataset?.act;
      if (act === 'toggle') {
        this.showingCode = !this.showingCode;
        host.classList.toggle('nymph-ext-showing-code', this.showingCode);
        // Showing the code: the host shrinks to its badge row instead of keeping
        // the chart's height as an empty gap.
        host.style.aspectRatio = this.showingCode ? 'auto' : this.shape || 'auto';
        host.style.minHeight = this.showingCode ? '32px' : '';
        body.setAttribute(HIDDEN_ATTR, this.showingCode ? SHOWN : HIDDEN);
        toggle.textContent = this.showingCode ? (this.error ? 'Error' : 'Chart') : 'Code';
      } else if (act === 'copy') {
        try {
          await navigator.clipboard.writeText(this.error ?? this.yaml() ?? this.block.text);
        } catch {
          // no clipboard permission here: the code block's own copy button remains
        }
      } else if (act === 'fix' && this.error) {
        options.onFix?.(this.error);
      }
    });
  }

  /** Shows the code as the page had it and frees the viewer. Safe to call twice. */
  unmount() {
    if (this.block.body.getAttribute(HIDDEN_ATTR) !== null) {
      this.block.body.removeAttribute(HIDDEN_ATTR);
    }
    this.release();
  }
}
