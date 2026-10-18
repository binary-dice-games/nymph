// MIT License © 2026 Binary Dice Games
// spec/04_web_viewer plan steps 10-11: the viewer in Chromium, through
// Playwright (WebGL2 through SwiftShader when there is no GPU).
//
//   node run.mjs <build>/web <nymph source dir>
//
// Exits 77 (CTest's skip) when Playwright is not installed.

import { execSync } from 'node:child_process';
import { readFile } from 'node:fs/promises';
import { createServer } from 'node:http';
import { createRequire } from 'node:module';
import { extname, join, normalize } from 'node:path';

const [webDir, sourceDir] = process.argv.slice(2);
if (!webDir || !sourceDir) {
  console.error('usage: node run.mjs <build>/web <nymph source dir>');
  process.exit(2);
}

/** Playwright from the project, or the global install. */
async function loadPlaywright() {
  try {
    return await import('playwright');
  } catch {
    try {
      const root = execSync('npm root -g', { encoding: 'utf8' }).trim();
      return createRequire(join(root, 'noop.js'))('playwright');
    } catch {
      return null;
    }
  }
}

const playwright = await loadPlaywright();
if (!playwright) {
  console.log('Playwright is not installed: skipped');
  process.exit(77);
}

// ── A static server for the build output and the examples ────────────────────

const TYPES = {
  '.html': 'text/html',
  '.js': 'text/javascript',
  '.mjs': 'text/javascript',
  '.wasm': 'application/wasm',
  '.json': 'application/json',
  '.png': 'image/png',
  '.ttf': 'font/ttf',
  '.yaml': 'text/plain; charset=utf-8',
};
const TEST_PAGE = `<!doctype html><meta charset="utf-8"><body style="margin:0">
<script type="module">
  import * as nymph from '/nymph-viewer.js';
  window.nymph = nymph;
  window.nymphReady = true;
</script></body>`;

const server = createServer(async (req, res) => {
  const path = decodeURIComponent(new URL(req.url, 'http://x').pathname);
  if (path === '/test.html') {
    res.writeHead(200, { 'content-type': 'text/html' }).end(TEST_PAGE);
    return;
  }
  // The MCP App (spec/06_claude_integration): its page from the build and
  // the stand-in host from the sources.
  const routes = {
    '/claude/viewer.html': join(webDir, '..', 'claude', 'viewer.html'),
    '/mcp_host.html': join(sourceDir, 'web', 'tests', 'browser', 'mcp_host.html'),
  };
  if (routes[path]) {
    try {
      res.writeHead(200, { 'content-type': 'text/html; charset=utf-8' }).end(await readFile(routes[path]));
    } catch {
      res.writeHead(404).end('not found');
    }
    return;
  }
  const fromSource = path.startsWith('/examples/') || path.startsWith('/resources/');
  const file = fromSource ? join(sourceDir, normalize(path)) : join(webDir, normalize(path));
  try {
    const body = await readFile(file);
    res.writeHead(200, { 'content-type': TYPES[extname(file)] ?? 'application/octet-stream' }).end(body);
  } catch {
    res.writeHead(404).end('not found');
  }
});
await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
const base = `http://127.0.0.1:${server.address().port}`;

const browser = await playwright.chromium.launch({ args: ['--enable-unsafe-swiftshader', '--use-angle=swiftshader'] });

/** A fresh test page with the module loaded; `init` runs before any page script. */
async function openPage(init) {
  const page = await browser.newPage({ viewport: { width: 1000, height: 800 }, deviceScaleFactor: 1 });
  page.on('pageerror', (e) => console.log(`     page error: ${e.message}`));
  if (init) {
    await page.addInitScript(init);
  }
  await page.goto(`${base}/test.html`);
  await page.waitForFunction(() => window.nymphReady === true);
  return page;
}

// In-page helpers, installed on every test page.
const helpers = () => {
  /** A container of `width` CSS pixels, in the page. */
  window.box = (width) => {
    const div = document.createElement('div');
    div.style.width = `${width}px`;
    document.body.appendChild(div);
    return div;
  };
  /** The pixels of a viewer's canvas. */
  window.pixels = (viewer) => {
    const c = viewer.canvas;
    return { width: c.width, height: c.height, rgba: c.getContext('2d').getImageData(0, 0, c.width, c.height).data };
  };
  /**
   * WebGL in a browser against the software rasterizer. Looser than the GL
   * DESIGN tolerance (spec/02_opengl_rasterizer/DESIGN.md section 6), which
   * holds for nymph's own OpenGL rasterizer: ImGui's stock OpenGL3 backend
   * samples an RGBA32 atlas with the GPU's filtering (glyph edges differ by
   * up to 16), and through ANGLE a pixel centre lying exactly on a solid
   * edge may go to the other side (whole pixels flip along some edges).
   * Measured with SwiftShader (plan step 10): at least 99.68 % within 16,
   * at most 0.29 % beyond 64, mean at most 0.17.
   */
  window.compare = (a, b) => {
    if (a.width !== b.width || a.height !== b.height) {
      return { ok: false, why: `size ${a.width}x${a.height} vs ${b.width}x${b.height}` };
    }
    let within = 0, beyond = 0, sum = 0;
    const n = a.width * a.height;
    for (let i = 0; i < n; ++i) {
      let d = 0;
      for (let k = 0; k < 4; ++k) {
        const x = Math.abs(a.rgba[i * 4 + k] - b.rgba[i * 4 + k]);
        d = Math.max(d, x);
        sum += x;
      }
      within += d <= 16 ? 1 : 0;
      beyond += d > 64 ? 1 : 0;
    }
    const share = within / n, flipped = beyond / n, mean = sum / (n * 4);
    return { ok: share >= 0.995 && flipped <= 0.005 && mean < 0.5, share, flipped, mean };
  };
  /** Distinct colours in a canvas (a blank one has 1). */
  window.colours = (viewer) => {
    const { rgba } = window.pixels(viewer);
    const seen = new Set();
    for (let i = 0; i < rgba.length; i += 4 * 97) {
      seen.add((rgba[i] << 16) | (rgba[i + 1] << 8) | rgba[i + 2]);
    }
    return seen.size;
  };
};

// ── Test runner ──────────────────────────────────────────────────────────────

const tests = [];
const test = (name, fn) => tests.push({ name, fn });
const assert = (cond, message) => {
  if (!cond) {
    throw new Error(message);
  }
};

// ── Tests: the WebGL2 presenter (plan step 10) ───────────────────────────────

test('every example drawn with WebGL matches the software presenter', async () => {
  const page = await openPage(helpers);
  const list = (await import('node:fs')).readdirSync(join(sourceDir, 'examples')).filter((f) => f.endsWith('.yaml')).sort();
  for (const name of list) {
    const result = await page.evaluate(async (name) => {
      const source = await (await fetch(`/examples/${name}`)).text();
      const width = Number(/width:\s*(\d+)/.exec(source.split('\nroot:')[0])?.[1] ?? 800);
      const gl = await window.nymph.createViewer(window.box(width), { source, rasterizer: 'webgl' });
      const cpu = await window.nymph.createViewer(window.box(width), { source, rasterizer: 'software' });
      await Promise.all([gl.rendered(), cpu.rendered()]);
      const out = { gl: gl.info().rasterizer, cpu: cpu.info().rasterizer, ...window.compare(window.pixels(gl), window.pixels(cpu)) };
      gl.destroy();
      cpu.destroy();
      document.body.textContent = '';
      return out;
    }, name);
    assert(result.gl === 'webgl' && result.cpu === 'software', `${name}: rasterizers ${result.gl} / ${result.cpu}`);
    assert(result.ok, `${name}: ${result.why ?? `${(result.share * 100).toFixed(3)}% within 16, ${(result.flipped * 100).toFixed(3)}% beyond 64, mean ${result.mean?.toFixed(4)}`}`);
  }
  await page.close();
});

test('a plot of more than 65,535 vertices draws (32-bit indices)', async () => {
  const page = await openPage(helpers);
  const result = await page.evaluate(async () => {
    const ys = Array.from({ length: 40000 }, (_, i) => Math.sin(i / 300) * 10 + (i % 7));
    const source = `image: {width: 600, height: 300}\nroot: {type: Plot, children: [{type: PlotLine, label: big, ys: [${ys.join(',')}]}]}\n`;
    const gl = await window.nymph.createViewer(window.box(600), { source, rasterizer: 'webgl' });
    const cpu = await window.nymph.createViewer(window.box(600), { source, rasterizer: 'software' });
    await Promise.all([gl.rendered(), cpu.rendered()]);
    return { colours: window.colours(gl), ...window.compare(window.pixels(gl), window.pixels(cpu)) };
  });
  assert(result.colours > 3, 'the WebGL canvas is drawn');
  assert(result.ok, `${(result.share * 100).toFixed(3)}% within 16, ${(result.flipped * 100).toFixed(3)}% beyond 64, mean ${result.mean?.toFixed(4)}`);
  await page.close();
});

test('twenty viewers share one WebGL context', async () => {
  const page = await openPage(() => {
    window.webglContexts = 0;
    const getContext = HTMLCanvasElement.prototype.getContext;
    HTMLCanvasElement.prototype.getContext = function (type, ...rest) {
      if (type === 'webgl2' || type === 'webgl') {
        ++window.webglContexts;
      }
      return getContext.call(this, type, ...rest);
    };
  });
  await page.evaluate(helpers);
  const result = await page.evaluate(async () => {
    const viewers = [];
    for (let i = 0; i < 20; ++i) {
      viewers.push(await window.nymph.createViewer(window.box(300), {
        source: `image: {width: 300, height: 150}\nroot: {type: Plot, title: P${i}, children: [{type: PlotBars, label: b, ys: [${i}, 2, 3]}]}\n`,
      }));
    }
    await Promise.all(viewers.map((v) => v.rendered()));
    return { contexts: window.webglContexts, webgl: viewers.every((v) => v.info().rasterizer === 'webgl'), drawn: viewers.every((v) => window.colours(v) > 3) };
  });
  assert(result.contexts === 1, `${result.contexts} WebGL contexts`);
  assert(result.webgl, 'every viewer draws with WebGL');
  assert(result.drawn, 'every viewer is drawn');
  await page.close();
});

test('the software rasterizer is used when asked for or without WebGL2', async () => {
  const page = await openPage(() => {
    const getContext = HTMLCanvasElement.prototype.getContext;
    HTMLCanvasElement.prototype.getContext = function (type, ...rest) {
      return type === 'webgl2' ? null : getContext.call(this, type, ...rest);
    };
  });
  await page.evaluate(helpers);
  const result = await page.evaluate(async () => {
    const v = await window.nymph.createViewer(window.box(400), { source: 'image: {width: 400, height: 200}\nroot: {type: Label, text: no GPU here}\n' });
    await v.rendered();
    return { rasterizer: v.info().rasterizer, colours: window.colours(v) };
  });
  assert(result.rasterizer === 'software', result.rasterizer);
  assert(result.colours > 1, 'drawn');
  await page.close();
});

test('a lost WebGL context moves the viewers to the CPU', async () => {
  const page = await openPage(helpers);
  const result = await page.evaluate(async () => {
    const v = await window.nymph.createViewer(window.box(400), { source: 'image: {width: 400, height: 200}\nroot: {type: Plot, children: [{type: PlotLine, label: a, ys: [1, 3, 2]}]}\n' });
    await v.rendered();
    const before = v.info().rasterizer;
    const canvas = v.core.glCanvas;
    canvas.getContext('webgl2').getExtension('WEBGL_lose_context').loseContext();
    await new Promise((r) => setTimeout(r, 100));
    await v.rendered();
    return { before, after: v.info().rasterizer, colours: window.colours(v) };
  });
  assert(result.before === 'webgl' && result.after === 'software', `${result.before} -> ${result.after}`);
  assert(result.colours > 3, 'drawn again');
  await page.close();
});

test('the demo page shows every example', async () => {
  const page = await browser.newPage({ viewport: { width: 1200, height: 900 } });
  const errors = [];
  page.on('pageerror', (e) => errors.push(e.message));
  await page.goto(`${base}/demo/index.html`);
  await page.waitForFunction(() => document.querySelectorAll('.live canvas').length >= 20, null, { timeout: 60000 });
  assert(errors.length === 0, errors.join('; '));
  await page.close();
});

// ── Tests: input, scheduling and the element (plan step 11) ──────────────────

const PLOT = 'image: {width: 400, height: 300}\nroot:\n  type: Plot\n  title: P\n  x_min: 0\n  x_max: 4\n  y_min: 0\n  y_max: 10\n' +
  '  children:\n    - {type: PlotLine, label: temp, xs: [0, 1, 2, 3, 4], ys: [1, 4, 7, 2, 5]}\n';

/** In-page: a viewer of PLOT at the top of the page, and helpers to find points on it. */
const plotPage = async (page, source = PLOT, options = {}) => {
  await page.evaluate(helpers);
  await page.evaluate(async ({ source, options }) => {
    const v = await window.nymph.createViewer(window.box(400), { source, ...options });
    await v.rendered();
    window.v = v;
    window.events = [];
    for (const e of ['hover', 'click', 'view']) {
      v.on(e, (detail) => window.events.push({ e, detail }));
    }
    /** Page coordinates of data point (x, y) of plot `i`. */
    window.at = (x, y, i = 0) => {
      const p = window.v.state().views[i];
      const box = window.v.canvas.getBoundingClientRect();
      return {
        x: box.left + p.x + ((x - p.xMin) / (p.xMax - p.xMin)) * p.width,
        y: box.top + p.y + ((p.yMax - y) / (p.yMax - p.yMin)) * p.height,
      };
    };
  }, { source, options });
};
const settleIn = (page) => page.evaluate(() => new Promise((r) => setTimeout(r, 150)).then(() => window.v.rendered()));

test('dragging a plot pans it and fires view', async () => {
  const page = await openPage();
  await plotPage(page);
  const a = await page.evaluate(() => window.at(2, 5));
  await page.mouse.move(a.x, a.y);
  await page.mouse.down();
  await page.mouse.move(a.x - 50, a.y, { steps: 5 });
  await page.mouse.up();
  await settleIn(page);
  const views = await page.evaluate(() => window.events.filter((x) => x.e === 'view').map((x) => x.detail));
  assert(views.length >= 1, 'a view event');
  const last = views[views.length - 1][0];
  assert(last.xMin > 0.3 && Math.abs(last.xMax - last.xMin - 4) < 1e-6, `x range ${last.xMin} to ${last.xMax}`);
  await page.close();
});

test('the wheel zooms a plot without scrolling the page, and scrolls it elsewhere', async () => {
  const page = await openPage();
  await plotPage(
    page,
    'image: {width: 400, height: 300}\nroot:\n  type: VerticalLayout\n  children:\n    - {type: Label, text: A heading over the plot}\n' +
      '    - {type: Plot, title: P, x_min: 0, x_max: 4, y_min: 0, y_max: 10, height: -1, children: [{type: PlotLine, label: a, ys: [1, 2]}]}\n',
  );
  await page.evaluate(() => {
    const spacer = document.createElement('div');
    spacer.style.height = '3000px';
    document.body.appendChild(spacer);
  });
  const p = await page.evaluate(() => window.at(2, 5));
  await page.mouse.move(p.x, p.y);
  await page.mouse.wheel(0, -120);
  await settleIn(page);
  const zoomed = await page.evaluate(() => ({ scroll: window.scrollY, range: window.v.state().views[0].xMax - window.v.state().views[0].xMin }));
  assert(zoomed.scroll === 0, `the page scrolled to ${zoomed.scroll}`);
  assert(zoomed.range < 4, `x range ${zoomed.range}`);
  const label = await page.evaluate(() => {
    const box = window.v.canvas.getBoundingClientRect();
    return { x: box.left + 200, y: box.top + 15 };
  });
  await page.mouse.move(label.x, label.y);
  await page.mouse.wheel(0, 200);
  await page.waitForFunction(() => window.scrollY > 0, null, { timeout: 5000 });
  await page.close();
});

test('clicking a point fires click with its pick', async () => {
  const page = await openPage();
  await plotPage(page);
  const p = await page.evaluate(() => window.at(2, 7));
  await page.mouse.move(p.x, p.y);
  await settleIn(page);
  await page.mouse.down();
  await page.waitForTimeout(50);
  await page.mouse.up();
  await settleIn(page);
  const clicks = await page.evaluate(() => window.events.filter((e) => e.e === 'click').map((e) => e.detail));
  assert(clicks.length === 1, `${clicks.length} click events`);
  assert(clicks[0].element === 'root/children/0' && clicks[0].index === 2 && clicks[0].values.y === '7', JSON.stringify(clicks[0]));
  await page.close();
});

test('hovering a point fires hover and draws the tooltip', async () => {
  const page = await openPage();
  await plotPage(page);
  const before = await page.evaluate(() => Array.from(window.pixels(window.v).rgba));
  const p = await page.evaluate(() => window.at(2, 7));
  await page.mouse.move(p.x, p.y);
  await settleIn(page);
  const result = await page.evaluate((before) => {
    const hovers = window.events.filter((x) => x.e === 'hover').map((x) => x.detail);
    const now = window.pixels(window.v).rgba;
    let changed = 0;
    for (let i = 0; i < now.length; ++i) changed += now[i] !== before[i] ? 1 : 0;
    return { hover: hovers[hovers.length - 1], changed };
  }, before);
  assert(result.hover?.label === 'temp' && result.hover.values.y === '7', JSON.stringify(result.hover));
  assert(result.changed > 100, `${result.changed} pixel channels changed`);
  await page.close();
});

test('an idle viewer asks for no animation frames', async () => {
  const page = await openPage(() => {
    window.frames = 0;
    const raf = window.requestAnimationFrame.bind(window);
    window.requestAnimationFrame = (fn) => {
      ++window.frames;
      return raf(fn);
    };
  });
  await plotPage(page);
  const p = await page.evaluate(() => window.at(1, 4));
  await page.mouse.move(p.x, p.y);
  await settleIn(page);
  const before = await page.evaluate(() => window.frames);
  await page.waitForTimeout(1000);
  const after = await page.evaluate(() => window.frames);
  assert(after === before, `${after - before} frames asked for while idle`);
  await page.close();
});

test('<nymph-viewer> shows YAML, nymph PNGs, inline YAML, plain PNGs and errors', async () => {
  const page = await openPage(helpers);
  const result = await page.evaluate(async () => {
    const show = (html) =>
      new Promise((resolve) => {
        const holder = document.createElement('div');
        holder.style.width = '500px';
        holder.innerHTML = html;
        const el = holder.firstElementChild;
        el.addEventListener('nymph-load', (e) => resolve({ ok: true, info: e.detail, el }));
        el.addEventListener('nymph-error', (e) => resolve({ ok: false, text: el.shadowRoot.textContent, line: e.detail.line }));
        document.body.appendChild(holder);
      });
    const yaml = await show('<nymph-viewer src="/examples/line.yaml"></nymph-viewer>');
    const png = await show('<nymph-viewer src="/examples/line.png"></nymph-viewer>');
    const inline = await show('<nymph-viewer>\n    root:\n      type: Label\n      text: inline\n</nymph-viewer>');
    const plain = await show('<nymph-viewer src="/resources/Nymph.png"></nymph-viewer>');
    const bad = await show('<nymph-viewer>root: {type: Plot, children: [{type: PlotLyne}]}</nymph-viewer>');
    return {
      yaml: yaml.ok && yaml.info.hasFigure && window.colours(yaml.el.viewer) > 3,
      png: png.ok && png.el.viewer.source().includes('Load test'),
      inline: inline.ok && inline.info.hasFigure,
      plain: plain.ok && !plain.info.hasFigure && plain.el.viewer.canvas.width > 0,
      bad: !bad.ok && bad.line === 1 && bad.text.includes("unknown element type 'PlotLyne'"),
    };
  });
  for (const [name, ok] of Object.entries(result)) {
    assert(ok, `${name} failed`);
  }
  await page.close();
});

test('toPng carries the source on screen, data included', async () => {
  const page = await openPage();
  await plotPage(page, 'image: {width: 400, height: 300}\nroot: {type: Plot, children: [{type: PlotLine, label: a, ys: $v}]}\ndata: {v: [1, 2]}\n');
  const ok = await page.evaluate(async () => {
    await window.v.setData({ v: [9, 8, 7] });
    const png = window.v.toPng();
    const signature = [0x89, 0x50, 0x4e, 0x47].every((b, i) => png[i] === b);
    const text = new TextDecoder().decode(png); // the source chunk is UTF-8
    return signature && text.includes(window.v.source()) && window.v.source().includes('[9,8,7]');
  });
  assert(ok, 'the PNG carries the source with the new data');
  await page.close();
});

test('theme auto follows the colour scheme', async () => {
  const page = await openPage();
  await page.emulateMedia({ colorScheme: 'light' });
  await plotPage(page, PLOT, { theme: 'auto' });
  const corner = () => page.evaluate(() => Array.from(window.pixels(window.v).rgba.slice(0, 3)));
  const light = await corner();
  await page.emulateMedia({ colorScheme: 'dark' });
  await settleIn(page);
  const dark = await corner();
  assert(light[0] > 200 && dark[0] < 80, `corner ${light} then ${dark}`);
  await page.close();
});

test('a fit="width" viewer lays out again when its box narrows', async () => {
  const page = await openPage();
  await plotPage(page, PLOT.replace('width: 400', 'width: 800'), { fit: 'width' });
  await page.evaluate(() => (window.v.container.style.width = '800px'));
  await settleIn(page);
  const wide = await page.evaluate(() => window.v.state().geometry.layoutWidth);
  await page.evaluate(() => (window.v.container.style.width = '400px'));
  await settleIn(page);
  const narrow = await page.evaluate(() => ({ g: window.v.state().geometry, css: window.v.canvas.style.width }));
  assert(wide === 800 && narrow.g.layoutWidth === 400 && narrow.css === '400px', `${wide} then ${JSON.stringify(narrow)}`);
  await page.close();
});

test('a fit="scale" viewer grows to its box and hovers the same point', async () => {
  const page = await openPage();
  await plotPage(page, PLOT, { fit: 'scale' });
  await page.evaluate(() => (window.v.container.style.width = '800px'));
  await settleIn(page);
  const g = await page.evaluate(() => ({ g: window.v.state().geometry, css: window.v.canvas.style.width, px: window.v.canvas.width }));
  assert(g.g.layoutWidth === 400 && g.g.cssScale === 2 && g.css === '800px' && g.px === 800, JSON.stringify(g));
  const p = await page.evaluate(() => window.at(2, 7));
  await page.mouse.move(p.x, p.y);
  await settleIn(page);
  const hover = await page.evaluate(() => window.events.filter((x) => x.e === 'hover').map((x) => x.detail).pop());
  assert(hover?.label === 'temp' && hover.values.y === '7', JSON.stringify(hover));
  await page.close();
});

test('fonts are fetched once per page, and a missing one keeps the chart', async () => {
  const page = await openPage(helpers);
  const requests = [];
  page.on('request', (r) => r.url().includes('/fonts/') && requests.push(r.url()));
  await page.evaluate(async () => {
    const source = 'image: {width: 300, height: 60}\nroot: {type: Label, text: mono, font: FiraMono-Regular.ttf}\n';
    const a = await window.nymph.createViewer(window.box(300), { source });
    const b = await window.nymph.createViewer(window.box(300), { source });
    await Promise.all([a.rendered(), b.rendered()]);
  });
  assert(requests.filter((u) => u.endsWith('/FiraMono-Regular.ttf')).length === 1, requests.join(', '));
  await page.close();

  const missing = await openPage(helpers);
  const result = await missing.evaluate(async () => {
    await window.nymph.loadNymph({ fontBaseUrl: '/missing/' }); // the first load decides
    const v = await window.nymph.createViewer(window.box(300), { source: 'root: {type: Label, text: kept}\n' });
    let error = null;
    await v.setSource('root: {type: Label, text: x, font: FiraSans-Bold.ttf}\n').catch((e) => (error = e));
    return { message: error?.message, kind: error?.kind, kept: v.source().includes('kept') };
  });
  assert(result.kind === 'fonts' && /404/.test(result.message) && /missing\/FiraSans-Bold.ttf/.test(result.message), result.message);
  assert(result.kept, 'the previous chart stays');
  await missing.close();
});

test('the single-file build works on its own in a page', async () => {
  const page = await browser.newPage();
  await page.route('**/single.html', (route) =>
    route.fulfill({
      contentType: 'text/html',
      body: `<!doctype html><body><div id="a" style="width:500px"></div><script type="module">
        import { createViewer } from '/nymph-viewer.single.js';
        const v = await createViewer(document.getElementById('a'), {
          source: 'image: {width: 500, height: 200}\\nroot: {type: Label, text: one file, font: FiraSans-Bold.ttf}\\n' });
        await v.rendered();
        window.result = { info: v.info(), width: v.canvas.width };
      </script>`,
    }),
  );
  await page.goto(`${base}/single.html`);
  await page.waitForFunction(() => window.result, null, { timeout: 30000 });
  const r = await page.evaluate(() => window.result);
  assert(r.info.hasFigure && r.info.rasterizer === 'webgl' && r.width === 500, JSON.stringify(r));
  await page.close();
});

// ── Tests: the menu (save, about) ────────────────────────────────────────────

/** Clicks menu item `label` and returns the download it starts. */
async function download(page, x, y, label) {
  await page.mouse.click(x, y, { button: 'right' });
  await page.waitForSelector('.nymph-menu');
  const [file] = await Promise.all([page.waitForEvent('download'), page.click(`.nymph-menu button:text-is("${label}")`)]);
  return { name: file.suggestedFilename(), bytes: await readFile(await file.path()) };
}

test('right-click saves PNG and YAML with the view on screen', async () => {
  const page = await openPage();
  await plotPage(page);
  const a = await page.evaluate(() => window.at(2, 5));
  await page.mouse.move(a.x, a.y);
  await page.mouse.down();
  await page.mouse.move(a.x - 50, a.y, { steps: 5 });
  await page.mouse.up();
  await settleIn(page);
  const items = await (async () => {
    await page.mouse.click(a.x, a.y, { button: 'right' });
    await page.waitForSelector('.nymph-menu');
    const labels = await page.$$eval('.nymph-menu button', (bs) => bs.map((b) => b.textContent));
    await page.keyboard.press('Escape');
    return labels;
  })();
  assert(items.join('|') === 'Save as PNG|Save as YAML|About nymph viewer', items.join('|'));
  assert((await page.$('.nymph-menu')) === null, 'Escape closes the menu');
  const yaml = await download(page, a.x, a.y, 'Save as YAML');
  assert(yaml.name === 'chart.yaml', yaml.name);
  assert(/\nview:\n  root: \{x: \[/.test(yaml.bytes.toString()), yaml.bytes.toString());
  const png = await download(page, a.x, a.y, 'Save as PNG');
  assert(png.name === 'chart.png' && png.bytes.subarray(1, 4).toString() === 'PNG', png.name);
  assert(png.bytes.includes('view:'), 'the PNG carries the YAML with its view');
  await page.close();
});

test('a right-drag box-zooms and opens no menu', async () => {
  const page = await openPage();
  await plotPage(page);
  const a = await page.evaluate(() => window.at(1, 8));
  const b = await page.evaluate(() => window.at(3, 2));
  await page.mouse.move(a.x, a.y);
  await page.mouse.down({ button: 'right' });
  await page.mouse.move(b.x, b.y, { steps: 6 });
  await page.mouse.up({ button: 'right' });
  await settleIn(page);
  const range = await page.evaluate(() => window.v.state().views[0]);
  assert((await page.$('.nymph-menu')) === null, 'no menu after a drag');
  assert(range.xMax - range.xMin < 3, `x range ${range.xMin} to ${range.xMax}`);
  await page.close();
});

test('About shows the version; menu: false turns the menu off', async () => {
  const page = await openPage();
  await plotPage(page);
  const a = await page.evaluate(() => window.at(2, 5));
  await page.mouse.click(a.x, a.y, { button: 'right' });
  await page.click('.nymph-menu button:text-is("About nymph viewer")');
  const text = await page.textContent('.nymph-about');
  assert(/nymph viewer/.test(text) && /Version \d+\.\d+\.\d+, drawn with WebGL2/.test(text), text);
  await page.keyboard.press('Escape');
  assert((await page.$('.nymph-about')) === null, 'Escape closes About');
  await page.evaluate(async () => {
    window.v.destroy();
    window.v = await window.nymph.createViewer(window.box(400), {
      source: 'image: {width: 400, height: 300}\nroot: {type: Label, text: no menu}\n',
      menu: false,
    });
    await window.v.rendered();
  });
  const box = await page.evaluate(() => {
    const r = window.v.canvas.getBoundingClientRect();
    return { x: r.left + 50, y: r.top + 20 };
  });
  await page.mouse.click(box.x, box.y, { button: 'right' });
  await page.waitForTimeout(100);
  assert((await page.$('.nymph-menu')) === null, 'no menu');
  await page.close();
});

test('the element saves under the name of its src', async () => {
  const page = await openPage();
  await page.evaluate(
    () =>
      new Promise((resolve) => {
        const holder = document.createElement('div');
        holder.style.width = '820px';
        holder.innerHTML = '<nymph-viewer src="/examples/line.yaml"></nymph-viewer>';
        holder.firstElementChild.addEventListener('nymph-load', resolve);
        document.body.appendChild(holder);
      }),
  );
  const at = await page.evaluate(() => {
    const r = document.querySelector('nymph-viewer').viewer.canvas.getBoundingClientRect();
    return { x: r.left + 200, y: r.top + 200 };
  });
  const file = await download(page, at.x, at.y, 'Save as YAML');
  assert(file.name === 'line.yaml', file.name);
  assert(file.bytes.toString() === (await readFile(join(sourceDir, 'examples', 'line.yaml'), 'utf8')), 'unchanged source');
  await page.close();
});

// ── Tests: the nymph MCP App (spec/06_claude_integration step 5) ─────────────

/** A tool result as nymph mcp's render_chart returns it, for an example. */
async function renderResult(name) {
  const source = await readFile(join(sourceDir, 'examples', `${name}.yaml`), 'utf8');
  const png = await readFile(join(sourceDir, 'examples', `${name}.png`));
  return {
    content: [
      { type: 'image', data: png.toString('base64'), mimeType: 'image/png' },
      { type: 'text', text: 'Rendered PNG' },
    ],
    structuredContent: { source, width: png.readUInt32BE(16), height: png.readUInt32BE(20) },
  };
}

/**
 * The host page with the App loaded and sent `result`; resolves once the App
 * shows a chart or gave up. `requests` collects the App frame's requests.
 */
async function openApp({ context = { theme: 'light', displayMode: 'inline', availableDisplayModes: ['inline', 'fullscreen'] }, result, failFonts = false }) {
  const page = await browser.newPage({ viewport: { width: 1000, height: 1300 }, deviceScaleFactor: 1 });
  page.on('pageerror', (e) => console.log(`     page error: ${e.message}`));
  await page.addInitScript(helpers);
  const requests = [];
  page.on('request', (r) => {
    if (r.frame() !== page.mainFrame()) {
      requests.push(r.url());
    }
  });
  await page.goto(`${base}/mcp_host.html`);
  await page.waitForFunction(() => window.hostReady === true);
  await page.evaluate((options) => window.hostStart(options), { context, result, failFonts });
  // The App frame is sandboxed (an opaque origin): reach it through Playwright.
  let frame = null;
  for (const until = Date.now() + 30000; !frame && Date.now() < until; ) {
    const child = page.frames().find((f) => f !== page.mainFrame());
    if (child && (await child.evaluate(() => window.nymphApp !== undefined).catch(() => false))) {
      frame = child;
    } else {
      await page.waitForTimeout(50);
    }
  }
  assert(frame, 'the App frame did not start');
  await frame.waitForFunction(() => window.nymphApp.error !== null || window.nymphApp.viewer !== null, null, { timeout: 30000 });
  if (await frame.evaluate(() => window.nymphApp.viewer !== null)) {
    await frame.evaluate(() => window.nymphApp.viewer.rendered());
  }
  return { page, frame, requests };
}

test('the MCP App draws render_chart\'s result like the PNG, with no network request', async () => {
  const { page, frame, requests } = await openApp({ result: await renderResult('line') });
  const out = await frame.evaluate(async (pngBase64) => {
    const viewer = window.nymphApp.viewer;
    const img = new Image();
    img.src = `data:image/png;base64,${pngBase64}`;
    await img.decode();
    const c = document.createElement('canvas');
    c.width = img.width;
    c.height = img.height;
    const ctx = c.getContext('2d');
    ctx.drawImage(img, 0, 0);
    const png = { width: c.width, height: c.height, rgba: ctx.getImageData(0, 0, c.width, c.height).data };
    return {
      rasterizer: viewer.info().rasterizer,
      colours: window.colours(viewer),
      height: document.documentElement.scrollHeight,
      ...window.compare(window.pixels(viewer), png),
    };
  }, (await renderResult('line')).content[0].data);
  assert(out.rasterizer === 'webgl', `rasterizer ${out.rasterizer}`);
  assert(out.colours > 10, `${out.colours} colours`);
  assert(out.ok, out.why ?? `${(out.share * 100).toFixed(3)}% within 16, ${(out.flipped * 100).toFixed(3)}% beyond 64`);
  const host = await page.evaluate(() => window.host);
  const init = host.messages.find((m) => m.method === 'ui/initialize');
  assert(init?.params?.protocolVersion === '2026-01-26', 'ui/initialize with the spec revision');
  assert(init.params.appCapabilities.availableDisplayModes.includes('fullscreen'), 'offers full screen');
  assert(host.messages.some((m) => m.method === 'ui/notifications/initialized'), 'initialized sent');
  const last = host.sizes.at(-1);
  assert(last && Math.abs(last.height - out.height) <= 1 && last.height >= 420, `size-changed ${JSON.stringify(last)} vs ${out.height}`);
  assert(requests.length === 0, `the App fetched ${requests.join(', ')}`);
  assert(host.toolCalls.length === 0, 'no font needed, no tool call');
  await page.close();
});

test('the MCP App gets a named font through viewer_font', async () => {
  const source =
    'image: {width: 400, height: 240, font: FiraSans-Regular.ttf}\n' +
    'root:\n  type: Plot\n  title: Fonts\n  children:\n    - {type: PlotLine, label: v, ys: [1, 3, 2]}\n';
  const { page, frame, requests } = await openApp({ result: { content: [], structuredContent: { source } } });
  const shown = await frame.evaluate(() => ({ error: window.nymphApp.error, colours: window.colours(window.nymphApp.viewer) }));
  assert(shown.error === null, `error: ${shown.error}`);
  assert(shown.colours > 5, 'drawn');
  const calls = await page.evaluate(() => window.host.toolCalls);
  assert(calls.length === 1 && calls[0].name === 'viewer_font' && calls[0].arguments.name === 'FiraSans-Regular.ttf', JSON.stringify(calls));
  assert(requests.length === 0, `the App fetched ${requests.join(', ')}`);
  await page.close();
});

test('the MCP App follows the host theme', async () => {
  const { page, frame } = await openApp({ result: await renderResult('line') });
  const corner = () => frame.evaluate(() => Array.from(window.pixels(window.nymphApp.viewer).rgba.slice(0, 3)));
  const light = await corner();
  await page.evaluate(() => window.hostSend({ method: 'ui/notifications/host-context-changed', params: { theme: 'dark' } }));
  await frame.waitForFunction(() => window.nymphApp.viewer.rendered().then(() => true));
  await frame.evaluate(() => window.nymphApp.viewer.rendered());
  const dark = await corner();
  assert(light[0] + light[1] + light[2] > 600, `light corner ${light}`);
  assert(dark[0] + dark[1] + dark[2] < 300, `dark corner ${dark}`);
  await page.close();

  const darkFirst = await openApp({ context: { theme: 'dark' }, result: await renderResult('line') });
  const first = await darkFirst.frame.evaluate(() => Array.from(window.pixels(window.nymphApp.viewer).rgba.slice(0, 3)));
  assert(first[0] + first[1] + first[2] < 300, `dark from the start ${first}`);
  await darkFirst.page.close();
});

test('the MCP App shows the PNG when the viewer cannot draw the chart', async () => {
  const result = await renderResult('fonts');
  const { page, frame } = await openApp({ result, failFonts: true });
  const out = await frame.evaluate(() => ({
    error: window.nymphApp.error,
    img: document.getElementById('fallback')?.src.slice(0, 22),
    message: document.getElementById('message').textContent,
  }));
  assert(out.error && out.error.includes('not embedded'), `error: ${out.error}`);
  assert(out.img === 'data:image/png;base64,', `fallback image: ${out.img}`);
  assert(out.message.includes('rendered image'), out.message);
  await page.close();
});

test('the MCP App asks the host for full screen', async () => {
  const { page, frame } = await openApp({ result: await renderResult('line') });
  const hidden = await frame.evaluate(() => document.getElementById('fullscreen').hidden);
  assert(!hidden, 'the button shows when the host offers full screen');
  await frame.click('#fullscreen');
  await frame.waitForFunction(() => window.nymphApp.displayMode === 'fullscreen');
  const asked = await page.evaluate(() => window.host.messages.find((m) => m.method === 'ui/request-display-mode')?.params);
  assert(asked?.mode === 'fullscreen', JSON.stringify(asked));
  await page.close();

  const plain = await openApp({ context: { theme: 'light' }, result: await renderResult('line') });
  assert(await plain.frame.evaluate(() => document.getElementById('fullscreen').hidden), 'no button without full screen');
  await plain.page.close();
});

// ── Tests: the view back to the model (spec/06_claude_integration step 6) ────

const POINTS_SOURCE =
  'description: Test points\nimage: {width: 600, height: 400}\n' +
  'root:\n  type: Plot\n  title: Points\n  children:\n' +
  '    - {type: PlotLine, label: v, xs: [0, 1, 2, 3, 4], ys: [1, 7, 3, 5, 2]}\n';

/** Page coordinates of data point (x, y) of the App's first plot (the frame sits at 0, 0). */
const appPoint = (frame, x, y) =>
  frame.evaluate(
    ([x, y]) => {
      const viewer = window.nymphApp.viewer;
      const p = viewer.state().views[0];
      const box = viewer.canvas.getBoundingClientRect();
      return {
        x: box.left + p.x + ((x - p.xMin) / (p.xMax - p.xMin)) * p.width,
        y: box.top + p.y + ((p.yMax - y) / (p.yMax - p.yMin)) * p.height,
      };
    },
    [x, y],
  );
const contextUpdates = (page) =>
  page.evaluate(() => window.host.messages.filter((m) => m.method === 'ui/update-model-context').map((m) => m.params));

test('the MCP App tells the model what the user clicked and the view they left', async () => {
  const { page, frame } = await openApp({ result: { content: [], structuredContent: { source: POINTS_SOURCE } } });
  await page.waitForTimeout(800);
  assert((await contextUpdates(page)).length === 0, 'nothing sent before the user touches the chart');

  const point = await appPoint(frame, 1, 7);
  await page.mouse.click(point.x, point.y);
  await page.waitForTimeout(800);
  let updates = await contextUpdates(page);
  assert(updates.length === 1, `${updates.length} updates after a click`);
  let text = updates[0].content[0].text;
  assert(text.startsWith('nymph chart "Test points": the user changed the view.'), text);
  assert(text.includes('Last clicked: root/children/0 (PlotLine "v") x=1 y=7'), text);
  assert(text.includes('as written'), text);
  assert(updates[0].structuredContent.lastClicked.element === 'root/children/0', JSON.stringify(updates[0].structuredContent));

  const from = await appPoint(frame, 2, 4);
  await page.mouse.move(from.x, from.y);
  await page.mouse.down();
  await page.mouse.move(from.x - 120, from.y + 40, { steps: 8 });
  await page.mouse.up();
  await page.waitForTimeout(900);
  updates = await contextUpdates(page);
  assert(updates.length === 2, `${updates.length} updates after a drag (debounced into one)`);
  text = updates[1].content[0].text;
  const expected = await frame.evaluate(() => {
    const yaml = window.nymphApp.viewer.toYaml();
    return yaml.slice(yaml.indexOf('\nview:') + 1).trimEnd();
  });
  assert(expected.startsWith('view:\n  root: {x: ['), expected);
  assert(text.includes(`\n${expected}\n`), `${text}\n--- expected ---\n${expected}`);
  assert(updates[1].structuredContent.view === expected, 'the same view block as structured content');
  assert(text.includes('Last clicked: root/children/0'), 'the click is still reported');
  assert(text.endsWith('To keep this view, render the source with this view key.'), text);

  await page.mouse.click(from.x, from.y, { button: 'right' });
  await page.waitForTimeout(200);
  assert(!(await frame.evaluate(() => document.querySelector('.nymph-menu') !== null)), 'no right-click menu in the App');
  await page.close();
});

// ── Run ──────────────────────────────────────────────────────────────────────

let failed = 0;
for (const t of tests) {
  try {
    await t.fn();
    console.log(`ok   ${t.name}`);
  } catch (e) {
    ++failed;
    console.log(`FAIL ${t.name}\n     ${e.stack ?? e}`);
  }
}
await browser.close();
server.close();
console.log(`${tests.length - failed} of ${tests.length} passed`);
process.exit(failed === 0 ? 0 : 1);
