// MIT License © 2026 Binary Dice Games
/**
 * @file share.js Links that reopen a document (spec/15_web_app/DESIGN.md
 * section 3.6): the source itself in the fragment (`#y=`, deflate-raw and
 * base64url, never sent to a server), or the URL, example or Drive file it
 * came from.
 */

/** Fragments longer than this get a warning: chat apps may cut them. */
export const LONG_LINK = 16 * 1024;

function toBase64Url(bytes) {
  let s = '';
  for (let i = 0; i < bytes.length; i += 0x8000) {
    s += String.fromCharCode(...bytes.subarray(i, i + 0x8000));
  }
  return btoa(s).replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/, '');
}

function fromBase64Url(text) {
  const s = atob(text.replace(/-/g, '+').replace(/_/g, '/'));
  const out = new Uint8Array(s.length);
  for (let i = 0; i < s.length; ++i) {
    out[i] = s.charCodeAt(i);
  }
  return out;
}

async function pipe(bytes, stream) {
  const out = new Blob([bytes]).stream().pipeThrough(stream);
  return new Uint8Array(await new Response(out).arrayBuffer());
}

/** The `y=` fragment value of `source`. */
export async function encodeSource(source) {
  return toBase64Url(await pipe(new TextEncoder().encode(source), new CompressionStream('deflate-raw')));
}

/** The source of a `y=` fragment value. */
export async function decodeSource(value) {
  const bytes = await pipe(fromBase64Url(value), new DecompressionStream('deflate-raw'));
  return new TextDecoder().decode(bytes);
}

/** The app's base URL (where index.html is). */
export function appBase() {
  return new URL('.', location.href.split('#')[0].split('?')[0]).href;
}

/**
 * The link that reopens `doc`, as {url, kind, long}: `?url=`, `?example=`,
 * `?drive=`, or `#y=` with the source.
 */
export async function linkFor(doc, source, mode) {
  const base = appBase();
  const edit = mode === 'edit' ? '&mode=edit' : '';
  const o = doc.origin;
  if (o.kind === 'url' && !doc.modified) {
    return { url: `${base}?url=${encodeURIComponent(o.url)}${edit}`, kind: 'url', long: false };
  }
  if (o.kind === 'example' && !doc.modified) {
    return { url: `${base}?example=${encodeURIComponent(o.example)}${edit}`, kind: 'example', long: false };
  }
  if (o.kind === 'drive' && !doc.modified) {
    return { url: `${base}?drive=${encodeURIComponent(o.fileId)}${edit}`, kind: 'drive', long: false };
  }
  const y = await encodeSource(source);
  return { url: `${base}${edit ? '?mode=edit' : ''}#y=${y}`, kind: 'source', long: y.length > LONG_LINK };
}

/** The snippet that embeds a chart in a page with this site's viewer. */
export function embedCode(source) {
  const script = new URL('nymph-viewer.js', appBase()).href;
  const escaped = source.replace(/&/g, '&amp;').replace(/</g, '&lt;');
  return `<script type="module" src="${script}"></script>\n<nymph-viewer>\n${escaped}</nymph-viewer>\n`;
}
