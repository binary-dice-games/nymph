# MIT License © 2026 Binary Dice Games
#
# NYMPH_RELEASE: the executables published for the Claude integration
# (spec/06_claude_integration/DESIGN.md section 6), one file with no
# shared-library dependency beyond the operating system's:
#
#   Linux        -static, NYMPH_OPENGL off (a static glibc cannot dlopen
#                libEGL reliably; the software rasterizer is fast enough)
#   MinGW        -static (libgcc, libstdc++, winpthread inside)
#   MSVC         the static runtime (/MT)
#   macOS        dynamic, but only on libSystem and libc++ (part of macOS;
#                Apple supports no static executables), NYMPH_OPENGL off,
#                stripped and signed ad hoc after the link (app/CMakeLists.txt;
#                spec/07_release_pipeline/DESIGN.md section 4.8)
#
# all built MinSizeRel and stripped. The `nymph-dist` target copies the
# executable to <build>/dist/nymph-<version>-<os>-<arch>[.exe] and writes
# <build>/dist/SHA256SUMS. Included from the root CMakeLists.txt before
# the targets are defined; the MSVC runtime is chosen there, before
# project(), as CMP0091 requires.

if(NOT NYMPH_RELEASE)
  return()
endif()
if(NYMPH_WEB)
  message(FATAL_ERROR "nymph: NYMPH_RELEASE is for the native executable, not the web build")
endif()

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE MinSizeRel CACHE STRING "Build type" FORCE)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(NYMPH_OPENGL OFF CACHE BOOL "" FORCE)
  add_link_options(-static)
elseif(MINGW)
  add_link_options(-static)
endif()
if(APPLE)
  add_link_options(-Wl,-dead_strip)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_link_options(-s)
endif()

if(NOT NYMPH_MCP_VIEWER)
  message(WARNING "nymph: a release without NYMPH_MCP_VIEWER serves PNGs only (no live chart in Claude)")
endif()

# The published name: nymph-<version>-<os>-<arch>.
string(TOLOWER "${CMAKE_SYSTEM_NAME}" _nymph_os)
if(_nymph_os STREQUAL "darwin")
  set(_nymph_os "macos")
endif()
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _nymph_arch)
if(_nymph_arch MATCHES "^(x86_64|amd64|x64)$")
  set(_nymph_arch "x86_64")
elseif(_nymph_arch MATCHES "^(aarch64|arm64)$")
  set(_nymph_arch "arm64")
endif()
set(NYMPH_DIST_NAME "nymph-${PROJECT_VERSION}-${_nymph_os}-${_nymph_arch}" CACHE STRING
  "Base name of the published executable and packages")
set(NYMPH_DIST_DIR "${PROJECT_BINARY_DIR}/dist")

# nymph_add_dist_target(): called once the nymph-cli target exists.
function(nymph_add_dist_target)
  add_custom_target(nymph-dist ALL
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${NYMPH_DIST_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:nymph-cli>"
            "${NYMPH_DIST_DIR}/${NYMPH_DIST_NAME}$<TARGET_FILE_SUFFIX:nymph-cli>"
    COMMAND "${CMAKE_COMMAND}" "-DDIR=${NYMPH_DIST_DIR}" -P "${PROJECT_SOURCE_DIR}/cmake/NymphChecksums.cmake"
    DEPENDS nymph-cli
    COMMENT "Writing ${NYMPH_DIST_DIR}"
    VERBATIM)

  # The Claude Desktop extension (cmake/NymphMcpb.cmake), into dist/ before
  # its SHA256SUMS is written.
  if(NYMPH_MCP)
    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
      set(platform win32)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
      set(platform darwin)
    else()
      set(platform linux)
    endif()
    set(NYMPH_MCPB_FILE "${NYMPH_DIST_DIR}/${NYMPH_DIST_NAME}.mcpb" PARENT_SCOPE)
    add_custom_target(nymph-mcpb
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${NYMPH_DIST_DIR}"
      COMMAND "${CMAKE_COMMAND}"
              "-DTEMPLATE=${PROJECT_SOURCE_DIR}/claude/mcpb/manifest.json.in"
              "-DEXE=$<TARGET_FILE:nymph-cli>"
              "-DICON=${PROJECT_SOURCE_DIR}/claude/mcpb/icon.png"
              "-DVERSION=${PROJECT_VERSION}" "-DPLATFORM=${platform}"
              "-DSTAGE=${PROJECT_BINARY_DIR}/mcpb"
              "-DOUTPUT=${NYMPH_DIST_DIR}/${NYMPH_DIST_NAME}.mcpb"
              -P "${PROJECT_SOURCE_DIR}/cmake/NymphMcpb.cmake"
      DEPENDS nymph-cli
      COMMENT "Writing ${NYMPH_DIST_NAME}.mcpb"
      VERBATIM)
    add_dependencies(nymph-dist nymph-mcpb)
  endif()

  # The claude.ai Skill (cmake/NymphSkill.cmake): the code execution
  # sandbox is Linux x86-64, so only that build makes it.
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND _nymph_arch STREQUAL "x86_64")
    set(NYMPH_SKILL_FILE "${NYMPH_DIST_DIR}/nymph-skill-${PROJECT_VERSION}.zip" PARENT_SCOPE)
    add_custom_target(nymph-skill
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${NYMPH_DIST_DIR}"
      COMMAND "${CMAKE_COMMAND}"
              "-DSKILL=${PROJECT_SOURCE_DIR}/claude/skill/nymph/SKILL.md"
              "-DGUIDE=${PROJECT_SOURCE_DIR}/docs/agent-guide.md"
              "-DEXE=$<TARGET_FILE:nymph-cli>"
              "-DSTAGE=${PROJECT_BINARY_DIR}/skill"
              "-DOUTPUT=${NYMPH_DIST_DIR}/nymph-skill-${PROJECT_VERSION}.zip"
              -P "${PROJECT_SOURCE_DIR}/cmake/NymphSkill.cmake"
      DEPENDS nymph-cli
      COMMENT "Writing nymph-skill-${PROJECT_VERSION}.zip"
      VERBATIM)
    add_dependencies(nymph-dist nymph-skill)
  endif()
endfunction()
