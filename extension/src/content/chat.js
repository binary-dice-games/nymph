// MIT License © 2026 Binary Dice Games
/**
 * @file chat.js
 * @brief Finds nymph sources in a chat's code blocks and keeps them replaced by
 * viewers while the answers stream and the page changes
 * (spec/10_gemini_extension/DESIGN.md sections 3.1, 3.3 and 4.1). Imported by
 * loader.js on the sites the user listed; start() and stop() follow the
 * settings.
 *
 * Every code block is tracked by an entry whose state moves:
 *
 *   watching -> ready -> mounting -> live <-> parked
 *        |                       \-> error (a claimed block the viewer refused: the host says why)
 *        \-----------> plain (not a nymph source, or the viewer refused it)
 *
 * `watching` is a block still changing: it is looked at only after its text
 * has been still for SETTLE_MS (longer while the page says an answer is being
 * written). Observers only queue work; it runs in idle callbacks.
 */

import { ext } from '../shared/api.js';
import { adapterFor } from './adapters/index.js';
import { CodeReplacement } from './code-replacer.js';
import { setCount } from './count.js';
import { MAX_LIVE_FRAMES, MAX_LIVE_VIEWERS, distanceToViewport, pickToPark } from './pool.js';
import { insertIntoComposer } from './adapters/generic.js';
import { canRunInline, injectPageStyle, removePageStyle } from './replacer.js';
import { claimsBlock, hashText, looksLikeNymph } from './source-detect.js';

/** How long a block's text must stay unchanged before it is considered. */
export const SETTLE_MS = 600;
/** The same while the page says an answer is still being written. */
export const SETTLE_STREAMING_MS = 3600;
/** Times a chart is put back after the page removes its host. */
const MAX_REMOUNTS = 3;

/**
 * @typedef {object} Entry
 * @property {import('./code-replacer.js').CodeBlock} block the block as last read
 * @property {'watching'|'ready'|'mounting'|'live'|'error'|'parked'|'plain'} state
 * @property {string} hash of the block's text
 * @property {boolean} dirty the text changed and has not settled yet
 * @property {number} timer settle timer, 0 when none
 * @property {boolean} longTimer the timer was armed while streaming
 * @property {CodeReplacement|null} chart
 * @property {string|null} yaml a parked chart's source with its view
 * @property {boolean} near within the observer margin
 * @property {number} gen bumped to cancel work in flight
 * @property {number} remounts
 * @property {string|null} error why the viewer refused the source
 */

let running = false;
let options = { theme: 'auto', origin: '' };
let adapter = null;
/** @type {Map<Element, Entry>} */
const entries = new Map();
/** @type {MutationObserver|null} */
let mutations = null;
/** @type {IntersectionObserver|null} */
let intersections = null;
let idleHandle = 0;

const idle = globalThis.requestIdleCallback ?? ((fn) => setTimeout(fn, 50));
const cancelIdle = globalThis.cancelIdleCallback ?? clearTimeout;

// ── Scheduling and scanning ──────────────────────────────────────────────────

function schedule() {
  if (!idleHandle && running) {
    idleHandle = idle(scan, { timeout: 200 });
  }
}

/** Reads every code block: new ones, changed ones, and the ones that left. */
function scan() {
  idleHandle = 0;
  if (!running) {
    return;
  }
  const seen = new Set();
  const streaming = adapter.isStreaming();
  for (const block of adapter.codeBlocks(document)) {
    seen.add(block.container);
    let entry = entries.get(block.container);
    if (!entry) {
      entry = {
        block, state: 'watching', hash: '', dirty: true, timer: 0, longTimer: false, chart: null,
        yaml: null, near: false, gen: 0, remounts: 0, error: null,
      };
      entries.set(block.container, entry);
      intersections.observe(block.container);
    }
    entry.block = block;
    if (entry.chart) {
      entry.chart.block = block;
    }
    const hash = hashText(block.text);
    if (hash !== entry.hash) {
      entry.hash = hash;
      entry.dirty = true;
      arm(entry, streaming);
    } else if (entry.dirty && (!entry.timer || (entry.longTimer && !streaming))) {
      arm(entry, streaming); // the answer finished: no need to wait longer
    }
  }
  for (const [container, entry] of [...entries]) {
    if (!seen.has(container) || !container.isConnected) {
      forget(entry, container);
    } else if ((entry.state === 'live' || entry.state === 'error') && !entry.chart?.host?.isConnected) {
      lostHost(entry);
    }
  }
}

/** (Re)starts an entry's settle timer. */
function arm(entry, streaming) {
  clearTimeout(entry.timer);
  entry.longTimer = streaming;
  entry.timer = setTimeout(() => {
    entry.timer = 0;
    settle(entry);
  }, streaming ? SETTLE_STREAMING_MS : SETTLE_MS);
}

/** The block's text has been still: decide what it is. */
async function settle(entry) {
  if (!running || !entry.dirty) {
    return;
  }
  entry.dirty = false;
  const { text, label } = entry.block;
  if (!claimsBlock(text, label) && !looksLikeNymph(text, label)) {
    console.info(`nymph: a code block is not a nymph source (label '${label}', ${text.length} characters)`);
    release(entry);
    entry.state = 'plain';
    return;
  }
  if (entry.state === 'live' && entry.chart && !entry.chart.error) {
    const gen = ++entry.gen;
    try {
      await entry.chart.update(text); // the user's zoom stays where the chart allows
    } catch (e) {
      if (gen === entry.gen) {
        release(entry);
        entry.state = 'ready'; // drawn again from scratch: a claimed block now shows the error
        entry.error = String(e?.message ?? e);
        mount(entry);
      }
    }
    return;
  }
  release(entry);
  entry.state = 'ready';
  entry.error = null;
  if (entry.near) {
    mount(entry);
  }
}

/** Drops whatever an entry shows, showing its code again. */
function release(entry) {
  entry.gen++;
  if (entry.chart) {
    entry.chart.unmount();
    entry.chart = null;
  }
  entry.yaml = null;
  report();
}

/** Forgets a block that left the page. */
function forget(entry, container) {
  clearTimeout(entry.timer);
  release(entry);
  intersections.unobserve(container);
  entries.delete(container);
}

/** The page removed a chart's host (a re-render of the message). */
function lostHost(entry) {
  const yaml = entry.chart.error ? null : entry.chart.yaml();
  const wasError = Boolean(entry.chart.error);
  release(entry);
  if (wasError) {
    entry.state = 'ready';
    if (entry.near) {
      mount(entry);
    }
    return;
  }
  if (entry.remounts < MAX_REMOUNTS && yaml) {
    entry.remounts++;
    entry.yaml = yaml;
    entry.state = 'parked';
    if (entry.near) {
      mount(entry, yaml);
    }
  } else {
    entry.state = 'plain';
  }
}

// ── Viewers and the pool ─────────────────────────────────────────────────────

function liveEntries() {
  return [...entries.values()].filter((e) => e.state === 'live' || e.state === 'mounting');
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
      distance: distanceToViewport((e.chart?.host ?? e.block.container).getBoundingClientRect(), innerHeight),
    })),
  );
  if (!key) {
    return false;
  }
  const yaml = key.chart?.yaml();
  release(key);
  key.yaml = yaml ?? null;
  key.state = yaml ? 'parked' : 'ready';
  return true;
}

/** Creates the entry's viewer (from `source`, a parked chart's YAML, if given). */
async function mount(entry, source) {
  if (entry.state === 'mounting' || entry.state === 'live' || !makeRoom()) {
    return;
  }
  const gen = ++entry.gen;
  const chart = new CodeReplacement(entry.block);
  entry.chart = chart;
  entry.state = 'mounting';
  try {
    await chart.mount(
      { ...options, claimed: claimsBlock(entry.block.text, entry.block.label), onFix: askToFix },
      source,
    );
  } catch (e) {
    if (gen === entry.gen) {
      entry.chart = null;
      entry.state = 'plain';
      entry.error = String(e?.message ?? e);
      console.warn('nymph: a code block was left as code:', entry.error);
    }
    return;
  }
  if (gen !== entry.gen || !running) {
    chart.unmount();
    return;
  }
  entry.state = chart.error ? 'error' : 'live';
  entry.error = chart.error;
  entry.yaml = null;
  report();
}

function report() {
  setCount('chat', [...entries.values()].filter((e) => e.state === 'live' || e.state === 'parked').length);
}

// ── Observers ────────────────────────────────────────────────────────────────

function onIntersect(records) {
  for (const r of records) {
    const entry = entries.get(r.target);
    if (!entry) {
      continue;
    }
    entry.near = r.isIntersecting;
    if (entry.near && entry.state === 'ready') {
      mount(entry);
    } else if (entry.near && entry.state === 'parked') {
      mount(entry, entry.yaml);
    }
  }
}

// ── Teaching the chat (spec/10_gemini_extension DESIGN 4.5) ──────────────────

/** A short message at the bottom of the page, for a few seconds. */
function toast(message) {
  const el = document.createElement('nymph-ext-toast');
  el.textContent = message;
  Object.assign(el.style, {
    position: 'fixed', left: '50%', bottom: '24px', transform: 'translateX(-50%)', zIndex: '2147483647',
    background: 'rgba(40, 60, 110, .95)', color: '#fff', font: '600 13px/1.4 system-ui, sans-serif',
    padding: '8px 14px', borderRadius: '6px', maxWidth: '80vw',
  });
  document.documentElement.appendChild(el);
  setTimeout(() => el.remove(), 5000);
}

/**
 * Puts the instructions that teach the chat nymph's format into the prompt
 * box, without sending them; copies them to the clipboard when the box cannot
 * take them.
 * @return {Promise<'inserted'|'copied'|'failed'>}
 */
export async function teach() {
  let text;
  try {
    const response = await fetch(ext.runtime.getURL('src/gemini/primer.txt'));
    text = (await response.text()).replace(/\r\n/g, '\n');
  } catch {
    toast('nymph: could not read the instructions.');
    return 'failed';
  }
  return giveText(
    text,
    'nymph: instructions added to the prompt box. Review them, then send.',
    'nymph: instructions copied. Paste them into the prompt box.',
    'nymph: could not add the instructions. Copy them from the nymph options page.',
  );
}

/**
 * Puts text in the prompt box without sending it, else on the clipboard.
 * @return {Promise<'inserted'|'copied'|'failed'>}
 */
async function giveText(text, insertedMessage, copiedMessage, failedMessage) {
  if (adapter && insertIntoComposer(adapter.composer?.() ?? null, text)) {
    toast(insertedMessage);
    return 'inserted';
  }
  try {
    await navigator.clipboard.writeText(text);
    toast(copiedMessage);
    return 'copied';
  } catch {
    toast(failedMessage);
    return 'failed';
  }
}

/**
 * The error button's action: the prompt that asks the chat to correct its
 * source (spec/10_gemini_extension/DESIGN.md section 3.4), in the prompt box
 * and not sent.
 * @param {string} error the viewer's message
 * @return {Promise<'inserted'|'copied'|'failed'>}
 */
export function askToFix(error) {
  return giveText(
    `The nymph chart failed to draw: ${error}\nFix the YAML and send the whole chart again.`,
    'nymph: the error is in the prompt box. Send it so the chat can fix the chart.',
    'nymph: the error was copied. Paste it into the prompt box.',
    'nymph: could not add the error to the prompt box.',
  );
}

/** The toolbar menu's "give this chat the instructions" (the worker sends it). */
function onMessage(message, sender, reply) {
  if (message?.type === 'nymph-teach' && sender.id === ext.runtime.id) {
    teach().then(reply);
    return true;
  }
  return false;
}

// ── Public ───────────────────────────────────────────────────────────────────

/**
 * Starts (or restarts with new options) drawing nymph sources found in the
 * page's code blocks.
 * @param {{theme?: 'source'|'auto', origin?: string}} [next]
 */
export function start(next = {}) {
  const theme = next.theme === 'source' ? 'source' : 'auto';
  const origin = next.origin ?? location.origin;
  if (running && theme === options.theme && origin === options.origin) {
    return;
  }
  if (running) {
    stop();
  }
  options = { theme, origin };
  adapter = adapterFor(origin);
  running = true;
  console.info(`nymph: chat mode on (${adapter.name} adapter)`);
  injectPageStyle();
  intersections = new IntersectionObserver(onIntersect, { rootMargin: '100% 0px' });
  mutations = new MutationObserver(schedule);
  mutations.observe(document.documentElement, { childList: true, subtree: true, characterData: true });
  ext.runtime.onMessage.addListener(onMessage);
  schedule();
}

/** Stops, showing every code block as the page had it and removing what was added. */
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
  ext.runtime.onMessage.removeListener(onMessage);
  for (const entry of entries.values()) {
    clearTimeout(entry.timer);
    entry.gen++;
    entry.chart?.unmount();
  }
  entries.clear();
  removePageStyle();
  setCount('chat', 0);
}
