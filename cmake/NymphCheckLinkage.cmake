# MIT License © 2026 Binary Dice Games
#
# Script mode (cmake -P): fails if BINARY lists an OpenGL library among the
# libraries it needs. nymph opens OpenGL at run time and must start on
# machines without it (spec/02_opengl_rasterizer/DESIGN.md section 6).
#
#   cmake -DBINARY=<file> [-DOBJDUMP=<objdump>] [-DDUMPBIN=<dumpbin>] [-DOTOOL=<otool>] -P NymphCheckLinkage.cmake

if(NOT BINARY OR NOT EXISTS "${BINARY}")
  message(FATAL_ERROR "NymphCheckLinkage: BINARY '${BINARY}' does not exist")
endif()

if(DUMPBIN)
  execute_process(
    COMMAND "${DUMPBIN}" /nologo /dependents "${BINARY}"
    OUTPUT_VARIABLE _out RESULT_VARIABLE _rc)
elseif(OTOOL)
  execute_process(
    COMMAND "${OTOOL}" -L "${BINARY}"
    OUTPUT_VARIABLE _out RESULT_VARIABLE _rc)
elseif(OBJDUMP)
  execute_process(
    COMMAND "${OBJDUMP}" -p "${BINARY}"
    OUTPUT_VARIABLE _out RESULT_VARIABLE _rc)
else()
  message(FATAL_ERROR "NymphCheckLinkage: none of OBJDUMP, DUMPBIN and OTOOL given")
endif()
if(NOT _rc EQUAL 0)
  message(FATAL_ERROR "NymphCheckLinkage: could not read the dependencies of ${BINARY}")
endif()

# ELF: "NEEDED  libEGL.so.1"; PE (objdump): "DLL Name: OPENGL32.dll";
# dumpbin: one DLL name per line; otool -L: one dylib or framework path per line.
string(TOLOWER "${_out}" _lower)
set(_libs "")
string(REGEX MATCHALL "needed[ \t]+[^ \t\r\n]+" _needed "${_lower}")
string(REGEX MATCHALL "dll name:[ \t]*[^ \t\r\n]+" _dll_names "${_lower}")
string(REGEX MATCHALL "[^ \t\r\n]+\\.dll" _dlls "${_lower}")
string(REGEX MATCHALL "[^ \t\r\n]+\\.dylib" _dylibs "${_lower}")
string(REGEX MATCHALL "[^ \t\r\n]+\\.framework/[^ \t\r\n]+" _frameworks "${_lower}")
list(APPEND _libs ${_needed} ${_dll_names} ${_dlls} ${_dylibs} ${_frameworks})
if(NOT _libs AND OBJDUMP AND _lower MATCHES "file format elf" AND NOT _lower MATCHES "dynamic section")
  # A fully static executable (a release build, cmake/NymphRelease.cmake)
  # needs no shared library at all, OpenGL included.
  message(STATUS "${BINARY}: statically linked, no shared library")
  return()
endif()
if(NOT _libs)
  message(FATAL_ERROR "NymphCheckLinkage: no dependencies found in the output for ${BINARY}:\n${_out}")
endif()

set(_bad "")
foreach(_lib IN LISTS _libs)
  if(_lib MATCHES "(^|[ \t:/])(libgl|libegl|libopengl|libglx|libglesv2|opengl32)[.-]"
      OR _lib MATCHES "/(opengl|opengles)\\.framework/")
    list(APPEND _bad "${_lib}")
  endif()
endforeach()
if(_bad)
  message(FATAL_ERROR "${BINARY} links an OpenGL library: ${_bad}")
endif()
message(STATUS "${BINARY}: no OpenGL library among its dependencies")
