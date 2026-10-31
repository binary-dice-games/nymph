// MIT License © 2026 Binary Dice Games
/** @file icons.js Inline SVG icons of the app (stroke icons, 24 x 24). */

const svg = (body) =>
  `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">${body}</svg>`;

export const icons = {
  file: svg('<path d="M14 3H7a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2V8z"/><path d="M14 3v5h5"/>'),
  link: svg('<path d="M10 14a4 4 0 0 0 5.7 0l3-3a4 4 0 0 0-5.7-5.7l-1 1"/><path d="M14 10a4 4 0 0 0-5.7 0l-3 3a4 4 0 0 0 5.7 5.7l1-1"/>'),
  drive: svg('<path d="M8 3h8l6 10-4 7H6l-4-7z"/><path d="M8 3l6 10H2M16 3 10 13M14 13h8M6 20l4-7"/>'),
  template: svg('<rect x="3" y="3" width="7" height="7" rx="1.5"/><rect x="14" y="3" width="7" height="7" rx="1.5"/><rect x="3" y="14" width="7" height="7" rx="1.5"/><rect x="14" y="14" width="7" height="7" rx="1.5"/>'),
  example: svg('<path d="M4 19V5M4 19h16"/><path d="m7 15 4-5 3 3 5-6"/>'),
  paste: svg('<rect x="6" y="4" width="12" height="17" rx="2"/><path d="M9 4V3h6v1"/>'),
  more: svg('<circle cx="5" cy="12" r="1.3"/><circle cx="12" cy="12" r="1.3"/><circle cx="19" cy="12" r="1.3"/>'),
  caret: svg('<path d="m7 10 5 5 5-5"/>'),
  download: svg('<path d="M12 4v11M7 10l5 5 5-5M5 20h14"/>'),
  copy: svg('<rect x="8" y="8" width="12" height="12" rx="2"/><path d="M16 8V6a2 2 0 0 0-2-2H6a2 2 0 0 0-2 2v8a2 2 0 0 0 2 2h2"/>'),
  code: svg('<path d="m8 8-4 4 4 4M16 8l4 4-4 4M13 5l-2 14"/>'),
  image: svg('<rect x="3" y="4" width="18" height="16" rx="2"/><circle cx="9" cy="10" r="2"/><path d="m21 17-5-5-8 8"/>'),
  theme: svg('<circle cx="12" cy="12" r="8"/><path d="M12 4v16a8 8 0 0 0 0-16z" fill="currentColor"/>'),
  info: svg('<circle cx="12" cy="12" r="9"/><path d="M12 11v6M12 8h.01"/>'),
  keyboard: svg('<rect x="2" y="6" width="20" height="12" rx="2"/><path d="M6 10h.01M10 10h.01M14 10h.01M18 10h.01M7 14h10"/>'),
  close: svg('<path d="M6 6l12 12M18 6 6 18"/>'),
  reset: svg('<path d="M4 4v6h6"/><path d="M5 15a8 8 0 1 0 2-8.5L4 10"/>'),
  save: svg('<path d="M5 3h11l3 3v13a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2z"/><path d="M7 3v6h8V3M7 21v-7h10v7"/>'),
  share: svg('<circle cx="18" cy="5" r="2.5"/><circle cx="6" cy="12" r="2.5"/><circle cx="18" cy="19" r="2.5"/><path d="m8.2 10.8 7.6-4.4M8.2 13.2l7.6 4.4"/>'),
  plus: svg('<path d="M12 5v14M5 12h14"/>'),
  live: svg('<circle cx="12" cy="12" r="2.5"/><path d="M7.8 7.8a6 6 0 0 0 0 8.4M16.2 7.8a6 6 0 0 1 0 8.4M5 5a10 10 0 0 0 0 14M19 5a10 10 0 0 1 0 14"/>'),
};

/**
 * Pictograms of chart types (62 x 52, in the accent colour), shown on the
 * import wizard's chart-type cards and the New chart templates.
 */
const chartIcons = {
  line: '<polyline points="6,40 18,28 30,32 42,16 56,20" fill="none" stroke="var(--accent)" stroke-width="3" stroke-linejoin="round" stroke-linecap="round"/><polyline points="6,44 18,38 30,40 42,30 56,34" fill="none" stroke="var(--muted)" stroke-width="2" stroke-linejoin="round" opacity=".6"/>',
  area: '<path d="M6 40 L18 26 L30 32 L42 14 L56 20 L56 46 L6 46Z" fill="var(--accent)" opacity=".25"/><polyline points="6,40 18,26 30,32 42,14 56,20" fill="none" stroke="var(--accent)" stroke-width="3" stroke-linejoin="round"/>',
  bars: '<rect x="8" y="22" width="7" height="24" rx="1.5" fill="var(--accent)"/><rect x="16" y="30" width="7" height="16" rx="1.5" fill="var(--muted)" opacity=".6"/><rect x="30" y="12" width="7" height="34" rx="1.5" fill="var(--accent)"/><rect x="38" y="24" width="7" height="22" rx="1.5" fill="var(--muted)" opacity=".6"/>',
  stacked: '<rect x="10" y="26" width="10" height="20" rx="1.5" fill="var(--accent)"/><rect x="10" y="16" width="10" height="10" fill="var(--muted)" opacity=".6"/><rect x="26" y="20" width="10" height="26" rx="1.5" fill="var(--accent)"/><rect x="26" y="8" width="10" height="12" fill="var(--muted)" opacity=".6"/><rect x="42" y="30" width="10" height="16" rx="1.5" fill="var(--accent)"/><rect x="42" y="22" width="10" height="8" fill="var(--muted)" opacity=".6"/>',
  hbars: '<rect x="8" y="8" width="44" height="8" rx="1.5" fill="var(--accent)"/><rect x="8" y="20" width="32" height="8" rx="1.5" fill="var(--accent)" opacity=".8"/><rect x="8" y="32" width="22" height="8" rx="1.5" fill="var(--accent)" opacity=".6"/><rect x="8" y="44" width="12" height="4" rx="1.5" fill="var(--muted)" opacity=".6"/>',
  scatter: '<circle cx="12" cy="38" r="3" fill="var(--accent)"/><circle cx="20" cy="32" r="3" fill="var(--accent)"/><circle cx="26" cy="36" r="3" fill="var(--accent)"/><circle cx="32" cy="24" r="3" fill="var(--accent)"/><circle cx="40" cy="22" r="3" fill="var(--accent)"/><circle cx="46" cy="14" r="3" fill="var(--accent)"/><circle cx="52" cy="18" r="3" fill="var(--muted)" opacity=".6"/>',
  pie: '<circle cx="31" cy="27" r="19" fill="var(--muted)" opacity=".35"/><path d="M31 27 L31 8 A19 19 0 0 1 48 35 Z" fill="var(--accent)"/><path d="M31 27 L48 35 A19 19 0 0 1 22 44 Z" fill="var(--accent)" opacity=".6"/>',
  histogram: '<rect x="6" y="38" width="8" height="8" fill="var(--accent)" opacity=".6"/><rect x="14" y="26" width="8" height="20" fill="var(--accent)" opacity=".8"/><rect x="22" y="12" width="8" height="34" fill="var(--accent)"/><rect x="30" y="18" width="8" height="28" fill="var(--accent)"/><rect x="38" y="30" width="8" height="16" fill="var(--accent)" opacity=".8"/><rect x="46" y="40" width="8" height="6" fill="var(--accent)" opacity=".6"/>',
  dashboard: '<rect x="6" y="6" width="50" height="6" rx="1.5" fill="var(--accent)"/><rect x="6" y="16" width="15" height="9" rx="1.5" fill="var(--muted)" opacity=".5"/><rect x="23.5" y="16" width="15" height="9" rx="1.5" fill="var(--muted)" opacity=".5"/><rect x="41" y="16" width="15" height="9" rx="1.5" fill="var(--muted)" opacity=".5"/><rect x="6" y="29" width="24" height="17" rx="1.5" fill="none" stroke="var(--muted)" stroke-width="1.5" opacity=".7"/><polyline points="9,42 15,36 20,39 27,32" fill="none" stroke="var(--accent)" stroke-width="2" stroke-linejoin="round" stroke-linecap="round"/><rect x="32" y="29" width="24" height="17" rx="1.5" fill="none" stroke="var(--muted)" stroke-width="1.5" opacity=".7"/><rect x="36" y="37" width="4" height="6" fill="var(--accent)"/><rect x="42" y="33" width="4" height="10" fill="var(--accent)"/><rect x="48" y="35" width="4" height="8" fill="var(--accent)"/>',
  flowchart: '<g fill="none" stroke-width="2" stroke-linejoin="round"><rect x="20" y="4" width="22" height="9" rx="4.5" stroke="var(--accent)"/><path d="M31 19 L39 25 L31 31 L23 25Z" stroke="var(--accent)"/><rect x="4" y="38" width="18" height="9" rx="1.5" stroke="var(--muted)"/><rect x="40" y="38" width="18" height="9" rx="1.5" stroke="var(--muted)"/><path d="M31 13 V19 M23 25 H13 V38 M39 25 H49 V38" stroke="var(--muted)" opacity=".8"/></g>',
  class: '<g fill="none" stroke-width="2"><rect x="4" y="6" width="22" height="30" rx="1.5" stroke="var(--accent)"/><path d="M4 14 H26 M4 26 H26" stroke="var(--accent)"/><rect x="36" y="18" width="22" height="28" rx="1.5" stroke="var(--muted)"/><path d="M36 26 H58 M36 37 H58" stroke="var(--muted)"/><path d="M36 30 H31" stroke="var(--muted)"/></g><path d="M31 30 L26 26 V34Z" fill="none" stroke="var(--muted)" stroke-width="1.5" stroke-linejoin="round"/><rect x="4" y="6" width="22" height="8" fill="var(--accent)" opacity=".25"/>',
  sequence: '<rect x="6" y="4" width="16" height="8" rx="1.5" fill="var(--accent)"/><rect x="40" y="4" width="16" height="8" rx="1.5" fill="var(--muted)" opacity=".7"/><path d="M14 12 V48 M48 12 V48" stroke="var(--muted)" stroke-width="1.5" stroke-dasharray="3 2" opacity=".7"/><path d="M14 21 H46 M48 31 H16 M14 41 H46" fill="none" stroke="var(--accent)" stroke-width="2"/><path d="M42 18 L47 21 L42 24 M20 28 L15 31 L20 34 M42 38 L47 41 L42 44" fill="none" stroke="var(--accent)" stroke-width="2" stroke-linejoin="round" stroke-linecap="round"/>',
  gantt: '<path d="M6 4 V48" stroke="var(--muted)" stroke-width="1.5" opacity=".6"/><rect x="8" y="8" width="18" height="7" rx="1.5" fill="var(--accent)"/><rect x="20" y="19" width="16" height="7" rx="1.5" fill="var(--accent)" opacity=".8"/><rect x="30" y="30" width="20" height="7" rx="1.5" fill="var(--accent)" opacity=".6"/><path d="M53 37 L57 41 L53 45 L49 41Z" fill="var(--muted)"/><path d="M44 4 V48" stroke="var(--muted)" stroke-width="1.5" stroke-dasharray="3 2" opacity=".7"/>',
  sankey: '<rect x="4" y="6" width="5" height="40" rx="1" fill="var(--accent)"/><rect x="53" y="6" width="5" height="14" rx="1" fill="var(--accent)"/><rect x="53" y="24" width="5" height="10" rx="1" fill="var(--muted)" opacity=".8"/><rect x="53" y="38" width="5" height="8" rx="1" fill="var(--muted)" opacity=".6"/><path d="M9 6 C31 6 31 6 53 6 L53 20 C31 20 31 20 9 20Z" fill="var(--accent)" opacity=".35"/><path d="M9 20 C31 20 31 24 53 24 L53 34 C31 34 31 30 9 30Z" fill="var(--muted)" opacity=".35"/><path d="M9 30 C31 30 31 38 53 38 L53 46 C31 46 31 46 9 46Z" fill="var(--muted)" opacity=".25"/>',
  mockup: '<rect x="5" y="5" width="52" height="42" rx="3" fill="none" stroke="var(--muted)" stroke-width="1.5"/><path d="M5 8 A3 3 0 0 1 8 5 H54 A3 3 0 0 1 57 8 V13 H5Z" fill="var(--accent)"/><rect x="10" y="19" width="22" height="4" rx="1" fill="var(--muted)" opacity=".6"/><rect x="10" y="27" width="42" height="6" rx="1.5" fill="none" stroke="var(--muted)" stroke-width="1.5" opacity=".7"/><rect x="34" y="38" width="18" height="6" rx="1.5" fill="var(--accent)"/><rect x="10" y="38" width="6" height="6" rx="1" fill="none" stroke="var(--accent)" stroke-width="1.5"/>',
};

/**
 * The pictogram of a chart type as an SVG string.
 * @param {string} id a chart type (line, bars, flowchart, ...)
 * @returns {string} the SVG, or an empty string for an unknown type
 */
export function chartIcon(id) {
  const body = chartIcons[id];
  return body ? `<svg viewBox="0 0 62 52" width="62" height="52" aria-hidden="true">${body}</svg>` : '';
}
