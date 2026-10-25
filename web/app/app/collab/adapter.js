// MIT License © 2026 Binary Dice Games
/**
 * @file adapter.js The whiteboard's adapter for nymph
 * (spec/18_collaboration/DESIGN.md section 6.4): content units are the
 * figure's layout pixels, placed where the viewer or the editor draws the
 * figure, whatever the window size, the mode or the editor's divider.
 */

/**
 * @param {HTMLElement} overlay the element the whiteboard covers (the stage)
 * @param {() => ({figureBox: () => object|null, on: Function, container?: HTMLElement})|null} current
 *        the viewer or editor shown now
 */
export function nymphAdapter(overlay, current) {
  const listeners = new Set();
  let off = null;
  let watched = null;
  const changed = () => listeners.forEach((cb) => cb());
  const ro = typeof ResizeObserver === 'function' ? new ResizeObserver(changed) : null;
  ro?.observe(overlay);

  /** Follows the module shown now (called when the mode changes). */
  function watch() {
    const target = current();
    if (target === watched) {
      return;
    }
    off?.();
    off = null;
    watched = target;
    if (target) {
      off = target.on('figure', changed);
    }
    changed();
  }

  return {
    element: overlay,
    watch,
    /** Content (layout pixels) to CSS pixels of the overlay. */
    transform() {
      const target = current();
      const box = target?.figureBox();
      if (!box || !target.container) {
        return { a: 1, b: 0, c: 0, d: 1, e: 0, f: 0, hidden: true };
      }
      const host = overlay.querySelector('collab-whiteboard') ?? overlay;
      const o = host.getBoundingClientRect();
      const c = target.container.getBoundingClientRect();
      return { a: box.scale, b: 0, c: 0, d: box.scale, e: c.left - o.left + box.left, f: c.top - o.top + box.top };
    },
    /**
     * Where drawings may show, CSS pixels of the overlay: the editor's figure
     * area (not over its panel), or null (View: the whole stage).
     */
    clip() {
      const target = current();
      const area = target?.figureBox()?.area;
      if (!area || !target.container) {
        return null;
      }
      const host = overlay.querySelector('collab-whiteboard') ?? overlay;
      const o = host.getBoundingClientRect();
      const c = target.container.getBoundingClientRect();
      return { x: c.left - o.left + area.left, y: c.top - o.top + area.top, width: area.width, height: area.height };
    },
    /** The figure's layout size, for the export. */
    contentSize() {
      const box = current()?.figureBox();
      return box ? { width: box.width / box.scale, height: box.height / box.scale } : null;
    },
    onChange(cb) {
      listeners.add(cb);
      return () => listeners.delete(cb);
    },
    destroy() {
      off?.();
      ro?.disconnect();
      listeners.clear();
    },
  };
}
