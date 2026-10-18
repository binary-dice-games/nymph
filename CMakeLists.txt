# https://cliutils.gitlab.io/modern-cmake/

cmake_minimum_required(VERSION 3.14...3.28)

# The published executables (cmake/NymphRelease.cmake). The MSVC runtime
# must be chosen before project().
option(NYMPH_RELEASE "Build the static, stripped executables published for the Claude integration" OFF)
if(NYMPH_RELEASE)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()

project(
  Nymph
  VERSION 0.1.0
  LANGUAGES C CXX)

option(NYMPH_BUILD_TESTS "Build the nymph tests" ${PROJECT_IS_TOP_LEVEL})
# The OpenGL rasterizer loads OpenGL at run time; nothing is linked and no
# GL headers are needed (spec/02_opengl_rasterizer/DESIGN.md section 5).
option(NYMPH_OPENGL "Build the optional OpenGL rasterizer" ON)
# The interactive web viewer: the library compiled to WebAssembly with a
# JavaScript API (spec/04_web_viewer/DESIGN.md section 6). Emscripten only.
option(NYMPH_WEB "Build the WebAssembly web viewer (with emcmake)" OFF)
# `nymph mcp`: an MCP server over stdio in the nymph executable
# (spec/06_claude_integration/DESIGN.md section 4.1). Native only.
option(NYMPH_MCP "Build the MCP server (nymph mcp)" ON)
# `nymph view`: the viewer in a native window, a Win32 window over the
# system's OpenGL, loaded at run time (spec/04_web_viewer/DESIGN.md
# section 10). Windows only for now.
option(NYMPH_VIEW "Build the native viewer window (nymph view, Windows only)" ${WIN32})
if(NYMPH_VIEW AND NOT WIN32)
  message(FATAL_ERROR "NYMPH_VIEW (nymph view) is Windows only: configure with -DNYMPH_VIEW=OFF")
endif()
if(NYMPH_RELEASE)
  set(NYMPH_VIEW OFF CACHE BOOL "" FORCE)
endif()
if(NYMPH_WEB)
  set(NYMPH_VIEW OFF CACHE BOOL "" FORCE)
endif()
if(NYMPH_WEB)
  if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "NYMPH_WEB needs the Emscripten toolchain: configure with emcmake cmake ...")
  endif()
  # The EGL / WGL loader does not apply (WebGL comes from the browser), and
  # the GoogleTest suite is native: the web build has its own Node and
  # browser tests (web/tests).
  set(NYMPH_OPENGL OFF CACHE BOOL "" FORCE)
  set(NYMPH_MCP OFF CACHE BOOL "" FORCE)
  set(NYMPH_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  # Fonts are fetched on demand, next to the module (fonts/).
  if(NOT DEFINED CACHE{NYMPH_EMBED_FONTS})
    set(NYMPH_EMBED_FONTS "none" CACHE STRING
      "Fonts of NYMPH_FONTS_DIR whose bytes are compiled in: all, none, or a ;-list of file names")
  endif()
  # Native wasm exception handling (every current browser and Node).
  add_compile_options(-fwasm-exceptions)
  add_link_options(-fwasm-exceptions)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# extern/* dependencies are git submodules (see .gitmodules).
# nymph_ensure_submodules() clones only the paths a given option actually
# needs, so a plain checkout without `--recursive` still configures. A no-op
# (and network-free) once a submodule is already checked out.
function(nymph_ensure_submodules)
  # CMAKE_CURRENT_SOURCE_DIR, not CMAKE_SOURCE_DIR: the latter names the
  # outer project's root when nymph is consumed via add_subdirectory().
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
    execute_process(
      COMMAND git submodule update --init -- ${ARGN}
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      RESULT_VARIABLE _nymph_submodule_result
    )
    if(NOT _nymph_submodule_result EQUAL 0)
      message(FATAL_ERROR "Failed to init git submodule(s): ${ARGN}")
    endif()
  endif()
endfunction()

include(cmake/NymphRelease.cmake)

# ── third-party dependencies ──────────────────────────────────────────────────

include(cmake/NymphDependencies.cmake)

# ── nymph — core library ──────────────────────────────────────────────────────

add_subdirectory(imuml)
add_subdirectory(src)

# ── nymph executable ──────────────────────────────────────────────────────────

if(PROJECT_IS_TOP_LEVEL AND NOT NYMPH_WEB)
  add_subdirectory(app)
  # The Claude Code plugin's committed files (cmake/NymphPluginSync.cmake);
  # NYMPH_PLUGIN_SUMS names the release SHA256SUMS files to pin, '|'-separated.
  set(NYMPH_PLUGIN_SUMS "" CACHE STRING "SHA256SUMS files of a published release, '|'-separated")
  add_custom_target(nymph-plugin-sync
    COMMAND "${CMAKE_COMMAND}" "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}" "-DVERSION=${PROJECT_VERSION}"
            "-DSUMS=${NYMPH_PLUGIN_SUMS}" -P "${PROJECT_SOURCE_DIR}/cmake/NymphPluginSync.cmake"
    COMMENT "Updating claude/plugin"
    VERBATIM)
  if(NYMPH_RELEASE)
    nymph_add_dist_target()
  endif()
endif()

# ── web viewer ────────────────────────────────────────────────────────────────

if(NYMPH_WEB)
  enable_testing()
  add_subdirectory(web)
  add_subdirectory(extension)
endif()

# ── tests ─────────────────────────────────────────────────────────────────────

if(NYMPH_BUILD_TESTS)
  nymph_ensure_submodules(extern/googletest)
  enable_testing()
  # Enumerate tests at ctest time instead of emitting per-target
  # "<name>[1]_tests.cmake" files, which Ninja mistakes for globs on
  # CMake >= 3.29 (honoured since CMake 3.22).
  set(CMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE PRE_TEST)
  include(GoogleTest)
  add_subdirectory(tests)
endif()
