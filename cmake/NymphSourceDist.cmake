# MIT License © 2026 Binary Dice Games
#
# Script mode: the release source tarball (spec/07_release_pipeline/DESIGN.md
# section 4.4). From a git checkout whose submodules are at their pinned
# commits, writes OUTPUT (nymph-<version>-src.tar.gz) with one top folder
# nymph-<version>/: `git archive` of HEAD and of each submodule, minus the
# submodule folders a build never reads (EXCLUDE below). Committed files
# only: local changes are not in it. No .git, so configuring it runs no git
# command and needs no network.
#
#   cmake -DSOURCE_DIR=<repo> -DVERSION=<x.y.z> -DOUTPUT=<file.tar.gz> -P NymphSourceDist.cmake

foreach(var SOURCE_DIR VERSION OUTPUT)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "NymphSourceDist.cmake: ${var} is not set")
  endif()
endforeach()
find_program(GIT git REQUIRED)

# Submodule folders a build never reads (documentation, tests, examples,
# tools): about 18 MB of the 46 MB the submodules archive to.
set(EXCLUDE
  extern/ImGuiColorTextEdit/docs
  extern/ImGuiColorTextEdit/example
  extern/ImGuiColorTextEdit/extras
  extern/googletest/docs
  extern/imgui/docs
  extern/imgui/examples
  extern/json/docs
  extern/json/tests
  extern/json/tools
  extern/json/single_include
  extern/json/nlohmann_json.natvis
  extern/stb/tests
  extern/stb/deprecated
)

function(run)
  execute_process(COMMAND ${ARGN} WORKING_DIRECTORY "${SOURCE_DIR}"
                  RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
  if(NOT code EQUAL 0)
    message(FATAL_ERROR "NymphSourceDist.cmake: '${ARGN}' failed: ${err}")
  endif()
  set(run_output "${out}" PARENT_SCOPE)
endfunction()

# Every submodule checked out at the commit the repository pins
# (' ' first in `git submodule status`; '-' not checked out, '+' another
# commit, 'U' a conflict).
run("${GIT}" submodule status)
string(REPLACE "\n" ";" lines "${run_output}")
set(submodules "")
foreach(line IN LISTS lines)
  if(line STREQUAL "")
    continue()
  endif()
  string(SUBSTRING "${line}" 0 1 state)
  string(REGEX REPLACE "^.[0-9a-f]+ ([^ ]+).*$" "\\1" path "${line}")
  if(NOT state STREQUAL " ")
    message(FATAL_ERROR "NymphSourceDist.cmake: submodule ${path} is not at its pinned commit "
                        "(run: git submodule update --init)")
  endif()
  list(APPEND submodules "${path}")
endforeach()
if(NOT submodules)
  message(FATAL_ERROR "NymphSourceDist.cmake: no submodule found")
endif()
run("${GIT}" status --porcelain --untracked-files=no)
if(NOT run_output STREQUAL "")
  message(STATUS "NymphSourceDist.cmake: local changes are not in the tarball (it archives HEAD)")
endif()

set(top "nymph-${VERSION}")
get_filename_component(output_dir "${OUTPUT}" DIRECTORY)
set(stage "${output_dir}/source-dist-stage")
file(REMOVE_RECURSE "${stage}")
file(MAKE_DIRECTORY "${stage}")

# The repository, then each submodule into its folder.
run("${GIT}" archive --format=tar "--prefix=${top}/" -o "${stage}/main.tar" HEAD)
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf main.tar WORKING_DIRECTORY "${stage}" RESULT_VARIABLE code)
if(NOT code EQUAL 0)
  message(FATAL_ERROR "NymphSourceDist.cmake: could not unpack the repository archive")
endif()
foreach(path IN LISTS submodules)
  execute_process(COMMAND "${GIT}" -C "${SOURCE_DIR}/${path}" archive --format=tar "--prefix=${top}/${path}/"
                          -o "${stage}/sub.tar" HEAD
                  RESULT_VARIABLE code ERROR_VARIABLE err)
  if(NOT code EQUAL 0)
    message(FATAL_ERROR "NymphSourceDist.cmake: git archive failed in ${path}: ${err}")
  endif()
  execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf sub.tar WORKING_DIRECTORY "${stage}" RESULT_VARIABLE code)
  if(NOT code EQUAL 0)
    message(FATAL_ERROR "NymphSourceDist.cmake: could not unpack ${path}")
  endif()
endforeach()
file(REMOVE "${stage}/main.tar" "${stage}/sub.tar")
foreach(path IN LISTS EXCLUDE)
  file(REMOVE_RECURSE "${stage}/${top}/${path}")
endforeach()

# Fixed timestamps, so two runs on one commit differ only by file order.
file(REMOVE "${OUTPUT}")
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar czf "${OUTPUT}" "--mtime=2026-01-01 00:00:00 UTC" "${top}"
                WORKING_DIRECTORY "${stage}" RESULT_VARIABLE code)
if(NOT code EQUAL 0)
  message(FATAL_ERROR "NymphSourceDist.cmake: could not write ${OUTPUT}")
endif()
file(REMOVE_RECURSE "${stage}")
file(SIZE "${OUTPUT}" size)
math(EXPR size_kb "${size} / 1024")
message(STATUS "Wrote ${OUTPUT} (${size_kb} KiB)")
