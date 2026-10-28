// MIT License © 2026 Binary Dice Games
/**
 * @file export.js Download PNG with drawings
 * (spec/18_collaboration/DESIGN.md section 6.7): the chart's PNG with the
 * whiteboard drawn over it, still carrying the chart's YAML (its `nymph`
 * iTXt chunk), so it reopens in nymph as the chart without the drawings.
 */

const SIGNATURE = [0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a];

/** The chunks of a PNG: [{type, bytes}] where bytes is the whole chunk (length, type, data, CRC). */
export function pngChunks(png) {
  if (png.length < 8 || SIGNATURE.some((b, i) => png[i] !== b)) {
    throw new Error('not a PNG');
  }
  const out = [];
  const view = new DataView(png.buffer, png.byteOffset, png.byteLength);
  let at = 8;
  while (at + 12 <= png.length) {
    const length = view.getUint32(at);
    const end = at + 12 + length;
    if (end > png.length) {
      throw new Error('a damaged PNG');
    }
    const type = String.fromCharCode(...png.subarray(at + 4, at + 8));
    out.push({ type, at, bytes: png.subarray(at, end) });
    at = end;
    if (type === 'IEND') {
      break;
    }
  }
  return out;
}

/** The `nymph` iTXt chunks of a PNG (whole). */
export function nymphChunks(png) {
  return pngChunks(png).filter((c) => {
    if (c.type !== 'iTXt') {
      return false;
    }
    const keyword = String.fromCharCode(...c.bytes.subarray(8, 8 + 5));
    return keyword === 'nymph' && c.bytes[13] === 0;
  });
}

/** `target` with `chunks` inserted after its IHDR chunk. */
export function withChunks(target, chunks) {
  const all = pngChunks(target);
  const ihdr = all[0];
  if (ihdr?.type !== 'IHDR') {
    throw new Error('a PNG without IHDR');
  }
  const cut = ihdr.at + ihdr.bytes.length;
  const extra = chunks.reduce((n, c) => n + c.bytes.length, 0);
  const out = new Uint8Array(target.length + extra);
  out.set(target.subarray(0, cut));
  let at = cut;
  for (const c of chunks) {
    out.set(c.bytes, at);
    at += c.bytes.length;
  }
  out.set(target.subarray(cut), at);
  return out;
}

function loadImage(url) {
  return new Promise((resolve, reject) => {
    const img = new Image();
    img.onload = () => resolve(img);
    img.onerror = () => reject(new Error('the drawings could not be drawn'));
    img.src = url;
  });
}

/**
 * The chart PNG with the drawings over it.
 * @param {Uint8Array} chartPng the chart as toPng() gives it (the whole figure)
 * @param {string} svg the whiteboard's toSvg() over `box`
 * @param {{x: number, y: number, width: number, height: number}} box the
 *        area shown, in layout pixels: the figure (0, 0, size) and any
 *        drawings outside it
 * @param {{width: number}} size the figure's layout size (the PNG's scale)
 * @returns {Promise<Uint8Array>} a PNG that carries the chart's YAML
 */
export async function pngWithDrawings(chartPng, svg, box, size) {
  const bitmap = await createImageBitmap(new Blob([chartPng], { type: 'image/png' }));
  const scale = bitmap.width / size.width;
  const canvas = document.createElement('canvas');
  canvas.width = Math.max(1, Math.ceil(box.width * scale));
  canvas.height = Math.max(1, Math.ceil(box.height * scale));
  const ctx = canvas.getContext('2d');
  ctx.fillStyle = '#ffffff';
  ctx.fillRect(0, 0, canvas.width, canvas.height);
  ctx.drawImage(bitmap, Math.round(-box.x * scale), Math.round(-box.y * scale));
  const url = URL.createObjectURL(new Blob([svg], { type: 'image/svg+xml' }));
  try {
    const img = await loadImage(url);
    ctx.drawImage(img, 0, 0, canvas.width, canvas.height);
  } finally {
    URL.revokeObjectURL(url);
  }
  const blob = await new Promise((resolve) => canvas.toBlob(resolve, 'image/png'));
  const drawn = new Uint8Array(await blob.arrayBuffer());
  return withChunks(drawn, nymphChunks(chartPng));
}
