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
