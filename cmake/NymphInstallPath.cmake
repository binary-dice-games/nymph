# MIT License © 2026 Binary Dice Games
#
# Install-time script (install(SCRIPT), cmake/NymphInstall.cmake): puts
# <prefix>/bin on the user's PATH with the PATH script just installed in
# <prefix>/share/nymph (spec/07_release_pipeline/DESIGN.md section 4.9).
# The script decides: nothing when the folder is already on PATH, nothing
# but a printed line as root or with NYMPH_NO_MODIFY_PATH=1. Never fails
# the install: a PATH it cannot change is reported, not fatal.
#
# Set by the install(CODE) before it: NYMPH_INSTALL_BINDIR,
# NYMPH_INSTALL_DATADIR, NYMPH_INSTALL_WINDOWS.

if(NOT "$ENV{DESTDIR}" STREQUAL "")
  message(STATUS "nymph: DESTDIR is set (a staged install); PATH not changed")
  return()
endif()

foreach(dir BINDIR DATADIR)
  if(IS_ABSOLUTE "${NYMPH_INSTALL_${dir}}")
    set(_nymph_${dir} "${NYMPH_INSTALL_${dir}}")
  else()
    set(_nymph_${dir} "${CMAKE_INSTALL_PREFIX}/${NYMPH_INSTALL_${dir}}")
  endif()
endforeach()

if(NYMPH_INSTALL_WINDOWS)
  find_program(_nymph_powershell NAMES pwsh powershell)
  if(_nymph_powershell)
    file(TO_NATIVE_PATH "${_nymph_BINDIR}" _nymph_native_bin)
    execute_process(
      COMMAND "${_nymph_powershell}" -NoProfile -ExecutionPolicy Bypass
              -File "${_nymph_DATADIR}/nymph/install.ps1" -PathOnly "${_nymph_native_bin}"
      RESULT_VARIABLE _nymph_result)
  else()
    set(_nymph_result "no PowerShell found")
  endif()
  # MSYS2 shells have a PATH of their own.
  if(DEFINED ENV{MSYSTEM})
    find_program(_nymph_sh sh)
    if(_nymph_sh)
      execute_process(COMMAND "${_nymph_sh}" "${_nymph_DATADIR}/nymph/install.sh" --path-only "${_nymph_BINDIR}")
    endif()
  endif()
else()
  execute_process(COMMAND sh "${_nymph_DATADIR}/nymph/install.sh" --path-only "${_nymph_BINDIR}"
                  RESULT_VARIABLE _nymph_result)
endif()

if(NOT _nymph_result STREQUAL "0")
  message(WARNING "nymph: could not put ${_nymph_BINDIR} on PATH (${_nymph_result}); add it yourself")
endif()
