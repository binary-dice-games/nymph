# https://cliutils.gitlab.io/modern-cmake/

cmake_minimum_required(VERSION 3.14...3.28)

project(
  Nymph
  VERSION 0.1.0
  LANGUAGES C CXX)

option(NYMPH_BUILD_TESTS "Build the nymph tests" ${PROJECT_IS_TOP_LEVEL})

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

# ── third-party dependencies ──────────────────────────────────────────────────

include(cmake/NymphDependencies.cmake)

# ── nymph — core library ──────────────────────────────────────────────────────

add_subdirectory(src)

# ── nymph executable ──────────────────────────────────────────────────────────

if(PROJECT_IS_TOP_LEVEL)
  add_subdirectory(app)
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
