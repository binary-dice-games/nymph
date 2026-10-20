# MIT License © 2026 Binary Dice Games
#
# `cmake --install` rules (spec/07_release_pipeline/DESIGN.md section 4.3):
#
#   bin/nymph[.exe]                    the executable
#   share/nymph/install.sh | .ps1      the PATH script (section 4.9), kept for --uninstall
#   share/doc/nymph/                   README, CHANGELOG, LICENSE and the agent docs
#   share/licenses/nymph/              LICENSE and the embedded fonts' licences
#
# then puts <prefix>/bin on the user's PATH by running that script
# (cmake/NymphInstallPath.cmake), unless NYMPH_INSTALL_UPDATE_PATH is OFF,
# DESTDIR is set, or the environment has NYMPH_NO_MODIFY_PATH=1. Everything
# is component "nymph". Included from the root CMakeLists.txt after the
# targets.

if(PROJECT_IS_TOP_LEVEL AND NOT NYMPH_WEB)
  set(_nymph_install_default ON)
else()
  set(_nymph_install_default OFF)
endif()
option(NYMPH_INSTALL "Add the cmake --install rules of the nymph executable" ${_nymph_install_default})
option(NYMPH_INSTALL_UPDATE_PATH "cmake --install puts <prefix>/bin on the user's PATH" ON)
if(NOT NYMPH_INSTALL OR NOT TARGET nymph-cli)
  return()
endif()

include(GNUInstallDirs)

# Windows: a per-user prefix needs no administrator rights and suits the
# per-user PATH entry (decision 9), instead of C:/Program Files/Nymph.
if(WIN32 AND CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT AND DEFINED ENV{LOCALAPPDATA})
  file(TO_CMAKE_PATH "$ENV{LOCALAPPDATA}/Programs/nymph" _nymph_prefix)
  set(CMAKE_INSTALL_PREFIX "${_nymph_prefix}" CACHE PATH "Install path prefix" FORCE)
endif()

install(TARGETS nymph-cli RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT nymph)

if(WIN32)
  set(_nymph_path_script "${PROJECT_SOURCE_DIR}/packaging/install/install.ps1")
else()
  set(_nymph_path_script "${PROJECT_SOURCE_DIR}/packaging/install/install.sh")
endif()
install(PROGRAMS "${_nymph_path_script}" DESTINATION "${CMAKE_INSTALL_DATADIR}/nymph" COMPONENT nymph)
if(WIN32)
  # MSYS2 shells keep their own PATH: the POSIX script serves them.
  install(PROGRAMS "${PROJECT_SOURCE_DIR}/packaging/install/install.sh"
          DESTINATION "${CMAKE_INSTALL_DATADIR}/nymph" COMPONENT nymph)
endif()

install(FILES README.md CHANGELOG.md LICENSE
              docs/agent-guide.md docs/cli.md docs/format.md docs/elements.md
        DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/nymph" COMPONENT nymph)
install(DIRECTORY docs/elements
        DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/nymph" COMPONENT nymph)
file(GLOB _nymph_font_licences "${NYMPH_FONTS_DIR}/*.txt")
install(FILES LICENSE ${_nymph_font_licences}
        DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/nymph" COMPONENT nymph)

if(NYMPH_INSTALL_UPDATE_PATH)
  # The values the install-time script needs; the prefix is read there,
  # since `cmake --install --prefix` may change it.
  install(CODE "set(NYMPH_INSTALL_BINDIR \"${CMAKE_INSTALL_BINDIR}\")
set(NYMPH_INSTALL_DATADIR \"${CMAKE_INSTALL_DATADIR}\")
set(NYMPH_INSTALL_WINDOWS \"${WIN32}\")" COMPONENT nymph)
  install(SCRIPT "${PROJECT_SOURCE_DIR}/cmake/NymphInstallPath.cmake" COMPONENT nymph)
endif()
