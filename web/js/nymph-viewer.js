// MIT License © 2026 Binary Dice Games
/**
 * @file nymph-viewer.js
 * @brief The nymph web viewer: nymph charts drawn live from their YAML
 *        source or a nymph PNG, with pan, zoom and data-point picks
 *        (spec/04_web_viewer/DESIGN.md section 5).
 *
 * One WebAssembly module is loaded per page, on the first viewer, and
 * shared by every viewer. The module does no I/O: this file fetches fonts
 * (from `fontBaseUrl`, only names of nymph's font catalog) and passes
 * bytes in.
 */

const FIT = { source: 0, width: 1, fill: 2 };
const POINTER = { move: 0, down: 1, up: 2, wheel: 3, leave: 4 };
const STATUS = { ok: 0, error: 1, fontsNeeded: 2, plainPng: 3 };

/** A problem in a nymph source (or a failed update). */
export class NymphError extends Error {
  /**
   * @param {{message: string, line?: number, column?: number, kind?: string, fonts?: string[]}} info
   */
  constructor(info) {
    super(info.message);
    this.name = 'NymphError';
    /** 1-based line in the source (or in the data given to setData), 0 if none. */
    this.line = info.line ?? 0;
    /** 1-based column, 0 if none. */
    this.column = info.column ?? 0;
    /** "source", "fonts" or "other". */
    this.kind = info.kind ?? 'other';
    /** For kind "fonts": the font files that could not be loaded. */
    this.fonts = info.fonts ?? [];
  }
}

// ── The shared module ─────────────────────────────────────────────────────────

/** Calls into the WebAssembly module, with strings and bytes marshalled. */
class Core {
  constructor(module, options) {
    this.m = module;
    this.fontBaseUrl = options.fontBaseUrl;
    this.fetchFont = options.fetchFont;
    /** @type {Map<string, Promise<void>>} one load per font, shared by every viewer */
    this.fontLoads = new Map();
    this.encoder = new TextEncoder();
    /** The hidden canvas of the WebGL2 context every viewer shares, once made. */
    this.glCanvas = null;
    this.glState = 'untried'; // 'ready', 'unavailable' or 'lost'
    /** @type {Set<NymphViewer>} viewers drawing with WebGL, to move to the CPU if it is lost */
    this.glViewers = new Set();
  }

  /**
   * Creates the one WebGL2 context of the page, on a hidden canvas (browsers
   * cap live contexts per page, so viewers share it and copy their frames
   * out). @returns {boolean} whether WebGL2 can be used
   */
  initGl() {
    if (this.glState !== 'untried') {
      return this.glState === 'ready';
    }
    this.glState = 'unavailable';
    if (typeof document === 'undefined') {
      return false;
    }
    const canvas = document.createElement('canvas');
    canvas.width = canvas.height = 1;
    this.m.specialHTMLTargets['!nymph-gl'] = canvas;
    if (this.withCString('!nymph-gl', (ptr) => this.m._nymph_gl_init(ptr)) !== 1) {
      return false;
    }
    canvas.addEventListener('webglcontextlost', (e) => {
      e.preventDefault();
      this.glState = 'lost';
      for (const viewer of [...this.glViewers]) {
        viewer.moveToCpu();
      }
    });
    this.glCanvas = canvas;
    this.glState = 'ready';
    return true;
  }

  /** Calls `fn(pointer, length)` with `bytes` copied into the module's memory. */
  withBytes(bytes, fn) {
    const ptr = this.m._malloc(Math.max(1, bytes.length));
    try {
      this.m.HEAPU8.set(bytes, ptr);
      return fn(ptr, bytes.length);
    } finally {
      this.m._free(ptr);
    }
  }

  withString(text, fn) {
    return this.withBytes(this.encoder.encode(text), fn);
  }

  /** A NUL-terminated string argument. */
  withCString(text, fn) {
    const bytes = this.encoder.encode(text);
    const z = new Uint8Array(bytes.length + 1);
    z.set(bytes);
    return this.withBytes(z, (ptr) => fn(ptr));
  }

  string(ptr) {
    return this.m.UTF8ToString(ptr);
  }

  lastError() {
    return JSON.parse(this.string(this.m._nymph_last_error())) ?? { message: 'unknown error' };
  }

  /** Fetches and registers the fonts `names`, each once per page. */
  async ensureFonts(names) {
    await Promise.all(
      names.map((name) => {
        if (!this.fontLoads.has(name)) {
          const load = (async () => {
            let bytes;
            try {
              bytes = await this.fetchFont(name);
            } catch (e) {
              throw new NymphError({ kind: 'fonts', fonts: [name], message: `font '${name}' could not be loaded: ${e.message}` });
            }
            const status = this.withCString(name, (namePtr) =>
              this.withBytes(bytes, (ptr, size) => this.m._nymph_register_font(namePtr, ptr, size)),
            );
            if (status !== STATUS.ok) {
              throw new NymphError({ ...this.lastError(), kind: 'fonts', fonts: [name] });
            }
          })();
          // A failed load may be retried by a later update.
          load.catch(() => this.fontLoads.delete(name));
          this.fontLoads.set(name, load);
        }
        return this.fontLoads.get(name);
      }),
    );
  }
}

let corePromise = null;

/** The default font fetcher: `fetch(fontBaseUrl + name)`. */
function fetchFrom(baseUrl) {
  return async (name) => {
    const response = await fetch(new URL(encodeURIComponent(name), baseUrl));
    if (!response.ok) {
      throw new Error(`${response.status} from ${response.url}`);
    }
    return new Uint8Array(await response.arrayBuffer());
  };
}

/**
 * Loads the WebAssembly module (once per page; later calls return the same
 * module and ignore their options).
 *
 * @param {object} [options]
 * @param {string|URL} [options.wasmUrl] the .wasm file (default: next to this file)
 * @param {string|URL} [options.fontBaseUrl] folder of the font files (default: fonts/ next to this file)
 * @param {(name: string) => Promise<Uint8Array>} [options.fetchFont] replaces the font fetcher
 */
export function loadNymph(options = {}) {
  if (!corePromise) {
    corePromise = (async () => {
      const here = import.meta.url;
      const { default: createNymphCore } = await import(new URL('./nymph-viewer-core.js', here).href);
      const wasmUrl = options.wasmUrl ? new URL(options.wasmUrl, here).href : undefined;
      const module = await createNymphCore(wasmUrl ? { locateFile: (path) => (path.endsWith('.wasm') ? wasmUrl : path) } : {});
      const fontBaseUrl = new URL(options.fontBaseUrl ?? './fonts/', here);
      return new Core(module, { fontBaseUrl, fetchFont: options.fetchFont ?? fetchFrom(fontBaseUrl) });
    })();
    corePromise.catch(() => {
      corePromise = null;
    });
  }
  return corePromise;
}

// ── A viewer ──────────────────────────────────────────────────────────────────

/**
 * One chart. Updates (setSource, setData, setPng) are applied in call
 * order; each resolves once the chart is shown, after fetching any font it
 * needs, and rejects with a NymphError, keeping the previous chart.
 */
export class NymphViewer {
  /** @private Use createViewer(). */
  constructor(core, handle, options) {
    this.core = core;
    this.handle = handle;
    this.options = options;
    this.queue = Promise.resolve();
    this.listeners = new Map();
    this.webgl = core.m._nymph_viewer_is_webgl(handle) === 1;
    if (this.webgl) {
      core.glViewers.add(this);
    }
    /** The canvas the chart is shown in (browsers), and its 2D context. */
    this.canvas = null;
    this.ctx2d = null;
    this.frameRequest = 0;
    this.clockOrigin = typeof performance !== 'undefined' ? performance.now() : 0;
    /** @type {Array<() => void>} resolvers of rendered() */
    this.renderedWaiters = [];
    this.plainImage = null;
  }

  // ── Showing it in a page ───────────────────────────────────────────────────

  /** @private Puts a canvas for the chart in `container`. */
  mount(container) {
    const canvas = document.createElement('canvas');
    canvas.style.display = 'block';
    canvas.setAttribute('role', 'img');
    container.appendChild(canvas);
    this.container = container;
    this.canvas = canvas;
    this.ctx2d = canvas.getContext('2d');
    this.fitToContainer();
  }

  /** @private Sizes the chart to its container's box and pixel ratio. */
  fitToContainer() {
    if (!this.container) {
      return;
    }
    const fit = this.options.fit ?? 'source';
    const width = this.container.clientWidth;
    const height = fit === 'fill' ? this.container.clientHeight : 0;
    this.resize(width, height, typeof devicePixelRatio === 'number' ? devicePixelRatio : 1);
  }

  /** @private Asks for an animation frame if one is due and none is asked for. */
  schedule() {
    if (this.handle === 0 || !this.canvas || this.frameRequest !== 0 || typeof requestAnimationFrame === 'undefined') {
      return;
    }
    if (!this.wantsFrame()) {
      this.settled();
      return;
    }
    this.frameRequest = requestAnimationFrame(() => {
      this.frameRequest = 0;
      this.drawFrame();
      this.schedule();
    });
  }

  /** @private Runs one frame and shows it in the canvas. */
  drawFrame() {
    if (this.handle === 0) {
      return;
    }
    const now = (performance.now() - this.clockOrigin) / 1000;
    if (this.webgl) {
      const drawn = this.core.m._nymph_viewer_frame_webgl(this.handle, now);
      if (drawn < 0) {
        throw new NymphError(this.core.lastError());
      }
      if (drawn === 1) {
        const width = this.core.m._nymph_viewer_pixel_width(this.handle);
        const height = this.core.m._nymph_viewer_pixel_height(this.handle);
        this.sizeCanvas(width, height);
        // The frame is in the bottom-left corner of the shared canvas.
        const gl = this.core.glCanvas;
        this.ctx2d.drawImage(gl, 0, gl.height - height, width, height, 0, 0, width, height);
      }
    } else {
      const frame = this.frameSoftware(now);
      if (frame) {
        this.sizeCanvas(frame.width, frame.height);
        this.ctx2d.putImageData(new ImageData(frame.rgba, frame.width, frame.height), 0, 0);
      }
    }
    this.afterFrame();
  }

  /** @private Device pixels of the canvas, and its CSS size from the geometry. */
  sizeCanvas(width, height) {
    const c = this.canvas;
    if (c.width !== width || c.height !== height) {
      c.width = width;
      c.height = height;
    }
    const g = this.state().geometry;
    c.style.width = `${g.layoutWidth * g.cssScale}px`;
    c.style.height = `${g.layoutHeight * g.cssScale}px`;
  }

  /** @private Hook for what follows a frame (events, in step 11). */
  afterFrame() {}

  /** @private Resolves rendered() once nothing is due and no update is pending. */
  settled() {
    const waiters = this.renderedWaiters;
    this.renderedWaiters = [];
    waiters.forEach((resolve) => resolve());
  }

  /**
   * Resolves once every update made so far is applied and drawn, and no
   * frame is due: the chart on screen is final.
   */
  async rendered() {
    await this.queue;
    if (!this.canvas || this.handle === 0 || (!this.wantsFrame() && this.frameRequest === 0)) {
      return;
    }
    await new Promise((resolve) => {
      this.renderedWaiters.push(resolve);
      this.schedule();
    });
  }

  /** @private The WebGL context was lost: carry on drawing on the CPU. */
  moveToCpu() {
    if (!this.webgl || this.handle === 0) {
      return;
    }
    const source = this.core.string(this.core.m._nymph_viewer_source(this.handle));
    this.core.glViewers.delete(this);
    this.core.m._nymph_viewer_destroy(this.handle);
    this.handle = this.core.m._nymph_viewer_create(FIT[this.options.fit ?? 'source'] ?? 0, this.options.tooltips === false ? 0 : 1, 0);
    this.webgl = false;
    if (this.themeOverride) {
      this.setTheme(this.themeOverride);
    }
    this.fitToContainer();
    if (source) {
      this.setSource(source).catch(() => {});
    }
  }

  /** What the viewer is showing and how. */
  info() {
    const s = this.handle === 0 ? null : this.state();
    return {
      rasterizer: this.webgl ? 'webgl' : 'software',
      hasFigure: s?.hasFigure ?? false,
      description: s?.description ?? '',
      size: s ? { width: s.geometry.layoutWidth * s.geometry.cssScale, height: s.geometry.layoutHeight * s.geometry.cssScale } : { width: 0, height: 0 },
    };
  }

  /** @private Runs `call` (returning a status) after earlier updates, fetching fonts as asked. */
  update(call) {
    const run = async () => {
      for (let attempt = 0; attempt < 4; ++attempt) {
        this.checkAlive();
        const status = call();
        if (status === STATUS.ok) {
          return true;
        }
        if (status === STATUS.plainPng) {
          return false;
        }
        const info = this.core.lastError();
        if (status === STATUS.fontsNeeded) {
          await this.core.ensureFonts(info.fonts);
          continue;
        }
        throw new NymphError(info);
      }
      throw new NymphError({ kind: 'fonts', message: 'fonts were registered but are still missing' });
    };
    const result = this.queue.then(run, run).then((shown) => {
      this.afterUpdate(shown);
      return shown;
    });
    this.queue = result.catch(() => {});
    return result;
  }

  /** @private A new chart (or a plain PNG) is in: label it and draw it. */
  afterUpdate(shown) {
    if (!this.canvas || this.handle === 0) {
      return;
    }
    this.canvas.setAttribute('aria-label', this.state().description);
    if (shown === false && this.plainImage) {
      this.showPlainImage(this.plainImage);
    }
    this.schedule();
  }

  /** @private Draws a PNG without a nymph source as it is. */
  async showPlainImage(bytes) {
    const bitmap = await createImageBitmap(new Blob([bytes], { type: 'image/png' }));
    this.sizeCanvas(bitmap.width, bitmap.height);
    this.canvas.style.width = `${Math.min(bitmap.width, this.container?.clientWidth || bitmap.width)}px`;
    this.canvas.style.height = 'auto';
    this.ctx2d.drawImage(bitmap, 0, 0);
  }

  /** @private */
  checkAlive() {
    if (this.handle === 0) {
      throw new NymphError({ message: 'the viewer was destroyed' });
    }
  }

  /**
   * Shows `yaml`.
   * @param {string} yaml a nymph source
   * @param {{resetView?: boolean}} [options] drop the zoom of the previous chart
   */
  setSource(yaml, { resetView = false } = {}) {
    return this.update(() =>
      this.core.withString(yaml, (ptr, size) => this.core.m._nymph_viewer_set_source(this.handle, ptr, size, resetView ? 1 : 0)),
    );
  }

  /**
   * Replaces the chart's data and keeps everything else, the view included.
   * @param {object|string} tables the value of the source's `data` key (an object, or YAML / JSON text)
   */
  setData(tables) {
    const text = typeof tables === 'string' ? tables : JSON.stringify(tables);
    return this.update(() => this.core.withString(text, (ptr, size) => this.core.m._nymph_viewer_set_data(this.handle, ptr, size)));
  }

  /**
   * Shows the source a nymph PNG carries.
   * @param {Uint8Array} bytes
   * @returns {Promise<boolean>} false for a PNG without a nymph source (no chart; show it as an image)
   */
  setPng(bytes, { resetView = true } = {}) {
    this.plainImage = bytes;
    return this.update(() =>
      this.core.withBytes(bytes, (ptr, size) => this.core.m._nymph_viewer_set_png(this.handle, ptr, size, resetView ? 1 : 0)),
    );
  }

  /** Overrides the source's theme: "light", "dark", "classic", or "" for the source's own. */
  setTheme(theme) {
    this.checkAlive();
    if (this.core.withCString(theme, (ptr) => this.core.m._nymph_viewer_set_theme(this.handle, ptr)) !== STATUS.ok) {
      throw new NymphError(this.core.lastError());
    }
    this.themeOverride = theme;
    this.schedule();
  }

  /** Sets the box the chart is shown in: CSS pixels and the device pixel ratio. */
  resize(width, height, pixelRatio = 1) {
    this.checkAlive();
    this.core.m._nymph_viewer_resize(this.handle, width, height, pixelRatio);
    this.schedule();
  }

  /**
   * Feeds a pointer event, in CSS pixels from the chart's top-left corner.
   * @param {'move'|'down'|'up'|'wheel'|'leave'} type
   * @param {{x?: number, y?: number, button?: number, wheelX?: number, wheelY?: number, touch?: boolean}} [e]
   */
  pointer(type, { x = 0, y = 0, button = 0, wheelX = 0, wheelY = 0, touch = false } = {}) {
    this.checkAlive();
    this.core.m._nymph_viewer_pointer(this.handle, POINTER[type] ?? 0, x, y, button, wheelX, wheelY, touch ? 1 : 0);
    this.schedule();
  }

  /** Back to the view the source describes. */
  resetView() {
    this.checkAlive();
    this.core.m._nymph_viewer_reset_view(this.handle);
    this.schedule();
  }

  /** Whether a frame is due (after an update, input or resize). */
  wantsFrame() {
    return this.handle !== 0 && this.core.m._nymph_viewer_wants_frame(this.handle) === 1;
  }

  /**
   * Runs a due frame with the software rasterizer.
   * @param {number} nowSeconds a monotonic clock
   * @returns {{width: number, height: number, rgba: Uint8ClampedArray}|null} null when no frame was due
   */
  frameSoftware(nowSeconds) {
    this.checkAlive();
    const m = this.core.m;
    const drawn = m._nymph_viewer_frame_software(this.handle, nowSeconds);
    if (drawn < 0) {
      throw new NymphError(this.core.lastError());
    }
    if (drawn === 0) {
      return null;
    }
    const width = m._nymph_viewer_pixel_width(this.handle);
    const height = m._nymph_viewer_pixel_height(this.handle);
    const ptr = m._nymph_viewer_pixels(this.handle);
    return { width, height, rgba: new Uint8ClampedArray(m.HEAPU8.buffer.slice(ptr, ptr + width * height * 4)) };
  }

  /**
   * The chart's state after the last frame: geometry, description, the
   * hovered pick, the click since the last call (taken), plot views.
   */
  state() {
    this.checkAlive();
    return JSON.parse(this.core.string(this.core.m._nymph_viewer_state(this.handle)));
  }

  /** The source on screen (with setData applied). */
  source() {
    this.checkAlive();
    return this.core.string(this.core.m._nymph_viewer_source(this.handle));
  }

  /** The chart as a nymph PNG (drawn on the CPU), its source embedded. */
  toPng() {
    this.checkAlive();
    const m = this.core.m;
    if (m._nymph_viewer_to_png(this.handle) !== STATUS.ok) {
      throw new NymphError(this.core.lastError());
    }
    const ptr = m._nymph_png_data();
    return new Uint8Array(m.HEAPU8.buffer.slice(ptr, ptr + m._nymph_png_size()));
  }

  /** Frees the chart; the viewer cannot be used afterwards. */
  destroy() {
    if (this.frameRequest !== 0) {
      cancelAnimationFrame(this.frameRequest);
      this.frameRequest = 0;
    }
    this.canvas?.remove();
    this.canvas = null;
    this.core.glViewers.delete(this);
    if (this.handle !== 0) {
      this.core.m._nymph_viewer_destroy(this.handle);
      this.handle = 0;
    }
    this.settled();
  }
}

/**
 * Creates a viewer (and loads the module on first use).
 * @param {HTMLElement|null} container where the chart is shown (null: drive it yourself, as in Node)
 * @param {object} [options]
 * @param {string} [options.source] YAML to show
 * @param {Uint8Array} [options.png] or a nymph PNG
 * @param {'source'|'width'|'fill'} [options.fit]
 * @param {boolean} [options.tooltips] nymph's own tooltips (default true)
 * @param {'auto'|'webgl'|'software'} [options.rasterizer] WebGL2 when available (default), or the CPU
 * @returns {Promise<NymphViewer>}
 */
export async function createViewer(container, options = {}) {
  const core = await loadNymph(options);
  // WebGL2 in a page unless asked otherwise; the CPU where it is missing.
  const webgl = container != null && options.rasterizer !== 'software' && core.initGl();
  const handle = core.m._nymph_viewer_create(FIT[options.fit ?? 'source'] ?? 0, options.tooltips === false ? 0 : 1, webgl ? 1 : 0);
  if (handle === 0) {
    throw new NymphError(core.lastError());
  }
  const viewer = new NymphViewer(core, handle, options);
  if (container != null) {
    viewer.mount(container);
  }
  if (options.source !== undefined) {
    await viewer.setSource(options.source);
  } else if (options.png !== undefined) {
    await viewer.setPng(options.png);
  }
  return viewer;
}
