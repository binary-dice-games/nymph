// MIT License © 2026 Binary Dice Games
/**
 * @file index.js
 * @brief Chooses the chat adapter for a site (spec/10_gemini_extension/DESIGN.md
 * section 4.3).
 */

import { gemini } from './gemini.js';
import { generic } from './generic.js';

const BY_ORIGIN = { 'https://gemini.google.com': gemini };

/**
 * @param {string} origin e.g. "https://gemini.google.com"
 * @return {typeof generic} the site's adapter, else the generic `<pre><code>` one
 */
export function adapterFor(origin) {
  return BY_ORIGIN[origin] ?? generic;
}
