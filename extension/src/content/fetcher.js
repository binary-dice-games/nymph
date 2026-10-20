// MIT License © 2026 Binary Dice Games
/**
 * @file fetcher.js
 * @brief Gets an image's bytes and asks the sniffer whether it is a nymph
 * PNG (spec/05_browser_extension/DESIGN.md section 4, fetcher).
 *
 * `data:`, `blob:` and same-origin URLs are read here, with the page's
 * cookies and its HTTP cache. Cross-origin URLs go to the service worker,
 * whose host permission lifts CORS, without cookies; it sniffs there and
 * sends back only nymph PNGs.
 */

import { ext } from '../shared/api.js';
import { sniffStream } from './sniff.js';

/** Fetches running at once. */
export const MAX_CONCURRENT_FETCHES = 4;

let running = 0;
/** @type {Array<() => void>} */
const waiting = [];

async function slot() {
  if (running >= MAX_CONCURRENT_FETCHES) {
    await new Promise((resolve) => waiting.push(resolve));
  }
  running++;
}

function release() {
  running--;
  waiting.shift()?.();
}

/**
 * Where an image URL is read.
 * @param {string} url absolute
 * @param {string} pageOrigin the page's origin
 * @return {'here'|'worker'|'never'}
 */
export function routeFor(url, pageOrigin) {
  let u;
  try {
    u = new URL(url);
  } catch {
    return 'never';
  }
  if (u.protocol === 'data:' || u.protocol === 'blob:') {
    return 'here';
  }
  if (u.protocol !== 'http:' && u.protocol !== 'https:') {
    return 'never';
  }
  return u.origin === pageOrigin ? 'here' : 'worker';
}

/** Base64 to bytes. */
export function fromBase64(text) {
  const raw = atob(text);
  const bytes = new Uint8Array(raw.length);
  for (let i = 0; i < raw.length; i++) {
    bytes[i] = raw.charCodeAt(i);
  }
  return bytes;
}

async function readHere(url) {
  const response = await fetch(url, { cache: 'force-cache', credentials: 'same-origin' });
  if (!response.ok || !response.body) {
    return { nymph: false, reason: `HTTP ${response.status}` };
  }
  return sniffStream(response.body);
}

async function readInWorker(url) {
  const answer = await ext.runtime.sendMessage({ type: 'nymph-sniff', url });
  if (!answer || answer.error) {
    return { nymph: false, reason: answer?.error ?? 'no answer' };
  }
  return answer.nymph ? { nymph: true, bytes: fromBase64(answer.base64) } : { nymph: false, reason: answer.reason };
}

/**
 * Reads an image and decides whether it is a nymph PNG.
 * @param {string} url the image's `currentSrc`
 * @return {Promise<{nymph: boolean, bytes?: Uint8Array, reason?: string}>}
 *   never rejects: a failed read is "not nymph" with a reason
 */
export async function readImage(url) {
  const route = routeFor(url, location.origin);
  if (route === 'never') {
    return { nymph: false, reason: 'unsupported URL' };
  }
  await slot();
  try {
    if (route === 'here') {
      try {
        return await readHere(url);
      } catch (e) {
        // A same-origin URL that redirects to a host without CORS.
        if (url.startsWith('http')) {
          return await readInWorker(url);
        }
        throw e;
      }
    }
    return await readInWorker(url);
  } catch (e) {
    return { nymph: false, reason: String(e?.message ?? e) };
  } finally {
    release();
  }
}
