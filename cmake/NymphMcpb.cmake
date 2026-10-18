# MIT License © 2026 Binary Dice Games
#
# Script mode: packs the Claude Desktop extension, an MCP bundle (.mcpb: a
# zip of manifest.json, icon.png and the executable under server/), for one
# platform (spec/06_claude_integration/DESIGN.md section 4.7).
#
#   cmake -DTEMPLATE=<manifest.json.in> -DEXE=<nymph executable> -DICON=<png>
#         -DVERSION=<x.y.z> -DPLATFORM=<linux|win32|darwin> -DSTAGE=<dir>
#         -DOUTPUT=<file.mcpb> -P NymphMcpb.cmake

foreach(var TEMPLATE EXE ICON VERSION PLATFORM STAGE OUTPUT)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "NymphMcpb.cmake: ${var} is not set")
  endif()
endforeach()

# server/nymph, or server/nymph.exe: Claude Desktop appends .exe to the
# command on Windows.
get_filename_component(suffix "${EXE}" LAST_EXT)
if(NOT suffix STREQUAL ".exe")
  set(suffix "")
endif()
set(exe_name "nymph${suffix}")

file(REMOVE_RECURSE "${STAGE}")
file(MAKE_DIRECTORY "${STAGE}/server")
get_filename_component(exe_file "${EXE}" NAME)
file(COPY "${EXE}" DESTINATION "${STAGE}/server"
  FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
if(NOT exe_file STREQUAL exe_name)
  file(RENAME "${STAGE}/server/${exe_file}" "${STAGE}/server/${exe_name}")
endif()
file(COPY "${ICON}" DESTINATION "${STAGE}")
get_filename_component(icon_file "${ICON}" NAME)
if(NOT icon_file STREQUAL "icon.png")
  file(RENAME "${STAGE}/${icon_file}" "${STAGE}/icon.png")
endif()

file(READ "${TEMPLATE}" manifest)
string(REPLACE "@NYMPH_VERSION@" "${VERSION}" manifest "${manifest}")
string(REPLACE "@NYMPH_EXE@" "${exe_name}" manifest "${manifest}")
string(REPLACE "@NYMPH_MCPB_PLATFORM@" "${PLATFORM}" manifest "${manifest}")
file(WRITE "${STAGE}/manifest.json" "${manifest}")

file(REMOVE "${OUTPUT}")
execute_process(
  # Files only, no directory entry: `mcpb unpack` (2.1.2) fails on one.
  COMMAND "${CMAKE_COMMAND}" -E tar cf "${OUTPUT}" --format=zip manifest.json icon.png "server/${exe_name}"
  WORKING_DIRECTORY "${STAGE}"
  RESULT_VARIABLE code)
if(NOT code EQUAL 0)
  message(FATAL_ERROR "NymphMcpb.cmake: could not write ${OUTPUT}")
endif()
