# MIT License © 2026 Binary Dice Games
#
# spec/06_claude_integration/PLAN.md step 4: the built `nymph mcp`, run as
# a process with its standard input from a file, completes the handshake,
# renders a chart and exits 0 when its input ends.
#
#   cmake -DNYMPH=<nymph executable> -DWORK_DIR=<dir> -P mcp_process.cmake

foreach(var NYMPH WORK_DIR)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "mcp_process.cmake: ${var} is not set")
  endif()
endforeach()

file(MAKE_DIRECTORY "${WORK_DIR}")
set(input "${WORK_DIR}/mcp_process_input.jsonl")
file(WRITE "${input}"
  "{\"jsonrpc\":\"2.0\",\"id\":0,\"method\":\"initialize\",\"params\":{\"protocolVersion\":\"2025-11-25\",\"capabilities\":{},\"clientInfo\":{\"name\":\"ctest\"}}}\n"
  "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}\n"
  "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"render_chart\",\"arguments\":{\"source\":\"image: {width: 200, height: 150}\\nroot:\\n  type: Plot\\n  children:\\n    - {type: PlotLine, label: v, ys: [1, 3, 2]}\\n\"}}}\n")

execute_process(
  COMMAND "${NYMPH}" mcp --rasterizer software
  INPUT_FILE "${input}"
  OUTPUT_VARIABLE output
  ERROR_VARIABLE errors
  RESULT_VARIABLE code
  TIMEOUT 60)
if(NOT code EQUAL 0)
  message(FATAL_ERROR "nymph mcp exited with '${code}'; stderr: ${errors}")
endif()
if(NOT errors STREQUAL "")
  message(FATAL_ERROR "nymph mcp wrote to stderr: ${errors}")
endif()

# One response per line. Not a CMake list: the JSON text holds ';'.
string(FIND "${output}" "\n" first_end)
if(first_end EQUAL -1)
  message(FATAL_ERROR "expected 2 response lines: ${output}")
endif()
string(SUBSTRING "${output}" 0 ${first_end} init)
math(EXPR rest_start "${first_end} + 1")
string(SUBSTRING "${output}" ${rest_start} -1 render)
string(LENGTH "${render}" render_length)
math(EXPR render_last "${render_length} - 1")
string(FIND "${render}" "\n" second_end)
if(NOT second_end EQUAL render_last)
  message(FATAL_ERROR "expected exactly 2 response lines, each ending in a newline: ${output}")
endif()
string(SUBSTRING "${render}" 0 ${second_end} render)

string(JSON version GET "${init}" result protocolVersion)
string(JSON server GET "${init}" result serverInfo name)
if(NOT version STREQUAL "2025-11-25" OR NOT server STREQUAL "nymph")
  message(FATAL_ERROR "unexpected initialize result: ${init}")
endif()
string(JSON kind GET "${render}" result content 0 type)
string(JSON width GET "${render}" result structuredContent width)
string(JSON height GET "${render}" result structuredContent height)
if(NOT kind STREQUAL "image" OR NOT width EQUAL 200 OR NOT height EQUAL 150)
  message(FATAL_ERROR "unexpected render result (kind ${kind}, ${width}x${height})")
endif()
message(STATUS "nymph mcp: handshake and render over stdio OK")
