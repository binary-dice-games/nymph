# MIT License © 2026 Binary Dice Games
#
# Script mode: brings the Claude Code plugin in claude/plugin/ up to date
# (spec/06_claude_integration/DESIGN.md section 4.6), then the Cursor plugin
# in cursor/plugin/ (spec/12_cursor_integration/DESIGN.md section 4.1),
# which gets the same skill, launcher and pins. Their outputs are
# committed, since the plugins install from the repository. Claude's:
#
#   bin/checksums.txt             the executables of release VERSION from
#                                 the SHA256SUMS files in SUMS (none: kept)
#   .claude-plugin/plugin.json    from claude/plugin.json.in, with the
#                                 version of the release checksums.txt pins
#                                 (0.0.0 when it pins none)
#   skills/nymph/SKILL.md         copy of claude/skill/nymph/SKILL.md
#   skills/nymph/reference.md     copy of docs/agent-guide.md
#
# The plugin's version is the pinned release's, not the source's, so Claude
# Code offers an update exactly when the pin changes
# (spec/07_release_pipeline/DESIGN.md section 4.5).
#
#   cmake -DSOURCE_DIR=<repo> -DVERSION=<x.y.z> [-DSUMS=<file|file|...>]
#         -P NymphPluginSync.cmake

foreach(var SOURCE_DIR VERSION)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "NymphPluginSync.cmake: ${var} is not set")
  endif()
endforeach()
set(plugin "${SOURCE_DIR}/claude/plugin")

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

# The pinned version, from the first executable checksums.txt names.
set(pinned "0.0.0")
if(EXISTS "${plugin}/bin/checksums.txt")
  file(STRINGS "${plugin}/bin/checksums.txt" entries REGEX "^[0-9a-f]+  nymph-[0-9]+\\.[0-9]+\\.[0-9]+-")
  if(entries)
    list(GET entries 0 entry)
    string(REGEX REPLACE "^[0-9a-f]+  nymph-([0-9]+\\.[0-9]+\\.[0-9]+)-.*$" "\\1" pinned "${entry}")
  endif()
endif()

file(READ "${SOURCE_DIR}/claude/plugin.json.in" manifest)
string(REPLACE "@NYMPH_VERSION@" "${pinned}" manifest "${manifest}")
file(WRITE "${plugin}/.claude-plugin/plugin.json" "${manifest}")
configure_file("${SOURCE_DIR}/claude/skill/nymph/SKILL.md" "${plugin}/skills/nymph/SKILL.md" COPYONLY)
configure_file("${SOURCE_DIR}/docs/agent-guide.md" "${plugin}/skills/nymph/reference.md" COPYONLY)

# The Cursor plugin in cursor/plugin/ (spec/12_cursor_integration/DESIGN.md
# section 4.1): the same skill, reference, launcher and pins as the Claude
# plugin's (which Cursor's ${CURSOR_PLUGIN_ROOT} lets it run unchanged),
# plus its own manifest and rule.
set(cursor "${SOURCE_DIR}/cursor/plugin")
file(READ "${SOURCE_DIR}/cursor/plugin.json.in" manifest)
string(REPLACE "@NYMPH_VERSION@" "${pinned}" manifest "${manifest}")
file(WRITE "${cursor}/.cursor-plugin/plugin.json" "${manifest}")
configure_file("${SOURCE_DIR}/cursor/rule/nymph.mdc.in" "${cursor}/rules/nymph.mdc" COPYONLY)
configure_file("${SOURCE_DIR}/claude/skill/nymph/SKILL.md" "${cursor}/skills/nymph/SKILL.md" COPYONLY)
configure_file("${SOURCE_DIR}/docs/agent-guide.md" "${cursor}/skills/nymph/reference.md" COPYONLY)
configure_file("${plugin}/bin/nymph" "${cursor}/bin/nymph" COPYONLY)
configure_file("${plugin}/bin/checksums.txt" "${cursor}/bin/checksums.txt" COPYONLY)
