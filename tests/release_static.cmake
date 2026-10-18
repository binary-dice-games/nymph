# MIT License © 2026 Binary Dice Games
#
# spec/06_claude_integration/PLAN.md step 7: a Linux release executable is
# fully static, with no program interpreter and no dynamic section.
#
#   cmake -DREADELF=<readelf> -DNYMPH=<executable> -P release_static.cmake

foreach(var READELF NYMPH)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "release_static.cmake: ${var} is not set")
  endif()
endforeach()
execute_process(COMMAND "${READELF}" -lW "${NYMPH}" OUTPUT_VARIABLE headers RESULT_VARIABLE code)
if(NOT code EQUAL 0)
  message(FATAL_ERROR "readelf failed on ${NYMPH}")
endif()
foreach(bad INTERP DYNAMIC)
  string(REGEX MATCH "\n +${bad} " found "${headers}")
  if(found)
    message(FATAL_ERROR "${NYMPH} has a ${bad} program header: it is not static\n${headers}")
  endif()
endforeach()
message(STATUS "${NYMPH}: static")
