# MIT License © 2026 Binary Dice Games
#
# Script mode: brings the Claude Code plugin in claude/plugin/ up to date
# (spec/06_claude_integration/DESIGN.md section 4.6). Its outputs are
# committed, since the plugin installs from the repository:
#
#   .claude-plugin/plugin.json    from claude/plugin.json.in, with VERSION
#   skills/nymph/SKILL.md         copy of claude/skill/nymph/SKILL.md
#   skills/nymph/reference.md     copy of docs/agent-guide.md
#   bin/checksums.txt             the executables of release VERSION from
#                                 the SHA256SUMS files in SUMS (none: kept)
#
#   cmake -DSOURCE_DIR=<repo> -DVERSION=<x.y.z> [-DSUMS=<file|file|...>]
#         -P NymphPluginSync.cmake

foreach(var SOURCE_DIR VERSION)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "NymphPluginSync.cmake: ${var} is not set")
  endif()
endforeach()
set(plugin "${SOURCE_DIR}/claude/plugin")

file(READ "${SOURCE_DIR}/claude/plugin.json.in" manifest)
string(REPLACE "@NYMPH_VERSION@" "${VERSION}" manifest "${manifest}")
file(WRITE "${plugin}/.claude-plugin/plugin.json" "${manifest}")
configure_file("${SOURCE_DIR}/claude/skill/nymph/SKILL.md" "${plugin}/skills/nymph/SKILL.md" COPYONLY)
configure_file("${SOURCE_DIR}/docs/agent-guide.md" "${plugin}/skills/nymph/reference.md" COPYONLY)

string(REPLACE "|" ";" SUMS "${SUMS}")
if(SUMS)
  set(lines "# SHA-256 of the nymph ${VERSION} release executables (cmake/NymphPluginSync.cmake).\n")
  set(count 0)
  foreach(sums IN LISTS SUMS)
    file(STRINGS "${sums}" entries REGEX "^[0-9a-f]+  nymph-${VERSION}-[a-z]+-[a-z0-9_]+(\\.exe)?$")
    foreach(entry IN LISTS entries)
      string(APPEND lines "${entry}\n")
      math(EXPR count "${count} + 1")
    endforeach()
  endforeach()
  if(count EQUAL 0)
    message(FATAL_ERROR "NymphPluginSync.cmake: no nymph-${VERSION} executable in ${SUMS}")
  endif()
  file(WRITE "${plugin}/bin/checksums.txt" "${lines}")
  message(STATUS "claude/plugin: ${count} release executables pinned")
endif()
