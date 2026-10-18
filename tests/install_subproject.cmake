# MIT License © 2026 Binary Dice Games
#
# spec/07_release_pipeline/PLAN.md step 3: nymph added with
# add_subdirectory() adds nothing to the host project's install, and a tree
# without extern/ contents and without git fails to configure with the
# message naming the source tarball.
#
#   cmake -DSOURCE_DIR=<repo> -DWORK_DIR=<dir> -DGENERATOR=<generator> -P install_subproject.cmake

foreach(var SOURCE_DIR WORK_DIR GENERATOR)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "install_subproject.cmake: ${var} is not set")
  endif()
endforeach()
file(REMOVE_RECURSE "${WORK_DIR}")

# ── A host project ───────────────────────────────────────────────────────────
file(MAKE_DIRECTORY "${WORK_DIR}/host")
file(WRITE "${WORK_DIR}/host/CMakeLists.txt" "cmake_minimum_required(VERSION 3.14)
project(host LANGUAGES C CXX)
add_subdirectory(\"${SOURCE_DIR}\" nymph)
")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -S "${WORK_DIR}/host" -B "${WORK_DIR}/host-build" -G "${GENERATOR}"
          -DNYMPH_MCP=OFF -DNYMPH_OPENGL=OFF
  RESULT_VARIABLE code OUTPUT_VARIABLE output ERROR_VARIABLE errors)
if(NOT code EQUAL 0)
  message(FATAL_ERROR "host project did not configure:\n${output}\n${errors}")
endif()
file(GLOB_RECURSE install_scripts "${WORK_DIR}/host-build/*cmake_install.cmake")
foreach(script IN LISTS install_scripts)
  file(READ "${script}" text)
  if(text MATCHES "nymph-cli|/bin/nymph|share/doc/nymph|NymphInstallPath")
    message(FATAL_ERROR "the host's install would install nymph files (${script})")
  endif()
endforeach()

# ── A tree without the submodules or git ─────────────────────────────────────
file(MAKE_DIRECTORY "${WORK_DIR}/bare/extern")
file(COPY "${SOURCE_DIR}/CMakeLists.txt" DESTINATION "${WORK_DIR}/bare")
file(COPY "${SOURCE_DIR}/cmake" DESTINATION "${WORK_DIR}/bare")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -S "${WORK_DIR}/bare" -B "${WORK_DIR}/bare-build" -G "${GENERATOR}"
  RESULT_VARIABLE code OUTPUT_VARIABLE output ERROR_VARIABLE errors)
if(code EQUAL 0 OR NOT errors MATCHES "nymph-<version>-src.tar.gz")
  message(FATAL_ERROR "a tree without extern/ did not fail with the message:\n${errors}")
endif()
message(STATUS "subproject install and bare tree: as expected")
