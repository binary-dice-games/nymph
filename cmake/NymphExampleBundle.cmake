# MIT License © 2026 Binary Dice Games
#
# Script mode: joins the example sources into one text file, which
# NymphEmbedFile.cmake then compiles into `nymph` (`nymph example`, the
# MCP tool `example_chart`).
#
#   cmake -DEXAMPLES=<examples dir> -DOUTPUT=<bundle.txt> -P NymphExampleBundle.cmake
#
# Each example is a line "#@@ <name>" (the file name without .yaml)
# followed by the file; examples go in file name order.

foreach(var EXAMPLES OUTPUT)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "NymphExampleBundle.cmake: ${var} is not set")
  endif()
endforeach()

file(GLOB sources "${EXAMPLES}/*.yaml")
list(SORT sources)
set(bundle "")
foreach(source IN LISTS sources)
  get_filename_component(name "${source}" NAME_WE)
  file(READ "${source}" text)
  string(REPLACE "\r\n" "\n" text "${text}")
  if(NOT text MATCHES "\n$")
    string(APPEND text "\n")
  endif()
  string(APPEND bundle "#@@ ${name}\n${text}")
endforeach()
file(WRITE "${OUTPUT}" "${bundle}")
