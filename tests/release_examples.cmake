# MIT License © 2026 Binary Dice Games
#
# spec/06_claude_integration/PLAN.md step 7: the release executable, run as
# a process, draws every example with the software rasterizer. With EXACT
# (x86-64, where the committed PNGs were drawn) each must match its PNG byte
# for byte; on other architectures floating-point results differ slightly
# (arm64: one colour level, or a one-pixel layout shift), so the examples
# that differ are listed but do not fail the test.
#
#   cmake -DNYMPH=<executable> -DEXAMPLES=<examples dir> -DWORK_DIR=<dir>
#         [-DEXACT=ON] [-DRUNNER=<emulator, '|'-separated>] -P release_examples.cmake

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
if(different AND EXACT)
  message(FATAL_ERROR "differ from the committed PNG: ${different}")
elseif(different)
  list(LENGTH different n)
  message(STATUS "${count} examples drawn; ${n} not byte-identical to the committed PNG (another architecture): ${different}")
else()
  message(STATUS "${count} examples reproduced byte for byte")
endif()
