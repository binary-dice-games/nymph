# MIT License © 2026 Binary Dice Games
#
# spec/06_claude_integration/PLAN.md step 10: the claude.ai Skill zip holds
# nymph/SKILL.md, nymph/reference.md and an executable nymph/bin/nymph
# (files only), its SKILL.md is the project's, and the executable draws the
# example of SKILL.md.
#
#   cmake -DSKILL_ZIP=<file.zip> -DSOURCE_DIR=<repo> -DWORK_DIR=<dir> -P skill_check.cmake

foreach(var SKILL_ZIP SOURCE_DIR WORK_DIR)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "skill_check.cmake: ${var} is not set")
  endif()
endforeach()
if(NOT EXISTS "${SKILL_ZIP}")
  message(FATAL_ERROR "${SKILL_ZIP} does not exist (build the nymph-dist target)")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar tf "${SKILL_ZIP}" OUTPUT_VARIABLE listing)
if(NOT listing STREQUAL "nymph/SKILL.md\nnymph/reference.md\nnymph/bin/nymph\n")
  message(FATAL_ERROR "unexpected entries in ${SKILL_ZIP}:\n${listing}")
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf "${SKILL_ZIP}" WORKING_DIRECTORY "${WORK_DIR}")
foreach(pair "SKILL.md=claude/skill/nymph/SKILL.md" "reference.md=docs/agent-guide.md")
  string(REPLACE "=" ";" pair "${pair}")
  list(GET pair 0 copy)
  list(GET pair 1 original)
  execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files "${WORK_DIR}/nymph/${copy}" "${SOURCE_DIR}/${original}"
                  RESULT_VARIABLE different)
  if(different)
    message(FATAL_ERROR "nymph/${copy} differs from ${original}")
  endif()
endforeach()

# The executable runs as unpacked (its mode kept) and draws SKILL.md's example.
set(exe "${WORK_DIR}/nymph/bin/nymph")
file(READ "${WORK_DIR}/nymph/SKILL.md" skill)
string(FIND "${skill}" "```yaml\n" start)
string(FIND "${skill}" "\n```\n" stop)
if(start EQUAL -1 OR stop LESS start)
  message(FATAL_ERROR "no yaml example in SKILL.md")
endif()
math(EXPR start "${start} + 8")
math(EXPR length "${stop} - ${start} + 1")
string(SUBSTRING "${skill}" ${start} ${length} example)
file(WRITE "${WORK_DIR}/example.yaml" "${example}")
execute_process(COMMAND "${exe}" render "${WORK_DIR}/example.yaml" -o "${WORK_DIR}/example.png" --rasterizer software
                RESULT_VARIABLE code ERROR_VARIABLE errors)
if(NOT code EQUAL 0 OR NOT EXISTS "${WORK_DIR}/example.png")
  message(FATAL_ERROR "nymph/bin/nymph did not draw SKILL.md's example ('${code}'): ${errors}")
endif()
message(STATUS "${SKILL_ZIP}: entries, copies and executable OK")
