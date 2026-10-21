# MIT License © 2026 Binary Dice Games
#
# spec/06_claude_integration/PLAN.md step 8: the committed files of the
# Claude Code plugin match their sources (run the nymph-plugin-sync target
# after changing SKILL.md or docs/agent-guide.md).
#
#   cmake -DSOURCE_DIR=<repo> -DVERSION=<x.y.z> -P plugin_sync_check.cmake

set(plugin "${SOURCE_DIR}/claude/plugin")
set(stale "")
foreach(pair "skills/nymph/SKILL.md=claude/skill/nymph/SKILL.md" "skills/nymph/reference.md=docs/agent-guide.md")
  string(REPLACE "=" ";" pair "${pair}")
  list(GET pair 0 copy)
  list(GET pair 1 original)
  execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files "${plugin}/${copy}" "${SOURCE_DIR}/${original}"
                  RESULT_VARIABLE different)
  if(different)
    list(APPEND stale "claude/plugin/${copy}")
  endif()
endforeach()
# The version is the pinned release's (spec/07_release_pipeline/DESIGN.md
# section 4.5): 0.0.0 while checksums.txt pins none.
set(pinned "0.0.0")
file(STRINGS "${plugin}/bin/checksums.txt" entries REGEX "^[0-9a-f]+  nymph-[0-9]+\\.[0-9]+\\.[0-9]+-")
set(versions "")
foreach(entry IN LISTS entries)
  string(REGEX REPLACE "^[0-9a-f]+  nymph-([0-9]+\\.[0-9]+\\.[0-9]+)-.*$" "\\1" entry_version "${entry}")
  list(APPEND versions "${entry_version}")
endforeach()
list(REMOVE_DUPLICATES versions)
list(LENGTH versions version_count)
if(version_count GREATER 1)
  list(APPEND stale "claude/plugin/bin/checksums.txt (pins several versions: ${versions})")
elseif(version_count EQUAL 1)
  set(pinned "${versions}")
endif()
file(READ "${plugin}/.claude-plugin/plugin.json" manifest)
string(JSON plugin_version GET "${manifest}" version)
if(NOT plugin_version STREQUAL pinned)
  list(APPEND stale "claude/plugin/.claude-plugin/plugin.json (version ${plugin_version}, not the pinned ${pinned})")
endif()
# The Cursor plugin (spec/12_cursor_integration/PLAN.md step 3): the same
# skill, reference, launcher and pins, its own rule, and the pinned version.
set(cursor "${SOURCE_DIR}/cursor/plugin")
foreach(pair
    "skills/nymph/SKILL.md=claude/skill/nymph/SKILL.md"
    "skills/nymph/reference.md=docs/agent-guide.md"
    "rules/nymph.mdc=cursor/rule/nymph.mdc.in"
    "bin/nymph=claude/plugin/bin/nymph"
    "bin/checksums.txt=claude/plugin/bin/checksums.txt")
  string(REPLACE "=" ";" pair "${pair}")
  list(GET pair 0 copy)
  list(GET pair 1 original)
  execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files "${cursor}/${copy}" "${SOURCE_DIR}/${original}"
                  RESULT_VARIABLE different)
  if(different)
    list(APPEND stale "cursor/plugin/${copy}")
  endif()
endforeach()
file(READ "${cursor}/.cursor-plugin/plugin.json" manifest)
string(JSON cursor_version GET "${manifest}" version)
if(NOT cursor_version STREQUAL pinned)
  list(APPEND stale "cursor/plugin/.cursor-plugin/plugin.json (version ${cursor_version}, not the pinned ${pinned})")
endif()
if(stale)
  message(FATAL_ERROR "out of date: ${stale}; run: cmake --build <build> --target nymph-plugin-sync")
endif()
message(STATUS "claude/plugin and cursor/plugin are in sync")
