// MIT License © 2026 Binary Dice Games
/**
 * @file sniff.js
 * @brief Tells a nymph PNG from any other image by reading only its chunk
 * headers (spec/05_browser_extension/DESIGN.md section 4, sniffer).
 *
 * nymph writes its source in an `iTXt` chunk with keyword `nymph` right
 * after `IHDR` (spec/01_core_features/DESIGN.md section 4, `png`). The
 * sniffer walks chunk headers up to the first `IDAT` and stops: a PNG
 * without the chunk there is not a nymph PNG, and its pixels are never
 * read. The chunk's content is not parsed here: the viewer's
 * `setPng()` (the library's `read_source()`) does that, with CRC checks.
 */

/** Largest nymph PNG read in full. */
export const MAX_PNG_BYTES = 32 * 1024 * 1024;
/** The header walk gives up past this many bytes without reaching `IDAT`. */
export const MAX_HEADER_BYTES = 1024 * 1024;

const SIGNATURE = [0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a];
const KEYWORD = [0x6e, 0x79, 0x6d, 0x70, 0x68, 0x00]; // "nymph\0"
const MAX_CHUNK_LENGTH = 0x7fffffff; // PNG's limit

/**
 * Incremental chunk-header walker. Feed it the stream's bytes in order;
 * it answers as soon as it can.
 */
export class PngSniffer {
  /**
   * @param {object} [limits]
   * @param {number} [limits.maxHeaderBytes] see MAX_HEADER_BYTES
   */
  constructor(limits = {}) {
    this.maxHeaderBytes = limits.maxHeaderBytes ?? MAX_HEADER_BYTES;
    /** @type {Uint8Array} bytes received so far */
    this.bytes = new Uint8Array(0);
    /** Offset of the next chunk header. */
    this.at = SIGNATURE.length;
    /** @type {'more'|'nymph'|'no'} */
    this.verdict = 'more';
    /** Why the answer is "no" (for tests and logs). */
    this.reason = '';
  }

  /**
   * Adds bytes and walks as far as they allow.
   * @param {Uint8Array} chunk the next bytes of the stream
   * @return {'more'|'nymph'|'no'} 'more' while undecided
   */
  push(chunk) {
    if (this.verdict !== 'more') {
      return this.verdict;
    }
    const joined = new Uint8Array(this.bytes.length + chunk.length);
    joined.set(this.bytes);
    joined.set(chunk, this.bytes.length);
    this.bytes = joined;
    return this.walk();
  }

  /**
   * The stream ended: undecided means "no".
   * @return {'nymph'|'no'}
   */
  end() {
    if (this.verdict === 'more') {
      this.no('ended before IDAT');
    }
    return this.verdict;
  }

  /** @private */
  no(reason) {
    this.verdict = 'no';
    this.reason = reason;
    return this.verdict;
  }

  /** @private */
  walk() {
    const b = this.bytes;
    if (b.length < SIGNATURE.length) {
      return SIGNATURE.every((v, i) => i >= b.length || b[i] === v) ? 'more' : this.no('not a PNG');
    }
    if (!SIGNATURE.every((v, i) => b[i] === v)) {
      return this.no('not a PNG');
    }
    for (;;) {
      if (this.at > this.maxHeaderBytes) {
        return this.no('no IDAT within the header limit');
      }
      if (b.length < this.at + 8) {
        return 'more';
      }
      const length = ((b[this.at] << 24) | (b[this.at + 1] << 16) | (b[this.at + 2] << 8) | b[this.at + 3]) >>> 0;
      const type = String.fromCharCode(b[this.at + 4], b[this.at + 5], b[this.at + 6], b[this.at + 7]);
      if (length > MAX_CHUNK_LENGTH) {
        return this.no('bad chunk length');
      }
      if (type === 'IDAT' || type === 'IEND') {
        return this.no('no nymph chunk before IDAT');
      }
      if (type === 'iTXt') {
        const need = Math.min(length, KEYWORD.length);
        if (b.length < this.at + 8 + need) {
          return 'more';
        }
        const data = this.at + 8;
        if (length >= KEYWORD.length && KEYWORD.every((v, i) => b[data + i] === v)) {
          this.verdict = 'nymph';
          return this.verdict;
        }
      }
      this.at += 12 + length;
    }
  }
}

/**
 * Reads a stream and decides whether it is a nymph PNG. A "no" cancels the
 * stream right away; a "yes" reads it to the end.
 * @param {ReadableStream<Uint8Array>} stream the image's body
 * @param {object} [limits]
 * @param {number} [limits.maxBytes] see MAX_PNG_BYTES
 * @param {number} [limits.maxHeaderBytes] see MAX_HEADER_BYTES
 * @return {Promise<{nymph: boolean, bytes?: Uint8Array, reason?: string}>}
 *   the whole file when it is a nymph PNG; never rejects for bad bytes
 *   (a read error rejects)
 */
export async function sniffStream(stream, limits = {}) {
  const maxBytes = limits.maxBytes ?? MAX_PNG_BYTES;
  const sniffer = new PngSniffer(limits);
  const reader = stream.getReader();
  let verdict = 'more';
  const parts = [];
  let total = 0;
  try {
    for (;;) {
      const { done, value } = await reader.read();
      if (done) {
        break;
      }
      total += value.length;
      if (total > maxBytes) {
        await reader.cancel();
        return { nymph: false, reason: 'too large' };
      }
      if (verdict === 'more') {
        verdict = sniffer.push(value);
        if (verdict === 'no') {
          await reader.cancel();
          return { nymph: false, reason: sniffer.reason };
        }
        if (verdict === 'nymph') {
          parts.push(sniffer.bytes);
          continue;
        }
      } else {
        parts.push(value);
      }
    }
  } finally {
    reader.releaseLock();
  }
  if (verdict === 'more') {
    return { nymph: false, reason: (sniffer.end(), sniffer.reason) };
  }
  const bytes = new Uint8Array(total);
  let at = 0;
  for (const part of parts) {
    bytes.set(part, at);
    at += part.length;
  }
  return { nymph: true, bytes };
}

/**
 * The same decision for bytes already in memory.
 * @param {Uint8Array} bytes a whole file
 * @return {boolean} whether it is a nymph PNG
 */
export function isNymphPng(bytes) {
  return new PngSniffer().push(bytes) === 'nymph';
}
