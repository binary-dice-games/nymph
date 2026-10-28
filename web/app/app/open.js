// MIT License © 2026 Binary Dice Games
/**
 * @file open.js Where files come from (spec/15_web_app/DESIGN.md section
 * 3.4): the file picker, drop, paste and URLs. Everything ends in bytes
 * and a name; the module never sees the network.
 */

/** Files larger than this are refused (the library's own limits are lower). */
export const MAX_BYTES = 16 * 1024 * 1024;

export class OpenError extends Error {
  constructor(message, kind = 'open') {
    super(message);
    this.kind = kind;
  }
}

/** Whether `bytes` start with the PNG signature. */
export function isPng(bytes) {
  const sig = [0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a];
  return !!bytes && bytes.length >= 8 && sig.every((b, i) => bytes[i] === b);
}

/** {png, text} of a file's bytes: a PNG, or UTF-8 text. */
export function sniff(bytes) {
  if (bytes.length > MAX_BYTES) {
    throw new OpenError(`the file is ${(bytes.length / 1048576).toFixed(1)} MB; the most is 16 MB`, 'size');
  }
  if (isPng(bytes)) {
    return { png: true };
  }
  let text;
  try {
    text = new TextDecoder('utf-8', { fatal: true }).decode(bytes);
  } catch {
    throw new OpenError('this is neither a nymph YAML file (UTF-8 text) nor a PNG', 'type');
  }
  if (text.charCodeAt(0) === 0xfeff) {
    text = text.slice(1);
  }
  return { png: false, text };
}

/**
 * The URL to fetch for a page URL people copy: a GitHub `blob/` page and a
 * gist page become their raw files (which allow other sites to read them).
 */
export function rawUrl(input) {
  let u;
  try {
    u = new URL(input.trim());
  } catch {
    throw new OpenError('that is not a URL', 'url');
  }
  if (u.protocol !== 'https:' && u.protocol !== 'http:') {
    throw new OpenError('only http and https URLs can be opened', 'url');
  }
  if (u.hostname === 'github.com') {
    const m = u.pathname.match(/^\/([^/]+)\/([^/]+)\/(?:blob|raw)\/(.+)$/);
    if (m) {
      return `https://raw.githubusercontent.com/${m[1]}/${m[2]}/${m[3]}`;
    }
  }
  if (u.hostname === 'gist.github.com') {
    const m = u.pathname.match(/^\/([^/]+)\/([0-9a-f]+)\/?$/i);
    if (m) {
      return `https://gist.githubusercontent.com/${m[1]}/${m[2]}/raw`;
    }
  }
  return u.href;
}

/** Fetches a URL without credentials: {bytes, name, url}. */
export async function fetchUrl(input) {
  const url = rawUrl(input);
  let response;
  try {
    response = await fetch(url, { credentials: 'omit', redirect: 'follow', cache: 'no-cache' });
  } catch {
    throw new OpenError(
      "that server does not let other sites read the file (or it cannot be reached). Download it and open it here.",
      'cors',
    );
  }
  if (!response.ok) {
    throw new OpenError(`the server answered ${response.status} ${response.statusText}`.trim(), 'http');
  }
  const length = Number(response.headers.get('content-length') ?? 0);
  if (length > MAX_BYTES) {
    throw new OpenError(`the file is ${(length / 1048576).toFixed(1)} MB; the most is 16 MB`, 'size');
  }
  const bytes = new Uint8Array(await response.arrayBuffer());
  const name = decodeURIComponent(new URL(response.url || url).pathname.split('/').pop() || 'chart.yaml');
  return { bytes, name, url: input.trim() };
}

/** Bytes of a File. */
export async function readFile(file) {
  if (file.size > MAX_BYTES) {
    throw new OpenError(`the file is ${(file.size / 1048576).toFixed(1)} MB; the most is 16 MB`, 'size');
  }
  return new Uint8Array(await file.arrayBuffer());
}

/**
 * Asks for a file: the File System Access picker where there is one (it
 * gives a handle to save back to), else the file input. Resolves with
 * {file, handle} or null when cancelled.
 */
export async function pickFile(input) {
  if (typeof window.showOpenFilePicker === 'function') {
    try {
      const [handle] = await window.showOpenFilePicker({
        types: [
          { description: 'nymph charts', accept: { 'text/yaml': ['.yaml', '.yml'], 'image/png': ['.png'] } },
        ],
        excludeAcceptAllOption: false,
        multiple: false,
      });
      return { file: await handle.getFile(), handle };
    } catch (e) {
      if (e.name === 'AbortError') {
        return null;
      }
      // Not allowed here (a frame, a policy): the plain input.
    }
  }
  return new Promise((resolve) => {
    const done = () => {
      const file = input.files?.[0];
      input.value = '';
      resolve(file ? { file, handle: null } : null);
    };
    input.addEventListener('change', done, { once: true });
    input.addEventListener('cancel', () => resolve(null), { once: true });
    input.click();
  });
}

/** Writes `bytes` through a FileSystemFileHandle, asking for permission. */
export async function writeHandle(handle, bytes) {
  if ((await handle.queryPermission?.({ mode: 'readwrite' })) !== 'granted') {
    if ((await handle.requestPermission?.({ mode: 'readwrite' })) !== 'granted') {
      throw new OpenError('permission to write the file was not given', 'permission');
    }
  }
  const w = await handle.createWritable();
  await w.write(bytes);
  await w.close();
}

/**
 * The nymph source a PNG carries (its `nymph` iTXt chunk), or null. Read
 * here, not by the module, so a source that does not load can still be
 * reported.
 */
export async function pngSource(bytes) {
  if (!isPng(bytes)) {
    return null;
  }
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  for (let at = 8; at + 12 <= bytes.length; ) {
    const length = view.getUint32(at);
    const type = String.fromCharCode(...bytes.subarray(at + 4, at + 8));
    const data = bytes.subarray(at + 8, at + 8 + length);
    if (type === 'iTXt') {
      const keywordEnd = data.indexOf(0);
      const keyword = new TextDecoder().decode(data.subarray(0, keywordEnd));
      if (keyword === 'nymph') {
        const compressed = data[keywordEnd + 1] !== 0;
        let pos = keywordEnd + 3;
        for (let field = 0; field < 2; ++field) {
          pos = data.indexOf(0, pos) + 1; // language tag, translated keyword
        }
        let payload = data.subarray(pos);
        if (compressed) {
          const stream = new Blob([payload]).stream().pipeThrough(new DecompressionStream('deflate'));
          payload = new Uint8Array(await new Response(stream).arrayBuffer());
        }
        return new TextDecoder().decode(payload);
      }
    }
    if (type === 'IEND') {
      break;
    }
    at += 12 + length;
  }
  return null;
}
