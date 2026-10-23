// MIT License © 2026 Binary Dice Games
/**
 * @file nymph-editor.js
 * The nymph editor in a web page: the editor of `nymph view --edit` (click to
 * select, inspector, floating toolbar, tree and data editing) compiled to
 * WebAssembly (nymph-editor-core.wasm, separate from the viewer's module so
 * the viewer stays small), drawn with WebGL2 into one canvas
 * (spec/15_web_app/DESIGN.md sections 5.2 and 5.3).
 *
 *   import { createEditor } from './editor/nymph-editor.js';
 *   const editor = await createEditor(document.getElementById('box'), { source: yamlText });
 *   editor.on('change', (c) => console.log(c.label, c.origin, editor.source()));
 *
 * The module does no I/O: the page passes source text or PNG bytes and
 * answers the editor's requests (save, CSV import / export). Fonts are
 * fetched from `fontBaseUrl` (the viewer's fonts/ folder by default), only
 * names of nymph's font catalog.
 */

import { NymphError, fetchFrom } from '../nymph-viewer.js';

export { NymphError };

const STATUS = { ok: 0, error: 1, fontsNeeded: 2, plainPng: 3 };
const POINTER = { move: 0, down: 1, up: 2, wheel: 3, leave: 4 };

/** Calls into the module, with strings and bytes marshalled. */
class Core {
  constructor(module, fetchFont) {
    this.m = module;
    this.fetchFont = fetchFont;
    this.fontLoads = new Map();
    this.encoder = new TextEncoder();
  }

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

  /** Calls `fn` with NUL-terminated copies of `texts`. */
  withCStrings(texts, fn) {
    const ptrs = [];
    try {
      for (const t of texts) {
        const bytes = this.encoder.encode(t);
        const ptr = this.m._malloc(bytes.length + 1);
        this.m.HEAPU8.set(bytes, ptr);
        this.m.HEAPU8[ptr + bytes.length] = 0;
        ptrs.push(ptr);
      }
      return fn(...ptrs);
    } finally {
      ptrs.forEach((p) => this.m._free(p));
    }
  }

  string(ptr) {
    return this.m.UTF8ToString(ptr);
  }

  lastError() {
    return JSON.parse(this.string(this.m._nymph_editor_last_error())) ?? { message: 'unknown error' };
  }

  /** Throws the module's last error unless `status` is ok. */
  check(status) {
    if (status !== STATUS.ok) {
      throw new NymphError(this.lastError());
    }
  }

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
            const status = this.withCStrings([name], (namePtr) =>
              this.withBytes(bytes, (ptr, size) => this.m._nymph_editor_register_font(namePtr, ptr, size)),
            );
            this.check(status);
          })();
          load.catch(() => this.fontLoads.delete(name));
          this.fontLoads.set(name, load);
        }
        return this.fontLoads.get(name);
      }),
    );
  }

  /** Runs `call` (returning a status), fetching the fonts it asks for and retrying. */
  async withFonts(call) {
    for (let attempt = 0; attempt < 4; ++attempt) {
      const status = call();
      if (status !== STATUS.fontsNeeded) {
        return status;
      }
      await this.ensureFonts(this.lastError().fonts ?? []);
    }
    return call();
  }
}

/** One module instance per editor: the module holds one editor. */
async function loadCore(options) {
  const here = import.meta.url;
  const create = (await import(new URL('./nymph-editor-core.js', here).href)).default;
  const wasmUrl = options.wasmUrl ? new URL(options.wasmUrl, here).href : undefined;
  const module = await create(wasmUrl ? { locateFile: (path) => (path.endsWith('.wasm') ? wasmUrl : path) } : {});
  // The viewer's font folder: fonts/ next to nymph-viewer.js, one level up.
  const fontBase = options.fontBaseUrl ?? new URL('../fonts/', here).href;
  return new Core(module, options.fetchFont ?? fetchFrom(fontBase, here));
}

function isPng(bytes) {
  const sig = [0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a];
  return bytes && bytes.length >= 8 && sig.every((b, i) => bytes[i] === b);
}

/**
 * The editor. Created by createEditor(); every method runs synchronously in
 * the module except those that may fetch fonts (open, setSource), which
 * return promises.
 */
export class NymphEditor {
  /** @private */
  constructor(core, container, options) {
    this.core = core;
    this.m = core.m;
    this.container = container;
    this.options = options;
    this.listeners = new Map();
    this.destroyed = false;
    this.frameRequested = false;
    this.headless = !!options.headless;
    this.m.nymphEditorEvent = (kind, payload) => this.event(kind, payload);
  }

  /** @private Creates the canvas, the GL context and the editor. */
  init() {
    let target = '';
    if (!this.headless) {
      // Its own box, so the page's container keeps its own positioning.
      this.root = document.createElement('div');
      this.root.className = 'nymph-editor';
      this.root.style.cssText = 'position:relative;width:100%;height:100%;overflow:hidden;';
      this.container.appendChild(this.root);
      const canvas = document.createElement('canvas');
      canvas.className = 'nymph-editor-canvas';
      canvas.style.cssText = 'display:block;width:100%;height:100%;outline:none;touch-action:none;';
      canvas.tabIndex = 0;
      canvas.setAttribute('role', 'application');
      canvas.setAttribute(
        'aria-label',
        this.options.label ?? 'Visual chart editor (not readable by screen readers: download the YAML to edit it elsewhere)',
      );
      this.root.appendChild(canvas);
      this.canvas = canvas;
      target = '!nymph-editor';
      this.m.specialHTMLTargets[target] = canvas;
    }
    this.core.check(
      this.core.withCStrings([target], (t) => this.m._nymph_editor_create(t, this.headless ? 1 : 0, this.options.panel === false ? 0 : 1)),
    );
    if (this.options.fileName) {
      this.core.withCStrings([this.options.fileName], (n) => this.m._nymph_editor_set_file_name(n));
    }
    if (this.options.theme) {
      this.setTheme(this.options.theme);
    }
    if (!this.headless) {
      this.attachInput();
      this.observer = new ResizeObserver(() => this.resize());
      this.observer.observe(this.container);
    }
    this.resize();
  }

  // ── Documents ──────────────────────────────────────────────────────────

  /**
   * Opens a YAML source or a nymph PNG as a new document (no undo history,
   * not modified). Rejects with a NymphError when it does not bind; a PNG
   * without a nymph chart rejects with kind "png".
   * @param {string|Uint8Array} input
   */
  async open(input, opts = {}) {
    let status;
    if (typeof input !== 'string' && isPng(input)) {
      status = await this.core.withFonts(() => this.core.withBytes(input, (p, n) => this.m._nymph_editor_open_png(p, n)));
      if (status === STATUS.plainPng) {
        throw new NymphError({ kind: 'png', message: 'this PNG has no nymph chart in it' });
      }
    } else {
      const text = typeof input === 'string' ? input : new TextDecoder().decode(input);
      status = await this.core.withFonts(() =>
        this.core.withString(text, (p, n) =>
          this.m._nymph_editor_open(p, n, opts.resetView === false ? 0 : 1, (opts.repair ?? this.options.repair) ? 1 : 0),
        ),
      );
    }
    this.core.check(status);
    this.schedule();
  }

  /**
   * Replaces the source. With `asChange` it is one undo step of the document
   * (the shell's own edits, an agent's rewrite), labelled `label` and
   * attributed to `origin`; without, it opens it as a new document.
   */
  async setSource(yaml, opts = {}) {
    if (!opts.asChange) {
      return this.open(yaml, opts);
    }
    await this.core.ensureFonts([]);
    const status = this.core.withCStrings([opts.label ?? 'Edit source', opts.origin ?? 'user'], (l, o) =>
      this.core.withString(yaml, (p, n) => this.m._nymph_editor_change_source(p, n, l, o)),
    );
    this.core.check(status);
    this.schedule();
  }

  /** The current YAML, without the view. */
  source() {
    return this.core.string(this.m._nymph_editor_source());
  }

  /** The source pane's text: ahead of source() while it waits or does not load. */
  paneText() {
    return this.core.string(this.m._nymph_editor_pane_text());
  }

  /** The YAML with the view on screen in its `view` key: what a save writes. */
  toYaml() {
    return this.core.string(this.m._nymph_editor_yaml());
  }

  /** A PNG of toYaml(), which it carries. */
  toPng() {
    const size = this.m._nymph_editor_to_png();
    if (size < 0) {
      throw new NymphError(this.core.lastError());
    }
    return this.m.HEAPU8.slice(this.m._nymph_editor_png_data(), this.m._nymph_editor_png_data() + size);
  }

  /** {modified, version, undo, redo, selection, panel, error, ...} */
  state() {
    return JSON.parse(this.core.string(this.m._nymph_editor_state()));
  }

  modified() {
    return !!this.state().modified;
  }

  version() {
    return this.state().version ?? 0;
  }

  /** After a save: the current source is the saved one. */
  markSaved() {
    this.core.check(this.m._nymph_editor_mark_saved());
  }

  undo() {
    this.core.check(this.m._nymph_editor_undo());
    this.schedule();
  }

  redo() {
    this.core.check(this.m._nymph_editor_redo());
    this.schedule();
  }

  resetView() {
    this.core.check(this.m._nymph_editor_reset_view());
    this.schedule();
  }

  /** "light", "dark", "classic", or "" for the source's own. */
  setTheme(theme) {
    this.core.check(this.core.withCStrings([theme === 'source' ? '' : theme], (t) => this.m._nymph_editor_set_theme(t)));
    this.schedule();
  }

  showPanel(on) {
    this.core.check(this.m._nymph_editor_show_panel(on ? 1 : 0));
    this.schedule();
  }

  /**
   * Shows or hides the YAML source pane beside the figure (Ctrl+U in the
   * editor); `line` (1-based) puts its cursor there.
   */
  showSource(on, line = 0) {
    if (on) {
      this.core.ensureFonts(['FiraMono-Regular.ttf']).then(() => this.schedule(), () => {});
    }
    this.core.check(this.m._nymph_editor_show_source(on ? 1 : 0, line));
    this.schedule();
  }

  /** The source does not bind: only the source pane edits until it does. */
  repairing() {
    return !!this.state().repairing;
  }

  // ── Tools (spec/14_editor_workspace/DESIGN.md section 6) ──────────────

  /**
   * Runs an editor tool (editor_state, editor_set_fields, editor_add_element,
   * ...) with its JSON arguments; changes are attributed to `origin`.
   * @returns {object} the CallToolResult's structured content (or its text
   *          as `{text}`); throws a NymphError for an error result.
   */
  tool(name, args = {}, opts = {}) {
    const raw = this.core.withCStrings([name, JSON.stringify(args), opts.origin ?? 'user'], (n, a, o) =>
      this.m._nymph_editor_tool(n, a, o),
    );
    const result = JSON.parse(this.core.string(raw));
    if (result === null) {
      throw new NymphError(this.core.lastError());
    }
    this.schedule();
    const text = result.content?.[0]?.text ?? '';
    if (result.isError) {
      throw new NymphError({ kind: 'tool', message: text });
    }
    if (result.structuredContent) {
      return result.structuredContent;
    }
    const image = result.content?.find((c) => c.type === 'image');
    if (image) {
      return { image: image.data, mimeType: image.mimeType, ...(result.content.find((c) => c.type === 'text') ? { text } : {}) };
    }
    try {
      return JSON.parse(text);
    } catch {
      return { text };
    }
  }

  /** The tools' `tools/list` entries (name, description, inputSchema, ...). */
  toolDefinitions() {
    return JSON.parse(this.core.string(this.m._nymph_editor_tool_definitions()));
  }

  /** Makes the changes until endGroup() one undo step (an agent's turn). */
  beginGroup(label, origin) {
    this.core.check(this.core.withCStrings([label, origin], (l, o) => this.m._nymph_editor_begin_group(l, o)));
  }

  endGroup() {
    this.core.check(this.m._nymph_editor_end_group());
  }

  /** Where element `path`'s `field` is drawn, CSS pixels in the canvas, or null. */
  targetRect(path, field = '') {
    return JSON.parse(this.core.string(this.core.withCStrings([path, field], (p, f) => this.m._nymph_editor_target(p, f))));
  }

  /** The answer to a "csv-import" event: the file's text replaces the table. */
  importCsv(table, csv) {
    this.core.check(this.core.withCStrings([table, csv], (t, c) => this.m._nymph_editor_import_csv(t, c)));
    this.schedule();
  }

  // ── Events ─────────────────────────────────────────────────────────────

  /**
   * "change" {version, label, origin, modified}, "selection" {path, field} or
   * null, "save", "csv-import" {table}, "csv-export" {table, fileName, csv},
   * "error". @returns a function that removes the listener
   */
  on(event, cb) {
    if (!this.listeners.has(event)) {
      this.listeners.set(event, new Set());
    }
    this.listeners.get(event).add(cb);
    return () => this.listeners.get(event)?.delete(cb);
  }

  /** @private */
  emit(event, detail) {
    for (const cb of this.listeners.get(event) ?? []) {
      try {
        cb(detail);
      } catch (e) {
        console.error(e);
      }
    }
  }

  /** @private From the module. */
  event(kind, payload) {
    let detail = null;
    try {
      detail = JSON.parse(payload);
    } catch {
      detail = payload;
    }
    if (kind === 'cursor') {
      if (this.canvas) {
        this.canvas.style.cursor = detail;
      }
      return;
    }
    if (kind === 'fonts') {
      this.core
        .ensureFonts(detail)
        .then(() => {
          this.core.check(this.m._nymph_editor_retry_view());
          this.schedule();
        })
        .catch((e) => this.emit('error', e));
      return;
    }
    if (kind === 'copy') {
      if (typeof navigator !== 'undefined' && navigator.clipboard) {
        navigator.clipboard.writeText(detail).catch(() => {});
      }
      return;
    }
    this.emit(kind, detail);
  }

  // ── Drawing and input ──────────────────────────────────────────────────

  /** @private */
  resize() {
    if (this.destroyed) {
      return;
    }
    const ratio = this.headless ? 1 : Math.min(window.devicePixelRatio || 1, 3);
    const box = this.headless ? { width: this.options.width ?? 1000, height: this.options.height ?? 640 } : this.container.getBoundingClientRect();
    const w = Math.max(1, Math.floor(box.width));
    const h = Math.max(1, Math.floor(box.height));
    if (this.canvas) {
      this.canvas.width = Math.round(w * ratio);
      this.canvas.height = Math.round(h * ratio);
    }
    this.m._nymph_editor_resize(w, h, ratio);
    this.schedule();
  }

  /** @private Asks for frames while the editor wants them. */
  schedule() {
    if (this.destroyed || this.frameRequested) {
      return;
    }
    if (this.headless) {
      return; // frames run on frame()
    }
    this.frameRequested = true;
    requestAnimationFrame(() => {
      this.frameRequested = false;
      if (this.destroyed) {
        return;
      }
      if (this.m._nymph_editor_frame(performance.now() / 1000) < 0) {
        this.emit('error', new NymphError(this.core.lastError()));
      }
      this.syncTextInput();
      if (this.m._nymph_editor_wants_frame() === 1) {
        this.schedule();
      }
    });
  }

  /**
   * Runs the due frames now (headless editors, tests).
   * @param {number} [count] frames to run
   */
  frame(count = 1) {
    for (let i = 0; i < count; ++i) {
      this.clock = (this.clock ?? 1000) + 1 / 60;
      this.m._nymph_editor_frame(this.headless ? this.clock : performance.now() / 1000);
    }
  }

  /** @private Pointer, wheel, keys, text and paste on the canvas. */
  attachInput() {
    const canvas = this.canvas;
    const at = (e) => {
      const r = canvas.getBoundingClientRect();
      return { x: e.clientX - r.left, y: e.clientY - r.top };
    };
    const pointer = (type, p, button = 0, wx = 0, wy = 0, touch = 0) => {
      this.m._nymph_editor_pointer(POINTER[type], p.x, p.y, button, wx, wy, touch);
      this.schedule();
    };
    const add = (target, name, fn, opts) => {
      target.addEventListener(name, fn, opts);
      (this.removers ??= []).push(() => target.removeEventListener(name, fn, opts));
    };
    add(canvas, 'pointermove', (e) => pointer('move', at(e)));
    add(canvas, 'pointerdown', (e) => {
      canvas.setPointerCapture(e.pointerId);
      this.keys.focus({ preventScroll: true });
      pointer('move', at(e));
      pointer('down', at(e), e.button, 0, 0, e.pointerType === 'touch' ? 1 : 0);
      e.preventDefault();
    });
    add(canvas, 'pointerup', (e) => pointer('up', at(e), e.button));
    add(canvas, 'pointercancel', (e) => pointer('up', at(e), e.button));
    add(canvas, 'pointerleave', () => pointer('leave', { x: 0, y: 0 }));
    add(
      canvas,
      'wheel',
      (e) => {
        e.preventDefault();
        const unit = e.deltaMode === 1 ? 1 / 3 : e.deltaMode === 2 ? 1 : 1 / 100;
        pointer('move', at(e));
        pointer('wheel', at(e), 0, -e.deltaX * unit, -e.deltaY * unit);
      },
      { passive: false },
    );
    // The keyboard belongs to the hidden text area: a click must not move
    // the focus to the canvas (mousedown's default), and a canvas reached
    // with Tab hands it on.
    add(canvas, 'mousedown', (e) => {
      e.preventDefault();
      this.keys.focus({ preventScroll: true });
    });
    add(canvas, 'focus', () => this.keys.focus({ preventScroll: true }));
    add(canvas, 'contextmenu', (e) => e.preventDefault());
    add(canvas, 'dblclick', (e) => e.preventDefault());

    // The keyboard goes to a hidden text area: IME composition, and the
    // page's paste event (ImGui reads its clipboard synchronously).
    const keys = document.createElement('textarea');
    keys.className = 'nymph-editor-keys';
    keys.setAttribute('aria-hidden', 'true');
    keys.autocapitalize = 'off';
    keys.autocomplete = 'off';
    keys.spellcheck = false;
    keys.style.cssText = 'position:absolute;left:0;top:0;width:1px;height:1px;opacity:0;border:0;padding:0;resize:none;pointer-events:none;';
    this.root.appendChild(keys);
    this.keys = keys;
    const mods = (e) => (e.ctrlKey || e.metaKey ? 1 : 0) | (e.shiftKey ? 2 : 0) | (e.altKey ? 4 : 0);
    const plain = (e) => e.key.length === 1 && !e.ctrlKey && !e.metaKey && !e.altKey;
    add(keys, 'keydown', (e) => {
      if (e.isComposing) {
        return;
      }
      const used = this.core.withCStrings([e.key], (k) => this.m._nymph_editor_key(k, 1, e.repeat ? 1 : 0, mods(e)));
      this.schedule();
      // Printable keys arrive as text (input event); Ctrl+V needs the paste
      // event; anything else the editor took keeps the browser's default
      // (Ctrl+D is not a bookmark here).
      const paste = (e.ctrlKey || e.metaKey) && (e.key === 'v' || e.key === 'V');
      if (used === 1 && !plain(e) && !paste) {
        e.preventDefault();
      }
      if (plain(e) && this.m._nymph_editor_wants_keyboard() !== 1) {
        e.preventDefault(); // nothing types: no stray text in the area
      }
    });
    add(keys, 'keyup', (e) => {
      this.core.withCStrings([e.key], (k) => this.m._nymph_editor_key(k, 0, 0, mods(e)));
      this.schedule();
    });
    add(keys, 'input', (e) => {
      if (e.isComposing) {
        return;
      }
      if (keys.value) {
        this.core.withCStrings([keys.value], (t) => this.m._nymph_editor_text(t));
        keys.value = '';
        this.schedule();
      }
    });
    add(keys, 'compositionend', () => {
      if (keys.value) {
        this.core.withCStrings([keys.value], (t) => this.m._nymph_editor_text(t));
        keys.value = '';
        this.schedule();
      }
    });
    add(keys, 'paste', (e) => {
      e.preventDefault();
      const text = e.clipboardData?.getData('text') ?? '';
      this.core.withCStrings([text], (t) => this.m._nymph_editor_set_clipboard(t));
      this.schedule();
    });
    add(keys, 'copy', (e) => e.preventDefault());
    add(keys, 'cut', (e) => e.preventDefault());
  }

  /** @private The text area follows the keyboard's owner (phones, IME). */
  syncTextInput() {
    if (!this.keys) {
      return;
    }
    const typing = this.m._nymph_editor_wants_keyboard() === 1;
    this.keys.inputMode = typing ? 'text' : 'none';
  }

  /** Gives the editor the keyboard. */
  focus() {
    this.keys?.focus({ preventScroll: true });
  }

  /** Frees the editor, its canvas and its WebGL context. */
  destroy() {
    if (this.destroyed) {
      return;
    }
    this.destroyed = true;
    this.observer?.disconnect();
    this.removers?.forEach((r) => r());
    this.m._nymph_editor_destroy();
    this.root?.remove();
    this.m.nymphEditorEvent = null;
  }
}

/**
 * Creates an editor in `container` (which it fills).
 *
 * @param {HTMLElement|null} container null with `headless`
 * @param {object} options
 * @param {string} [options.source] YAML source
 * @param {Uint8Array} [options.png] or a nymph PNG
 * @param {string} [options.fileName] shown in the panel's dialogs
 * @param {string} [options.theme] "source", "light", "dark", "classic"
 * @param {boolean} [options.panel] the editor panel (default true)
 * @param {boolean} [options.sourcePane] show the YAML source pane (default false)
 * @param {boolean} [options.repair] open a source that does not bind in repair
 *        state (the source pane at the error) instead of rejecting
 * @param {boolean} [options.headless] no canvas or GL: frames run, nothing is drawn (tests)
 * @param {string|URL} [options.wasmUrl] default: nymph-editor-core.wasm next to this file
 * @param {string|URL} [options.fontBaseUrl] default: ../fonts/
 * @returns {Promise<NymphEditor>} rejects with NymphError (no WebGL2, a source that does not bind)
 */
export async function createEditor(container, options = {}) {
  const core = await loadCore(options);
  const editor = new NymphEditor(core, container, options);
  try {
    editor.init();
    if (options.source !== undefined || options.png !== undefined) {
      await editor.open(options.png ?? options.source);
    }
    if (options.sourcePane || editor.repairing()) {
      editor.showSource(true);
    }
  } catch (e) {
    editor.destroy();
    throw e;
  }
  return editor;
}
