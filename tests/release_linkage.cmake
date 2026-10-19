# MIT License © 2026 Binary Dice Games
#
# The release executable needs no shared library beyond the operating
# system's (spec/06_claude_integration/PLAN.md step 7,
# spec/07_release_pipeline/DESIGN.md section 4.8):
#
#   Linux   fully static: no program interpreter, no dynamic section (readelf)
#   macOS   only /usr/lib/libSystem.B.dylib and /usr/lib/libc++.1.dylib (otool -L)
#
#   cmake -DNYMPH=<executable> (-DREADELF=<readelf> | -DOTOOL=<otool>) -P release_linkage.cmake

cmake_policy(SET CMP0007 NEW)
if(NOT DEFINED NYMPH)
  message(FATAL_ERROR "release_linkage.cmake: NYMPH is not set")
endif()

if(READELF)
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
elseif(OTOOL)
  execute_process(COMMAND "${OTOOL}" -L "${NYMPH}" OUTPUT_VARIABLE listing RESULT_VARIABLE code)
  if(NOT code EQUAL 0)
    message(FATAL_ERROR "otool failed on ${NYMPH}")
  endif()
  # The first line names the file itself; each further line is one library.
  string(REPLACE "\n" ";" lines "${listing}")
  list(REMOVE_AT lines 0)
  set(foreign "")
  foreach(line IN LISTS lines)
    string(STRIP "${line}" line)
    if(line STREQUAL "")
      continue()
    endif()
    string(REGEX REPLACE " \\(.*$" "" library "${line}")
    if(NOT library MATCHES "^/usr/lib/(libSystem\\.B|libc\\+\\+\\.1)\\.dylib$")
      list(APPEND foreign "${library}")
    endif()
  endforeach()
  if(foreign)
    message(FATAL_ERROR "${NYMPH} needs libraries outside macOS: ${foreign}\n${listing}")
  endif()
  message(STATUS "${NYMPH}: only libSystem and libc++")
else()
  message(FATAL_ERROR "release_linkage.cmake: READELF or OTOOL must be set")
endif()
