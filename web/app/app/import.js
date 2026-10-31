// MIT License © 2026 Binary Dice Games
/**
 * @file import.js Import a CSV table or a mermaid diagram as a new chart.
 *
 * The pure part (no DOM) parses CSV and writes nymph YAML: a chart of the
 * kind picked (line, area, bars, stacked, horizontal bars, scatter, pie,
 * histogram) laid out like a report page, with a title box in a heading
 * font, a subtitle, optional summary cards, the plot, a separator line and
 * a footnote (see examples/fonts.yaml). A mermaid diagram is converted by
 * the viewer module and then framed the same way.
 *
 * The wizard (importWizard) walks the user through the steps with a live
 * preview and resolves with the source to open.
 */

// ── CSV ───────────────────────────────────────────────────────────────────────

/** The delimiter of a CSV text: the one of , ; and tab its first line uses most. */
export function detectDelimiter(text) {
  const first = text.split(/\r?\n/).find((l) => l.trim() !== '') ?? '';
  let best = ',';
  let most = 0;
  for (const d of [',', ';', '\t']) {
    let n = 0;
    let quoted = false;
    for (const c of first) {
      if (c === '"') {
        quoted = !quoted;
      } else if (c === d && !quoted) {
        n++;
      }
    }
    if (n > most) {
      most = n;
      best = d;
    }
  }
  return best;
}

/**
 * Parses a CSV text (RFC 4180 quoting; comma, semicolon or tab).
 * @param {string} text the file's text
 * @returns {{header: string[], rows: string[][], delimiter: string}} names
 *   made unique and never empty; every row as long as the header
 * @throws {Error} when there is no header or no data row
 */
export function parseCsv(text) {
  text = text.replace(/^﻿/, '');
  const delimiter = detectDelimiter(text);
  const records = [];
  let row = [];
  let cell = '';
  let quoted = false;
  let wasQuoted = false;
  const endCell = () => {
    row.push(wasQuoted ? cell : cell.trim());
    cell = '';
    wasQuoted = false;
  };
  const endRow = () => {
    endCell();
    if (row.some((c) => c !== '')) {
      records.push(row);
    }
    row = [];
  };
  for (let i = 0; i < text.length; i++) {
    const c = text[i];
    if (quoted) {
      if (c === '"' && text[i + 1] === '"') {
        cell += '"';
        i++;
      } else if (c === '"') {
        quoted = false;
      } else {
        cell += c;
      }
    } else if (c === '"' && cell.trim() === '') {
      quoted = true;
      wasQuoted = true;
      cell = '';
    } else if (c === delimiter) {
      endCell();
    } else if (c === '\n') {
      endRow();
    } else if (c !== '\r') {
      cell += c;
    }
  }
  endRow();
  if (records.length < 2) {
    throw new Error('the CSV file needs a header row and at least one row of data');
  }
  const seen = new Set();
  const header = records[0].map((name, i) => {
    let n = name.replace(/\s+/g, ' ').trim() || `column ${i + 1}`;
    for (let k = 2; seen.has(n); k++) {
      n = `${name || 'column'} ${k}`;
    }
    seen.add(n);
    return n;
  });
  const rows = records.slice(1).map((r) => header.map((_, i) => (r[i] ?? '').replace(/\s*\r?\n\s*/g, ' ')));
  return { header, rows, delimiter };
}

/**
 * A cell as a number: "1,234.5", "$12", "45 %", and "1,5" when the file
 * uses decimal commas. NaN when it is not a number.
 */
export function parseNumber(cell, decimalComma = false) {
  let s = String(cell).trim().replace(/^[$€£¥]\s*|\s*[$€£¥]$/g, '').replace(/\s*%$/, '').replace(/[\s ]/g, '');
  if (decimalComma) {
    s = s.replace(/\.(?=\d{3}(\D|$))/g, '').replace(',', '.');
  } else if (/^[-+]?\d{1,3}(,\d{3})+(\.\d+)?$/.test(s)) {
    s = s.replace(/,/g, '');
  }
  return /^[-+]?(\d+\.?\d*|\.\d+)(e[-+]?\d+)?$/i.test(s) ? Number(s) : NaN;
}

/**
 * The columns of a parsed CSV: name, whether every non-empty cell is a
 * number, and the numbers (null for an empty cell).
 */
export function analyzeTable(table) {
  const decimalComma = table.delimiter === ';' && table.rows.some((r) => r.some((c) => /^-?\d+,\d+$/.test(c)));
  const columns = table.header.map((name, index) => {
    const cells = table.rows.map((r) => r[index]);
    const filled = cells.filter((c) => c !== '');
    const numbers = cells.map((c) => (c === '' ? null : parseNumber(c, decimalComma)));
    const numeric = filled.length > 0 && numbers.every((n) => n === null || Number.isFinite(n));
    return { name, index, numeric, numbers: numeric ? numbers : null, cells };
  });
  return { ...table, decimalComma, columns };
}

// ── Chart kinds ───────────────────────────────────────────────────────────────

/**
 * The charts a CSV can become. `x` names the role of the first mapping
 * (null: none), `series` how many value columns it takes.
 */
export const CHART_KINDS = [
  { id: 'line', title: 'Line chart', note: 'A trend along an ordered axis', x: 'X axis', series: 'many' },
  { id: 'area', title: 'Area chart', note: 'A trend, with its volume filled', x: 'X axis', series: 'many' },
  { id: 'bars', title: 'Bar chart', note: 'Compare categories, series side by side', x: 'Categories', series: 'many' },
  { id: 'stacked', title: 'Stacked bars', note: 'The parts of a total per category', x: 'Categories', series: 'many' },
  { id: 'hbars', title: 'Horizontal bars', note: 'Rankings and long category names', x: 'Categories', series: 'many' },
  { id: 'scatter', title: 'Scatter plot', note: 'How two measures relate', x: 'X axis', series: 'many' },
  { id: 'pie', title: 'Pie chart', note: 'The shares of one total', x: 'Slice names', series: 'one' },
  { id: 'histogram', title: 'Histogram', note: 'How one measure is distributed', x: null, series: 'one' },
];

/** The kind a table suits best: a line for numeric X, bars for categories. */
export function suggestKind(info) {
  const numeric = info.columns.filter((c) => c.numeric);
  const text = info.columns.filter((c) => !c.numeric);
  if (numeric.length === 0) {
    throw new Error('the CSV file has no column of numbers to chart');
  }
  if (numeric.length === 1 && text.length === 0) {
    return 'histogram';
  }
  if (text.length > 0) {
    return info.rows.length > 16 ? 'line' : 'bars';
  }
  return 'line';
}

/**
 * The default mapping of columns for a chart kind: {x, series} with column
 * indexes; x is -1 for the row number.
 */
export function defaultMapping(kind, info) {
  const numeric = info.columns.filter((c) => c.numeric).map((c) => c.index);
  const text = info.columns.filter((c) => !c.numeric).map((c) => c.index);
  const def = CHART_KINDS.find((k) => k.id === kind);
  let x = -1;
  if (def.x) {
    if (kind === 'scatter') {
      x = numeric.length > 1 ? numeric[0] : -1;
    } else {
      x = text[0] ?? (numeric.length > 1 ? numeric[0] : -1);
    }
  }
  let series = numeric.filter((i) => i !== x);
  // Columns far smaller than the largest would lie flat on its axis.
  const peak = (i) => Math.max(0, ...info.columns[i].numbers.map((v) => Math.abs(v ?? 0)));
  const top = Math.max(0, ...series.map(peak));
  series = series.filter((i) => peak(i) * 20 >= top);
  if (series.length === 0) {
    series = numeric.slice(0, 1);
  }
  if (def.series === 'one' || (kind === 'scatter' && x >= 0)) {
    series = series.slice(0, 1);
  }
  return { x, series: series.slice(0, 8) };
}

// ── Look ──────────────────────────────────────────────────────────────────────

/** Font sets: heading, subtitle, body, numbers and the plot's text. */
export const TYPE_STYLES = [
  {
    id: 'editorial',
    title: 'Editorial',
    note: 'Serif heading, sans text',
    css: "'nymph PT Serif', Georgia, serif",
    fonts: { title: 'PT_Serif-Web-Bold.ttf', subtitle: 'PT_Serif-Web-Italic.ttf', body: 'FiraSans-Regular.ttf', strong: 'FiraSans-Bold.ttf', number: 'PT_Serif-Web-Bold.ttf', note: 'FiraSans-Italic.ttf', plot: '' },
  },
  {
    id: 'modern',
    title: 'Modern',
    note: 'Fira Sans throughout',
    css: "'nymph Fira Sans', system-ui, sans-serif",
    fonts: { title: 'FiraSans-Bold.ttf', subtitle: 'FiraSans-Italic.ttf', body: 'FiraSans-Regular.ttf', strong: 'FiraSans-Bold.ttf', number: 'FiraSans-Bold.ttf', note: 'FiraSans-Italic.ttf', plot: '' },
  },
  {
    id: 'technical',
    title: 'Technical',
    note: 'Monospaced heading and numbers',
    css: "'nymph Fira Mono', ui-monospace, monospace",
    fonts: { title: 'FiraMono-Bold.ttf', subtitle: 'FiraSans-Italic.ttf', body: 'FiraSans-Regular.ttf', strong: 'FiraMono-Bold.ttf', number: 'FiraMono-Bold.ttf', note: 'FiraSans-Italic.ttf', plot: 'FiraMono-Regular.ttf' },
  },
];

/** Image sizes: width and height in pixels; `auto` keeps a diagram's own. */
export const SIZES = [
  { id: 'wide', title: 'Wide', width: 960, height: 600 },
  { id: 'standard', title: 'Standard', width: 800, height: 520 },
  { id: 'slide', title: 'Slide', width: 1280, height: 720 },
  { id: 'square', title: 'Square', width: 720, height: 720 },
];

/** Headers: a filled text box, a title over a rule, or plain text. */
export const HEADERS = [
  { id: 'box', title: 'Text box' },
  { id: 'rule', title: 'Title and rule' },
  { id: 'plain', title: 'Plain' },
];

/** Legend places. */
export const LEGENDS = [
  { id: 'top', title: 'Above' },
  { id: 'right', title: 'Right' },
  { id: 'inside', title: 'Inside' },
  { id: 'none', title: 'None' },
];

/** The look a new chart starts with. */
export function defaultLook(name, kind = '') {
  const base = String(name ?? '').replace(/\.[^.]*$/, '').replace(/[_-]+/g, ' ').trim();
  return {
    title: base ? base[0].toUpperCase() + base.slice(1) : 'Chart',
    subtitle: '',
    note: name ? `Source: ${name}` : '',
    theme: 'light',
    type: 'editorial',
    header: 'box',
    size: kind === 'mermaid' ? 'auto' : 'wide',
    legend: 'top',
    cards: false,
    xLabel: '',
    yLabel: '',
    labels: {}, // series legend names by column index
  };
}

// ── YAML writing ──────────────────────────────────────────────────────────────

const RESERVED = /^(true|false|yes|no|on|off|null|~|y|n)$/i;

/** A text field's value: plain when safe, else double-quoted; a leading $ doubled. */
export function yamlText(text) {
  const s = String(text ?? '').replace(/^\$/, '$$$$');
  if (/^[A-Za-z][A-Za-z0-9 _.()/%+-]*$/.test(s) && !/\s$/.test(s) && !RESERVED.test(s)) {
    return s;
  }
  return JSON.stringify(s);
}

/** A reference to a column: $name, "$long name", or $N when the name would mislead. */
export function columnRef(column) {
  if (/^[A-Za-z_][A-Za-z0-9_]*$/.test(column.name)) {
    return `$${column.name}`;
  }
  if (/[:$]/.test(column.name) || /^\d+$/.test(column.name)) {
    return `$${column.index + 1}`;
  }
  return JSON.stringify(`$${column.name}`);
}

/** One CSV cell for the `data` block. */
function csvCell(text) {
  const s = String(text);
  return /[",]|^\s|\s$/.test(s) ? `"${s.replace(/"/g, '""')}"` : s;
}

/** A number as a short text: 12.5, 1,234, 0.031. */
export function formatNumber(n) {
  if (!Number.isFinite(n)) {
    return '–';
  }
  const abs = Math.abs(n);
  const digits = abs >= 100 ? 0 : abs >= 10 ? 1 : abs >= 1 ? 2 : 3;
  return n.toLocaleString('en-US', { maximumFractionDigits: digits });
}

/** A flow mapping: {type: Label, text: x}. */
function flow(fields) {
  return `{${Object.entries(fields)
    .filter(([, v]) => v !== undefined && v !== null && v !== '')
    .map(([k, v]) => `${k}: ${v}`)
    .join(', ')}}`;
}

/** Indents every line of `text` by `n` spaces. */
function indent(text, n) {
  const pad = ' '.repeat(n);
  return text
    .split('\n')
    .map((l) => (l === '' ? l : pad + l))
    .join('\n');
}

const MUTED = { text_color_light: '"#5F6B7D"', text_color_dark: '"#A3ADBD"' };

/** The heading: title and subtitle in a text box, over a rule, or plain. */
function headerYaml(look, fonts, width) {
  const big = width >= 1100 ? 34 : width >= 860 ? 30 : 26;
  const items = [];
  if (look.title) {
    items.push(`- ${flow({ type: 'Label', text: yamlText(look.title), font: fonts.title, font_size: big })}`);
  }
  if (look.subtitle) {
    items.push(`- ${flow({ type: 'Label', text: yamlText(look.subtitle), font: fonts.subtitle, font_size: Math.round(big * 0.55), wrap: 'true', ...MUTED })}`);
  }
  if (items.length === 0) {
    return [];
  }
  if (look.header === 'box') {
    return [`- type: VerticalLayout\n  card: true\n  padding: 16\n  spacing: 4\n  children:\n${indent(items.join('\n'), 4)}`];
  }
  if (look.header === 'rule') {
    return [...items, '- {type: Separator}'];
  }
  return items;
}

/** The footnote: a rule, then the note on the left and `right` on the right. */
function footerYaml(look, fonts, right) {
  if (!look.note && !right) {
    return [];
  }
  const row = [
    look.note ? `- ${flow({ type: 'Label', text: yamlText(look.note), font: fonts.note, font_size: 13, ...MUTED })}` : null,
    '- {type: Spring}',
    right ? `- ${flow({ type: 'Label', text: yamlText(right), font: fonts.body, font_size: 13, ...MUTED })}` : null,
  ].filter(Boolean);
  return ['- {type: Separator}', `- type: HorizontalLayout\n  children:\n${indent(row.join('\n'), 4)}`];
}

/** The image line. */
function imageYaml(width, height, look, fonts) {
  return `image: ${flow({ width, height, theme: look.theme === 'light' ? '' : look.theme, padding: 14, font: fonts.body, font_size: 15 })}`;
}

/** The legend fields of a Plot. */
function legendFields(legend) {
  return {
    top: ['legend_location: North', 'legend_flags: Outside|Horizontal'],
    right: ['legend_location: East', 'legend_flags: Outside'],
    inside: ['legend_location: NorthEast'],
    none: [],
  }[legend];
}

/**
 * Category ticks: every label while they fit in `room` pixels, then every
 * k-th (a label takes about 8 px per character on the X axis, 24 px on Y).
 */
function categoryTicks(axis, cells, room) {
  const n = cells.length;
  const longest = Math.max(1, ...cells.map((c) => String(c).length));
  const fit = Math.max(2, Math.floor(room / (axis === 'x' ? longest * 8 + 24 : 24)));
  const step = Math.max(1, Math.ceil(n / fit));
  const at = [];
  for (let i = 0; i < n; i += step) {
    at.push(i);
  }
  return [`${axis}_ticks: [${at.join(', ')}]`, `${axis}_tick_labels: [${at.map((i) => JSON.stringify(cells[i] || ' ')).join(', ')}]`];
}

/** The summary cards: one text box per series with its headline number. */
function cardsYaml(kind, info, mapping, look, fonts, palette) {
  const series = mapping.series.map((i) => info.columns[i]);
  if (series.length === 0 || series.length > 4) {
    return [];
  }
  const total = (c) => c.numbers.reduce((a, n) => a + (n ?? 0), 0);
  const cards = series.map((c, k) => {
    const values = c.numbers.filter((n) => n !== null);
    let value;
    let detail;
    if (kind === 'line' || kind === 'area' || kind === 'scatter') {
      value = values.at(-1);
      const first = values[0];
      const xCells = mapping.x >= 0 ? info.columns[mapping.x].cells : null;
      const from = xCells ? xCells[c.numbers.indexOf(first)] : 'the first row';
      detail = values.length > 1 && first !== 0 ? `${value >= first ? '+' : ''}${formatNumber(((value - first) / Math.abs(first)) * 100)}% since ${from}` : 'latest value';
      if (kind === 'scatter') {
        value = values.reduce((a, n) => a + n, 0) / values.length;
        detail = `mean of ${values.length} points`;
      }
    } else if (kind === 'histogram') {
      const sorted = [...values].sort((a, b) => a - b);
      value = sorted[Math.floor(sorted.length / 2)];
      detail = `median of ${values.length} values`;
    } else if (kind === 'pie') {
      const t = total(c);
      const top = values.indexOf(Math.max(...values));
      value = t;
      const name = mapping.x >= 0 ? info.columns[mapping.x].cells[c.numbers.indexOf(values[top])] : `row ${top + 1}`;
      detail = `largest: ${name}, ${formatNumber((values[top] / t) * 100)}%`;
    } else {
      value = total(c);
      detail = `total over ${values.length} ${values.length === 1 ? 'category' : 'categories'}`;
    }
    const color = palette?.[k % palette.length];
    const caption = (look.labels[c.index] || c.name).toUpperCase();
    return `- type: VerticalLayout\n  card: true\n  width: -1\n  padding: 12\n  spacing: 2\n  children:\n${indent(
      [
        `- ${flow({ type: 'Label', text: yamlText(caption), font: fonts.strong, font_size: 12, text_color: color ? JSON.stringify(color) : '' , ...(color ? {} : MUTED) })}`,
        `- ${flow({ type: 'Label', text: yamlText(formatNumber(value)), font: fonts.number, font_size: 28 })}`,
        `- ${flow({ type: 'Label', text: yamlText(detail), font_size: 12, ...MUTED })}`,
      ].join('\n'),
      4,
    )}`;
  });
  return [`- type: HorizontalLayout\n  spacing: 10\n  children:\n${indent(cards.join('\n'), 4)}`];
}

/** ImPlot's Deep colours, the light theme's series palette. */
const DEEP = ['#4C72B0', '#DD8452', '#55A868', '#C44E52', '#8172B3', '#937860', '#DA8BC3', '#8C8C8C', '#CCB974', '#64B5CD'];

/** A number with 4 significant digits, for axis limits. */
const round4 = (v) => Number(v.toPrecision(4));

/** The range of `values` with `pad` of its span added on each side; from 0 when `zero` and all are positive. */
function padRange(values, pad, zero = false) {
  let lo = Math.min(...values);
  let hi = Math.max(...values);
  if (lo === hi) {
    lo -= Math.abs(lo) * 0.1 || 1;
    hi += Math.abs(hi) * 0.1 || 1;
  }
  const span = hi - lo;
  let a = lo - span * pad;
  if (zero && lo >= 0) {
    a = 0;
  } else if (lo >= 0 && a < 0) {
    a = 0;
  }
  return [round4(a), round4(hi + span * pad)];
}

/** The numbers of some columns, gaps left out. */
const numbersOf = (columns) => columns.flatMap((c) => c.numbers.filter((v) => v !== null));

/**
 * The Plot element of a CSV chart. `extra` receives derived data columns
 * ({name, numbers}) the plot refers to (the shares of a pie).
 */
function plotYaml(kind, info, mapping, look, fonts, palette, extra, size) {
  const xCol = mapping.x >= 0 ? info.columns[mapping.x] : null;
  const series = mapping.series.map((i) => info.columns[i]);
  const label = (c) => yamlText(look.labels[c.index] || c.name);
  const n = info.rows.length;
  const fields = ['type: Plot', 'title: "##chart"', 'height: -1'];
  if (fonts.plot) {
    fields.push(`font: ${fonts.plot}`);
  }
  const children = [];
  // A single series needs no legend above the plot: the heading names it.
  const legend = series.length > 1 || kind === 'pie' ? look.legend : look.legend === 'top' ? 'none' : look.legend;
  const xLabel = look.xLabel || (xCol ? xCol.name : '');
  const yLabel = look.yLabel || (series.length === 1 ? series[0].name : '');
  const legendOf = (where) => (where === 'none' ? [] : legendFields(where));
  const done = () => `${fields.join('\n')}\nchildren:\n${indent(children.join('\n'), 2)}`;

  if (kind === 'pie') {
    const c = series[0];
    // Slices labelled with their share: a derived column of percentages.
    const total = c.numbers.reduce((a, v) => a + Math.max(0, v ?? 0), 0) || 1;
    const names = new Set(info.header);
    let share = `${c.name} share`;
    for (let k = 2; names.has(share); k++) {
      share = `${c.name} share ${k}`;
    }
    const shareCol = { name: share, index: info.columns.length + extra.length, numbers: c.numbers.map((v) => (v === null ? null : round4((Math.max(0, v) / total) * 100))) };
    extra.push(shareCol);
    fields.push(`flags: Equal|NoMouseText${legend === 'none' ? '|NoLegend' : ''}`, 'x_flags: NoDecorations', 'y_flags: NoDecorations');
    fields.push(...legendOf(legend === 'top' ? 'right' : legend));
    const slices = xCol ? columnRef(xCol) : JSON.stringify(c.cells.map((_, i) => `row ${i + 1}`).join('\n'));
    children.push(`- ${flow({ type: 'PlotPieChart', labels: slices, values: columnRef(shareCol), radius: 0.44, label_fmt: '"%.0f%%"' })}`);
    return done();
  }
  fields.push(...(legend === 'none' ? ['flags: NoLegend'] : legendFields(legend)));
  if (kind === 'histogram') {
    const c = series[0];
    fields.push(`x_label: ${yamlText(look.xLabel || c.name)}`, `y_label: ${yamlText(look.yLabel || 'count')}`);
    children.push(`- ${flow({ type: 'PlotHistogram', label: label(c), values: columnRef(c), bins: -2, fill_alpha: 0.85 })}`);
    return done();
  }
  const values = numbersOf(series);
  if (values.length === 0) {
    throw new Error('the columns chosen have no numbers');
  }
  const categorical = kind === 'bars' || kind === 'stacked' || kind === 'hbars' || (xCol && !xCol.numeric);
  const categories = xCol ? xCol.cells : info.rows.map((_, i) => String(i + 1));
  const isBars = kind === 'bars' || kind === 'stacked' || kind === 'hbars';
  // The top of the value axis: bars start at 0 and end on a round number.
  const barTop = () =>
    niceCeil(
      1.06 *
        (kind === 'stacked'
          ? Math.max(...info.rows.map((_, r) => series.reduce((a, c) => a + Math.max(0, c.numbers[r] ?? 0), 0)))
          : Math.max(...values)),
    );
  if (kind === 'hbars') {
    if (look.yLabel || yLabel) {
      fields.push(`x_label: ${yamlText(look.yLabel || yLabel)}`);
    }
    if (look.xLabel) {
      fields.push(`y_label: ${yamlText(look.xLabel)}`);
    }
    fields.push('y_flags: Invert', ...categoryTicks('y', categories, size.height * 0.55), `y_min: -0.6`, `y_max: ${n - 0.4}`);
    if (Math.min(...values) >= 0) {
      fields.push('x_min: 0', `x_max: ${barTop()}`);
    }
  } else {
    if (xLabel) {
      fields.push(`x_label: ${yamlText(xLabel)}`);
    }
    if (yLabel) {
      fields.push(`y_label: ${yamlText(yLabel)}`);
    }
    if (categorical) {
      fields.push(...categoryTicks('x', categories, size.width - 140));
      fields.push(isBars ? 'x_min: -0.6' : 'x_min: -0.3', isBars ? `x_max: ${n - 0.4}` : `x_max: ${n - 0.7}`);
    } else {
      const xv = xCol ? xCol.numbers.filter((v) => v !== null) : info.rows.map((_, i) => i);
      const [a, b] = padRange(xv, 0.03);
      fields.push(`x_min: ${a}`, `x_max: ${b}`);
    }
    if (isBars) {
      if (Math.min(...values) >= 0) {
        fields.push('y_min: 0', `y_max: ${barTop()}`);
      }
    } else {
      const [a, b] = padRange(values, 0.08, kind === 'area');
      fields.push(`y_min: ${a}`, `y_max: ${b}`);
    }
  }
  const xs = !categorical && xCol ? columnRef(xCol) : '';
  const markers = n <= 40;
  if (isBars) {
    if (series.length === 1 && kind !== 'hbars') {
      children.push(`- ${flow({ type: 'PlotBars', label: label(series[0]), ys: columnRef(series[0]), bar_size: 0.62 })}`);
    } else {
      children.push(
        `- ${flow({
          type: 'PlotBarGroups',
          labels: `[${series.map(label).join(', ')}]`,
          values: `[${series.map(columnRef).join(', ')}]`,
          group_size: series.length === 1 ? 0.62 : 0.78,
          stacked: kind === 'stacked' ? 'true' : '',
          horizontal: kind === 'hbars' ? 'true' : '',
        })}`,
      );
    }
    return done();
  }
  const colors = palette?.length ? palette : DEEP;
  series.forEach((c, k) => {
    if (kind === 'area') {
      // The fill and its edge line share the series colour and legend entry.
      const color = JSON.stringify(colors[k % colors.length]);
      children.push(`- ${flow({ type: 'PlotShaded', label: label(c), color, xs, ys: columnRef(c), fill_alpha: 0.18 })}`);
      const edge = JSON.stringify(`##${look.labels[c.index] || c.name} line`); // ## keeps it out of the legend
      children.push(`- ${flow({ type: 'PlotLine', label: edge, color, xs, ys: columnRef(c), line_weight: 2.5 })}`);
    } else if (kind === 'scatter') {
      children.push(`- ${flow({ type: 'PlotScatter', label: label(c), xs, ys: columnRef(c), marker: 'Circle', marker_size: 4, fill_alpha: 0.7 })}`);
    } else {
      children.push(`- ${flow({ type: 'PlotLine', label: label(c), xs, ys: columnRef(c), line_weight: 2.5, marker: markers ? 'Circle' : '', marker_size: markers ? 3.5 : '' })}`);
    }
  });
  return done();
}

/** A round upper limit: 1, 1.2, 1.5, 2, 2.5, 3, 4, 5, 6 or 8 times a power of ten, at or above `v`. */
export function niceCeil(v) {
  if (!(v > 0)) {
    return 1;
  }
  const p = 10 ** Math.floor(Math.log10(v));
  for (const m of [1, 1.2, 1.5, 2, 2.5, 3, 4, 5, 6, 8, 10]) {
    if (m * p >= v) {
      return Number((m * p).toPrecision(6));
    }
  }
  return 10 * p;
}

/** The data block: the CSV again, cleaned (numbers as numbers), then the derived columns. */
function dataYaml(info, extra = []) {
  const lines = [[...info.header, ...extra.map((c) => c.name)].map(csvCell).join(', ')];
  const num = (v) => (v === null ? '' : String(v));
  for (let r = 0; r < info.rows.length; r++) {
    const cells = info.columns.map((c) => (c.numeric ? num(c.numbers[r]) : csvCell(c.cells[r])));
    lines.push([...cells, ...extra.map((c) => num(c.numbers[r]))].join(', '));
  }
  return `data: |\n${indent(lines.join('\n'), 2)}\n`;
}

/** The size of the image: the one chosen, or `auto` (a diagram's own). */
function sizeOf(look, auto) {
  const s = SIZES.find((z) => z.id === look.size);
  return s ? { width: s.width, height: s.height } : auto;
}

/**
 * Writes the nymph YAML of a chart made from a CSV table.
 * @param {object} info analyzeTable()'s result
 * @param {string} kind a CHART_KINDS id
 * @param {{x: number, series: number[]}} mapping column indexes (x -1: row number)
 * @param {object} look defaultLook() with the user's changes
 * @param {string[]} [palette] the theme's series colours (cards, area fills)
 * @returns {string} the source
 * @throws {Error} when no value column is chosen
 */
export function csvChartSource(info, kind, mapping, look, palette) {
  if (mapping.series.length === 0) {
    throw new Error('choose at least one column of values');
  }
  const fonts = TYPE_STYLES.find((t) => t.id === look.type)?.fonts ?? TYPE_STYLES[0].fonts;
  const { width, height } = sizeOf(look, SIZES[0]);
  const def = CHART_KINDS.find((k) => k.id === kind);
  const names = mapping.series.map((i) => look.labels[i] || info.columns[i].name);
  const description = `${def.title} of ${names.join(', ')}${mapping.x >= 0 && def.x ? ` by ${info.columns[mapping.x].name}` : ''}.`;
  const right = `${info.rows.length} ${info.rows.length === 1 ? 'row' : 'rows'}`;
  const extra = [];
  const children = [
    ...headerYaml(look, fonts, width),
    ...(look.cards ? cardsYaml(kind, info, mapping, look, fonts, palette) : []),
    `- ${plotYaml(kind, info, mapping, look, fonts, palette, extra, { width, height }).replace(/\n/g, '\n  ')}`,
    ...footerYaml(look, fonts, right),
  ];
  return [
    'nymph: 1',
    `description: ${yamlText(description)}`,
    imageYaml(width, height, look, fonts),
    'root:',
    '  type: VerticalLayout',
    '  spacing: 10',
    '  children:',
    indent(children.join('\n'), 4),
    dataYaml(info, extra),
  ].join('\n');
}

// ── Mermaid ───────────────────────────────────────────────────────────────────

/** Splits a YAML source into its top-level entries: [{key, text}]. */
export function topLevel(source) {
  const entries = [];
  let pending = [];
  for (const line of source.replace(/\r\n/g, '\n').split('\n')) {
    const m = /^([A-Za-z_][\w]*)\s*:/.exec(line);
    if (m) {
      entries.push({ key: m[1], text: [...pending, line].join('\n') });
      pending = [];
    } else if (entries.length && (line === '' || /^\s/.test(line))) {
      entries.at(-1).text += `\n${line}`;
    } else {
      pending.push(line);
    }
  }
  return entries;
}

/** The root element of a top-level `root:` entry as a list item ("- type: ..."). */
function rootAsItem(text) {
  const body = text.replace(/^(#.*\n)*root\s*:/, '');
  const inline = body.split('\n')[0].trim();
  if (inline) {
    return `- ${inline}`; // root: {type: ...}
  }
  const lines = body.split('\n').slice(1).filter((l, i, all) => l.trim() !== '' || all.slice(i).some((x) => x.trim() !== ''));
  const pad = Math.min(...lines.filter((l) => l.trim() && !l.trim().startsWith('#')).map((l) => l.match(/^ */)[0].length));
  const out = [];
  let first = true;
  for (const l of lines) {
    const rest = l.slice(Math.min(pad, l.match(/^ */)[0].length));
    if (first && rest.trim() && !rest.trim().startsWith('#')) {
      out.push(`- ${rest}`);
      first = false;
    } else {
      out.push(rest === '' ? '' : `  ${rest}`);
    }
  }
  return out.join('\n').replace(/\s+$/, '');
}

/** The width and height of an `image:` entry, or null. */
function imageSize(text) {
  const w = /\bwidth:\s*(\d+)/.exec(text ?? '');
  const h = /\bheight:\s*(\d+)/.exec(text ?? '');
  return w && h ? { width: Number(w[1]), height: Number(h[1]) } : null;
}

/** The title a converted diagram carries (mermaid's `title`), or ''. */
export function mermaidTitle(source) {
  const root = topLevel(source).find((e) => e.key === 'root');
  const m = /^\s{2}title:\s*(.+)$/m.exec(root?.text ?? '');
  if (!m) {
    return '';
  }
  const t = m[1].trim();
  try {
    return t.startsWith('"') ? JSON.parse(t) : t.replace(/^'(.*)'$/, '$1');
  } catch {
    return t;
  }
}

/**
 * Frames a nymph source converted from mermaid with a heading and a
 * footnote, in the theme, fonts and size chosen.
 * @param {string} converted the converter's YAML
 * @param {object} look defaultLook() with the user's changes
 * @returns {string} the new source
 */
export function mermaidChartSource(converted, look) {
  const fonts = TYPE_STYLES.find((t) => t.id === look.type)?.fonts ?? TYPE_STYLES[0].fonts;
  const entries = topLevel(converted);
  const root = entries.find((e) => e.key === 'root');
  if (!root) {
    throw new Error('the converted diagram has no root');
  }
  const own = imageSize(entries.find((e) => e.key === 'image')?.text) ?? { width: 800, height: 500 };
  const header = headerYaml(look, fonts, look.size === 'auto' ? Math.max(own.width, 720) : sizeOf(look, own).width);
  const footer = footerYaml(look, fonts, '');
  // The heading and footnote take room the diagram's own size did not have.
  const extra = (look.title ? 60 : 0) + (look.subtitle ? 26 : 0) + (look.header === 'box' && header.length ? 32 : 0) + (footer.length ? 40 : 0) + 28;
  const size = look.size === 'auto' ? { width: Math.max(own.width, 720), height: Math.min(4096, own.height + extra) } : sizeOf(look, own);
  let item = rootAsItem(root.text);
  if (look.title) {
    item = item.replace(/\n {2}title:.*(?=\n|$)/, ''); // the heading shows it
  }
  // The diagram takes the room left, unless it sets its own height.
  if (!/^- \{/.test(item) && !/^ {2}height:/m.test(item)) {
    item = item.replace(/^- (.*)$/m, '- $1\n  height: -1');
  }
  const rest = entries.filter((e) => !['nymph', 'image', 'root'].includes(e.key)).map((e) => e.text.replace(/\s+$/, ''));
  return [
    'nymph: 1',
    ...rest.filter((t) => /^description\s*:/m.test(t)),
    imageYaml(size.width, size.height, look, fonts),
    'root:',
    '  type: VerticalLayout',
    '  spacing: 10',
    '  children:',
    indent([...header, item, ...footer].join('\n'), 4),
    ...rest.filter((t) => !/^description\s*:/m.test(t)),
    '',
  ].join('\n');
}

// ── The wizard ────────────────────────────────────────────────────────────────

/** Small drawings of each chart kind for the first step's cards. */
const KIND_ICONS = {
  line: '<polyline points="6,40 18,28 30,32 42,16 56,20" fill="none" stroke="var(--accent)" stroke-width="3" stroke-linejoin="round" stroke-linecap="round"/><polyline points="6,44 18,38 30,40 42,30 56,34" fill="none" stroke="var(--muted)" stroke-width="2" stroke-linejoin="round" opacity=".6"/>',
  area: '<path d="M6 40 L18 26 L30 32 L42 14 L56 20 L56 46 L6 46Z" fill="var(--accent)" opacity=".25"/><polyline points="6,40 18,26 30,32 42,14 56,20" fill="none" stroke="var(--accent)" stroke-width="3" stroke-linejoin="round"/>',
  bars: '<rect x="8" y="22" width="7" height="24" rx="1.5" fill="var(--accent)"/><rect x="16" y="30" width="7" height="16" rx="1.5" fill="var(--muted)" opacity=".6"/><rect x="30" y="12" width="7" height="34" rx="1.5" fill="var(--accent)"/><rect x="38" y="24" width="7" height="22" rx="1.5" fill="var(--muted)" opacity=".6"/>',
  stacked: '<rect x="10" y="26" width="10" height="20" rx="1.5" fill="var(--accent)"/><rect x="10" y="16" width="10" height="10" fill="var(--muted)" opacity=".6"/><rect x="26" y="20" width="10" height="26" rx="1.5" fill="var(--accent)"/><rect x="26" y="8" width="10" height="12" fill="var(--muted)" opacity=".6"/><rect x="42" y="30" width="10" height="16" rx="1.5" fill="var(--accent)"/><rect x="42" y="22" width="10" height="8" fill="var(--muted)" opacity=".6"/>',
  hbars: '<rect x="8" y="8" width="44" height="8" rx="1.5" fill="var(--accent)"/><rect x="8" y="20" width="32" height="8" rx="1.5" fill="var(--accent)" opacity=".8"/><rect x="8" y="32" width="22" height="8" rx="1.5" fill="var(--accent)" opacity=".6"/><rect x="8" y="44" width="12" height="4" rx="1.5" fill="var(--muted)" opacity=".6"/>',
  scatter: '<circle cx="12" cy="38" r="3" fill="var(--accent)"/><circle cx="20" cy="32" r="3" fill="var(--accent)"/><circle cx="26" cy="36" r="3" fill="var(--accent)"/><circle cx="32" cy="24" r="3" fill="var(--accent)"/><circle cx="40" cy="22" r="3" fill="var(--accent)"/><circle cx="46" cy="14" r="3" fill="var(--accent)"/><circle cx="52" cy="18" r="3" fill="var(--muted)" opacity=".6"/>',
  pie: '<circle cx="31" cy="27" r="19" fill="var(--muted)" opacity=".35"/><path d="M31 27 L31 8 A19 19 0 0 1 48 35 Z" fill="var(--accent)"/><path d="M31 27 L48 35 A19 19 0 0 1 22 44 Z" fill="var(--accent)" opacity=".6"/>',
  histogram: '<rect x="6" y="38" width="8" height="8" fill="var(--accent)" opacity=".6"/><rect x="14" y="26" width="8" height="20" fill="var(--accent)" opacity=".8"/><rect x="22" y="12" width="8" height="34" fill="var(--accent)"/><rect x="30" y="18" width="8" height="28" fill="var(--accent)"/><rect x="38" y="30" width="8" height="16" fill="var(--accent)" opacity=".8"/><rect x="46" y="40" width="8" height="6" fill="var(--accent)" opacity=".6"/>',
};

/** Whether a file is mermaid by its name or its first keyword. */
export function isMermaidFile(name, text) {
  if (/\.(mmd|mermaid)$/i.test(name ?? '')) {
    return true;
  }
  const first = (text ?? '')
    .replace(/^﻿/, '')
    .split('\n')
    .map((l) => l.trim())
    .find((l) => l && !l.startsWith('%%') && l !== '---');
  return /^(flowchart|graph|sequenceDiagram|classDiagram|stateDiagram(-v2)?|erDiagram|gantt|pie|mindmap|sankey(-beta)?|xychart(-beta)?|journey|timeline)\b/.test(first ?? '');
}

/**
 * Runs the import wizard for a file's text.
 * @param {object} deps
 * @param {string} deps.name the file name
 * @param {string} deps.text its text
 * @param {Function} deps.dialog, deps.el, deps.dialogButtons ui.js helpers
 * @param {Function} deps.createViewer the viewer module's createViewer
 * @param {Function} deps.importMermaid the viewer module's importMermaid
 * @param {string} deps.wasmUrl the viewer module's URL
 * @returns {Promise<{source: string, name: string}|undefined>} undefined when cancelled
 * @throws {Error} when the file cannot be read as CSV or mermaid
 */
export async function importWizard(deps) {
  const { name, text, el } = deps;
  const mermaid = isMermaidFile(name, text);
  const state = { look: defaultLook(name, mermaid ? 'mermaid' : 'csv'), step: 0 };
  let info = null;
  if (mermaid) {
    state.mermaidText = text.replace(/^﻿/, '');
  } else {
    info = analyzeTable(parseCsv(text));
    state.kind = suggestKind(info);
    state.mapping = defaultMapping(state.kind, info);
    state.look.cards = state.mapping.series.length <= 3 && state.kind !== 'histogram';
  }
  const steps = mermaid ? ['Diagram', 'Look'] : ['Chart type', 'Data', 'Look'];
  const base = name.replace(/\.[^.]*$/, '') || 'chart';

  return deps.dialog(
    mermaid ? 'Import a mermaid diagram' : 'Import a CSV table',
    (body, close) => {
      body.classList.add('wizard');
      const stepper = el('ol', { class: 'stepper' });
      const panel = el('div', { class: 'wizard-panel' });
      const previewBox = el('div', { class: 'preview-canvas' });
      const previewNote = el('div', { class: 'preview-note', role: 'status' });
      const preview = el('div', { class: 'wizard-preview' }, previewBox, previewNote);
      const back = el('button', { type: 'button', class: 'btn' }, 'Back');
      const next = el('button', { type: 'button', class: 'btn primary' }, 'Next');
      const cancel = el('button', { type: 'button', class: 'btn ghost' }, 'Cancel');
      body.append(stepper, el('div', { class: 'wizard-main' }, panel, preview), el('div', { class: 'row' }, cancel, el('span', { class: 'spacer' }), back, next));

      let viewer = null;
      let themes = [];
      let source = '';
      let converted = null; // {source, kind, warnings} for mermaid
      let timer = 0;
      let drawn = 0;
      const finish = (value) => {
        clearTimeout(timer);
        viewer?.destroy();
        viewer = null;
        close(value);
      };
      cancel.onclick = () => finish(undefined);
      body.closest('dialog')?.addEventListener('close', () => viewer?.destroy());

      const palette = () => themes.find((t) => t.id === state.look.theme)?.palette;

      /** The source of the current choices; null with a message when there is none. */
      const build = () => {
        if (mermaid) {
          if (!converted) {
            return { error: 'Converting…' };
          }
          if (converted.error) {
            return { error: converted.error };
          }
          return { source: mermaidChartSource(converted.source, state.look) };
        }
        try {
          return { source: csvChartSource(info, state.kind, state.mapping, state.look, palette()) };
        } catch (e) {
          return { error: e.message };
        }
      };

      const refresh = () => {
        clearTimeout(timer);
        timer = setTimeout(draw, 180);
      };

      const draw = async () => {
        const token = ++drawn;
        const got = build();
        next.disabled = !got.source && state.step === steps.length - 1;
        if (!got.source) {
          previewNote.textContent = got.error;
          previewNote.className = 'preview-note err';
          return;
        }
        source = got.source;
        try {
          if (!viewer) {
            viewer = await deps.createViewer(previewBox, { fit: 'scale', wasmUrl: deps.wasmUrl, tooltips: false, menu: false, resetButton: false });
            themes = viewer.themes();
            if (state.step === steps.length - 1) {
              render(); // the theme cards
            }
            if (!mermaid && state.look.cards) {
              source = build().source ?? source; // the cards take the theme's colours
            }
          }
          if (token !== drawn || !viewer) {
            return;
          }
          await viewer.setSource(source);
          previewNote.textContent = mermaid && converted?.warnings?.length ? `${converted.warnings.length} part(s) of the diagram were left out: ${converted.warnings[0].message}` : '';
          previewNote.className = 'preview-note';
          next.disabled = false;
        } catch (e) {
          if (token === drawn) {
            previewNote.textContent = e.message ?? String(e);
            previewNote.className = 'preview-note err';
            next.disabled = state.step === steps.length - 1;
          }
        }
      };

      const convert = async () => {
        try {
          converted = await deps.importMermaid(state.mermaidText, { wasmUrl: deps.wasmUrl });
          const t = mermaidTitle(converted.source);
          if (t && state.look.title === defaultLook(name).title) {
            state.look.title = t;
          }
        } catch (e) {
          converted = { error: e.line ? `Line ${e.line}: ${e.message}` : e.message ?? String(e) };
        }
        if (kindNote) {
          kindNote.textContent = diagramNote();
        }
        refresh();
      };

      // ── Controls ──
      const field = (label, control, hint) => el('label', { class: 'field' }, el('span', { class: 'field-label' }, label), control, hint ? el('span', { class: 'hint' }, hint) : null);
      const textInput = (key, placeholder) => {
        const input = el('input', { type: 'text', value: state.look[key], placeholder, spellcheck: 'true' });
        input.addEventListener('input', () => {
          state.look[key] = input.value;
          refresh();
        });
        return input;
      };
      const select = (options, value, onChange) => {
        const s = el('select', {}, options.map((o) => el('option', { value: o.value, selected: String(o.value) === String(value) }, o.label)));
        s.addEventListener('change', () => onChange(s.value));
        return s;
      };
      const chips = (options, key, onChange) =>
        el(
          'div',
          { class: 'chips', role: 'radiogroup' },
          options.map((o) =>
            el(
              'button',
              {
                type: 'button',
                role: 'radio',
                class: `chip${state.look[key] === o.id ? ' on' : ''}`,
                'aria-checked': String(state.look[key] === o.id),
                onclick: () => {
                  state.look[key] = o.id;
                  onChange?.();
                  render();
                  refresh();
                },
              },
              o.title,
            ),
          ),
        );

      const kindStep = () =>
        el(
          'div',
          { class: 'kind-grid', role: 'radiogroup', 'aria-label': 'Chart type' },
          CHART_KINDS.map((k) =>
            el(
              'button',
              {
                type: 'button',
                role: 'radio',
                'aria-checked': String(state.kind === k.id),
                class: `kind-card${state.kind === k.id ? ' on' : ''}`,
                onclick: () => {
                  state.kind = k.id;
                  state.mapping = defaultMapping(k.id, info);
                  render();
                  refresh();
                },
                ondblclick: () => go(1),
              },
              el('span', { class: 'kind-icon', html: `<svg viewBox="0 0 62 52" width="62" height="52" aria-hidden="true">${KIND_ICONS[k.id]}</svg>` }),
              el('strong', {}, k.title),
              el('span', {}, k.note),
            ),
          ),
        );

      const dataStep = () => {
        const def = CHART_KINDS.find((k) => k.id === state.kind);
        const out = el('div', { class: 'stack' });
        if (def.x) {
          const usable = info.columns.filter((c) => state.kind !== 'scatter' || c.numeric);
          out.append(
            field(
              def.x,
              select(
                [{ value: -1, label: 'Row number (1, 2, 3, …)' }, ...usable.map((c) => ({ value: c.index, label: `${c.name}${c.numeric ? '' : ' (text)'}` }))],
                state.mapping.x,
                (v) => {
                  state.mapping.x = Number(v);
                  state.mapping.series = state.mapping.series.filter((i) => i !== state.mapping.x);
                  render();
                  refresh();
                },
              ),
            ),
          );
        }
        const one = def.series === 'one';
        const list = el('div', { class: 'series-list' });
        for (const c of info.columns.filter((col) => col.numeric && col.index !== state.mapping.x)) {
          const on = state.mapping.series.includes(c.index);
          const box = el('input', { type: one ? 'radio' : 'checkbox', name: 'series', checked: on, 'aria-label': c.name });
          box.addEventListener('change', () => {
            if (one) {
              state.mapping.series = [c.index];
            } else if (box.checked) {
              state.mapping.series = info.columns.filter((x) => x.index === c.index || state.mapping.series.includes(x.index)).map((x) => x.index);
            } else {
              state.mapping.series = state.mapping.series.filter((i) => i !== c.index);
            }
            render();
            refresh();
          });
          const rename = el('input', { type: 'text', value: state.look.labels[c.index] ?? '', placeholder: c.name, 'aria-label': `Legend name of ${c.name}`, disabled: !on });
          rename.addEventListener('input', () => {
            state.look.labels[c.index] = rename.value;
            refresh();
          });
          list.append(el('div', { class: `series-row${on ? ' on' : ''}` }, el('label', { class: 'series-name' }, box, el('span', {}, c.name)), rename));
        }
        out.append(el('div', { class: 'field' }, el('span', { class: 'field-label' }, one ? 'Values' : 'Series (legend name on the right)'), list));
        if (state.kind !== 'pie') {
          out.append(
            el(
              'div',
              { class: 'two' },
              field(state.kind === 'hbars' ? 'Category axis label' : 'X axis label', textInput('xLabel', state.kind === 'histogram' ? info.columns[state.mapping.series[0]]?.name ?? '' : info.columns[state.mapping.x]?.name ?? '')),
              field(state.kind === 'hbars' ? 'Value axis label' : 'Y axis label', textInput('yLabel', state.kind === 'histogram' ? 'count' : '')),
            ),
          );
        }
        // A glance at the table.
        const shown = info.rows.slice(0, 4);
        out.append(
          el(
            'div',
            { class: 'table-peek' },
            el(
              'table',
              {},
              el('tr', {}, info.columns.map((c) => el('th', { class: state.mapping.series.includes(c.index) ? 'used' : c.index === state.mapping.x ? 'axis' : '' }, c.name, el('small', {}, c.numeric ? 'number' : 'text')))),
              shown.map((r) => el('tr', {}, r.map((cell) => el('td', {}, cell)))),
            ),
            el('p', { class: 'hint' }, `${info.rows.length} rows, ${info.columns.length} columns.`),
          ),
        );
        return out;
      };

      let kindNote = null;
      const diagramNote = () =>
        converted?.kind ? `A ${converted.kind} diagram. Fix the text here if part of it does not convert.` : 'The diagram\'s text: fix it here if part of it does not convert.';
      const diagramStep = () => {
        const area = el('textarea', { spellcheck: 'false', 'aria-label': 'Mermaid text', rows: 12 }, state.mermaidText);
        let t = 0;
        area.addEventListener('input', () => {
          state.mermaidText = area.value;
          clearTimeout(t);
          t = setTimeout(convert, 300);
        });
        kindNote = el('p', {}, diagramNote());
        return el('div', { class: 'stack' }, kindNote, area);
      };

      const lookStep = () => {
        const out = el('div', { class: 'stack' });
        out.append(field('Title', textInput('title', 'A title')), field('Subtitle', textInput('subtitle', 'One line on what it shows')), field('Footnote', textInput('note', 'Source: …')));
        const typeGrid = el(
          'div',
          { class: 'type-grid', role: 'radiogroup', 'aria-label': 'Fonts' },
          TYPE_STYLES.map((t) =>
            el(
              'button',
              {
                type: 'button',
                role: 'radio',
                'aria-checked': String(state.look.type === t.id),
                class: `type-card${state.look.type === t.id ? ' on' : ''}`,
                onclick: () => {
                  state.look.type = t.id;
                  render();
                  refresh();
                },
              },
              el('span', { class: 'type-sample', style: `font-family:${t.css}` }, 'Aa'),
              el('strong', {}, t.title),
              el('span', {}, t.note),
            ),
          ),
        );
        out.append(el('div', { class: 'field' }, el('span', { class: 'field-label' }, 'Fonts'), typeGrid));
        const themeList = themes.length ? themes : [{ id: 'light', name: 'Light', background: '#ffffff', text_color: '#000000', palette: [] }];
        const themeGrid = el(
          'div',
          { class: 'theme-strip', role: 'radiogroup', 'aria-label': 'Colour theme' },
          themeList.map((t) =>
            el(
              'button',
              {
                type: 'button',
                role: 'radio',
                title: t.description ?? t.name,
                'aria-checked': String(state.look.theme === t.id),
                class: `theme-chip${state.look.theme === t.id ? ' on' : ''}`,
                onclick: () => {
                  state.look.theme = t.id;
                  render();
                  refresh();
                },
              },
              el('span', { class: 'theme-swatch', style: `background:${t.background};color:${t.text_color}` }, el('b', {}, 'Aa'), el('span', { class: 'swatch-dots' }, (t.palette ?? []).slice(0, 4).map((c) => el('i', { style: `background:${c}` })))),
              el('span', {}, t.name),
            ),
          ),
        );
        out.append(el('div', { class: 'field' }, el('span', { class: 'field-label' }, 'Colour theme'), themeGrid));
        out.append(el('div', { class: 'field' }, el('span', { class: 'field-label' }, 'Heading'), chips(HEADERS, 'header')));
        const sizes = mermaid ? [{ id: 'auto', title: 'Fit the diagram' }, ...SIZES] : SIZES;
        out.append(el('div', { class: 'field' }, el('span', { class: 'field-label' }, 'Size'), chips(sizes.map((s) => ({ id: s.id, title: s.width ? `${s.title} ${s.width}×${s.height}` : s.title })), 'size')));
        if (!mermaid) {
          out.append(el('div', { class: 'field' }, el('span', { class: 'field-label' }, 'Legend'), chips(LEGENDS, 'legend')));
          const cards = el('input', { type: 'checkbox', checked: state.look.cards, disabled: state.mapping.series.length > 4 });
          cards.addEventListener('change', () => {
            state.look.cards = cards.checked;
            refresh();
          });
          out.append(el('label', { class: 'check' }, cards, el('span', {}, 'Summary cards: a text box with the headline number of each series (up to 4)')));
        }
        return out;
      };

      const render = () => {
        stepper.replaceChildren(
          ...steps.map((s, i) =>
            el('li', { class: i === state.step ? 'on' : i < state.step ? 'done' : '' }, el('button', { type: 'button', onclick: () => go(i), disabled: i > state.step + 1 }, el('b', {}, String(i + 1)), s)),
          ),
        );
        const which = mermaid ? [diagramStep, lookStep][state.step] : [kindStep, dataStep, lookStep][state.step];
        const scroll = panel.scrollTop;
        panel.replaceChildren(which());
        panel.scrollTop = scroll;
        back.hidden = state.step === 0;
        next.textContent = state.step === steps.length - 1 ? 'Create chart' : 'Next';
      };

      const go = (step) => {
        state.step = Math.max(0, Math.min(steps.length - 1, step));
        panel.scrollTop = 0;
        render();
      };
      back.onclick = () => go(state.step - 1);
      next.onclick = () => {
        if (state.step < steps.length - 1) {
          go(state.step + 1);
          return;
        }
        const got = build();
        if (got.source) {
          finish({ source: got.source, name: `${base}.yaml` });
        }
      };

      render();
      if (mermaid) {
        convert();
      } else {
        refresh();
      }
    },
    { class: 'wizard-dlg' },
  );
}
