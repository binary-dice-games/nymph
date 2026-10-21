# MIT License © 2026 Binary Dice Games
#
# spec/07_release_pipeline/PLAN.md step 6: in a copy of the plugin's
# sources, nymph-plugin-sync pins the executables of a release SHA256SUMS
# (the five platforms, nothing else) and sets the plugin version to that
# release; a sync without sums keeps both; the staleness check passes, and
# fails once the version and the pin disagree.
#
#   cmake -DSOURCE_DIR=<repo> -DWORK_DIR=<dir> -P plugin_sync_versions.cmake

foreach(var SOURCE_DIR WORK_DIR)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "plugin_sync_versions.cmake: ${var} is not set")
  endif()
endforeach()
file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}/docs")
file(COPY "${SOURCE_DIR}/claude" "${SOURCE_DIR}/cursor" DESTINATION "${WORK_DIR}")
file(COPY "${SOURCE_DIR}/docs/agent-guide.md" DESTINATION "${WORK_DIR}/docs")

function(sync)
  execute_process(COMMAND "${CMAKE_COMMAND}" "-DSOURCE_DIR=${WORK_DIR}" ${ARGN}
                          -P "${SOURCE_DIR}/cmake/NymphPluginSync.cmake"
                  RESULT_VARIABLE code ERROR_VARIABLE errors)
  if(NOT code EQUAL 0)
    message(FATAL_ERROR "sync ${ARGN} failed: ${errors}")
  endif()
endfunction()
function(check expect_ok what)
  execute_process(COMMAND "${CMAKE_COMMAND}" "-DSOURCE_DIR=${WORK_DIR}" -DVERSION=ignored
                          -P "${SOURCE_DIR}/tests/plugin_sync_check.cmake"
                  RESULT_VARIABLE code OUTPUT_QUIET ERROR_QUIET)
  if(expect_ok AND NOT code EQUAL 0)
    message(FATAL_ERROR "the staleness check fails ${what}")
  elseif(NOT expect_ok AND code EQUAL 0)
    message(FATAL_ERROR "the staleness check passes ${what}")
  endif()
endfunction()
function(plugin_version out)
  file(READ "${WORK_DIR}/claude/plugin/.claude-plugin/plugin.json" manifest)
  string(JSON v GET "${manifest}" version)
  set(${out} "${v}" PARENT_SCOPE)
endfunction()

# A release SHA256SUMS: executables, and files the plugin must not pin.
set(h "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef")
file(WRITE "${WORK_DIR}/SHA256SUMS" "${h}  install.sh
${h}  nymph-0.2.0-linux-arm64
${h}  nymph-0.2.0-linux-arm64.mcpb
${h}  nymph-0.2.0-linux-x86_64
${h}  nymph-0.2.0-macos-arm64
${h}  nymph-0.2.0-macos-universal.mcpb
${h}  nymph-0.2.0-macos-x86_64
${h}  nymph-0.2.0-src.tar.gz
${h}  nymph-0.2.0-windows-x86_64.exe
${h}  nymph-skill-0.2.0.zip
")

sync(-DVERSION=0.2.0 "-DSUMS=${WORK_DIR}/SHA256SUMS")
file(STRINGS "${WORK_DIR}/claude/plugin/bin/checksums.txt" pinned REGEX "^[0-9a-f]+  ")
list(LENGTH pinned count)
if(NOT count EQUAL 5)
  message(FATAL_ERROR "pinned ${count} files, not the 5 executables: ${pinned}")
endif()
plugin_version(v)
if(NOT v STREQUAL "0.2.0")
  message(FATAL_ERROR "plugin version ${v} after pinning 0.2.0")
endif()
check(TRUE "after pinning 0.2.0")

sync(-DVERSION=9.9.9)
plugin_version(v)
file(STRINGS "${WORK_DIR}/claude/plugin/bin/checksums.txt" again REGEX "^[0-9a-f]+  ")
if(NOT v STREQUAL "0.2.0" OR NOT again STREQUAL pinned)
  message(FATAL_ERROR "a sync without sums changed the pin or the version (${v})")
endif()

file(READ "${WORK_DIR}/claude/plugin/.claude-plugin/plugin.json" manifest)
string(REPLACE "\"0.2.0\"" "\"0.3.0\"" manifest "${manifest}")
file(WRITE "${WORK_DIR}/claude/plugin/.claude-plugin/plugin.json" "${manifest}")
check(FALSE "with version 0.3.0 and a 0.2.0 pin")
# The Cursor plugin follows the same pin.
file(READ "${WORK_DIR}/cursor/plugin/.cursor-plugin/plugin.json" manifest)
string(JSON cursor_version GET "${manifest}" version)
if(NOT cursor_version STREQUAL "0.2.0")
  message(FATAL_ERROR "cursor plugin version ${cursor_version} pinning 0.2.0")
endif()
message(STATUS "plugin sync: version follows the pin")
