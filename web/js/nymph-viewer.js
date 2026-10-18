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
  constructor(core, handle) {
    this.core = core;
    this.handle = handle;
    this.queue = Promise.resolve();
    this.listeners = new Map();
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
    const result = this.queue.then(run, run);
    this.queue = result.catch(() => {});
    return result;
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
  }

  /** Sets the box the chart is shown in: CSS pixels and the device pixel ratio. */
  resize(width, height, pixelRatio = 1) {
    this.checkAlive();
    this.core.m._nymph_viewer_resize(this.handle, width, height, pixelRatio);
  }

  /**
   * Feeds a pointer event, in CSS pixels from the chart's top-left corner.
   * @param {'move'|'down'|'up'|'wheel'|'leave'} type
   * @param {{x?: number, y?: number, button?: number, wheelX?: number, wheelY?: number, touch?: boolean}} [e]
   */
  pointer(type, { x = 0, y = 0, button = 0, wheelX = 0, wheelY = 0, touch = false } = {}) {
    this.checkAlive();
    this.core.m._nymph_viewer_pointer(this.handle, POINTER[type] ?? 0, x, y, button, wheelX, wheelY, touch ? 1 : 0);
  }

  /** Back to the view the source describes. */
  resetView() {
    this.checkAlive();
    this.core.m._nymph_viewer_reset_view(this.handle);
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
    if (this.handle !== 0) {
      this.core.m._nymph_viewer_destroy(this.handle);
      this.handle = 0;
    }
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
 * @returns {Promise<NymphViewer>}
 */
export async function createViewer(container, options = {}) {
  const core = await loadNymph(options);
  const handle = core.m._nymph_viewer_create(FIT[options.fit ?? 'source'] ?? 0, options.tooltips === false ? 0 : 1);
  if (handle === 0) {
    throw new NymphError(core.lastError());
  }
  const viewer = new NymphViewer(core, handle);
  if (options.source !== undefined) {
    await viewer.setSource(options.source);
  } else if (options.png !== undefined) {
    await viewer.setPng(options.png);
  }
  return viewer;
}
