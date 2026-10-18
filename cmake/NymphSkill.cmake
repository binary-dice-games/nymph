# MIT License © 2026 Binary Dice Games
#
# Script mode: packs the claude.ai Skill (spec/06_claude_integration/DESIGN.md
# section 4.8): a zip whose top folder is the skill, as claude.ai expects,
#
#   nymph/SKILL.md       claude/skill/nymph/SKILL.md
#   nymph/reference.md   docs/agent-guide.md
#   nymph/bin/nymph      the static Linux x86-64 executable (mode 755)
#
# listing files only (no directory entries).
#
#   cmake -DSKILL=<SKILL.md> -DGUIDE=<agent-guide.md> -DEXE=<nymph>
#         -DSTAGE=<dir> -DOUTPUT=<file.zip> -P NymphSkill.cmake

foreach(var SKILL GUIDE EXE STAGE OUTPUT)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "NymphSkill.cmake: ${var} is not set")
  endif()
endforeach()

file(REMOVE_RECURSE "${STAGE}")
file(MAKE_DIRECTORY "${STAGE}/nymph/bin")
configure_file("${SKILL}" "${STAGE}/nymph/SKILL.md" COPYONLY)
configure_file("${GUIDE}" "${STAGE}/nymph/reference.md" COPYONLY)
get_filename_component(exe_file "${EXE}" NAME)
file(COPY "${EXE}" DESTINATION "${STAGE}/nymph/bin"
  FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
if(NOT exe_file STREQUAL "nymph")
  file(RENAME "${STAGE}/nymph/bin/${exe_file}" "${STAGE}/nymph/bin/nymph")
endif()

file(REMOVE "${OUTPUT}")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E tar cf "${OUTPUT}" --format=zip nymph/SKILL.md nymph/reference.md nymph/bin/nymph
  WORKING_DIRECTORY "${STAGE}"
  RESULT_VARIABLE code)
if(NOT code EQUAL 0)
  message(FATAL_ERROR "NymphSkill.cmake: could not write ${OUTPUT}")
endif()
