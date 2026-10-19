# The browser extension

The nymph extension for Chrome turns nymph PNGs on any page into live
charts: pan, zoom, values on hover, and save as PNG or YAML from the
right-click menu. It finds the PNGs that carry their YAML source, puts a
[web viewer](viewer.md) of the same size in their place, and keeps doing
so as the page changes (comments, previews, feeds that load more as you
scroll). Other images are left alone. Design:
[spec/05_browser_extension/DESIGN.md](../spec/05_browser_extension/DESIGN.md).

## Install

From the Chrome Web Store: search for "nymph charts" and click **Add to
Chrome**. Each nymph release publishes the new version there, after the
store's review ([releasing.md](releasing.md)).

Or load it unpacked: take `nymph-extension-<version>.zip` from a GitHub
release and unzip it, or build it with the web viewer
([building.md](building.md#web-viewer)):

```sh
emcmake cmake -S . -B build-web -DNYMPH_WEB=ON -DCMAKE_BUILD_TYPE=MinSizeRel
cmake --build build-web
```

`build-web/extension/nymph/` is the unpacked extension and
`build-web/extension/nymph-extension-<version>.zip` the same files zipped.
In Chrome (116 or later), open `chrome://extensions`, turn on
**Developer mode**, click **Load unpacked** and pick the `nymph` folder
(or the unzipped release folder).

Then pin its button: open the puzzle-piece menu in the toolbar and click
the pin next to "nymph charts". A tab with this hint opens once on
install.

## The button

The button next to the address bar shows and toggles nymph for the site
in the current tab:

| | |
|---|---|
| Colour icon | On for this site. The badge counts the charts on the tab. |
| Grey icon | Off for this site. |
| Click | Turns the site off or on, in every tab of it, at once; remembered. |
| Right-click, Options | The theme, and the sites you turned off. |

nymph runs on every site until you turn one off; there is no list of
sites to allow. Browser pages (`chrome://`, the Web Store) are off
limits to extensions, and the button is disabled there.

## On a chart

Everything the [web viewer](viewer.md#interaction) does: drag to pan,
wheel or pinch to zoom, hover or click for values, right-click to save
as PNG or YAML with your zoom. A badge in the chart's top-right corner,
shown on hover, has **Image** (switch this chart back to its picture,
and **Chart** to return) and **Open** (the image, or its link, in a new
tab). A chart inside a link does not follow the link when you use it.

Options: **As the image** keeps each chart's own theme (it looks like
the PNG); **Follow the browser** draws charts light or dark with your
browser.

## What is replaced

An image is replaced when it is a PNG whose `nymph` chunk comes before
its image data (as the `nymph` CLI and the viewer write it), at least
32 x 32 pixels and shown at least 24 x 24, and the viewer accepts its
source. Corrupt PNGs, sources the viewer rejects and wish nymph images
stay images.

The YAML travels inside the PNG only while the file is copied byte for
byte (README). A site that re-encodes uploaded images drops it, and the
extension then sees a plain PNG.

## Sites

| Site | Status |
|---|---|
| Pages that allow WebAssembly (`'wasm-unsafe-eval'` or no Content Security Policy) | Charts run in the page: one module and one WebGL context for all of them, up to 32 live at once. |
| Pages whose policy forbids it (GitHub and most large sites) | Each chart runs in a small extension frame, up to 12 live at once; a chart starts when it is first on screen. |
| GitHub | Markdown images (READMEs, files, issues, pull requests, comments, previews) are ordinary images, so they work as above. Not yet checked on github.com itself: that GitHub serves uploaded and proxied PNGs unchanged. |
| Google Docs | The editor draws pages on a canvas, so its images cannot be replaced. Views that show images as `<img>` work if Google serves the original bytes (not yet checked). |

Charts far from the screen are freed and come back, with your zoom, when
you scroll back to them.

## Privacy

Nothing leaves your browser. The extension reads again only the image
URLs the page already shows: same-site images with the page's cookies
(as the page itself loads them), images from other sites without
cookies. For an image that is not a nymph PNG it reads only the first
chunk headers, usually from the browser's cache. It needs access to all
sites to run everywhere and to read images across sites. A page can
tell that the extension is installed.

## Limits

- Images inside shadow roots, iframes, `<canvas>` elements and CSS
  backgrounds are not looked at.
- A nymph chunk moved after the image data (by an image optimiser) is
  not found.
- The right-click menu opens inside the chart's frame in frame mode, so
  a very small chart clips it.
