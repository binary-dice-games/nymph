# MIT License © 2026 Binary Dice Games
#
# spec/07_release_pipeline/PLAN.md step 3: `cmake --install` of the build
# into a scratch prefix installs the files of DESIGN.md section 4.3, the
# installed nymph runs (version, render, source extracted back), and the
# PATH step runs without touching anything (scratch HOME and
# NYMPH_NO_MODIFY_PATH=1: the real profile and Path are never changed).
#
#   cmake -DBUILD_DIR=<build> -DWORK_DIR=<dir> -DVERSION=<x.y.z>
#         -DSOURCE=<a .yaml> -DWINDOWS=<bool> [-DRUNNER=<emulator, '|'-separated>]
#         -P install_check.cmake

foreach(var BUILD_DIR WORK_DIR VERSION SOURCE)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "install_check.cmake: ${var} is not set")
  endif()
endforeach()
string(REPLACE "|" ";" RUNNER "${RUNNER}")
file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}/home")
set(prefix "${WORK_DIR}/prefix")

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env "HOME=${WORK_DIR}/home" "USERPROFILE=${WORK_DIR}/home" NYMPH_NO_MODIFY_PATH=1
          "${CMAKE_COMMAND}" --install "${BUILD_DIR}" --prefix "${prefix}" --component nymph
  RESULT_VARIABLE code OUTPUT_VARIABLE output ERROR_VARIABLE errors)
if(NOT code EQUAL 0)
  message(FATAL_ERROR "cmake --install failed (${code}):\n${output}\n${errors}")
endif()

if(WINDOWS)
  set(exe "${prefix}/bin/nymph.exe")
  set(path_script "${prefix}/share/nymph/install.ps1")
else()
  set(exe "${prefix}/bin/nymph")
  set(path_script "${prefix}/share/nymph/install.sh")
endif()
foreach(file "${exe}" "${path_script}"
             share/doc/nymph/README.md share/doc/nymph/CHANGELOG.md share/doc/nymph/LICENSE
             share/doc/nymph/agent-guide.md share/doc/nymph/cli.md share/doc/nymph/format.md
             share/doc/nymph/elements.md share/licenses/nymph/LICENSE)
  if(NOT IS_ABSOLUTE "${file}")
    set(file "${prefix}/${file}")
  endif()
  if(NOT EXISTS "${file}")
    message(FATAL_ERROR "not installed: ${file}")
  endif()
endforeach()
file(GLOB font_licences "${prefix}/share/licenses/nymph/*.txt")
if(NOT font_licences)
  message(WARNING "no font licence installed (NYMPH_FONTS_DIR without *.txt?)")
endif()

# The PATH step ran and changed nothing.
if(NOT output MATCHES "NYMPH_NO_MODIFY_PATH=1|already on PATH")
  message(FATAL_ERROR "the PATH step did not run:\n${output}\n${errors}")
endif()
file(GLOB_RECURSE home_files "${WORK_DIR}/home/*")
if(home_files)
  message(FATAL_ERROR "the install wrote into HOME: ${home_files}")
endif()

execute_process(COMMAND ${RUNNER} "${exe}" --version OUTPUT_VARIABLE version_text RESULT_VARIABLE code)
string(STRIP "${version_text}" version_text)
if(NOT code EQUAL 0 OR NOT version_text STREQUAL "nymph ${VERSION}")
  message(FATAL_ERROR "installed nymph --version: '${version_text}' (${code}), not 'nymph ${VERSION}'")
endif()
execute_process(COMMAND ${RUNNER} "${exe}" render "${SOURCE}" -o "${WORK_DIR}/chart.png" --rasterizer software
                RESULT_VARIABLE code ERROR_VARIABLE errors)
if(NOT code EQUAL 0 OR NOT EXISTS "${WORK_DIR}/chart.png")
  message(FATAL_ERROR "installed nymph could not render ${SOURCE}: ${errors}")
endif()
execute_process(COMMAND ${RUNNER} "${exe}" "${WORK_DIR}/chart.png" OUTPUT_VARIABLE extracted RESULT_VARIABLE code)
file(READ "${SOURCE}" source_text)
string(REPLACE "\r\n" "\n" extracted "${extracted}")
if(NOT code EQUAL 0 OR NOT extracted STREQUAL source_text)
  message(FATAL_ERROR "installed nymph did not extract the source back (${code})")
endif()
message(STATUS "cmake --install: ${prefix} works")
