// MIT License © 2026 Binary Dice Games
/**
 * @file pool.js
 * @brief Which live viewer to park when the page holds too many
 * (spec/05_browser_extension/DESIGN.md section 3.5, viewer pool).
 */

/** The most live viewers on a page when they run in the content script. */
export const MAX_LIVE_VIEWERS = 32;
/**
 * The most when each runs in its own frame: each has a module instance and
 * a WebGL context (browsers allow about 16 per process).
 */
export const MAX_LIVE_FRAMES = 12;

/**
 * Picks the viewer to park: the farthest from the viewport among those
 * outside the observer margin and not under the pointer.
 * @template T
 * @param {Iterable<{key: T, near: boolean, hovered: boolean, distance: number}>} entries
 *   the live viewers; `distance` is in CSS px from the viewport
 * @return {T|null} the one to park, or null when none may be
 */
export function pickToPark(entries) {
  let best = null;
  for (const e of entries) {
    if (e.near || e.hovered) {
      continue;
    }
    if (best === null || e.distance > best.distance) {
      best = e;
    }
  }
  return best === null ? null : best.key;
}

/**
 * Distance of a box from the viewport, 0 when they overlap.
 * @param {{top: number, bottom: number}} rect the box, in viewport coordinates
 * @param {number} viewportHeight
 * @return {number}
 */
export function distanceToViewport(rect, viewportHeight) {
  if (rect.bottom < 0) {
    return -rect.bottom;
  }
  if (rect.top > viewportHeight) {
    return rect.top - viewportHeight;
  }
  return 0;
}
