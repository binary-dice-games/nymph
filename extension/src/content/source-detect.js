// MIT License © 2026 Binary Dice Games
/**
 * @file source-detect.js
 * @brief Cheap pre-check of whether the text of a chat's code block may be a
 * nymph source (spec/10_gemini_extension/DESIGN.md section 3.1). Only the
 * viewer's parser decides; this keeps ordinary YAML from reaching it.
 */

/** Largest text considered, in characters (DESIGN 3.1 rule 2). */
export const MAX_SOURCE_CHARS = 256 * 1024;

/** Language labels a nymph source may carry; an empty label is allowed too. */
const LABELS = new Set(['nymph', 'yaml', 'yml']);

/** Top-level keys a nymph source may start with (docs/format.md). */
const FIRST_KEYS = new Set(['nymph', 'description', 'image', 'data', 'source', 'root']);

/**
 * The first top-level key of a YAML text, skipping blank lines, comments and
 * a leading `---`.
 * @param {string} text
 * @return {string|null} the key, or null when there is none
 */
function firstKey(text) {
  for (const raw of text.split('\n')) {
    const line = raw.replace(/\r$/, '');
    if (line.trim() === '' || line.trimStart().startsWith('#') || line.trim() === '---') {
      continue;
    }
    const match = /^([A-Za-z_][\w-]*)\s*:(?:\s|$)/.exec(line);
    return match ? match[1] : null;
  }
  return null;
}

/**
 * Whether a code block's text may be a nymph source.
 * @param {string} text the block's text
 * @param {string} [label] the block's language label, if any
 * @return {boolean} true when the label, size and shape allow a nymph source
 */
export function looksLikeNymph(text, label = '') {
  const name = label.trim().toLowerCase();
  if (name !== '' && !LABELS.has(name)) {
    return false;
  }
  if (text.length === 0 || text.length > MAX_SOURCE_CHARS) {
    return false;
  }
  const key = firstKey(text);
  return key !== null && FIRST_KEYS.has(key) && /^root\s*:/m.test(text);
}

/**
 * Whether a source the viewer rejected deserves an error message and the fix
 * button (DESIGN 3.4): a `nymph` label, or `root:` / `nymph:` as first key,
 * so other YAML is never flagged.
 * @param {string} text the block's text
 * @param {string} [label] the block's language label, if any
 * @return {boolean}
 */
export function fixButtonWanted(text, label = '') {
  if (label.trim().toLowerCase() === 'nymph') {
    return true;
  }
  const key = firstKey(text);
  return key === 'root' || key === 'nymph';
}

/**
 * Whether a block is plainly meant as a nymph source, so that it is replaced
 * by the viewer even when the viewer rejects it (the host then shows the
 * error): a `nymph` label, or `nymph:` / `root:` as first key
 * (spec/10_gemini_extension/DESIGN.md section 3.1).
 * @param {string} text the block's text
 * @param {string} [label] the block's language label, if any
 * @return {boolean}
 */
export function claimsBlock(text, label = '') {
  return text.length > 0 && text.length <= MAX_SOURCE_CHARS && fixButtonWanted(text, label);
}

/**
 * A stable key for a block's text (FNV-1a, 32 bits, as 8 hex digits), for
 * caches that must notice a changed block.
 * @param {string} text
 * @return {string}
 */
export function hashText(text) {
  let hash = 0x811c9dc5;
  for (let i = 0; i < text.length; i++) {
    hash ^= text.charCodeAt(i);
    hash = Math.imul(hash, 0x01000193) >>> 0;
  }
  return hash.toString(16).padStart(8, '0');
}

/** The size of an image whose source says nothing (`docs/format.md`, `image`). */
export const DEFAULT_IMAGE_SIZE = Object.freeze({ width: 800, height: 500 });

/**
 * The `image` size a source asks for, read without parsing the YAML, to
 * shape the chart's box before the viewer has drawn it. Understands the flow
 * form (`image: {width: 640, height: 400}`) and the block form; anything else
 * gives the default. The viewer's own layout size replaces this once drawn.
 * @param {string} text the block's text
 * @return {{width: number, height: number}}
 */
export function sourceSize(text) {
  const flow = /^image\s*:\s*\{([^}\n]*)\}/m.exec(text);
  let body = flow ? flow[1] : '';
  if (!flow) {
    const block = /^image\s*:\s*\n((?:[ \t]+.*\n?)+)/m.exec(text);
    body = block ? block[1] : '';
  }
  const pick = (key, fallback) => {
    const v = Number(new RegExp(String.raw`(?:^|[\s,{])${key}\s*:\s*(\d+)`).exec(body)?.[1]);
    return v >= 16 && v <= 4096 ? v : fallback;
  };
  return { width: pick('width', DEFAULT_IMAGE_SIZE.width), height: pick('height', DEFAULT_IMAGE_SIZE.height) };
}
