# nymph — OpenGL Rasterizer: Design

**Status: designed, not started.** [PLAN.md](PLAN.md) is the ordered
implementation plan. This document describes only what this project adds
or changes; the rest of the architecture is
[spec/01_core_features/DESIGN.md](../01_core_features/DESIGN.md)
(referred to below as *core DESIGN*).

## 1. Purpose / Scope

nymph draws every image on the CPU with its own software rasterizer
(core DESIGN section 4, `soft_rasterizer`). This project adds a second
rasterizer that draws the same ImGui draw data with OpenGL, so a machine
with a GPU renders through it, and makes the choice a command-line option:

```sh
nymph chart.yaml                          # OpenGL if available, else software
nymph chart.yaml --rasterizer opengl      # the same, with a warning on fallback
nymph chart.yaml --rasterizer software    # always the CPU (byte-exact, as today)
nymph rasterizers                         # what this machine can use, and why not
```

Hard requirements, from the request:

1. **Selectable**: `--rasterizer software|opengl`; the CLI default is
   `opengl`.
2. **Fallback**: if OpenGL cannot be used for any reason (no library, a
   missing function, no device, no 3.3 context, a shader or framebuffer
   failure), the image is rendered by the software rasterizer and the
   command still succeeds.
3. **No link-time OpenGL dependency**: the `nymph` executable must start
   and work on a machine with no OpenGL at all. No `libGL`, `libEGL`,
   `libOpenGL`, `libGLX` or `opengl32.dll` appears among its linked
   libraries; they are opened at run time (`dlopen` / `LoadLibraryW`) and
   every function is looked up by name (`dlsym` / `eglGetProcAddress`,
   `GetProcAddress` / `wglGetProcAddress`).

In scope: the rasterizer interface, the OpenGL rasterizer, the run-time
loader and headless context creation for Linux (EGL) and Windows / MSYS2
(WGL), the fallback rules, the CLI option and command, a library API to
choose the rasterizer, and the build option that leaves it out.

Out of scope: OpenGL ES, GLX, macOS / CGL, Vulkan or Direct3D, rendering
several images in one GPU batch, and any change to the source format.
Section 9 lists how the excluded APIs would fit.

## 2. Design Goals

1. **Same picture.** The OpenGL image matches the software image within a
   small tolerance (section 6): same geometry, same blend equation, same
   texture sampling rule, same clear colour. A user should not be able to
   tell which rasterizer drew a chart.
2. **Never worse than today.** Whatever happens with OpenGL, nymph
   produces the image the software rasterizer would have produced, with
   exit code 0. OpenGL problems are never user errors.
3. **No new build dependency.** Building nymph needs no OpenGL headers,
   libraries or SDK: nymph declares the few GL / EGL / WGL types,
   constants and function pointer types it uses itself.
4. **The frame driver does not change.** Frames, fonts, layout and the
   ImGui texture protocol stay exactly as in core DESIGN section 4
   (`render_figure()`); only the final "draw data → pixels" call is
   swappable.
5. **Determinism where it was promised.** `--rasterizer software` (and the
   library's default) keeps core DESIGN goal 3: same source and build,
   same bytes. The OpenGL path is repeatable on one machine and driver,
   not across GPUs (section 7).

## 3. Key Abstractions

```
                 render_frames(options, draw, backend)      (figure_renderer)
                         │  frames, fonts, texture_store — unchanged
                         ▼
                 raster_backend::rasterize(draw_data, textures, clear)
                    ┌────┴──────────────────────┐
          software_backend                 gl_backend
          (soft_rasterizer)                    │ owns
                                          gl_device ── gl_api (function table)
                                               │            ▲
                                    platform context        │ looked up through
                                    EGL (posix) / WGL (win) │
                                               └── shared_library (dlopen / LoadLibraryW)
```

### `raster_backend` (`src/render/raster_backend.hpp`)

```cpp
class raster_backend {
 public:
  virtual ~raster_backend() = default;
  /// ImGui's BackendRendererName while frames run.
  virtual const char* name() const noexcept = 0;
  /// Same contract as soft_rasterizer's rasterize(): an image of
  /// round(DisplaySize * FramebufferScale), cleared to clear_color.
  /// @throws raster_unavailable when this backend cannot draw this image
  ///         (the caller falls back); std::runtime_error for bad draw data
  ///         (unknown texture, unknown callback), as rasterize() does.
  virtual image rasterize(const ImDrawData&, const texture_store&, ImU32 clear_color) = 0;
};
```

`software_backend` forwards to the existing `rasterize()`. `render_frames()`
and `render_figure()` gain a `raster_backend&` parameter (an overload
without it uses a `software_backend`, so existing callers and tests are
unchanged), and `configure()` takes the backend's name for
`BackendRendererName` instead of the constant `nymph_soft_rasterizer`.

`raster_unavailable` (derived from `std::runtime_error`) is the only
exception that means "fall back". Bad draw data is a nymph bug and must
not be hidden by a silent fallback, so it keeps its own exception and
propagates from either backend.

### `shared_library` (`src/render/gl/shared_library.hpp/.cpp`)

A move-only handle to a library opened at run time: `open(names)` tries
each name in order and keeps the first that loads (the error text of the
last failure otherwise); `symbol(name)` returns a `void*` or null;
the destructor closes it. It is the only code that calls `dlopen` /
`dlsym` / `dlclose` or `LoadLibraryW` / `GetProcAddress` / `FreeLibrary`,
behind one narrow `#if defined(_WIN32)` guard (CLAUDE.md, Platform
Support). On Linux it links `${CMAKE_DL_LIBS}` (libdl on glibc older
than 2.34, nothing newer), which is not an OpenGL library.

### `gl_api` (`src/render/gl/gl_api.hpp/.cpp`)

nymph's own GL declarations: the scalar types (`GLuint`, `GLenum`, ...),
the ~40 constants and the ~35 function pointer types of the functions the
rasterizer calls (OpenGL 3.3 core), gathered in one struct:

```cpp
struct gl_api {
  PFN_glGenTextures GenTextures = nullptr;
  ...
  /// Fills every pointer through `lookup`; returns the name of the first
  /// one that is null, or an empty string when all were found.
  std::string load(void* (*lookup)(const char* name, void* user), void* user);
};
```

Nothing includes `<GL/gl.h>`, `<EGL/egl.h>` or `<windows.h>`'s `wingdi.h`
GL parts for GL purposes: the headers may not exist on the build machine
(goal 3), and their prototypes would invite a direct call, which would be
a link-time dependency. A direct call to a GL function name anywhere in
nymph is a bug; the linkage test of section 6 catches it.

The function list is exactly what `gl_backend` uses (textures, buffers,
vertex arrays, shaders and programs, framebuffer objects, blending,
scissor, viewport, `glDrawElementsBaseVertex`, `glReadPixels`,
`glGetString`, `glGetIntegerv`, `glGetError`, `glFinish`, `glPixelStorei`,
`glTexParameteri`).

### `gl_device` (`src/render/gl/gl_device.hpp`, `gl_device_posix.cpp`, `gl_device_win.cpp`)

Owns the loaded libraries, an OpenGL 3.3 core context with no window
surface, and the filled `gl_api`. Platform-specific creation lives in two
suffixed files, chosen by CMake: the EGL and WGL paths share no code
beyond the interface, so one file with guards would be mostly `#if`
(CLAUDE.md, Platform Support). MSYS2 defines `_WIN32` and has
`opengl32.dll`, so it takes the WGL file.

```cpp
struct gl_device_options {
  /// Accept a software OpenGL (Mesa llvmpipe / softpipe, SwiftShader,
  /// Microsoft's GDI Generic). Off for users: a CPU GL is slower than
  /// nymph's own rasterizer and gains nothing. Tests turn it on.
  bool allow_software_gl = false;
  /// Library names to try instead of the platform defaults (tests).
  std::vector<std::string> library_names;
};

class gl_device {
 public:
  struct result {
    std::unique_ptr<gl_device> device; ///< null on failure
    std::string reason;                ///< why, when device is null
  };
  /// @brief Never throws for an OpenGL problem; reports it in `reason`.
  static result create(const gl_device_options&);
  const gl_api& gl() const noexcept;
  std::string renderer_name() const;  ///< GL_VENDOR + GL_RENDERER + GL_VERSION
  int max_size() const noexcept;      ///< min(MAX_TEXTURE_SIZE, MAX_RENDERBUFFER_SIZE, MAX_VIEWPORT_DIMS)
  /// Makes the context current for the scope and restores whatever context
  /// (EGL or WGL) was current on this thread before.
  class current_scope;
};
```

(A plain result struct, not `std::expected`: nymph is C++20.)

**Linux (EGL).** No X or Wayland display is needed, which matters: nymph
runs headless.

1. `dlopen("libEGL.so.1")` (then `libEGL.so`). Look up
   `eglGetProcAddress` and the EGL 1.4 core entry points with `dlsym`;
   extension entry points through `eglGetProcAddress`.
2. Pick a display, first that initializes:
   `eglGetPlatformDisplay(EGL_PLATFORM_DEVICE_EXT, device)` for each
   device from `eglQueryDevicesEXT` (headless NVIDIA and Mesa GPUs),
   then `EGL_PLATFORM_SURFACELESS_MESA`, then
   `eglGetDisplay(EGL_DEFAULT_DISPLAY)`.
3. `eglBindAPI(EGL_OPENGL_API)`; choose a config with
   `EGL_RENDERABLE_TYPE = EGL_OPENGL_BIT`; create a 3.3 core context
   (EGL 1.5 attributes, or `EGL_KHR_create_context`).
4. Make it current with no surface (`EGL_KHR_surfaceless_context`), or
   with a 1x1 pbuffer when that extension is missing.
5. GL functions through `eglGetProcAddress` (valid for core functions
   with EGL 1.5 or `EGL_KHR_get_all_proc_addresses`); otherwise `dlsym`
   on `libOpenGL.so.0`, then `libGL.so.1`.

GLX is not tried: it needs an X server, which a headless agent machine
does not have, and a machine with an X server and a GPU has EGL too.

**Windows and MSYS2 (WGL).**

1. `LoadLibraryW(L"opengl32.dll")`; the `wgl*` functions and the OpenGL
   1.1 functions through `GetProcAddress`; everything newer through
   `wglGetProcAddress` (whose failure values `0`, `1`, `2`, `3` and `-1`
   all mean "not found").
2. A hidden window (`CreateWindowExW`, class with `CS_OWNDC`, never
   shown) for a device context; `ChoosePixelFormat` / `SetPixelFormat`
   with a plain RGBA8 `PIXELFORMATDESCRIPTOR`. `user32` and `gdi32` are
   ordinary Windows system libraries present on every Windows install
   (Server Core included) and are linked normally; they are not OpenGL.
3. A legacy context to obtain `wglCreateContextAttribsARB`, then the 3.3
   core context; the legacy one is deleted. A missing
   `wglCreateContextAttribsARB` (Microsoft's GDI Generic, remote
   sessions without a GPU driver) is a failure with that reason.

**Software GL is refused by default.** After the context is current,
`GL_RENDERER` is matched against `llvmpipe`, `softpipe`, `SwiftShader`
and `GDI Generic` (and an EGL device reporting
`EGL_MESA_device_software` is skipped). The request is "use the GPU when
there is one"; Mesa's CPU drivers are slower than nymph's own
rasterizer and give a less exact image. Tests set `allow_software_gl` so
the OpenGL path runs on machines and CI runners without a GPU.

### `gl_backend` (`src/render/gl_rasterizer.hpp/.cpp`)

The OpenGL implementation of `raster_backend`, platform-independent, with
the structure of ImGui's `imgui_impl_opengl3.cpp` (not compiled: it has its
own loader that links or opens `libGL` itself, and it assumes a window).

Per `rasterize()` call, inside a `gl_device::current_scope`:

- **Target**: a framebuffer object with an `GL_RGBA8` colour texture of
  `round(DisplaySize * FramebufferScale)`; a size above
  `gl_device::max_size()` throws `raster_unavailable` (that one image
  falls back). Not sRGB, no multisampling, `GL_DITHER` and
  `GL_FRAMEBUFFER_SRGB` off. Cleared to `clear_color / 255`.
- **Textures**: every texture in the `texture_store` is uploaded fresh
  (`GL_RGBA8` for RGBA32, `GL_R8` with swizzle `(1, 1, 1, R)` for
  Alpha8, which is how ImGui's GPU backends and the software rasterizer
  read Alpha8: white with that alpha). `GL_LINEAR`, `GL_CLAMP_TO_EDGE`,
  `GL_UNPACK_ALIGNMENT` 1. The atlas is small, so uploading per image
  keeps the backend stateless between images; texture ids map to GL
  names through a per-call table, and an unknown id throws
  `std::runtime_error` as the software rasterizer does.
- **Program**: GLSL `330 core`, ImGui's own shader: position through an
  orthographic projection of `DisplayPos .. DisplayPos + DisplaySize`,
  output `vertex colour * texture(uv)`. Compiled once per `gl_backend`;
  a compile or link failure is `raster_unavailable` with the info log.
- **State**: blending `GL_FUNC_ADD`, `SRC_ALPHA, ONE_MINUS_SRC_ALPHA` on
  colour and `ONE, ONE_MINUS_SRC_ALPHA` on alpha (the equation of core
  DESIGN's `soft_rasterizer`), no depth, stencil or culling, scissor on.
- **Commands**: one vertex and index buffer per draw list
  (`GL_STREAM_DRAW`); for each command the clip rectangle is mapped with
  `(clip - DisplayPos) * FramebufferScale`, clamped to the target, and
  set as the scissor (with the Y flip of a bottom-left origin); then
  `glDrawElementsBaseVertex` with `IdxOffset` and `VtxOffset`, 16- or
  32-bit indices to match `sizeof(ImDrawIdx)`. Reset-render-state
  callbacks are skipped; any other callback throws, as in the software
  rasterizer.
- **Read back**: `glFinish`, `glReadPixels(GL_RGBA, GL_UNSIGNED_BYTE)`
  with `GL_PACK_ALIGNMENT` 1, rows reversed into the top-first `image`.
  Alpha is forced to 255 (the software path's target is opaque and
  stays opaque; a driver could round `1 - a` differently).
- **Errors**: `glGetError` after the upload and after the read; an
  error, or an incomplete framebuffer, throws `raster_unavailable`.
- Every GL object created for the call is deleted before it returns,
  also when it throws.

### `renderer` — the library API (`src/nymph.hpp`)

```cpp
namespace bdg::nymph {

/// @brief The rasterizers nymph can draw with.
enum class rasterizer { software, opengl };

/// @brief Renders sources with a chosen rasterizer, keeping an OpenGL
///        context between calls.
class renderer {
 public:
  /// @param requested `opengl` tries to create an OpenGL device now;
  ///        on failure the renderer uses `software` and fallback_reason()
  ///        says why. Never throws for OpenGL problems.
  explicit renderer(rasterizer requested = rasterizer::software);
  ~renderer();                        // destroys the context, if any
  renderer(renderer&&) noexcept;
  renderer& operator=(renderer&&) noexcept;

  /// @brief As the free render_png(); OpenGL problems during the call
  ///        render that image with the software rasterizer instead.
  /// @throws as the free render_png().
  std::vector<uint8_t> render_png(std::string_view source);

  rasterizer requested() const noexcept;
  /// @brief What the next render_png() will try.
  rasterizer active() const noexcept;
  /// @brief What drew the last image (equals active() unless that image
  ///        fell back, e.g. it was larger than the GPU allows).
  rasterizer last_used() const noexcept;
  /// @brief Why OpenGL was not used, last time it was not; empty otherwise.
  const std::string& fallback_reason() const noexcept;
  /// @brief GL vendor, renderer and version when active() is opengl.
  std::string device_name() const;

 private:
  struct impl;
  std::unique_ptr<impl> impl_;
};

/// @brief Whether OpenGL can be used here, and why not.
/// @return {true, device name} or {false, reason}; creates and destroys a
///         device.
std::pair<bool, std::string> probe_opengl();

/// Unchanged: renderer(rasterizer::software).render_png(source).
std::vector<uint8_t> render_png(std::string_view source);
}
```

Fallback rules, in one place (`renderer::impl`):

| What failed | This image | Later images |
|---|---|---|
| Device creation (library, symbol, display, context, software GL refused) | software | software (`active()` = software) |
| Image larger than `max_size()` | software | OpenGL |
| Shader, framebuffer or GL error during `rasterize()` | software | software (the device is dropped: a driver that failed once is not trusted again) |
| Bad draw data (`std::runtime_error`, not `raster_unavailable`) | error, propagated | — |

A `renderer` is used by one thread at a time, like the free functions
(ImGui's current context is process-global, core DESIGN section 4).
`check_source()` does not rasterize and is unchanged.

## 4. Public Contract: the command line

Additions to core DESIGN section 6 and `docs/cli.md`:

| Argument | Meaning |
|---|---|
| `--rasterizer software\|opengl` | On `render` and the implicit `nymph <in>` render. Default `opengl`. |
| `nymph rasterizers` | Prints one line per rasterizer: `software: available`, and `opengl: available (<vendor> <renderer> <version>)` or `opengl: unavailable (<reason>)`. Exit 0 either way. |

- An unknown value (`--rasterizer vulkan`) is a usage error: exit 2.
- `--rasterizer` on `check`, `extract`, `schema` or `fonts` is a usage
  error, as other misplaced options are today.
- **Default (no flag)**: OpenGL, falling back **silently**. The output is
  the chart either way, and an agent reading stderr should see nothing
  on success (core DESIGN goal 2).
- **`--rasterizer opengl` given explicitly**: the same, but a fallback
  prints one warning line on stderr and still exits 0:
  `nymph: warning: OpenGL unavailable (<reason>); used the software rasterizer`.
  Asking for OpenGL by name is the user saying they care.
- **`--rasterizer software`**: never touches OpenGL (no library is even
  opened), and gives exactly the bytes the library's `render_png()`
  gives.

`run_cli` keeps its signature. Internally the CLI distinguishes "no flag"
from "explicit `opengl`" only for the warning.

## 5. Build

- CMake option `NYMPH_OPENGL` (default `ON`). `OFF` compiles none of
  `src/render/gl/` or `gl_rasterizer`; `renderer(rasterizer::opengl)` and
  `probe_opengl()` report the reason "nymph was built without OpenGL
  support", and the CLI option is still accepted (scripts keep working
  across builds).
- `gl_device_posix.cpp` on every non-`WIN32` platform, `gl_device_win.cpp`
  on `WIN32` (MSVC and MSYS2).
- Link: `${CMAKE_DL_LIBS}` on POSIX; `user32` and `gdi32` on Windows. No
  OpenGL library, no `find_package(OpenGL)`, no GL headers.
- No new submodule.

## 6. Testing Strategy

GPU tests cannot assume a GPU. Three layers:

1. **Always run** (no GL needed): `shared_library` with a missing name
   reports the loader's error; `gl_api::load` with a lookup that lacks one
   function names it; `renderer(opengl)` with `library_names` pointing at
   a missing library falls back, renders the same bytes as software and
   reports the reason; the CLI warning appears only with an explicit
   flag; usage errors; `--rasterizer software` bytes equal `render_png()`;
   a build with `NYMPH_OPENGL=OFF` (manual, recorded in the plan).
2. **When OpenGL is available** (`allow_software_gl = true`, so Mesa
   llvmpipe through EGL counts; `GTEST_SKIP()` with the probe's reason
   otherwise): every scene of `test_soft_rasterizer` and every example
   rendered by both rasterizers and compared with a tolerance: at least
   99.5 % of pixels within 2 per channel, none beyond 64, and the mean
   absolute difference below 0.5 (the step that adds the test tunes these
   numbers against llvmpipe and one hardware GPU, and records them).
   Plus: two renders on one device give identical bytes; an over-size
   image falls back; the previous EGL / WGL context is current again after
   rendering.
3. **Linkage test** (CTest, a CMake script around `CMAKE_OBJDUMP -p`, or
   `dumpbin /dependents` on MSVC): the `nymph` executable's needed
   libraries contain none of `libGL`, `libEGL`, `libOpenGL`, `libGLX`,
   `libGLESv2`, `opengl32`.

A machine with `libegl1` and Mesa installed (`libegl-mesa0`) runs layer 2
on the CPU through llvmpipe; without them those tests skip, which the
test output says.

## 7. Design Decisions

- **Swap only the last call.** The frame driver already keeps CPU copies
  of every texture (`texture_store`) and produces final `ImDrawData`; a
  GPU backend that played ImGui's texture protocol itself would duplicate
  that and couple the GPU to the frame loop. Uploading the store at
  rasterize time keeps one frame path for both rasterizers, so layout,
  fonts and the three-frame rule cannot drift between them.
- **nymph's own loader, not glad, GLEW or ImGui's loader.** GLEW links
  `libGL`; ImGui's `imgui_impl_opengl3_loader.h` opens `libGL.so.1` and
  uses GLX's `glXGetProcAddress`, which is wrong for an EGL context and
  absent on headless machines; glad would be a generated file or a
  submodule for ~35 functions. A table of 35 pointers is shorter than any
  of them and needs no GL headers to build (goal 3).
- **EGL, headless, on Linux.** nymph's users are agents on servers and
  CI runners with no display server. EGL's device and surfaceless
  platforms create a context with no X or Wayland; GLX cannot.
- **OpenGL 3.3 core only.** It is the oldest version with everything the
  rasterizer needs (texture swizzle for Alpha8, `glDrawElementsBaseVertex`
  for `VtxOffset`, framebuffer objects, GLSL 330) and every GPU driver of
  the last decade has it. OpenGL ES 3.0 would cover ARM boards; it is an
  extension (section 9), not v1.
- **Refuse software OpenGL.** "Fall back when there is no GPU" is the
  request; Mesa's llvmpipe is OpenGL without a GPU, slower than nymph's
  rasterizer and a source of driver-dependent pixels. Refusing it makes
  the CLI default do what it says. Rejected: accepting any GL context
  (would quietly make headless machines slower).
- **The library default stays software; the CLI default is OpenGL.** The
  free `render_png()` promises same bytes for same source and build, and
  embedding programs may have their own GL context on the thread; neither
  should change under them. The CLI is one image per process on the
  machine it runs on, where the GPU is the expected choice. Rejected: an
  OpenGL library default (breaks a documented contract and can disturb a
  host application's GL state despite the save / restore).
- **Fallback is silent by default, loud when asked for.** Exit 0 and the
  right image either way (goal 2); a warning only for an explicit
  `--rasterizer opengl`, and `nymph rasterizers` to ask why. Rejected: an
  error when OpenGL was requested explicitly (the user asked for a
  fallback in every case).
- **Not in the source, not in an environment variable.** Which rasterizer
  draws is a property of the machine, not of the chart: putting it in
  `image` would make the same source render differently or fail per
  machine, and the round trip would carry a machine choice. An
  environment variable would break core DESIGN section 8 ("no
  environment variable" affects the frame) without a request for it.
- **Not recorded in the PNG.** The image carries its source byte for byte
  (core DESIGN goal 1); the rasterizer is not part of it. A later need can
  add a separate `iTXt` chunk without touching the source chunk.
- **Determinism, relaxed for OpenGL only.** GPUs differ in sub-pixel
  precision, filtering precision and blend rounding, so the OpenGL image
  is not byte-identical to the software one, nor across GPUs. It is
  repeatable on one device (no dithering, no multisampling, fixed state),
  which is what an agent re-rendering an edit needs. Committed example
  images and golden tests use `--rasterizer software`.
- **A device per `renderer`, not a process-wide one.** Creating an EGL /
  WGL context costs tens of milliseconds, so library users rendering many
  charts keep a `renderer`; but a hidden global context destroyed during
  static destruction, after the driver may have been unloaded, is a known
  crash source. The owner decides its lifetime.

## 8. Constraints and Invariants

- No translation unit calls an OpenGL, EGL or WGL function by its
  linker name; every call goes through `gl_api` or the device's own
  pointer table. The linkage test enforces the result.
- `--rasterizer software` and `renderer(rasterizer::software)` never open
  a graphics library.
- An OpenGL failure never changes the exit code or the source checks:
  every `nymph::error` is raised by `bind()` or the layout pass, before
  any rasterizer runs.
- The context current on the calling thread before a `rasterize()` is
  current again after it, also on exception.
- The software rasterizer's bytes for a given source and build are
  unchanged by this project (every existing test still passes
  unmodified).

## 9. Extension Points

- **OpenGL ES 3.0** (ARM boards, some VMs): `eglBindAPI(EGL_OPENGL_ES_API)`
  as a second attempt in `gl_device_posix.cpp`, a `#version 300 es`
  shader header, and `GL_R8` swizzle (core in ES 3.0). `gl_api` is
  unchanged.
- **GLX or macOS**: another `gl_device_<platform>.cpp` that fills the same
  `gl_api`; `gl_backend` does not change.
- **Another GPU API** (Vulkan, Direct3D 11): another `raster_backend` and
  another `rasterizer` enum value; the frame driver and the fallback table
  do not change.
- **Batching**: `renderer` already keeps the device; keeping uploaded
  textures between images (the embedded fonts' atlas) is an optimization
  inside `gl_backend` if measurements ever ask for it.
