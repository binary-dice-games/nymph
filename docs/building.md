# nymph — Building

nymph targets **Linux, MSYS2, and native Windows (MSVC)**.

---

## Prerequisites

| Tool | Minimum version | Notes |
|------|----------------|-------|
| CMake | 3.14 | |
| C++ compiler | C++20 | GCC 10+, Clang 12+, MSVC 2022 |
| Git | any | Required to check out submodules |

Dependencies live in `extern/` as git submodules and are compiled from
source; no system packages are needed. Configuring from a git checkout
initializes them, so `git submodule update --init --recursive` is
optional.

| Submodule | Version | Used for |
|-----------|---------|----------|
| `extern/imgui` | v1.92.9b | Frame generation and fonts (no ImGui backend: nymph has its own CPU and OpenGL rasterizers) |
| `extern/implot` | v1.0 | 2D plots |
| `extern/implot3d` | v0.4 | 3D plots |
| `extern/libyaml` | 0.2.5 | Parsing the YAML source |
| `extern/stb` | `31c1ad3` | PNG encoding, zlib |
| `extern/json` | v3.12.0 | JSON for the MCP server (`NYMPH_MCP`), header-only |
| `extern/googletest` | v1.15.2 | Tests only (`NYMPH_BUILD_TESTS`) |

To move a dependency to another version, check out the new commit inside
the submodule **and stage it** (`git add extern/<name>`): configuring runs
`git submodule update`, which resets every submodule to the staged
commit.

### MSYS2 (Windows host)

From an **MSYS2 MSYS** shell:

```bash
pacman -S --needed mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-gcc
```

Configuring and building from there is identical to Linux.

### Native Windows (MSVC)

Use a *Developer PowerShell for VS 2022* (or run `vcvars64.bat`) so `cl.exe`
is on `PATH`, then build with the Ninja or Visual Studio generator.

---

## Build

```sh
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure -j
```

Outputs:

| Target | Output |
|--------|--------|
| `nymph` | Static library |
| `nymph_imgui`, `yaml` | Static libraries of the dependencies |
| `nymph-cli` | `build/app/nymph` executable |
| `test_*` | GoogleTest executables under `build/tests/` |

---

## CMake options

| Option | Default | Description |
|--------|---------|-------------|
| `NYMPH_BUILD_TESTS` | `ON` when top-level | Build the GoogleTest suite |
| `NYMPH_FONTS_DIR` | `fonts/` | Folder whose `*.ttf` / `*.otf` fonts and `*.txt` licences are compiled into nymph |
| `NYMPH_EMBED_FONTS` | `all` | Which of those fonts have their bytes compiled in: `all`, `none`, or a `;`-list of file names |
| `NYMPH_WEB` | `OFF` | Build the interactive web viewer (WebAssembly) instead of the CLI; needs Emscripten (`emcmake`). See "Web viewer" below. |
| `NYMPH_MCP` | `ON` | Build the MCP server, `nymph mcp` (off in the web build). `OFF` drops the `extern/json` dependency; `nymph mcp` then exits 2. |
| `NYMPH_MCP_VIEWER` | empty | The MCP App page to embed in `nymph mcp`, `build-web/claude/viewer.html` from the web build (below); empty: the server returns PNGs only. |
| `NYMPH_OPENGL` | `ON` | Build the optional OpenGL rasterizer. `OFF` leaves only the software rasterizer; `--rasterizer opengl` then falls back with the reason "nymph was built without OpenGL support". |

---

## OpenGL rasterizer

Building it needs **no OpenGL package, header or SDK**: nymph declares the
few GL, EGL and WGL functions it uses itself and opens the libraries at
run time, so the executable links none of them (the `nymph_no_gl_linkage`
test checks this with `objdump` or `dumpbin`) and runs on machines with no
OpenGL at all.

At run time it uses, when present:

- **Linux:** `libEGL.so.1` (headless: no X or Wayland display needed) and
  a GPU driver with OpenGL 3.3. Mesa's CPU drivers (llvmpipe, softpipe)
  are not used: they are slower than nymph's own rasterizer.
- **Windows / MSYS2:** `opengl32.dll` and a GPU driver with OpenGL 3.3. A
  remote desktop session or VM without a GPU driver falls back to the
  software rasterizer.

`nymph rasterizers` prints whether OpenGL is available and, if not, why.

The OpenGL tests run on any OpenGL 3.3 device, Mesa llvmpipe included, and
skip (with the reason) when there is none. To run them on a Linux machine
without a GPU, install Mesa's EGL, e.g. on Debian / Ubuntu:

```sh
sudo apt install libegl1 libegl-mesa0 libgl1-mesa-dri
```

---

## Embedded fonts

The fonts in `fonts/` are compiled into the library: at build time
`cmake/NymphFonts.cmake` turns each file into a generated C++ array
(`build/src/generated/fonts/`), so the binary reads no font file at run
time. Sources name a font by its file name.

- **Add a font:** copy its `.ttf` / `.otf` and its licence text (as
  `<Family>-<Licence>.txt`) into `fonts/` and build. The configure step
  re-runs by itself and picks it up; `nymph fonts` lists it.
- **Use another set:** `cmake -S . -B build -DNYMPH_FONTS_DIR=/path/to/fonts`.
  An empty folder leaves only the built-in `default` font; tests that name
  the standard fonts then skip.
- Each font adds its size to the binary (the standard set is 2.8 MB).
  File names must not contain `"` or `\`.
- **Embed only some fonts:** `-DNYMPH_EMBED_FONTS=none` (or a `;`-list
  such as `"FiraSans-Regular.ttf;FiraSans-Bold.ttf"`). Every font name
  stays valid in sources; a font without bytes must be registered at run
  time with `bdg::nymph::register_font(name, bytes)` (checked against its
  size and SHA-256) before a source that names it can render, or
  rendering fails at its `font` field. The fonts not embedded are copied,
  with the licences, to `build/fonts/` for hosting. `nymph fonts` marks
  them `(not embedded)`. The web viewer uses this to load fonts on demand
  ([spec/04_web_viewer](../spec/04_web_viewer/DESIGN.md)).
- Licences: the standard fonts are SIL OFL 1.1 (see `fonts/README.md`);
  `nymph fonts --licenses` prints the texts compiled in with them.

---

## Web viewer

The web viewer is the library compiled to WebAssembly with a JavaScript
API ([spec/04_web_viewer](../spec/04_web_viewer/DESIGN.md)). It needs the
[Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html)
(verified with 6.0.12) and, for its tests, Node.

```sh
emcmake cmake -S . -B build-web -DNYMPH_WEB=ON -DCMAKE_BUILD_TYPE=MinSizeRel
cmake --build build-web
ctest --test-dir build-web        # Node tests of the module
```

`build-web/web/` then holds `nymph-viewer.js` (the API),
`nymph-viewer-core.js` and `nymph-viewer-core.wasm` (about 630 KB gzip),
`nymph-viewer.single.js` (all three in one file, about 690 KB gzip),
`fonts/`, the font files fetched on demand (the web build embeds no font:
`NYMPH_EMBED_FONTS` defaults to `none` there), and `demo/index.html`,
every example live next to its PNG (serve the folder over HTTP to open
it). See [viewer.md](viewer.md) for what to serve.

It also writes `build-web/claude/viewer.html`, the MCP App that shows
`nymph mcp`'s charts live in a Claude conversation. Embed it in the
native build to serve it:

```sh
cmake -S . -B build -DNYMPH_MCP_VIEWER=$PWD/build-web/claude/viewer.html
cmake --build build
```

The same build assembles the Chrome extension: `build-web/extension/nymph/`
(load it unpacked from `chrome://extensions`) and
`build-web/extension/nymph-extension-<version>.zip`. See
[extension.md](extension.md). `ctest` also runs the
browser tests, the extension's included, when Playwright for Node is
installed (they are skipped otherwise). `NYMPH_WEB` turns `NYMPH_OPENGL` and the native GoogleTest suite
off, uses native wasm exceptions (`-fwasm-exceptions`) and 32-bit ImGui
indices, and builds no CLI.

## Using nymph from another CMake project

```cmake
add_subdirectory(extern/nymph)
target_link_libraries(my_app PRIVATE nymph::nymph)
```

Tests and the `nymph` executable are skipped when nymph is not the
top-level project.

---

## Formatting

Format C++ sources with `clang-format` using the repository's
`.clang-format` (`format-all.ps1` formats the whole tree on Windows).
