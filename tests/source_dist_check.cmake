# MIT License © 2026 Binary Dice Games
#
# spec/07_release_pipeline/PLAN.md step 4: the release source tarball has
# one top folder, no .git, no generated or WebAssembly file, and builds,
# passes its tests, installs and runs from an empty folder. Run it with no
# network (CI: `unshare -rn` on Linux) to check that nothing is fetched.
# Not a ctest test: it builds the whole project again.
#
#   cmake -DTARBALL=<nymph-x.y.z-src.tar.gz> -DVERSION=<x.y.z> -DWORK_DIR=<empty dir>
#         [-DGENERATOR=Ninja] [-DJOBS=4] -P source_dist_check.cmake

foreach(var TARBALL VERSION WORK_DIR)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "source_dist_check.cmake: ${var} is not set")
  endif()
endforeach()
get_filename_component(TARBALL "${TARBALL}" ABSOLUTE)
get_filename_component(WORK_DIR "${WORK_DIR}" ABSOLUTE)
if(NOT JOBS)
  cmake_host_system_information(RESULT JOBS QUERY NUMBER_OF_LOGICAL_CORES)
endif()
set(generator_args "")
if(GENERATOR)
  set(generator_args -G "${GENERATOR}")
endif()

function(step name)
  message(STATUS "source tarball: ${name}")
  execute_process(COMMAND ${ARGN} WORKING_DIRECTORY "${WORK_DIR}" RESULT_VARIABLE code)
  if(NOT code EQUAL 0)
    message(FATAL_ERROR "source tarball: ${name} failed (${code})")
  endif()
endfunction()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")
step("unpack" "${CMAKE_COMMAND}" -E tar xzf "${TARBALL}")

# ── Contents ─────────────────────────────────────────────────────────────────
file(GLOB top_entries RELATIVE "${WORK_DIR}" "${WORK_DIR}/*")
if(NOT top_entries STREQUAL "nymph-${VERSION}")
  message(FATAL_ERROR "the tarball's top level is '${top_entries}', not nymph-${VERSION}")
endif()
set(src "${WORK_DIR}/nymph-${VERSION}")
file(GLOB_RECURSE entries LIST_DIRECTORIES true RELATIVE "${src}" "${src}/*")
set(bad "")
foreach(entry IN LISTS entries)
  if(entry MATCHES "(^|/)\\.git$|\\.wasm$|(^|/)generated(/|$)")
    list(APPEND bad "${entry}")
  endif()
endforeach()
if(bad)
  message(FATAL_ERROR "the tarball holds files a source tarball must not: ${bad}")
endif()
foreach(required CMakeLists.txt LICENSE extern/imgui/imgui.h extern/googletest/CMakeLists.txt
                 extern/json/include/nlohmann/json.hpp packaging/install/install.sh)
  if(NOT EXISTS "${src}/${required}")
    message(FATAL_ERROR "the tarball lacks ${required}")
  endif()
endforeach()

# ── Build, test, install, run ────────────────────────────────────────────────
step("configure" "${CMAKE_COMMAND}" -S "${src}" -B "${WORK_DIR}/build" ${generator_args} -DCMAKE_BUILD_TYPE=Release)
step("build" "${CMAKE_COMMAND}" --build "${WORK_DIR}/build" --parallel ${JOBS})
step("test" "${CMAKE_COMMAND}" -E chdir "${WORK_DIR}/build" ctest --output-on-failure -j ${JOBS})
file(MAKE_DIRECTORY "${WORK_DIR}/home")
step("install" "${CMAKE_COMMAND}" -E env "HOME=${WORK_DIR}/home" "USERPROFILE=${WORK_DIR}/home"
     NYMPH_NO_MODIFY_PATH=1 "${CMAKE_COMMAND}" --install "${WORK_DIR}/build" --prefix "${WORK_DIR}/prefix")
find_program(installed NAMES nymph PATHS "${WORK_DIR}/prefix/bin" NO_DEFAULT_PATH REQUIRED)
execute_process(COMMAND "${installed}" --version OUTPUT_VARIABLE version_text RESULT_VARIABLE code)
string(STRIP "${version_text}" version_text)
if(NOT code EQUAL 0 OR NOT version_text STREQUAL "nymph ${VERSION}")
  message(FATAL_ERROR "the installed nymph says '${version_text}', not 'nymph ${VERSION}'")
endif()
message(STATUS "source tarball: builds, passes its tests and installs offline")
