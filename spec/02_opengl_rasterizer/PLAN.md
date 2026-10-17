# nymph — OpenGL Rasterizer: Implementation Plan

See [DESIGN.md](DESIGN.md) for the design this plan implements, and
[spec/01_core_features/DESIGN.md](../01_core_features/DESIGN.md) section 4
(`render_figure()`, `soft_rasterizer`) for the code it plugs into.

Steps are ordered so the seam comes first (nothing changes for users),
then the parts that can be tested without a GPU, then the GPU path on
Linux, then the user-facing switch, then Windows. Every step leaves the
existing tests passing unmodified: the software rasterizer's bytes must
not change. Every test is a GoogleTest file in `tests/`, added with
`nymph_add_test()`.

Tests that need OpenGL create the device with `allow_software_gl = true`
and call `GTEST_SKIP()` with the device's failure reason when there is
none. To run them on a machine without a GPU, install Mesa's EGL
(Debian / Ubuntu: `libegl1 libegl-mesa0 libgl1-mesa-dri`); they then run
on llvmpipe. Each step records in its result whether its GL tests ran or
skipped, and on what device.

## Steps

### 1. Rasterizer seam

*Result: done.* `src/render/raster_backend.hpp` (with `raster_too_large`,
added in step 5), `render_frames()` / `render_figure()` overloads; 4 tests
in `test_figure_renderer.cpp`. `layout_figure()` keeps the name
`nymph_soft_rasterizer` (it never rasterizes). Existing tests unchanged.

**Goal:** `render_frames()` draws through a `raster_backend`, and the
software rasterizer is one; images are byte-identical to before.

**Deliverables:**
- `src/render/raster_backend.hpp`: `raster_backend` (`name()`,
  `rasterize()`), `raster_unavailable : std::runtime_error`, and
  `software_backend` (forwards to `rasterize()` from
  `soft_rasterizer.hpp`, name `nymph_soft_rasterizer`).
- `src/render/figure_renderer.hpp/.cpp`: `render_frames(options, draw,
  raster_backend&)` and `render_figure(f, raster_backend&)`; the existing
  overloads call them with a local `software_backend`. `configure()`
  sets `io.BackendRendererName` from `backend.name()`.
- `src/CMakeLists.txt`: the new header.

**Tests** (`tests/test_figure_renderer.cpp`):
- A recording fake backend is called exactly once per `render_frames()`,
  with draw data whose `DisplaySize * FramebufferScale` is the image
  size, with the theme's opaque window background as the clear colour,
  and its returned image is what `render_frames()` returns.
- `io.BackendRendererName` equals the backend's name while `draw` runs.
- A `raster_unavailable` thrown by the backend propagates out of
  `render_frames()` (the caller decides on fallback), and the previous
  ImGui contexts are current again.
- All existing tests pass unmodified (the software bytes are unchanged).

### 2. Run-time loading and the GL function table

*Result: done.* `shared_library` gained a `pin` flag (kept in the process
after close) for EGL and opengl32; `gl_api` is generated from one X-macro
list; 5 tests in `test_gl_loader.cpp`; `nymph_no_gl_linkage` (CTest)
passes for the Linux binary and, run by hand, for the MinGW `nymph.exe`
(imports GDI32, KERNEL32, msvcrt, USER32 and the MinGW runtime). Checked
by hand that the script fails for an ELF binary linked with `libEGL.so.1`
and a PE binary importing `OPENGL32.dll`.

**Goal:** nymph can open a shared library and fill a table of OpenGL
function pointers at run time, with no OpenGL header or library at build
time, behind the `NYMPH_OPENGL` option.

**Deliverables:**
- `src/render/gl/shared_library.hpp/.cpp`: `shared_library::open(names)`
  (first that loads; on failure the loader's message for each name),
  `symbol(name)`, move-only, closes on destruction. One
  `#if defined(_WIN32)` guard: `LoadLibraryW` / `GetProcAddress` /
  `FreeLibrary` versus `dlopen(RTLD_NOW | RTLD_LOCAL)` / `dlsym` /
  `dlclose`.
- `src/render/gl/gl_api.hpp/.cpp`: GL scalar types, the constants and
  function pointer types `gl_backend` needs (DESIGN.md section 3,
  `gl_api`), `struct gl_api` and `load(lookup, user)` returning the first
  missing name. `APIENTRY` is `__stdcall` on 32-bit Windows, empty
  elsewhere.
- Root `CMakeLists.txt`: `option(NYMPH_OPENGL ... ON)`;
  `src/CMakeLists.txt` adds `src/render/gl/` only when it is on, defines
  `NYMPH_HAS_OPENGL=1` privately, and links `${CMAKE_DL_LIBS}` (POSIX)
  or `user32 gdi32` (`WIN32`).
- `cmake/NymphCheckLinkage.cmake`: a script that runs
  `CMAKE_OBJDUMP -p` (or `dumpbin /dependents` when `MSVC`) on a binary
  and fails if a needed library matches `libGL`, `libEGL`, `libOpenGL`,
  `libGLX`, `libGLESv2` or `opengl32` (case-insensitive).
  `tests/CMakeLists.txt` registers it as test `nymph_no_gl_linkage` on
  the `nymph-cli` executable; skipped with a message if neither tool is
  found.
- `docs/building.md`: the `NYMPH_OPENGL` option, and that no OpenGL
  package is needed to build.

**Tests** (`tests/test_gl_loader.cpp`, built only with `NYMPH_OPENGL`):
- Opening `nymph-no-such-library.so` (`.dll` on Windows) fails, and the
  message names the library.
- Opening the C runtime (`libc.so.6` / `msvcrt.dll`) succeeds and
  `symbol("strlen")` is not null; `symbol("nymph_no_such_symbol")` is
  null.
- `gl_api::load` with a lookup that knows every name fills every pointer
  and returns empty; with a lookup missing `glReadPixels` it returns
  `"glReadPixels"`.
- `nymph_no_gl_linkage` passes. Checked once by hand that it fails when
  `nymph-cli` is linked with `-lGL` (recorded in the result).

### 3. Headless OpenGL device on Linux (EGL)

*Result: done.* `gl_device.cpp` holds the shared part (function table,
version check, software-GL rule, limits). The EGL display is never
terminated (shared per process; DESIGN.md section 3). When every display
fails, the reason that got furthest is reported. 6 tests in
`test_gl_device.cpp` (with `tests/gl_test_device.hpp`); GL tests ran on
Mesa llvmpipe 25.2.8 through EGL (no display server), and skip with the
reason when libEGL is absent.

**Goal:** on Linux, `gl_device::create()` gives a current-able OpenGL 3.3
core context with no window or display server, or a one-line reason why
not.

**Deliverables:**
- `src/render/gl/gl_device.hpp`: `gl_device_options`, `gl_device`
  (`create()` returning `{device, reason}`, `gl()`, `renderer_name()`,
  `max_size()`, `current_scope`), as in DESIGN.md section 3.
- `src/render/gl/gl_device_posix.cpp`: the EGL sequence of DESIGN.md
  section 3 (device platform, then Mesa surfaceless, then default
  display; 3.3 core context; surfaceless or 1x1 pbuffer; GL functions
  through `eglGetProcAddress`, else `libOpenGL.so.0` / `libGL.so.1`);
  `current_scope` saves and restores `eglGetCurrentContext` /
  `Display` / `Surface(EGL_DRAW / EGL_READ)` and the bound API; the
  software-GL check on `GL_RENDERER`; everything released in reverse
  order in the destructor. Reasons are short and specific
  ("libEGL.so.1: cannot open shared object file", "no EGL display",
  "no OpenGL 3.3 core context (EGL error 0x3003)", "software OpenGL
  (llvmpipe) refused").
- `src/render/gl/gl_device_win.cpp`: a stub whose `create()` reports
  "OpenGL on Windows is not implemented yet" (replaced in step 7), so
  every platform builds from this step on.

**Tests** (`tests/test_gl_device.cpp`):
- With `library_names = {"nymph-no-such-libEGL.so"}`, `create()` returns
  no device and a reason naming that library. (Always runs.)
- With `allow_software_gl = true` (skips without a device): the device
  reports a non-empty renderer name and `max_size() >= 1024`; inside a
  `current_scope`, `glGetString(GL_VERSION)` starts with a version
  `>= 3.3`; after the scope, no EGL context is current.
- A context made current before the scope (a second device) is current
  again after it, also when the scope's body throws.
- Creating and destroying a device 20 times in a row works (no leaked
  display or library handle breaking later creation).
- On llvmpipe with `allow_software_gl = false`, `create()` fails with
  the "software OpenGL" reason (skips on a hardware GPU).

### 4. OpenGL rasterizer

*Result: done.* Departures: rows are not reversed after the read (the
projection draws the top edge into framebuffer row 0) and alpha is not
forced (DESIGN.md section 3); textures are uploaded when a command first
uses them. `tests/raster_scenes.hpp` (builder, textures, 14 scenes),
`tests/raster_compare.hpp`, 7 tests in `test_gl_rasterizer.cpp`. Measured
on llvmpipe: scenes worst 100 % within 2, max 1, mean 0.09; the ten
examples plus `scale: 2` and dark variants of two worst max 44, mean
0.0044, 100.000 % within 2 (rounded). Thresholds kept, with the share
rule relaxed to "all but one pixel" for images under 200 pixels. Not
measured on a hardware GPU (none available where this was built).

**Goal:** `gl_backend` turns ImGui draw data into an image that matches
the software rasterizer within the tolerance of DESIGN.md section 6.

**Deliverables:**
- `src/render/gl_rasterizer.hpp/.cpp`: `gl_backend : raster_backend`
  (name `nymph_opengl3`) over a `gl_device&`, as in DESIGN.md section 3,
  `gl_backend`: FBO target, texture upload (Alpha8 as swizzled `GL_R8`),
  the GLSL 330 program, ImGui's blend state, scissor per command,
  `glDrawElementsBaseVertex`, read back with rows reversed and alpha
  255, `raster_unavailable` for size limits, compile / link / FBO / GL
  errors, `std::runtime_error` for unknown textures and callbacks, every
  GL object freed on all paths.
- `tests/raster_compare.hpp`: `compare(a, b)` returning the share of
  pixels within 2 per channel, the largest channel difference and the
  mean absolute difference; `expect_close(a, b)` asserting the DESIGN.md
  thresholds and, on failure, writing both images and a difference image
  named after the current test to the build directory.
- Move the hand-built scenes of `test_soft_rasterizer.cpp` into
  `tests/raster_scenes.hpp` so both rasterizers' tests use the same
  scenes (the software tests keep their exact assertions).

**Tests** (`tests/test_gl_rasterizer.cpp`; skip without a device):
- Every scene in `raster_scenes.hpp` (rectangles, shared-edge triangles
  and the fan, clipping, blending cases, textured 1:1 and half-texel
  quads, the moved white pixel, `VtxOffset` over 65,536 vertices, two
  textures, `FramebufferScale = 2`) is `expect_close` to the software
  image, with the images' sizes equal.
- Exact checks where GPUs agree: an opaque axis-aligned rectangle on
  whole pixels has exactly its colour and changes nothing else; the
  clear colour fills an empty frame exactly.
- A reset-render-state callback is ignored; any other callback throws
  `std::runtime_error`, not `raster_unavailable`; an unknown texture id
  throws `std::runtime_error`.
- An image larger than `max_size()` (a hand-built 1x1 display with a
  framebuffer scale above it) throws `raster_unavailable`.
- Rasterizing the same draw data twice on one device gives identical
  bytes.
- `render_figure()` with a `gl_backend` for every `examples/*.yaml` is
  `expect_close` to the software image (also at `scale: 2` and in the
  dark theme for two of them). Record the measured worst values for
  llvmpipe and for one hardware GPU in this step's result, and tighten
  or loosen the DESIGN.md thresholds (update DESIGN.md first) so they
  hold on both with margin.

### 5. `renderer` and fallback

*Result: done.* `rasterizer`, `rasterizer_name()`, `renderer`,
`probe_opengl()` in `nymph.hpp`; `src/render/renderer_options.hpp`
(`renderer_test_options`, `make_renderer()`); `raster_too_large` tells the
per-image fallback from the permanent one; `gl_backend::limit_size()` lets
the over-size test run on any device. 8 tests in `test_renderer.cpp`.

**Goal:** library users choose the rasterizer, and every OpenGL failure
falls back to software as the table in DESIGN.md section 3 says.

**Deliverables:**
- `src/nymph.hpp/.cpp`: `enum class rasterizer`, `class renderer`
  (pimpl), `probe_opengl()`; the free `render_png()` unchanged in
  behaviour (it uses a software renderer). With `NYMPH_OPENGL` off,
  `renderer(opengl)` and `probe_opengl()` report "nymph was built
  without OpenGL support".
- `src/render/` (internal): a way to pass `gl_device_options` to a
  `renderer` for tests (an internal constructor or factory in a
  non-public header, e.g. `src/render/renderer_internal.hpp`), so tests
  can force a missing library or allow software GL.
- Doxygen on every new public declaration, with the fallback rules.

**Tests** (`tests/test_renderer.cpp`):
- `renderer()` and `renderer(rasterizer::software)` give exactly the
  bytes of the free `render_png()` for every example, and
  `fallback_reason()` stays empty. (Always runs.)
- With a missing EGL / opengl32 library forced: `active()` and
  `last_used()` are `software`, `fallback_reason()` names the library,
  and the PNG is byte-identical to the software one. (Always runs.)
- A source error throws `nymph::error` with its position from every
  renderer, before any rasterizer runs.
- With software GL allowed (skips without a device): `active()` and
  `last_used()` are `opengl`, `device_name()` is not empty, the PNG
  carries the source byte for byte, and its pixels are `expect_close` to
  the software PNG's.
- An over-size image (`width: 4096, scale: 2` when `max_size()` is
  below 8192; skipped otherwise) renders with `last_used() == software`
  and the next normal image with `opengl`.
- A fault injected after device creation (an internal test hook that
  makes `gl_backend` throw `raster_unavailable` once) gives a software
  image, then `active() == software` for later calls.
- A moved-from renderer can be destroyed; the moved-to one renders.

### 6. Command line

*Result: done.* `--rasterizer` anywhere among the arguments; also a
usage error on an implicit extract (`nymph x.png --rasterizer opengl`).
`run_cli` overload with `renderer_test_options`. 6 tests in
`test_cli.cpp`. Manual: with `libEGL.so.1` moved away, `nymph
rasterizers` reports it missing and `nymph examples/dashboard.yaml` writes
a PNG identical to the committed one; `ldd nymph` lists no GL library.

**Goal:** `nymph` renders with OpenGL by default, falls back silently,
warns only when OpenGL was requested by name, and can report what is
available.

**Deliverables:**
- `src/cli/cli.cpp`: `--rasterizer <software|opengl>` on `render` and
  the implicit render (parsed for every command so misuse can be
  reported; an error on `check`, `extract`, `schema`, `fonts`); a
  `renderer` built from it, only for commands that render, after every
  argument and input problem has been reported; the warning line of
  DESIGN.md section 4 for an explicit `opengl` that fell back;
  `nymph rasterizers` (no other arguments). Usage text updated.
- `docs/cli.md`: the option, the default, the fallback and warning, the
  `rasterizers` command, and that `--rasterizer software` is the choice
  for byte-identical output across machines.
- `docs/agent-guide.md`: one short note (nothing to do; use
  `--rasterizer software` when comparing images byte for byte).
- `README.md`: the intro and Quick Start say nymph uses the GPU when
  there is one and needs none; `CHANGELOG.md` entries.

**Tests** (`tests/test_cli.cpp`; all always run, the OpenGL outcome is
not assumed):
- `--rasterizer software` writes exactly the bytes of `render_png()`.
- `--rasterizer vulkan` and a missing value exit 2 with the usage;
  `check --rasterizer software` exits 2.
- Default render: exit 0, empty stderr, a PNG that carries the source,
  whether OpenGL was used or not.
- With the OpenGL library forced missing (an internal hook the test can
  set on `run_cli`'s renderer factory): the default prints nothing;
  `--rasterizer opengl` prints exactly one line starting
  `nymph: warning: OpenGL unavailable (` and exits 0 with the software
  bytes.
- `nymph rasterizers` exits 0 and prints `software: available` and one
  `opengl: ` line that matches `probe_opengl()`.
- `nymph rasterizers foo` exits 2.
- `nymph_no_gl_linkage` still passes, and `nymph examples/line.yaml`
  works with `LD_LIBRARY_PATH` pointing at an empty directory and Mesa's
  EGL uninstalled or hidden (manual check on a machine without
  `libEGL.so.1`, recorded in the result).

### 7. Windows and MSYS2 (WGL)

*Result: done, verified by cross build and Wine only.* Built with MinGW-w64
(GCC 13, the MSYS2 toolchain family) from Linux; every test executable run
under Wine 9 passes (one pre-existing symlink test skips on Windows). With
a virtual X server, Wine's opengl32 reaches Mesa llvmpipe, so the WGL path
(hidden window, pixel format, `wglCreateContextAttribsARB`, lookups
through `wglGetProcAddress` / `GetProcAddress`, context restore) ran for
real: the GL tests passed with the same differences as on Linux. Without
a display, `nymph rasterizers` reports "no hidden window for OpenGL" and
the GL tests skip. Still to do on real machines: native MSVC (`dumpbin`
linkage test, `/W4`), MSYS2, and a hardware GPU on Windows.

**Goal:** the same OpenGL path works on native Windows (MSVC) and MSYS2,
loaded from `opengl32.dll` at run time, with the same fallback.

**Deliverables:**
- `src/render/gl/gl_device_win.cpp`: the WGL sequence of DESIGN.md
  section 3 (hidden `CS_OWNDC` window, pixel format, legacy context,
  `wglCreateContextAttribsARB` 3.3 core, GL 1.1 functions through
  `GetProcAddress`, the rest through `wglGetProcAddress` with its
  failure values), `current_scope` saving and restoring
  `wglGetCurrentContext` / `wglGetCurrentDC`, the software-GL check
  (`GDI Generic`), window class unregistered and window destroyed in the
  destructor.
- `shared_library` and `gl_api` reviewed for MSVC (calling convention,
  `FARPROC` to function pointer casts without warnings at `/W4`).
- `docs/building.md`: Windows notes (nothing to install; a remote
  desktop session or VM without a GPU driver falls back).

**Tests:**
- Every test of steps 2-6 builds and passes with MSVC and with MSYS2
  (GL tests run on a machine with a GPU driver; record the device).
- `nymph_no_gl_linkage` passes with `dumpbin` (MSVC) and `objdump`
  (MSYS2): `opengl32.dll` is not an import of `nymph.exe`.
- Manual: `nymph rasterizers` on a machine with a GPU, and in a VM or
  remote session without one, gives `available` and a reason
  respectively; `nymph examples/dashboard.yaml` works in both.

### 8. Measurements and documentation

*Result: done; the timing question is open with the user.* No hardware
GPU was available, so the timings use llvmpipe (4 cores) as the OpenGL
device, release build:

| Example | software render_png | llvmpipe device creation | llvmpipe render (first / again) | software rasterization alone |
|---|---|---|---|---|
| `line` | 30 ms | 35 ms | 50 / 28 ms | 4 ms |
| `dashboard` | 42 ms | 11 ms | 58 / 45 ms | 6 ms |
| `surface`, `scale: 2` | 145 ms | 9 ms | 148 / 126 ms | 31 ms |

The last column is the most a faster rasterizer can save: binding, frame
generation and above all PNG encoding (stb, level 8) take the rest. A
hardware GPU cannot make `nymph` much faster per image, and creating its
context (tens of ms on NVIDIA's EGL) can cost more than it saves on small
charts. The CLI default stays `opengl` as requested; whether to keep it
is raised with the user.

**Goal:** the project's claims are measured and every document matches the
code.

**Deliverables:**
- A timing of `nymph render` for three examples (small, `dashboard`,
  `scale: 2` surface) with each rasterizer on one GPU machine and on the
  software fallback, recorded in this step's result (wall time, and how
  much of it is device creation). If OpenGL is not faster on any of
  them, record it and raise with the user whether the CLI default should
  stay `opengl` before closing the project.
- `spec/01_core_features/DESIGN.md`: sections 1 and 4 (`render_figure()`
  takes a backend; "no GPU" now means "no GPU needed") and section 8's
  linkage statement corrected to point here.
- `spec/02_opengl_rasterizer/DESIGN.md`: status, and any change made
  during implementation.
- `spec/INDEX.md`: status "Done" and topics.
- `README.md`, `docs/`, `CHANGELOG.md` checked against the code.

**Tests:**
- `ctest -j` passes three times in a row on Linux with and without Mesa
  EGL installed.

## Completion Criteria

- [x] All tests pass under `ctest -j` three times in a row, with OpenGL
      available (GL tests run) and without it (GL tests skip). *Linux:
      230 tests, three runs with Mesa llvmpipe through EGL, one with
      libEGL removed.*
- [ ] Builds and tests pass on Linux (GCC), MSYS2 and native Windows
      (MSVC), and with `-DNYMPH_OPENGL=OFF`. *Linux (GCC) and
      `NYMPH_OPENGL=OFF` (212 tests) verified; a MinGW-w64 cross build
      passes every test under Wine 9. Native MSVC and MSYS2 not yet.*
- [x] The `nymph` executable lists no OpenGL, EGL, GLX or `opengl32`
      library among its dependencies (`nymph_no_gl_linkage`), and renders
      on a machine where none is installed. *Linux by test and with
      libEGL removed; the MinGW `nymph.exe` checked by hand.*
- [ ] `nymph chart.yaml` uses OpenGL when a hardware GPU is available,
      and otherwise falls back silently with exit 0; `--rasterizer
      opengl` warns on fallback; `nymph rasterizers` says why. *The
      fallback, warning and report are verified; use of a hardware GPU is
      not (none available), only the same path on llvmpipe.*
- [x] `--rasterizer software` and the library's `render_png()` give the
      same bytes as before this project for every example. *All ten
      committed example PNGs are reproduced byte for byte.*
- [x] OpenGL images of every example match the software images within
      the DESIGN.md tolerance. *On llvmpipe, through EGL and through WGL
      under Wine.*
- [x] README, `docs/`, CHANGELOG, both projects' DESIGN.md and
      `spec/INDEX.md` match the code.
