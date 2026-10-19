# Placeholder images

The images `Image` and `ImageButton` show in a user-interface mockup
(`image: person`), compiled into nymph by
`cmake/NymphPlaceholders.cmake`; the name is the file name without
`.svg`. Design: `spec/09_ui_mockups/DESIGN.md` section 3, "Placeholder
images".

They are drawn by nymph with ImDrawList, not by an SVG renderer, so each
file keeps to a small subset (a test checks every file):

- One `<svg>` with a square `viewBox` (`0 0 100 100`);
  `preserveAspectRatio="none"` stretches it to any box (only `cross`).
- Only self-closing `path` (M L H V C S Q T Z, absolute or relative; no
  arcs), `rect` (with `rx` / `ry`), `circle`, `ellipse`, `polygon` and
  `line`; no groups, transforms, gradients, text or images.
- `fill` and `stroke` colours: `#000000` (drawn in the image's `tint`),
  `#808080` (the tint at half alpha) or `#FFFFFF` (the background, to cut
  shapes out: ImGui fills have no holes); `none`.
- Every filled shape a simple polygon (no self-intersections); painter's
  order is the file order.
- No XML comments: the files are compiled into the web viewer, which is
  inlined into HTML, where a comment marker would end the script.

| File | Shows |
|---|---|
| `cross` | The wireframe box with diagonals: "an image goes here" (stretches to any shape) |
| `person` | Head and shoulders: a portrait photo |
| `people` | Two people: a team or group photo |
| `landscape` | Mountains and the sun: any photo |
| `product` | A box: a product shot |
| `logo` | A generic mark |
| `video` | A frame with a play button |
| `map` | A folded map with a pin |
| `chart` | Bars: a chart thumbnail |
| `document` | A page with lines and a folded corner |
| `icon` | A rounded square with a star: an app or file icon |
| `building` | A building: a venue, an office or a company |

To add one, drop a file that follows these rules here and rebuild. The
files in this folder were drawn for nymph and are under its MIT licence;
a file from elsewhere needs a licence that allows it, noted here.
