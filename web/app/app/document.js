// MIT License © 2026 Binary Dice Games
/**
 * @file document.js The open document (spec/15_web_app/DESIGN.md section
 * 3.3): its text, where it came from, and whether it changed.
 */

/** The source without its top-level `view` key (zoom and pan). */
export function withoutView(source) {
  const lines = source.split('\n');
  const out = [];
  let skipping = false;
  for (const line of lines) {
    if (/^view\s*:/.test(line)) {
      skipping = true;
      continue;
    }
    if (skipping && (line === '' || /^[\s#-]/.test(line))) {
      continue; // the view's own lines (indented), blank lines and comments in it
    }
    skipping = false;
    out.push(line);
  }
  return out.join('\n').replace(/\n*$/, '\n');
}

/** A short, stable id for text (FNV-1a, hex). */
export function hashText(text) {
  let h = 0x811c9dc5;
  for (let i = 0; i < text.length; ++i) {
    h ^= text.charCodeAt(i);
    h = Math.imul(h, 0x01000193) >>> 0;
  }
  return h.toString(16).padStart(8, '0');
}

/** The file name without folders, or a default. */
export function baseName(name, fallback = 'chart.yaml') {
  const n = (name ?? '').split(/[\\/]/).pop().split('?')[0];
  return n || fallback;
}

/**
 * One document. `origin.kind`: local, url, drive, link, example, template,
 * paste, new. `source` is the YAML; a PNG input keeps `png: true` so Save
 * writes a PNG back.
 */
export class Doc {
  constructor({ name, origin, source, png = false, id }) {
    this.name = baseName(name, png ? 'chart.png' : 'chart.yaml');
    this.origin = origin;
    this.source = source;
    this.savedSource = source;
    this.png = png;
    this.id = id ?? Doc.idFor(origin, this.name, source);
  }

  /** The id recent files and drafts are kept under. */
  static idFor(origin, name, source) {
    switch (origin.kind) {
      case 'url':
        return `url:${origin.url}`;
      case 'drive':
        return `drive:${origin.fileId}`;
      case 'example':
        return `example:${origin.example}`;
      default:
        return `${origin.kind}:${name}:${hashText(source)}`;
    }
  }

  /** Changed since it was opened or saved; zoom and pan alone do not count. */
  get modified() {
    return withoutView(this.source) !== withoutView(this.savedSource);
  }

  /** Whether Save can write back to where it came from. */
  get savesInPlace() {
    return (this.origin.kind === 'local' && !!this.origin.handle) || this.origin.kind === 'drive';
  }

  /** The name with the extension the save writes. */
  fileName(kind) {
    const stem = this.name.replace(/\.(ya?ml|png)$/i, '');
    return `${stem}.${kind === 'png' ? 'png' : 'yaml'}`;
  }
}
