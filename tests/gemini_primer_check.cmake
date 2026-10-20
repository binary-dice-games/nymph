# MIT License © 2026 Binary Dice Games
#
# Checks the text that teaches Gemini nymph (spec/10_gemini_extension plan
# step 8): the committed extension/src/gemini/primer.txt is what
# cmake/NymphExtensionPrimer.cmake makes from its template and the example
# sources (line endings do not matter), it names no tool Gemini does not
# have, every example it embeds is the file in examples/ verbatim, and it is
# short enough for a Gem's instructions.
#
#   cmake -DSOURCE_DIR=<repo> -DWORK_DIR=<scratch dir> -P gemini_primer_check.cmake

foreach(var SOURCE_DIR WORK_DIR)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "gemini_primer_check: ${var} is not set")
  endif()
endforeach()
file(MAKE_DIRECTORY "${WORK_DIR}")

execute_process(COMMAND "${CMAKE_COMMAND}" "-DSOURCE_DIR=${SOURCE_DIR}" "-DOUTPUT=${WORK_DIR}/primer.txt"
                -P "${SOURCE_DIR}/cmake/NymphExtensionPrimer.cmake" RESULT_VARIABLE status)
if(NOT status EQUAL 0)
  message(FATAL_ERROR "the primer generator failed")
endif()

function(read_lf file out)
  file(READ "${file}" text)
  string(REPLACE "\r\n" "\n" text "${text}")
  set(${out} "${text}" PARENT_SCOPE)
endfunction()

read_lf("${WORK_DIR}/primer.txt" fresh)
read_lf("${SOURCE_DIR}/extension/src/gemini/primer.txt" committed)
if(NOT fresh STREQUAL committed)
  message(FATAL_ERROR "extension/src/gemini/primer.txt is stale: run cmake -DSOURCE_DIR=. -P cmake/NymphExtensionPrimer.cmake")
endif()

# Nothing about tools or files Gemini does not have.
foreach(word render_chart check_chart chart_schema extract_chart list_fonts output_path ".png" "MCP")
  string(FIND "${committed}" "${word}" at)
  if(NOT at EQUAL -1)
    message(FATAL_ERROR "the primer mentions '${word}', which Gemini has no use for")
  endif()
endforeach()
string(FIND "${committed}" "{{" at)
if(NOT at EQUAL -1)
  message(FATAL_ERROR "the primer has an unreplaced {{...}} marker")
endif()

# Every embedded example is its example file, verbatim.
read_lf("${SOURCE_DIR}/extension/gemini/primer.md.in" template)
string(REGEX MATCHALL "{{example:[a-z0-9_]+}}" marks "${template}")
list(LENGTH marks count)
if(count LESS 3)
  message(FATAL_ERROR "the template embeds only ${count} examples")
endif()
foreach(mark IN LISTS marks)
  string(REGEX REPLACE "^{{example:(.*)}}$" "\\1" name "${mark}")
  read_lf("${SOURCE_DIR}/examples/${name}.yaml" body)
  string(REGEX REPLACE "\n+$" "" body "${body}")
  string(FIND "${committed}" "${body}" at)
  if(at EQUAL -1)
    message(FATAL_ERROR "the primer does not hold examples/${name}.yaml verbatim")
  endif()
endforeach()

# Every example source of the repository is named in the primer, and the web
# address it gives them is the project's raw examples folder.
file(GLOB example_files RELATIVE "${SOURCE_DIR}/examples" "${SOURCE_DIR}/examples/*.yaml")
list(LENGTH example_files example_count)
if(example_count LESS 20)
  message(FATAL_ERROR "found only ${example_count} example sources")
endif()
foreach(file IN LISTS example_files)
  string(REGEX REPLACE "\\.yaml$" "" name "${file}")
  string(FIND "${committed}" "${name}" at)
  if(at EQUAL -1)
    message(FATAL_ERROR "the primer does not name the example '${name}'")
  endif()
endforeach()
string(FIND "${committed}" "https://raw.githubusercontent.com/binary-dice-games/nymph/refs/heads/main/examples/" at)
if(at EQUAL -1)
  message(FATAL_ERROR "the primer does not give the address of the examples")
endif()

# Size: a Gem's instruction limit is not published (PLAN step 1 item 5). A
# 6,700 character primer was accepted; 10,000 is the most this check allows
# until a larger one is tried.
string(LENGTH "${committed}" length)
if(length GREATER 10000)
  message(FATAL_ERROR "the primer is ${length} characters, over 10000")
endif()
message(STATUS "gemini primer ok (${length} characters, ${count} examples)")
