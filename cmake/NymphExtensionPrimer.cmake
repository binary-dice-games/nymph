# MIT License © 2026 Binary Dice Games
#
# Script mode: writes the text that teaches Gemini nymph's format
# (spec/10_gemini_extension/DESIGN.md section 4.5): the template
# extension/gemini/primer.md.in with each {{example:NAME}} replaced by the
# text of examples/NAME.yaml, so the examples in the primer are the ones the
# test suite draws and cannot drift or be invalid. The output,
# extension/src/gemini/primer.txt, is committed (the extension ships it).
#
#   cmake -DSOURCE_DIR=<repo> [-DOUTPUT=<file>] -P NymphExtensionPrimer.cmake

if(NOT DEFINED SOURCE_DIR)
  message(FATAL_ERROR "NymphExtensionPrimer.cmake: SOURCE_DIR is not set")
endif()
if(NOT DEFINED OUTPUT)
  set(OUTPUT "${SOURCE_DIR}/extension/src/gemini/primer.txt")
endif()

file(READ "${SOURCE_DIR}/extension/gemini/primer.md.in" text)
string(REPLACE "\r\n" "\n" text "${text}") # the file is LF whatever the checkout's line endings
# {{example-names}}: the name of every example source, for the web examples.
file(GLOB example_files RELATIVE "${SOURCE_DIR}/examples" "${SOURCE_DIR}/examples/*.yaml")
list(SORT example_files)
set(names "")
foreach(file IN LISTS example_files)
  string(REGEX REPLACE "\\.yaml$" "" name "${file}")
  list(APPEND names "${name}")
endforeach()
list(JOIN names ", " names_text)
string(REPLACE "{{example-names}}" "${names_text}" text "${text}")
string(REGEX MATCHALL "{{example:[a-z0-9_]+}}" marks "${text}")
foreach(mark IN LISTS marks)
  string(REGEX REPLACE "^{{example:(.*)}}$" "\\1" name "${mark}")
  set(example "${SOURCE_DIR}/examples/${name}.yaml")
  if(NOT EXISTS "${example}")
    message(FATAL_ERROR "NymphExtensionPrimer.cmake: ${mark} names no example (${example})")
  endif()
  file(READ "${example}" body)
  string(REPLACE "\r\n" "\n" body "${body}")
  string(REGEX REPLACE "\n+$" "" body "${body}")
  string(REPLACE "${mark}" "${body}" text "${text}")
endforeach()
file(WRITE "${OUTPUT}" "${text}")
