# The web viewer

The web viewer shows a nymph chart live in a web page, from its YAML
source or from a nymph PNG: drag to pan, wheel or pinch to zoom, hover or
click for values. It is the nymph library compiled to WebAssembly and
drawn with WebGL2 (on the CPU when WebGL2 is missing), behind a small
JavaScript API and a `<nymph-viewer>` element, for markdown viewers,
chatbots, notebooks and documentation sites. Design:
[spec/04_web_viewer/DESIGN.md](../spec/04_web_viewer/DESIGN.md); build:
[building.md](building.md#web-viewer).

## Files to serve

From `build-web/web/`, served together from one folder:

| File | What |
|------|------|
| `nymph-viewer.js` | The API (an ES module). Import this. |
| `nymph-viewer-core.js`, `nymph-viewer-core.wasm` | The module (about 630 KB gzip), loaded on the first viewer. |
| `fonts/` | The font files and their licences, fetched only when a chart names a font. |
| `nymph-viewer.single.js` | Instead of the three files above: everything in one file (about 690 KB gzip). Fonts still come from `fonts/` next to it, or `fontBaseUrl`. |

The page's Content Security Policy must allow `'wasm-unsafe-eval'` (or
`'unsafe-eval'`) in `script-src`. The module reads no file and makes no
request itself: the API fetches `src` URLs and fonts.

## Embedding

The element, with a URL (`.yaml` or a nymph `.png`) or with the YAML as
its text:

```html
<script type="module" src="/nymph/nymph-viewer.js"></script>

<nymph-viewer src="/charts/revenue.yaml"></nymph-viewer>
<nymph-viewer src="/charts/revenue.png" fit="width" theme="auto"></nymph-viewer>
<nymph-viewer>
  root:
    type: Plot
    title: Revenue
    children:
      - {type: PlotLine, label: revenue, ys: [12, 15, 11, 18]}
</nymph-viewer>
```

A markdown renderer can turn a fenced block whose language is `nymph`
into the last form (escape `<` and `&` in the text). Attributes: `src`,
`fit` (`source`, `width`, `fill`), `theme` (`source`, `light`, `dark`,
`classic`, `auto`), `tooltips` (`false` turns nymph's own off). Events,
bubbling: `nymph-load` (shown; `detail` is `info()`), `nymph-hover`,
`nymph-click`, `nymph-view`, `nymph-error` (the error is also shown as
text in the element). Its `viewer` property is the `NymphViewer`.

The API:

```js
import { createViewer } from '/nymph/nymph-viewer.js';

const viewer = await createViewer(document.getElementById('chart'), { source: yamlText, theme: 'auto' });
viewer.on('hover', (pick) => console.log(pick?.label, pick?.values));
setInterval(() => viewer.setData({ revenue: latestValues() }), 250);
```

| Call | What |
|------|------|
| `createViewer(container, options)` | A viewer in `container` (a canvas is added to it). Options: `source` or `png`, `fit`, `theme`, `tooltips`, `rasterizer` (`auto`, `webgl`, `software`), and for the first viewer of the page `wasmUrl`, `fontBaseUrl`, `fetchFont(name)`. |
| `setSource(yaml, {resetView})` | Shows a new source; the zoom of elements that did not change is kept unless `resetView`. |
| `setData(tables)` | Replaces the source's `data` (an object, or YAML / JSON text); the view is kept. |
| `setPng(bytes)` | Shows the source a nymph PNG carries; resolves `false` for a PNG without one (shown as a plain image). |
| `setTheme(theme)`, `resetView()`, `resize(w, h, ratio)` | Theme override (`''` for the source's), back to the source's view, box size (the element and `createViewer` follow their container themselves). |
| `on(event, callback)` | `hover` (pick or `null`), `click` (pick), `view` (every plot's area and ranges after a pan or zoom settles), `error` (an update failed). Returns a function that removes the listener. |
| `rendered()` | Resolves when every update so far is on screen. |
| `info()`, `state()`, `source()` | Rasterizer, description and size; geometry, hovered pick and plot views; the source on screen. |
| `toPng()` | The chart as a nymph PNG (its source, with the current data, embedded). |
| `destroy()` | Removes the canvas and frees the viewer. |

Updates resolve once the chart is shown and reject with a `NymphError`
(`line`, `column`, `kind`, `message`: the same text as the `nymph` CLI)
while the previous chart stays. They apply in call order; several
`setData()` calls waiting behind a font fetch collapse into the last.

A pick is `{element, type, label, index, values}`: `element` is the
element's path in the source (`root/children/1`), `values` its values as
the axes show them (`{x: "2026-03-02", y: "7.25 kg"}`).

## Interaction

| Element | Pointer | Hover / click |
|---------|---------|---------------|
| `Plot` | Drag pans, wheel zooms (over an axis: that axis), right-drag box-zooms, double-click fits, legend click hides a series. | Nearest point within 8 px of lines, scatter, stairs, stems, shaded, digital, error bars; the bar, histogram bin, heatmap or 2D-histogram cell, pie slice under the pointer. |
| `Plot3D` | Drag rotates, wheel zooms, double-click resets. | None yet. |
| `GanttChart` | Drag and wheel pan and zoom time. | The task or milestone: start, end, section. |
| `SankeyDiagram` | None. | The node (total) or flow (from, to, value). |
| UML diagrams, `Flowchart` | Drag pans, wheel zooms around the pointer, double-click fits. | The node or edge (not yet in sequence diagrams): id, name, label. |

A plot with `flags: NoInputs` does not move. The wheel scrolls the page
except over a plot or diagram that takes input; on touch screens a
vertical swipe scrolls the page, a horizontal one pans, two fingers zoom.
Source limits (`x_min`, ...) are where a plot starts; double-click or
`resetView()` goes back. New data outside the visible range is not
followed unless the axis has the `AutoFit` flag.

## Rendering

Every viewer of a page draws through one WebGL2 context (browsers allow
only about 16) and copies its frames to its own canvas; frames run only
after input, an update or a resize, and not while the chart is off
screen. Without WebGL2, or after the context is lost, viewers draw on the
CPU. Both look like the PNG the CLI renders (WebGL within the tolerance
of [spec/04_web_viewer/PLAN.md](../spec/04_web_viewer/PLAN.md) step 10).
