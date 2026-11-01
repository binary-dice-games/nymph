// MIT License © 2026 Binary Dice Games
// Makes the screenshots and animations of the web app's tutorial page
// (web/app/tutorial.html) by driving the built site in Chromium through
// Playwright, as the browser tests do (after inksplat's tutorial maker).
//
//   node web/tests/tutorial/media.mjs <build>/web <nymph source dir> [name...]
//
// With names (`view`, `edit-text`...), only those pictures are made
// (`save-menu` makes the share menu's too). Needs
// Playwright for Node and ffmpeg on the PATH. Not part of the test suite: run
// it by hand after a change to the look of the app and commit the files it
// writes to web/app/tutorial/. A visible cursor and the keys pressed are drawn
// by the page itself (OVERLAY), since a headless browser shows neither.

import { execFileSync, execSync } from 'node:child_process';
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { serve } from '../browser/serve.mjs';

const [webDir, sourceDir, ...only] = process.argv.slice(2);
if (!webDir || !sourceDir) {
  console.error('usage: node media.mjs <build>/web <nymph source dir> [name...]');
  process.exit(2);
}
const OUT = join(sourceDir, 'web', 'app', 'tutorial');
const VIEW = { width: 1280, height: 720 };
/** The longest time, in seconds, a GIF shows one frame before its last. */
const MAX_STILL = 1.4;

async function loadPlaywright() {
  try {
    return await import('playwright');
  } catch {
    const root = execSync('npm root -g', { encoding: 'utf8' }).trim();
    return createRequire(join(root, 'noop.js'))('playwright');
  }
}
const { chromium } = await loadPlaywright();

// ── The overlay: a cursor and the keys pressed ───────────────────────────────

/**
 * Drawn over the page: a mouse pointer that follows the mouse (with a ring
 * while pressed) and a badge with the keys pressed (`tutorialKeys`).
 */
const OVERLAY = `(() => {
  const css = \`
    #tutorial-cursor { position: fixed; left: 0; top: 0; z-index: 2147483647; pointer-events: none; width: 0; height: 0; }
    #tutorial-cursor svg { position: absolute; left: -2px; top: -1px; width: 24px; height: 24px; filter: drop-shadow(0 1px 2px rgba(0,0,0,.4)); }
    #tutorial-cursor .ring { position: absolute; left: -16px; top: -16px; width: 32px; height: 32px; border-radius: 50%;
      background: rgba(15, 159, 134, .35); border: 2px solid rgba(15, 159, 134, .9); transform: scale(.3); opacity: 0; transition: transform 120ms, opacity 120ms; }
    #tutorial-cursor.down .ring { transform: scale(1); opacity: 1; }
    #tutorial-keys { position: fixed; z-index: 2147483646; pointer-events: none; display: flex; gap: 6px; font: 700 18px system-ui, sans-serif; }
    #tutorial-keys kbd { min-width: 34px; padding: 6px 12px; border-radius: 8px; background: #1c2024; color: #fff; text-align: center;
      box-shadow: 0 3px 0 #000, 0 6px 16px rgba(0,0,0,.3); font: inherit; }
    #tutorial-keys span { align-self: center; color: #1c2024; text-shadow: 0 0 3px #fff, 0 0 6px #fff; }
  \`;
  const make = () => {
    if (document.getElementById('tutorial-cursor')) return;
    const style = document.createElement('style');
    style.textContent = css;
    const cursor = document.createElement('div');
    cursor.id = 'tutorial-cursor';
    cursor.innerHTML = '<div class="ring"></div><svg viewBox="0 0 24 24"><path d="M3 2l16 9-7 1.6L8.6 20z" fill="#111" stroke="#fff" stroke-width="1.6" stroke-linejoin="round"/></svg>';
    const keys = document.createElement('div');
    keys.id = 'tutorial-keys';
    document.documentElement.append(style, cursor, keys);
    const at = (e) => (cursor.style.transform = 'translate(' + e.clientX + 'px,' + e.clientY + 'px)');
    addEventListener('pointermove', at, true);
    addEventListener('pointerdown', (e) => { at(e); cursor.classList.add('down'); }, true);
    addEventListener('pointerup', () => cursor.classList.remove('down'), true);
    window.tutorialKeys = (text, x, y) => {
      keys.style.left = x + 'px';
      keys.style.top = y + 'px';
      keys.replaceChildren(...text.split(/(\\+)/).filter(Boolean).map((part) => {
        const el = document.createElement(part === '+' ? 'span' : 'kbd');
        el.textContent = part;
        return el;
      }));
    };
  };
  if (document.readyState === 'loading') addEventListener('DOMContentLoaded', make);
  else make();
})();`;

// ── Recording ────────────────────────────────────────────────────────────────

/** The recording of each page that has one. */
const recordings = new WeakMap();

/** Records what `page` shows (Chrome's screencast) and writes it as an animated GIF. */
class Recording {
  constructor(page, cdp) {
    this.page = page;
    this.cdp = cdp;
    this.frames = [];
  }

  static async start(page) {
    const cdp = await page.context().newCDPSession(page);
    const rec = new Recording(page, cdp);
    cdp.on('Page.screencastFrame', (f) => {
      rec.frames.push({ data: Buffer.from(f.data, 'base64'), t: f.metadata.timestamp ?? 0 });
      cdp.send('Page.screencastFrameAck', { sessionId: f.sessionId }).catch(() => {});
    });
    await cdp.send('Page.startScreencast', { format: 'png', maxWidth: VIEW.width, maxHeight: VIEW.height });
    await page.waitForTimeout(400);
    recordings.set(page, rec);
    return rec;
  }

  /**
   * Stops and writes `<name>.gif`: `crop` of the page ({x, y, width,
   * height}), scaled by `scale`, at 10 frames a second, holding the last
   * frame for `hold` seconds before it loops.
   */
  async save(name, crop, { scale = 1, hold = 2 } = {}) {
    await this.page.waitForTimeout(500);
    recordings.delete(this.page);
    await this.cdp.send('Page.stopScreencast');
    await this.cdp.detach();
    const work = mkdtempSync(join(tmpdir(), 'nymph-gif-'));
    try {
      const list = [];
      this.frames.forEach((f, i) => {
        const file = join(work, `${String(i).padStart(5, '0')}.png`);
        writeFileSync(file, f.data);
        const next = this.frames[i + 1];
        // Pauses longer than MAX_STILL are the script looking for things: shortened.
        const duration = next ? Math.min(MAX_STILL, Math.max(0.01, next.t - f.t)) : hold;
        list.push(`file '${file}'`, `duration ${duration.toFixed(3)}`);
      });
      // The concat demuxer needs the last file again for its duration to count.
      list.push(list[list.length - 2]);
      writeFileSync(join(work, 'list.txt'), list.join('\n'));
      const w = Math.round(crop.width * scale);
      const h = Math.round(crop.height * scale);
      const filter =
        `crop=${crop.width}:${crop.height}:${crop.x}:${crop.y},fps=10,` +
        `scale=${w}:${h}:flags=lanczos,split[a][b];` +
        '[a]palettegen=max_colors=128:stats_mode=diff[p];' +
        '[b][p]paletteuse=dither=none:diff_mode=rectangle';
      execFileSync('ffmpeg', ['-y', '-loglevel', 'error', '-f', 'concat', '-safe', '0', '-i', join(work, 'list.txt'),
        '-filter_complex', filter, '-loop', '0', join(OUT, `${name}.gif`)]);
    } finally {
      rmSync(work, { recursive: true, force: true });
    }
    console.log(`wrote ${name}.gif (${this.frames.length} frames)`);
  }
}

/** Writes `<name>.png`: `clip` of the page. */
async function still(page, name, clip) {
  await page.screenshot({ path: join(OUT, `${name}.png`), clip });
  console.log(`wrote ${name}.png`);
}

// ── Mouse and keys ───────────────────────────────────────────────────────────

/** Where the mouse of each page is, so it can glide from there. */
const mice = new WeakMap();

/** Moves the mouse along a straight line, slowly enough to be seen. */
async function glide(page, to, steps = 18) {
  const start = mice.get(page) ?? to;
  for (let i = 1; i <= steps; i++) {
    const at = [start[0] + ((to[0] - start[0]) * i) / steps, start[1] + ((to[1] - start[1]) * i) / steps];
    await page.mouse.move(...at);
    mice.set(page, at);
    await page.waitForTimeout(16);
  }
}

/** Waits until the chart is drawn (viewer drawn, or editor idle). */
async function shown(page) {
  await page.waitForFunction(
    () => {
      const a = window.nymphApp?.app;
      if (!a?.doc) {
        return !!a;
      }
      if (a.mode === 'edit') {
        return a.editor && a.editor.m._nymph_editor_wants_frame() === 0;
      }
      return a.viewer && !a.viewer.wantsFrame?.();
    },
    null,
    { timeout: 60000 },
  );
}

/** Lets the editor draw what the last input changed. */
async function settle(page, ms = 250) {
  await page.waitForTimeout(ms);
  await shown(page);
}

/**
 * Glides to `at` and clicks, with the button held over a few frames (the
 * editor is Dear ImGui: a press and a release in one frame can be missed).
 */
async function click(page, at, { button = 'left', double = false } = {}) {
  await glide(page, at);
  await page.waitForTimeout(150);
  for (let n = double ? 2 : 1; n > 0; n--) {
    await page.mouse.down({ button });
    await page.waitForTimeout(90);
    await page.mouse.up({ button });
    await page.waitForTimeout(60);
  }
  await settle(page);
}

/** Drags the mouse through `points` (pressed from the first to the last). */
async function drag(page, points, steps = 10) {
  await glide(page, points[0]);
  await page.mouse.down();
  await page.waitForTimeout(80);
  for (const p of points.slice(1)) {
    await glide(page, p, steps);
  }
  await page.mouse.up();
  await settle(page);
}

/** Shows `text` as keys on the page (`Ctrl+A`), or nothing for ''. */
const keys = (page, text, at) => page.evaluate(([t, x, y]) => window.tutorialKeys(t, x, y), [text, ...at]);

/** Presses a key, showing it on the page for a moment at `at`. */
async function press(page, key, at, label = key) {
  await keys(page, label, at);
  await page.keyboard.press(key);
  await page.waitForTimeout(450);
  await keys(page, '', at);
  await settle(page);
}

/** Types `text` at a reading pace. */
async function type(page, text) {
  for (const ch of text) {
    await page.keyboard.type(ch);
    await page.waitForTimeout(55);
  }
  await settle(page);
}

// ── Pages ────────────────────────────────────────────────────────────────────

const site = await serve([join(webDir, 'site')]);
const browser = await chromium.launch({ args: ['--enable-unsafe-swiftshader', '--use-angle=swiftshader'] });

/** A fresh page of the app at `path`, with the overlay, ready. */
async function app(path = '/') {
  const context = await browser.newContext({ viewport: VIEW, deviceScaleFactor: 1, serviceWorkers: 'block', colorScheme: 'light' });
  const page = await context.newPage();
  page.on('pageerror', (e) => console.error('[page]', e.message));
  await page.addInitScript(OVERLAY);
  await page.goto(`${site.base}${path}`);
  await page.waitForFunction(() => document.documentElement.dataset.ready === 'true', null, { timeout: 30000 });
  if (path !== '/') {
    await page.waitForFunction(() => !!window.nymphApp?.app?.doc, null, { timeout: 60000 });
    await shown(page);
  }
  await page.waitForTimeout(600);
  return page;
}

/**
 * Where element `path`'s `field` is drawn in the editor, in page pixels
 * ({x, y, width, height}, its centre in `cx`, `cy`).
 */
async function target(page, path, field = '') {
  const r = await page.evaluate(
    ([p, f]) => {
      const e = window.nymphApp.app.editor;
      const rect = e.targetRect(p, f);
      const fig = e.figureBox();
      const c = e.canvas.getBoundingClientRect();
      if (!rect || !fig) {
        return null;
      }
      // Target rects are in layout pixels, the y from the top of the figure area.
      const x = c.left + fig.left + rect.x * fig.scale;
      const y = c.top + fig.top + (rect.y - (fig.area?.top ?? 0)) * fig.scale;
      return { x, y, width: rect.width * fig.scale, height: rect.height * fig.scale };
    },
    [path, field],
  );
  if (!r) {
    throw new Error(`no target ${path} ${field}`);
  }
  return { ...r, cx: r.x + r.width / 2, cy: r.y + r.height / 2 };
}

/** The editor's side panel (its x, the y of its top). */
async function panel(page) {
  return page.evaluate(() => {
    const e = window.nymphApp.app.editor;
    const c = e.canvas.getBoundingClientRect();
    return { x: c.right - e.state().stripWidth, top: c.top, right: c.right };
  });
}

// ── The pictures ─────────────────────────────────────────────────────────────

const shots = new Map();
const shot = (name, fn) => shots.set(name, fn);

/** The part of the page with the chart of the `line` example in View. */
const VIEW_AREA = { x: 200, y: 56, width: 880, height: 480 };

shot('start', async () => {
  const page = await app('/');
  await still(page, 'start', { x: 0, y: 0, width: VIEW.width, height: VIEW.height });
  await page.context().close();
});

shot('new-chart', async () => {
  const page = await app('/');
  await page.click('#btn-new');
  await page.waitForSelector('dialog .tpl-card, dialog .card, dialog button', { timeout: 10000 });
  await page.waitForTimeout(1200);
  const box = await page.locator('dialog[open]').boundingBox();
  await still(page, 'new-chart', pad(box, 12));
  await page.context().close();
});

shot('view', async () => {
  const page = await app('/?example=line');
  const rec = await Recording.start(page);
  await glide(page, [640, 620]);
  // Hover: the values under the pointer.
  await glide(page, [600, 230], 30);
  await page.waitForTimeout(900);
  await glide(page, [740, 170], 20);
  await page.waitForTimeout(900);
  // Wheel: zoom in about the pointer.
  await glide(page, [700, 220], 10);
  for (let i = 0; i < 6; i++) {
    await page.mouse.wheel(0, -120);
    await page.waitForTimeout(120);
  }
  await settle(page, 600);
  // Drag: pan.
  await drag(page, [[700, 300], [560, 340]], 25);
  await page.waitForTimeout(700);
  // Double-click: fit again.
  await click(page, [640, 300], { double: true });
  await page.waitForTimeout(900);
  await rec.save('view', VIEW_AREA, { scale: 0.75 });
  await page.context().close();
});

shot('editor', async () => {
  const page = await app('/?example=dashboard&mode=edit');
  const t = await target(page, 'root/children/2/children/0/children/0', 'title');
  await click(page, [t.cx, t.cy]);
  await glide(page, [480, 690]);
  await settle(page, 500);
  await still(page, 'editor', { x: 0, y: 0, width: VIEW.width, height: VIEW.height });
  await page.context().close();
});

shot('edit-text', async () => {
  const page = await app('/?example=line&mode=edit');
  const rec = await Recording.start(page);
  await glide(page, [500, 620]);
  const t = await target(page, 'root', 'title');
  await click(page, [t.cx, t.cy]);
  await page.waitForTimeout(500);
  // The title's field, at the top of the panel's selection section.
  const p = await panel(page);
  const fieldY = await fieldRow(page, p.x);
  await click(page, [p.x + 120, fieldY]);
  await press(page, 'Control+A', [p.x + 40, fieldY + 40], 'Ctrl+A');
  await type(page, 'Load test, server 2');
  await press(page, 'Enter', [p.x + 40, fieldY + 40]);
  await glide(page, [600, 600]);
  await page.waitForTimeout(800);
  await rec.save('edit-text', { x: 0, y: 56, width: VIEW.width, height: 560 }, { scale: 0.7 });
  await page.context().close();
});

shot('style', async () => {
  const page = await app('/?example=bar_groups&mode=edit');
  const rec = await Recording.start(page);
  await glide(page, [500, 640]);
  const root = await target(page, 'root');
  await click(page, [root.x + root.width * 0.55, root.y + root.height * 0.45]);
  await page.waitForTimeout(500);
  // The floating toolbar under the selection: its colour scheme button.
  const bar = await toolbar(page, root);
  await click(page, bar.scheme);
  await page.waitForTimeout(500);
  const paired = await schemeRow(page, bar.scheme, 'Paired');
  await glide(page, paired, 20);
  await page.waitForTimeout(300);
  await click(page, paired);
  await page.waitForTimeout(400);
  await glide(page, [root.x + root.width + 40, root.y + root.height + 120]);
  await page.waitForTimeout(800);
  await rec.save('style', { x: 0, y: 56, width: 960, height: 600 }, { scale: 0.75 });
  await page.context().close();
});

shot('add-element', async () => {
  const page = await app('/?example=line&mode=edit');
  const rec = await Recording.start(page);
  await glide(page, [500, 640]);
  // The context menu opens at the press; its rows are 23 px apart under a title.
  const at = [600, 400];
  await click(page, at, { button: 'right' });
  await page.waitForTimeout(600);
  const below = [at[0] + 60, at[1] + 74];
  await glide(page, below);
  await settle(page, 600);
  // The submenu lists the element types the parent takes, by name, at the top of the page.
  const plot = submenuRow(below, 'Plot');
  await glide(page, [below[0] + 120, below[1]], 10);
  await glide(page, plot, 14);
  await page.waitForTimeout(400);
  await click(page, plot);
  await page.waitForTimeout(400);
  await glide(page, [920, 660]);
  await page.waitForTimeout(1200);
  await rec.save('add-element', { x: 0, y: 56, width: 960, height: 664 }, { scale: 0.7 });
  await page.context().close();
});

shot('data', async () => {
  const page = await app('/?example=line&mode=edit');
  const rec = await Recording.start(page);
  await glide(page, [500, 640]);
  await click(page, [87, 73]); // the data tab
  await page.waitForTimeout(900);
  // The grid: a row of column names, then one row of fields per data row, 35 px apart.
  const cell = [246, 183 + 35 * 3]; // cpu at minute 3 (88)
  await click(page, cell);
  await press(page, 'Control+A', [cell[0] + 80, cell[1] - 10], 'Ctrl+A');
  await type(page, '40');
  await press(page, 'Enter', [cell[0] + 80, cell[1] - 10]);
  await page.waitForTimeout(500);
  await click(page, [31, 73]); // the chart tab
  await glide(page, [600, 620]);
  await page.waitForTimeout(1500);
  await rec.save('data', { x: 0, y: 56, width: 960, height: 560 }, { scale: 0.75 });
  await page.context().close();
});

shot('theme', async () => {
  const page = await app('/?example=dashboard&mode=edit');
  await page.evaluate(() => {
    window.nymphApp.commands.call('app_choose_theme', {}, 'user');
  });
  await page.waitForSelector('dialog[open]');
  await page.waitForTimeout(1200);
  const box = await page.locator('dialog[open]').boundingBox();
  await still(page, 'theme', pad(box, 12));
  await page.context().close();
});

shot('import', async () => {
  const page = await app('/');
  const rec = await Recording.start(page);
  const chooser = page.waitForEvent('filechooser');
  await click(page, await centre(page, '#btn-import'));
  await (await chooser).setFiles({
    name: 'sales.csv',
    mimeType: 'text/csv',
    buffer: Buffer.from('month,online,stores\nJan,120,200\nFeb,150,190\nMar,180,185\nApr,240,170\nMay,260,160\nJun,310,150\n'),
  });
  await page.waitForSelector('dialog.wizard-dlg .kind-card');
  await page.waitForTimeout(1200);
  await click(page, await centre(page, 'dialog.wizard-dlg .kind-card:has-text("Line chart")'));
  await page.waitForTimeout(1200);
  await click(page, await centre(page, 'dialog.wizard-dlg button:has-text("Next")'));
  await page.waitForSelector('dialog.wizard-dlg .series-row');
  await page.waitForTimeout(1200);
  await click(page, await centre(page, 'dialog.wizard-dlg button:has-text("Next")'));
  await page.waitForTimeout(600);
  const title = 'dialog.wizard-dlg .field:has-text("Title") input';
  await click(page, await centre(page, title));
  await page.fill(title, '');
  await type(page, 'Online sales overtake stores');
  await page.waitForTimeout(1500);
  await click(page, await centre(page, 'dialog.wizard-dlg button:has-text("Create chart")'));
  await page.waitForSelector('dialog.wizard-dlg', { state: 'detached' });
  await shown(page);
  await page.waitForTimeout(1500);
  await rec.save('import', { x: 0, y: 0, width: VIEW.width, height: VIEW.height }, { scale: 0.6, hold: 2.5 });
  await page.context().close();
});

shot('save-menu', async () => {
  const page = await app('/?example=line');
  await page.click('#btn-save-menu');
  await page.waitForTimeout(500);
  await still(page, 'save-menu', menuClip(await page.locator('#btn-save-menu').boundingBox()));
  await page.keyboard.press('Escape');
  await page.click('#btn-share');
  await page.waitForTimeout(500);
  await still(page, 'share-menu', menuClip(await page.locator('#btn-share').boundingBox()));
  await page.context().close();
});

// ── Helpers that find things ────────────────────────────────────────────────

/** `box` grown by `n` pixels on each side, inside the page. */
function pad(box, n) {
  const x = Math.max(0, Math.floor(box.x - n));
  const y = Math.max(0, Math.floor(box.y - n));
  return { x, y, width: Math.min(VIEW.width - x, Math.ceil(box.width + 2 * n)), height: Math.min(VIEW.height - y, Math.ceil(box.height + 2 * n)) };
}

/** The part of the page under a top-bar button that holds its menu. */
function menuClip(button) {
  const width = 340;
  const x = Math.max(0, Math.min(VIEW.width - width, Math.round(button.x + button.width - width + 8)));
  return { x, y: 0, width, height: 250 };
}

/** The centre of the element `selector` matches. */
async function centre(page, selector) {
  const box = await page.locator(selector).first().boundingBox();
  return [box.x + box.width / 2, box.y + box.height / 2];
}

/**
 * The pixels of the page, to find what the editor drew: the recording's last
 * frame while one runs (a screenshot would stop the screencast), else a
 * screenshot.
 */
async function pixels(page) {
  const rec = recordings.get(page);
  let png;
  if (rec) {
    const count = rec.frames.length;
    for (let i = 0; i < 8 && rec.frames.length === count; i++) {
      await page.waitForTimeout(50);
    }
    png = rec.frames.at(-1).data;
  } else {
    png = await page.screenshot();
  }
  // ffmpeg decodes the PNG to RGBA bytes; its size is the page's (device scale 1).
  const data = execFileSync('ffmpeg', ['-loglevel', 'error', '-i', '-', '-f', 'rawvideo', '-pix_fmt', 'rgba', '-'], {
    input: png,
    maxBuffer: 64 << 20,
  });
  if (data.length !== VIEW.width * VIEW.height * 4) {
    throw new Error(`a frame of ${data.length} bytes, not the page's size`);
  }
  return { width: VIEW.width, height: VIEW.height, data };
}

/** The y of the first text field (a white row with a border) in the panel's selection section. */
async function fieldRow(page, panelX) {
  const img = await pixels(page);
  const x = panelX + 250;
  const at = (y) => (y * img.width + x) * 4;
  const white = (y) => img.data[at(y)] > 250 && img.data[at(y) + 1] > 250 && img.data[at(y) + 2] > 250;
  // Below the elements tree (it ends about 420 px down): the first white band 18 to 40 px high.
  for (let y = 430; y < img.height - 20; y++) {
    let n = 0;
    while (y + n < img.height && white(y + n)) {
      n++;
    }
    if (n >= 18 && n <= 40) {
      return y + n / 2;
    }
    y += n;
  }
  throw new Error('no text field in the panel');
}

/** The floating toolbar's colour scheme button, under (or over) `box`. */
async function toolbar(page, box) {
  const img = await pixels(page);
  // The scheme button is a strip of many bright colours: find it under (or over) the box.
  const many = (x, y) => {
    const seen = new Set();
    for (let i = 0; i < 28; i++) {
      const o = (y * img.width + x + i) * 4;
      const [r, g, b] = [img.data[o], img.data[o + 1], img.data[o + 2]];
      if (Math.max(r, g, b) - Math.min(r, g, b) > 60) {
        seen.add(`${r >> 5},${g >> 5},${b >> 5}`);
      }
    }
    return seen.size >= 5;
  };
  const ys = [];
  for (let y = Math.round(box.y + box.height); y < Math.min(img.height - 1, box.y + box.height + 90); y++) {
    ys.push(y);
  }
  for (let y = Math.round(box.y) - 1; y > Math.max(0, box.y - 90); y--) {
    ys.push(y);
  }
  for (const y of ys) {
    for (let x = Math.round(box.x); x < box.x + box.width - 30; x += 2) {
      if (many(x, y)) {
        return { scheme: [x + 14, y] };
      }
    }
  }
  throw new Error('no toolbar');
}

/** The rows of many colours (colour scheme swatches) between x0 and x0 + 120, as their centres. */
function swatchRows(img, x0) {
  const rows = [];
  let run = null;
  for (let y = 60; y < img.height; y++) {
    const seen = new Set();
    for (let x = x0; x < x0 + 120; x += 3) {
      const o = (y * img.width + x) * 4;
      seen.add(`${img.data[o] >> 5},${img.data[o + 1] >> 5},${img.data[o + 2] >> 5}`);
    }
    if (seen.size >= 7) {
      run = run ?? { from: y };
      run.to = y;
    } else if (run) {
      rows.push((run.from + run.to) / 2);
      run = null;
    }
  }
  return rows;
}

/**
 * The centre of the row `label` of the colour scheme list opened from the
 * toolbar button at `opener`: its swatch rows, in the order the editor lists
 * the schemes.
 */
async function schemeRow(page, opener, label) {
  const order = ['Default', 'Deep', 'Dark', 'Pastel', 'Paired', 'Viridis', 'Plasma', 'Hot', 'Cool', 'Pink', 'Jet'];
  const rows = swatchRows(await pixels(page), Math.round(opener[0] + 50)).filter((y) => Math.abs(y - opener[1]) > 12);
  // The list's first row: the first swatch row with another 25 px under it
  // (pale schemes may go unseen, so the rows are counted from there).
  const first = rows.find((y) => rows.some((z) => Math.abs(z - y - 25) <= 2));
  if (first === undefined) {
    throw new Error(`no colour scheme list (${rows.length} rows of colours)`);
  }
  return [opener[0] + 20, first + 25 * order.indexOf(label)];
}

/**
 * The row `label` of the element-type submenu opened from the context menu
 * row at `from`: the types in alphabetical order, 23 px apart, the list
 * starting near the top of the page (it is taller than the room below).
 */
function submenuRow(from, label) {
  const names = ['ActivityDiagram', 'ClassDiagram', 'ColorScale', 'ComponentDiagram', 'DataFlowDiagram', 'DeploymentDiagram',
    'ERDiagram', 'Flowchart', 'GanttChart', 'HorizontalLayout', 'Image', 'Label', 'MindMap', 'NetworkDiagram', 'ObjectDiagram',
    'OrgChart', 'PackageDiagram', 'Plot', 'Plot3D'];
  return [from[0] + 230, 78 + 23 * names.indexOf(label)];
}

// ── Run ──────────────────────────────────────────────────────────────────────

mkdirSync(OUT, { recursive: true });
let failed = 0;
for (const [name, fn] of shots) {
  if (only.length && !only.includes(name)) {
    continue;
  }
  try {
    await fn();
  } catch (e) {
    failed++;
    console.error(`FAIL ${name}: ${e.stack ?? e.message}`);
  }
}
await browser.close();
site.close();
process.exit(failed ? 1 : 0);
