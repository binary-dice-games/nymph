# MIT License © 2026 Binary Dice Games
#
# spec/06_claude_integration/PLAN.md step 8: the committed files of the
# Claude Code plugin match their sources (run the nymph-plugin-sync target
# after changing SKILL.md, docs/agent-guide.md or the version).
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
file(READ "${plugin}/.claude-plugin/plugin.json" manifest)
string(JSON plugin_version GET "${manifest}" version)
if(NOT plugin_version STREQUAL VERSION)
  list(APPEND stale "claude/plugin/.claude-plugin/plugin.json (version ${plugin_version}, not ${VERSION})")
endif()
if(stale)
  message(FATAL_ERROR "out of date: ${stale}; run: cmake --build <build> --target nymph-plugin-sync")
endif()
message(STATUS "claude/plugin is in sync")
