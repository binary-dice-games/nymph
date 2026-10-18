// MIT License © 2026 Binary Dice Games
/**
 * @file cache.js
 * @brief A small least-recently-used map: what the extension learned about
 * each image URL on this page (spec/05_browser_extension/DESIGN.md
 * section 3.5).
 */

/** A Map that keeps at most `limit` entries, dropping the least recently used. */
export class LruCache {
  /** @param {number} limit the most entries kept (at least 1) */
  constructor(limit) {
    this.limit = Math.max(1, limit);
    /** @private @type {Map<string, any>} */
    this.map = new Map();
  }

  /** @return {number} entries kept */
  get size() {
    return this.map.size;
  }

  /**
   * @param {string} key
   * @return {any} the value (now the most recently used), or undefined
   */
  get(key) {
    if (!this.map.has(key)) {
      return undefined;
    }
    const value = this.map.get(key);
    this.map.delete(key);
    this.map.set(key, value);
    return value;
  }

  /**
   * Stores a value as the most recently used, dropping the oldest entry
   * when full.
   * @param {string} key
   * @param {any} value
   */
  set(key, value) {
    this.map.delete(key);
    this.map.set(key, value);
    if (this.map.size > this.limit) {
      this.map.delete(this.map.keys().next().value);
    }
  }

  /** @param {string} key @return {boolean} whether it was there */
  delete(key) {
    return this.map.delete(key);
  }
}
