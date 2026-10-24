// MIT License © 2026 Binary Dice Games
/**
 * @file device.js What this device can do (spec/15_web_app/DESIGN.md section
 * 3.10). Mobile is decided once, at load, from the device (no fine pointer
 * anywhere, or a screen narrower than 720 CSS px on its short side), never
 * from the window, so resizing a desktop window never takes the editor away.
 * The shell asks for capabilities, never for "mobile" itself.
 */

function detectMobile() {
  const override = new URLSearchParams(location.search).get('device');
  if (override === 'mobile' || override === 'desktop') {
    return override === 'mobile';
  }
  const fine = typeof matchMedia === 'function' && matchMedia('(any-pointer: fine)').matches;
  const shortSide = Math.min(screen.width || 0, screen.height || 0);
  return !fine || (shortSide > 0 && shortSide < 720);
}

const mobile = detectMobile();

function webgl2() {
  try {
    return !!document.createElement('canvas').getContext('webgl2');
  } catch {
    return false;
  }
}

/** The device's capabilities, fixed for the page's life. */
export const device = Object.freeze({
  mobile,
  /** The editor (mouse and keyboard, small targets): desktop only. */
  directEditing: !mobile,
  /** The editor's WebGL2. */
  webgl2: webgl2(),
  /** Save back to the opened file (Chromium's File System Access API). */
  saveInPlace: typeof window.showOpenFilePicker === 'function',
  /** Share links with the source in the fragment (deflate). */
  compression: typeof CompressionStream === 'function' && typeof DecompressionStream === 'function',
  /** Files from the OS ("Open with", installed app). */
  launchQueue: 'launchQueue' in window,
  /**
   * Agents the user runs reach the app (spec/17_agent_connectors): WebMCP
   * wherever the browser has it; the `nymph mcp` bridge on a computer only
   * (a phone runs no `nymph mcp`).
   */
  agents: Object.freeze({
    webmcp: typeof navigator !== 'undefined' && !!navigator.modelContext,
    bridge: !mobile,
  }),
});

/** Whether Edit can be offered, and why not. */
export function editAvailability() {
  if (!device.directEditing) {
    return { ok: false, why: 'Editing needs a computer with a mouse and a keyboard.' };
  }
  if (!device.webgl2) {
    return { ok: false, why: 'This browser cannot draw with WebGL2, which the editor needs.' };
  }
  return { ok: true };
}
