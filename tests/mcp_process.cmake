# MIT License © 2026 Binary Dice Games
#
# spec/06_claude_integration/PLAN.md step 4: the built `nymph mcp`, run as
# a process with its standard input from a file, completes the handshake,
# renders a chart and exits 0 when its input ends.
#
#   cmake -DNYMPH=<nymph executable> -DWORK_DIR=<dir> [-DRUNNER=<emulator>]
#         [-DEXPECT_VIEWER=ON] -P mcp_process.cmake
#
# RUNNER runs a cross-built executable (wine, qemu-aarch64); EXPECT_VIEWER
# also checks that resources/list offers the MCP App (NYMPH_MCP_VIEWER).

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
# The runner comes '|'-separated: a ';' list would split on the way here.
string(REPLACE "|" ";" RUNNER "${RUNNER}")
set(expected_lines 2)
if(EXPECT_VIEWER)
  file(APPEND "${input}" "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"resources/list\"}\n")
  set(expected_lines 3)
endif()

execute_process(
  COMMAND ${RUNNER} "${NYMPH}" mcp --rasterizer software
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
set(rest "${output}")
set(count 0)
string(FIND "${rest}" "\n" end)
while(NOT end EQUAL -1)
  string(SUBSTRING "${rest}" 0 ${end} line_${count})
  math(EXPR count "${count} + 1")
  math(EXPR next "${end} + 1")
  string(SUBSTRING "${rest}" ${next} -1 rest)
  string(FIND "${rest}" "\n" end)
endwhile()
if(NOT count EQUAL expected_lines OR NOT rest STREQUAL "")
  message(FATAL_ERROR "expected ${expected_lines} response lines, each ending in a newline, got ${count}: ${output}")
endif()
set(init "${line_0}")
set(render "${line_1}")

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
if(EXPECT_VIEWER)
  string(JSON first_uri GET "${line_2}" result resources 0 uri)
  if(NOT first_uri STREQUAL "ui://nymph/viewer")
    message(FATAL_ERROR "resources/list does not offer the MCP App: ${line_2}")
  endif()
endif()
message(STATUS "nymph mcp: handshake and render over stdio OK")
