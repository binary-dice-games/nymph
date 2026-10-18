# MIT License © 2026 Binary Dice Games
#
# Script mode: writes the nymph MCP App, one self-contained HTML page
# (spec/06_claude_integration/DESIGN.md section 4.2).
#
#   cmake -DTEMPLATE=<viewer.html.in> -DVIEWER=<nymph-viewer.single.js>
#         -DAPP=<app.js> -DOUTPUT=<viewer.html> -P NymphClaudeApp.cmake
#
# The single-file viewer holds raw NUL characters (its wasm as a string),
# which CMake strings cannot carry, so the page is the template's three
# static pieces and the two scripts joined byte for byte by `cmake -E cat`;
# configure_file() would also rewrite the @ and ${} of the JavaScript.
# Emscripten keeps the module text safe inside an HTML <script> (NUL comes
# back from U+FFFD, CR is escaped); this script refuses text that would end
# or disturb the script element.

foreach(var TEMPLATE VIEWER APP OUTPUT)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "NymphClaudeApp.cmake: ${var} is not set")
  endif()
endforeach()

file(READ "${TEMPLATE}" page)
# Only for the checks: read as hex, so NUL bytes do not cut the text short.
file(READ "${VIEWER}" viewer_hex HEX)
file(READ "${APP}" app)

string(TOLOWER "${app}" app_lowered)
foreach(bad "</script" "<script" "<!--")
  string(FIND "${app_lowered}" "${bad}" at)
  if(NOT at EQUAL -1)
    message(FATAL_ERROR "NymphClaudeApp.cmake: app.js contains '${bad}', which cannot be inlined in HTML")
  endif()
endforeach()
# In the viewer (minified JavaScript), the lower- and upper-case spellings,
# searched in its hex text at whole-byte offsets.
foreach(bad "</script" "</SCRIPT" "<script" "<SCRIPT" "<!--")
  string(HEX "${bad}" bad_hex)
  set(rest "${viewer_hex}")
  string(FIND "${rest}" "${bad_hex}" at)
  while(NOT at EQUAL -1)
    math(EXPR odd "${at} % 2")
    if(NOT odd)
      message(FATAL_ERROR "NymphClaudeApp.cmake: the viewer contains '${bad}', which cannot be inlined in HTML")
    endif()
    math(EXPR next "${at} + 1")
    string(SUBSTRING "${rest}" ${next} -1 rest)
    string(FIND "${rest}" "${bad_hex}" at)
  endwhile()
endforeach()

# The template around the two placeholders, in order.
string(FIND "${page}" "@NYMPH_VIEWER_JS@" viewer_at)
string(FIND "${page}" "@NYMPH_APP_JS@" app_at)
if(viewer_at EQUAL -1 OR app_at EQUAL -1 OR app_at LESS viewer_at)
  message(FATAL_ERROR "NymphClaudeApp.cmake: ${TEMPLATE} needs @NYMPH_VIEWER_JS@, then @NYMPH_APP_JS@")
endif()
string(LENGTH "@NYMPH_VIEWER_JS@" viewer_length)
string(LENGTH "@NYMPH_APP_JS@" app_length)
string(SUBSTRING "${page}" 0 ${viewer_at} head)
math(EXPR middle_start "${viewer_at} + ${viewer_length}")
math(EXPR middle_length "${app_at} - ${middle_start}")
string(SUBSTRING "${page}" ${middle_start} ${middle_length} middle)
math(EXPR tail_start "${app_at} + ${app_length}")
string(SUBSTRING "${page}" ${tail_start} -1 tail)

file(WRITE "${OUTPUT}.head" "${head}")
file(WRITE "${OUTPUT}.middle" "${middle}")
file(WRITE "${OUTPUT}.tail" "${tail}")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E cat "${OUTPUT}.head" "${VIEWER}" "${OUTPUT}.middle" "${APP}" "${OUTPUT}.tail"
  OUTPUT_FILE "${OUTPUT}.tmp"
  RESULT_VARIABLE code)
file(REMOVE "${OUTPUT}.head" "${OUTPUT}.middle" "${OUTPUT}.tail")
if(NOT code EQUAL 0)
  message(FATAL_ERROR "NymphClaudeApp.cmake: could not join the page (${code})")
endif()
file(RENAME "${OUTPUT}.tmp" "${OUTPUT}")
