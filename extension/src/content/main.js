// MIT License © 2026 Binary Dice Games
/**
 * @file main.js
 * @brief Finds nymph PNGs on the page and keeps them replaced by viewers
 * while the page changes (spec/05_browser_extension/DESIGN.md sections
 * 3.1 and 3.5). Imported by loader.js when the extension is on for the
 * site; start() and stop() follow the toolbar button.
 *
 * Every image is tracked by an entry whose state moves:
 *
 *   watching -> reading -> live <-> parked
 *                      \-> plain (not a nymph PNG, or the viewer refused it)
 *
 * Observers only queue work; it runs in idle callbacks.
 */

import { LruCache } from './cache.js';
import { readImage } from './fetcher.js';
import { MAX_LIVE_FRAMES, MAX_LIVE_VIEWERS, distanceToViewport, pickToPark } from './pool.js';
import { HIDDEN_ATTR, HOST_TAG, Replacement, canRunInline, injectPageStyle, removePageStyle } from './replacer.js';

/** Smallest natural size of a candidate image. */
const MIN_NATURAL = 32;
/** Smallest rendered size of a candidate image, in CSS px. */
const MIN_RENDERED = 24;
/** Times a chart is put back after the page removes its host. */
const MAX_REMOUNTS = 3;

/**
 * @typedef {object} Entry
 * @property {HTMLImageElement} img
 * @property {'watching'|'reading'|'waiting'|'live'|'parked'|'plain'} state
 * @property {string} url the URL read (state reading and after)
 * @property {Replacement|null} chart
 * @property {string|null} yaml a parked chart's source with its view
 * @property {boolean} near within the observer margin
 * @property {number} gen bumped to cancel work in flight
 * @property {number} remounts
 * @property {{content: object, yaml: string|null}|null} [pending] a viewer waiting for room in the pool
 */

let running = false;
let options = { theme: 'source' };
/** URL -> read result or the promise of one. */
const cache = new LruCache(4000);
/** @type {Map<HTMLImageElement, Entry>} */
const entries = new Map();
/** Observed element (image or host) -> entry. */
const byTarget = new WeakMap();
/** @type {MutationObserver|null} */
let mutations = null;
/** @type {IntersectionObserver|null} */
let intersections = null;
const queue = { images: new Set(), sweep: false };
let idleHandle = 0;
let lastCount = -1;

// ── Scheduling ───────────────────────────────────────────────────────────────

const idle = globalThis.requestIdleCallback ?? ((fn) => setTimeout(fn, 50));
const cancelIdle = globalThis.cancelIdleCallback ?? clearTimeout;

function schedule() {
  if (!idleHandle && running) {
    idleHandle = idle(runQueue, { timeout: 200 });
  }
}

function runQueue() {
  idleHandle = 0;
  if (!running) {
    return;
  }
  const images = [...queue.images];
  queue.images.clear();
  if (queue.sweep) {
    queue.sweep = false;
    sweep();
  }
  for (const img of images) {
    track(img);
  }
  retryWaiting();
}

// ── Tracking images ──────────────────────────────────────────────────────────

/** Starts watching an image (once). */
function track(img) {
  if (!img.isConnected) {
    return;
  }
  let entry = entries.get(img);
  if (!entry) {
    entry = { img, state: 'watching', url: '', chart: null, yaml: null, near: false, gen: 0, remounts: 0 };
    entries.set(img, entry);
    byTarget.set(img, entry);
    intersections.observe(img);
    return; // the observer's first callback says whether it is near
  }
  const url = img.currentSrc || img.src;
  if (entry.state !== 'watching' && url && url !== entry.url) {
    reset(entry); // a new picture in the same element
  }
  if (entry.near) {
    consider(entry);
  }
}

/** Drops whatever an entry showed and starts over with the image. */
function reset(entry) {
  entry.gen++;
  if (entry.chart) {
    if (entry.chart.host) {
      intersections.unobserve(entry.chart.host);
    }
    entry.chart.unmount();
    entry.chart = null;
  }
  entry.state = 'watching';
  entry.url = '';
  entry.yaml = null;
  entry.remounts = 0;
  reportCount();
}

/** Forgets an image that left the page. */
function forget(entry) {
  reset(entry);
  intersections.unobserve(entry.img);
  entries.delete(entry.img);
}

/** Images and hosts the page removed or tampered with. */
function sweep() {
  for (const entry of [...entries.values()]) {
    if (!entry.img.isConnected) {
      forget(entry);
      continue;
    }
    if (entry.state !== 'live' || !entry.chart) {
      continue;
    }
    const host = entry.chart.host;
    if (!host?.isConnected) {
      // The page re-rendered the image's parent without our host.
      const yaml = entry.chart.yaml();
      reset(entry);
      if (entry.remounts < MAX_REMOUNTS && yaml) {
        entry.remounts++;
        entry.yaml = yaml;
        entry.state = 'parked';
        if (entry.near) {
          revive(entry);
        }
      } else {
        entry.state = 'plain';
      }
    } else if (entry.img.getAttribute(HIDDEN_ATTR) !== 'hidden') {
      entry.img.setAttribute(HIDDEN_ATTR, 'hidden');
    }
  }
}

/** Whether an image may be a chart, before reading it. */
function isCandidate(img) {
  if (!img.complete || img.naturalWidth < MIN_NATURAL || img.naturalHeight < MIN_NATURAL) {
    return false;
  }
  const rect = img.getBoundingClientRect();
  return rect.width >= MIN_RENDERED && rect.height >= MIN_RENDERED;
}

/** Reads a near image and replaces it when it is a nymph PNG. */
async function consider(entry) {
  if (entry.state === 'parked') {
    revive(entry);
    return;
  }
  if (entry.state !== 'watching' || !isCandidate(entry.img)) {
    return;
  }
  const url = entry.img.currentSrc || entry.img.src;
  if (!url) {
    return;
  }
  const gen = ++entry.gen;
  entry.state = 'reading';
  entry.url = url;
  let result = cache.get(url);
  if (!result) {
    result = readImage(url);
    cache.set(url, result);
    result.then((r) => cache.get(url) === result && cache.set(url, r));
  }
  result = await result;
  if (gen !== entry.gen || !running) {
    return;
  }
  if (!result.nymph) {
    entry.state = 'plain';
    return;
  }
  entry.state = 'waiting';
  show(entry, { png: result.bytes });
}

// ── Viewers and the pool ─────────────────────────────────────────────────────

function liveEntries() {
  return [...entries.values()].filter((e) => e.state === 'live' || (e.state === 'reading' && e.chart));
}

/** Makes room for one more viewer; false when none may be parked. */
function makeRoom() {
  const live = liveEntries();
  if (live.length < (canRunInline() ? MAX_LIVE_VIEWERS : MAX_LIVE_FRAMES)) {
    return true;
  }
  const key = pickToPark(
    live.map((e) => ({
      key: e,
      near: e.near,
      hovered: e.chart?.host?.matches(':hover') ?? false,
      distance: distanceToViewport(e.chart?.host?.getBoundingClientRect() ?? e.img.getBoundingClientRect(), innerHeight),
    })),
  );
  if (!key) {
    return false;
  }
  park(key);
  return true;
}

/** Frees a far viewer, keeping its source and view. */
function park(entry) {
  const yaml = entry.chart?.yaml();
  const url = entry.url;
  reset(entry);
  entry.url = url;
  if (yaml) {
    entry.yaml = yaml;
    entry.state = 'parked';
  } else {
    entry.state = 'watching';
  }
}

/** A parked chart near the viewport again. */
function revive(entry) {
  const yaml = entry.yaml;
  entry.state = 'waiting';
  show(entry, { source: yaml }, yaml);
}

/** Entries waiting for a free viewer. */
function retryWaiting() {
  for (const entry of entries.values()) {
    if (entry.state === 'waiting' && entry.pending && entry.near) {
      const { content, yaml } = entry.pending;
      entry.pending = null;
      show(entry, content, yaml);
    }
  }
}

/** Creates the entry's viewer, or queues it until there is room. */
async function show(entry, content, yaml = null) {
  if (!makeRoom()) {
    entry.pending = { content, yaml };
    return;
  }
  const gen = ++entry.gen;
  const chart = new Replacement(entry.img, entry.url);
  entry.chart = chart;
  entry.state = 'reading';
  try {
    await chart.mount(content, options);
  } catch (e) {
    if (gen === entry.gen) {
      entry.chart = null;
      entry.state = 'plain';
      console.debug('nymph: left as an image', entry.url, e?.message ?? e);
    }
    return;
  }
  if (gen !== entry.gen || !running) {
    chart.unmount();
    return;
  }
  entry.state = 'live';
  entry.yaml = null;
  byTarget.set(chart.host, entry);
  intersections.observe(chart.host);
  reportCount();
}

// ── Observers ────────────────────────────────────────────────────────────────

function onIntersect(records) {
  for (const r of records) {
    const entry = byTarget.get(r.target);
    if (!entry) {
      continue;
    }
    // A live chart's image is hidden (never intersecting): its host speaks for it.
    if (entry.state === 'live' && r.target === entry.img) {
      continue;
    }
    entry.near = r.isIntersecting;
    if (entry.near) {
      consider(entry);
    }
  }
  retryWaiting();
}

function onMutations(records) {
  for (const r of records) {
    if (r.type === 'childList') {
      for (const node of r.addedNodes) {
        if (node.nodeType !== Node.ELEMENT_NODE || node.localName === HOST_TAG) {
          continue;
        }
        if (node.localName === 'img') {
          queue.images.add(node);
        } else {
          for (const img of node.getElementsByTagName('img')) {
            queue.images.add(img);
          }
        }
      }
      if (r.removedNodes.length > 0) {
        queue.sweep = true;
      }
    } else if (r.target.localName === 'img') {
      if (r.attributeName === HIDDEN_ATTR) {
        queue.sweep = true;
      } else {
        queue.images.add(r.target);
      }
    }
  }
  schedule();
}

/** Images that finish loading or switch `srcset` candidates. */
function onLoad(e) {
  if (e.target?.localName === 'img') {
    queue.images.add(e.target);
    schedule();
  }
}

function reportCount() {
  const count = [...entries.values()].filter((e) => e.state === 'live' || e.state === 'parked').length;
  if (count !== lastCount) {
    lastCount = count;
    try {
      chrome.runtime.sendMessage({ type: 'nymph-count', count }).catch(() => {});
    } catch {
      // the extension was reloaded: this script is orphaned
    }
  }
}

// ── Public ───────────────────────────────────────────────────────────────────

/**
 * Starts (or restarts with new options) replacing nymph PNGs on the page.
 * @param {{theme?: 'source'|'auto'}} [next]
 */
export function start(next = {}) {
  const theme = next.theme === 'auto' ? 'auto' : 'source';
  if (running && theme === options.theme) {
    return;
  }
  if (running) {
    stop();
  }
  options = { theme };
  running = true;
  injectPageStyle();
  intersections = new IntersectionObserver(onIntersect, { rootMargin: '100% 0px' });
  mutations = new MutationObserver(onMutations);
  mutations.observe(document.documentElement, {
    childList: true,
    subtree: true,
    attributes: true,
    attributeFilter: ['src', 'srcset', 'sizes', HIDDEN_ATTR],
  });
  document.addEventListener('load', onLoad, true);
  for (const img of document.images) {
    queue.images.add(img);
  }
  lastCount = -1;
  schedule();
  reportCount();
}

/** Stops, restoring every image and removing everything the extension added. */
export function stop() {
  if (!running) {
    return;
  }
  running = false;
  if (idleHandle) {
    cancelIdle(idleHandle);
    idleHandle = 0;
  }
  mutations.disconnect();
  intersections.disconnect();
  document.removeEventListener('load', onLoad, true);
  for (const entry of entries.values()) {
    entry.gen++;
    entry.chart?.unmount();
  }
  entries.clear();
  queue.images.clear();
  removePageStyle();
  lastCount = -1;
  reportCount();
}
