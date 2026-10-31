// MIT License © 2026 Binary Dice Games
/**
 * @file ui.js The app's DOM helpers: elements, menus, dialogs, toasts and
 * the banner. Every text from a file goes in as textContent, never HTML
 * (spec/15_web_app/DESIGN.md section 8).
 */

import { icons } from './icons.js';

/** Creates an element: el('button', {class: 'btn', onclick}, 'text', child). */
export function el(tag, props = {}, ...children) {
  const node = document.createElement(tag);
  for (const [key, value] of Object.entries(props)) {
    if (value === undefined || value === null || value === false) {
      continue;
    }
    if (key.startsWith('on') && typeof value === 'function') {
      node.addEventListener(key.slice(2), value);
    } else if (key === 'class') {
      node.className = value;
    } else if (key === 'html') {
      node.innerHTML = value; // only for the app's own icons
    } else if (key === 'dataset') {
      Object.assign(node.dataset, value);
    } else {
      node.setAttribute(key, value === true ? '' : value);
    }
  }
  for (const child of children.flat()) {
    if (child !== null && child !== undefined && child !== false) {
      node.append(child instanceof Node ? child : document.createTextNode(String(child)));
    }
  }
  return node;
}

// ── Toasts ──────────────────────────────────────────────────────────────────

export function toast(message, ms = 3200) {
  const box = document.getElementById('toasts');
  const t = el('div', { class: 'toast', role: 'status' }, message);
  box.append(t);
  setTimeout(() => t.remove(), ms);
}

// ── Menus ───────────────────────────────────────────────────────────────────

let openMenu = null;

/** Closes the open menu, if any. */
export function closeMenu() {
  if (openMenu) {
    openMenu.remove();
    openMenu = null;
  }
}

/**
 * Opens a menu under `anchor`: items {label, icon, note, disabled, run} or
 * '-' for a separator. Keyboard: arrows, Enter, Escape.
 */
export function showMenu(anchor, items) {
  closeMenu();
  const menu = el('div', { class: 'menu', role: 'menu' });
  for (const item of items) {
    if (item === '-') {
      menu.append(el('hr'));
      continue;
    }
    if (!item || item.hidden) {
      continue;
    }
    const b = el(
      'button',
      { type: 'button', role: 'menuitem', disabled: !!item.disabled, title: item.title },
      item.icon ? el('span', { html: icons[item.icon] ?? '', style: 'display:inline-flex' }) : null,
      el('span', {}, item.label),
      item.note ? el('span', { class: 'note' }, item.note) : null,
    );
    b.addEventListener('click', () => {
      closeMenu();
      item.run?.();
    });
    menu.append(b);
  }
  document.body.append(menu);
  const r = anchor.getBoundingClientRect();
  const w = menu.offsetWidth;
  const left = Math.max(8, Math.min(r.right - w, window.innerWidth - w - 8));
  menu.style.left = `${left}px`;
  menu.style.top = `${Math.min(r.bottom + 6, window.innerHeight - menu.offsetHeight - 8)}px`;
  openMenu = menu;
  const buttons = [...menu.querySelectorAll('button:not(:disabled)')];
  buttons[0]?.focus();
  menu.addEventListener('keydown', (e) => {
    const i = buttons.indexOf(document.activeElement);
    if (e.key === 'ArrowDown') {
      buttons[(i + 1) % buttons.length]?.focus();
      e.preventDefault();
    } else if (e.key === 'ArrowUp') {
      buttons[(i - 1 + buttons.length) % buttons.length]?.focus();
      e.preventDefault();
    } else if (e.key === 'Escape') {
      closeMenu();
      anchor.focus();
    }
  });
  setTimeout(() => {
    const away = (e) => {
      if (!menu.contains(e.target)) {
        closeMenu();
        document.removeEventListener('pointerdown', away, true);
      }
    };
    document.addEventListener('pointerdown', away, true);
  });
}

// ── Dialogs ─────────────────────────────────────────────────────────────────

/**
 * A modal dialog. `build(body, close)` fills it; resolves with what
 * close(value) is given (undefined for Escape). `options.class` adds a
 * class to the dialog element, `options.cancelValue` is Escape's value.
 */
export function dialog(title, build, options = {}) {
  return new Promise((resolve) => {
    const d = el('dialog', { class: `dlg ${options.class ?? ''}`.trim(), 'aria-label': title });
    const body = el('div', { class: 'body' }, el('h2', {}, title));
    d.append(body);
    let done = false;
    const close = (value) => {
      if (done) {
        return;
      }
      done = true;
      d.close();
      d.remove();
      resolve(value);
    };
    d.addEventListener('cancel', (e) => {
      e.preventDefault();
      close(options.cancelValue);
    });
    build(body, close);
    document.body.append(d);
    d.showModal();
    d.querySelector('[autofocus]')?.focus();
  });
}

/** Buttons for a dialog: [{label, value, kind: 'primary'|'danger'}]. */
export function dialogButtons(close, buttons) {
  return el(
    'div',
    { class: 'row' },
    buttons.map((b) =>
      el('button', { type: 'button', class: `btn ${b.kind ?? ''}`, autofocus: !!b.focus, onclick: () => close(b.value) }, b.label),
    ),
  );
}

/** A question with buttons; resolves with the chosen button's value. */
export function ask(title, message, buttons) {
  return dialog(
    title,
    (body, close) => {
      if (message) {
        body.append(typeof message === 'string' ? el('p', {}, message) : message);
      }
      body.append(dialogButtons(close, buttons));
    },
    { cancelValue: 'cancel' },
  );
}

// ── Banner ──────────────────────────────────────────────────────────────────

/**
 * Shows the banner over the chart: {kind: 'error'|'info'|'warning', text,
 * where, actions: [{label, run, primary}]}; null hides it.
 */
export function banner(spec) {
  const b = document.getElementById('banner');
  b.replaceChildren();
  if (!spec) {
    b.hidden = true;
    return;
  }
  b.hidden = false;
  b.className = `banner ${spec.kind ?? 'warning'}`;
  b.append(
    el('div', { class: 'text' }, spec.where ? el('span', { class: 'where' }, spec.where) : null, spec.text),
    ...(spec.actions ?? []).map((a) =>
      el('button', { type: 'button', class: `btn ${a.primary ? 'primary' : ''}`, onclick: a.run }, a.label),
    ),
    spec.dismiss === false
      ? null
      : el('button', { type: 'button', class: 'btn ghost icon', 'aria-label': 'Dismiss', html: icons.close, onclick: () => banner(null) }),
  );
}

/** The busy line at the top of the page while `promise` runs. */
export async function busy(promise) {
  const p = document.getElementById('progress');
  busy.count = (busy.count ?? 0) + 1;
  p.hidden = false;
  try {
    return await promise;
  } finally {
    busy.count -= 1;
    if (busy.count === 0) {
      p.hidden = true;
    }
  }
}

/** Downloads `bytes` (a string or a Uint8Array) as `name`. */
export function download(bytes, name, type) {
  const blob = new Blob([bytes], { type });
  const url = URL.createObjectURL(blob);
  const a = el('a', { href: url, download: name });
  document.body.append(a);
  a.click();
  a.remove();
  setTimeout(() => URL.revokeObjectURL(url), 30000);
}
