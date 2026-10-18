// MIT License © 2026 Binary Dice Games
/**
 * @file frame.js
 * @brief The viewer inside an extension frame
 * (spec/05_browser_extension/DESIGN.md section 3.9).
 *
 * The content script sends a MessagePort with the first message; every
 * other message travels on that port, out of the page's sight:
 *
 *   in:  {type: 'show', png?: Uint8Array, source?: string, theme, fileName}
 *   out: {type: 'rendered'} | {type: 'error', message} | {type: 'yaml', yaml}
 *
 * `yaml` is sent after the chart is shown and after every pan or zoom, so
 * the content script always holds the chart's current view for parking.
 */

import { createViewer } from '../../viewer/nymph-viewer.js';

let port = null;

addEventListener('message', (e) => {
  if (port || e.data?.type !== 'nymph-frame-init' || !e.ports[0]) {
    return;
  }
  port = e.ports[0];
  port.onmessage = (m) => {
    if (m.data?.type === 'show') {
      show(m.data);
    }
  };
});

async function show({ png, source, theme, fileName }) {
  try {
    const viewer = await createViewer(document.getElementById('box'), {
      ...(png ? { png } : { source }),
      fit: 'scale',
      theme,
      fileName,
    });
    if (!viewer.info().hasFigure) {
      throw new Error('not a nymph source');
    }
    await viewer.rendered();
    const sendYaml = () => {
      try {
        port.postMessage({ type: 'yaml', yaml: viewer.toYaml() });
      } catch {
        // nothing to send
      }
    };
    viewer.on('view', sendYaml);
    port.postMessage({ type: 'rendered', description: viewer.info().description });
    sendYaml();
  } catch (e) {
    port.postMessage({ type: 'error', message: String(e?.message ?? e) });
  }
}
