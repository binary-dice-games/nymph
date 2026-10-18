// MIT License © 2026 Binary Dice Games
/**
 * @file sizing.js
 * @brief The host element's CSS, mirroring the image's sizing rules so the
 * chart takes the image's box and reflows as it would
 * (spec/05_browser_extension/DESIGN.md section 3.2).
 */

/** Computed properties copied from the image as they are. */
export const COPIED = [
  'marginTop', 'marginRight', 'marginBottom', 'marginLeft',
  'paddingTop', 'paddingRight', 'paddingBottom', 'paddingLeft',
  'borderTopWidth', 'borderRightWidth', 'borderBottomWidth', 'borderLeftWidth',
  'borderTopStyle', 'borderRightStyle', 'borderBottomStyle', 'borderLeftStyle',
  'borderTopColor', 'borderRightColor', 'borderBottomColor', 'borderLeftColor',
  'borderRadius', 'boxSizing', 'verticalAlign', 'cssFloat', 'maxWidth', 'maxHeight',
];

/** A width or height attribute as CSS: "400" is 400px, "50%" stays. */
function attrLength(value) {
  if (value == null || value.trim() === '') {
    return '';
  }
  const v = value.trim();
  return /^\d+(\.\d+)?$/.test(v) ? `${v}px` : /^\d+(\.\d+)?%$/.test(v) ? v : '';
}

/**
 * The host's style for an image.
 * @param {object} img what is known of the image
 * @param {string|null} img.attrWidth its `width` attribute
 * @param {string|null} img.attrHeight its `height` attribute
 * @param {string} img.inlineWidth its inline `style.width` ('' when none)
 * @param {string} img.inlineHeight its inline `style.height`
 * @param {number} img.naturalWidth
 * @param {number} img.naturalHeight
 * @param {Record<string, string>} img.computed its computed style (COPIED and `display`)
 * @return {Record<string, string>} CSS properties (camelCase) for the host
 */
export function hostStyle(img) {
  const style = {};
  for (const name of COPIED) {
    if (img.computed[name] !== undefined && img.computed[name] !== '') {
      style[name] = img.computed[name];
    }
  }
  const display = img.computed.display ?? 'inline';
  style.display = display === 'block' || display === 'flex' || display === 'grid' ? 'block' : 'inline-block';
  const width = img.inlineWidth || attrLength(img.attrWidth);
  const height = img.inlineHeight || attrLength(img.attrHeight);
  if (width) {
    style.width = width;
    style.height = height || 'auto';
  } else if (height) {
    style.width = 'auto';
    style.height = height;
  } else {
    style.width = `${img.naturalWidth}px`;
    style.height = 'auto';
  }
  if (img.naturalWidth > 0 && img.naturalHeight > 0) {
    style.aspectRatio = `${img.naturalWidth} / ${img.naturalHeight}`;
  }
  return style;
}
