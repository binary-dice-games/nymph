// MIT License © 2026 Binary Dice Games
/**
 * @file replacer.js
 * @brief Puts a viewer in the place of an image, and the image back
 * (spec/05_browser_extension/DESIGN.md sections 3.2 to 3.4).
 *
 * The viewer goes in a host element right after the image, in a shadow
 * root (page CSS cannot reach it), covered by a copy of the image
 * until its first frame is drawn. The image is hidden with an
 * attribute and one stylesheet rule, never removed, so the page's
 * framework keeps owning it.
 *
 * Two ways to run the viewer (section 3.9): **inline**, in this content
 * script (one module and one WebGL context for the page), or, when the
 * page's Content Security Policy blocks WebAssembly here, in an
 * extension **frame** per chart.
 */

import { COPIED, hostStyle } from './sizing.js';

/** The attribute that hides a replaced image. */
export const HIDDEN_ATTR = 'data-nymph-ext';
/** The host element's tag (never registered, so it is just a box). */
export const HOST_TAG = 'nymph-ext-chart';
const STYLE_ID = 'nymph-ext-style';

/** The page-level rule that hides replaced images. */
export function injectPageStyle() {
  if (document.getElementById(STYLE_ID)) {
    return;
  }
  const style = document.createElement('style');
  style.id = STYLE_ID;
  style.textContent = `img[${HIDDEN_ATTR}="hidden"] { display: none !important; }`;
  (document.head ?? document.documentElement).appendChild(style);
}

/** Removes the rule (turning the extension off). */
export function removePageStyle() {
  document.getElementById(STYLE_ID)?.remove();
}

const SHADOW_CSS = `
:host { position: relative; line-height: 0; }
.box { width: 100%; }
.box > canvas { max-width: 100%; }
.box > iframe { position: absolute; inset: 0; width: 100%; height: 100%; border: 0; display: block;
  background: transparent; color-scheme: normal; }
.image { display: none; width: 100%; height: auto; }
:host(.nymph-ext-starting) .image { display: block; position: absolute; inset: 0; height: 100%; z-index: 2; }
:host(.nymph-ext-starting) .badge { display: none; }
:host(.nymph-ext-showing-image) .box { display: none; }
:host(.nymph-ext-showing-image) .image { display: block; }
.badge { position: absolute; top: 6px; right: 6px; display: flex; gap: 4px; opacity: 0;
  transition: opacity .15s; line-height: normal; z-index: 1; }
:host(:hover) .badge, .badge:focus-within { opacity: 1; }
.badge button { font: 600 11px/1 system-ui, sans-serif; width: 52px; padding: 4px 0; border-radius: 4px; cursor: pointer;
  color: #fff; background: rgba(40, 60, 110, .82); border: 1px solid rgba(255, 255, 255, .35); }
.badge button:hover, .badge button:focus-visible { background: rgba(51, 103, 214, .95); outline: none; }
`;

let viewerModule = null;

/** The bundled viewer API, loaded on the first nymph image. */
async function loadViewer() {
  viewerModule ??= import(chrome.runtime.getURL('viewer/nymph-viewer.js'));
  return viewerModule;
}

let inlineAllowed = null;

/**
 * Whether WebAssembly may be compiled in this content script: a page's
 * Content Security Policy without 'wasm-unsafe-eval' forbids it here.
 * @return {boolean}
 */
export function canRunInline() {
  if (inlineAllowed === null) {
    try {
      new WebAssembly.Module(new Uint8Array([0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00]));
      inlineAllowed = true;
    } catch {
      inlineAllowed = false;
    }
  }
  return inlineAllowed;
}

/**
 * How long a frame may take to load. Showing the chart has no limit:
 * Chrome runs a frame only while it is on screen, so a chart below the
 * viewport is drawn (and swapped in) when the user scrolls to it.
 */
const FRAME_LOAD_TIMEOUT_MS = 30000;

/** What hostStyle() needs of the image, read before it is hidden. */
function snapshot(img) {
  const cs = getComputedStyle(img);
  const computed = { display: cs.display };
  for (const name of COPIED) {
    computed[name] = cs[name];
  }
  const rect = img.getBoundingClientRect();
  const px = (v) => parseFloat(v) || 0;
  const contentWidth =
    rect.width - px(cs.paddingLeft) - px(cs.paddingRight) - px(cs.borderLeftWidth) - px(cs.borderRightWidth);
  return {
    sizing: {
      attrWidth: img.getAttribute('width'),
      attrHeight: img.getAttribute('height'),
      inlineWidth: img.style.width,
      inlineHeight: img.style.height,
      naturalWidth: img.naturalWidth,
      naturalHeight: img.naturalHeight,
      computed,
    },
    contentWidth,
  };
}

/** The file name for "Save as": the image's, without its extension. */
function fileNameOf(url) {
  try {
    const last = new URL(url).pathname.split('/').pop() ?? '';
    const name = decodeURIComponent(last).replace(/\.png$/i, '');
    return /^[\w .-]{1,80}$/.test(name) ? name : 'chart';
  } catch {
    return 'chart';
  }
}

/**
 * A chart in place of one image.
 */
export class Replacement {
  /**
   * @param {HTMLImageElement} img the image replaced
   * @param {string} url the image's URL when it was read
   */
  constructor(img, url) {
    this.img = img;
    this.url = url;
    /** @type {HTMLElement|null} */
    this.host = null;
    this.viewer = null;
    /** Whether the user switched this chart back to its image. */
    this.showingImage = false;
    /** Frame mode: the port to the frame, and the chart's latest source with its view. */
    this.port = null;
    this.lastYaml = null;
  }

  /**
   * Creates the viewer and swaps it in once drawn.
   * @param {{png?: Uint8Array, source?: string}} content a nymph PNG, or a source (a parked chart)
   * @param {{theme: 'source'|'auto'}} options
   * @return {Promise<void>} rejects (the image untouched) when the viewer
   *   rejects the content or cannot start
   */
  async mount(content, options) {
    const img = this.img;
    const shot = snapshot(img);
    const host = document.createElement(HOST_TAG);
    // Until the chart is drawn, a copy of the image covers it, so the swap
    // is invisible. The viewer is not hidden meanwhile: Chrome does not run
    // frames in an iframe it considers hidden, and the chart would never
    // draw (frame mode).
    host.className = 'nymph-ext-starting';
    const shadow = host.attachShadow({ mode: 'open' });
    shadow.innerHTML = `<style>${SHADOW_CSS}</style><div class="box"></div><img class="image" alt="">` +
      '<div class="badge"><button type="button" data-act="toggle">Image</button>' +
      '<button type="button" data-act="open" title="Open the image">Open</button></div>';
    const box = shadow.querySelector('.box');
    const cover = shadow.querySelector('.image');
    cover.src = this.url;
    cover.alt = img.alt;
    await cover.decode().catch(() => {});
    if (!img.isConnected) {
      throw new Error('the image left the page');
    }
    Object.assign(host.style, hostStyle(shot.sizing));
    this.host = host;
    img.after(host);
    img.setAttribute(HIDDEN_ATTR, 'hidden');
    // A stylesheet width rule on the image (not visible in its attributes)
    // shows as a different width: keep the image's.
    const s = getComputedStyle(host);
    const px = (v) => parseFloat(v) || 0;
    const width =
      host.getBoundingClientRect().width - px(s.paddingLeft) - px(s.paddingRight) - px(s.borderLeftWidth) - px(s.borderRightWidth);
    if (shot.contentWidth > 0 && Math.abs(width - shot.contentWidth) > 1) {
      host.style.width = `${shot.contentWidth}px`;
      host.style.maxWidth = '100%';
    }
    const settings = {
      theme: options.theme === 'auto' ? 'auto' : 'source',
      fileName: fileNameOf(this.url),
    };
    try {
      if (canRunInline()) {
        await this.startInline(box, content, settings);
      } else {
        await this.startFrame(box, content, settings);
      }
    } catch (e) {
      this.unmount();
      throw e;
    }
    if (!host.isConnected || !img.isConnected) {
      this.unmount();
      throw new Error('the image left the page');
    }
    this.wire(shadow);
    host.classList.remove('nymph-ext-starting');
    const canvas = this.viewer?.canvas;
    if (canvas && !canvas.getAttribute('aria-label') && img.alt) {
      canvas.setAttribute('aria-label', img.alt);
    }
  }

  /** @private The viewer in this content script. */
  async startInline(box, content, settings) {
    const nymph = await loadViewer();
    const viewer = await nymph.createViewer(box, { ...content, fit: 'scale', ...settings });
    this.viewer = viewer;
    if (!viewer.info().hasFigure) {
      throw new Error('not a nymph source');
    }
    await viewer.rendered();
  }

  /** @private The viewer in an extension frame. */
  async startFrame(box, content, settings) {
    const frame = document.createElement('iframe');
    frame.title = this.img.alt || 'nymph chart';
    frame.setAttribute('scrolling', 'no');
    const url = chrome.runtime.getURL('src/frame/frame.html');
    const loaded = new Promise((resolve, reject) => {
      const timer = setTimeout(() => reject(new Error('the frame did not load')), FRAME_LOAD_TIMEOUT_MS);
      frame.addEventListener('load', () => (clearTimeout(timer), resolve()), { once: true });
    });
    frame.src = url;
    box.appendChild(frame);
    await loaded;
    const channel = new MessageChannel();
    this.port = channel.port1;
    const shown = new Promise((resolve, reject) => {
      this.port.onmessage = (m) => {
        const msg = m.data ?? {};
        if (msg.type === 'yaml' && typeof msg.yaml === 'string') {
          this.lastYaml = msg.yaml;
        } else if (msg.type === 'rendered') {
          resolve();
        } else if (msg.type === 'error') {
          reject(new Error(msg.message));
        }
      };
    });
    frame.contentWindow.postMessage({ type: 'nymph-frame-init' }, new URL(url).origin, [channel.port2]);
    this.port.postMessage({ type: 'show', ...content, ...settings });
    await shown;
  }

  /** @private Badge buttons and link handling. */
  wire(shadow) {
    const host = this.host;
    const toggle = shadow.querySelector('[data-act="toggle"]');
    const picture = shadow.querySelector('.image');
    shadow.querySelector('.badge').addEventListener('click', (e) => {
      const act = e.target?.dataset?.act;
      if (act === 'toggle') {
        this.showingImage = !this.showingImage;
        host.classList.toggle('nymph-ext-showing-image', this.showingImage);
        toggle.textContent = this.showingImage ? 'Chart' : 'Image';
      } else if (act === 'open') {
        const link = this.img.closest('a[href]');
        window.open(link ? link.href : this.url, '_blank', 'noopener');
      }
    });
    // Images are often inside links: using the chart must not follow them.
    for (const type of ['click', 'auxclick', 'dragstart']) {
      host.addEventListener(type, (e) => {
        e.preventDefault();
        e.stopPropagation();
      });
    }
  }

  /**
   * The viewer's source with the user's view, for parking.
   * @return {string|null}
   */
  yaml() {
    if (this.port) {
      return this.lastYaml;
    }
    try {
      return this.viewer?.toYaml() ?? null;
    } catch {
      return null;
    }
  }

  /** Shows the image again and frees the viewer. Safe to call twice. */
  unmount() {
    this.img.removeAttribute(HIDDEN_ATTR);
    try {
      this.viewer?.destroy();
    } catch {
      // already gone
    }
    this.viewer = null;
    this.port?.close();
    this.port = null;
    this.host?.remove();
    this.host = null;
  }
}
