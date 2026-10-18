# MIT License © 2026 Binary Dice Games
#
# spec/06_claude_integration/PLAN.md step 7: the release executable, run as
# a process, draws every example byte for byte like its committed PNG with
# the software rasterizer.
#
#   cmake -DNYMPH=<executable> -DEXAMPLES=<examples dir> -DWORK_DIR=<dir>
#         [-DRUNNER=<emulator, '|'-separated>] -P release_examples.cmake

foreach(var NYMPH EXAMPLES WORK_DIR)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "release_examples.cmake: ${var} is not set")
  endif()
endforeach()
string(REPLACE "|" ";" RUNNER "${RUNNER}")
file(MAKE_DIRECTORY "${WORK_DIR}")
file(GLOB sources "${EXAMPLES}/*.yaml")
list(SORT sources)
set(different "")
set(count 0)
foreach(source IN LISTS sources)
  get_filename_component(name "${source}" NAME_WE)
  set(out "${WORK_DIR}/${name}.png")
  file(REMOVE "${out}")
  execute_process(
    COMMAND ${RUNNER} "${NYMPH}" render "${source}" -o "${out}" --rasterizer software
    RESULT_VARIABLE code ERROR_VARIABLE errors)
  if(NOT code EQUAL 0)
    message(FATAL_ERROR "${name}: nymph exited with '${code}': ${errors}")
  endif()
  execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files "${out}" "${EXAMPLES}/${name}.png" RESULT_VARIABLE same)
  if(NOT same EQUAL 0)
    list(APPEND different "${name}")
  endif()
  math(EXPR count "${count} + 1")
endforeach()
if(count EQUAL 0)
  message(FATAL_ERROR "no examples in ${EXAMPLES}")
endif()
if(different)
  message(FATAL_ERROR "differ from the committed PNG: ${different}")
endif()
message(STATUS "${count} examples reproduced byte for byte")
