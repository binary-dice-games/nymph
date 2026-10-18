# MIT License © 2026 Binary Dice Games
#
# spec/06_claude_integration/PLAN.md step 9: the Claude Desktop extension.
# The .mcpb unpacks to manifest.json, icon.png and the executable (files
# only: `mcpb unpack` fails on directory entries); the manifest carries the
# version, the platform and the tools the server offers the model; and the
# unpacked executable runs `nymph mcp`.
#
#   cmake -DMCPB=<file.mcpb> -DVERSION=<x.y.z> -DPLATFORM=<linux|win32|darwin>
#         -DWORK_DIR=<dir> [-DRUNNER=<emulator, '|'-separated>] -P mcpb_check.cmake

foreach(var MCPB VERSION PLATFORM WORK_DIR)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "mcpb_check.cmake: ${var} is not set")
  endif()
endforeach()
string(REPLACE "|" ";" RUNNER "${RUNNER}")
if(NOT EXISTS "${MCPB}")
  message(FATAL_ERROR "${MCPB} does not exist (build the nymph-dist target)")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" -E tar tf "${MCPB}" OUTPUT_VARIABLE listing RESULT_VARIABLE code)
if(NOT code EQUAL 0)
  message(FATAL_ERROR "cannot list ${MCPB}")
endif()
if(listing MATCHES "/\n")
  message(FATAL_ERROR "${MCPB} has directory entries:\n${listing}")
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf "${MCPB}" WORKING_DIRECTORY "${WORK_DIR}" RESULT_VARIABLE code)
if(NOT code EQUAL 0)
  message(FATAL_ERROR "cannot unpack ${MCPB}")
endif()
file(READ "${WORK_DIR}/manifest.json" manifest)
string(JSON version GET "${manifest}" version)
string(JSON platform GET "${manifest}" compatibility platforms 0)
string(JSON entry GET "${manifest}" server entry_point)
string(JSON command GET "${manifest}" server mcp_config command)
if(NOT version STREQUAL VERSION OR NOT platform STREQUAL PLATFORM)
  message(FATAL_ERROR "manifest version ${version} / platform ${platform}, expected ${VERSION} / ${PLATFORM}")
endif()
if(NOT EXISTS "${WORK_DIR}/${entry}" OR NOT EXISTS "${WORK_DIR}/icon.png")
  message(FATAL_ERROR "entry point ${entry} or icon.png missing:\n${listing}")
endif()
if(NOT command STREQUAL "\${__dirname}/server/nymph")
  message(FATAL_ERROR "unexpected command ${command}")
endif()

# The manifest's tools, against what the unpacked server offers the model.
string(JSON n LENGTH "${manifest}" tools)
math(EXPR last "${n} - 1")
set(listed "")
foreach(i RANGE ${last})
  string(JSON name GET "${manifest}" tools ${i} name)
  list(APPEND listed "${name}")
endforeach()
file(WRITE "${WORK_DIR}/input.jsonl"
  "{\"jsonrpc\":\"2.0\",\"id\":0,\"method\":\"initialize\",\"params\":{\"protocolVersion\":\"2025-06-18\"}}\n"
  "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\"}\n")
execute_process(COMMAND ${RUNNER} "${WORK_DIR}/${entry}" mcp INPUT_FILE "${WORK_DIR}/input.jsonl"
                OUTPUT_VARIABLE output RESULT_VARIABLE code TIMEOUT 60)
if(NOT code EQUAL 0)
  message(FATAL_ERROR "the unpacked server exited with '${code}'")
endif()
string(FIND "${output}" "\n" end)
math(EXPR start "${end} + 1")
string(SUBSTRING "${output}" ${start} -1 tools_line)
string(JSON n LENGTH "${tools_line}" result tools)
math(EXPR last "${n} - 1")
set(offered "")
foreach(i RANGE ${last})
  string(JSON name GET "${tools_line}" result tools ${i} name)
  string(JSON visibility ERROR_VARIABLE no_visibility GET "${tools_line}" result tools ${i} _meta ui visibility)
  if(no_visibility OR visibility MATCHES "model")
    list(APPEND offered "${name}")
  endif()
endforeach()
if(NOT listed STREQUAL offered)
  message(FATAL_ERROR "manifest tools (${listed}) differ from the server's (${offered})")
endif()
message(STATUS "${MCPB}: ${n} server tools, manifest in step")
